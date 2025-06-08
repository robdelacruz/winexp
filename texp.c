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
#include "exp.h"

typedef struct {
    short year;
    short month;
    short day;
} shortdate_t;

int match_date(char *sz, shortdate_t *sd);
void list_expenses(str_t scat, time_t startdt, time_t enddt, arena_t *exp_arena, arena_t scratch);
void list_categories(time_t startdt, time_t enddt, arena_t *exp_arena, arena_t scratch);

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
    arena_t exp_arena;
    arena_t scratch_arena;

    char *expenses_text_file=NULL;
    shortdate_t dt1={0,0,0};
    shortdate_t dt2={0,0,0};
    shortdate_t tmpdate;
    str_t scmd = STR("");
    str_t sarg = STR("");
    int z;

    init_arena(&exp_arena, SIZE_LARGE);
    init_arena(&scratch_arena, SIZE_MEDIUM);

    enum EXPCMD {
        LIST,
        CAT,
        YTD,
        INFO
    };
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
        list_expenses(sarg, startdt, enddt, &exp_arena, scratch_arena);
    } else if (str_equals(scmd, "cat")) {
        list_categories(startdt, enddt, &exp_arena, scratch_arena);
    } else {
        printf(HELP_ROOT);
    }

    free_arena(&exp_arena);
    free_arena(&scratch_arena);
}

void list_expenses(str_t scat, time_t startdt, time_t enddt, arena_t *exp_arena, arena_t scratch) {
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t et;
    load_expense_file(exp_arena, scratch, &et);
    sort_exptbl(&et, cmp_exp_date);

    printf("Display: Expenses\n");
    printf("Date range [%s] to [%s]\n", startdt_iso, enddt_iso);
    if (scat.len > 0)
        printf("Filter by category [%s]\n", scat.bytes);
    printf("\n");

    // istart = index to first exp record within date range
    // iend = index to last exp record within date range
    int istart=-1, iend=-1;
    for (int i=0; i < et.len; i++) {
        exp_t xp = et.base[i];

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
        exp_t xp = et.base[i];
        assert(xp.date >= startdt && xp.date < enddt);

        str_t catname = strtbl_get(&et.cats, xp.catid);
        if (scat.len > 0 && strcmp(catname.bytes, scat.bytes) != 0)
            continue;

        char sdate[ISO_DATE_LEN+1];
        date_to_iso(xp.date, sdate, sizeof(sdate));
        str_t desc = strtbl_get(&et.strings, xp.descid);
        printf("%-12s %-30.30s %9.2f  %-10s  #%-5d\n", sdate, desc.bytes, xp.amt, catname.bytes, i+1);

        total += xp.amt;
    }

    printf("------------------------------------------------------------------------\n");
    printf("%-12s %-30s %9.2f    %-10s\n", "Totals", "", total, "");
}

void list_categories(time_t startdt, time_t enddt, arena_t *exp_arena, arena_t scratch) {
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t et;
    load_expense_file(exp_arena, scratch, &et);
    sort_exptbl(&et, cmp_exp_date);

    printf("Display: Categories\n");
    printf("Date range [%s] to [%s]\n", startdt_iso, enddt_iso);
    printf("\n");

    // istart = index to first exp record within date range
    // iend = index to last exp record within date range
    int istart=-1, iend=-1;
    for (int i=0; i < et.len; i++) {
        exp_t xp = et.base[i];

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
    sort_exptbl_part(&et, istart, iend, cmp_exp_cat);

    double total = 0.0;
    double catsubtotal = 0.0;
    short cur_catid = -1;
    for (int i=istart; i <= iend; i++) {
        exp_t xp = et.base[i];
        assert(xp.date >= startdt && xp.date < enddt);
        total += xp.amt;

        if (cur_catid != -1 && xp.catid != cur_catid) {
            str_t catname = strtbl_get(&et.cats, cur_catid);
            printf("%-12.12s %12.2f\n", catname.bytes, catsubtotal);

            cur_catid = xp.catid;
            catsubtotal = xp.amt;
            continue;
        }

        cur_catid = xp.catid;
        catsubtotal += xp.amt;
    }
    assert(cur_catid != -1);

    str_t catname = strtbl_get(&et.cats, cur_catid);
    printf("%-12.12s %12.2f\n", catname.bytes, catsubtotal);

    printf("------------------------------------------------------------------------\n");
    printf("%-12.12s %12.2f\n", "Totals", total);
}

void print_tables(exptbl_t et) {
    printf("expense_strings:\n");
    for (int i=1; i < et.strings.len; i++) {
        str_t s = strtbl_get(&et.strings, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
    printf("expenses:\n");
    for (int i=0; i < et.len; i++) {
        exp_t xp = et.base[i];
        str_t desc = strtbl_get(&et.strings, xp.descid);
        str_t catname = strtbl_get(&et.cats, xp.catid);
        printf("%d: '%.*s' %.2f '%.*s'\n", i, desc.len, desc.bytes, xp.amt, catname.len, catname.bytes);
    }
    printf("categories:\n");
    for (int i=1; i < et.cats.len; i++) {
        str_t s = strtbl_get(&et.cats, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
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

