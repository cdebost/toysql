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

static void test_keywords()
{
	struct lex   lex;
	struct token token;

	lex_init(&lex, "AS");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_KEYWORD);
	EXPECT_EQ(token.keyword, KW_AS);

	lex_init(&lex, "aS");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_KEYWORD);
	EXPECT_EQ(token.keyword, KW_AS);
}

static void test_quoted_ident()
{
	struct lex   lex;
	struct token token;

	lex_init(&lex, "AS \"AS\"");
	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_KEYWORD);
	EXPECT_EQ(token.keyword, KW_AS);

	lex_next_token(&lex, &token);

	lex_next_token(&lex, &token);
	EXPECT_EQ(token.tclass, TK_IDENT);
	EXPECT_TRUE(strncmp(token.val_str.str, "AS", sizeof("AS")));
}

TEST_SUITE(lex, TEST(test_empty), TEST(test_int), TEST(test_str), TEST(test_keywords), TEST(test_quoted_ident));
