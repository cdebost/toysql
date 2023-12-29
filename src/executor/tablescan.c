#include "tablescan.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "storage/heap.h"

extern struct heap_page *tables_heap;
extern struct heap_page *columns_heap;
extern struct heap_page *table_foo_heap;
extern struct heap_page *empty_heap;

void tablescan_begin(struct tablescan_iter *iter, struct table *table)
{
	assert(table);
	iter->table = table;
	if (strcmp(table->name, "tables") == 0)
		iter->page = tables_heap;
	else if (strcmp(table->name, "columns") == 0)
		iter->page = columns_heap;
	else if (strcmp(table->name, "foo") == 0)
		iter->page = table_foo_heap;
	else
		iter->page = empty_heap;
	iter->slotno = 0;
	iter->slotcnt = heap_page_slot_count(iter->page);
	iter->tup = NULL;
	iter->tupsize = -1;
}

int tablescan_next(struct tablescan_iter *iter)
{
	if (iter->slotno >= iter->slotcnt) {
		iter->tup = NULL;
		iter->tupsize = -1;
	} else {
		iter->tupsize = heap_page_read_tuple(iter->page, iter->slotno, &iter->tup);
		iter->slotno++;
	}
	return iter->tupsize;
}

void tablescan_end(struct tablescan_iter *iter)
{
}
