#pragma once
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

// Base commune des tâches : factorise ce que HelloTask faisait à la main
// (pile statique, fiche de tâche, trampoline). Une tâche = une classe qui
// hérite de StaticTaskBase<taille de pile en mots> et écrit sa méthode run().
template <uint32_t StackWords>
class StaticTaskBase {
public:
    void start(const char* name, UBaseType_t priority) {
        xTaskCreateStatic(&StaticTaskBase::trampoline, name, StackWords, this, priority, stack_,
                          &tcb_);
    }

protected:
    StaticTaskBase() = default;
    ~StaticTaskBase() = default;  // protégé et non virtuel : on ne détruit jamais une tâche ici

    virtual void run() = 0;

private:
    static void trampoline(void* self) { static_cast<StaticTaskBase*>(self)->run(); }

    alignas(8) StackType_t stack_[StackWords];
    StaticTask_t tcb_;
};