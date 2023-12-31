#ifndef PARSE_H
#define PARSE_H

#include "connection.h"

enum sql_command {
	COM_CREATE,
	COM_SELECT
};

int parse(struct conn *con, void **query_tree);

#endif // PARSE_H
