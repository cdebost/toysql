#include "kvmap.h"

#include <stdlib.h>
#include <string.h>

int kvmap_init(struct kvmap *map, size_t capacity)
{
	map->size     = 0;
	map->capacity = capacity;
	map->keys     = malloc(sizeof(char *) * capacity);
	map->values   = malloc(sizeof(char *) * capacity);
	return map->keys != NULL && map->values != NULL;
}

void kvmap_free(struct kvmap *map)
{
	int i;

	for (i = 0; i < map->size; ++i) {
		free((void *)map->keys[i]);
		free((void *)map->values[i]);
	}
	map->size     = 0;
	map->capacity = 0;
	free(map->keys);
	free(map->values);
}

static int kvmap_reserve(struct kvmap *map, size_t capacity)
{
	const char **new_keys;
	const char **new_values;

	if (map->capacity >= capacity)
		return 0;

	new_keys   = realloc(map->keys, sizeof(char *) * capacity);
	new_values = realloc(map->values, sizeof(char *) * capacity);

	if (new_keys != NULL && new_values != NULL) {
		map->capacity = capacity;
		map->keys     = new_keys;
		map->values   = new_values;
		return 0;
	} else {
		return 1;
	}
}

const char *kvmap_get(struct kvmap *map, const char *key)
{
	int i;

	for (i = 0; i < map->size; ++i) {
		if (strcmp(map->keys[i], key) == 0)
			break;
	}
	return i == map->size ? NULL : map->values[i];
}

int kvmap_put(struct kvmap *map, const char *key, const char *value)
{
	int i;

	if (key == NULL)
		return 1;

	if (map->size == map->capacity) {
		if (kvmap_reserve(map,
				  map->capacity == 0 ? 1 : 2 * map->capacity))
			return 1;
	}

	for (i = 0; i < map->size; ++i) {
		if (strcmp(map->keys[i], key) == 0)
			break;
	}
	if (i == map->size) {
		map->keys[i] = strdup(key);
		map->size++;
	} else if (map->values[i] != NULL) {
		free((void *)map->values[i]);
	}
	map->values[i] = value == NULL ? NULL : strdup(value);
	return 0;
}
