CC=gcc
WINDRES=windres

EXE=t
OBJECTS=t.o clib.o

INCS=
LIBS=-lregex
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable $(INCS)
LDFLAGS=$(LIBS)

ifeq ($(OS), Windows_NT)
	EXE=t.exe
	OBJECTS+= strptime.o
	CFLAGS+= -DWINDOWS
endif

all: $(EXE)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

t.exe: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(EXE) $(OBJECTS)

