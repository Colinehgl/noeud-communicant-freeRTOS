#include "FreeRTOS.h"
#include "console.hpp"
#include "exp_priority_inversion.hpp"
#include "qemu_uart.hpp"
#include "rtos.hpp"
#include "task.h"

// ---------------------------------------------------------------------------
// Le message qui circule dans les queues. Trivial (que des entiers) : copiable
// octet par octet par FreeRTOS. value est en dixièmes de degré (250 = 25,0 °C).
// ---------------------------------------------------------------------------
struct Sample {
    uint32_t tick;
    int16_t  value;
};

using SampleQueue = Queue<Sample, 8>;

// Moyenne glissante sur 4 mesures (lisse le bruit). Pas d'allocation.
class MovingAverage {
public:
    int16_t push(int16_t v) {
        sum_ += v - buf_[idx_];  // on retire la plus ancienne, on ajoute la nouvelle
        buf_[idx_] = v;
        idx_       = (idx_ + 1U) % kWindow;
        if (count_ < kWindow) {
            ++count_;  // pendant le remplissage, on moyenne ce qu'on a
        }
        return static_cast<int16_t>(sum_ / static_cast<int32_t>(count_));
    }

private:
    static constexpr uint32_t kWindow = 4;
    int16_t                   buf_[kWindow] = {};
    int32_t                   sum_          = 0;
    uint32_t                  idx_          = 0;
    uint32_t                  count_        = 0;
};

// ---------------------------------------------------------------------------
// AcquisitionTask (priorité 3) : "lit" un capteur simulé toutes les 100 ms.
// Elle ne doit JAMAIS bloquer sur la queue (sinon la période serait faussée) :
// send avec wait = 0, et si la file est pleine on compte la mesure perdue.
// ---------------------------------------------------------------------------
class AcquisitionTask final : public StaticTaskBase<256> {
public:
    AcquisitionTask(SampleQueue& out, Console& c) : out_(out), console_(c) {}

private:
    [[noreturn]] void run() override {
        TickType_t last = xTaskGetTickCount();
        for (;;) {
            const Sample s{xTaskGetTickCount(), readSensor()};
            if (!out_.send(s, 0)) {
                ++dropped_;
                console_.line("[A] perdues=", dropped_);
            }
            (void)xTaskDelayUntil(&last, pdMS_TO_TICKS(100));
        }
    }

    // Capteur simulé : 25,0 °C plus un bruit pseudo-aléatoire de -0,5 à +0,5 °C.
    int16_t readSensor() {
        seed_       = seed_ * 1664525U + 1013904223U;  // générateur congruentiel linéaire
        const int n = static_cast<int>((seed_ >> 24) % 11U) - 5;
        return static_cast<int16_t>(250 + n);
    }

    SampleQueue& out_;
    Console&     console_;
    uint32_t     seed_    = 12345U;
    uint32_t     dropped_ = 0;
};

// ---------------------------------------------------------------------------
// ProcessingTask (priorité 2) : filtre chaque mesure reçue, puis la transmet.
// Elle attend sans limite (portMAX_DELAY) : Bloquée, donc 0 % de processeur
// tant qu'aucune mesure n'arrive.
// ---------------------------------------------------------------------------
class ProcessingTask final : public StaticTaskBase<256> {
public:
    ProcessingTask(SampleQueue& in, SampleQueue& out) : in_(in), out_(out) {}

private:
    [[noreturn]] void run() override {
        MovingAverage avg;
        for (;;) {
            Sample s{};
            if (in_.receive(s)) {
                s.value = avg.push(s.value);
                (void)out_.send(s);  // bloque si la sortie est pleine : régulation du flux
            }
        }
    }

    SampleQueue& in_;
    SampleQueue& out_;
};

// ---------------------------------------------------------------------------
// CommunicationTask (priorité 1) : affiche les mesures filtrées sur l'UART.
// (Plus tard : elle les mettra en trames avec CRC, étape A7.)
// ---------------------------------------------------------------------------
class CommunicationTask final : public StaticTaskBase<256> {
public:
    CommunicationTask(SampleQueue& in, Console& c) : in_(in), console_(c) {}

private:
    [[noreturn]] void run() override {
        for (;;) {
            Sample s{};
            if (in_.receive(s)) {
                console_.line("[C] t=", s.tick);
                console_.line("[C] v=", static_cast<uint32_t>(s.value));  // valeurs > 0 ici
                // EXPÉRIENCE (voir explication) : décommente pour ralentir le consommateur.
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }

    SampleQueue& in_;
    Console&     console_;
};

int main() {
    // static : ces objets doivent survivre au démarrage de l'ordonnanceur.
    static QemuUart uart;
    static Console  console{uart};  // son Mutex est créé ici, avant l'ordonnanceur

    uart.init();

#if defined(EXP_SEM)
    // make EXP=SEM run : inversion de priorité avec un sémaphore binaire.
    uart.puts("A5 : inversion de priorite, SEMAPHORE BINAIRE\r\n");
    startPriorityInversionExperiment(console, false);
#elif defined(EXP_MUTEX)
    // make EXP=MUTEX run : même scénario avec un mutex (héritage de priorité).
    uart.puts("A5 : inversion de priorite, MUTEX\r\n");
    startPriorityInversionExperiment(console, true);
#else
    static SampleQueue       rawQueue;       // acquisition -> traitement
    static SampleQueue       filteredQueue;  // traitement  -> communication
    static AcquisitionTask   acquisition{rawQueue, console};
    static ProcessingTask    processing{rawQueue, filteredQueue};
    static CommunicationTask communication{filteredQueue, console};

    uart.puts("A5 : acquisition -> traitement -> communication\r\n");

    acquisition.start("acq", 3);
    processing.start("proc", 2);
    communication.start("comm", 1);
#endif

    vTaskStartScheduler();  // ne retourne jamais
    for (;;) {}
}