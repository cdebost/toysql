#include "parser.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "dtype.h"
#include "executor/select.h"
#include "executor/create.h"
#include "lex.h"
#include "parser/parse_tree.h"
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

static int parse_select_exprs(struct lex *lex, struct pt_select *select)
{
	struct pt_select_expr *select_expr;

	vec_init(&select->select_list, 1);

	for (;;) {
		select_expr = mem_alloc(sizeof(struct pt_select_expr));
		memset(select_expr, 0, sizeof(struct pt_select_expr));

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
			select_expr->name = lex->token.val_str;
			token_next_skip_space(lex);
		}

		vec_push(&select->select_list, select_expr);

		if (lex->token.tclass != TK_COMMA)
			break;
	}

	return 0;
}

static int parse_tables(struct lex *lex, struct pt_select *select)
{
	struct pt_table *table;

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
	table = mem_zalloc(sizeof(struct pt_table));
	table->name = lex->token.val_str;
	vec_init(&select->from, 1);
	vec_push(&select->from, table);
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

static int parse_select(struct lex *lex, struct pt_select *select)
{
	if (parse_select_exprs(lex, select))
		return 1;
	assert(select->select_list.size > 0);

	if (lex->token.tclass == TK_SEMICOLON || lex->token.tclass == TK_EOF) {
		if (((struct pt_select_expr *)select->select_list.data[0])
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

	if (parse_tables(lex, select))
		return 1;

	if (lex->token.tclass != TK_SEMICOLON && lex->token.tclass != TK_EOF) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"),
		       errdetail("Expected end of query"),
		       errpos_from_lex(lex));
		return 1;
	}

	return 0;
}

static struct dtype *token_to_dtype(struct lex_token *token)
{
	switch (token->tclass) {
	case TK_BIGINT:
		return &dtypes[DTYPE_INT8];
	case TK_CHAR:
		return &dtypes[DTYPE_CHAR];
	case TK_INT:
		return &dtypes[DTYPE_INT4];
	case TK_SMALLINT:
		return &dtypes[DTYPE_INT2];
	default:
		return NULL;
	}
}

static int parse_coltype(struct lex *lex, struct pt_table_col *col)
{
	if (lex->token.tclass < TK_AS && lex->token.tclass != TK_IDENT) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected type"), errpos_from_lex(lex));
		return 1;
	}
	col->type = lex->token;
	token_next_skip_space(lex);

	if (lex->token.tclass == TK_PAREN_OPEN) {
		token_next_skip_space(lex);

		if (lex->token.tclass != TK_NUM) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected number"), errpos_from_lex(lex));
			return 1;
		}
		col->arg = mem_alloc(sizeof(int));
		*col->arg = lex->token.val_int;
		token_next_skip_space(lex);

		if (lex->token.tclass != TK_PAREN_CLOSE) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected close parenthesis"), errpos_from_lex(lex));
			return 1;
		}
		token_next_skip_space(lex);
	}
	return 0;
}

static int parse_column_list(struct lex *lex, struct pt_create *create)
{
	struct vec *list = &create->table_columns;
	struct pt_table_col *col;

	for (;;) {
		col = mem_zalloc(sizeof(struct pt_table_col));

		if (lex->token.tclass != TK_IDENT) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected identifier"), errpos_from_lex(lex));
			return 1;
		}
		col->name = lex->token.val_str;
		token_next_skip_space(lex);

		if (parse_coltype(lex, col))
			return 1;

		vec_push(list, col);

		if (lex->token.tclass == TK_PAREN_CLOSE)
			break;
		if (lex->token.tclass != TK_COMMA) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected close parenthesis or comma"), errpos_from_lex(lex));
			return 1;
		}
		token_next_skip_space(lex);
	}
	return 0;
}

static int parse_create(struct lex *lex, struct pt_create *create)
{
	assert(lex->token.tclass == TK_CREATE);
	token_next_skip_space(lex);

	if (lex->token.tclass != TK_TABLE) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected TABLE after CREATE"), errpos_from_lex(lex));
		return 1;
	}
	token_next_skip_space(lex);

	if (lex->token.tclass != TK_IDENT) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected identifier"), errpos_from_lex(lex));
		return 1;
	}
	create->table_name = lex->token.val_str;
	token_next_skip_space(lex);

	if (lex->token.tclass != TK_PAREN_OPEN) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected column list"), errpos_from_lex(lex));
		return 1;
	}
	token_next_skip_space(lex);

	if (parse_column_list(lex, create))
		return 1;

	if (lex->token.tclass != TK_PAREN_CLOSE) {
		errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected close parenthesis at end of column list"), errpos_from_lex(lex));
		return 1;
	}

	return 0;
}

int make_parse_tree(struct lex *lex, struct pt *pt)
{
	memset(pt, 0, sizeof(struct pt));
	lex_next_token(lex);

	switch (lex->token.tclass) {
	case TK_SELECT:
		pt->command = COM_SELECT;
		return parse_select(lex, &pt->select);
	case TK_CREATE:
		pt->command = COM_CREATE;
		return parse_create(lex, &pt->create);
	default:
		errlog(ERROR, errcode(ER_FEATURE_NOT_SUPPORTED),
		       errmsg("Syntax error"),
		       errdetail("Only select and create statements supported"),
		       errpos_from_lex(lex));
		return 1;
	}
}


int transform_select_expr(struct pt_select_expr *expr, struct select *select)
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
		if (expr->name.len > 0) {
			scol->name = mem_alloc(expr->name.len + 1);
			memcpy(scol->name, expr->name.str, expr->name.len);
			scol->name[expr->name.len] = '\0';
		}
		vec_push(&select->select_list, scol);
		break;
	case SELECT_EXPR_NUM:
		scol	      = mem_zalloc(sizeof(struct select_col));
		scol->type    = SELECT_COL_LITERAL;
		scol->typeoid = DTYPE_INT4;
		scol->typemod = -1;
		scol->val_int = expr->val_int;
		if (expr->name.len > 0) {
			scol->name = mem_alloc(expr->name.len + 1);
			memcpy(scol->name, expr->name.str, expr->name.len);
			scol->name[expr->name.len] = '\0';
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
		if (expr->name.len > 0) {
			scol->name = mem_alloc(expr->name.len + 1);
			memcpy(scol->name, expr->name.str, expr->name.len);
			scol->name[expr->name.len] = '\0';
		}
		vec_push(&select->select_list, scol);
		break;
	}
	return 0;
}

int transform_select(struct pt_select *pt_select, struct select *select)
{
	int i;

	memset(select, 0, sizeof(struct select));
	select->sql_command = COM_SELECT;

	vec_init(&select->from, pt_select->from.size);
	if (pt_select->from.size > 0) {
		struct pt_table *pt_table = pt_select->from.data[0];
		struct table *table;
		table = open_table(&pt_table->name);
		if (table == NULL)
			return 1;
		vec_push(&select->from, table);
	}
	assert(select->from.size < 2);

	for (i = 0; i < pt_select->select_list.size; ++i) {
		struct pt_select_expr *expr =
			(struct pt_select_expr *)pt_select->select_list.data[i];

		if (transform_select_expr(expr, select))
			return 1;
	}
	return 0;
}

int transform_create(struct pt_create *pt_create, struct create *create)
{
	int i;

	memset(create, 0, sizeof(struct create));
	create->command = COM_CREATE;

	create->table_name = mem_alloc(pt_create->table_name.len + 1);
	memcpy(create->table_name, pt_create->table_name.str, pt_create->table_name.len);
	create->table_name[pt_create->table_name.len] = '\0';

	vec_init(&create->table_columns, pt_create->table_columns.size);

	for (i = 0; i < pt_create->table_columns.size; ++i) {
		struct pt_table_col *pt_col = pt_create->table_columns.data[i];
		struct column *col = mem_zalloc(sizeof(struct column));

		col->name = mem_alloc(pt_col->name.len + 1);
		memcpy((char *)col->name, pt_col->name.str, pt_col->name.len);
		((char *)col->name)[pt_col->name.len] = '\0';
		col->ind = i + 1;

		struct dtype *dtype = token_to_dtype(&pt_col->type);
		if (dtype == NULL) {
			errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("Expected type name"), errpos(pt_col->type.begin));
			return 1;
		}
		col->typeoid = dtype->oid;

		if (pt_col->arg != NULL) {
			if (dtype->len != -1) {
				errlog(ERROR, errcode(ER_SYNTAX_ERROR), errmsg("Syntax error"), errdetail("This type does not take a length argument"), errpos(pt_col->type.end));
			return 1;
			}
			col->typemod = *pt_col->arg;
		}

		vec_push(&create->table_columns, col);
	}

	return 0;
}

int transform(struct pt *pt, void **query_tree)
{
	switch (pt->command) {
	case COM_SELECT:
		*query_tree = mem_alloc(sizeof(struct select));
		return transform_select(&pt->select,
					(struct select *)*query_tree);
	case COM_CREATE:
		*query_tree = mem_alloc(sizeof(struct create));
		return transform_create(&pt->create,
					(struct create *)*query_tree);
	default:
		assert(0);
	}
}

int parse(struct conn *conn, void **query_tree)
{
	struct lex	  lex;
	struct pt parse_tree;

	if (!conn->query || conn->query[0] == '\0')
		return 1;

	lex_init(&lex, conn->query);

	if (make_parse_tree(&lex, &parse_tree))
		return 1;

	if (transform(&parse_tree, query_tree))
		return 1;

	return 0;
}
