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

arena_t new_arena(u64 cap) {
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
void *arena_alloc(arena_t *a, usize size) {
    if (a->pos + size > a->cap)
        panic("arena_alloc() not enough memory");

    char *p = (char*)a->base + a->pos;
    a->pos += size;
    return (void*) p;
//    return malloc(size);
}
void arena_reset(arena_t *a) {
    a->pos = 0;
}

s8 s8_new(arena_t *a, char *s) {
    s8 retstr;
    retstr.len = strlen(s);
    retstr.data = arena_alloc(a, retstr.len+1);
    strncpy(retstr.data, s, retstr.len);
    retstr.data[retstr.len] = 0;
    return retstr;
}
s8 s8_dup(arena_t *a, s8 src) {
    s8 retstr;
    retstr.len = src.len;
    retstr.data = arena_alloc(a, src.len+1);
    strncpy((char*) retstr.data, (char*) src.data, src.len);
    retstr.data[retstr.len] = 0;
    return retstr;
}

strtbl_t new_strtbl(arena_t *a, u16 cap) {
    strtbl_t st;
    st.base = arena_alloc(a, sizeof(s8) * cap);
    st.base[0] = s8("");
    st.len = 1;
    st.cap = cap;
    return st;
}
void strtbl_reset(strtbl_t *st) {
    st->base[0] = s8("");
    st->len = 1;
}
u16 strtbl_add(strtbl_t *st, s8 str) {
    assert(st->len < st->cap);
    if (st->len >= st->cap) {
        fprintf(stderr, "strtbl_add() Exceeded capacity %d\n", st->cap);
        abort();
    }
    st->base[st->len] = str;
    st->len++;
    return st->len-1;
}
void strtbl_replace(strtbl_t *st, u16 idx, s8 str) {
    assert(idx < st->len);
    if (idx >= st->len)
        return;
    st->base[idx] = str;
}
s8 strtbl_get(strtbl_t *st, u16 idx) {
    if (idx >= st->len)
        return s8("");
    return st->base[idx];
}
u16 strtbl_find(strtbl_t *st, s8 str) {
    for (int i=1; i < st->len; i++) {
        if (strcmp(str.data, st->base[i].data) == 0)
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
