#ifndef PARSE_H
#define PARSE_H

#include <assert.h>

#include "connection.h"
#include "dtype.h"
#include "parser/lex.h"
#include "util/vec.h"

enum com_type { SQL_SELECT };

enum select_expr_type {
	SELECT_EXPR_STAR,
	SELECT_EXPR_LITERAL,
	SELECT_EXPR_FIELD
};

struct select_expr {
        enum select_expr_type type;
        u32 typeoid;
	u32 typemod;
        union {
                int val_int;
                struct lex_str val_str;
		struct {
			struct lex_str fieldname;
			u32 tableoid;
			u32 colno;
		};
	};
	struct lex_str as;
};

struct select_table {
	struct lex_str name;
	struct table  *table;
};

struct parse_tree {
	enum com_type	 com_type;
        struct vec select_exprs;
	i8		     ntables;
	struct select_table *select_table;
};

int parse(struct conn *con, struct parse_tree *parse_tree);

#endif // PARSE_H
