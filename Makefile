# =============================================================================
#  Makefile - Noeud communicant temps reel (Cortex-M3 simule sous QEMU)
#
#  Partie A : application FreeRTOS      (dossier partA_freertos/)
#  Partie B : mini-RTOS ecrit a la main (dossier partB_minirtos/)
#
#  Prise en main :
#    make help               liste de toutes les commandes
#    make init               cree l'arborescence et les fichiers de base
#    make check-tools        verifie que les outils sont installes
#    make run                compile la partie A et la lance dans QEMU
#    make PART=B run         pareil pour la partie B
#    make test               tests unitaires sur PC (sans QEMU)
#    make ci                 tout ce que doit verifier l'integration continue
# =============================================================================


PART          ?= A
BUILD         ?= debug
V             ?= 0
WERROR        ?= 1

PREFIX        ?= arm-none-eabi-
GDB           ?= gdb-multiarch
QEMU          ?= qemu-system-arm
QEMU_MACHINE  ?= mps2-an385
QEMU_CPU      ?= cortex-m3
GDB_PORT      ?= 1234
QEMU_TIMEOUT  ?= 5

# Texte que doit afficher le programme sur l'UART pour reussir "make qemu-test"
# (a adapter a ce que ton programme affiche au demarrage)
EXPECT_A      ?= Hello
EXPECT_B      ?= Hello
EXPECT        ?= $(EXPECT_$(PART))

# Tests sur PC. Par defaut : doctest (en-tete seul, paquet doctest-dev).
# Catch2 v3  : make test TEST_LIBS="-lCatch2Main -lCatch2"
# GoogleTest : make test TEST_LIBS="-lgtest -lgtest_main -pthread"
HOST_CXX      ?= g++
TEST_LIBS     ?=

FREERTOS_URL  ?= https://github.com/FreeRTOS/FreeRTOS-Kernel
FREERTOS_TAG  ?=
FREERTOS_PATH := partA_freertos/FreeRTOS-Kernel

TIDY_CHECKS   ?= bugprone-*,clang-analyzer-*,performance-*,modernize-use-nullptr,modernize-use-override

# ------------------------------ Outils ARM -----------------------------------

CC      := $(PREFIX)gcc
CXX     := $(PREFIX)g++
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size
NM      := $(PREFIX)nm

# ------------------------- Choix de la partie (A ou B) -----------------------

ifeq ($(PART),A)
  PART_DIR := partA_freertos
else ifeq ($(PART),B)
  PART_DIR := partB_minirtos
else
  $(error PART doit valoir A ou B (recu : "$(PART)"))
endif

# --------------------------- Type de compilation -----------------------------

DEFS := -DPART_$(PART)

ifeq ($(BUILD),release)
  OPT   ?= -Os
  DEFS  += -DNDEBUG
else ifeq ($(BUILD),debug)
  OPT   ?= -O0
else
  $(error BUILD doit valoir debug ou release (recu : "$(BUILD)"))
endif

OBJ_DIR := build/$(PART)-$(BUILD)
ELF     := $(OBJ_DIR)/firmware.elf
BIN     := $(OBJ_DIR)/firmware.bin
HEX     := $(OBJ_DIR)/firmware.hex
MAP     := $(OBJ_DIR)/firmware.map
LST     := $(OBJ_DIR)/firmware.lst
LOG     := $(OBJ_DIR)/uart.log
LDSCRIPT := $(PART_DIR)/linker.ld

# ------------------------------ Sources --------------------------------------

INCLUDES := -Icommon -Ihal -I$(PART_DIR)

ifeq ($(PART),A)
  ifneq ($(wildcard $(FREERTOS_PATH)/tasks.c),)
    INCLUDES += -isystem $(FREERTOS_PATH)/include -isystem $(FREERTOS_PATH)/portable/GCC/ARM_CM3
    FREERTOS_SRCS := $(addprefix $(FREERTOS_PATH)/, \
        tasks.c queue.c list.c timers.c event_groups.c stream_buffer.c \
        portable/GCC/ARM_CM3/port.c portable/MemMang/heap_4.c)
  endif
endif

OWN_CXX := $(wildcard common/*.cpp) $(wildcard hal/*.cpp) $(wildcard $(PART_DIR)/*.cpp)
OWN_C   := $(wildcard $(PART_DIR)/*.c)
OWN_S   := $(wildcard $(PART_DIR)/*.S)
ALL_C   := $(OWN_C) $(FREERTOS_SRCS)

OBJS := $(addprefix $(OBJ_DIR)/, $(OWN_CXX:.cpp=.o) $(ALL_C:.c=.o) $(OWN_S:.S=.o))
DEPS := $(OBJS:.o=.d)

# ------------------------------- Options -------------------------------------

CPU := -mcpu=cortex-m3 -mthumb

GEN_FLAGS := $(CPU) $(OPT) -g3 -ffunction-sections -fdata-sections -fstack-usage \
             -MD -MP $(DEFS) $(INCLUDES)

# Le code des autres (FreeRTOS) n'est pas compile avec -Werror ; le tien, si.
WERR_FLAG := $(if $(filter 1,$(WERROR)),-Werror)

CFLAGS   := $(GEN_FLAGS) -std=c11 -Wall -Wextra
CXXFLAGS := $(GEN_FLAGS) -std=c++17 -Wall -Wextra -Wconversion -Wshadow $(WERR_FLAG) \
            -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
ASFLAGS  := $(CPU) -g3 -x assembler-with-cpp $(DEFS)

LDFLAGS  := -nostartfiles -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(MAP) \
            -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs

QEMU_BASE := -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) -kernel $(ELF)
GDB_BREAK ?= $(if $(filter B,$(PART)),appMain,main)
GDB_FLAGS ?=

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

MAKEFLAGS += --no-print-directory
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

# =============================================================================
##@ Compilation
# =============================================================================

.PHONY: all release rebuild
all: $(ELF) ## Compile la partie choisie (PART=A ou PART=B, BUILD=debug|release)

release: ## Compile en optimisant la taille (-Os)
	$(MAKE) BUILD=release all

rebuild: ## Nettoie puis recompile
	$(MAKE) clean
	$(MAKE) all

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "  LD    $@"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CPU) $(LDFLAGS) $(OBJS) -o $@
	$(Q)$(SIZE) $@

$(OBJ_DIR)/%.o: %.cpp
	@echo "  CXX   $<"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@echo "  CC    $<"
	@mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@echo "  AS    $<"
	@mkdir -p $(@D)
	$(Q)$(CC) $(ASFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: all-parts
all-parts: ## Compile la partie A puis la partie B
	$(MAKE) PART=A all
	$(MAKE) PART=B all

# =============================================================================
##@ Execution et debug (QEMU + GDB)
# =============================================================================

.PHONY: run run-timeout qemu-test qemu-test-all qemu-gdb gdb debug

run: $(ELF) ## Lance dans QEMU (UART dans le terminal ; quitter : Ctrl+A puis X)
	$(QEMU) $(QEMU_BASE) -nographic

run-timeout: $(ELF) ## Lance QEMU pendant QEMU_TIMEOUT secondes, sortie UART dans le terminal
	-timeout $(QEMU_TIMEOUT) $(QEMU) $(QEMU_BASE) -nographic

qemu-test: $(ELF) ## Test automatique : l'UART doit afficher EXPECT avant QEMU_TIMEOUT secondes
	@echo "  QEMU  timeout $(QEMU_TIMEOUT)s, attend : $(EXPECT)"
	@rm -f $(LOG)
	@timeout $(QEMU_TIMEOUT) $(QEMU) $(QEMU_BASE) -display none -monitor none \
	    -serial file:$(LOG) </dev/null >/dev/null 2>&1 || true
	@if grep -aqF -- "$(EXPECT)" $(LOG) 2>/dev/null; then \
	    echo "  PASS  '$(EXPECT)' trouve"; \
	else \
	    echo "  FAIL  '$(EXPECT)' absent. Sortie UART :"; cat $(LOG) 2>/dev/null; exit 1; \
	fi

qemu-test-all: ## qemu-test pour la partie A puis la partie B
	$(MAKE) PART=A qemu-test
	$(MAKE) PART=B qemu-test

qemu-gdb: $(ELF) ## Terminal 1 : QEMU en pause, attend GDB sur le port GDB_PORT
	$(QEMU) $(QEMU_BASE) -nographic -S -gdb tcp::$(GDB_PORT)

gdb: $(ELF) ## Terminal 2 : GDB se connecte a QEMU et s'arrete sur GDB_BREAK
	$(GDB) $(GDB_FLAGS) $(ELF) -ex "target remote :$(GDB_PORT)" \
	    -ex "break $(GDB_BREAK)" -ex "continue"

debug: $(ELF) ## Debug dans un seul terminal (QEMU en fond, UART ecrite dans le fichier uart.log)
	@echo "  QEMU  en arriere-plan, UART -> $(LOG)"
	@mkdir -p $(OBJ_DIR)
	@$(QEMU) $(QEMU_BASE) -display none -monitor none -serial file:$(LOG) \
	    -S -gdb tcp::$(GDB_PORT) </dev/null >/dev/null 2>&1 & echo $$! > $(OBJ_DIR)/qemu.pid
	@sleep 1
	-@$(GDB) $(GDB_FLAGS) $(ELF) -ex "target remote :$(GDB_PORT)" \
	    -ex "break $(GDB_BREAK)" -ex "continue"
	@kill `cat $(OBJ_DIR)/qemu.pid` 2>/dev/null; rm -f $(OBJ_DIR)/qemu.pid

# =============================================================================
##@ Analyse du binaire
# =============================================================================

.PHONY: size sections symbols stack check-heap vectors disasm bin hex map

size: $(ELF) ## Taille du programme (text = Flash, data+bss = RAM)
	@$(SIZE) $(ELF)

sections: $(ELF) ## Sections du binaire (verifie .isr_vector et .init_array pour le C++)
	@$(OBJDUMP) -h $(ELF)
	@$(OBJDUMP) -h $(ELF) | grep -q init_array \
	    && echo "  .init_array present (constructeurs des objets globaux)" \
	    || echo "  (pas de .init_array : normal si aucun objet global avec constructeur)"

symbols: $(ELF) ## Les 30 plus gros symboles (repere ce qui coute de la memoire)
	@$(NM) --size-sort -S -C $(ELF) | tail -n 30

stack: $(ELF) ## Consommation de pile par fonction (fichiers .su), les 20 plus gourmandes
	@find $(OBJ_DIR) -name '*.su' -exec cat {} + | awk -F'\t' '{print $$2 "\t" $$1}' \
	    | sort -rn | head -n 20

check-heap: $(ELF) ## Montre les symboles d'allocation dynamique presents dans le binaire
	@echo "Symboles d'allocation dynamique (ta version de new dans no_heap.cpp est normale) :"
	@$(NM) -C $(ELF) | grep -E 'operator new|operator delete| malloc| free|pvPortMalloc' \
	    || echo "  aucun"

vectors: $(ELF) ## Premiers octets de la table des vecteurs (1er mot = SP initial, 2e = Reset)
	@$(OBJDUMP) -s -j .isr_vector $(ELF) | head -n 8

disasm: $(ELF) ## Genere le desassemble complet (utile pour lire PendSV)
	@$(OBJDUMP) -d -S -C $(ELF) > $(LST)
	@echo "  ecrit : $(LST)"

bin: $(BIN) ## Genere firmware.bin (pour une vraie carte, plus tard)
hex: $(HEX) ## Genere firmware.hex

$(BIN): $(ELF)
	$(Q)$(OBJCOPY) -O binary $< $@
	@echo "  ecrit : $@"

$(HEX): $(ELF)
	$(Q)$(OBJCOPY) -O ihex $< $@
	@echo "  ecrit : $@"

map: $(ELF) ## Indique ou se trouve le fichier .map (placement memoire detaille)
	@echo "$(MAP)"

# =============================================================================
##@ Tests unitaires sur PC (sans QEMU)
# =============================================================================

TEST_DIR  := build/tests
TEST_BIN  := $(TEST_DIR)/run_tests
TEST_SRCS := $(wildcard common/*.cpp) $(wildcard tests/*.cpp)
TEST_HDRS := $(wildcard common/*.hpp) $(wildcard hal/*.hpp) $(wildcard tests/*.hpp)
HOST_CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -Wconversion -Wshadow $(WERR_FLAG) \
                 -DHOST_TEST -Icommon -Ihal -Itests

.PHONY: test test-asan coverage

test: $(TEST_BIN) ## Compile et lance les tests unitaires (code de common/ + faux materiel)
	@$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(TEST_HDRS)
	@echo "  HOST  tests"
	@mkdir -p $(@D)
	$(Q)$(HOST_CXX) $(HOST_CXXFLAGS) $(TEST_SRCS) -o $@ $(TEST_LIBS)

test-asan: ## Tests avec detection des acces memoire invalides et comportements indefinis
	@mkdir -p $(TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer \
	    $(TEST_SRCS) -o $(TEST_DIR)/run_tests_asan $(TEST_LIBS)
	@$(TEST_DIR)/run_tests_asan

coverage: ## Couverture des tests sur common/ (necessite gcovr)
	@mkdir -p $(TEST_DIR)
	$(HOST_CXX) $(HOST_CXXFLAGS) --coverage $(TEST_SRCS) -o $(TEST_DIR)/run_tests_cov $(TEST_LIBS)
	@$(TEST_DIR)/run_tests_cov
	gcovr -r . --object-directory $(TEST_DIR) --filter 'common/' --print-summary

# =============================================================================
##@ Qualite du code
# =============================================================================

LINT_DIRS    := $(wildcard common hal partA_freertos partB_minirtos tests)
FORMAT_FILES := $(shell find common hal partA_freertos partB_minirtos tests -type f \
                  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' \) \
                  -not -path '*FreeRTOS-Kernel*' 2>/dev/null)

.PHONY: lint lint-cppcheck lint-tidy format format-check

lint: lint-cppcheck lint-tidy ## cppcheck + clang-tidy

lint-cppcheck: ## Analyse statique avec cppcheck
	cppcheck --enable=warning,style,performance,portability --std=c++17 --inline-suppr \
	    --error-exitcode=1 --suppress=missingIncludeSystem --suppress=unusedFunction \
	    -Icommon -Ihal -I$(PART_DIR) -i $(FREERTOS_PATH) $(LINT_DIRS)

lint-tidy: ## clang-tidy sur le code portable de common/
	@if [ -n "$(wildcard common/*.cpp)" ]; then \
	    clang-tidy --checks='$(TIDY_CHECKS)' --warnings-as-errors='bugprone-*' \
	        $(wildcard common/*.cpp) -- -std=c++17 -Icommon -Ihal -DHOST_TEST; \
	else echo "  (aucun .cpp dans common/)"; fi

format: ## Reformate ton code avec clang-format (fichier .clang-format)
	@if [ -n "$(FORMAT_FILES)" ]; then clang-format -i $(FORMAT_FILES); \
	else echo "  (aucun fichier a formater)"; fi

format-check: ## Verifie le format sans modifier (pour la CI)
	@if [ -n "$(FORMAT_FILES)" ]; then clang-format --dry-run --Werror $(FORMAT_FILES); \
	else echo "  (aucun fichier a verifier)"; fi

.PHONY: ci
ci: ## Tout ce que verifie l'integration continue (format, lint, tests, A et B, QEMU)
	$(MAKE) format-check
	$(MAKE) lint
	$(MAKE) test
	$(MAKE) all-parts
	$(MAKE) qemu-test-all

# =============================================================================
##@ Projet et environnement
# =============================================================================

.PHONY: init freertos install-deps check-tools info clean distclean

init: ## Cree les dossiers, .gitignore, .clang-format et un main de test doctest
	@mkdir -p common hal partA_freertos partB_minirtos tests
	@[ -f .gitignore ] || printf 'build/\n*.o\n*.d\n*.su\n' > .gitignore
	@[ -f .clang-format ] || printf 'BasedOnStyle: LLVM\nIndentWidth: 4\nColumnLimit: 100\n' > .clang-format
	@[ -f tests/test_main.cpp ] || printf '#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN\n#include <doctest/doctest.h>\n' > tests/test_main.cpp
	@echo "  arborescence prete (common hal partA_freertos partB_minirtos tests)"

freertos: ## Ajoute (ou initialise) le sous-module FreeRTOS-Kernel pour la partie A
	@if [ -f $(FREERTOS_PATH)/tasks.c ]; then \
	    echo "  FreeRTOS deja present"; \
	elif [ -f .gitmodules ] && grep -q FreeRTOS-Kernel .gitmodules; then \
	    git submodule update --init --recursive; \
	else \
	    git submodule add $(FREERTOS_URL) $(FREERTOS_PATH) && git submodule update --init --recursive; \
	fi
	@[ -z "$(FREERTOS_TAG)" ] || (cd $(FREERTOS_PATH) && git checkout $(FREERTOS_TAG))

install-deps: ## Installe les outils (Ubuntu/Debian/WSL, demande sudo)
	sudo apt update
	sudo apt install -y gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
	    gdb-multiarch qemu-system-arm make git cppcheck clang-tidy clang-format doctest-dev gcovr

check-tools: ## Verifie la presence des outils
	@for t in $(CC) $(CXX) $(SIZE) $(QEMU) $(GDB) make git; do \
	    if command -v $$t >/dev/null 2>&1; then echo "  OK         $$t"; \
	    else echo "  MANQUANT   $$t"; fi; done
	@for t in $(HOST_CXX) cppcheck clang-tidy clang-format gcovr; do \
	    if command -v $$t >/dev/null 2>&1; then echo "  OK         $$t (optionnel)"; \
	    else echo "  absent     $$t (optionnel)"; fi; done

info: ## Affiche la configuration deduite (utile pour comprendre ce qui est compile)
	@echo "PART      = $(PART)  ($(PART_DIR))"
	@echo "BUILD     = $(BUILD)  (OPT=$(OPT))"
	@echo "ELF       = $(ELF)"
	@echo "LDSCRIPT  = $(LDSCRIPT)"
	@echo "C++       = $(OWN_CXX)"
	@echo "C         = $(ALL_C)"
	@echo "ASM       = $(OWN_S)"
	@echo "CXXFLAGS  = $(CXXFLAGS)"
	@echo "LDFLAGS   = $(LDFLAGS)"
	@echo "QEMU      = $(QEMU) $(QEMU_BASE)"

clean: ## Supprime le build de la partie/du type courant
	rm -rf $(OBJ_DIR)

distclean: ## Supprime tout le dossier build/
	rm -rf build

# =============================================================================
##@ Aide
# =============================================================================

.PHONY: help
help: ## Affiche cette aide
	@echo "Variables utiles : PART=A|B  BUILD=debug|release  V=1 (commandes completes)"
	@echo "                   QEMU_TIMEOUT=10  EXPECT='texte'  GDB_BREAK=fonction  WERROR=0"
	@awk 'BEGIN {FS = ":.*##"} \
	    /^##@/ {printf "\n%s\n", substr($$0, 5)} \
	    /^[a-zA-Z0-9_-]+:.*##/ {printf "  %-16s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo