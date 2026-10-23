#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/*exceptions*/
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
void (* const vector_table[])(void) = {
    (void (*)(void))(&_estack),   /* 0 : SP initial */
    Reset_Handler,                /* 1 */
    NMI_Handler,                  /* 2 */
    HardFault_Handler,            /* 3 */
    MemManage_Handler,            /* 4 */
    BusFault_Handler,             /* 5 */
    UsageFault_Handler,           /* 6 */
    0, 0, 0, 0,                   /* 7-10 : réservés */
    SVC_Handler,                  /* 11 */
    DebugMon_Handler,             /* 12 */
    0,                            /* 13 : réservé */
    PendSV_Handler,               /* 14 */
    SysTick_Handler,              /* 15 */
};

void Reset_Handler(void)
{
    /* 1. Copier .data de la Flash vers la RAM */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* 2. Mettre .bss à zéro */
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }

    /* 3. Lancer le programme */
    main();

    for (;;) {}
}

void Default_Handler(void)
{
    for (;;) {}   /* on s'arrête, visible avec GDB (Ctrl C) */
}