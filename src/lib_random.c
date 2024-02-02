/*
** lib_random.c
** Teascript random module
*/

#define lib_random_c
#define TEA_LIB

#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"

static void random_seed(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 0 or 1 arguments, got %d", count);
    if(count == 0)
    {
        srand(time(NULL));
    }
    else
    {
        srand((unsigned int)tea_check_number(T, 0));
    }
    tea_push_null(T);
}

static void random_random(tea_State* T)
{
    tea_push_number(T, ((double)rand()) / (double)RAND_MAX);
}

static void random_range(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);
    if(tea_is_number(T, 0) && tea_is_number(T, 1) && count == 2)
    {
        int upper = tea_get_number(T, 1);
        int lower = tea_get_number(T, 0);
        tea_push_number(T, (rand() % (upper - lower + 1)) + lower);
        return;
    }
    else if(tea_is_range(T, 0) && count == 1)
    {
        double u, l;
        tea_get_range(T, 0, &l, &u, NULL);
        int upper = u, lower = l;
        tea_push_number(T, (rand() % (upper - lower + 1)) + lower);
        return;
    }
    tea_error(T, "Expected two numbers or a range");
}

static void random_shuffle(tea_State* T)
{
    tea_check_list(T, 0);

    GClist* list = AS_LIST(T->base[0]);

    if(list->count < 2)
    {
        return;
    }

    for(int i = 0; i < list->count - 1; i++)
    {
        int j = floor(i + rand() / (RAND_MAX / (list->count - i) + 1));
        Value value = list->items[j];
        list->items[j] = list->items[i];
        list->items[i] = value;
    }
}

static void random_choice(tea_State* T)
{
    tea_check_list(T, 0);

    int len = tea_len(T, 0);
    int index = rand() % len;

    tea_get_item(T, 0, index);
}

static const tea_Module random_module[] = {
    { "seed", random_seed, TEA_VARARGS },
    { "random", random_random, 0 },
    { "range", random_range, TEA_VARARGS },
    { "choice", random_choice, 1 },
    { "shuffle", random_shuffle, 1 },
    { NULL, NULL },
};

TEAMOD_API void tea_import_random(tea_State* T)
{
    srand(time(NULL));
    tea_create_module(T, TEA_MODULE_RANDOM, random_module);
}