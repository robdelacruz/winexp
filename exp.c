#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "clib.h"
#include "exp.h"

static exp_t read_expense(char *buf, exptbl_t *et);
static void chomp(char *buf);
static char *skip_ws(char *startp);
static char *next_field(char *startp);

void init_exptbl(exptbl_t *et, short cap, arena_t *a) {
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

int cmp_exp_date(exptbl_t *et, void *a, void *b) {
    exp_t *expa = a;
    exp_t *expb = b;
    if (expa->date < expb->date) return -1;
    if (expa->date > expb->date) return 1;
    return 0;
}
int cmp_exp_cat(exptbl_t *et, void *a, void *b) {
    exp_t *expa = a;
    exp_t *expb = b;
    str_t cata = strtbl_get(et->cats, expa->catid);
    str_t catb = strtbl_get(et->cats, expb->catid);

    return strcasecmp(cata.bytes, catb.bytes);
}
static void swap_exp(exp_t *exps, int i, int j) {
    exp_t tmp = exps[i];
    exps[i] = exps[j];
    exps[j] = tmp;
}
static int sort_exptbl_partition(exptbl_t *et, int start, int end, exptbl_cmpfunc_t cmp) {
    int imid = start;
    exp_t pivot = et->base[end];

    for (int i=start; i < end; i++) {
        if (cmp(et, &et->base[i], &pivot) < 0) {
            swap_exp(et->base, imid, i);
            imid++;
        }
    }
    swap_exp(et->base, imid, end);
    return imid;
}
void sort_exptbl_part(exptbl_t *et, int start, int end, exptbl_cmpfunc_t cmp) {
    if (start >= end)
        return;
    int pivot = sort_exptbl_partition(et, start, end, cmp);
    sort_exptbl_part(et, start, pivot-1, cmp);
    sort_exptbl_part(et, pivot+1, end, cmp);
}
void sort_exptbl(exptbl_t *et, exptbl_cmpfunc_t cmp) {
    sort_exptbl_part(et, 0, et->len-1, cmp);
}

str_t get_expense_filename(arena_t *a) {
    char buf[2048];
    static char expenses_filename[] = "expenses";
    char *path;

    path = getenv("EXP2FILE");
    if (path != NULL && strlen(path) > 0)
        return new_str(a, path);

    // $WINEXPFILE not set, so read expense filename from home directory

#ifdef _WIN32
    path = getenv("USERPROFILE");
    if (path != NULL && strlen(path) > 0) {
        snprintf(buf, sizeof(buf), "%s\\%s", path, expenses_filename);
        return new_str(a, buf);
    }
    char *homedrive = getenv("HOMEDRIVE");
    char *homepath = getenv("HOMEPATH");
    if (homedrive != NULL && homepath != NULL) {
        snprintf(buf, sizeof(buf), "%s%s\\%s", homedrive, homepath, expenses_filename);
        return new_str(a, buf);
    }
    // Can't determine home directory on Windows, so just use current directory.
    return new_str(a, expenses_filename);
#else
    path = getenv("HOME");
    if (path == NULL || strlen(path) == 0)
        path = "~";
    snprintf(buf, sizeof(buf), "%s/%s", path, expenses_filename);
    return new_str(a, buf);
#endif
}

static int file_exists(const char *file) {
    struct stat st;
    if (stat(file, &st) == 0)
        return 1;
    else
        return 0;
}
// Create expense file if it doesn't exist.
void touch_expense_file(const char *expfile) {
    if (!file_exists(expfile)) {
        FILE *f = fopen(expfile, "a");
        if (f == NULL) {
            print_error("Error creating expense file");
            exit(1);
        }
        fclose(f);
        printf("Expense file created: %s\n", expfile);
    }
}

void load_expense_file(arena_t *exp_arena, arena_t scratch, exptbl_t *et) {
    FILE *f;
    char buf[1024];

    str_t expfile = get_expense_filename(&scratch);
    touch_expense_file(expfile.bytes);
    f = fopen(expfile.bytes, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening '%s': ", expfile.bytes);
        print_error(NULL);
        return;
    }

    init_exptbl(et, 100, exp_arena);
    while (1) {
        errno = 0;
        char *pz = fgets(buf, sizeof(buf), f);
        if (pz == NULL)
            break;

        chomp(buf);
        if (strlen(buf) == 0)
            continue;

        exp_t exp = read_expense(buf, et);
        add_exp(et, exp);
    }
    fclose(f);

    // Sort categories table alphabetically
    strtbl_t tmpcats = dup_strtbl(et->cats, &scratch);
    sort_strtbl(&et->cats, cmp_str);

    // Re-set exp catid's to new sorted categories table.
    for (int i=0; i < et->len; i++) {
        exp_t *exp = &et->base[i];
        str_t catname = strtbl_get(tmpcats, exp->catid);
        exp->catid = strtbl_find(et->cats, catname);
    }
}

static exp_t read_expense(char *buf, exptbl_t *et) {
    exp_t retexp;
    arena_t *arena = et->arena;

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
    retexp.descid = strtbl_add(&et->strings, new_str(arena, pfield));

    // amount
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.amt = atof(pfield);

    // category
    pfield = nextp;
    nextp = next_field(pfield);
    retexp.catid = strtbl_find(et->cats, (str_t){pfield, strlen(pfield)});
    if (retexp.catid == 0) {
        retexp.catid = strtbl_add(&et->cats, new_str(arena, pfield));
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

void save_expense_file(exptbl_t et, arena_t scratch) {
    FILE *f;
    char isodate[ISO_DATE_LEN+1];

    str_t expfile = get_expense_filename(&scratch);

    // Back up expense file to .bak before overwriting it.
    if (file_exists(expfile.bytes)) {
        char backupfile[2048];
        snprintf(backupfile, sizeof(backupfile), "%s.bak", expfile.bytes);

        if (remove(backupfile))
            perror("Error removing previous backup file");
        if (rename(expfile.bytes, backupfile))
            perror("Error creating backup file");
    }

    f = fopen(expfile.bytes, "w");
    if (f == NULL) {
        fprintf(stderr, "Error opening '%s': ", expfile.bytes);
        print_error(NULL);
        return;
    }
    sort_exptbl(&et, cmp_exp_date);

    for (int i=0; i < et.len; i++) {
        exp_t exp = et.base[i];
        date_to_iso(exp.date, isodate, sizeof(isodate));
        str_t sdesc = strtbl_get(et.strings, exp.descid);
        str_t scat = strtbl_get(et.cats, exp.catid);
        fprintf(f, "%s; %s; %s; %f; %s\n", isodate, "00:00", sdesc.bytes, exp.amt, scat.bytes);
    }
    fclose(f);
}

