#pragma once
#include <cstdint>

#include "qemu_uart.hpp"
#include "rtos.hpp"

using RxByteQueue = Queue<uint8_t, 64>;

// À appeler dans main(), avant l'activation de l'interruption.
void isrAttach(QemuUart &uart, RxByteQueue &queue);

// Octets perdus parce que la queue était pleine (pour le diagnostic).
uint32_t isrLostBytes();