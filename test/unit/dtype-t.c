#include "dtype.h"
#include "test.h"

static void test_ints()
{
	EXPECT_EQ(dtype_len(DTYPE_INT2, -1), 2);
	EXPECT_EQ(dtype_len(DTYPE_INT4, -1), 4);
	EXPECT_EQ(dtype_len(DTYPE_INT8, -1), 8);
}

static void test_char()
{
	EXPECT_EQ(dtype_len(DTYPE_CHAR, 10), 10);
}

TEST_SUITE(dtype, TEST(test_ints), TEST(test_char));
