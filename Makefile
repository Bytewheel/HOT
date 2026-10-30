CC      ?= cc
CFLAGS  ?= -O2
CPPFLAGS?=

WARN      = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wwrite-strings -Wconversion -Wsign-conversion
HOTFLAGS  = -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -Iinclude
ALL_CFLAGS = -std=c11 $(WARN) $(CFLAGS) $(HOTFLAGS)
SANFLAGS   = -fsanitize=address,undefined -fno-sanitize-recover=all

LIB   = libhotcompat.a
OBJS  = src/hot.o src/hotwire.o src/hotstore.o src/hotext.o
BINS  = tests/test_hot tests/spec_vet tests/golden_hot tests/fuzz_hot \
        tests/test_wire tests/test_store tests/test_ext

all: $(LIB) $(BINS)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

src/hot.o: src/hot.c include/hot/time.h
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -c -o $@ $<

src/hotwire.o: src/hotwire.c include/hot/wire.h include/hot/time.h include/hot/ext.h
	$(CC) $(ALL_CFLAGS) -D_HOT_TIME_BITS=2 $(CPPFLAGS) -c -o $@ $<

src/hotstore.o: src/hotstore.c include/hot/store.h include/hot/time.h include/hot/ext.h
	$(CC) $(ALL_CFLAGS) -D_HOT_TIME_BITS=2 $(CPPFLAGS) -c -o $@ $<

# library ships at rung 2: full ext surface (docs/08 X11)
src/hotext.o: src/hotext.c include/hot/ext.h include/hot/time.h
	$(CC) $(ALL_CFLAGS) -D_HOT_TIME_BITS=2 $(CPPFLAGS) -c -o $@ $<

tests/test_hot: tests/test_hot.c $(LIB) include/hot/time.h
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -o $@ tests/test_hot.c $(LIB)

# T13/T14 ride the rung-2 v3 surface (docs/08 X11)
tests/test_wire: tests/test_wire.c $(LIB) include/hot/wire.h include/hot/time.h include/hot/ext.h
	$(CC) $(ALL_CFLAGS) -D_HOT_TIME_BITS=2 $(CPPFLAGS) -o $@ tests/test_wire.c $(LIB)

tests/test_store: tests/test_store.c $(LIB) include/hot/store.h include/hot/time.h
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -o $@ tests/test_store.c $(LIB)

# T11/T12 ride the rung-2 surface (docs/08 X11)
tests/test_ext: tests/test_ext.c $(LIB) include/hot/ext.h include/hot/time.h
	$(CC) $(ALL_CFLAGS) -D_HOT_TIME_BITS=2 $(CPPFLAGS) -o $@ tests/test_ext.c $(LIB)

tests/spec_vet: tests/spec_vet.c
	$(CC) -std=c11 $(WARN) $(CFLAGS) $(CPPFLAGS) -o $@ $<

tests/golden_hot: tests/golden_hot.c src/hot.c include/hot/time.h
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -o $@ tests/golden_hot.c src/hot.c

tests/fuzz_hot: tests/fuzz_hot.c src/hot.c include/hot/time.h
	$(CC) -std=c11 $(WARN) $(SANFLAGS) $(CFLAGS) $(HOTFLAGS) $(CPPFLAGS) \
	    -o $@ tests/fuzz_hot.c src/hot.c

fuzz: tests/fuzz_hot
	ASAN_OPTIONS=detect_leaks=0 ./tests/fuzz_hot

golden: tests/golden_hot
	./tests/golden_hot

golden-update: tests/golden_hot
	./tests/golden_hot --update

gate:
	scripts/cast-gate.sh src include tests

ci:
	scripts/ci.sh

check: all
	./tests/test_hot
	./tests/spec_vet
	./tests/test_wire
	./tests/test_store
	./tests/test_ext
	./tests/golden_hot
	ASAN_OPTIONS=detect_leaks=0 ./tests/fuzz_hot

clean:
	rm -f $(LIB) $(OBJS) $(BINS)

.PHONY: all check clean fuzz golden golden-update gate ci