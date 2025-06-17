#ifndef CLIB_H
#define CLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef WINDOWS
char* strptime(const char *buf, const char *fmt, struct tm *tm);
#define localtime_r(t, tm) (localtime_s(tm, t) == 0 ? tm : NULL)
#endif

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
int szequals(const char *s1, const char *s2);
#define szequals(s1, s2) (!strcmp(s1, s2))

typedef struct {
    void *base;
    unsigned long pos;
    unsigned long cap;
} arena_t;

void init_arena(arena_t *a, unsigned long cap);
void free_arena(arena_t *a);
void reset_arena(arena_t *a);
void *aalloc(arena_t *a, unsigned long size);

#define STR(sz) (str_t){(char *)sz, (countof(sz)-1)}
typedef struct {
    char *bytes;
    short len;
} str_t;

str_t new_str(arena_t *a, char *sz);
str_t dup_str(arena_t *a, str_t src);
int str_equals(str_t s, char *sz);

typedef int (*cmpfunc_t)(void *a, void *b);

typedef struct {
    arena_t *arena;
    str_t *base;
    short cap;
    short len;
} strtbl_t;

void init_strtbl(strtbl_t *st, arena_t *a, short cap);
strtbl_t dup_strtbl(strtbl_t st, arena_t *a);
short strtbl_add(strtbl_t *st, str_t s);
void strtbl_replace(strtbl_t *st, short idx, str_t s);
str_t strtbl_get(strtbl_t st, short idx);
short strtbl_find(strtbl_t st, str_t s);

void sort_strtbl(strtbl_t *t, cmpfunc_t cmp);
int cmp_str(void *a, void *b);

#define ENTRY(sz, f) (entry_t){STR(sz), f}
typedef struct {
    str_t desc;
    float val;
} entry_t;

typedef struct {
    arena_t *arena;
    entry_t *base;
    short cap;
    short len;
} entrytbl_t;

void init_entrytbl(entrytbl_t *t, arena_t *a, short cap);
short entrytbl_add(entrytbl_t *t, entry_t e);

void sort_entrytbl(entrytbl_t *t, cmpfunc_t cmp);
int cmp_entry_val(void *a, void *b);
int cmp_entry_desc(void *a, void *b);

time_t date_today();
time_t date_from_cal(short year, short month, short day);
time_t date_from_iso(char *isodate);
void date_strftime(time_t dt, const char *fmt, char *buf, size_t buf_len);
void date_to_iso(time_t dt, char *buf, size_t buf_len);
void date_to_cal(time_t dt, short *retyear, short *retmonth, short *retday);
time_t date_prev_month(time_t dt);
time_t date_next_month(time_t dt);
time_t date_prev_day(time_t dt);
time_t date_next_day(time_t dt);

#endif
