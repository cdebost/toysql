#include "parser.h"

#include "lex.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "pgwire.h"
#include "univ.h"
#include "util/error.h"

static void errpos_from_lex(struct lex *lex)
{
	errpos(lex->token.begin + 1);
}

static void token_next_skip_space(struct lex *lex)
{
	for (;;) {
		lex_next_token(lex);
		if (lex->token.tclass != TK_SPACE)
			break;
	}
}

static int parse_select_exprs(struct conn *conn, struct lex *lex,
                              struct parse_tree *parse_tree)
{
	vec_init(&parse_tree->select_exprs, 1);

	token_next_skip_space(lex);
	if (lex->token.tclass == TK_STAR) {
		errlog(ERROR, errcode(ER_FEATURE_NOT_SUPPORTED),
		       errmsg("Syntax error"),
		       errdetail("Select * not implemented"),
		       errpos_from_lex(lex));
		return 1;
	}

	for (;;) {
		struct select_expr *select_expr;

		select_expr =
			mem_alloc(&conn->mem_root, sizeof(struct select_expr));
		memset(select_expr, 0, sizeof(struct select_expr));
		switch (lex->token.tclass) {
		case TK_IDENT:
			select_expr->type      = SELECT_EXPR_FIELD;
			select_expr->fieldname = lex->token.val_str;
			break;
		case TK_NUM:
			select_expr->type    = SELECT_EXPR_LITERAL;
			select_expr->typeoid = DTYPE_INT4;
			select_expr->typemod = -1;
			select_expr->val_int = lex->token.val_int;
			break;
		case TK_STR:
			select_expr->type    = SELECT_EXPR_LITERAL;
			select_expr->typeoid = DTYPE_CHAR;
			select_expr->typemod = lex->token.val_str.len;
			select_expr->val_str = lex->token.val_str;
			break;
		default:
			errlog(ERROR, errcode(ER_SYNTAX_ERROR),
			       errmsg("Syntax error"),
			       errdetail("Expected identifier or literal"),
			       errpos_from_lex(lex));
			return 1;
		}

		token_next_skip_space(lex);
		if (lex->token.tclass == TK_AS) {
			token_next_skip_space(lex);
			if (lex->token.tclass != TK_IDENT) {
				errlog(ERROR, errcode(ER_SYNTAX_ERROR),
				       errmsg("Syntax error"),
				       errdetail("Expected identifier"),
				       errpos_from_lex(lex));
				return 1;
			}
			select_expr->as = lex->token.val_str;
			token_next_skip_space(lex);
		}

		vec_push(&parse_tree->select_exprs, select_expr);

		if (lex->token.tclass != TK_COMMA)
			break;
		token_next_skip_space(lex);
	}

	return 0;
}

static int parse_tables(struct conn *conn, struct lex *lex,
			struct parse_tree *parse_tree)
{
	token_next_skip_space(lex);
	if (lex->token.tclass != TK_IDENT) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected table name"), errpos_from_lex(lex));
		return 1;
	}
	parse_tree->select_table.name = lex->token.val_str;
	return 0;
}

static int parse_select(struct conn *conn, struct lex *lex,
			struct parse_tree *parse_tree)
{
	if (parse_select_exprs(conn, lex, parse_tree))
		return 1;

	if (lex->token.tclass == TK_SEMICOLON || lex->token.tclass == TK_EOF)
		return 0;

	if (lex->token.tclass != TK_FROM) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected FROM clause or end of query"),
		       errpos_from_lex(lex));
		return 1;
	}

	if (parse_tables(conn, lex, parse_tree))
		return 1;

	return 0;
}

int parse(struct conn *conn, struct parse_tree *parse_tree)
{
	struct lex lex;

	if (!conn->query || conn->query[0] == '\0')
		return 1;

	lex_init(&lex, conn->query);
	lex_next_token(&lex);

	switch (lex.token.tclass) {
	case TK_SELECT:
		parse_tree->com_type = SQL_SELECT;
		return parse_select(conn, &lex, parse_tree);
	default:
		errlog(ERROR, errcode(ER_FEATURE_NOT_SUPPORTED),
		       errmsg("Syntax error"),
		       errdetail("Only select statements supported"),
		       errpos_from_lex((&lex)));
		return 1;
	}

	return 0;
}
