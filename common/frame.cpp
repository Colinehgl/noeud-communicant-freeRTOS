#include "frame.hpp"

namespace frame {

std::optional<std::size_t> encode(uint8_t type, const uint8_t *data, std::size_t len, uint8_t *out,
                                  std::size_t cap) {
    if (len > kMaxData || cap < len + kOverhead) {
        return std::nullopt;
    }
    out[0] = kSof;
    out[1] = static_cast<uint8_t>(len);
    out[2] = type;
    for (std::size_t i = 0; i < len; ++i) {
        out[3 + i] = data[i];
    }
    const uint16_t crc = crc16::compute(&out[1], len + 2);
    out[3 + len] = static_cast<uint8_t>(crc >> 8);
    out[4 + len] = static_cast<uint8_t>(crc & 0xFFU);
    return len + kOverhead;
}

void packMeasure(const MeasurePayload &m, std::array<uint8_t, kMeasureSize> &out) {
    out[0] = static_cast<uint8_t>(m.tick & 0xFFU);
    out[1] = static_cast<uint8_t>((m.tick >> 8) & 0xFFU);
    out[2] = static_cast<uint8_t>((m.tick >> 16) & 0xFFU);
    out[3] = static_cast<uint8_t>((m.tick >> 24) & 0xFFU);
    const auto v = static_cast<uint16_t>(m.value);
    out[4] = static_cast<uint8_t>(v & 0xFFU);
    out[5] = static_cast<uint8_t>(v >> 8);
}

std::optional<MeasurePayload> unpackMeasure(const Frame &f) {
    if (f.type != toByte(Type::Measure) || f.length != kMeasureSize) {
        return std::nullopt;
    }
    const uint32_t tick =
        static_cast<uint32_t>(f.data[0]) | (static_cast<uint32_t>(f.data[1]) << 8) |
        (static_cast<uint32_t>(f.data[2]) << 16) | (static_cast<uint32_t>(f.data[3]) << 24);
    const auto v = static_cast<uint16_t>(f.data[4] | (f.data[5] << 8));
    return MeasurePayload{tick, static_cast<int16_t>(v)};
}

void packCommand(const CommandPayload &c, std::array<uint8_t, kCommandSize> &out) {
    out[0] = c.cmdId;
    out[1] = static_cast<uint8_t>(c.param & 0xFFU);
    out[2] = static_cast<uint8_t>(c.param >> 8);
}

std::optional<CommandPayload> unpackCommand(const Frame &f) {
    if (f.type != toByte(Type::Command) || f.length != kCommandSize) {
        return std::nullopt;
    }
    const uint8_t cmdId = f.data[0];
    const auto param = static_cast<int16_t>(f.data[1] | (f.data[2] << 8));
    return CommandPayload{cmdId, param};
}

void packAck(const AckPayload &a, std::array<uint8_t, kAckSize> &out) {
    out[0] = a.cmdId;
    out[1] = a.status;
}

std::optional<AckPayload> unpackAck(const Frame &f) {
    if (f.type != toByte(Type::Ack) || f.length != kAckSize) {
        return std::nullopt;
    }
    return AckPayload{f.data[0], f.data[1]};
}

} // namespace frame

std::optional<Status> FrameDecoder::feed(uint8_t byte) {
    idle_ = 0;
    switch (state_) {
    case State::WaitSof:
        if (byte == frame::kSof) {
            crc_ = crc16::kInit;
            received_ = 0;
            state_ = State::Length;
        }
        return std::nullopt;

    case State::Length:
        if (byte > frame::kMaxData) {
            state_ = State::WaitSof;
            return Status::Overflow;
        }
        frame_.length = byte;
        crc_ = crc16::update(crc_, byte);
        state_ = State::Type;
        return std::nullopt;

    case State::Type:
        frame_.type = byte;
        crc_ = crc16::update(crc_, byte);
        state_ = (frame_.length == 0) ? State::CrcHi : State::Data;
        return std::nullopt;

    case State::Data:
        frame_.data[received_] = byte;
        ++received_;
        crc_ = crc16::update(crc_, byte);
        if (received_ == frame_.length) {
            state_ = State::CrcHi;
        }
        return std::nullopt;

    case State::CrcHi:
        crcHi_ = byte;
        state_ = State::CrcLo;
        return std::nullopt;

    case State::CrcLo: {
        state_ = State::WaitSof;
        const auto rxCrc = static_cast<uint16_t>((crcHi_ << 8) | byte);
        return (rxCrc == crc_) ? Status::Ok : Status::BadCrc;
    }
    }
    return std::nullopt;
}

std::optional<Status> FrameDecoder::tick() {
    if (state_ == State::WaitSof) {
        idle_ = 0;
        return std::nullopt;
    }
    ++idle_;
    if (idle_ >= timeoutTicks_) {
        reset();
        return Status::Timeout;
    }
    return std::nullopt;
}

void FrameDecoder::reset() {
    state_ = State::WaitSof;
    crc_ = crc16::kInit;
    received_ = 0;
    idle_ = 0;
}