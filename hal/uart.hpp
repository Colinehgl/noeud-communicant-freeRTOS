#pragma once
#include <cstdint>

class Uart {
  public:
    virtual ~Uart() = default;
    virtual void putc(uint8_t b) = 0;
    virtual bool getc(uint8_t &b) = 0;

    void puts(const char *s) {
        while (*s) {
            putc(static_cast<uint8_t>(*s++));
        }
    }
};