#include "parser/lex.h"
#include "test.h"

#include <stdio.h>

static void test_empty()
{
	struct lex   lex;
	struct token token;

	lex_init(&lex, "");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_EOF);
}

static void test_int()
{
	struct lex   lex;
	struct token token;

	lex_init(&lex, "123456");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_NUM);
	EXPECT_EQ(token.val_int, 123456);

	lex_init(&lex, "-123456");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_NUM);
	EXPECT_EQ(token.val_int, -123456);
}

static void test_str()
{
	struct lex   lex;
	struct token token;

	lex_init(&lex, "'hello world 123'");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_STR);
	EXPECT_TRUE(strncmp(token.val_str.str, "hello world 123",
			    sizeof("hello world 123")));

	lex_init(&lex, "'hello world 123");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_INVALID);
}

TEST_SUITE(lex, TEST(test_empty), TEST(test_int), TEST(test_str));
