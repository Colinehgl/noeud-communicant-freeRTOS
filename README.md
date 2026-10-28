# Nœud communicant temps réel en C++ embarqué (Cortex-M3)

Un petit appareil **simulé** (aucun matériel nécessaire) qui mesure une température, filtre les mesures et les envoie sur une liaison série sous forme de trames protégées par un CRC, en exécutant plusieurs activités en même temps.

Le projet est en C++17 « sobre » (sans exceptions, RTTI ni allocation dynamique), compilé pour un **ARM Cortex-M3** et exécuté dans **QEMU** (carte `mps2-an385`).

<!-- Remplace USER/REPO puis décommente : [![CI](https://github.com/USER/REPO/actions/workflows/ci.yml/badge.svg)](https://github.com/USER/REPO/actions) -->

## Les deux parties du projet

| | Partie A : `partA_freertos/` | Partie B : `partB_minirtos/` |
|---|---|---|
| **Rôle** | Construire l'application avec **FreeRTOS**, un système temps réel (RTOS) existant et très répandu | Écrire **son propre mini-RTOS** pour comprendre ce qu'il y a à l'intérieur |
| **On apprend** | Les concepts et l'API : tâches, priorités, files de messages, mutex, interruptions | La pile, les interruptions, l'assembleur, le changement de contexte |
| **Difficulté** | Moyenne | Élevée |
| **État** | Terminée | Pas commencée |

## Démarrage rapide

Sur Ubuntu/Debian (ou WSL sous Windows) :

```bash
git clone --recurse-submodules <url-du-depot>
cd <depot>
make install-deps      # installe compilateur croisé, QEMU, GDB, outils de test (sudo)
make check-tools       # vérifie ce qui est présent ou manquant
make run               # compile la partie A et la lance dans QEMU
```

FreeRTOS est un **sous-module git**. Si tu as cloné sans `--recurse-submodules` : `make freertos` (ou `git submodule update --init --recursive`).

Pour quitter QEMU : `Ctrl+A` puis `X`.

## Ce que fait l'application (partie A)

```
AcquisitionTask (prio 3) --rawQueue--> ProcessingTask (prio 2) --filteredQueue--> CommunicationTask (prio 1)
 capteur simulé, 100 ms                 moyenne glissante sur 4                     trames binaires sur l'UART

 UART (réception, par interruption) --rxBytes--> RxTask (prio 2) : décode, exécute la commande, répond par un ACK
```

- **Acquisition** : lit un capteur simulé toutes les 100 ms (25,0 °C plus un bruit de ±0,5 °C). Elle ne bloque jamais : si la file est pleine, la mesure est perdue et comptée.
- **Traitement** : lisse les mesures avec une moyenne glissante.
- **Communication** : encode chaque mesure en trame et l'envoie octet par octet sur l'UART.
- **Réception** : un octet reçu déclenche une interruption qui le dépose dans une file ; `RxTask` le décode, vérifie le CRC, exécute les commandes valides et renvoie un ACK.

Format d'une trame :

```
| SOF 0x7E | LONGUEUR (1 o.) | TYPE (1 o.) | DONNEES (0..N o.) | CRC16 (2 o.) |
```

L'**UART** est une liaison série à deux fils ; le **CRC** est un petit nombre calculé à partir des données, qui permet au récepteur de détecter une trame abîmée.

## Partie A : application FreeRTOS

FreeRTOS est écrit en C : on l'appelle depuis le C++ et on l'enveloppe dans des classes (`StaticTaskBase`, `Queue`, `Mutex`, `LockGuard`, `BinarySemaphore`, `Console`).

| Étape | Titre | Ce qu'elle apporte |
|---|---|---|
| **A0** | Pourquoi un RTOS | Explique pourquoi une seule boucle infinie ne suffit pas dès qu'on a plusieurs activités, et ce que gagne la programmation par tâches. |
| **A1** | Programme minimal sous QEMU | Valide toute la chaîne (compilation croisée, démarrage, script de mémoire, UART) avec un simple message affiché, sans FreeRTOS. |
| **A2** | Intégration de FreeRTOS | Ajoute le noyau en sous-module git et crée la première tâche, sans allocation dynamique. |
| **A3** | Priorités et préemption | Trois tâches de priorités différentes : la plus prioritaire prend la main dès qu'elle est prête, et `xTaskDelayUntil` garde une période exacte. |
| **A4** | Files de messages | Les tâches s'échangent des mesures par des files protégées par le système, ce qui évite les accès simultanés à une variable partagée. |
| **A5** | Mutex, sémaphores, inversion de priorité | Un mutex protège l'UART partagée ; une expérience reproduit l'inversion de priorité (~800 ms d'attente avec un sémaphore, ~250 ms avec un mutex). |
| **A6** | Interruptions et API `FromISR` | La réception UART se fait par interruption plutôt que par interrogation permanente, avec des fonctions du noyau réservées aux interruptions. |
| **A7** | Protocole de trames avec CRC | Définit un format de trame, un CRC calculé à la compilation et un décodeur à états qui résiste aux octets parasites et aux trames corrompues. |
| **A8** | Watchdog et ressources | Surveille la taille des piles, détecte les débordements, interdit `new`/`delete` et vérifie que toutes les tâches restent vivantes. |
| **A9** | Finalisation | Tests unitaires, test sous QEMU, analyse statique, contrôle de taille et intégration continue (`ci.yml`). |

## Partie B : écrire son mini-RTOS

Le dossier `partB_minirtos/` est à créer. Une fois `partB_minirtos/linker.ld` présent, `make ci` compile et teste automatiquement la partie B.

| Étape | Titre | Ce qu'elle apporte |
|---|---|---|
| **B0** | Comprendre le Cortex-M3 | Registres, deux piles (MSP/PSP), modes, et les exceptions SysTick, PendSV et SVC sur lesquelles repose un RTOS. |
| **B1** | Startup et linker script | Code de démarrage écrit à la main, y compris l'appel des constructeurs des objets globaux C++ (`.init_array`). |
| **B2** | Interdire l'allocation, console | `new` et `delete` arrêtent le programme, et une console minimale affiche des entiers sans `printf`. |
| **B3** | La base de temps (SysTick) | Un timer matériel génère un « tick » d'une milliseconde, le battement de cœur du noyau. |
| **B4** | Les tâches (TCB et piles) | Chaque tâche a une fiche (TCB) et une pile statique préparée avec une fausse trame d'exception, pour pouvoir la lancer. |
| **B5** | Changement de contexte coopératif | Le handler PendSV, en assembleur, sauvegarde une tâche et en restaure une autre quand la tâche appelle `yield()`. |
| **B6** | La préemption | Le tick déclenche PendSV : les tâches sont interrompues de force, sans avoir à coopérer. |
| **B7** | Délais, états et tâche idle | `delay(ms)` bloque une tâche ; la tâche idle met le processeur en veille quand tout le monde attend. |
| **B8** | Ordonnancement par priorités | Le noyau choisit la tâche prête la plus prioritaire, et alterne à priorité égale. |
| **B9** | Sémaphores, mutex, files | Reconstruit les primitives de la partie A avec la même interface, pour réutiliser l'application presque telle quelle. |
| **B10** | Inversion de priorité et héritage | Reproduit l'inversion sur son propre noyau, puis la corrige avec l'héritage de priorité. |
| **B11** | Mesures et robustesse | Mesure le temps de changement de contexte, la pile consommée par tâche et la taille du binaire, et détecte les débordements de pile. |
| **B12** | Application finale | Fait tourner la même application sur son propre noyau et compare honnêtement avec FreeRTOS. |

## Exécuter, déboguer, tester

### Exécuter

| Commande | Effet |
|---|---|
| `make` ou `make all` | Compile la partie choisie (`PART=A` par défaut) |
| `make run` | Compile et lance dans QEMU (quitter : `Ctrl+A` puis `X`) |
| `make run-timeout` | Lance QEMU pendant `QEMU_TIMEOUT` secondes (5 par défaut) |
| `make EXP=SEM run` | Expérience d'inversion de priorité avec un sémaphore binaire |
| `make EXP=MUTEX run` | La même expérience avec un mutex (héritage de priorité) |

### Déboguer (GDB)

```bash
make qemu-gdb    # terminal 1 : QEMU en pause, attend GDB sur le port 1234
make gdb         # terminal 2 : GDB se connecte et s'arrête sur main
make debug       # variante dans un seul terminal (sortie UART dans uart.log)
```

### Tester

| Commande | Effet |
|---|---|
| `make test` | Tests unitaires sur PC (doctest) du code de `common/` : CRC, trames, tampon circulaire |
| `make test-asan` | Les mêmes tests avec AddressSanitizer et UBSan |
| `make coverage` | Couverture des tests (nécessite `gcovr`) |
| `make qemu-test` | Lance QEMU et vérifie que l'UART affiche le texte attendu avant le timeout |
| `make lint` | Analyse statique (`cppcheck` + `clang-tidy`) |
| `make format` / `make format-check` | Formate le code / vérifie le formatage (`clang-format`) |
| **`make ci`** | **Tout ce que vérifie l'intégration continue** : format, lint, tests PC, compilation des parties, tests QEMU |

`make ci` ignore la partie B tant que `partB_minirtos/linker.ld` n'existe pas.

### Analyser le binaire

| Commande | Effet |
|---|---|
| `make size` | Taille du binaire (`text` = Flash, `data` + `bss` = RAM) |
| `make stack` | Consommation de pile par fonction, calculée à la compilation |
| `make symbols` | Les 30 plus gros symboles |
| `make check-heap` | Cherche `operator new`, `malloc` ou `pvPortMalloc` dans le binaire |
| `make sections` / `make vectors` / `make disasm` | Sections, table des vecteurs, désassemblage |
| `make bin` / `make hex` | Fichiers à flasher sur une vraie carte |

### Variables utiles

`PART=A|B`, `BUILD=debug|release`, `V=1` (affiche les commandes complètes), `QEMU_TIMEOUT=10`, `EXPECT='texte'`, `GDB_BREAK=fonction`, `WERROR=0` (désactive `-Werror`). Chaque combinaison a son dossier de build (`build/A-debug`, `build/A-debug-SEM`…).

`make help` liste toutes les commandes, `make clean` nettoie le build courant, `make distclean` supprime tout `build/`.

## Organisation du dépôt

```
.
├── README.md
├── Makefile
├── common/                  # code portable, sans dépendance au matériel ni à FreeRTOS
│   ├── ring_buffer.hpp      # tampon circulaire RingBuffer<T, N>
│   ├── crc16.hpp            # CRC avec table calculée à la compilation (constexpr)
│   ├── frame.hpp / .cpp     # encodage et décodage des trames
│   └── status.hpp           # enum class Status
├── hal/                     # couche d'abstraction matérielle
│   ├── uart.hpp             # interface abstraite Uart
│   ├── qemu_uart.hpp / .cpp # implémentation pour la carte simulée
│   └── mock_uart.hpp        # faux UART pour les tests sur PC
├── partA_freertos/
│   ├── FreeRTOS-Kernel/     # sous-module git
│   ├── FreeRTOSConfig.h     # configuration du noyau
│   ├── startup.c            # démarrage et table des vecteurs
│   ├── linker.ld            # placement du programme en mémoire
│   ├── rtos.hpp             # enveloppes C++ : StaticTaskBase, Queue, Mutex, LockGuard, BinarySemaphore
│   ├── console.hpp          # UART partagée, protégée par un mutex
│   ├── rx_isr.hpp / .cpp    # interruption de réception UART
│   ├── main.cpp             # les tâches de l'application et main()
│   ├── exp_priority_inversion.hpp / .cpp   # expérience d'inversion de priorité
│   └── hooks.cpp            # fonctions que FreeRTOS appelle par leur nom (extern "C")
├── partB_minirtos/          # à créer en partie B
├── tests/                   # tests unitaires sur PC (doctest)
└── .github/workflows/ci.yml # intégration continue : exécute `make ci`
```

## Prérequis

Compilateur croisé `arm-none-eabi-g++` / `-gcc`, `qemu-system-arm`, `gdb-multiarch`, `make`, `git`, `cppcheck`, `clang-tidy`, `clang-format`, `doctest-dev` et `gcovr` (optionnel). `make install-deps` installe le tout.

## Ressources

- *Cortex-M3 Devices Generic User Guide* (ARM)
- freertos.org et le livre gratuit *Mastering the FreeRTOS Real Time Kernel*
- Joseph Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*
- C++ Core Guidelines et MISRA C++:2023 pour les règles de codage