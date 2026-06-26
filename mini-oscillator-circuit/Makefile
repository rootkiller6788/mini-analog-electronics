# Makefile — mini-oscillator-circuit
# Builds library, tests, examples, demos, and benchmarks.

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm

# Source files for the library
LIB_SRCS = src/oscillator_core.c \
           src/oscillator_rc.c \
           src/oscillator_lc.c \
           src/oscillator_crystal.c \
           src/oscillator_relaxation.c \
           src/oscillator_analysis.c \
           src/oscillator_pll.c \
           src/oscillator_vco.c

LIB_OBJS = $(LIB_SRCS:.c=.o)

# Test
TEST_SRC   = tests/test_oscillator.c
TEST_BIN   = tests/test_oscillator

# Examples
EX_SRC1 = examples/example_wien_bridge.c
EX_SRC2 = examples/example_colpitts.c
EX_SRC3 = examples/example_crystal.c
EX_SRC4 = examples/example_555_relaxation.c
EX_SRC5 = examples/example_pll_vco.c
EX_BIN1 = examples/example_wien_bridge
EX_BIN2 = examples/example_colpitts
EX_BIN3 = examples/example_crystal
EX_BIN4 = examples/example_555_relaxation
EX_BIN5 = examples/example_pll_vco

.PHONY: all clean test examples

all: test examples

# --- Library objects ---

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Test ---

$(TEST_BIN): $(TEST_SRC) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

# --- Examples ---

$(EX_BIN1): $(EX_SRC1) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN2): $(EX_SRC2) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN3): $(EX_SRC3) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN4): $(EX_SRC4) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN5): $(EX_SRC5) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

examples: $(EX_BIN1) $(EX_BIN2) $(EX_BIN3) $(EX_BIN4) $(EX_BIN5)

# --- Run individual examples ---

run-wien: $(EX_BIN1)
	./$(EX_BIN1)

run-colpitts: $(EX_BIN2)
	./$(EX_BIN2)

run-crystal: $(EX_BIN3)
	./$(EX_BIN3)

run-555: $(EX_BIN4)
	./$(EX_BIN4)

run-pll: $(EX_BIN5)
	./$(EX_BIN5)

# --- Clean ---

clean:
	rm -f src/*.o
	rm -f $(TEST_BIN) $(TEST_BIN).exe
	rm -f $(EX_BIN1) $(EX_BIN1).exe
	rm -f $(EX_BIN2) $(EX_BIN2).exe
	rm -f $(EX_BIN3) $(EX_BIN3).exe
	rm -f $(EX_BIN4) $(EX_BIN4).exe
	rm -f $(EX_BIN5) $(EX_BIN5).exe
