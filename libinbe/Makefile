CC ?= cc
AR ?= ar
RANLIB ?= ranlib

CFLAGS ?= -Wall -Wextra -O2
CFLAGS += -I.

LIB = libinbe.a
OBJS = inbe.o

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

inbe.o: inbe.c inbe.h
	$(CC) $(CFLAGS) -c inbe.c -o inbe.o

clean:
	rm -f $(OBJS) $(LIB) test

test: $(LIB) test.c
	$(CC) $(CFLAGS) test.c $(LIB) -o test
	./test

.PHONY: all clean test
