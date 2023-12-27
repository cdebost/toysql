#include <stdio.h>
#include <string.h>

#include "executor/select.h"
#include "util/mem.h"

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur)
{
	cur->conn	= conn;
	cur->parse_tree = parse_tree;
	cur->yielded	= 0;
	return 0;
}

int cursor_next(struct cursor *cur, struct row *row)
{
	int colno;

	if (cur->yielded)
		return 1;

	row->nfields = cur->parse_tree->select_exprs.size;
	row->fields  = mem_zalloc(&cur->conn->mem_root,
				  sizeof(struct row_field) * row->nfields);

	for (colno = 0; colno < row->nfields; ++colno) {
		struct select_expr *expr =
			(struct select_expr *)
				cur->parse_tree->select_exprs.data[colno];
		if (expr->dtype == DTYPE_INT) {
			char *buf = mem_alloc(&cur->conn->mem_root, 1024);
			snprintf(buf, 1024, "%d", expr->val_int);
			row->fields[colno].len	= strlen(buf);
			row->fields[colno].data = buf;
		} else {
			char *buf = mem_zalloc(&cur->conn->mem_root,
					       expr->val_str.len + 1);
			memcpy(buf, expr->val_str.str, expr->val_str.len);
			row->fields[colno].len	= expr->val_str.len;
			row->fields[colno].data = buf;
		}
	}

	cur->yielded = 1;
	return 0;
}
