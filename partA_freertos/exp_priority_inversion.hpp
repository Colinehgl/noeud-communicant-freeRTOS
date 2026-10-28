#pragma once
#include "console.hpp"

// Crée les 3 tâches de l'expérience (L priorité 1, M priorité 2, H priorité 3).
// Ne démarre PAS l'ordonnanceur : main() appelle vTaskStartScheduler() ensuite.
//   useMutex = false : verrou = sémaphore binaire (pas d'héritage -> inversion visible)
//   useMutex = true  : verrou = Mutex FreeRTOS (héritage de priorité -> inversion corrigée)
void startPriorityInversionExperiment(Console &console, bool useMutex);