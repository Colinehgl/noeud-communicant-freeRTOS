#include "FreeRTOS.h"
#include "task.h"

// Tout ce que FreeRTOS appelle par son nom doit être extern "C" ou le compilateur C++ renomme la fonction et ça casse tout
extern "C" {

void vApplicationGetIdleTaskMemory(StaticTask_t** tcb,
                                   StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* stackSize) {
    static StaticTask_t idleTcb;
    alignas(8) static StackType_t idleStack[configMINIMAL_STACK_SIZE];

    *tcb       = &idleTcb;
    *stack     = idleStack;
    *stackSize = configMINIMAL_STACK_SIZE;
}

// Appelé si une pile déborde (configCHECK_FOR_STACK_OVERFLOW = 2).
void vApplicationStackOverflowHook(TaskHandle_t, char*) {
    for (;;) {}   
}

// Appelé si pvPortMalloc échoue (configUSE_MALLOC_FAILED_HOOK = 1).
void vApplicationMallocFailedHook(void) {
    for (;;) {}
}

}  // extern "C"