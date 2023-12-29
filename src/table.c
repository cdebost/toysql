#include "table.h"

#include <stdlib.h>
#include <string.h>

void table_init(struct table *table, const char *name, u16 ncols)
{
	u16 colno;

	table->name = name;
	table->ncols = ncols;
	table->cols = malloc(sizeof(struct column) * ncols);
	memset(table->cols, 0, sizeof(struct column) * ncols);
	for (colno = 0; colno < ncols; ++colno) {
		table->cols[colno].ind = colno;
	}
}
