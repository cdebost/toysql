#ifndef SELECT_H
#define SELECT_H

#include "connection.h"
#include "executor/tablescan.h"
#include "storage/heap.h"
#include "table.h"
#include "util/vec.h"

/* A fully-resolved representation of a select query */
struct select {
	/* list of struct select_col */
	struct vec select_list;

	/* list of struct table */
	struct vec from;
};

enum select_col_type { SELECT_COL_LITERAL, SELECT_COL_FIELD };

/* An output column of the query */
struct select_col {
	enum select_col_type type;
	u32		     typeoid;
	u32		     typemod;
	union {
		int   val_int;
		char *val_str;
		struct {
			char *fieldname;
			u32   tableoid;
			u32   colno;
		};
	};
	char *name;
};

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
	struct select	      *select;
	struct tablescan_iter *iter;
	int		       eof;
};

int sql_select(struct conn *conn, struct select *select, struct cursor *cur);

int cursor_next(struct cursor *cur, struct row *row);

#endif
