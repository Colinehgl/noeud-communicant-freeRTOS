// UART0 de la carte mps2-an385 (CMSDK APB UART)
#include "qemu_uart.hpp"

namespace {
constexpr uint32_t kBase    = 0x40004000UL;
constexpr uint32_t kData    = kBase + 0x00;
constexpr uint32_t kState   = kBase + 0x04;
constexpr uint32_t kCtrl    = kBase + 0x08;
constexpr uint32_t kBauddiv = kBase + 0x10;

constexpr uint32_t kStateTxFull = 1u << 0;
constexpr uint32_t kStateRxFull = 1u << 1;
constexpr uint32_t kCtrlTxEn    = 1u << 0;
constexpr uint32_t kCtrlRxEn    = 1u << 1;

inline volatile uint32_t& reg(uint32_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}
}  // namespace

void QemuUart::init() {
    reg(kBauddiv) = 16;                      // QEMU exige une valeur >= 16
    reg(kCtrl)    = kCtrlTxEn | kCtrlRxEn;
}

void QemuUart::putc(uint8_t b) {
    while (reg(kState) & kStateTxFull) {}    // polling : on attend de la place
    reg(kData) = b;
}

bool QemuUart::getc(uint8_t& b) {
    if (!(reg(kState) & kStateRxFull)) {
        return false;
    }
    b = static_cast<uint8_t>(reg(kData));
    return true;
}