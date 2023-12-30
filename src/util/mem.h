#ifndef MEM_H
#define MEM_H

#include "univ.h"

/* A container for dynamic memory allocations */
struct mem_root {
	void **allocs;
	size_t size;
	size_t capacity;
};

void *mem_alloc(size_t size);

void *mem_zalloc(size_t size);

void mem_root_init(struct mem_root *mem_root);

void mem_root_clear(struct mem_root *mem_root);

struct mem_root *mem_root_set(struct mem_root *mem_root);

#endif // MEM_H
