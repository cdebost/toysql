#ifndef SELECT_H
#define SELECT_H

#include "connection.h"
#include "executor/tablescan.h"
#include "parser/parser.h"
#include "storage/heap.h"
#include "table.h"

struct row_field {
	u32   len;
	u8   *data;
};

struct row {
	u16		  nfields;
	struct row_field *fields;
};

struct cursor {
	struct conn	      *conn;
	struct parse_tree     *parse_tree;
	struct tablescan_iter *iter;
	int		       eof;
};

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur);

int cursor_next(struct cursor *cur, struct row *row);

#endif
