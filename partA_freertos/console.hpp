#pragma once
#include <stdint.h>

#include "rtos.hpp"
#include "uart.hpp"

// ---------------------------------------------------------------------------
// Console : l'UART est partagée entre plusieurs tâches.
// Le Mutex garantit qu'une ligne est écrite en entier par une seule tâche ;
// LockGuard (RAII) garantit qu'il est toujours rendu.
//
// Le Mutex est créé dans le constructeur : déclare la Console "static" dans main.
// ---------------------------------------------------------------------------
class Console {
  public:
    explicit Console(Uart &u) : uart_(u) {}

    // Écrit "<label><valeur>\r\n" d'un seul bloc.
    void line(const char *label, uint32_t value) {
        char digits[11];
        toDecimal(value, digits);

        LockGuard guard(mutex_); // verrou pris ici...
        uart_.puts(label);
        uart_.puts(digits);
        uart_.puts("\r\n");
    } // ...et rendu ici automatiquement

    // Écrit "<label1><v1><label2><v2>\r\n" d'un seul bloc.
    // v1 est non signée, v2 est signée (peut être négative : affichée avec '-').
    void line(const char *label1, uint32_t v1, const char *label2, int32_t v2) {
        char digits1[11];
        char digits2[12]; // '-' + 10 chiffres max
        toDecimal(v1, digits1);
        toDecimalSigned(v2, digits2);

        LockGuard guard(mutex_);
        uart_.puts(label1);
        uart_.puts(digits1);
        uart_.puts(label2);
        uart_.puts(digits2);
        uart_.puts("\r\n");
    }

    // Écrit "<texte>\r\n" d'un seul bloc (le "\r\n" est ajouté ici : ne pas le mettre dans msg).
    void text(const char *msg) {
        LockGuard guard(mutex_);
        uart_.puts(msg);
        uart_.puts("\r\n");
    }

    // Écrit "<label><c> code=<n>\r\n" d'un seul bloc.
    // <c> est le caractère s'il est affichable, '.' sinon (Entrée, Échap, etc.).
    void byteLine(const char *label, uint8_t b) {
        char digits[11];
        toDecimal(b, digits);
        const char shown[2] = {(b >= 32U && b < 127U) ? static_cast<char>(b) : '.', '\0'};

        LockGuard guard(mutex_);
        uart_.puts(label);
        uart_.puts(shown);
        uart_.puts(" code=");
        uart_.puts(digits);
        uart_.puts("\r\n");
    }

  private:
    static void toDecimal(uint32_t v, char *out) { // out : au moins 11 octets
        char tmp[10];
        unsigned n = 0;
        do {
            tmp[n++] = static_cast<char>('0' + v % 10U);
            v /= 10U;
        } while (v != 0U);
        unsigned i = 0;
        while (n > 0) {
            out[i++] = tmp[--n];
        }
        out[i] = '\0';
    }

    static void toDecimalSigned(int32_t v, char *out) { // out : au moins 12 octets
        if (v < 0) {
            out[0] = '-';
            // 0U - x évite le dépassement signé pour INT32_MIN
            toDecimal(0U - static_cast<uint32_t>(v), out + 1);
        } else {
            toDecimal(static_cast<uint32_t>(v), out);
        }
    }

    Uart &uart_;
    Mutex mutex_;
};