#include "util/mem.h"

#include <stdlib.h>
#include <string.h>

static _Thread_local struct mem_root *mem_root;

void *mem_alloc(size_t size)
{
	void **allocs;
	size_t new_cap;
	void  *alloc;

	if (mem_root->size == mem_root->capacity) {
		new_cap = mem_root->capacity == 0 ? 1 : 2 * mem_root->capacity;
		allocs	= realloc(mem_root->allocs, sizeof(void *) * new_cap);
		if (allocs == NULL)
			exit(EXIT_FAILURE);
		mem_root->allocs   = allocs;
		mem_root->capacity = new_cap;
	}
	alloc				   = malloc(size);
	mem_root->allocs[mem_root->size++] = alloc;
	return alloc;
}

void *mem_zalloc(size_t size)
{
	void *mem = mem_alloc(size);
	memset(mem, 0, size);
	return mem;
}

void mem_root_init(struct mem_root *mem_root)
{
	memset(mem_root, 0, sizeof(struct mem_root));
}

void mem_root_clear(struct mem_root *mem_root)
{
	size_t i;

	for (i = 0; i < mem_root->size; ++i)
		free(mem_root->allocs[i]);
	free(mem_root->allocs);
	mem_root->allocs   = NULL;
	mem_root->size	   = 0;
	mem_root->capacity = 0;
}

struct mem_root *mem_root_set(struct mem_root *new_mem_root)
{
	struct mem_root *previous = mem_root;
	mem_root		  = new_mem_root;
	return previous;
}
