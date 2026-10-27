#pragma once
#include <cstdint>

#include "uart.hpp"

class QemuUart final : public Uart {
public:
    void init();
    void putc(uint8_t b) override;
    bool getc(uint8_t& b) override;

    // ---- A6 : réception par interruption ----

    // À appeler APRÈS init(), de préférence depuis une tâche (ordonnanceur démarré).
    void enableRxInterrupt() {
        reg(kCtrl)     = reg(kCtrl) | kCtrlRxEn | kCtrlRxIrqEn;
        reg(kIntClear) = kIntRx;
        *reinterpret_cast<volatile uint8_t*>(kNvicIpr + kRxIrqNumber) = kRxIrqPriority;
        *reinterpret_cast<volatile uint32_t*>(kNvicIser)               = (1U << kRxIrqNumber);
    }

    // Non virtuel : utilisable dans une ISR. Lit un octet s'il y en a un.
    bool rxPoll(uint8_t& b) {
        if ((reg(kState) & kStateRxFull) == 0U) {
            return false;
        }
        b = static_cast<uint8_t>(reg(kData) & 0xFFU);
        return true;
    }

    // Acquitte l'interruption de réception (on écrit 1 dans le bit à effacer).
    void clearRxIrq() { reg(kIntClear) = kIntRx; }

private:
    static volatile uint32_t& reg(uintptr_t off) {
        return *reinterpret_cast<volatile uint32_t*>(kBase + off);
    }

    // UART0 de la carte mps2-an385 (UART CMSDK APB)
    static constexpr uintptr_t kBase     = 0x40004000U;
    static constexpr uintptr_t kData     = 0x00U;
    static constexpr uintptr_t kState    = 0x04U;
    static constexpr uintptr_t kCtrl     = 0x08U;
    static constexpr uintptr_t kIntClear = 0x0CU;  // INTSTATUS en lecture, INTCLEAR en écriture

    static constexpr uint32_t kStateRxFull = 1U << 1;
    static constexpr uint32_t kCtrlRxEn    = 1U << 1;
    static constexpr uint32_t kCtrlRxIrqEn = 1U << 3;
    static constexpr uint32_t kIntRx       = 1U << 1;

    // NVIC. L'IRQ n correspond à l'entrée 16 + n de la table des vecteurs.
    static constexpr uintptr_t kNvicIser       = 0xE000E100U;
    static constexpr uintptr_t kNvicIpr        = 0xE000E400U;  // 1 octet par IRQ
    static constexpr uint32_t  kRxIrqNumber    = 0U;           // UART0 RX (à vérifier)
    static constexpr uint8_t   kRxIrqPriority  = 0xC0U;        // 192 >= 160 : appels FromISR permis
};