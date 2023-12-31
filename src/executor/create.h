#ifndef CREATE_H
#define CREATE_H

#include "univ.h"
#include "parser/parser.h"
#include "util/vec.h"

struct create {
	enum sql_command command;

	char *table_name;

	struct vec table_columns;
};

int sql_create_table(struct create *create);

#endif
