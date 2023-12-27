#ifndef PARSE_H
#define PARSE_H

#include <assert.h>

#include "connection.h"
#include "parser/lex.h"
#include "util/vec.h"

enum com_type { SQL_SELECT };

enum select_expr_type {
        SELECT_EXPR_STAR,
        SELECT_EXPR_LITERAL
};

enum dtype {
        DTYPE_INT,
        DTYPE_STR
};

struct select_expr {
        enum select_expr_type type;
        enum dtype dtype;
        union {
                int val_int;
                struct lex_str val_str;
        };
        struct lex_str as;
};

struct parse_tree {
	enum com_type	 com_type;
        struct vec select_exprs;
	struct lex_str table_name;
};

int parse(struct conn *con, struct parse_tree *parse_tree);

#endif // PARSE_H
