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
    short descid;
    float amt;
    short catid;
} exp_t;

typedef struct {
    arena_t *arena;
    exp_t *base;
    short cap;
    short len;

    strtbl_t strings;
    strtbl_t cats;
} exptbl_t;

int file_exists(const char *file);
void load_expense_file(const char *srcfile);
static exp_t read_expense(char *buf, exptbl_t *xps);

static void chomp(char *buf);
static char *skip_ws(char *startp);
static char *next_field(char *startp);

void init_exptbl(exptbl_t *et, arena_t *a, short cap);
short add_exp(exptbl_t *et, exp_t exp);
void replace_exp(exptbl_t *et, short idx, exp_t exp);
exp_t *get_exp(exptbl_t *et, short idx);

arena_t main_arena;
exptbl_t exps;

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

    init_arena(&main_arena, SIZE_LARGE);

    printf("load_expense_file()\n");
    load_expense_file(expenses_text_file);
    printf("Number of expenses read: %d\n", exps.len);

/*
    printf("expense_strings:\n");
    for (int i=1; i < exps.strings.len; i++) {
        str_t s = strtbl_get(&exps.strings, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
*/
    printf("exps:\n");
    for (int i=0; i < exps.len; i++) {
        exp_t xp = exps.base[i];
        str_t desc = strtbl_get(&exps.strings, xp.descid);
        str_t catname = strtbl_get(&exps.cats, xp.catid);
        printf("%d: '%.*s' %.2f '%.*s'\n", i, desc.len, desc.bytes, xp.amt, catname.len, catname.bytes);
    }
    printf("cats:\n");
    for (int i=1; i < exps.cats.len; i++) {
        str_t s = strtbl_get(&exps.cats, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
}

int file_exists(const char *file) {
    struct stat st;
    if (stat(file, &st) == 0)
        return 1;
    else
        return 0;
}

void init_exptbl(exptbl_t *et, arena_t *a, short cap) {
    et->arena = a;
    et->base = aalloc(a, sizeof(exp_t) * cap);
    et->len = 0;
    et->cap = cap;
    init_strtbl(&et->strings, a, 512);
    init_strtbl(&et->cats, a, 8);
}
short add_exp(exptbl_t *et, exp_t exp) {
    assert(et->cap > 0);
    assert(et->len >= 0);

    // If out of space, double the capacity.
    // Create a new memory block with double capacity and copy existing string table to it.
    if (et->len >= et->cap) {
        if (et->cap == SHRT_MAX) {
            fprintf(stderr, "add_exp() Maximum capacity reached %d\n", et->cap);
            abort();
        }
        int newcap = (int)et->cap * 2;
        if (newcap > SHRT_MAX)
            newcap = SHRT_MAX;

        exp_t *newbase = aalloc(et->arena, sizeof(exp_t) * newcap);
        memcpy(newbase, et->base, sizeof(exp_t) * et->cap);
        et->base = newbase;
        et->cap = newcap;
    }

    et->base[et->len] = exp;
    et->len++;
    return et->len-1;
}
void replace_exp(exptbl_t *et, short idx, exp_t exp) {
    assert(idx < et->len);
    if (idx >= et->len)
        return;
    et->base[idx] = exp;
}
exp_t *get_exp(exptbl_t *et, short idx) {
    if (idx >= et->len)
        return NULL;
    return &et->base[idx];
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

    reset_arena(&main_arena);
    init_exptbl(&exps, &main_arena, 1000);

    while (1) {
        errno = 0;
        char *pz = fgets(buf, sizeof(buf), f);
        if (pz == NULL)
            break;

        chomp(buf);
        if (strlen(buf) == 0)
            continue;

        exp_t exp = read_expense(buf, &exps);
        add_exp(&exps, exp);
    }

    fclose(f);
}


static exp_t read_expense(char *buf, exptbl_t *xps) {
    exp_t retexp;
    arena_t *arena = xps->arena;
    strtbl_t *strings = &xps->strings;
    strtbl_t *cats = &xps->cats;

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
    retexp.descid = strtbl_add(strings, new_str(arena, pfield));

    // amount
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.amt = atof(pfield);

    // category
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.catid = strtbl_find(cats, (str_t){pfield, strlen(pfield)});
    if (retexp.catid == 0) {
        retexp.catid = strtbl_add(cats, new_str(arena, pfield));
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
