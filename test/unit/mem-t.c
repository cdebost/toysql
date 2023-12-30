#include "test.h"
#include "util/mem.h"

static void test_alloc()
{
	struct mem_root r;
	char	       *str;
	unsigned char  *sentinel;

	mem_root_init(&r);
	mem_root_set(&r);
	str	 = mem_alloc(sizeof(char) * 10);
	sentinel = mem_alloc(sizeof(char));

	*sentinel = 0xFF;

	strcpy((char *)str, "123456789");

	EXPECT_EQ(*sentinel, 0xFF);
}

static void test_zalloc()
{
	struct mem_root r;
	char	       *str;

	mem_root_init(&r);
	mem_root_set(&r);
	str = mem_zalloc(sizeof(char) * 10);

	EXPECT_EQ(strlen(str), 0);
}

static void test_clear()
{
	struct mem_root r;

	mem_root_init(&r);
	mem_root_set(&r);
	mem_alloc(1);
	mem_zalloc(5);
	mem_alloc(10);
	mem_zalloc(50);
	mem_alloc(10000);

	mem_root_clear(&r);
	EXPECT_EQ(r.size, 0);
}

TEST_SUITE(mem, TEST(test_alloc), TEST(test_zalloc), TEST(test_clear));
