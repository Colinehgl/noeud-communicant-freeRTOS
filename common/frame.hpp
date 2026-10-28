#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "crc16.hpp"
#include "status.hpp"

namespace frame {

inline constexpr uint8_t      kSof      = 0x7E;
inline constexpr std::size_t kMaxData  = 32;
inline constexpr std::size_t kOverhead = 5;  // SOF + LONGUEUR + TYPE + CRC (2 octets)
inline constexpr std::size_t kMaxFrame = kMaxData + kOverhead;
static_assert(kMaxData <= 255, "LONGUEUR tient sur un octet");

enum class Type : uint8_t { 
    Measure = 0x01, 
    Ack = 0x02, 
    Command = 0x03 
};

constexpr uint8_t toByte(Type t) { return static_cast<uint8_t>(t); }

struct Frame {
    uint8_t                       type   = 0;
    uint8_t                       length = 0;
    std::array<uint8_t, kMaxData> data{};
};

std::optional<std::size_t> encode(uint8_t type, const uint8_t* data, std::size_t len, uint8_t* out,
                                std::size_t cap);

// ---- Mesure ----
inline constexpr std::size_t kMeasureSize = 6;

struct MeasurePayload {
    uint32_t tick;
    int16_t  value;
};

void                         packMeasure(const MeasurePayload& m, std::array<uint8_t, kMeasureSize>& out);
std::optional<MeasurePayload> unpackMeasure(const Frame& f);

// ---- Commande ----
inline constexpr std::size_t kCommandSize = 3;

struct CommandPayload {
    uint8_t cmdId;
    int16_t param;
};

void                          packCommand(const CommandPayload& c, std::array<uint8_t, kCommandSize>& out);
std::optional<CommandPayload> unpackCommand(const Frame& f);

// ---- Acquittement (ACK) ----
inline constexpr std::size_t kAckSize = 2;

struct AckPayload {
    uint8_t cmdId;
    uint8_t status; // 0 = Succès, 1 = Erreur
};

void                      packAck(const AckPayload& a, std::array<uint8_t, kAckSize>& out);
std::optional<AckPayload> unpackAck(const Frame& f);

}  // namespace frame

class FrameDecoder {
public:
    enum class State : uint8_t { WaitSof, Length, Type, Data, CrcHi, CrcLo };
    static constexpr uint32_t kDefaultTimeoutTicks = 50;

    explicit FrameDecoder(uint32_t timeoutTicks = kDefaultTimeoutTicks)
        : timeoutTicks_(timeoutTicks) {}

    std::optional<Status> feed(uint8_t byte);
    std::optional<Status> tick();

    void                 reset();
    const frame::Frame&  lastFrame() const { return frame_; }
    State                state() const { return state_; }

private:
    State         state_ = State::WaitSof;
    frame::Frame  frame_{};
    uint16_t      crc_      = crc16::kInit;
    uint8_t       crcHi_    = 0;
    uint8_t       received_ = 0;
    uint32_t      idle_     = 0;
    uint32_t      timeoutTicks_;
};