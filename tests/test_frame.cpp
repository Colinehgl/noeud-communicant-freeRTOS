#include <doctest/doctest.h>
#include "frame.hpp"
#include "status.hpp"
#include <array>

TEST_CASE("Command packaging and unpackaging") {
    frame::CommandPayload original{0x42, 12345};
    std::array<uint8_t, frame::kCommandSize> raw{};
    
    frame::packCommand(original, raw);
    
    frame::Frame f{};
    f.type = frame::toByte(frame::Type::Command);
    f.length = frame::kCommandSize;
    for (std::size_t i = 0; i < frame::kCommandSize; ++i) {
        f.data[i] = raw[i];
    }

    auto unpacked = frame::unpackCommand(f);
    REQUIRE(unpacked.has_value());
    CHECK(unpacked->cmdId == 0x42);
    CHECK(unpacked->param == 12345);
}

TEST_CASE("Ack packaging and unpackaging") {
    frame::AckPayload original{0x42, 0};
    std::array<uint8_t, frame::kAckSize> raw{};
    
    frame::packAck(original, raw);
    
    frame::Frame f{};
    f.type = frame::toByte(frame::Type::Ack);
    f.length = frame::kAckSize;
    for (std::size_t i = 0; i < frame::kAckSize; ++i) {
        f.data[i] = raw[i];
    }

    auto unpacked = frame::unpackAck(f);
    REQUIRE(unpacked.has_value());
    CHECK(unpacked->cmdId == 0x42);
    CHECK(unpacked->status == 0);
}

TEST_CASE("FrameDecoder with Command frame and CRC") {
    frame::CommandPayload cmd{0x10, -500};
    std::array<uint8_t, frame::kCommandSize> rawCmd{};
    frame::packCommand(cmd, rawCmd);

    // Encoder la trame complète (SOF + Len + Type + Data + CRC)
    std::array<uint8_t, frame::kMaxFrame> wire{};
    auto encodedLen = frame::encode(
        frame::toByte(frame::Type::Command),
        rawCmd.data(),
        rawCmd.size(),
        wire.data(),
        wire.size()
    );
    REQUIRE(encodedLen.has_value());

    // Alimenter le décodeur octet par octet
    FrameDecoder decoder;
    std::optional<Status> status = std::nullopt;

    for (std::size_t i = 0; i < *encodedLen; ++i) {
        status = decoder.feed(wire[i]);
    }

    REQUIRE(status.has_value());
    CHECK(static_cast<int>(*status) == static_cast<int>(Status::Ok));

    const auto& decodedFrame = decoder.lastFrame();
    CHECK(decodedFrame.type == frame::toByte(frame::Type::Command));

    auto unpackedCmd = frame::unpackCommand(decodedFrame);
    REQUIRE(unpackedCmd.has_value());
    CHECK(unpackedCmd->cmdId == 0x10);
    CHECK(unpackedCmd->param == -500);
}