#ifndef SELECT_H
#define SELECT_H

#include "connection.h"
#include "parser/parser.h"
#include "storage/heap.h"
#include "table.h"

struct row_field {
	u32   len;
	byte *data;
};

struct row {
	u16		  nfields;
	struct row_field *fields;
};

struct tablescan_iter {
	struct table *table;
	struct heap_page *heap_page;
	u16	      slotno;
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
