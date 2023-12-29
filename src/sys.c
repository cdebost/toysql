#include "sys.h"

#include <assert.h>
#include <stdlib.h>

#include "dtype.h"
#include "executor/tablescan.h"
#include "storage/heap.h"
#include "table.h"
#include "univ.h"
#include "util/bytes.h"
#include "util/error.h"
#include "util/vec.h"

#define NAME_LENGTH 64

static struct table tables;
static struct table columns;

struct heap_page *tables_heap	 = NULL;
struct heap_page *columns_heap = NULL;

static u32 table_oid_seq  = 1;
static u32 column_oid_seq = 1;

struct table table_foo;
struct heap_page *table_foo_heap;
struct heap_page *empty_heap;

struct tables_tup {
	u32 oid;
	char name[NAME_LENGTH];
} __attribute__((packed));

struct columns_tup {
	u32 oid;
	u32 tableoid;
	char name[NAME_LENGTH];
	u32 typeoid;
	u32 typemod;
} __attribute__((packed));

void sys_bootstrap(void)
{
	tables_heap = malloc(PAGE_SIZE);
	heap_page_init(tables_heap);

	columns_heap = malloc(PAGE_SIZE);
	heap_page_init(columns_heap);

	/* Create the tables table */
	table_init(&tables, "tables", 2);
	tables.cols[0].name    = "oid";
	tables.cols[0].typeoid = DTYPE_INT4;
	tables.cols[0].typemod = -1;
	tables.cols[1].name    = "name";
	tables.cols[1].typeoid = DTYPE_CHAR;
	tables.cols[1].typemod = NAME_LENGTH;
	if (sys_add_table(&tables))
		errlog(PANIC, errmsg("Sys bootstrap failed"),
		       errdetail("Could not add tables table"));

	/* Create the columns table */
	table_init(&columns, "columns", 5);
	columns.cols[0].name    = "oid";
	columns.cols[0].typeoid = DTYPE_INT4;
	columns.cols[0].typemod = -1;
	columns.cols[1].name    = "tableoid";
	columns.cols[1].typeoid = DTYPE_INT4;
	columns.cols[1].typemod = -1;
	columns.cols[2].name    = "name";
	columns.cols[2].typeoid = DTYPE_CHAR;
	columns.cols[2].typemod = NAME_LENGTH;
	columns.cols[3].name    = "typeoid";
	columns.cols[3].typeoid = DTYPE_INT4;
	columns.cols[3].typemod = -1;
	columns.cols[4].name    = "typemod";
	columns.cols[4].typeoid = DTYPE_INT4;
	columns.cols[4].typemod = -1;
	if (sys_add_table(&columns))
		errlog(PANIC, errmsg("Sys bootstrap failed"),
		       errdetail("Could not add columns table"));
}

void init_dummy_tables(void)
{
	byte		  tup[1024];

	table_init(&table_foo, "foo", /*ncols=*/2);
	table_foo.cols[0].name = "a";
	table_foo.cols[0].typeoid = DTYPE_CHAR;
	table_foo.cols[0].typemod = 5;
	table_foo.cols[1].name = "b";
	table_foo.cols[1].typeoid = DTYPE_INT4;
	table_foo.cols[1].typemod = -1;
	sys_add_table(&table_foo);

	table_foo_heap = malloc(PAGE_SIZE);
	heap_page_init(table_foo_heap);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "one");
	*(tup + 5) = 1;
	heap_page_add_tuple(table_foo_heap, tup, 9);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "two");
	*(tup + 5) = 2;
	heap_page_add_tuple(table_foo_heap, tup, 9);
	memset(tup, 0, sizeof(tup));
	strcpy(tup, "three");
	*(tup + 5) = 3;
	heap_page_add_tuple(table_foo_heap, tup, 9);
}

int sys_add_table(struct table *tab)
{
	struct tables_tup ttup;
	struct columns_tup ctup;
	u16   colno;

	tab->oid = table_oid_seq++;

	memset(&ttup, 0, sizeof(ttup));
	ttup.oid = tab->oid;
	strncpy(ttup.name, tab->name, NAME_LENGTH);
	heap_page_add_tuple(tables_heap, (byte *)&ttup, sizeof(ttup));

	for (colno = 0; colno < tab->ncols; ++colno) {
		struct column *col = &tab->cols[colno];
		memset(&ctup, 0, sizeof(ctup));
		ctup.oid = column_oid_seq++;
		ctup.tableoid = tab->oid;
		strncpy(ctup.name, col->name, NAME_LENGTH);
		ctup.typeoid = col->typeoid;
		ctup.typemod = col->typemod;
		heap_page_add_tuple(columns_heap, (byte *)&ctup, sizeof(ctup));
	}

	return 0;
}

struct table *sys_load_table_by_name(const char *tab_name)
{
	struct table *table = NULL;
	struct tablescan_iter iter;
	struct tables_tup *ttup;
	struct columns_tup *ctup;
	struct vec    cols;
	u16	      colno;

	tablescan_begin(&iter, &tables);
	while (tablescan_next(&iter) != -1) {
		assert(iter.tupsize == sizeof(struct tables_tup));
		ttup = (struct tables_tup *)iter.tup;
		if (strcmp(ttup->name, tab_name) != 0)
			continue;
		table	    = malloc(sizeof(struct table));
		table->oid  = ttup->oid;
		table->name = strdup(ttup->name);
		break;
	}
	tablescan_end(&iter);

	if (table == NULL)
		return NULL;

	vec_init(&cols, 1);
	tablescan_begin(&iter, &columns);
	while (tablescan_next(&iter) != -1) {
		struct column *col;

		assert(iter.tupsize == sizeof(struct columns_tup));
		ctup = (struct columns_tup *)iter.tup;
		if (ctup->tableoid != table->oid)
			continue;
		col	  = malloc(sizeof(struct column));
		col->name = malloc(NAME_LENGTH);
		strncpy((char *)col->name, ctup->name, NAME_LENGTH);
		col->typeoid = ctup->typeoid;
		col->typemod = ctup->typemod;
		vec_push(&cols, col);
	}
	tablescan_end(&iter);
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
