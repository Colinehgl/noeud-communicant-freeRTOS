#pragma once
#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

// ---------------------------------------------------------------------------
// StaticTaskBase : base commune des tâches (pile statique, fiche de tâche,
// trampoline). Une tâche = une classe qui hérite de StaticTaskBase<taille de
// pile en mots> et écrit sa méthode run().
// ---------------------------------------------------------------------------
template <uint32_t StackWords> class StaticTaskBase {
  public:
    void start(const char *name, UBaseType_t priority) {
        (void)xTaskCreateStatic(&StaticTaskBase::trampoline, name, StackWords, this, priority,
                                stack_, &tcb_);
    }

    StaticTaskBase(const StaticTaskBase &) = delete;
    StaticTaskBase &operator=(const StaticTaskBase &) = delete;

  protected:
    StaticTaskBase() = default;
    ~StaticTaskBase() = default; // protégé, non virtuel : on ne détruit jamais une tâche
    virtual void run() = 0;

  private:
    static void trampoline(void *self) { static_cast<StaticTaskBase *>(self)->run(); }

    alignas(8) StackType_t stack_[StackWords];
    StaticTask_t tcb_;
};

// ---------------------------------------------------------------------------
// Queue<T, N> : file de messages à mémoire statique.
// ---------------------------------------------------------------------------
template <typename T, size_t N> class Queue {
    static_assert(std::is_trivially_copyable_v<T>,
                  "FreeRTOS copie les octets de T (memcpy) : T doit etre trivial");
    static_assert(N > 0, "une queue de taille 0 n'a pas de sens");

  public:
    Queue()
        : storage_{},
          handle_(xQueueCreateStatic(static_cast<UBaseType_t>(N),
                                     static_cast<UBaseType_t>(sizeof(T)), storage_, &control_)) {
        configASSERT(handle_ != nullptr);
    }

    // Une file a une identité : ni copie ni déplacement.
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;
    Queue(Queue &&) = delete;
    Queue &operator=(Queue &&) = delete;

    bool send(const T &item, TickType_t timeout = portMAX_DELAY) {
        return xQueueSend(handle_, &item, timeout) == pdTRUE;
    }

    bool receive(T &item, TickType_t timeout = portMAX_DELAY) {
        return xQueueReceive(handle_, &item, timeout) == pdTRUE;
    }

    // À utiliser uniquement depuis une ISR (étape A6).
    bool sendFromIsr(const T &item, BaseType_t &woken) {
        return xQueueSendFromISR(handle_, &item, &woken) == pdTRUE;
    }

    UBaseType_t messagesWaiting() const { return uxQueueMessagesWaiting(handle_); }

  private:
    // cppcheck-suppress uninitMemberVar
    uint8_t storage_[N * sizeof(T)];
    StaticQueue_t control_;
    QueueHandle_t handle_; // en dernier : initialisé après storage_ et control_
};

// ---------------------------------------------------------------------------
// Mutex : un VERROU. Celui qui le prend est celui qui le rend.
// Seul le mutex FreeRTOS fait l'héritage de priorité (voir exp_priority_inversion.cpp).
// Mémoire statique, créé dans le constructeur (comme Queue) : à déclarer "static"
// dans main, avant vTaskStartScheduler(). À ne pas utiliser depuis une ISR.
// ---------------------------------------------------------------------------
class Mutex {
  public:
    Mutex() : handle_(xSemaphoreCreateMutexStatic(&buffer_)) { configASSERT(handle_ != nullptr); }

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    void lock() { (void)xSemaphoreTake(handle_, portMAX_DELAY); }

    // true si le verrou a été obtenu avant la fin de "wait" (0 = ne pas attendre).
    bool tryLock(TickType_t wait = 0) { return xSemaphoreTake(handle_, wait) == pdTRUE; }

    void unlock() { (void)xSemaphoreGive(handle_); }

  private:
    StaticSemaphore_t buffer_; // avant handle_ : doit exister quand handle_ est initialisé
    SemaphoreHandle_t handle_;
};

// ---------------------------------------------------------------------------
// LockGuard (RAII) : prend le verrou à la construction, le rend à la destruction,
// quel que soit le chemin de sortie du bloc (return anticipé compris).
// Fonctionne avec tout type qui a lock() et unlock().
// ---------------------------------------------------------------------------
template <typename LockT> class BasicLockGuard {
  public:
    explicit BasicLockGuard(LockT &m) : m_(m) { m_.lock(); }
    ~BasicLockGuard() { m_.unlock(); }

    BasicLockGuard(const BasicLockGuard &) = delete;
    BasicLockGuard &operator=(const BasicLockGuard &) = delete;

  private:
    LockT &m_;
};

using LockGuard = BasicLockGuard<Mutex>;

// ---------------------------------------------------------------------------
// BinarySemaphore : un SIGNAL. Celui qui donne n'est pas forcément celui qui prend.
// Il démarre VIDE : take() bloque tant que personne n'a fait give().
// Pas d'héritage de priorité (c'est ce qui rend l'inversion visible dans l'expérience).
// ---------------------------------------------------------------------------
class BinarySemaphore {
  public:
    BinarySemaphore() : handle_(xSemaphoreCreateBinaryStatic(&buffer_)) {
        configASSERT(handle_ != nullptr);
    }

    BinarySemaphore(const BinarySemaphore &) = delete;
    BinarySemaphore &operator=(const BinarySemaphore &) = delete;

    bool take(TickType_t wait = portMAX_DELAY) { return xSemaphoreTake(handle_, wait) == pdTRUE; }
    bool give() { return xSemaphoreGive(handle_) == pdTRUE; }

  private:
    StaticSemaphore_t buffer_;
    SemaphoreHandle_t handle_;
};