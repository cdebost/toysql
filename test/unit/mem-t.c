#include "test.h"
#include "util/mem.h"

static void test_alloc()
{
	struct mem_root r;
	char	       *str;
	unsigned char  *sentinel;

	mem_root_init(&r);
	str	 = mem_alloc(&r, sizeof(char) * 10);
	sentinel = mem_alloc(&r, sizeof(char));

	*sentinel = 0xFF;

	strcpy((char *)str, "123456789");

	EXPECT_EQ(*sentinel, 0xFF);
}

static void test_zalloc()
{
	struct mem_root r;
	char	       *str;

	mem_root_init(&r);
	str = mem_zalloc(&r, sizeof(char) * 10);

	EXPECT_EQ(strlen(str), 0);
}

static void test_clear()
{
	struct mem_root r;

	mem_root_init(&r);
	mem_alloc(&r, 1);
	mem_zalloc(&r, 5);
	mem_alloc(&r, 10);
	mem_zalloc(&r, 50);
	mem_alloc(&r, 10000);

	mem_root_clear(&r);
	EXPECT_EQ(r.size, 0);
}

TEST_SUITE(mem, TEST(test_alloc), TEST(test_zalloc), TEST(test_clear));
