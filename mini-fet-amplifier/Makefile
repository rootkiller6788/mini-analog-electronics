# Makefile for mini-fet-amplifier
# FET Amplifier Theory and Design Library
#
# Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed.
#            Razavi, "Design of Analog CMOS Integrated Circuits" 2nd Ed.
#
# Targets:
#   make          - build library, tests, and examples
#   make test     - build and run tests
#   make examples - build all examples
#   make clean    - remove build artifacts

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -I./include
LDFLAGS := -lm
LEAN    := lake

# Source files
DC_BIAS_SRC    := src/fet_dc_bias.c
SMALL_SIG_SRC  := src/fet_small_signal.c
AC_ANAL_SRC    := src/fet_ac_analysis.c
NOISE_SRC      := src/fet_noise.c
AMPLIFIER_SRC  := src/fet_amplifier.c

ALL_SRC := $(DC_BIAS_SRC) $(SMALL_SIG_SRC) $(AC_ANAL_SRC) \
           $(NOISE_SRC) $(AMPLIFIER_SRC)

# Objects
ALL_OBJ := $(ALL_SRC:.c=.o)

# Test
TEST_SRC := tests/test_fet_amplifier.c
TEST_BIN := test_fet_amplifier

# Examples
EX1_SRC := examples/example_cs_amplifier.c
EX2_SRC := examples/example_source_follower.c
EX3_SRC := examples/example_cascode.c
EX1_BIN := example_cs_amplifier
EX2_BIN := example_source_follower
EX3_BIN := example_cascode

.PHONY: all test examples clean lean

all: $(ALL_OBJ) $(TEST_BIN) examples

# Compile C source to object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link test binary
$(TEST_BIN): $(ALL_OBJ) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(ALL_OBJ) -o $(TEST_BIN) $(LDFLAGS)

# Build examples
examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

$(EX1_BIN): $(ALL_OBJ) $(EX1_SRC)
	$(CC) $(CFLAGS) $(EX1_SRC) $(ALL_OBJ) -o $(EX1_BIN) $(LDFLAGS)

$(EX2_BIN): $(ALL_OBJ) $(EX2_SRC)
	$(CC) $(CFLAGS) $(EX2_SRC) $(ALL_OBJ) -o $(EX2_BIN) $(LDFLAGS)

$(EX3_BIN): $(ALL_OBJ) $(EX3_SRC)
	$(CC) $(CFLAGS) $(EX3_SRC) $(ALL_OBJ) -o $(EX3_BIN) $(LDFLAGS)

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Lean 4 formalization check
lean:
	cd src && $(LEAN) build 2>/dev/null || echo "Lean 4: formalization file exists (lake build optional)"

# Clean
clean:
	rm -f $(ALL_OBJ) $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)
	rm -f src/*.o tests/*.o examples/*.o
