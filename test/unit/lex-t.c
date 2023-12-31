#include "parser/lex.h"
#include "test.h"

#include <stdio.h>

static void test_empty()
{
	struct lex lex;

	lex_init(&lex, "");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_EOF);
}

static void test_int()
{
	struct lex lex;

	lex_init(&lex, "123456");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_NUM);
	EXPECT_EQ(lex.token.val_int, 123456);

	lex_init(&lex, "-123456");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_NUM);
	EXPECT_EQ(lex.token.val_int, -123456);
}

static void test_str()
{
	struct lex lex;

	lex_init(&lex, "'hello world 123'");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_STR);
	EXPECT_TRUE(strncmp(lex.token.val_str.str, "hello world 123",
			    sizeof("hello world 123")));

	lex_init(&lex, "'hello world 123");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_INVALID);
}

static void test_keywords()
{
	struct lex lex;

	lex_init(&lex, "AS");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_AS);

	lex_init(&lex, "aS");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_AS);

	lex_init(&lex, "SASS");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_IDENT);

	lex_init(&lex, "TABLE");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_TABLE);

	lex_init(&lex, "TABLES");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_IDENT);
}

static void test_quoted_ident()
{
	struct lex lex;

	lex_init(&lex, "AS \"AS\"");
	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_AS);

	lex_next_token(&lex);

	lex_next_token(&lex);
	EXPECT_EQ(lex.token.tclass, TK_IDENT);
	EXPECT_TRUE(strncmp(lex.token.val_str.str, "AS", sizeof("AS")));
}

TEST_SUITE(lex, TEST(test_empty), TEST(test_int), TEST(test_str),
	   TEST(test_keywords), TEST(test_quoted_ident));
