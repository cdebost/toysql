/*Simple test runner*/

#include "stddef.h"
#include "stdio.h"

#include "test.h"

struct suite {
	const char   *name;
	struct _test *tests;
	size_t	      ntests;
};

#define RUN_TEST_SUITE(suite)                                 \
	do {                                                  \
		extern struct _test  suite##_tests[];         \
		extern unsigned long suite##_tests_len;       \
		for (int i = 0; i < suite##_tests_len; ++i) { \
			run_test(#suite, suite##_tests + i);  \
		}                                             \
	} while (0)

int test_fail;

static void run_test(const char *suite, struct _test *test)
{
	printf("%s.%s...\n", suite, test->name);
	test_fail = 0;
	test->fn();
	if (test_fail)
		printf("%s.%s...FAIL\n", suite, test->name);
	else
		printf("%s.%s...OK\n", suite, test->name);
}

int main(void)
{
	RUN_TEST_SUITE(heap);
	RUN_TEST_SUITE(kvmap);
	RUN_TEST_SUITE(lex);
	RUN_TEST_SUITE(mem);
	RUN_TEST_SUITE(vec);
}
