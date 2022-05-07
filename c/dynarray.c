#include "dynarray.h"

static DynArray _create_dynarr(size_t start_capacity, size_t element_size)
{
    char *arr = malloc(element_size * start_capacity);

    if (!arr)
    {
        fprintf(stderr, "MALLOC FAILURE (%s:%d)", __FILE__, __LINE__);
        abort();
    }

    DynArray vec = {
        .capacity = start_capacity,
        .size = 0,
        .element_size = element_size,
        .arr = arr,
    };

    return vec;
}

static void _dynarr_push(DynArray *arr, void *item)
{
    if (arr->size == arr->capacity)
    {
        char *new_arr = realloc(arr->arr, arr->capacity * arr->element_size * 2);

        if (!new_arr)
        {
            fprintf(stderr, "REALLOC FAILURE (%s:%d)", __FILE__, __LINE__);
            abort();
        }

        arr->arr = new_arr;
        arr->capacity *= 2;
    }

    memcpy(arr->arr + arr->size * arr->element_size, item, arr->element_size);
    ++arr->size;
}

void dynarr_shrink(DynArray *arr)
{
    size_t new_capacity = arr->size * arr->element_size;
    char *new_arr = realloc(arr->arr, new_capacity);

    if (!new_arr)
    {
        fprintf(stderr, "REALLOC FAILURE (%s:%d)", __FILE__, __LINE__);
        abort();
    }

    arr->arr = new_arr;
    arr->capacity = new_capacity;
}

void dynarr_push_multiple(DynArray *arr, void *item_arr, size_t count)
{
    size_t item_arr_size = count * arr->element_size;

    if (arr->size + count >= arr->capacity)
    {
        size_t new_capacity = arr->capacity * 2;

        while (new_capacity < arr->size + count)
        {
            new_capacity *= 2;
        }

        char *new_arr = realloc(arr->arr, new_capacity * arr->element_size);

        if (!new_arr)
        {
            fprintf(stderr, "REALLOC FAILURE (%s:%d)", __FILE__, __LINE__);
            abort();
        }

        arr->arr = new_arr;
        arr->capacity = new_capacity;
    }

    memcpy(arr->arr + arr->size * arr->element_size, item_arr, item_arr_size);
    arr->size += count;
}
