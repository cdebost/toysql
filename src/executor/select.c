#include "executor/select.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtype.h"
#include "sys.h"
#include "univ.h"
#include "util/bytes.h"
#include "util/error.h"
#include "util/mem.h"

int sql_select(struct select *select, struct cursor *cur)
{
	struct table *table;

	cur->select	= select;
	cur->iter	= NULL;
	cur->eof	= 0;

	assert(select->from.size < 2);
	if (select->from.size > 0) {
		table = (struct table *)select->from.data[0];
		assert(table);
		cur->iter = mem_alloc(sizeof(struct tablescan_iter));
		tablescan_begin(cur->iter, table);
	}

	return 0;
}

static void eval_field_expr(struct table *table, u8 *tup,
			    struct select_col *scol, struct row_field *field)
{
	int    i;
	struct column *col;
	size_t off = 0;

	for (i = 0; i < table->ncols; ++i) {
		col = &table->cols[i];
		if (strcmp(col->name, scol->val_str) == 0)
			break;
		off += dtype_len(col->typeoid, col->typemod);
	}
	if (i == table->ncols) {
		field->len  = 0;
		field->data = (u8 *)"";
	} else {
		field->len  = dtype_len(col->typeoid, col->typemod);
		field->data = tup + off;
	}
}

static void eval_literal_expr(struct select_col *scol, struct row_field *field)
{
	switch (scol->typeoid) {
	case DTYPE_INT2:
	case DTYPE_INT4:
	case DTYPE_INT8:
		field->len   = dtype_len(scol->typeoid, -1);
		field->data  = mem_zalloc(field->len);
		*field->data = scol->val_int;
		break;
	case DTYPE_CHAR:
		field->len  = strlen(scol->val_str);
		field->data = mem_zalloc(field->len + 1);
		strcpy((char *)field->data, scol->val_str);
		break;
	default:
		errlog(FATAL,
		       errmsg("Unexpected literal of type %d", scol->typeoid));
		break;
	}
}

static void eval_result_column(struct cursor *cur, struct select_col *scol,
			       struct row_field *col, u8 *tup)
{
	if (scol->type == SELECT_COL_FIELD) {
		assert(tup);
		eval_field_expr(cur->iter->table, tup, scol, col);
	} else {
		eval_literal_expr(scol, col);
	}
}

int cursor_next(struct cursor *cur, struct row *row)
{
	int colno;

	if (cur->eof)
		return 1;

	if (cur->iter) {
		if (tablescan_next(cur->iter) == -1) {
			cur->eof = 1;
			return 1;
		}
	}

	row->nfields = cur->select->select_list.size;
	row->fields  = mem_zalloc(sizeof(struct row_field) * row->nfields);

	for (colno = 0; colno < row->nfields; ++colno) {
		struct select_col *scol =
			(struct select_col *)
				cur->select->select_list.data[colno];
		eval_result_column(cur, scol, &row->fields[colno],
				   cur->iter ? cur->iter->tup : NULL);
	}

	if (!cur->iter)
		cur->eof = 1;

	return 0;
}
