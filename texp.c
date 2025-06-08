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
    short year;
    short month;
    short day;
} shortdate_t;

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


void init_exptbl(exptbl_t *et, arena_t *a, short cap);
short add_exp(exptbl_t *et, exp_t exp);
void replace_exp(exptbl_t *et, short idx, exp_t exp);
exp_t *get_exp(exptbl_t *et, short idx);

static int cmp_exp_date(exptbl_t *et, void *a, void *b);
static int cmp_exp_cat(exptbl_t *et, void *a, void *b);
typedef int (*comparefunc_t)(exptbl_t *et, void *a, void *b);
static void sort_exptbl(exptbl_t *et, comparefunc_t cmpfunc);
static void sort_exps_part(exptbl_t *et, int start, int end, comparefunc_t cmp);

int file_exists(const char *file);
str_t get_expense_filename(arena_t *a);
void touch_expense_file(const char *expfile);
void load_expense_file(arena_t *exp_arena, arena_t scratch, exptbl_t *exps);

static exp_t read_expense(char *buf, exptbl_t *exps);
static void chomp(char *buf);
static char *skip_ws(char *startp);
static char *next_field(char *startp);

int match_date(char *sz, shortdate_t *sd);
void list_expenses(str_t scat, time_t startdt, time_t enddt);
void list_categories(time_t startdt, time_t enddt);

arena_t exp_arena;
arena_t scratch_arena;
exptbl_t exps;

enum EXPCMD {
    LIST,
    CAT,
    YTD,
    INFO
};

const char HELP_ROOT[] = 
R"(exp - Utility for keeping track and reporting of daily expenses.

Usage:

    exp <command> [arguments]

Commands:

    add     add an expense
    edit    edit an expense
    del     delete an expense
    list    display list of expenses
    cat     display category subtotals
    ytd     display year to date subtotals
    info    display expense file location and other info

Use "exp help [command]" to display information about a command.

Examples:
    exp add DESC AMT CAT [DATE]
    exp edit RECNO
    exp del RECNO
    exp list [CAT] [STARTDATE] [ENDDATE]
    exp cat [STARTDATE] [ENDDATE]
    exp ytd [YEAR]

    exp help
    exp help add
    exp help edit

)";
const char HELP_LIST[] =
R"(exp list - Display list of expenses.

Usage:

    exp list [CAT] [YEAR] | [YEAR-MONTH] | [DATE] | [STARTDATE] [ENDDATE]

    CAT         : category
    YEAR        : year in YYYY format
    YEAR-MONTH  : year and month in YYYY-MM format
    DATE        : date in iso date format YYYY-MM-DD
    STARTDATE   : start date in iso date format YYYY-MM-DD
    ENDDATE     : end date in iso date format YYYY-MM-DD

    Specify CAT to show expenses belonging to category.
    Specify YEAR to show expenses occurring on that year.
    Specify YEAR-MONTH to show expenses occurring on that month.
    Specify DATE to show expenses occurring on that date.
    Specify STARTDATE and/or ENDDATE to specify an inclusive date range.

Example:

    exp list dine_out
    exp list dine_out 2025
    exp list dine_out 2025-06
    exp list dine_out 2025-06-01
    exp list 2025-06
    exp list 2025-06-01
    exp list 2025-01-01 2025-12-31

)";


int main(int argc, char *argv[]) {
    char *expenses_text_file=NULL;
    shortdate_t dt1={0,0,0};
    shortdate_t dt2={0,0,0};
    shortdate_t tmpdate;
    str_t scmd = STR("");
    str_t sarg = STR("");
    int z;

    init_arena(&exp_arena, SIZE_LARGE);
    init_arena(&scratch_arena, SIZE_MEDIUM);

    enum ARGSTEP {
        READ_NONE,
        READ_CMD,
        READ_ARG,
        READ_DATE1,
        READ_DATE2
    } argstep = READ_NONE;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];

        if (argstep == READ_NONE) {
            scmd = new_str(&scratch_arena, arg);
            argstep = READ_CMD;
            continue;
        } else if (argstep == READ_CMD) {
            if (match_date(arg, &tmpdate)) {
                dt1 = tmpdate;
                argstep = READ_DATE1;
            } else {
                sarg = new_str(&scratch_arena, arg);
                argstep = READ_ARG;
            }
            continue;
        }
        if (argstep == READ_ARG) {
            if (match_date(arg, &tmpdate)) {
                dt1 = tmpdate;
                argstep = READ_DATE1;
            }
            continue;
        }
        if (argstep == READ_DATE1) {
            if (match_date(arg, &tmpdate)) {
                dt2 = tmpdate;
                argstep = READ_DATE2;
            }
            continue;
        }
    }

    shortdate_t today;
    date_to_cal(date_today(), &today.year, &today.month, &today.day);

    time_t startdt=0, enddt=0;
    if (dt1.year == 0) {
        // No dates specified, default to current month.
        startdt = date_from_cal(today.year, today.month, 1);
        if (dt2.year == 0)
            enddt = date_next_month(startdt);
    } else if (dt1.month == 0) {
        // Year specified yyyy
        startdt = date_from_cal(dt1.year, 1, 1);
        if (dt2.year == 0)
            enddt = date_from_cal(dt1.year+1, 1, 1);
    } else if (dt1.day == 0) {
        // Month specified yyyy-mm
        startdt = date_from_cal(dt1.year, dt1.month, 1);
        if (dt2.year == 0)
            enddt = date_next_month(startdt);
    } else {
        // Full date specified yyyy-mm-dd
        startdt = date_from_cal(dt1.year, dt1.month, dt1.day);
        if (dt2.year == 0)
            enddt = date_next_day(startdt);
    }

    if (dt2.year == 0) {
    } else if (dt2.month == 0) {
        // Year specified yyyy
        enddt = date_from_cal(dt2.year+1, 1, 1);
    } else if (dt2.day == 0) {
        // Month specified yyyy-mm
        enddt = date_next_month(date_from_cal(dt2.year, dt2.month, 1));
    } else {
        // Full date specified yyyy-mm-dd
        enddt = date_next_day(date_from_cal(dt2.year, dt2.month, dt2.day));
    }

    if (str_equals(scmd, "info")) {
        str_t expfile = get_expense_filename(&scratch_arena);
        printf("exp config info\n\n");
        printf("    expense file  : %s\n\n", expfile.bytes);
        printf("Set the WINEXPFILE environment var to change the active expense file.\n");
        printf("Expense file will be created automatically when you add or display expenses.\n\n");
    } else if (str_equals(scmd, "help")) {
        if (str_equals(sarg, "list"))
            printf(HELP_LIST);
        else
            printf(HELP_ROOT);
    } else if (str_equals(scmd, "list")) {
        list_expenses(sarg, startdt, enddt);
    } else if (str_equals(scmd, "cat")) {
        list_categories(startdt, enddt);
    } else {
        printf(HELP_ROOT);
    }

    free_arena(&exp_arena);
    free_arena(&scratch_arena);
}

void list_expenses(str_t scat, time_t startdt, time_t enddt) {
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t exps;
    load_expense_file(&exp_arena, scratch_arena, &exps);
    sort_exptbl(&exps, cmp_exp_date);

    printf("Display: Expenses\n");
    printf("Date range [%s] to [%s]\n", startdt_iso, enddt_iso);
    if (scat.len > 0)
        printf("Filter by category [%s]\n", scat.bytes);
    printf("\n");

    // istart = index to first exp record within date range
    // iend = index to last exp record within date range
    int istart=-1, iend=-1;
    for (int i=0; i < exps.len; i++) {
        exp_t xp = exps.base[i];

        if (xp.date < startdt)
            continue;
        if (xp.date >= enddt)
            break;

        if (istart == -1)
            istart = i;
        iend = i;
    }
    assert(iend >= istart);
    if (istart == -1 || iend == -1) {
        printf("No expenses found.\n");
        return;
    }

    double total = 0.0;
    for (int i=istart; i <= iend; i++) {
        exp_t xp = exps.base[i];
        assert(xp.date >= startdt && xp.date < enddt);

        str_t catname = strtbl_get(&exps.cats, xp.catid);
        if (scat.len > 0 && strcmp(catname.bytes, scat.bytes) != 0)
            continue;

        char sdate[ISO_DATE_LEN+1];
        date_to_iso(xp.date, sdate, sizeof(sdate));
        str_t desc = strtbl_get(&exps.strings, xp.descid);
        printf("%-12s %-30.30s %9.2f  %-10s  #%-5d\n", sdate, desc.bytes, xp.amt, catname.bytes, i+1);

        total += xp.amt;
    }

    printf("------------------------------------------------------------------------\n");
    printf("%-12s %-30s %9.2f    %-10s\n", "Totals", "", total, "");
}

void list_categories(time_t startdt, time_t enddt) {
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t exps;
    load_expense_file(&exp_arena, scratch_arena, &exps);
    sort_exptbl(&exps, cmp_exp_date);

    printf("Display: Categories\n");
    printf("Date range [%s] to [%s]\n", startdt_iso, enddt_iso);
    printf("\n");

    // istart = index to first exp record within date range
    // iend = index to last exp record within date range
    int istart=-1, iend=-1;
    for (int i=0; i < exps.len; i++) {
        exp_t xp = exps.base[i];

        if (xp.date < startdt)
            continue;
        if (xp.date >= enddt)
            break;

        if (istart == -1)
            istart = i;
        iend = i;
    }
    assert(iend >= istart);
    if (istart == -1 || iend == -1) {
        printf("No expenses found.\n");
        return;
    }

    // Sort expenses within the date range by categories
    sort_exps_part(&exps, istart, iend, cmp_exp_cat);

    double total = 0.0;
    double catsubtotal = 0.0;
    short cur_catid = -1;
    for (int i=istart; i <= iend; i++) {
        exp_t xp = exps.base[i];
        assert(xp.date >= startdt && xp.date < enddt);
        total += xp.amt;

        if (cur_catid != -1 && xp.catid != cur_catid) {
            str_t catname = strtbl_get(&exps.cats, cur_catid);
            printf("%-12.12s %12.2f\n", catname.bytes, catsubtotal);

            cur_catid = xp.catid;
            catsubtotal = xp.amt;
            continue;
        }

        cur_catid = xp.catid;
        catsubtotal += xp.amt;
    }
    assert(cur_catid != -1);

    str_t catname = strtbl_get(&exps.cats, cur_catid);
    printf("%-12.12s %12.2f\n", catname.bytes, catsubtotal);

    printf("------------------------------------------------------------------------\n");
    printf("%-12.12s %12.2f\n", "Totals", total);
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

int match_date(char *sz, shortdate_t *sd) {
    int z;
    regex_t regdate;
    regmatch_t match[6];

    sd->year = 0;
    sd->month = 0;
    sd->day = 0;

//  match[0]: yyyy-mm-dd
//  * match[1]: yyyy
//  match[2]: -mm-dd
//  * match[3]: mm
//  match[4]: -dd
//  * match[5]: dd
//
//  Without optional groups:
//    ^([0-9]{4})-([0-9]{2})-([0-9]{2})$
//
//  POSIX regex (regex.h) doesn't support \d or (?:) non-capturing groups, so we have 
//  to use [0-9] and ()? for optional groups.

    z = regcomp(&regdate, "^([0-9]{4})(-([0-9]{2})(-([0-9]{2}))?)?$", REG_EXTENDED);
    assert(z == 0);

    z = regexec(&regdate, sz, 6, match, 0);
    regfree(&regdate);
    if (z != 0)
        return 0;

    if (match[1].rm_so != -1) {
        assert(match[1].rm_eo >= 0);
        sz[match[1].rm_eo] = 0;
        sd->year = atoi(sz + match[1].rm_so);
    }
    if (match[3].rm_so != -1) {
        assert(match[3].rm_eo >= 0);
        sz[match[3].rm_eo] = 0;
        sd->month = atoi(sz + match[3].rm_so);
    }
    if (match[5].rm_so != -1) {
        assert(match[5].rm_eo >= 0);
        sz[match[5].rm_eo] = 0;
        sd->day = atoi(sz + match[5].rm_so);
    }

    return 1;
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

static int cmp_exp_date(exptbl_t *et, void *a, void *b) {
    exp_t *expa = a;
    exp_t *expb = b;
    if (expa->date < expb->date) return -1;
    if (expa->date > expb->date) return 1;
    return 0;
}
static int cmp_exp_cat(exptbl_t *et, void *a, void *b) {
    exp_t *expa = a;
    exp_t *expb = b;
    str_t cata = strtbl_get(&et->cats, expa->catid);
    str_t catb = strtbl_get(&et->cats, expb->catid);

    return strcasecmp(cata.bytes, catb.bytes);
}
static void swap_exp(exp_t *exps, int i, int j) {
    exp_t tmp = exps[i];
    exps[i] = exps[j];
    exps[j] = tmp;
}
static int sort_exps_partition(exptbl_t *et, int start, int end, comparefunc_t cmp) {
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
static void sort_exps_part(exptbl_t *et, int start, int end, comparefunc_t cmp) {
    if (start >= end)
        return;
    int pivot = sort_exps_partition(et, start, end, cmp);
    sort_exps_part(et, start, pivot-1, cmp);
    sort_exps_part(et, pivot+1, end, cmp);
}
static void sort_exptbl(exptbl_t *et, comparefunc_t cmp) {
    sort_exps_part(et, 0, et->len-1, cmp);
}

str_t get_expense_filename(arena_t *a) {
    char buf[2048];
    static char expenses_filename[] = "expenses";
    char *path;

    path = getenv("WINEXPFILE");
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

#define BUFLINE_SIZE 1000
void load_expense_file(arena_t *exp_arena, arena_t scratch, exptbl_t *exps) {
    FILE *f;
    char buf[BUFLINE_SIZE];

    str_t expfile = get_expense_filename(&scratch);
    touch_expense_file(expfile.bytes);

    reset_arena(exp_arena);
    init_exptbl(exps, exp_arena, 100);

    f = fopen(expfile.bytes, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening '%s': ", expfile.bytes);
        print_error(NULL);
        return;
    }

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
