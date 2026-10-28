#include "rx_isr.hpp"

namespace {
QemuUart *s_uart = nullptr; // renseignés par main() : pas d'objet global construit
RxByteQueue *s_queue = nullptr;
volatile uint32_t s_lost = 0;
} // namespace

void isrAttach(QemuUart &uart, RxByteQueue &queue) {
    s_uart = &uart;
    s_queue = &queue;
}

uint32_t isrLostBytes() { return s_lost; }

// extern "C" : le nom doit correspondre EXACTEMENT à l'entrée de la table des vecteurs.
extern "C" void UART0RX_Handler(void) {
    BaseType_t woken = pdFALSE;
    if (s_uart != nullptr && s_queue != nullptr) {
        s_uart->clearRxIrq(); // d'abord acquitter, puis vider : aucun octet ne peut être oublié
        uint8_t b = 0;
        while (s_uart->rxPoll(b)) {
            if (!s_queue->sendFromIsr(b, woken)) {
                s_lost = s_lost + 1U;
            }
        }
    }
    portYIELD_FROM_ISR(woken);
}