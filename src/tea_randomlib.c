// tea_random.c
// Teascript random module

#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "tea.h"

#include "tea_import.h"
#include "tea_core.h"

static void random_seed(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);
    if(count == 0)
    {
        srand(time(NULL));
        tea_push_null(T);
        return;
    }
    srand((unsigned int)tea_check_number(T, 0));
    tea_push_null(T);
}

static void random_random(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_push_number(T, ((double)rand()) / (double)RAND_MAX);
}

static void random_range(TeaState* T)
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

static void random_shuffle(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    tea_check_list(T, 0);

    // A small internal hack
    TeaObjectList* list = AS_LIST(T->base[0]);

    if(list->items.count < 2)
    {
        return;
    }

    for(int i = 0; i < list->items.count - 1; i++)
    {
        int j = floor(i + rand() / (RAND_MAX / (list->items.count - i) + 1));
        TeaValue value = list->items.values[j];
        list->items.values[j] = list->items.values[i];
        list->items.values[i] = value;
    }
}

static void random_choice(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    tea_check_list(T, 0);

    int len = tea_len(T, 0);
    int index = rand() % len;

    tea_get_item(T, 0, index);
}

static const TeaModule random_module[] = {
    { "seed", random_seed },
    { "random", random_random },
    { "range", random_range},
    { "choice", random_choice },
    { "shuffle", random_shuffle },
    { NULL, NULL },
};

void tea_import_random(TeaState* T)
{
    srand(time(NULL));
    tea_create_module(T, TEA_RANDOM_MODULE, random_module);
}