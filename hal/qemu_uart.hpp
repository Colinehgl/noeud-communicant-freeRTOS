#pragma once
#include "uart.hpp"

class QemuUart final : public Uart {
public:
    void init();
    void putc(uint8_t b) override;
    bool getc(uint8_t& b) override;
};