#ifndef EXP_H
#define EXP_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

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

str_t get_expense_filename(arena_t *a);
void touch_expense_file(const char *expfile);
void load_expense_file(arena_t *exp_arena, arena_t scratch, exptbl_t *et);
void save_expense_file(exptbl_t et, arena_t scratch);

void init_exptbl(exptbl_t *et, short cap, arena_t *a);
short add_exp(exptbl_t *et, exp_t exp);
void replace_exp(exptbl_t *et, short idx, exp_t exp);
exp_t *get_exp(exptbl_t *et, short idx);

typedef int (*exptbl_cmpfunc_t)(exptbl_t *et, void *a, void *b);
void sort_exptbl(exptbl_t *et, exptbl_cmpfunc_t cmp);
void sort_exptbl_part(exptbl_t *et, int start, int end, exptbl_cmpfunc_t cmp);
int cmp_exp_date(exptbl_t *et, void *a, void *b);
int cmp_exp_cat(exptbl_t *et, void *a, void *b);

#endif
