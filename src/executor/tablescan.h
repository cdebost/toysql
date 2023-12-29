/* Full table scans using table heaps */

#ifndef TABLESCAN_H
#define TABLESCAN_H

#include "univ.h"
#include "table.h"

struct tablescan_iter {
	struct table *table;
	struct heap_page *page;
	u16	      slotno;
	u16           slotcnt;
	byte *tup;
	i32 tupsize;
};

/* Initialize a tablescan on a table */
void tablescan_begin(struct tablescan_iter *iter, struct table *table);

/* Get the next tuple. Returns the size of the tuple or -1 if eof */
int tablescan_next(struct tablescan_iter *iter);

/* Dispose the tablescan object */
void tablescan_end(struct tablescan_iter *iter);

#endif // TABLESCAN_H
