#ifndef HEAP_H
#define HEAP_H

#include "univ.h"

struct heap_slot {
	/* offset from the beginning of the page to the tuple data */
	u16 off;
	/* size of the tuple data */
	u16 sz;
};

struct heap_page {
	/* version of the layout of heap pages */
	u8 version;
	/* offset to beginning of free space on the page */
	u16 free_low;
	/* offset to end of free space on the page */
	u16		 free_high;
	struct heap_slot slots[];
};

/* Initialize a blank page as an empty heap page */
void heap_page_init(struct heap_page *page);

/* Count the number of actively used slots on the heap page */
u16 heap_page_slot_count(struct heap_page *page);

/* Add a new tuple to the end of the page, in the first free slot */
void heap_page_add_tuple(struct heap_page *page, const byte *data, size_t size);

/* Read a tuple from the specified slot number */
u16 heap_page_read_tuple(struct heap_page *page, u16 slotno, byte **data);

#endif
