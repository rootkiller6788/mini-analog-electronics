# Makefile for mini-diode-rectifier
# Diode Rectifier and Power Supply Library
#
# Reference: Sedra & Smith (2020), Rashid (2018), Mohan (2003)
#
# Targets:
#   make          - build library, tests, and examples
#   make test     - build and run tests
#   make examples - build all examples
#   make clean    - remove build artifacts

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -I./include
LDFLAGS := -lm

# Source files
SRC_DIODE    := src/diode_physics.c
SRC_RECT     := src/rectifier_topology.c
SRC_FILTER   := src/filter_capacitor.c
SRC_ZENER    := src/zener_regulator.c
SRC_POWER    := src/power_rectifier.c
SRC_SNUBBER  := src/snubber_protection.c

ALL_SRC := $(SRC_DIODE) $(SRC_RECT) $(SRC_FILTER) $(SRC_ZENER) $(SRC_POWER) $(SRC_SNUBBER)

# Test
TEST_SRC := tests/test_diode_rectifier.c
TEST_BIN := test_diode_rectifier

# Examples
EX1_SRC := examples/example_bridge_psu.c
EX2_SRC := examples/example_zener_regulator.c
EX3_SRC := examples/example_voltage_multiplier.c
EX4_SRC := examples/example_three_phase_rectifier.c
EX1_BIN := example_bridge_psu
EX2_BIN := example_zener_regulator
EX3_BIN := example_voltage_multiplier
EX4_BIN := example_three_phase_rectifier

.PHONY: all test examples clean

all: $(TEST_BIN) examples

# Compile C source to object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link test binary
$(TEST_BIN): $(ALL_SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(ALL_SRC) -o $(TEST_BIN) $(LDFLAGS)

# Build examples
examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN) $(EX4_BIN)

$(EX1_BIN): $(ALL_SRC) $(EX1_SRC)
	$(CC) $(CFLAGS) $(EX1_SRC) $(ALL_SRC) -o $(EX1_BIN) $(LDFLAGS)

$(EX2_BIN): $(ALL_SRC) $(EX2_SRC)
	$(CC) $(CFLAGS) $(EX2_SRC) $(ALL_SRC) -o $(EX2_BIN) $(LDFLAGS)

$(EX3_BIN): $(ALL_SRC) $(EX3_SRC)
	$(CC) $(CFLAGS) $(EX3_SRC) $(ALL_SRC) -o $(EX3_BIN) $(LDFLAGS)

$(EX4_BIN): $(ALL_SRC) $(EX4_SRC)
	$(CC) $(CFLAGS) $(EX4_SRC) $(ALL_SRC) -o $(EX4_BIN) $(LDFLAGS)

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean
clean:
	rm -f src/*.o $(TEST_BIN).exe $(TEST_BIN) $(EX1_BIN).exe $(EX2_BIN).exe $(EX3_BIN).exe $(EX4_BIN).exe $(EX1_BIN) $(EX2_BIN) $(EX3_BIN) $(EX4_BIN)