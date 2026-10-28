#include "FreeRTOS.h"
#include "console.hpp"
#include "exp_priority_inversion.hpp"
#include "frame.hpp"
#include "qemu_uart.hpp"
#include "rtos.hpp"
#include "rx_isr.hpp"
#include "status.hpp"
#include "task.h"
#include <array>

// ---------------------------------------------------------------------------
// Le message qui circule dans les queues. Trivial (que des entiers) : copiable
// octet par octet par FreeRTOS. value est en dixièmes de degré (250 = 25,0 °C).
// ---------------------------------------------------------------------------
struct Sample {
    uint32_t tick;
    int16_t value;
};

using SampleQueue = Queue<Sample, 8>;

// Moyenne glissante sur 4 mesures (lisse le bruit). Pas d'allocation.
class MovingAverage {
  public:
    int16_t push(int16_t v) {
        sum_ += v - buf_[idx_]; // on retire la plus ancienne, on ajoute la nouvelle
        buf_[idx_] = v;
        idx_ = (idx_ + 1U) % kWindow;
        if (count_ < kWindow) {
            ++count_; // pendant le remplissage, on moyenne ce qu'on a
        }
        return static_cast<int16_t>(sum_ / static_cast<int32_t>(count_));
    }

  private:
    static constexpr uint32_t kWindow = 4;
    int16_t buf_[kWindow] = {};
    int32_t sum_ = 0;
    uint32_t idx_ = 0;
    uint32_t count_ = 0;
};

// ---------------------------------------------------------------------------
// AcquisitionTask (priorité 3) : "lit" un capteur simulé toutes les 100 ms.
// ---------------------------------------------------------------------------
class AcquisitionTask final : public StaticTaskBase<256> {
  public:
    AcquisitionTask(SampleQueue &out, Console &c) : out_(out), console_(c) {}

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

    int16_t readSensor() {
        seed_ = seed_ * 1664525U + 1013904223U;
        const int n = static_cast<int>((seed_ >> 24) % 11U) - 5;
        return static_cast<int16_t>(250 + n);
    }

    SampleQueue &out_;
    Console &console_;
    uint32_t seed_ = 12345U;
    uint32_t dropped_ = 0;
};

// ---------------------------------------------------------------------------
// ProcessingTask (priorité 2) : filtre chaque mesure reçue, puis la transmet.
// ---------------------------------------------------------------------------
class ProcessingTask final : public StaticTaskBase<256> {
  public:
    ProcessingTask(SampleQueue &in, SampleQueue &out) : in_(in), out_(out) {}

  private:
    [[noreturn]] void run() override {
        MovingAverage avg;
        for (;;) {
            Sample s{};
            if (in_.receive(s)) {
                s.value = avg.push(s.value);
                (void)out_.send(s);
            }
        }
    }

    SampleQueue &in_;
    SampleQueue &out_;
};

// ---------------------------------------------------------------------------
// CommunicationTask (priorité 1) : encode et envoie les mesures sous forme de trames binaires.
// ---------------------------------------------------------------------------
class CommunicationTask final : public StaticTaskBase<256> {
  public:
    CommunicationTask(SampleQueue &in, QemuUart &uart) : in_(in), uart_(uart) {}

  private:
    [[noreturn]] void run() override {
        for (;;) {
            Sample s{};
            if (in_.receive(s)) {
                // 1. Préparer la charge utile (payload) de la mesure
                frame::MeasurePayload payload{s.tick, s.value};
                std::array<uint8_t, frame::kMeasureSize> rawPayload{};
                frame::packMeasure(payload, rawPayload);

                // 2. Encoder dans une trame complète (SOF + Longueur + Type + Données + CRC)
                std::array<uint8_t, frame::kMaxFrame> wireBuffer{};
                const auto encodedLen =
                    frame::encode(frame::toByte(frame::Type::Measure), rawPayload.data(),
                                  rawPayload.size(), wireBuffer.data(), wireBuffer.size());

                // 3. Envoyer la trame octet par octet sur l'UART si l'encodage a réussi
                if (encodedLen.has_value()) {
                    for (std::size_t i = 0; i < *encodedLen; ++i) {
                        uart_.putc(wireBuffer[i]);
                    }
                }
            }
        }
    }

    SampleQueue &in_;
    QemuUart &uart_;
};

// ---------------------------------------------------------------------------
// RxTask (priorité 2) : consomme les octets reçus par IRQ, décode et gère les commandes.
// ---------------------------------------------------------------------------
class RxTask final : public StaticTaskBase<256> {
  public:
    RxTask(RxByteQueue &in, QemuUart &uart, Console &c) : in_(in), uart_(uart), console_(c) {}

  private:
    [[noreturn]] void run() override {
        uart_.enableRxInterrupt();
        FrameDecoder decoder{50}; // Timeout de 50 ticks

        for (;;) {
            uint8_t b = 0;
            if (in_.receive(b)) {
                // Alimenter le décodeur de trames avec l'octet reçu
                if (const auto status = decoder.feed(b)) {
                    if (*status == Status::Ok) {
                        const auto &f = decoder.lastFrame();

                        if (f.type == frame::toByte(frame::Type::Command)) {
                            if (auto cmd = frame::unpackCommand(f)) {
                                // Une seule ligne, écrite d'un bloc (param peut être négatif)
                                console_.line("[R] Commande recue ID=", cmd->cmdId,
                                              " param=", cmd->param);

                                // Traitement / Action (ex: statut de succès = 0)
                                const uint8_t statusAck = 0;

                                // Préparer et renvoyer un ACK en réponse
                                frame::AckPayload ack{cmd->cmdId, statusAck};
                                std::array<uint8_t, frame::kAckSize> rawAck{};
                                frame::packAck(ack, rawAck);

                                std::array<uint8_t, frame::kMaxFrame> wireAck{};
                                if (auto len = frame::encode(frame::toByte(frame::Type::Ack),
                                                             rawAck.data(), rawAck.size(),
                                                             wireAck.data(), wireAck.size())) {
                                    for (std::size_t i = 0; i < *len; ++i) {
                                        uart_.putc(wireAck[i]);
                                    }
                                }
                            }
                        } else {
                            console_.line("[R] Trame valide recue, type=", f.type);
                        }

                    } else if (*status == Status::BadCrc) {
                        // Console::text ajoute déjà "\r\n"
                        console_.text("[R] Erreur : CRC invalide !");
                    } else if (*status == Status::Overflow) {
                        console_.text("[R] Erreur : Debordement de trame !");
                    }
                }
            }
        }
    }

    RxByteQueue &in_;
    QemuUart &uart_;
    Console &console_;
};

int main() {
    static QemuUart uart;
    static Console console{uart};

    uart.init(); // Indispensable pour initialiser le matériel avant tout affichage

#if defined(EXP_SEM)
    uart.puts("A5 : inversion de priorite, SEMAPHORE BINAIRE\r\n");
    startPriorityInversionExperiment(console, false);
#elif defined(EXP_MUTEX)
    uart.puts("A5 : inversion de priorite, MUTEX\r\n");
    startPriorityInversionExperiment(console, true);
#else
    static SampleQueue rawQueue;
    static SampleQueue filteredQueue;
    static RxByteQueue rxBytes;
    static AcquisitionTask acquisition{rawQueue, console};
    static ProcessingTask processing{rawQueue, filteredQueue};
    static CommunicationTask communication{filteredQueue, uart};
    static RxTask rx{rxBytes, uart, console};

    uart.puts("A8 : protocole bidirectionnel, commandes et ACK\r\n");

    isrAttach(uart, rxBytes);

    acquisition.start("acq", 3);
    processing.start("proc", 2);
    communication.start("comm", 1);
    rx.start("rx", 2);
#endif

    vTaskStartScheduler();
    for (;;) {
    }
}