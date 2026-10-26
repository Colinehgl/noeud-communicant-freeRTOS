#include "exp_priority_inversion.hpp"

#include "rtos.hpp"

// ---------------------------------------------------------------------------
// Expérience de l'inversion de priorité (étape A5).
//
//   t=0     L (prio 1) prend le verrou et commence 300 ms de travail
//   t=50    H (prio 3) veut le verrou -> se bloque en attendant L
//   t=100   M (prio 2) devient prête et fait 500 ms de calcul, sans verrou
//
// Avec un SÉMAPHORE BINAIRE : M (2) est plus prioritaire que L (1), donc M prend
//   le processeur. L n'avance plus, H attend tant que M tourne : INVERSION.
//   Attendu : H obtient le verrou vers t=850 (attente ~800 ms).
// Avec un MUTEX : L hérite de la priorité 3 dès que H se bloque. M ne peut plus
//   la préempter, L finit sa section critique et H passe.
//   Attendu : H obtient le verrou vers t=300 (attente ~250 ms).
// ---------------------------------------------------------------------------

namespace {

// Adaptateur : un sémaphore binaire utilisé comme verrou. Il démarre vide,
// donc on le "donne" une fois pour qu'il soit libre au départ.
class SemaphoreLock {
public:
    SemaphoreLock() { (void)sem_.give(); }
    void lock() { (void)sem_.take(); }
    void unlock() { (void)sem_.give(); }

private:
    BinarySemaphore sem_;
};

// Travail CPU de "ms" millisecondes de temps PROCESSEUR, pas de temps écoulé :
// on ne compte un ms que lorsqu'on voit le tick avancer d'exactement 1.
// Si la tâche est préemptée, le saut de tick au retour ne compte que pour 1 ms.
// (Une attente basée sur l'heure murale se terminerait trop tôt après préemption
//  et fausserait l'expérience.)
void busyWork(uint32_t ms) {
    TickType_t prev = xTaskGetTickCount();
    uint32_t   done = 0;
    while (done < ms) {
        const TickType_t now = xTaskGetTickCount();
        if (now != prev) {
            prev = now;
            ++done;
        }
    }
}

template <typename LockT>
class LowTask final : public StaticTaskBase<256> {
public:
    LowTask(LockT& lock, Console& c) : lock_(lock), console_(c) {}

private:
    [[noreturn]] void run() override {
        lock_.lock();
        console_.line("[L] verrou pris     t=", xTaskGetTickCount());
        busyWork(300);  // section critique : 300 ms de travail
        lock_.unlock();
        console_.line("[L] verrou rendu    t=", xTaskGetTickCount());
        for (;;) {
            vTaskSuspend(nullptr);
        }
    }

    LockT&   lock_;
    Console& console_;
};

template <typename LockT>
class MediumTask final : public StaticTaskBase<256> {
public:
    explicit MediumTask(Console& c) : console_(c) {}

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

    Console& console_;
};

template <typename LockT>
class HighTask final : public StaticTaskBase<256> {
public:
    HighTask(LockT& lock, Console& c) : lock_(lock), console_(c) {}

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

    LockT&   lock_;
    Console& console_;
};

// Les 3 tâches + le verrou, regroupés pour qu'ils aient une durée de vie statique.
template <typename LockT>
struct Experiment {
    explicit Experiment(Console& c) : low(lock, c), medium(c), high(lock, c) {}

    void start() {
        low.start("L", 1);
        medium.start("M", 2);
        high.start("H", 3);
    }

    LockT                lock;  // déclaré avant les tâches : construit en premier
    LowTask<LockT>       low;
    MediumTask<LockT>    medium;
    HighTask<LockT>      high;
};

}  // namespace

void startPriorityInversionExperiment(Console& console, bool useMutex) {
    // static : la mémoire doit survivre au démarrage de l'ordonnanceur.
    if (useMutex) {
        static Experiment<Mutex> experiment(console);
        experiment.start();
    } else {
        static Experiment<SemaphoreLock> experiment(console);
        experiment.start();
    }
}