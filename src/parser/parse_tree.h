#ifndef PARSE_TREE_H
#define PARSE_TREE_H

#include "parser/lex.h"
#include "parser/parser.h"
#include "util/vec.h"

struct pt_select {
	/* list of pt_select_expr */
	struct vec select_list;
	/* list of pt_table */
	struct vec from;
};

enum pt_select_expr_type {
	SELECT_EXPR_STAR,
	SELECT_EXPR_NUM,
	SELECT_EXPR_STR,
	SELECT_EXPR_FIELD
};

struct pt_select_expr {
	enum pt_select_expr_type type;
        union {
                int val_int;
                struct lex_str val_str;
		struct lex_str fieldname;
	};
	struct lex_str name;
};

struct pt_table {
	struct lex_str name;
};

struct pt_create {
	struct lex_str table_name;
	struct vec table_columns;
};

struct pt_table_col {
	struct lex_str name;
	struct lex_token type;
	int *arg;
};

struct pt {
	enum sql_command command;
	union {
		struct pt_select select;
		struct pt_create create;
	};
};

#endif
