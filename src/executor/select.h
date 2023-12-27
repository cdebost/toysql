#ifndef SELECT_H
#define SELECT_H

#include "connection.h"
#include "parser/parser.h"

struct cursor {
	struct conn	  *conn;
	struct parse_tree *parse_tree;
	int		   yielded;
};

struct row_field {
	u32   len;
	byte *data;
};

struct row {
	u16		  nfields;
	struct row_field *fields;
};

int sql_select(struct conn *conn, struct parse_tree *parse_tree,
	       struct cursor *cur);

int cursor_next(struct cursor *cur, struct row *row);

#endif
