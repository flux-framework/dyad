AR := ar
CFLAGS := -g -O0 -fpic -I./

all: libtap.a

libtap.a: tap.o
	$(AR) -rc $@ $^
	ranlib $@

tap.o: tap.c tap.h
	$(CC) $(CFLAGS) $< -c -o $@

.PHONY: clean

clean:
	@rm -f *.o *~ libtap.a
