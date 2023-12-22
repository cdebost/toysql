/*Postgres wire protocol implementation*/

#ifndef PGWIRE_H
#define PGWIRE_H

#include "univ.h"

/* Error severities */
static const char PGWIRE_ERROR[]   = "ERROR";
static const char PGWIRE_FATAL[]   = "FATAL";
static const char PGWIRE_PANIC[]   = "PANIC";
static const char PGWIRE_WARNING[] = "WARNING";
static const char PGWIRE_NOTICE[]  = "NOTICE";
static const char PGWIRE_DEBUG[]   = "DEBUG";
static const char PGWIRE_INFO[]	   = "INFO";
static const char PGWIRE_LOG[]	   = "LOG";

struct pgwire_error {
	const char *severity;
	/* Code: the SQLSTATE code for the error. Not localizable. Always
	 * present.*/
	const char *code;
	/* Message: the primary human-readable error message. This should be
	 * accurate but terse (typically one line). Always present. */
	const char *message;
	/* Detail: an optional secondary error message carrying more detail
	 * about the problem. Might run to multiple lines. */
	const char *detail;
};

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

int pgwire_handshake(int socket);

int pgwire_send_error(int socket, struct pgwire_error *error);

int pgwire_read_query(int socket, char **query);

int pgwire_send_metadata(int socket, struct pgwire_rowdesc *row_desc);

int pgwire_send_data(int socket, struct pgwire_datarow *row);

int pgwire_complete_command(int socket);

int pgwire_terminate(int socket);

#endif // PGWIRE_H
