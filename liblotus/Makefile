CC ?= cc
AR ?= ar
RANLIB ?= ranlib

CFLAGS ?= -Wall -Wextra -O2
CFLAGS += -I.

LIB = liblotus.a
OBJS = lotus.o

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

lotus.o: lotus.c lotus.h
	$(CC) $(CFLAGS) -c lotus.c -o lotus.o

clean:
	rm -f $(OBJS) $(LIB) test

test: $(LIB) test.c
	$(CC) $(CFLAGS) test.c $(LIB) -o test
	./test

.PHONY: all clean test
