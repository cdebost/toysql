#include "vec.h"

#include <stdio.h>
#include <stdlib.h>

#include "univ.h"

void vec_init(struct vec *v, size_t capacity)
{
	v->data	    = malloc(capacity * sizeof(void *));
	v->size	    = 0;
	v->capacity = capacity;
}

static int vec_reserve(struct vec *v, size_t capacity)
{
	void *data;

	if (capacity <= v->capacity)
		return 0;
	data = realloc(v->data, capacity * sizeof(void *));
	if (data == NULL)
		return 1;
	v->data	    = data;
	v->capacity = capacity;
	return 0;
}

void vec_push(struct vec *v, void *item)
{
	if (v->size >= v->capacity) {
		if (vec_reserve(v, v->capacity == 0 ? 10 : v->capacity * 2))
			exit(1);
	}
	v->data[v->size] = item;
	v->size++;
}

void *vec_pop(struct vec *v)
{
	if (v->size == 0)
		return NULL;
	return v->data[--v->size];
}

void vec_free(struct vec *v)
{
	if (v->data != NULL)
		free(v->data);
}
