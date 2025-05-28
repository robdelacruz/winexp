#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
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

typedef struct {
    short year;
    short month;
    short day;
} shortdate_t;

int file_exists(const char *file);
void load_expense_file(const char *srcfile, exptbl_t *exps);
static exp_t read_expense(char *buf, exptbl_t *exps);

static void chomp(char *buf);
static char *skip_ws(char *startp);
static char *next_field(char *startp);

void init_exptbl(exptbl_t *et, arena_t *a, short cap);
short add_exp(exptbl_t *et, exp_t exp);
void replace_exp(exptbl_t *et, short idx, exp_t exp);
exp_t *get_exp(exptbl_t *et, short idx);

int is_cat(char *s);
int is_yyyy(char *s);
int is_yyyy_mm(char *s);
int is_yyyy_mm_dd(char *s);

int match_date(char *sz, shortdate_t *sd, arena_t scratch);

arena_t main_arena;
arena_t scratch_arena;
exptbl_t exps;

regex_t reg_yyyy, reg_yyyy_mm, reg_yyyy_mm_dd;

/*
exp list [cat]
exp list [cat] date
exp list [cat] date date

date = yyyy or yyyy-mm or yyyy-mm-dd

*/

int main(int argc, char *argv[]) {
    char *expenses_text_file=NULL;
    char *argcat=NULL;
    char *startdate=NULL, *enddate=NULL;
    char *yyyy=NULL, *yyyy_mm=NULL, *yyyy_mm_dd=NULL;
    int iarg;
    int z;

    z = regcomp(&reg_yyyy, "^[0-9]{4}$", REG_EXTENDED);
    assert(z == 0);
    z = regcomp(&reg_yyyy_mm, "^([0-9]{4})-([0-9]{2})$", REG_EXTENDED);
    assert(z == 0);
    z = regcomp(&reg_yyyy_mm_dd, "^([0-9]{4})-([0-9]{2})-([0-9]{2})$", REG_EXTENDED);
    assert(z == 0);

    if (argc < 2) {
        printf("Enter a date (yyyy) (yyyy-mm) or (yyyy-mm-dd)\n");
        exit(0);
    }

    init_arena(&scratch_arena, SIZE_MEDIUM);
    init_arena(&main_arena, SIZE_LARGE);

    shortdate_t sd;
    printf("match_date('%s')\n", argv[1]);
    z = match_date(argv[1], &sd, scratch_arena);
    if (z == 0) {
        printf("No match\n");
    } else {
        printf("Match: year=%d month=%d day=%d\n", sd.year, sd.month, sd.day);
    }

/*
    if (argc < 2) {
        printf("Usage: %s <expenses_text_file>\n\n", argv[0]);
        exit(0);
    }

    iarg = 1;
    expenses_text_file = argv[iarg];
    if (!file_exists(expenses_text_file)) {
        fprintf(stderr, "Expense file '%s' not found.\n", expenses_text_file);
        exit(1);
    }

    init_arena(&scratch_arena, SIZE_MEDIUM);
    init_arena(&main_arena, SIZE_LARGE);
    load_expense_file(expenses_text_file, &exps);

    time_t today = date_today();
    short y, m, d;
    date_to_cal(today, &y, &m, &d);
    time_t start_month = date_from_cal(y, m, 1);
    time_t next_month = date_next_month(start_month);

    char sdate[ISO_DATE_LEN+1];
    date_to_iso(start_month, sdate, sizeof(sdate));
    printf("Date range: %s - ", sdate);
    date_to_iso(next_month, sdate, sizeof(sdate));
    printf("%s\n\n", sdate);

    for (int i=0; i < exps.len; i++) {
        exp_t xp = exps.base[i];
        if (xp.date >= start_month && xp.date < next_month) {
            str_t desc = strtbl_get(&exps.strings, xp.descid);
            str_t catname = strtbl_get(&exps.cats, xp.catid);
            date_to_iso(xp.date, sdate, sizeof(sdate));

            printf("%-12s %-30s %9.2f  %-10s  #%-5d\n", sdate, desc.bytes, xp.amt, catname.bytes, i);
        }
    }
*/

    regfree(&reg_yyyy);
    regfree(&reg_yyyy_mm);
    regfree(&reg_yyyy_mm_dd);
}

void print_tables() {
    printf("expense_strings:\n");
    for (int i=1; i < exps.strings.len; i++) {
        str_t s = strtbl_get(&exps.strings, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
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

int match_date(char *sz, shortdate_t *sd, arena_t scratch) {
    int z;
    regmatch_t match[4];
    str_t scopy = new_str(&scratch, sz);

    sd->year = 0;
    sd->month = 0;
    sd->day = 0;

    z = regexec(&reg_yyyy, sz, 0, NULL, 0);
    if (z == 0) {
        sd->year = atoi(sz);
        return 1;
    }
    z = regexec(&reg_yyyy_mm, sz, 3, match, 0);
    if (z == 0) {
        assert(match[1].rm_so >= 0);
        assert(match[1].rm_eo >= 0);
        assert(match[2].rm_so >= 0);
        assert(match[2].rm_eo >= 0);

        scopy.bytes[match[1].rm_eo] = 0;
        sd->year = atoi(scopy.bytes + match[1].rm_so);
        scopy.bytes[match[2].rm_eo] = 0;
        sd->month = atoi(scopy.bytes + match[2].rm_so);
        return 1;
    }
    z = regexec(&reg_yyyy_mm_dd, sz, 4, match, 0);
    if (z == 0) {
        assert(match[1].rm_so >= 0);
        assert(match[1].rm_eo >= 0);
        assert(match[2].rm_so >= 0);
        assert(match[2].rm_eo >= 0);
        assert(match[3].rm_so >= 0);
        assert(match[3].rm_eo >= 0);

        scopy.bytes[match[1].rm_eo] = 0;
        sd->year = atoi(scopy.bytes + match[1].rm_so);
        scopy.bytes[match[2].rm_eo] = 0;
        sd->month = atoi(scopy.bytes + match[2].rm_so);
        scopy.bytes[match[3].rm_eo] = 0;
        sd->day = atoi(scopy.bytes + match[3].rm_so);
        return 1;
    }
    return 0;
}

int is_cat(char *s) {
    if (is_yyyy(s) || is_yyyy_mm(s) || is_yyyy_mm_dd(s))
        return 0;
    return 1;
}
int is_yyyy(char *s) {
    int z = regexec(&reg_yyyy, s, 0, NULL, 0);
    if (z == 0) return 1; // match
    return 0;
}
int is_yyyy_mm(char *s) {
    int z = regexec(&reg_yyyy_mm, s, 0, NULL, 0);
    if (z == 0) return 1; // match
    return 0;
}
int is_yyyy_mm_dd(char *s) {
    int z = regexec(&reg_yyyy_mm_dd, s, 0, NULL, 0);
    if (z == 0) return 1; // match
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
void load_expense_file(const char *srcfile, exptbl_t *exps) {
    FILE *f;
    char buf[BUFLINE_SIZE];

    f = fopen(srcfile, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening '%s'.\n", srcfile);
        return;
    }

    reset_arena(&main_arena);
    init_exptbl(exps, &main_arena, 1000);

    while (1) {
        errno = 0;
        char *pz = fgets(buf, sizeof(buf), f);
        if (pz == NULL)
            break;

        chomp(buf);
        if (strlen(buf) == 0)
            continue;

        exp_t exp = read_expense(buf, exps);
        add_exp(exps, exp);
    }

    fclose(f);
}


static exp_t read_expense(char *buf, exptbl_t *exps) {
    exp_t retexp;
    arena_t *arena = exps->arena;
    strtbl_t *strings = &exps->strings;
    strtbl_t *cats = &exps->cats;

    // Sample expense line:
    // 2016-05-01; 00:00; Mochi Cream coffee; 100.00; coffee

    char *pfield;
    char *nextp;

    // date
    pfield = buf;
    nextp = next_field(pfield);
    retexp.date = date_from_iso(pfield);

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
