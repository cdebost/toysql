#include "test.h"
#include "util/kvmap.h"

#include <stdio.h>

static void test_kvmap()
{
	struct kvmap m;
	kvmap_init(&m, 0);

	EXPECT_EQ(m.size, 0);

	kvmap_put(&m, "a", "1");
	kvmap_put(&m, "b", "2");
	kvmap_put(&m, "b", "3");

	EXPECT_EQ(m.size, 2);
	EXPECT_STREQ(kvmap_get(&m, "a"), "1");
	EXPECT_STREQ(kvmap_get(&m, "b"), "3");
	EXPECT_EQ(kvmap_get(&m, "c"), (char *)NULL);
}

TEST_SUITE(kvmap, TEST(test_kvmap));
