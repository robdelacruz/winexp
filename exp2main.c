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
void list_expenses(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
void list_categories(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
void list_ytd(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
void prompt_add(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
void prompt_edit(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
void prompt_del(char *argv[], int argc, arena_t exp_arena, arena_t scratch);
short prompt_cat(strtbl_t *cats, short default_catid);

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

    exp list [CAT] [YEAR | YEAR-MONTH | DATE | STARTDATE ENDDATE]

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
const char HELP_CAT[] =
R"(exp cat - Display category totals.

Usage:

    exp cat [YEAR | YEAR-MONTH | DATE | STARTDATE ENDDATE]

    YEAR        : year in YYYY format
    YEAR-MONTH  : year and month in YYYY-MM format
    DATE        : date in iso date format YYYY-MM-DD
    STARTDATE   : start date in iso date format YYYY-MM-DD
    ENDDATE     : end date in iso date format YYYY-MM-DD

    Specify YEAR to show categories occurring on that year.
    Specify YEAR-MONTH to show categories occurring on that month.
    Specify DATE to show categories occurring on that date.
    Specify STARTDATE and/or ENDDATE to specify an inclusive date range.

    If called without any parameters, categories for the current month will be displayed.

Example:
    exp cat 2019
    exp cat 2019-04
    exp cat 2019-04-01
    exp cat 2019-04-01 2019-04-30

)";
const char HELP_YTD[] =
R"(exp ytd - Display year-to-date totals.

Usage:

    exp ytd [YEAR]

    YEAR  : year in YYYY format

    If called without any parameters, the current year will be displayed.

)";
const char HELP_INFO[] =
R"(exp info - Display configuration information (expense file location, etc.)

    Set the WINEXPFILE environment var to change the active expense file.
    Expense file will be created automatically when you add or display expenses

)";
const char HELP_ADD[] =
R"(exp add - Add expense.

Usage:

    exp add DESC AMT CAT [DATE]

    DESC : description of expense
    AMT  : numeric amount
    CAT  : category name
    DATE : optional - date in iso date format YYYY-MM-DD

    if DATE is not specified, the current date will be used.

Example:
    exp add "expense description" 1.23 category_name 2019-04-01
    exp add "buy groceries" 250.00 groceries

)";
const char HELP_EDIT[] =
R"(exp edit - Edit expense.

Usage:

    exp edit RECNO

    RECNO : record number of expense to edit

    The record number is the #nnn number in rightmost column of the expense
    displayed from the [exp list] command.

Example:
    exp edit 1895

)";
const char HELP_DEL[] =
R"(exp del - Delete expense.

Usage:

    exp del RECNO

    RECNO : record number of expense to delete

    The record number is the #nnn number in rightmost column of the expense
    displayed from the [exp list] command.

Example:
    exp del 1895

)";


int main(int argc, char *argv[]) {
    arena_t exp_arena;
    arena_t scratch_arena;
    init_arena(&exp_arena, SIZE_LARGE);
    init_arena(&scratch_arena, SIZE_MEDIUM);

    // argv[]: exp2 CMD [args...]
    // Skip over program name.
    argv++;
    argc--;

    char *scmd = *argv;
    if (scmd == NULL) {
        printf(HELP_ROOT);
        return 0;
    }
    if (szequals(scmd, "help")) {
        argv++;
        if (*argv == NULL)
            printf(HELP_ROOT);
        else if (szequals(*argv, "list"))
            printf(HELP_LIST);
        else if (szequals(*argv, "cat"))
            printf(HELP_CAT);
        else if (szequals(*argv, "ytd"))
            printf(HELP_YTD);
        else if (szequals(*argv, "info"))
            printf(HELP_INFO);
        else if (szequals(*argv, "add"))
            printf(HELP_ADD);
        else if (szequals(*argv, "edit"))
            printf(HELP_EDIT);
        else if (szequals(*argv, "del"))
            printf(HELP_DEL);
        else
            printf(HELP_ROOT);
    } else if (szequals(scmd, "info")) {
        str_t expfile = get_expense_filename(&scratch_arena);
        printf("exp config info\n\n");
        printf("    expense file  : %s\n\n", expfile.bytes);
        printf("Set the WINEXPFILE environment var to change the active expense file.\n");
        printf("Expense file will be created automatically when you add or display expenses.\n\n");
    } else if (szequals(scmd, "list"))
        list_expenses(argv+1, argc-1, exp_arena, scratch_arena);
    else if (szequals(scmd, "cat"))
        list_categories(argv+1, argc-1, exp_arena, scratch_arena);
    else if (szequals(scmd, "ytd"))
        list_ytd(argv+1, argc-1, exp_arena, scratch_arena);
    else if (szequals(scmd, "add"))
        prompt_add(argv+1, argc-1, exp_arena, scratch_arena);
    else if (szequals(scmd, "edit"))
        prompt_edit(argv+1, argc-1, exp_arena, scratch_arena);
    else if (szequals(scmd, "del"))
        prompt_del(argv+1, argc-1, exp_arena, scratch_arena);
    else
        printf(HELP_ROOT);

    free_arena(&exp_arena);
    free_arena(&scratch_arena);
}

void read_filter_args(char *argv[], int argc, str_t *scat, time_t *startdt, time_t *enddt, arena_t *scratch) {
    shortdate_t dt1={0,0,0};
    shortdate_t dt2={0,0,0};
    shortdate_t tmpdate;

    *scat = STR("");
    *startdt = 0;
    *enddt = 0;

    enum ARGSTEP {
        READ_NONE,
        READ_CAT,
        READ_DATE1,
        READ_DATE2
    } argstep = READ_NONE;

    for (int i=0; i < argc; i++) {
        char *arg = argv[i];
        if (argstep == READ_NONE) {
            if (match_date(arg, &tmpdate)) {
                dt1 = tmpdate;
                argstep = READ_DATE1;
            } else {
                *scat = new_str(scratch, arg);
                argstep = READ_CAT;
            }
        } else if (argstep == READ_CAT) {
            if (match_date(arg, &tmpdate)) {
                dt1 = tmpdate;
                argstep = READ_DATE1;
            }
        } else if (argstep == READ_DATE1) {
            if (match_date(arg, &tmpdate)) {
                dt2 = tmpdate;
                argstep = READ_DATE2;
                break;
            }
        }
    }

    shortdate_t today;
    date_to_cal(date_today(), &today.year, &today.month, &today.day);

    if (dt1.year == 0) {
        // No dates specified, default to current month.
        *startdt = date_from_cal(today.year, today.month, 1);
        if (dt2.year == 0)
            *enddt = date_next_month(*startdt);
    } else if (dt1.month == 0) {
        // Year specified yyyy
        *startdt = date_from_cal(dt1.year, 1, 1);
        if (dt2.year == 0)
            *enddt = date_from_cal(dt1.year+1, 1, 1);
    } else if (dt1.day == 0) {
        // Month specified yyyy-mm
        *startdt = date_from_cal(dt1.year, dt1.month, 1);
        if (dt2.year == 0)
            *enddt = date_next_month(*startdt);
    } else {
        // Full date specified yyyy-mm-dd
        *startdt = date_from_cal(dt1.year, dt1.month, dt1.day);
        if (dt2.year == 0)
            *enddt = date_next_day(*startdt);
    }

    if (dt2.year == 0) {
    } else if (dt2.month == 0) {
        // Year specified yyyy
        *enddt = date_from_cal(dt2.year+1, 1, 1);
    } else if (dt2.day == 0) {
        // Month specified yyyy-mm
        *enddt = date_next_month(date_from_cal(dt2.year, dt2.month, 1));
    } else {
        // Full date specified yyyy-mm-dd
        *enddt = date_next_day(date_from_cal(dt2.year, dt2.month, dt2.day));
    }

}

void list_expenses(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
    // exp list [CAT] [YYYY | YYYY-MM | YYYY-MM-DD | STARTDATE ENDDATE]

    str_t scat = STR("");
    time_t startdt=0, enddt=0;
    read_filter_args(argv, argc, &scat, &startdt, &enddt, &scratch);

    int z;
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t et;
    z = load_expense_file(&exp_arena, scratch, &et);
    if (z != 0)
        return;

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

        str_t catname = strtbl_get(et.cats, xp.catid);
        if (scat.len > 0 && strcmp(catname.bytes, scat.bytes) != 0)
            continue;

        char sdate[ISO_DATE_LEN+1];
        date_to_iso(xp.date, sdate, sizeof(sdate));
        str_t desc = strtbl_get(et.strings, xp.descid);
        printf("%-12s %-30.30s %9.2f  %-10s  #%-5d\n", sdate, desc.bytes, xp.amt, catname.bytes, i+1);

        total += xp.amt;
    }

    printf("------------------------------------------------------------------------\n");
    printf("%-12s %-30s %9.2f    %-10s\n", "Totals", "", total, "");

/*
    printf("categories:\n");
    for (int i=1; i < et.cats.len; i++) {
        str_t s = strtbl_get(et.cats, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
*/
}

void list_categories(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
    // exp cat [YYYY | YYYY-MM | YYYY-MM-DD | STARTDATE ENDDATE]

    str_t scat = STR("");
    time_t startdt=0, enddt=0;
    read_filter_args(argv, argc, &scat, &startdt, &enddt, &scratch);

    int z;
    char startdt_iso[ISO_DATE_LEN+1], enddt_iso[ISO_DATE_LEN+1];
    date_to_iso(startdt, startdt_iso, sizeof(startdt_iso));
    date_to_iso(date_prev_day(enddt), enddt_iso, sizeof(enddt_iso));

    exptbl_t et;
    z = load_expense_file(&exp_arena, scratch, &et);
    if (z != 0)
        return;
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

    entry_t catentry;
    entrytbl_t cattbl;
    init_entrytbl(&cattbl, &scratch, 20);

    double total = 0.0;
    double catsubtotal = 0.0;
    short cur_catid = -1;
    for (int i=istart; i <= iend; i++) {
        exp_t xp = et.base[i];
        assert(xp.date >= startdt && xp.date < enddt);
        total += xp.amt;

        if (cur_catid != -1 && xp.catid != cur_catid) {
            catentry.desc = strtbl_get(et.cats, cur_catid);
            catentry.val = catsubtotal;
            entrytbl_add(&cattbl, catentry);

            cur_catid = xp.catid;
            catsubtotal = xp.amt;
            continue;
        }

        cur_catid = xp.catid;
        catsubtotal += xp.amt;
    }
    assert(cur_catid != -1);

    catentry.desc = strtbl_get(et.cats, cur_catid);
    catentry.val = catsubtotal;
    entrytbl_add(&cattbl, catentry);

    sort_entrytbl(&cattbl, cmp_entry_val);

    for (int i=0; i < cattbl.len; i++) {
        entry_t e = cattbl.base[i];
        printf("%-12.12s %12.2f\n", e.desc.bytes, e.val);
    }
    printf("------------------------------------------------------------------------\n");
    printf("%-12.12s %12.2f\n", "Totals", total);
}

void list_ytd(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
}

// Remove trailing \n or \r chars.
static void chomp(char *buf) {
    ssize_t buf_len = strlen(buf);
    for (int i=buf_len-1; i >= 0; i--) {
        if (buf[i] == '\n' || buf[i] == '\r')
            buf[i] = '\0';
    }
}
static void read_input(const char *prompt, char *buf, short bufsize) {
    if (prompt != NULL)
        printf("%s", prompt);

    memset(buf, 0, bufsize);
    char *pz = fgets(buf, bufsize, stdin);
    if (pz == NULL) {
        perror("Error reading input");
        exit(1);
    }
    chomp(buf);
}
void prompt_add(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
    char buf[1024];
    short descid=0;
    float amt=-1;
    short catid=0;
    time_t dt=0;
    int z;

    regex_t reg;
    z = regcomp(&reg, "^[0-9]{4}-[0-9]{2}-[0-9]{2}$", REG_EXTENDED);
    assert(z == 0);

    exptbl_t et;
    z = load_expense_file(&exp_arena, scratch, &et);
    if (z != 0)
        return;

    // argv[]: [DESC] [AMT] [CAT] [DATE]
    if (argc >= 1)
        descid = strtbl_add(&et.strings, argv[0]);
    if (argc >= 2)
        amt = atof(argv[1]);
    if (argc >= 3) {
        // Add new category name to cats table if necessary. 
        catid = strtbl_find(et.cats, argv[2]);
        if (catid == 0)
            catid = strtbl_add(&et.cats, argv[2]);
    }
    if (argc >= 4) {
        if (szequals(argv[3], "-") || szequals(argv[3], "today"))
            dt = date_today();
        else if (regexec(&reg, argv[3], 0, NULL, 0) == 0)
            dt = date_from_iso(argv[3]);
    }

    // DESC
    while (descid == 0) {
        read_input("Expense Description: ", buf, sizeof(buf));
        if (strlen(buf) == 0)
            continue;
        descid = strtbl_add(&et.strings, buf);
    }

    // AMT
    while (amt == -1) {
        read_input("Amount: ", buf, sizeof(buf));
        if (strlen(buf) == 0)
            continue;
        amt = atof(buf);
    }

    // CAT
    catid = prompt_cat(&et.cats, 0);

    // DATE
    while (dt == 0) {
        read_input("Date (yyyy-mm-dd or leave blank for today): ", buf, sizeof(buf));
        if (strlen(buf) == 0)
            dt = date_today();
        else if (szequals(buf, "-") || szequals(buf, "today"))
            dt = date_today();
        else if (regexec(&reg, buf, 0, NULL, 0) == 0)
            dt = date_from_iso(buf);
    }
    regfree(&reg);

    assert(descid > 0 && catid > 0 && dt > 0);

    exp_t exp;
    exp.date = dt;
    exp.descid = descid;
    exp.amt = amt;
    exp.catid = catid;

    add_exp(&et, exp);
    z = save_expense_file(et, scratch);
    if (z != 0) {
        printf("Record not added.\n");
        return;
    }
    printf("Record added.\n");
    char isodate[ISO_DATE_LEN+1];
    date_to_iso(exp.date, isodate, sizeof(isodate));
    printf("%s; %s; %.2f; %s\n", isodate, strtbl_get(et.strings, descid).bytes, exp.amt, strtbl_get(et.cats, catid).bytes);
}

void prompt_edit(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
    // edit RECNO
    // RECNO should be in the range from 1 to [NUM EXPENSES]
    // exptbl array of expenses is indexed from 0 to [NUM EXPENSES] - 1
    // so exptbl.base[0] is RECNO 1, exptbl.base[1] is RECNO 2, etc.
    //
    // argv[]: [RECNO]

    char prompt[2048];
    char buf[1024];
    int z;

    regex_t reg;
    z = regcomp(&reg, "^[0-9]{4}-[0-9]{2}-[0-9]{2}$", REG_EXTENDED);
    assert(z == 0);

    if (argc == 0) {
        printf(HELP_EDIT);
        return;
    }
    int recno = atoi(argv[0]);
    if (recno == 0) {
        printf(HELP_EDIT);
        return;
    }

    exptbl_t et;
    z = load_expense_file(&exp_arena, scratch, &et);
    if (z != 0)
        return;
    if (recno < 0 || recno > et.len) {
        fprintf(stderr, "Record out of range.\n");
        return;
    }
    exp_t *exp = get_exp(&et, recno-1);
    if (exp == NULL) {
        fprintf(stderr, "Record out of range.\n");
        return;
    }

    // DESC
    snprintf(prompt, sizeof(prompt), "Description [%s]: ", strtbl_get(et.strings, exp->descid).bytes);
    read_input(prompt, buf, sizeof(buf));
    if (strlen(buf) > 0)
        exp->descid = strtbl_add(&et.strings, buf);

    // AMT
    snprintf(prompt, sizeof(prompt), "Amount [%.2f]: ", exp->amt);
    read_input(prompt, buf, sizeof(buf));
    if (strlen(buf) > 0)
        exp->amt = atof(buf);

    // CAT
    exp->catid = prompt_cat(&et.cats, exp->catid);

    // DATE
    char isodate[ISO_DATE_LEN+1];
    date_to_iso(exp->date, isodate, sizeof(isodate));
    snprintf(prompt, sizeof(prompt), "Date [%s]: ", isodate);
    read_input(prompt, buf, sizeof(buf));
    if (strlen(buf) > 0) {
        if (regexec(&reg, buf, 0, NULL, 0) == 0)
            exp->date = date_from_iso(buf);
        else
            printf("Ignoring invalid date.\n");
    }

    z = save_expense_file(et, scratch);
    if (z != 0) {
        printf("Record not updated.\n");
        return;
    }
    printf("Record updated.\n");
}

void prompt_del(char *argv[], int argc, arena_t exp_arena, arena_t scratch) {
    // del RECNO
    // RECNO should be in the range from 1 to [NUM EXPENSES]
    // exptbl array of expenses is indexed from 0 to [NUM EXPENSES] - 1
    // so exptbl.base[0] is RECNO 1, exptbl.base[1] is RECNO 2, etc.
    //
    // argv[]: [RECNO]

    char buf[5];
    int z;

    if (argc == 0) {
        printf(HELP_DEL);
        return;
    }
    int recno = atoi(argv[0]);
    if (recno == 0) {
        printf(HELP_DEL);
        return;
    }

    exptbl_t et;
    z = load_expense_file(&exp_arena, scratch, &et);
    if (z != 0)
        return;
    if (recno < 0 || recno > et.len) {
        fprintf(stderr, "Record out of range.\n");
        return;
    }
    exp_t *exp = get_exp(&et, recno-1);
    if (exp == NULL) {
        fprintf(stderr, "Record out of range.\n");
        return;
    }
    char isodate[ISO_DATE_LEN+1];
    date_to_iso(exp->date, isodate, sizeof(isodate));
    printf("\n%s; %s; %.2f; %s\n", isodate, strtbl_get(et.strings, exp->descid).bytes, exp->amt, strtbl_get(et.cats, exp->catid).bytes);
    read_input("Delete? (y/n): ", buf, sizeof(buf));

    if (strcasecmp(buf, "y") != 0)
        return;

    del_exp(&et, recno-1);
    z = save_expense_file(et, scratch);
    if (z == 0)
        printf("Record deleted.\n");
}

void print_tables(exptbl_t et) {
    printf("expense_strings:\n");
    for (int i=1; i < et.strings.len; i++) {
        str_t s = strtbl_get(et.strings, i);
        printf("[%d] '%.*s'\n", i, s.len, s.bytes);
    }
    printf("expenses:\n");
    for (int i=0; i < et.len; i++) {
        exp_t xp = et.base[i];
        str_t desc = strtbl_get(et.strings, xp.descid);
        str_t catname = strtbl_get(et.cats, xp.catid);
        printf("%d: '%.*s' %.2f '%.*s'\n", i, desc.len, desc.bytes, xp.amt, catname.len, catname.bytes);
    }
    printf("categories:\n");
    for (int i=1; i < et.cats.len; i++) {
        str_t s = strtbl_get(et.cats, i);
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

short prompt_cat(strtbl_t *cats, short default_catid) {
    char prompt[2048];
    char buf[1024];
    str_t default_catname = STR("");
    short catid=0;
    short ask_catname = 1;

    if (default_catid < 1 || default_catid >= cats->len) {
        default_catid = 0;
        snprintf(prompt, sizeof(prompt), "Category (enter '?' for list): ");
    } else {
        snprintf(prompt, sizeof(prompt), "Category [%s] (enter '?' for list): ", strtbl_get(*cats, default_catid).bytes);
    }

    while (catid == 0) {
        if (ask_catname) {
            while (1) {
                read_input(prompt, buf, sizeof(buf));
                if (strlen(buf) == 0) {
                    if (default_catid > 0)
                        return default_catid;
                    else
                        continue;
                }
                if (szequals(buf, "?")) {
                    ask_catname = 0;
                    break;
                }
                catid = strtbl_find(*cats, buf);
                if (catid == 0)
                    catid = strtbl_add(cats, buf);
                break;
            }
        } else {
            if (cats->len <= 1) {
                printf("No categories yet.\n");
                ask_catname = 1;
                break;
            }
            while (1) {
                printf("Categories:\n");
                printf("[0] (Enter new category)\n");
                for (int i=1; i < cats->len; i++)
                    printf("[%d] %s\n", i, cats->base[i].bytes);
                printf("\n");

                read_input("Select category [n]: ", buf, sizeof(buf));
                if (strlen(buf) == 0)
                    continue;
                catid = atoi(buf);
                if (catid == 0) {
                    ask_catname = 1;
                    break;
                }
                if (catid >= 1 && catid < cats->len)
                    break;
            }
        }
    }
    assert(catid >= 1 && catid < cats->len);

    return catid;
}

