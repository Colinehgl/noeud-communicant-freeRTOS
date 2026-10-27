// UART0 de la carte mps2-an385 (CMSDK APB UART)
#include "qemu_uart.hpp"

namespace {
// Constantes complémentaires pour la transmission et le baudrate
constexpr uintptr_t kBauddiv     = 0x10U;
constexpr uint32_t  kCtrlTxEn    = 1U << 0;
constexpr uint32_t  kStateTxFull = 1U << 0;
}  // namespace

void QemuUart::init() {
    reg(kBauddiv) = 16;                      // QEMU exige une valeur >= 16
    reg(kCtrl)    = kCtrlTxEn | kCtrlRxEn;   // Active TX et RX
}

void QemuUart::putc(uint8_t b) {
    while ((reg(kState) & kStateTxFull) != 0U) {}    // Polling : on attend que le tampon de transmission se libère
    reg(kData) = b;
}

bool QemuUart::getc(uint8_t& b) {
    if ((reg(kState) & kStateRxFull) == 0U) {
        return false;
    }
    b = static_cast<uint8_t>(reg(kData) & 0xFFU);
    return true;
}