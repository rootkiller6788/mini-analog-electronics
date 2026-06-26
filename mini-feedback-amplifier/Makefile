# ==========================================================================
# Makefile — mini-feedback-amplifier
#
# Targets:
#   make             — build all (library + examples)
#   make test        — build and run tests
#   make examples    — build examples
#   make clean       — remove build artifacts
#   make all         — build everything (lib + tests + examples)
#
# Compiler: GCC with C11, math library, and all warnings enabled
# ==========================================================================

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -O2 -g
INCLUDES := -I./include
LDFLAGS  := -lm

SRCDIR   := src
INCDIR   := include
TSTDIR   := tests
EXADIR   := examples
BLDDIR   := build

# Source files
CORE_SRC  := $(SRCDIR)/feedback_core.c $(SRCDIR)/stability.c \
             $(SRCDIR)/compensation.c $(SRCDIR)/topology_analysis.c
CORE_OBJ  := $(patsubst $(SRCDIR)/%.c,$(BLDDIR)/%.o,$(CORE_SRC))
TEST_SRC  := $(TSTDIR)/test_feedback.c
TEST_OBJ  := $(BLDDIR)/test_feedback.o
EXA_SRC   := $(wildcard $(EXADIR)/*.c)
EXA_BIN   := $(patsubst $(EXADIR)/%.c,$(BLDDIR)/%,$(EXA_SRC))

.PHONY: all test examples clean lib

# Default target
all: lib test examples

# Create build directory
$(BLDDIR):
	mkdir -p $(BLDDIR)

# --- Library (object files) ---
lib: $(BLDDIR) $(CORE_OBJ)
	@echo "=== Library built ==="

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/feedback_amplifier.h $(INCDIR)/stability_analysis.h $(INCDIR)/frequency_compensation.h $(INCDIR)/amplifier_topologies.h | $(BLDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# --- Test ---
test: $(BLDDIR) $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE_OBJ) $(TEST_OBJ) -o $(BLDDIR)/test_feedback $(LDFLAGS)
	@echo "=== Running tests ==="
	./$(BLDDIR)/test_feedback

$(BLDDIR)/test_feedback.o: $(TSTDIR)/test_feedback.c $(INCDIR)/feedback_amplifier.h $(INCDIR)/stability_analysis.h $(INCDIR)/frequency_compensation.h $(INCDIR)/amplifier_topologies.h | $(BLDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# --- Examples ---
examples: $(BLDDIR) $(CORE_OBJ) $(EXA_BIN)
	@echo "=== Examples built ==="

$(BLDDIR)/%: $(EXADIR)/%.c $(CORE_OBJ) | $(BLDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(CORE_OBJ) -o $@ $(LDFLAGS)

# --- Run individual examples ---
run-ex1: $(BLDDIR)/example1_noninverting_amp
	./$(BLDDIR)/example1_noninverting_amp

run-ex2: $(BLDDIR)/example2_stability_compensation
	./$(BLDDIR)/example2_stability_compensation

run-ex3: $(BLDDIR)/example3_topology_comparison
	./$(BLDDIR)/example3_topology_comparison

run-all-examples: examples
	@for exe in $(EXA_BIN); do \
		echo "=== Running $$exe ==="; \
		$$exe; \
		echo ""; \
	done

# --- Clean ---
clean:
	rm -rf $(BLDDIR)
	@echo "=== Cleaned ==="

# --- Help ---
help:
	@echo "Targets:"
	@echo "  make             — build library only"
	@echo "  make test        — build and run tests"
	@echo "  make examples    — build all examples"
	@echo "  make all         — build library + tests + examples"
	@echo "  make run-ex1     — run example 1 (non-inverting amp)"
	@echo "  make run-ex2     — run example 2 (stability & compensation)"
	@echo "  make run-ex3     — run example 3 (topology comparison)"
	@echo "  make run-all-examples — run all examples"
	@echo "  make clean       — remove build artifacts"
