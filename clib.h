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
i8: -128 to 127
i16: -32,768 to 32,767
i32: -2,147,483,648 to 2,147,483,647

u8: 0 to 255
u16: 0 to 65535
u32: 0 to 4,294,967,295
*/

// Thanks https://nullprogram.com/blog/2023/10/08/
typedef signed char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
typedef i64 size;
typedef u64 usize;

void panic(char *s);
void panic_err(char *s);
void print_error(const char *s);

typedef struct {
    void *base;
    u64 pos;
    u64 cap;
} arena_t;

arena_t new_arena(u64 cap);
void free_arena(arena_t a);
void *arena_alloc(arena_t *a, usize size);
void arena_reset(arena_t *a);

// Thanks https://nullprogram.com/blog/2023/10/08/
#define s8(s) (s8){(char *)s, lengthof(s)}
typedef struct {
    char *data;
    i16 len;
} s8;

s8 s8_new(arena_t *a, char *s);
s8 s8_dup(arena_t *a, s8 src);

typedef struct {
    s8 *base;
    u16 cap;
    u16 len;
} strtbl_t;

strtbl_t new_strtbl(arena_t *a, u16 cap);
void strtbl_reset(strtbl_t *st);
u16 strtbl_add(strtbl_t *st, s8 str);
void strtbl_replace(strtbl_t *st, u16 idx, s8 str);
s8 strtbl_get(strtbl_t *st, u16 idx);
u16 strtbl_find(strtbl_t *st, s8 str);

time_t date_from_iso(char *isodate);

#endif
