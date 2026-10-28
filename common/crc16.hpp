#pragma once
// CRC-16/CCITT-FALSE : polynome 0x1021, valeur initiale 0xFFFF, pas d'inversion
// des bits, pas de XOR final. Table calculee par le compilateur (constexpr) :
// elle est rangee en Flash, rien n'est calcule au demarrage.
#include <array>
#include <cstddef>
#include <cstdint>

namespace crc16 {

inline constexpr uint16_t kPoly = 0x1021;
inline constexpr uint16_t kInit = 0xFFFF;

constexpr std::array<uint16_t, 256> makeTable() {
    std::array<uint16_t, 256> t{};
    for (unsigned i = 0; i < 256; ++i) {
        auto crc = static_cast<uint16_t>(i << 8);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ kPoly)
                                  : static_cast<uint16_t>(crc << 1);
        }
        t[i] = crc;
    }
    return t;
}

inline constexpr auto kTable = makeTable();

// Fait avancer le CRC d'un octet (utile pour le decodeur, qui recoit octet par octet).
constexpr uint16_t update(uint16_t crc, uint8_t byte) {
    return static_cast<uint16_t>((crc << 8) ^ kTable[((crc >> 8) ^ byte) & 0xFFU]);
}

// CRC d'un bloc d'octets. "crc" permet de poursuivre un calcul commence avant.
constexpr uint16_t compute(const uint8_t *data, std::size_t len, uint16_t crc = kInit) {
    for (std::size_t i = 0; i < len; ++i) {
        crc = update(crc, data[i]);
    }
    return crc;
}

// ---- Verifications faites par le compilateur : si l'une echoue, rien ne compile ----
namespace detail {
inline constexpr uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
}
static_assert(kTable[0] == 0x0000 && kTable[1] == 0x1021 && kTable[255] == 0x1EF0,
              "table CRC-16/CCITT incorrecte");
static_assert(compute(detail::kCheck, sizeof detail::kCheck) == 0x29B1,
              "CRC-16/CCITT-FALSE de \"123456789\" doit valoir 0x29B1");

} // namespace crc16