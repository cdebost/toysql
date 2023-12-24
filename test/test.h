#ifndef TEST_H
#define TEST_H

#include <string.h>

typedef void (*_test_fn)(void);

struct _test {
	const char *name;
	_test_fn    fn;
};

#define TEST_SUITE(name, tests)               \
	struct _test  name##_tests[] = tests; \
	unsigned long name##_tests_len =      \
		sizeof(name##_tests) / sizeof(struct _test)

#define TEST(name)          \
	{                   \
#name, name \
	}

extern int test_fail;

#define EXPECT_TRUE(cond)                                                 \
	if (!cond) {                                                      \
		test_fail = 1;                                            \
		fprintf(stderr, "%s:%s:%d assertion failed: " #cond "\n", \
			__func__, __FILE__, __LINE__);                    \
		return;                                                   \
	}

#define EXPECT_EQ(a, b)                                                  \
	if (a != b) {                                                    \
		test_fail = 1;                                           \
		fprintf(stderr,                                          \
			"%s:%s:%d assertion failed: " #a " == " #b "\n", \
			__func__, __FILE__, __LINE__);                   \
		fprintf(stderr, "left: ");                               \
		fprintf(stderr, _PARAM_FSTRING(a), a);                   \
		fprintf(stderr, "\n");                                   \
		fprintf(stderr, "right: ");                              \
		fprintf(stderr, _PARAM_FSTRING(b), b);                   \
		fprintf(stderr, "\n");                                   \
		return;                                                  \
	}

#define EXPECT_STREQ(a, b)                                               \
	if (strcmp(a, b) != 0) {                                         \
		test_fail = 1;                                           \
		fprintf(stderr,                                          \
			"%s:%s:%d assertion failed: " #a " == " #b "\n", \
			__func__, __FILE__, __LINE__);                   \
		fprintf(stderr, "left: %s\n", a);                        \
		fprintf(stderr, "right: %s\n", b);                       \
		return;                                                  \
	}

#define _PARAM_FSTRING(X) \
	_Generic((X), \
                int: "%d", \
                char: "%c", \
                long: "%ld", \
                unsigned long: "%lu", \
                char *: "%s", \
                const char *: "%s" \
        )

#endif // TEST_H
