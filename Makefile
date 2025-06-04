CC=gcc
WINDRES=windres

EXE=texp
OBJECTS=texp.o clib.o

INCS=
LIBS=
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
CFLAGS+= $(INCS)

ifeq ($(OS), Windows_NT)
	EXE=texp.exe
	OBJECTS+= strptime.o
	CFLAGS+= -DWINDOWS
	LIBS+= -lregex
endif

all: $(EXE)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(EXE): $(OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -rf $(EXE) $(OBJECTS)

