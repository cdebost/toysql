#ifndef __VEC_H__
#define __VEC_H__

#include <stddef.h>

struct vec {
	void **data;
	size_t size;
	size_t capacity;
};

void  vec_init(struct vec *v, size_t capacity);
void  vec_push(struct vec *v, void *item);
void *vec_pop(struct vec *v);
void  vec_free(struct vec *v);

#endif
