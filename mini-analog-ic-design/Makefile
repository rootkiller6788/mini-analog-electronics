CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude
LDFLAGS = -lm

SRCS = src/analog_ic_basis.c src/analog_ic_amplifier.c src/analog_ic_current_mirror.c src/analog_ic_noise.c src/analog_ic_bandgap.c src/analog_ic_advanced.c
OBJS = src/analog_ic_basis.o src/analog_ic_amplifier.o src/analog_ic_current_mirror.o src/analog_ic_noise.o src/analog_ic_bandgap.o src/analog_ic_advanced.o

TESTS = tests/test_analog_ic_basis.c tests/test_analog_ic_amplifier.c tests/test_analog_ic_noise.c
EXAMPLES = examples/example_two_stage_opamp.c examples/example_bandgap_reference.c examples/example_current_mirror.c

.PHONY: all clean test examples bench

all: test examples

%.o: %.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.test: tests/%.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

examples/%.exe: examples/%.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

test: $(OBJS) $(TESTS:.c=.test)
	@echo "=== Running Tests ==="
	@for t in $(TESTS:.c=.test); do echo "--- $$t ---"; ./$$t || exit 1; done
	@echo "=== All Tests Passed ==="

examples: $(OBJS) $(EXAMPLES:.c=.exe)
	@echo "=== Running Examples ==="
	@for e in $(EXAMPLES:.c=.exe); do echo "--- $$e ---"; ./$$e; done
	@echo "=== All Examples Complete ==="

bench: $(OBJS) benches/analog_ic_bench.c
	$(CC) $(CFLAGS) benches/analog_ic_bench.c $(OBJS) -o benches/analog_ic_bench.bench $(LDFLAGS)
	./benches/analog_ic_bench.bench

clean:
	rm -f src/*.o tests/*.test examples/*.exe benches/*.bench
