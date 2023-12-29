#include "dtype.h"
#include "util/bytes.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/select.h"
#include "parser/parser.h"
#include "storage/heap.h"
#include "sys.h"
#include "univ.h"
#include "util/error.h"
#include "util/mem.h"

static struct table table_foo;
static struct heap_page *table_foo_heap_page;
static struct heap_page *empty_heap_page;

void init_dummy_tables(void)
{
	byte		  tup[1024];

	empty_heap_page = malloc(PAGE_SIZE);
	heap_page_init(empty_heap_page);

	table_init(&table_foo, "foo", /*ncols=*/2);
	table_foo.cols[0].name = "a";
	table_foo.cols[0].typeoid = DTYPE_CHAR;
	table_foo.cols[0].typemod = 5;
	table_foo.cols[1].name = "b";
	table_foo.cols[1].typeoid = DTYPE_INT4;
	table_foo.cols[1].typemod = -1;

	sys_add_table(&table_foo);

	table_foo_heap_page = malloc(PAGE_SIZE);
	heap_page_init(table_foo_heap_page);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "one");
	*(tup + 5) = 1;
	heap_page_add_tuple(table_foo_heap_page, tup, 9);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "two");
	*(tup + 5) = 2;
	heap_page_add_tuple(table_foo_heap_page, tup, 9);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "three");
	*(tup + 5) = 3;
	heap_page_add_tuple(table_foo_heap_page, tup, 9);
}

static int open_table(const struct lex_str *name, struct table **table)
{
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, name->str, name->len);
	*table = sys_load_table_by_name(buf);
	return *table == NULL;
}

static int resolve(struct parse_tree *parse_tree, struct table *table)
{
	int i;

	if (table == NULL)
		return 0;

	for (i = 0; i < parse_tree->select_exprs.size; ++i) {
		struct select_expr *expr = (struct select_expr *)parse_tree->select_exprs.data[i];
		if (expr->type == SELECT_EXPR_FIELD) {
			int colno;
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
			expr->colno = colno;
			expr->typeoid = table->cols[colno].typeoid;
			expr->typemod  = table->cols[colno].typemod;
		}
	}
	return 0;
}

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur)
{
	struct table *table = NULL;

	cur->conn	= conn;
	cur->parse_tree = parse_tree;
	cur->iter	= NULL;
	cur->eof	= 0;

	if (parse_tree->select_table.name.len > 0) {
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
		if (strcmp(cur->iter->table->name, "foo") == 0)
			cur->iter->heap_page = table_foo_heap_page;
		else
			cur->iter->heap_page = empty_heap_page;
		cur->iter->slotno = 0;
	}

	if (resolve(parse_tree, table))
		return 1;

	return 0;
}

static int tablescan_iter_next(struct tablescan_iter *iter, byte **tup)
{
	u16 tup_size;

	if (iter->slotno >= heap_page_slot_count(iter->heap_page))
		return -1;
	tup_size =
		heap_page_read_tuple(iter->heap_page, iter->slotno, tup);
	iter->slotno++;
	return tup_size;
}

static void eval_field_expr(struct table *table, byte *tup,
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
		field->data = "";
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
		field->data = mem_zalloc(mem_root, sizeof(field->len));
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
