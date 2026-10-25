#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* ---- Horloge et tick ---- */
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 25000000 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* ---- Ordonnanceur ---- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                 12
#define configSTACK_DEPTH_TYPE                  uint32_t

/* ---- Mémoire ---- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 16 * 1024 ) )

/* ---- Fonctionnalités ---- */
#define configUSE_MUTEXES                       1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_TIMERS                        0   /* activé plus tard (A7) */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configQUEUE_REGISTRY_SIZE               0

/* ---- Sécurité ---- */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configASSERT( x ) \
    do { if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ) {} } } while( 0 )

/* ---- Priorités d'interruption (voir A6) ---- */
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    160

/* ---- Fonctions de l'API incluses dans le binaire ---- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1   /* requis pour portMAX_DELAY */
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1

/* ---- Branchement sur la table des vecteurs ---- */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif