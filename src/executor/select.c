#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/select.h"
#include "parser/parser.h"
#include "storage/heap.h"
#include "univ.h"
#include "util/error.h"
#include "util/mem.h"

static struct table table_foo;

void init_dummy_tables(void)
{
	struct heap_page *heap_page;
	byte		  tup[1024];

	table_foo.ncols	       = 2;
	table_foo.cols	       = malloc(sizeof(struct column) * 2);
	table_foo.cols[0].name = "a";
	table_foo.cols[0].len  = 5;
	table_foo.cols[1].name = "b";
	table_foo.cols[1].len  = 3;

	heap_page	    = malloc(PAGE_SIZE);
	table_foo.heap_page = heap_page;
	heap_page_init(heap_page);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "one");
	strcpy(tup + 5, "x");
	heap_page_add_tuple(heap_page, tup, 8);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "two");
	strcpy(tup + 5, "xx");
	heap_page_add_tuple(heap_page, tup, 8);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "three");
	strcpy(tup + 5, "xxx");
	heap_page_add_tuple(heap_page, tup, 8);
}

static int open_table(const struct lex_str *name, struct table **table)
{
	if (strncmp(name->str, "foo", name->len) == 0) {
		*table = &table_foo;
	} else {
		*table = NULL;
	}
	return *table == NULL;
}

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur)
{
	cur->conn	= conn;
	cur->parse_tree = parse_tree;
	cur->iter	= NULL;
	cur->eof	= 0;

	if (parse_tree->select_table.name.len > 0) {
		struct table *table;
		if (open_table(&parse_tree->select_table.name, &table)) {
			char *name = mem_zalloc(
				&conn->mem_root,
				parse_tree->select_table.name.len + 1);
			memcpy(name, parse_tree->select_table.name.str,
			       parse_tree->select_table.name.len);
			errlog(ERROR, errcode(ER_UNDEFINED_TABLE),
			       errmsg("Unknown table %s", name));
			return 1;
		}
		cur->iter	  = mem_alloc(&conn->mem_root,
					      sizeof(struct tablescan_iter));
		cur->iter->table  = table;
		cur->iter->slotno = 0;
	}

	return 0;
}

static int tablescan_iter_next(struct tablescan_iter *iter, byte **tup)
{
	u16 tup_size;

	if (iter->slotno >= heap_page_slot_count(iter->table->heap_page))
		return -1;
	tup_size =
		heap_page_read_tuple(iter->table->heap_page, iter->slotno, tup);
	iter->slotno++;
	return tup_size;
}

static void eval_field_expr(struct table *table, byte *tup,
			    struct select_expr *expr, struct row_field *field)
{
	int    i;
	size_t off = 0;

	for (i = 0; i < table->ncols; ++i) {
		if (strncmp(table->cols[i].name, expr->val_str.str,
			    expr->val_str.len) == 0)
			break;
		off += table->cols[i].len;
	}
	if (i == table->ncols) {
		field->len  = 0;
		field->data = "";
	} else {
		field->len  = table->cols[i].len;
		field->data = tup + off;
	}
}

static void eval_literal_expr(struct mem_root	 *mem_root,
			      struct select_expr *expr, struct row_field *field)
{
	if (expr->dtype == DTYPE_INT) {
		char *buf = mem_alloc(mem_root, 1024);
		snprintf(buf, 1024, "%d", expr->val_int);
		field->len  = strlen(buf);
		field->data = buf;
	} else {
		char *buf = mem_zalloc(mem_root, expr->val_str.len + 1);
		memcpy(buf, expr->val_str.str, expr->val_str.len);
		field->len  = expr->val_str.len;
		field->data = buf;
	}
}

static void eval_result_column(struct cursor *cur, struct select_expr *expr,
			       struct row_field *col, byte *tup)
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
	byte *tup = NULL;
	int   tup_size;

	if (cur->eof)
		return 1;

	if (cur->iter) {
		tup_size = tablescan_iter_next(cur->iter, &tup);
		if (tup_size == -1) {
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
		eval_result_column(cur, expr, &row->fields[colno], tup);
	}

	if (!cur->iter)
		cur->eof = 1;

	return 0;
}
