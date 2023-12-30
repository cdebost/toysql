#include "parser.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "executor/select.h"
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

static int parse_select_exprs(struct lex *lex, struct parse_tree *parse_tree)
{
	struct select_expr *select_expr;

	vec_init(&parse_tree->select_exprs, 1);

	for (;;) {
		select_expr = mem_alloc(sizeof(struct select_expr));
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
			select_expr->type    = SELECT_EXPR_NUM;
			select_expr->val_int = lex->token.val_int;
			break;
		case TK_STR:
			select_expr->type    = SELECT_EXPR_STR;
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

static int parse_tables(struct lex *lex, struct parse_tree *parse_tree)
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
	parse_tree->select_table = mem_zalloc(sizeof(struct select_table));
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

static int parse_select(struct lex *lex, struct parse_tree *parse_tree)
{
	if (parse_select_exprs(lex, parse_tree))
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

	if (parse_tables(lex, parse_tree))
		return 1;

	if (lex->token.tclass != TK_SEMICOLON && lex->token.tclass != TK_EOF) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected end of query"),
		       errpos_from_lex(lex));
		return 1;
	}

	return 0;
}

int make_parse_tree(struct lex *lex, struct parse_tree *parse_tree)
{
	memset(parse_tree, 0, sizeof(struct parse_tree));
	lex_next_token(lex);

	switch (lex->token.tclass) {
	case TK_SELECT:
		parse_tree->com_type = SQL_SELECT;
		return parse_select(lex, parse_tree);
	default:
		errlog(ERROR, errcode(ER_FEATURE_NOT_SUPPORTED),
		       errmsg("Syntax error"),
		       errdetail("Only select statements supported"),
		       errpos_from_lex(lex));
		return 1;
	}
}

int transform_select_expr(struct select_expr *expr, struct select *select)
{
	struct table *table = select->from.size > 0 ? select->from.data[0] :
						      NULL;
	struct select_col *scol;
	struct column	  *col;
	int		   colno;

	switch (expr->type) {
	case SELECT_EXPR_STAR:
		for (colno = 0; colno < table->ncols; ++colno) {
			col  = &table->cols[colno];
			scol = mem_zalloc(sizeof(struct select_col));
			memset(scol, 0, sizeof(struct select_col));
			scol->type	= SELECT_COL_FIELD;
			scol->typeoid	= col->typeoid;
			scol->typemod	= col->typemod;
			scol->fieldname = (char *)col->name;
			scol->colno	= colno;
			vec_push(&select->select_list, scol);
		}
		break;
	case SELECT_EXPR_FIELD:
		for (colno = 0; colno < table->ncols; ++colno) {
			if (strncmp(expr->fieldname.str,
				    table->cols[colno].name,
				    expr->fieldname.len) == 0)
				break;
		}
		if (colno == table->ncols) {
			char name[1024];
			memcpy(name, expr->fieldname.str, expr->fieldname.len);
			name[expr->fieldname.len] = '\0';
			errlog(ERROR, errcode(ER_UNDEFINED_COLUMN),
			       errmsg("Unknown column %s", name));
			return 1;
		}
		col  = &table->cols[colno];
		scol		= mem_zalloc(sizeof(struct select_col));
		scol->type	= SELECT_COL_FIELD;
		scol->typeoid	= col->typeoid;
		scol->typemod	= col->typemod;
		scol->fieldname = (char *)col->name;
		scol->colno	= colno;
		if (expr->as.len > 0) {
			scol->name = mem_alloc(expr->as.len + 1);
			memcpy(scol->name, expr->as.str, expr->as.len);
			scol->name[expr->as.len] = '\0';
		}
		vec_push(&select->select_list, scol);
		break;
	case SELECT_EXPR_NUM:
		scol	      = mem_zalloc(sizeof(struct select_col));
		scol->type    = SELECT_COL_LITERAL;
		scol->typeoid = DTYPE_INT4;
		scol->typemod = -1;
		scol->val_int = expr->val_int;
		if (expr->as.len > 0) {
			scol->name = mem_alloc(expr->as.len + 1);
			memcpy(scol->name, expr->as.str, expr->as.len);
			scol->name[expr->as.len] = '\0';
		}
		vec_push(&select->select_list, scol);
		break;
	case SELECT_EXPR_STR:
		scol	      = mem_zalloc(sizeof(struct select_col));
		scol->type    = SELECT_COL_LITERAL;
		scol->typeoid = DTYPE_CHAR;
		scol->typemod = expr->val_str.len;
		scol->val_str = mem_alloc(expr->val_str.len + 1);
		memcpy(scol->val_str, expr->val_str.str, expr->val_str.len);
		scol->val_str[expr->val_str.len] = '\0';
		if (expr->as.len > 0) {
			scol->name = mem_alloc(expr->as.len + 1);
			memcpy(scol->name, expr->as.str, expr->as.len);
			scol->name[expr->as.len] = '\0';
		}
		vec_push(&select->select_list, scol);
		break;
	}
	return 0;
}

int transform_select(struct parse_tree *parse_tree, struct select *select)
{
	int i;

	memset(select, 0, sizeof(struct select));

	vec_init(&select->from, parse_tree->ntables);
	if (parse_tree->ntables > 0) {
		struct table *table;
		table = open_table(&parse_tree->select_table->name);
		if (table == NULL)
			return 1;
		vec_push(&select->from, table);
	}
	assert(select->from.size < 2);

	for (i = 0; i < parse_tree->select_exprs.size; ++i) {
		struct select_expr *expr =
			(struct select_expr *)parse_tree->select_exprs.data[i];

		if (transform_select_expr(expr, select))
			return 1;
	}
	return 0;
}

int transform(struct parse_tree *parse_tree, void **query_tree)
{
	switch (parse_tree->com_type) {
	case SQL_SELECT:
		*query_tree = mem_alloc(sizeof(struct select));
		return transform_select(parse_tree,
					(struct select *)*query_tree);
	default:
		assert(0);
	}
}

int parse(struct conn *conn, void **query_tree)
{
	struct lex	  lex;
	struct parse_tree parse_tree;

	if (!conn->query || conn->query[0] == '\0')
		return 1;

	lex_init(&lex, conn->query);

	if (make_parse_tree(&lex, &parse_tree))
		return 1;

	if (transform(&parse_tree, query_tree))
		return 1;

	return 0;
}
