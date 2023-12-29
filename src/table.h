#ifndef TABLE_H
#define TABLE_H

#include "univ.h"

struct column {
	/* user-defined name of the column */
	const char *name;
	/* ordinal number of the column within the table */
	u16 ind;
	/* oid of the column's datatype */
	u32 typeoid;
	/* additional type specific data, e.g. length for chars */
	i32 typemod;
};

struct table {
	u32 oid;
	/* user-defined name of the table */
	const char *name;
	/* number of columns */
	u16		  ncols;
	/* columns */
	struct column	 *cols;
};

void table_init(struct table *table, const char *name, u16 ncols);

#endif
