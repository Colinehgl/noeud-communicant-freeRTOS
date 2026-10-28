#pragma once
#include <cstdint>

// Resultat d'une tentative de reception de trame. Jamais d'exception, jamais de code magique.
enum class Status : uint8_t {
    Ok,       // trame complete et CRC correct
    BadCrc,   // trame complete mais abimee
    Timeout,  // trame restee incomplete trop longtemps
    Overflow, // LONGUEUR annoncee superieure a la taille maximale acceptee
};

constexpr const char *toString(Status s) {
    switch (s) {
    case Status::Ok:
        return "ok";
    case Status::BadCrc:
        return "bad-crc";
    case Status::Timeout:
        return "timeout";
    case Status::Overflow:
        return "overflow";
    }
    return "?";
}