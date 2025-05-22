#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "clib.h"

void panic(char *s) {
    if (s)
        fprintf(stderr, "%s\n", s);
    abort();
}
void panic_err(char *s) {
    if (s)
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    abort();
}
void print_error(const char *s) {
    if (s)
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else
        fprintf(stderr, "%s\n", strerror(errno));
}

arena_t new_arena(unsigned long cap) {
    arena_t a; 

    if (cap == 0)
        cap = SIZE_MEDIUM;

    a.base = malloc(cap);
    if (!a.base)
        panic("Not enough memory to initialize arena");

    a.pos = 0;
    a.cap = cap;
    return a;
}
void free_arena(arena_t a) {
    free(a.base);
}
void arena_reset(arena_t *a) {
    a->pos = 0;
}
void *arena_alloc(arena_t *a, unsigned long size) {
    if (a->pos + size > a->cap)
        panic("arena_alloc() not enough memory");

    char *p = (char*)a->base + a->pos;
    a->pos += size;
    return (void*) p;
//    return malloc(size);
}

str_t str_new(arena_t *a, char *sz) {
    str_t retstr;
    retstr.len = strlen(sz);
    retstr.bytes = arena_alloc(a, retstr.len+1);
    strncpy(retstr.bytes, sz, retstr.len);
    retstr.bytes[retstr.len] = 0;
    return retstr;
}
str_t str_dup(arena_t *a, str_t src) {
    str_t retstr;
    retstr.len = src.len;
    retstr.bytes = arena_alloc(a, src.len+1);
    strncpy((char*) retstr.bytes, (char*) src.bytes, src.len);
    retstr.bytes[retstr.len] = 0;
    return retstr;
}

strtbl_t new_strtbl(arena_t *a, short cap) {
    if (cap == 0)
        cap = SIZE_TINY;

    strtbl_t st;
    st.arena = a;
    st.base = arena_alloc(a, sizeof(str_t) * cap);
    st.base[0] = STR("");
    st.len = 1;
    st.cap = cap;
    return st;
}
void strtbl_reset(strtbl_t *st) {
    st->base[0] = STR("");
    st->len = 1;
}
short strtbl_add(strtbl_t *st, str_t s) {
    assert(st->cap > 0);
    assert(st->len >= 0);

    // If we're out of space, double the capacity.
    // Create a new memory block with double capacity and copy existing string table to it.
    if (st->len >= st->cap) {
        if (st->cap == SHRT_MAX) {
            fprintf(stderr, "strtbl_add() Maximum capacity reached %d\n", st->cap);
            abort();
        }
        int newcap = (int)st->cap * 2;
        if (newcap > SHRT_MAX)
            newcap = SHRT_MAX;

        str_t *newbase = arena_alloc(st->arena, sizeof(str_t) * newcap);
        memcpy(newbase, st->base, sizeof(str_t) * st->cap);
        st->base = newbase;
        st->cap = newcap;
    }

    st->base[st->len] = s;
    st->len++;
    return st->len-1;
}
void strtbl_replace(strtbl_t *st, short idx, str_t s) {
    assert(idx < st->len);
    if (idx >= st->len)
        return;
    st->base[idx] = s;
}
str_t strtbl_get(strtbl_t *st, short idx) {
    if (idx >= st->len)
        return STR("");
    return st->base[idx];
}
short strtbl_find(strtbl_t *st, str_t s) {
    for (int i=1; i < st->len; i++) {
        if (strcmp(s.bytes, st->base[i].bytes) == 0)
            return i;
    }
    return 0;
}

char* strptime(const char *buf, const char *fmt, struct tm *tm);

time_t date_from_iso(char *isodate) {
    time_t t;
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    if (strptime(isodate, "%F", &tm) == NULL) {
        fprintf(stderr, "date_from_iso('%s') strptime() error\n", isodate);
        return 0;
    }
    t = mktime(&tm);
    if (t == -1) {
        fprintf(stderr, "date_assign_iso('%s') mktime() error\n", isodate);
        return 0;
    }
    return t;
}
