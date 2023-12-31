#include "executor/create.h"

#include "storage/heap.h"
#include "table.h"
#include "util/mem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "univ.h"
#include "sys.h"
#include "connection.h"
#include "parser/parser.h"

extern struct heap_page *heap_pages[];

int sql_create_table(struct create *create)
{
	struct table table;
	struct heap_page *heap;
	int i;

	assert(create->command == COM_CREATE);

	table_init(&table, create->table_name, create->table_columns.size);

	for (i = 0; i < create->table_columns.size; ++i) {
		struct column *col = (struct column *)create->table_columns.data[i];
		table.cols[i].ind = col->ind;
		table.cols[i].name = col->name;
		table.cols[i].typemod = col->typemod;
		table.cols[i].typeoid = col->typeoid;
	}

	if (sys_add_table(&table))
		return 1;

	heap = malloc(PAGE_SIZE);
	heap_page_init(heap);
	heap_pages[table.oid] = heap;

	return 0;
}
