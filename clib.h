#ifndef CLIB_H
#define CLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SIZE_MB      1024*1024
#define SIZE_TINY    512
#define SIZE_SMALL   1024
#define SIZE_MEDIUM  32768
#define SIZE_LARGE   (1024*1024)
#define SIZE_HUGE    (1024*1024*1024)

#define ISO_DATE_LEN 10

#define countof(v) (sizeof(v) / sizeof((v)[0]))
#define lengthof(s) (countof(s) - 1)
#define memzero(p, v) (memset(p, 0, sizeof(v)))

/*
char: -128 to 127
short (16): -32,768 to 32,767
int (32): -2,147,483,648 to 2,147,483,647
long (32-64)

unsigned char: 0 to 255
unsigned short (16): 0 to 65535
unsigned int (32): 0 to 4,294,967,295
unsigned long (32-64)

float (32)
double (64)
*/

typedef ptrdiff_t idx_t;

void panic(char *s);
void panic_err(char *s);
void print_error(const char *s);

typedef struct {
    void *base;
    unsigned long pos;
    unsigned long cap;
} arena_t;

arena_t new_arena(unsigned long cap);
void free_arena(arena_t a);
void arena_reset(arena_t *a);
void *arena_alloc(arena_t *a, unsigned long size);

#define STR(sz) (str_t){(char *)sz, (countof(sz)-1)}
typedef struct {
    char *bytes;
    short len;
} str_t;

str_t str_new(arena_t *a, char *sz);
str_t str_dup(arena_t *a, str_t src);

typedef struct {
    arena_t *arena;
    str_t *base;
    short cap;
    short len;
} strtbl_t;

strtbl_t new_strtbl(arena_t *a, short cap);
void strtbl_reset(strtbl_t *st);
short strtbl_add(strtbl_t *st, str_t s);
void strtbl_replace(strtbl_t *st, short idx, str_t s);
str_t strtbl_get(strtbl_t *st, short idx);
short strtbl_find(strtbl_t *st, str_t s);

time_t date_from_iso(char *isodate);

#endif
