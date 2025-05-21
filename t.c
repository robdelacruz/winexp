#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "clib.h"

typedef struct {
    time_t date;
    u16 descid;
    float amt;
    u8 catid;
} exp_t;

typedef struct {
    exp_t *base;
    u16 cap;
    u16 len;
} exptbl_t;

int file_exists(const char *file);
void load_expense_file(const char *srcfile);
static exp_t read_expense(char *buf);

static void chomp(char *buf);
static char *skip_ws(char *startp);
static char *next_field(char *startp);

exptbl_t new_exptbl(arena_t *a, u16 cap);
u16 exptbl_add(exptbl_t *tbl, exp_t exp);
void exptbl_replace(exptbl_t *tbl, u16 idx, exp_t exp);
exp_t *exptbl_get(exptbl_t *tbl, u16 idx);

arena_t arena1;
exptbl_t xps;
strtbl_t expense_strings;
strtbl_t cats;

int main(int argc, char *argv[]) {
    const char *expenses_text_file;

    if (argc < 2) {
        printf("Usage: %s <expenses_text_file>\n\n", argv[0]);
        exit(0);
    }

    expenses_text_file = argv[1];

    if (!file_exists(expenses_text_file)) {
        fprintf(stderr, "Expense file '%s' not found.\n", expenses_text_file);
        exit(1);
    }

    arena1 = new_arena(SIZE_LARGE);
    expense_strings = new_strtbl(&arena1, 15000);
    cats = new_strtbl(&arena1, 50);
    xps = new_exptbl(&arena1, 15000);

    printf("load_expense_file()\n");
    load_expense_file(expenses_text_file);
    printf("Number of expenses read: %d\n", xps.len);

    printf("load_expense_file()\n");
    load_expense_file(expenses_text_file);
    printf("Number of expenses read: %d\n", xps.len);

    printf("load_expense_file()\n");
    load_expense_file(expenses_text_file);
    printf("Number of expenses read: %d\n", xps.len);

//    printf("expense_strings:\n");
//    for (int i=1; i < expense_strings.len; i++) {
//        s8 s = strtbl_get(&expense_strings, i);
//        printf("[%d] '%.*s'\n", i, s.len, s.data);
//    }
    printf("cats:\n");
    for (int i=1; i < cats.len; i++) {
        s8 s = strtbl_get(&cats, i);
        printf("[%d] '%.*s'\n", i, s.len, s.data);
    }

    printf("xps:\n");
    for (int i=0; i < xps.len; i++) {
        exp_t xp = xps.base[i];
        s8 desc = strtbl_get(&expense_strings, xp.descid);
        s8 catname = strtbl_get(&cats, xp.catid);
        printf("%d: '%.*s' %.2f '%.*s'\n", i, desc.len, desc.data, xp.amt, catname.len, catname.data);
    }
}

int file_exists(const char *file) {
    struct stat st;
    if (stat(file, &st) == 0)
        return 1;
    else
        return 0;
}

exptbl_t new_exptbl(arena_t *a, u16 cap) {
    exptbl_t tbl;
    tbl.base = arena_alloc(a, sizeof(exp_t) * cap);
    tbl.len = 0;
    tbl.cap = cap;
    return tbl;
}
u16 exptbl_add(exptbl_t *tbl, exp_t exp) {
    assert(tbl->len < tbl->cap);
    tbl->base[tbl->len] = exp;
    tbl->len++;
    return tbl->len-1;
}
void exptbl_replace(exptbl_t *tbl, u16 idx, exp_t exp) {
    assert(idx < tbl->len);
    if (idx >= tbl->len)
        return;
    tbl->base[idx] = exp;
}
exp_t *exptbl_get(exptbl_t *tbl, u16 idx) {
    if (idx >= tbl->len)
        return NULL;
    return &tbl->base[idx];
}

#define BUFLINE_SIZE 1000
void load_expense_file(const char *srcfile) {
    FILE *f;
    char buf[BUFLINE_SIZE];

    f = fopen(srcfile, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening '%s'.\n", srcfile);
        return;
    }

//    arena_reset(&arena1);
    xps.len = 0;
    strtbl_reset(&expense_strings);
    strtbl_reset(&cats);

    while (1) {
        errno = 0;
        char *pz = fgets(buf, sizeof(buf), f);
        if (pz == NULL)
            break;

        chomp(buf);
        if (strlen(buf) == 0)
            continue;

        exp_t exp = read_expense(buf);
        exptbl_add(&xps, exp);
    }

    fclose(f);
}


static exp_t read_expense(char *buf) {
    exp_t retexp;

    // Sample expense line:
    // 2016-05-01; 00:00; Mochi Cream coffee; 100.00; coffee

    char *pfield;
    char *nextp;

    // date
    pfield = buf;
    nextp = next_field(pfield);
    retexp.date = date_from_iso(pfield);
    retexp.date = 0;

    // skip time field
    pfield = nextp;
    nextp = next_field(pfield);

    // description
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.descid = strtbl_add(&expense_strings, s8_new(&arena1, pfield));

    // amount
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.amt = atof(pfield);

    // category
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.catid = strtbl_find(&cats, (s8){pfield, strlen(pfield)});
    if (retexp.catid == 0) {
        retexp.catid = strtbl_add(&cats, s8_new(&arena1, pfield));
    }

    return retexp;
}

// Remove trailing \n or \r chars.
static void chomp(char *buf) {
    ssize_t buf_len = strlen(buf);
    for (int i=buf_len-1; i >= 0; i--) {
        if (buf[i] == '\n' || buf[i] == '\r')
            buf[i] = '\0';
    }
}
static char *skip_ws(char *startp) {
    char *p = startp;
    while (*p == ' ')
        p++;
    return p;
}
static char *next_field(char *startp) {
    char *p = startp;
    while (*p != '\0' && *p != ';')
        p++;

    if (*p == ';') {
        *p = '\0';
        return skip_ws(p+1);
    }

    return p;
}
