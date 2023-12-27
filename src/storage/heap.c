#include "storage/heap.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const size_t HEAP_HEADER_SIZE = offsetof(struct heap_page, slots);

void heap_page_init(struct heap_page *page)
{
	memset(page, 0, PAGE_SIZE);
	page->version	= 1;
	page->free_low	= HEAP_HEADER_SIZE;
	page->free_high = PAGE_SIZE;
}

u16 heap_page_slot_count(struct heap_page *page)
{
	return (page->free_low - HEAP_HEADER_SIZE) / sizeof(struct heap_slot);
}

void heap_page_add_tuple(struct heap_page *page, const byte *data, size_t size)
{
	u16		  slotno;
	struct heap_slot *slot;
	byte		 *tup;

	slotno	  = heap_page_slot_count(page);
	slot	  = &page->slots[slotno];
	slot->off = page->free_high - size;
	slot->sz  = size;

	page->free_low += sizeof(struct heap_slot);
	page->free_high -= size;

	tup = (byte *)page + slot->off;
	memcpy(tup, data, size);
}

u16 heap_page_read_tuple(struct heap_page *page, u16 slotno, byte **data)
{
	struct heap_slot *slot;
	byte		 *tup;

	assert(slotno < heap_page_slot_count(page));
	slot = &page->slots[slotno];
	tup  = (byte *)page + slot->off;

	*data = tup;
	return slot->sz;
}
