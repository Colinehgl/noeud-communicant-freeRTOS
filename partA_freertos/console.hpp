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
    explicit Console(Uart& u) : uart_(u) {}

    // Écrit "<label><valeur>\r\n" d'un seul bloc.
    void line(const char* label, uint32_t value) {
        char digits[11];
        toDecimal(value, digits);

        LockGuard guard(mutex_);  // verrou pris ici...
        uart_.puts(label);
        uart_.puts(digits);
        uart_.puts("\r\n");
    }  // ...et rendu ici automatiquement

    // Écrit "<texte>\r\n" d'un seul bloc.
    void text(const char* msg) {
        LockGuard guard(mutex_);
        uart_.puts(msg);
        uart_.puts("\r\n");
    }

private:
    static void toDecimal(uint32_t v, char* out) {  // out : au moins 11 octets
        char     tmp[10];
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

    Uart&  uart_;
    Mutex  mutex_;
};