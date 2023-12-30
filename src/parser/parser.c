#include "parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "lex.h"
#include "pgwire.h"
#include "sys.h"
#include "table.h"
#include "univ.h"
#include "util/error.h"
#include "util/mem.h"
#include "util/vec.h"

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
	struct select_expr *select_expr;

	vec_init(&parse_tree->select_exprs, 1);

	for (;;) {
		select_expr =
			mem_alloc(&conn->mem_root, sizeof(struct select_expr));
		memset(select_expr, 0, sizeof(struct select_expr));

		token_next_skip_space(lex);
		switch (lex->token.tclass) {
		case TK_STAR:
			select_expr->type = SELECT_EXPR_STAR;
			break;
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
			       errdetail("Expected select expression"),
			       errpos_from_lex(lex));
			return 1;
		}

		token_next_skip_space(lex);
		if (lex->token.tclass == TK_AS) {
			if (select_expr->type == SELECT_EXPR_STAR) {
				errlog(ERROR, errcode(ER_SYNTAX_ERROR),
				       errmsg("Syntax error"),
				       errdetail(
					       "Cannot specify AS clause after *"),
				       errpos_from_lex(lex));
				return 1;
			}

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
	token_next_skip_space(lex);
	if (lex->token.tclass == TK_COMMA) {
		errlog(ERROR, errcode(ER_FEATURE_NOT_SUPPORTED),
		       errmsg("Syntax error"),
		       errdetail("Joins not implemented"),
		       errpos_from_lex(lex));
		return 1;
	}
	parse_tree->ntables = 1;
	parse_tree->select_table =
		mem_zalloc(&conn->mem_root, sizeof(struct select_table));
	parse_tree->select_table->name = lex->token.val_str;
	return 0;
}

static struct table *open_table(const struct lex_str *lname)
{
	char	     *name = strndup(lname->str, lname->len);
	struct table *table;

	table = sys_load_table_by_name(name);
	if (table == NULL)
		errlog(ERROR, errcode(ER_UNDEFINED_TABLE),
		       errmsg("Unknown table %s", name));
	free(name);
	return table;
}

static void expand_star_exprs(struct conn *conn, struct parse_tree *parse_tree,
			      struct table *table)
{
	struct vec	    new_exprs;
	int		    i;
	struct select_expr *expr;
	u16		    colno;

	assert(parse_tree->com_type == SQL_SELECT);

	vec_init(&new_exprs, parse_tree->select_exprs.size);
	for (i = 0; i < parse_tree->select_exprs.size; ++i) {
		expr = (struct select_expr *)parse_tree->select_exprs.data[i];
		if (expr->type != SELECT_EXPR_STAR) {
			vec_push(&new_exprs, expr);
			continue;
		}

		for (colno = 0; colno < table->ncols; ++colno) {
			struct column *col = &table->cols[colno];
			expr		   = malloc(sizeof(struct select_expr));
			memset(expr, 0, sizeof(struct select_expr));
			expr->type	    = SELECT_EXPR_FIELD;
			expr->typeoid	    = col->typeoid;
			expr->typemod	    = col->typemod;
			expr->fieldname.str = col->name;
			expr->fieldname.len = strlen(col->name);
			expr->colno	    = colno;
			vec_push(&new_exprs, expr);
		}
	}
	vec_free(&parse_tree->select_exprs);
	parse_tree->select_exprs = new_exprs;
}

static int resolve(struct conn *conn, struct parse_tree *parse_tree)
{
	int i;

	if (parse_tree->ntables > 0) {
		struct select_table *seltab = parse_tree->select_table;
		seltab->table		    = open_table(&seltab->name);
		if (seltab->table == NULL)
			return 1;
		expand_star_exprs(conn, parse_tree, seltab->table);
	}

	for (i = 0; i < parse_tree->select_exprs.size; ++i) {
		struct select_expr *expr =
			(struct select_expr *)parse_tree->select_exprs.data[i];
		if (expr->type == SELECT_EXPR_FIELD) {
			int	      colno;
			struct table *table = parse_tree->select_table->table;
			for (colno = 0; colno < table->ncols; ++colno) {
				if (strncmp(expr->fieldname.str,
					    table->cols[colno].name,
					    expr->fieldname.len) == 0)
					break;
			}
			if (colno == table->ncols) {
				char name[1024];
				memcpy(name, expr->fieldname.str,
				       expr->fieldname.len);
				name[expr->fieldname.len] = '\0';
				errlog(ERROR, errcode(ER_UNDEFINED_COLUMN),
				       errmsg("Unknown column %s", name));
				return 1;
			}
			expr->colno   = colno;
			expr->typeoid = table->cols[colno].typeoid;
			expr->typemod = table->cols[colno].typemod;
		}
	}
	return 0;
}

static int parse_select(struct conn *conn, struct lex *lex,
			struct parse_tree *parse_tree)
{
	if (parse_select_exprs(conn, lex, parse_tree))
		return 1;
	assert(parse_tree->select_exprs.size > 0);

	if (lex->token.tclass == TK_SEMICOLON || lex->token.tclass == TK_EOF) {
		if (((struct select_expr *)parse_tree->select_exprs.data[0])
			    ->type == SELECT_EXPR_STAR) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR),
			       errmsg("Syntax error"),
			       errdetail("Expected FROM clause after SELECT *"),
			       errpos_from_lex(lex));
			return 1;
		}
		return 0;
	}

	if (lex->token.tclass != TK_FROM) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected FROM clause or end of query"),
		       errpos_from_lex(lex));
		return 1;
	}

	if (parse_tables(conn, lex, parse_tree))
		return 1;

	if (lex->token.tclass != TK_SEMICOLON && lex->token.tclass != TK_EOF) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected end of query"),
		       errpos_from_lex(lex));
		return 1;
	}

	if (resolve(conn, parse_tree))
		return 1;

	return 0;
}

int parse(struct conn *conn, struct parse_tree *parse_tree)
{
	struct lex lex;

	if (!conn->query || conn->query[0] == '\0')
		return 1;

	memset(parse_tree, 0, sizeof(struct parse_tree));
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
