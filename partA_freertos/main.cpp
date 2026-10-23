#include "qemu_uart.hpp"

int main() {
    QemuUart uart;          // local à main : pas de constructeur global (voir A1 du README)
    uart.init();
    uart.puts("Bonjour depuis le Cortex-M3\r\n");
    for (;;) {}
}