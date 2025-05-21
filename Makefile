CC=gcc
WINDRES=windres

OBJECTS=t.o clib.o strptime.o

INCS=
LIBS=
CFLAGS=-std=gnu99 -Wall -Werror -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable $(INCS)
LDFLAGS=$(LIBS)

all: t.exe

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

t.exe: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf t.exe $(OBJECTS)

