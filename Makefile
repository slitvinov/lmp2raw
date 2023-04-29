.POSIX:
.SUFFIXES:
.SUFFIXES: .c
CFLAGS = -g -O2
M = main

all: $M
.c:
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
clean:; rm -f -- $M
