#include <doctest/doctest.h>

#include <array>
#include <cstdint>

#include "crc16.hpp"

TEST_CASE("crc16: vecteur de reference \"123456789\"") {
    const std::array<uint8_t, 9> msg{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(crc16::compute(msg.data(), msg.size()) == 0x29B1);
}

TEST_CASE("crc16: message vide = valeur initiale") {
    CHECK(crc16::compute(nullptr, 0) == crc16::kInit);
}

TEST_CASE("crc16: calcul par morceaux = calcul d'un seul bloc") {
    const std::array<uint8_t, 6> msg{0x06, 0x01, 0xDE, 0xAD, 0xBE, 0xEF};
    const uint16_t whole = crc16::compute(msg.data(), msg.size());
    uint16_t part = crc16::compute(msg.data(), 2);
    part = crc16::compute(msg.data() + 2, msg.size() - 2, part);
    CHECK(part == whole);
}

TEST_CASE("crc16: message suivi de son CRC (gros-boutiste) donne 0") {
    // Propriete utile pour le decodeur : on peut verifier la trame en calculant
    // le CRC sur tout, CRC compris, et en comparant a zero.
    std::array<uint8_t, 8> msg{0x04, 0x01, 0x00, 0xFA, 0x12, 0x34, 0, 0};
    const uint16_t crc = crc16::compute(msg.data(), 6);
    msg[6] = static_cast<uint8_t>(crc >> 8);
    msg[7] = static_cast<uint8_t>(crc & 0xFFU);
    CHECK(crc16::compute(msg.data(), msg.size()) == 0);
}

TEST_CASE("crc16: un bit inverse est toujours detecte") {
    std::array<uint8_t, 6> msg{0x04, 0x01, 0x00, 0xFA, 0x12, 0x34};
    const uint16_t ref = crc16::compute(msg.data(), msg.size());
    for (std::size_t i = 0; i < msg.size(); ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            msg[i] = static_cast<uint8_t>(msg[i] ^ (1U << bit));
            CHECK(crc16::compute(msg.data(), msg.size()) != ref);
            msg[i] = static_cast<uint8_t>(msg[i] ^ (1U << bit)); // on remet
        }
    }
}