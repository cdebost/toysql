/*Postgres wire protocol implementation*/

#ifndef PGWIRE_H
#define PGWIRE_H

#include "connection.h"
#include "univ.h"
#include "util/error.h"

struct pgwire_fielddesc {
	/* The field name. */
	const char *col;
	/* If the field can be identified as a column of a specific table, the
	 * object ID of the table; otherwise zero. */
	u32 tableoid;
	/* If the field can be identified as a column of a specific table, the
	 * attribute number of the column; otherwise zero. */
	u16 colno;
	/* The object ID of the field's data type. */
	u32 typeoid;
	/* The data type size (see pg_type.typlen). Note that negative values
	 * denote variable-width types. */
	u16 typelen;
	/* The type modifier (see pg_attribute.atttypmod). The meaning of the
	 * modifier is type-specific. */
	u32 typmod;
	/* The format code being used for the field. Currently will be zero
	 * (text) or one (binary). In a RowDescription returned from the
	 * statement variant of Describe, the format code is not yet known and
	 * will always be zero. */
	u16 format;
};

struct pgwire_rowdesc {
	/* Specifies the number of fields in a row (can be zero). */
	u16 numfields;
	/* Then, for each field, there is the following:  */
	struct pgwire_fielddesc *fields;
};

struct pgwire_datarow_field {
	/* The length of the column value, in bytes (this count does not include
	 * itself). Can be zero. As a special case, -1 indicates a NULL column
	 * value. No value bytes follow in the NULL case. */
	u32 fieldlen;
	/* The value of the column, in the format indicated by the associated
	 * format code. n is the above length. */
	char *data;
};

struct pgwire_datarow {
	/* The number of column values that follow (possibly zero). */
	u16 numfields;
	/* Next, the following pair of fields appear for each column: */
	struct pgwire_datarow_field *fields;
};

void pgwire_handle_connection(struct conn *conn);

int pgwire_flush_errors(struct conn *conn);

int pgwire_send_error(struct conn *conn, struct err *err);

#endif // PGWIRE_H
