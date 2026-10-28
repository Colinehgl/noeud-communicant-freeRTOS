#include "exp_priority_inversion.hpp"

#include "console.hpp"
#include "rtos.hpp"

// ---------------------------------------------------------------------------
// Expérience de l'inversion de priorité (étape A5).
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t kExpStackWords = 256;

class SemaphoreLock {
  public:
    SemaphoreLock() { (void)sem_.give(); }
    void lock() { (void)sem_.take(); }
    void unlock() { (void)sem_.give(); }

  private:
    BinarySemaphore sem_;
};

void busyWork(uint32_t ms) {
    TickType_t prev = xTaskGetTickCount();
    uint32_t done = 0;
    while (done < ms) {
        const TickType_t now = xTaskGetTickCount();
        if (now != prev) {
            prev = now;
            ++done;
        }
    }
}

template <typename LockT> class LowTask final : public StaticTaskBase<kExpStackWords> {
  public:
    LowTask(LockT &lock, Console &c) : lock_(lock), console_(c) {}

  private:
    [[noreturn]] void run() override {
        lock_.lock();
        console_.line("[L] verrou pris     t=", xTaskGetTickCount());
        busyWork(300);
        lock_.unlock();
        console_.line("[L] verrou rendu    t=", xTaskGetTickCount());
        for (;;) {
            vTaskSuspend(nullptr);
        }
    }

    LockT &lock_;
    Console &console_;
};

template <typename LockT> class MediumTask final : public StaticTaskBase<kExpStackWords> {
  public:
    explicit MediumTask(Console &c) : console_(c) {}

  private:
    [[noreturn]] void run() override {
        vTaskDelay(pdMS_TO_TICKS(100));
        console_.line("[M] debut calcul    t=", xTaskGetTickCount());
        busyWork(500);
        console_.line("[M] fin calcul      t=", xTaskGetTickCount());
        for (;;) {
            vTaskSuspend(nullptr);
        }
    }

    Console &console_;
};

template <typename LockT> class HighTask final : public StaticTaskBase<kExpStackWords> {
  public:
    HighTask(LockT &lock, Console &c) : lock_(lock), console_(c) {}

  private:
    [[noreturn]] void run() override {
        vTaskDelay(pdMS_TO_TICKS(50));
        const TickType_t asked = xTaskGetTickCount();
        console_.line("[H] veut le verrou  t=", asked);
        lock_.lock();
        const TickType_t got = xTaskGetTickCount();
        console_.line("[H] verrou obtenu   t=", got);
        console_.line("[H] attente (ticks) = ", got - asked);
        lock_.unlock();
        for (;;) {
            vTaskSuspend(nullptr);
        }
    }

    LockT &lock_;
    Console &console_;
};

template <typename LockT> struct Experiment {
    explicit Experiment(Console &c) : low(lock, c), medium(c), high(lock, c) {}

    void start() {
        low.start("L", 1);
        medium.start("M", 2);
        high.start("H", 3);
    }

    LockT lock; // déclaré avant les tâches : construit en premier
    LowTask<LockT> low;
    MediumTask<LockT> medium;
    HighTask<LockT> high;
};

} // namespace

void startPriorityInversionExperiment(Console &console, bool useMutex) {
    if (useMutex) {
        static Experiment<Mutex> experiment(console);
        experiment.start();
    } else {
        static Experiment<SemaphoreLock> experiment(console);
        experiment.start();
    }
}