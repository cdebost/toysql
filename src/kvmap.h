#ifndef KVMAP_H
#define KVMAP_H

#include "univ.h"

struct kvmap {
	size_t	     size;
	size_t	     capacity;
	const char **keys;
	const char **values;
};

int kvmap_init(struct kvmap *map, size_t capacity);

void kvmap_free(struct kvmap *map);

const char *kvmap_get(struct kvmap *map, const char *key);

int kvmap_put(struct kvmap *map, const char *key, const char *value);

#endif
