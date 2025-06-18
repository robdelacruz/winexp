#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
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

void init_arena(arena_t *a, unsigned long cap) {
    if (cap == 0)
        cap = SIZE_MEDIUM;

    a->base = malloc(cap);
    if (!a->base)
        panic("Not enough memory to initialize arena");

    a->pos = 0;
    a->cap = cap;
}
void free_arena(arena_t *a) {
    free(a->base);
}
void reset_arena(arena_t *a) {
    a->pos = 0;
}
void *aalloc(arena_t *a, unsigned long size) {
    if (a->pos + size > a->cap)
        panic("aalloc() not enough memory");

    char *p = (char*)a->base + a->pos;
    a->pos += size;
    return (void*) p;
}

str_t new_str(arena_t *a, const char *s) {
    str_t retstr;
    retstr.len = strlen(s);
    retstr.bytes = aalloc(a, retstr.len+1);
    strncpy(retstr.bytes, s, retstr.len);
    retstr.bytes[retstr.len] = 0;
    return retstr;
}
str_t dup_str(arena_t *a, str_t src) {
    str_t retstr;
    retstr.len = src.len;
    retstr.bytes = aalloc(a, src.len+1);
    strncpy((char*) retstr.bytes, (char*) src.bytes, src.len);
    retstr.bytes[retstr.len] = 0;
    return retstr;
}
int str_equals(str_t s, const char *sz) {
    return !strcmp(s.bytes, sz);
}

void init_strtbl(strtbl_t *st, arena_t *a, short cap) {
    if (cap == 0)
        cap = SIZE_TINY;

    st->arena = a;
    st->base = aalloc(a, sizeof(str_t) * cap);
    st->base[0] = STR("");
    st->len = 1;
    st->cap = cap;
}
strtbl_t dup_strtbl(strtbl_t st, arena_t *a) {
    strtbl_t dupst;
    init_strtbl(&dupst, a, st.cap);
    dupst.len = st.len;
    memcpy(dupst.base, st.base, sizeof(str_t) * st.cap);
    return dupst;
}
short strtbl_add(strtbl_t *st, const char *s) {
    assert(st->cap > 0);
    assert(st->len >= 0);

    // If out of space, double the capacity.
    // Create a new memory block with double capacity and copy existing string table to it.
    if (st->len >= st->cap) {
        if (st->cap == SHRT_MAX) {
            fprintf(stderr, "strtbl_add() Maximum capacity reached %d\n", st->cap);
            abort();
        }
        int newcap = (int)st->cap * 2;
        if (newcap > SHRT_MAX)
            newcap = SHRT_MAX;

        str_t *newbase = aalloc(st->arena, sizeof(str_t) * newcap);
        memcpy(newbase, st->base, sizeof(str_t) * st->cap);
        st->base = newbase;
        st->cap = newcap;
    }

    st->base[st->len] = new_str(st->arena, s);
    st->len++;
    return st->len-1;
}
void strtbl_replace(strtbl_t *st, short idx, const char *s) {
    assert(idx < st->len);
    if (idx >= st->len)
        return;
    st->base[idx] = new_str(st->arena, s);
}
str_t strtbl_get(strtbl_t st, short idx) {
    if (idx >= st.len)
        return STR("");
    return st.base[idx];
}
short strtbl_find(strtbl_t st, const char *s) {
    for (int i=1; i < st.len; i++) {
        if (strcmp(s, st.base[i].bytes) == 0)
            return i;
    }
    return 0;
}

static void swap_str(str_t *strs, int i, int j) {
    str_t tmp = strs[i];
    strs[i] = strs[j];
    strs[j] = tmp;
}
static int sort_strtbl_partition(strtbl_t *t, int start, int end, cmpfunc_t cmp) {
    int imid = start;
    str_t pivot = t->base[end];

    for (int i=start; i < end; i++) {
        if (cmp(&t->base[i], &pivot) < 0) {
            swap_str(t->base, imid, i);
            imid++;
        }
    }
    swap_str(t->base, imid, end);
    return imid;
}
void sort_strtbl_part(strtbl_t *t, int start, int end, cmpfunc_t cmp) {
    if (start >= end)
        return;
    int pivot = sort_strtbl_partition(t, start, end, cmp);
    sort_strtbl_part(t, start, pivot-1, cmp);
    sort_strtbl_part(t, pivot+1, end, cmp);
}
void sort_strtbl(strtbl_t *t, cmpfunc_t cmp) {
    // [0] element is always "" so don't include in sorting.
    sort_strtbl_part(t, 1, t->len-1, cmp);
}
int cmp_str(void *a, void *b) {
    str_t *stra = a;
    str_t *strb = b;
    return strcmp(stra->bytes, strb->bytes);
}

void init_entrytbl(entrytbl_t *t, arena_t *a, short cap) {
    if (cap == 0)
        cap = SIZE_TINY;

    t->arena = a;
    t->base = aalloc(a, sizeof(entry_t) * cap);
    t->len = 0;
    t->cap = cap;
}
short entrytbl_add(entrytbl_t *t, entry_t e) {
    assert(t->cap > 0);
    assert(t->len >= 0);

    // If out of space, double the capacity.
    // Create a new memory block with double capacity and copy existing entry table to it.
    if (t->len >= t->cap) {
        if (t->cap == SHRT_MAX) {
            fprintf(stderr, "entrytbl_add() Maximum capacity reached %d\n", t->cap);
            abort();
        }
        int newcap = (int)t->cap * 2;
        if (newcap > SHRT_MAX)
            newcap = SHRT_MAX;

        entry_t *newbase = aalloc(t->arena, sizeof(entry_t) * newcap);
        memcpy(newbase, t->base, sizeof(entry_t) * t->cap);
        t->base = newbase;
        t->cap = newcap;
    }

    t->base[t->len] = e;
    t->len++;
    return t->len-1;
}
static void swap_entry(entry_t *entries, int i, int j) {
    entry_t tmp = entries[i];
    entries[i] = entries[j];
    entries[j] = tmp;
}
static int sort_entrytbl_partition(entrytbl_t *t, int start, int end, cmpfunc_t cmp) {
    int imid = start;
    entry_t pivot = t->base[end];

    for (int i=start; i < end; i++) {
        if (cmp(&t->base[i], &pivot) < 0) {
            swap_entry(t->base, imid, i);
            imid++;
        }
    }
    swap_entry(t->base, imid, end);
    return imid;
}
void sort_entrytbl_part(entrytbl_t *t, int start, int end, cmpfunc_t cmp) {
    if (start >= end)
        return;
    int pivot = sort_entrytbl_partition(t, start, end, cmp);
    sort_entrytbl_part(t, start, pivot-1, cmp);
    sort_entrytbl_part(t, pivot+1, end, cmp);
}
void sort_entrytbl(entrytbl_t *t, cmpfunc_t cmp) {
    sort_entrytbl_part(t, 0, t->len-1, cmp);
}
int cmp_entry_val(void *a, void *b) {
    entry_t *entrya = a;
    entry_t *entryb = b;
    if (entrya->val < entryb->val) return 1;
    if (entrya->val > entryb->val) return -1;
    return 0;
}
int cmp_entry_desc(void *a, void *b) {
    entry_t *entrya = a;
    entry_t *entryb = b;
    return strcasecmp(entrya->desc.bytes, entryb->desc.bytes);
}

time_t date_today() {
    return time(NULL);
}
time_t date_from_cal(short year, short month, short day) {
    time_t t;
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    tm.tm_year = year - 1900;
    tm.tm_mon = month-1;
    tm.tm_mday = day;
    t = mktime(&tm);
    if (t == -1) {
        fprintf(stderr, "date_from_cal(%d, %d, %d) mktime() error\n", year, month, day);
        return 0;
    }
    return t;
}
time_t date_from_iso(char *isodate) {
    time_t t;
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    if (strptime(isodate, "%F", &tm) == NULL) {
        fprintf(stderr, "date_from_iso('%s') strptime() error\n", isodate);
        return 0;
    }
    t = mktime(&tm);
    if (t < 0) {
        fprintf(stderr, "date_assign_iso('%s') mktime() error\n", isodate);
        return 0;
    }
    return t;
}
void date_strftime(time_t dt, const char *fmt, char *buf, size_t buf_len) {
    struct tm tm;
    localtime_r(&dt, &tm);
    strftime(buf, buf_len, fmt, &tm);
}
void date_to_iso(time_t dt, char *buf, size_t buf_len) {
    struct tm tm;
    localtime_r(&dt, &tm);
    strftime(buf, buf_len, "%F", &tm);
}
void date_to_cal(time_t dt, short *retyear, short *retmonth, short *retday) {
    struct tm tm;
    localtime_r(&dt, &tm);
    if (retyear)
        *retyear = tm.tm_year + 1900;
    if (retmonth)
        *retmonth = tm.tm_mon+1;
    if (retday)
        *retday = tm.tm_mday;
}
time_t date_prev_month(time_t dt) {
    short year, month, day;
    date_to_cal(dt, &year, &month, &day);

    if (year == 0)
        return dt;

    if (month == 1)
        return date_from_cal(year-1, 12, day);
    else
        return date_from_cal(year, month-1, day);
}
time_t date_next_month(time_t dt) {
    short year, month, day;
    date_to_cal(dt, &year, &month, &day);

    if (month == 12)
        return date_from_cal(year+1, 1, day);
    else
        return date_from_cal(year, month+1, day);
}
time_t date_prev_day(time_t dt) {
    return dt - 24*60*60;
}
time_t date_next_day(time_t dt) {
    return dt + 24*60*60;
}
