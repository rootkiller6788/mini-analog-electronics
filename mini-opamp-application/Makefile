# Makefile for mini-opamp-application
# Operational Amplifier Application Circuits Library
#
# Reference: Sedra & Smith "Microelectronic Circuits" (2020),
#             Franco "Design with Operational Amplifiers and Analog ICs" (2015),
#             Jung "Op Amp Applications Handbook" (2005)
# Course coverage: MIT 6.002, Stanford EE101, Berkeley EE105,
#   Georgia Tech ECE 3042, ETH 227-0436, TUM EI

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I./include
LDFLAGS = -lm

SRCDIR = src
INCDIR = include
TESTDIR = tests
EXAMPLEDIR = examples
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TESTDIR)/%.c=$(BUILDDIR)/%)

EXAMPLE_SRCS = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLE_BINS = $(EXAMPLE_SRCS:$(EXAMPLEDIR)/%.c=$(BUILDDIR)/%)

.PHONY: all clean test examples

all: $(OBJS)
	@echo "Library objects built successfully."

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BINS)
	@echo "=== Running Tests ==="
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t && echo "PASS: $$t" || echo "FAIL: $$t"; \
	done
	@echo "=== Tests Complete ==="

$(BUILDDIR)/%: $(TESTDIR)/%.c $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

examples: $(EXAMPLE_BINS)
	@echo "=== Running Examples ==="
	@for e in $(EXAMPLE_BINS); do \
		echo "--- $$e ---"; \
		./$$e || echo "Example $$e returned non-zero"; \
	done
	@echo "=== Examples Complete ==="

$(BUILDDIR)/%: $(EXAMPLEDIR)/%.c $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)
