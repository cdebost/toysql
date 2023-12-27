#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

typedef void (*_test_fn)(void);

struct _test {
	const char *name;
	_test_fn    fn;
};

#define TEST_SUITE(name, ...)                           \
	struct _test  name##_tests[] = { __VA_ARGS__ }; \
	unsigned long name##_tests_len =                \
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

#define EXPECT_STREQ(A, B)                                                 \
	do {                                                               \
		const char *a = A;                                         \
		const char *b = B;                                         \
		if (strcmp(a, b) != 0) {                                   \
			test_fail = 1;                                     \
			fprintf(stderr,                                    \
				"%s:%s:%d assertion failed: " #A " == " #B \
				"\n",                                      \
				__func__, __FILE__, __LINE__);             \
			fprintf(stderr, "left: %s\n", a);                  \
			fprintf(stderr, "right: %s\n", b);                 \
			return;                                            \
		}                                                          \
	} while (0)

#define EXPECT_NULL(A)                                                         \
	do {                                                                   \
		void *a = A;                                                   \
		if (a != NULL) {                                               \
			test_fail = 1;                                         \
			fprintf(stderr,                                        \
				"%s:%s:%d assertion failed: " #A " == NULL\n", \
				__func__, __FILE__, __LINE__);                 \
			fprintf(stderr, "value: %#lx \"%s\"\n",                \
				PRINT_POINTER_ARGS(A, a));                     \
			return;                                                \
		}                                                              \
	} while (0)

#define PRINT_POINTER_ARGS(E, p) \
	(intptr_t)p, _Generic((E), \
                        char *: (char *)p, \
                        const char *: (char *)p, \
                        default: "" \
                        )

#define _PARAM_FSTRING(X) \
	_Generic((X), \
                int: "%d", \
                char: "%c", \
                long: "%ld", \
                unsigned long: "%lu", \
                long long: "%lld", \
                unsigned long long: "%llu", \
                char *: "%s", \
                const char *: "%s", \
			 default: "%d" \
        )

#endif // TEST_H
