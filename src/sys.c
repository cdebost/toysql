#include "sys.h"

#include <assert.h>
#include <stdlib.h>

#include "dtype.h"
#include "storage/heap.h"
#include "table.h"
#include "univ.h"
#include "util/bytes.h"
#include "util/error.h"
#include "util/vec.h"

static struct heap_page *tables	 = NULL;
static struct heap_page *columns = NULL;

static u32 table_oid_seq  = 1;
static u32 column_oid_seq = 1;

void sys_bootstrap(void)
{
	struct table tables_table;
	struct table columns_table;

	tables = malloc(PAGE_SIZE);
	heap_page_init(tables);

	columns = malloc(PAGE_SIZE);
	heap_page_init(columns);

	/* Create the tables table */
	table_init(&tables_table, "tables", 2);
	tables_table.cols[0].name    = "oid";
	tables_table.cols[0].typeoid = DTYPE_INT4;
	tables_table.cols[0].typemod = -1;
	tables_table.cols[1].name    = "name";
	tables_table.cols[1].typeoid = DTYPE_CHAR;
	tables_table.cols[1].typemod = 64;

	/* Create the columns table */
	table_init(&columns_table, "columns", 5);
	columns_table.cols[0].name    = "oid";
	columns_table.cols[0].typeoid = DTYPE_INT4;
	columns_table.cols[0].typemod = -1;
	columns_table.cols[1].name    = "tableoid";
	columns_table.cols[1].typeoid = DTYPE_INT4;
	columns_table.cols[1].typemod = -1;
	columns_table.cols[2].name    = "name";
	columns_table.cols[2].typeoid = DTYPE_CHAR;
	columns_table.cols[2].typemod = 64;
	columns_table.cols[3].name    = "typeoid";
	columns_table.cols[3].typeoid = DTYPE_INT4;
	columns_table.cols[3].typemod = -1;
	columns_table.cols[4].name    = "typemod";
	columns_table.cols[4].typeoid = DTYPE_INT4;
	columns_table.cols[4].typemod = -1;

	if (sys_add_table(&tables_table))
		errlog(PANIC, errmsg("Sys bootstrap failed"),
		       errdetail("Could not add tables table"));
	if (sys_add_table(&columns_table))
		errlog(PANIC, errmsg("Sys bootstrap failed"),
		       errdetail("Could not add columns table"));
}

int sys_add_table(struct table *tab)
{
	byte  tup[1024];
	byte *ptr;
	u16   colno;

	tab->oid = table_oid_seq++;

	memset(tup, 0, sizeof(tup));
	ptr = tup;
	ptr = ut_write_4(ptr, tab->oid);
	ptr = ut_write_str(ptr, tab->name);
	heap_page_add_tuple(tables, tup, ptr - tup);

	for (colno = 0; colno < tab->ncols; ++colno) {
		memset(tup, 0, sizeof(tup));
		ptr = tup;
		ptr = ut_write_4(ptr, column_oid_seq++);
		ptr = ut_write_4(ptr, tab->oid);
		ptr = ut_write_str(ptr, tab->cols[colno].name);
		ptr = ut_write_4(ptr, tab->cols[colno].typeoid);
		ptr = ut_write_4(ptr, tab->cols[colno].typemod);
		heap_page_add_tuple(columns, tup, ptr - tup);
	}

	return 0;
}

struct table *sys_load_table_by_name(const char *tab_name)
{
	struct table *table = NULL;
	struct vec    cols;
	u16	      slotno;
	u16	      nslots;
	byte	     *tup;
	u16	      colno;

	nslots = heap_page_slot_count(tables);
	for (slotno = 0; slotno < nslots; ++slotno) {
		u32  oid;
		char name[1024];

		heap_page_read_tuple(tables, slotno, &tup);
		tup = ut_read_4(tup, &oid);
		tup = (char *)ut_read_str(tup, name);
		if (strcmp(name, tab_name) != 0)
			continue;
		table	    = malloc(sizeof(struct table));
		table->oid  = oid;
		table->name = strdup(name);
		break;
	}
	if (table == NULL)
		return NULL;

	vec_init(&cols, 1);
	nslots = heap_page_slot_count(columns);
	for (slotno = 0; slotno < nslots; ++slotno) {
		struct column *col;
		u32	       oid;
		u32	       tableoid;

		heap_page_read_tuple(columns, slotno, &tup);
		tup = ut_read_4(tup, &oid);
		tup = ut_read_4(tup, &tableoid);
		if (tableoid != table->oid)
			continue;
		col	  = malloc(sizeof(struct column));
		col->name = malloc(64);
		tup	  = (char *)ut_read_str(tup, (char *)col->name);
		tup	  = (char *)ut_read_4(tup, &col->typeoid);
		tup	  = (char *)ut_read_4(tup, (u32 *)&col->typemod);
		vec_push(&cols, col);
	}
	assert(cols.size > 0);

	table->ncols = cols.size;
	table->cols  = malloc(sizeof(struct column) * cols.size);
	for (colno = 0; colno < table->ncols; ++colno) {
		struct column *col = (struct column *)cols.data[colno];
		table->cols[colno] = *col;
	}
	vec_free(&cols);

	return table;
}
