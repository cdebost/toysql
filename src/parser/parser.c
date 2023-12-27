#include "parser.h"

#include "lex.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "error.h"
#include "pgwire.h"
#include "univ.h"

static void token_next_skip_space(struct lex *lex, struct token *token)
{
	for (;;) {
		lex_next_token(lex, token);
		if (token->tclass != TK_SPACE)
			break;
	}
}

static int parse_select_exprs(struct conn *conn, struct lex *lex,
                              struct parse_tree *parse_tree)
{
	struct token token;

        vec_init(&parse_tree->select_exprs, 1);

	token_next_skip_space(lex, &token);
        if (token.tclass == TK_STAR) {
                struct select_expr *select_expr = malloc(sizeof(struct select_expr));
                select_expr->type = SELECT_EXPR_STAR;
                vec_push(&parse_tree->select_exprs, select_expr);
                return 0;
        }

        for (;;) {
                struct select_expr *select_expr;

                if (token.tclass != TK_NUM && token.tclass != TK_STR) {
                        struct err error;
                        memset(&error, 0, sizeof(error));
                        error.severity = ERROR;
                        error.type     = ER_FEATURE_NOT_SUPPORTED;
                        error.message  = "Syntax error";
                        error.detail   = "Only select literals supported";
                        error.position = lex->pos;
                        error.loc      = err_here;
                        pgwire_send_error(conn, &error);
                        return 1;
                }
                select_expr = malloc(sizeof(struct select_expr));
		memset(select_expr, 0, sizeof(struct select_expr));
                select_expr->type = SELECT_EXPR_LITERAL;
                if (token.tclass == TK_NUM) {
                        select_expr->dtype = DTYPE_INT;
                        select_expr->val_int = token.val_int;
                } else {
                        select_expr->dtype = DTYPE_STR;
                        select_expr->val_str = token.val_str;
                }

		token_next_skip_space(lex, &token);
		if (token.tclass == TK_KEYWORD && token.keyword == KW_AS) {
			token_next_skip_space(lex, &token);
			if (token.tclass != TK_IDENT) {
				struct err error;
				memset(&error, 0, sizeof(error));
				error.severity = ERROR;
				error.type     = ER_SYNTAX_ERROR;
				error.message  = "Syntax error";
				error.detail   = "Expected identifier";
				error.position = lex->pos;
				error.loc      = err_here;
				pgwire_send_error(conn, &error);
				return 1;
			}
			select_expr->as = token.val_str;
			token_next_skip_space(lex, &token);
		}

                vec_push(&parse_tree->select_exprs, select_expr);

                if (token.tclass != TK_COMMA)
                        break;
		token_next_skip_space(lex, &token);
        }

        return 0;
}

static int parse_select(struct conn *conn, struct lex *lex,
			struct parse_tree *parse_tree)
{
	struct token token;

        if (parse_select_exprs(conn, lex, parse_tree))
		return 1;

	token_next_skip_space(lex, &token);
        if (token.tclass != TK_SEMICOLON && token.tclass != TK_EOF) {
		struct err error;
		memset(&error, 0, sizeof(error));
		error.severity = ERROR;
		error.type     = ER_SYNTAX_ERROR;
		error.message  = "Syntax error";
		error.detail   = "Expected end of query";
		error.position = lex->pos;
		error.loc      = err_here;
		pgwire_send_error(conn, &error);
		return 1;
        }

	return 0;
}

int parse(struct conn *conn, struct parse_tree *parse_tree)
{
	struct lex lex;
	struct token token;

	if (!conn->query || conn->query[0] == '\0')
		return 1;

	lex_init(&lex, conn->query);
	lex_next_token(&lex, &token);

	if (token.tclass != TK_KEYWORD) {
		struct err error;
		memset(&error, 0, sizeof(error));
		error.severity = ERROR;
		error.type     = ER_SYNTAX_ERROR;
		error.message  = "Syntax error";
		error.detail   = "Expected keyword";
		error.position = lex.pos;
		error.loc      = err_here;
		pgwire_send_error(conn, &error);
		return 1;
	}

	switch (token.keyword) {
	case KW_SELECT:
		parse_tree->com_type = SQL_SELECT;
		return parse_select(conn, &lex, parse_tree);
	default: {
		struct err error;
		memset(&error, 0, sizeof(error));
		error.severity = ERROR;
		error.type     = ER_FEATURE_NOT_SUPPORTED;
		error.message  = "Syntax error";
		error.detail   = "Only select supported";
		error.position = lex.pos;
		error.loc      = err_here;
		pgwire_send_error(conn, &error);
		return 1;
	}
	}

	return 0;
}
