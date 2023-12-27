#include "test.h"
#include "vec.h"

#include <stdio.h>

static void test_vec()
{
	struct vec v;
	vec_init(&v, 0);

	EXPECT_EQ(v.size, 0);

	vec_push(&v, "a");
	vec_push(&v, "b");
	vec_push(&v, "c");

	EXPECT_EQ(v.size, 3);
	EXPECT_STREQ((char *)vec_pop(&v), "c");
	EXPECT_STREQ((char *)vec_pop(&v), "b");
	EXPECT_STREQ((char *)vec_pop(&v), "a");
	EXPECT_NULL((char *)vec_pop(&v));
}

TEST_SUITE(vec, TEST(test_vec));
