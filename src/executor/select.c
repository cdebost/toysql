#include "executor/select.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtype.h"
#include "parser/parser.h"
#include "sys.h"
#include "univ.h"
#include "util/bytes.h"
#include "util/error.h"
#include "util/mem.h"

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur)
{
	cur->conn	= conn;
	cur->parse_tree = parse_tree;
	cur->iter	= NULL;
	cur->eof	= 0;

	assert(parse_tree->ntables < 2);
	if (parse_tree->ntables > 0) {
		assert(parse_tree->select_table->table);
		cur->iter	  = mem_alloc(&conn->mem_root,
					      sizeof(struct tablescan_iter));
		tablescan_begin(cur->iter, parse_tree->select_table->table);
	}

	return 0;
}

static void eval_field_expr(struct table *table, u8 *tup,
			    struct select_expr *expr, struct row_field *field)
{
	int    i;
	struct column *col;
	size_t off = 0;

	for (i = 0; i < table->ncols; ++i) {
		col = &table->cols[i];
		if (strncmp(col->name, expr->val_str.str,
			    expr->val_str.len) == 0)
			break;
		off += dtype_len(col->typeoid, col->typemod);
	}
	if (i == table->ncols) {
		field->len  = 0;
		field->data = (u8 *)"";
	} else {
		field->len  = dtype_len(col->typeoid, col->typemod);
		field->data = tup + off;
	}
}

static void eval_literal_expr(struct mem_root	 *mem_root,
			      struct select_expr *expr, struct row_field *field)
{
	switch (expr->typeoid) {
	case DTYPE_INT2:
	case DTYPE_INT4:
	case DTYPE_INT8:
		field->len = dtype_len(expr->typeoid, -1);
		field->data = mem_zalloc(mem_root, field->len);
		*field->data = expr->val_int;
		break;
	case DTYPE_CHAR:
		field->len  = expr->val_str.len;
		field->data = mem_zalloc(mem_root, expr->val_str.len + 1);
		memcpy(field->data, expr->val_str.str, expr->val_str.len);
		break;
	default:
		errlog(FATAL, errmsg("Unexpected literal of type %d", expr->typeoid));
		break;
	}
}

static void eval_result_column(struct cursor *cur, struct select_expr *expr,
			       struct row_field *col, u8 *tup)
{
	if (expr->type == SELECT_EXPR_FIELD) {
		assert(tup);
		eval_field_expr(cur->iter->table, tup, expr, col);
	} else {
		eval_literal_expr(&cur->conn->mem_root, expr, col);
	}
}

int cursor_next(struct cursor *cur, struct row *row)
{
	int colno;

	if (cur->eof)
		return 1;

	if (cur->iter) {
		if (tablescan_next(cur->iter) == -1) {
			cur->eof = 1;
			return 1;
		}
	}

	row->nfields = cur->parse_tree->select_exprs.size;
	row->fields  = mem_zalloc(&cur->conn->mem_root,
				  sizeof(struct row_field) * row->nfields);

	for (colno = 0; colno < row->nfields; ++colno) {
		struct select_expr *expr =
			(struct select_expr *)
				cur->parse_tree->select_exprs.data[colno];
		eval_result_column(cur, expr, &row->fields[colno], cur->iter ? cur->iter->tup : NULL);
	}

	if (!cur->iter)
		cur->eof = 1;

	return 0;
}
