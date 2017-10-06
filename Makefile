CC     = cc -std=c99
CFLAGS = -Wall -Wextra -O3 -g3 -march=native

all: benchmark tests

benchmark: test/benchmark.c utf8.h test/utf8-encode.h test/bh-utf8.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test/benchmark.c $(LDLIBS)

tests: test/tests.c utf8.h test/utf8-encode.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test/tests.c $(LDLIBS)

bench: benchmark
	./benchmark

check: tests
	./tests

clean:
	rm -f benchmark tests
