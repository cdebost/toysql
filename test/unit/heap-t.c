#include "storage/heap.h"
#include "test.h"

#include "univ.h"

static void test_empty_page()
{
	u8		  page[PAGE_SIZE];
	struct heap_page *heap_page = (struct heap_page *)page;

	heap_page_init(heap_page);
	EXPECT_EQ(heap_page->version, 1);
	EXPECT_EQ(heap_page_slot_count(heap_page), 0);
}

static void test_add_tuple()
{
	u8		  page[PAGE_SIZE];
	struct heap_page *heap_page = (struct heap_page *)page;
	u8		  tup1[]    = "This is a short tuple";
	u8		  tup2[] =
		"This is a longer tuple that should take up more space on the page";
	u8   *stored_tup;
	u16   stored_tup_sz;

	heap_page_init(heap_page);
	heap_page_add_tuple(heap_page, tup1, sizeof(tup1));
	heap_page_add_tuple(heap_page, tup2, sizeof(tup2));

	EXPECT_EQ(heap_page_slot_count(heap_page), 2);
	stored_tup_sz = heap_page_read_tuple(heap_page, 0, &stored_tup);
	EXPECT_EQ(stored_tup_sz, sizeof(tup1));
	EXPECT_STREQ((char *)stored_tup, (char *)tup1);
	stored_tup_sz = heap_page_read_tuple(heap_page, 1, &stored_tup);
	EXPECT_EQ(stored_tup_sz, sizeof(tup2));
	EXPECT_STREQ((char *)stored_tup, (char *)tup2);
}

TEST_SUITE(heap, TEST(test_empty_page), TEST(test_add_tuple));
