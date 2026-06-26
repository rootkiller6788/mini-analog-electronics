# Makefile for mini-bjt-amplifier
CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -g
LDFLAGS  = -lm

SRC_DIR  = src
INC_DIR  = include
TST_DIR  = tests
EXA_DIR  = examples
BLD_DIR  = build

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(BLD_DIR)/%.o, $(SRCS))

TEST_SRC = $(TST_DIR)/test_bjt.c
TEST_BIN = $(BLD_DIR)/test_bjt

EXA_SRCS = $(wildcard $(EXA_DIR)/*.c)
EXA_BINS = $(patsubst $(EXA_DIR)/%.c, $(BLD_DIR)/%, $(EXA_SRCS))

.PHONY: all test examples run-examples clean lines

all: $(OBJS) $(TEST_BIN)

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/*.h
	@mkdir -p $(BLD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_SRC)
	@mkdir -p $(BLD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $(TEST_SRC) $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	@echo "Running tests..."
	@$(TEST_BIN)

examples: $(EXA_BINS)

$(BLD_DIR)/%: $(EXA_DIR)/%.c $(OBJS)
	@mkdir -p $(BLD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $< $(OBJS) -o $@ $(LDFLAGS)

run-examples: examples
	@for exe in $(EXA_BINS); do \
		echo "\n===== $$(basename $$exe) ====="; \
		$$exe; \
	done

clean:
	rm -rf $(BLD_DIR)

lines:
	@echo "include/ + src/ line count:"
	@find $(INC_DIR) $(SRC_DIR) -name "*.h" -o -name "*.c" | xargs wc -l
