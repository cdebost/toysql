#include "tablescan.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "storage/heap.h"

extern struct heap_page *heap_pages[];

void tablescan_begin(struct tablescan_iter *iter, struct table *table)
{
	assert(table);
	iter->table = table;
	iter->page = heap_pages[table->oid];
	assert(iter->page);
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
