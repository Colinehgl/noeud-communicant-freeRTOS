#include "FreeRTOS.h"
#include "qemu_uart.hpp"
#include "rtos.hpp"
#include "semphr.h"
#include "task.h"

// ---------------------------------------------------------------------------
// Console : l'UART est partagée entre plusieurs tâches. 
// Un mutex garantit qu'une ligne est écrite en entier par une seule tâche.
// ---------------------------------------------------------------------------
class Console {
public:
    explicit Console(Uart& u) : uart_(u) {}

    void init() { mutex_ = xSemaphoreCreateMutexStatic(&mutexBuf_); }

    // Écrit "<label><valeur>\r\n" d'un seul bloc.
    void line(const char* label, uint32_t value) {
        char digits[11];
        toDecimal(value, digits);

        xSemaphoreTake(mutex_, portMAX_DELAY);
        uart_.puts(label);
        uart_.puts(digits);
        uart_.puts("\r\n");
        xSemaphoreGive(mutex_);
    }

private:
    static void toDecimal(uint32_t v, char* out) {  // out : au moins 11 octets
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

    Uart&             uart_;
    SemaphoreHandle_t mutex_ = nullptr;
    StaticSemaphore_t mutexBuf_;
};

// ---------------------------------------------------------------------------
// HighTask (priorité 3) : se réveille exactement toutes les 500 ms.
// xTaskDelayUntil fixe l'instant du réveil (pas la durée de l'attente), donc
// les ticks affichés doivent être des multiples de 500 même si d'autres
// tâches calculent en continu : c'est la preuve de la préemption.
// ---------------------------------------------------------------------------
class HighTask final : public StaticTaskBase<256> {
public:
    explicit HighTask(Console& c) : console_(c) {}

private:
    [[noreturn]] void run() override {
        TickType_t last = xTaskGetTickCount();
        for (;;) {
            (void)xTaskDelayUntil(&last, pdMS_TO_TICKS(500));
            console_.line("[H] tick=", xTaskGetTickCount());
        }
    }

    Console& console_;
};

// ---------------------------------------------------------------------------
// WorkerTask (priorité 2) : ne se bloque jamais, compte en boucle et affiche
// son compteur toutes les secondes. Deux workers de même priorité se partagent
// le processeur à chaque tick (time slicing) : leurs compteurs doivent être
// du même ordre de grandeur.
// ---------------------------------------------------------------------------
class WorkerTask final : public StaticTaskBase<256> {
public:
    WorkerTask(Console& c, const char* label, const char* sliceLabel)
        : console_(c), label_(label), sliceLabel_(sliceLabel) {}

private:
    [[noreturn]] void run() override {
        volatile uint32_t counter  = 0;
        uint32_t          slices   = 0;
        TickType_t        lastSeen = xTaskGetTickCount();
        TickType_t        next     = pdMS_TO_TICKS(1000);
        for (;;) {
            counter = counter + 1U;
            const TickType_t now = xTaskGetTickCount();
            if (now != lastSeen) {
                lastSeen = now;
                ++slices;
            }
            if (now >= next) {
                console_.line(label_, counter);
                console_.line(sliceLabel_, slices);
                next += pdMS_TO_TICKS(1000);
            }
        }
    }

    Console&    console_;
    const char* label_;
    const char* sliceLabel_;
};

int main() {
    // static : ces objets doivent survivre au démarrage de l'ordonnanceur.
    static QemuUart   uart;
    static Console    console{uart};
    static HighTask   high{console};
    static WorkerTask workerA{console, "[A] n=", "[A] slices="};
    static WorkerTask workerB{console, "[B] n=", "[B] slices="};

    uart.init();
    console.init();
    uart.puts("A3 : priorites et preemption\r\n");

    high.start("high", 3);
    workerB.start("workerB", 1);  
    workerA.start("workerA", 1);   

    vTaskStartScheduler();  // ne retourne jamais
    for (;;) {}
}