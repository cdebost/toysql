#include "pgwire.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "connection.h"
#include "executor/select.h"
#include "executor/create.h"
#include "util/mem.h"
#include "parser/parser.h"
#include "util/bytes.h"
#include "util/error.h"
#include "dtype.h"

enum tag {
	TAG_AUTHENTICATION_REQUEST = 'R',
	TAG_PARAMETER_STATUS	   = 'S',
	TAG_ERROR_RESPONSE	   = 'E',
	TAG_QUERY		   = 'Q',
	TAG_ROW_DESCRIPTION	   = 'T',
	TAG_DATA_ROW		   = 'D',
	TAG_COMMAND_COMPLETE	   = 'C',
	TAG_READY_FOR_QUERY	   = 'Z',
	TAG_TERMINATE		   = 'X'
};

struct protocol_version {
	u16 minor;
	u16 major;
};

struct parameters {
	size_t size;
	size_t capacity;
	/*todo: use a mem heap*/
	char **keys;
	char **vals;
};

struct startup_message {
	/* The protocol version number. The most significant 16 bits are the
	 * major version number (3 for the protocol described here). The least
	 * significant 16 bits are the minor version number (0 for the protocol
	 * described here). */
	struct protocol_version protocol_version;
	struct parameters	params;
};

struct message {
	u8    type;
	u32   len;
	u8   *payload;
};

static u32 pgwire_read_packet_length(struct conn *conn)
{
	u8   buf[4];
	u32  len;

	if (read(conn->socket, buf, 4) < 4)
		return 0;
	if (!ut_read_4(buf, &len))
		return 0;
	return len;
}

static int read_message(struct conn *conn, struct message *message)
{
	ssize_t nread;
	u32	payload_len;

	nread = read(conn->socket, &message->type, 1);
	if (nread < 1)
		return 1;

	message->len = pgwire_read_packet_length(conn);
	if (message->len == 0)
		return 1;

	payload_len	 = message->len - sizeof(message->len);
	message->payload = malloc(payload_len);
	nread		 = read(conn->socket, message->payload, payload_len);
	if (nread < payload_len) {
		free(message->payload);
		return 1;
	}
	return 0;
}

static int write_message(struct conn *conn, struct message *message)
{
	u8   buf[4];
	u32  payload_len;

	if (write(conn->socket, &message->type, 1) < 1)
		return 1;
	ut_write_4(buf, message->len);
	if (write(conn->socket, buf, 4) < 4)
		return 1;
	payload_len = message->len - sizeof(message->len);
	if (write(conn->socket, message->payload, payload_len) < payload_len)
		return 1;
	return 0;
}

static const char *severity_str(enum errlevel l)
{
	switch (l) {
	case LOG:
		return "LOG";
	case INFO:
		return "INFO";
	case DEBUG:
		return "DEBUG";
	case NOTICE:
		return "NOTICE";
	case WARNING:
		return "WARNING";
	case ERROR:
		return "ERROR";
	case FATAL:
		return "FATAL";
	case PANIC:
		return "PANIC";
	}
}

extern struct err *errbuf_pop(void);

int pgwire_flush_errors(struct conn *conn)
{
	for (;;) {
		struct err *e = errbuf_pop();
		if (e == NULL)
			break;
		if (e->severity < INFO)
			continue;
		if (pgwire_send_error(conn, e))
			return 1;
	}
	return 0;
}

int pgwire_send_error(struct conn *conn, struct err *err)
{
	struct message message;
	u8	       buffer[1024];
	u8	      *ptr;

	message.type = TAG_ERROR_RESPONSE;

	ptr    = buffer;
	*ptr++ = 'S';
	ptr    = ut_write_str(ptr, severity_str(err->severity));
	*ptr++ = 'V';
	ptr    = ut_write_str(ptr, severity_str(err->severity));
	*ptr++ = 'C';
	ptr    = ut_write_str(ptr, errcode_to_str(err->code));
	*ptr++ = 'M';
	ptr    = ut_write_str(ptr, err->message);
	if (err->detail) {
		*ptr++ = 'D';
		ptr    = ut_write_str(ptr, err->detail);
	}
	if (err->hint) {
		*ptr++ = 'H';
		ptr    = ut_write_str(ptr, err->detail);
	}
	if (err->position > 0) {
		char buffer[1024];
		sprintf(buffer, "%lu", err->position);
		*ptr++ = 'P';
		ptr    = ut_write_str(ptr, buffer);
	}
	if (err->loc.file) {
		*ptr++ = 'F';
		ptr    = ut_write_str(ptr, err->loc.file);
	}
	if (err->loc.line) {
		char buffer[1024];
		sprintf(buffer, "%lu", err->loc.line);
		*ptr++ = 'L';
		ptr    = ut_write_str(ptr, buffer);
	}
	if (err->loc.routine) {
		*ptr++ = 'R';
		ptr    = ut_write_str(ptr, err->loc.routine);
	}
	*ptr++ = '\0';

	message.len	= (ptr - buffer) + sizeof(message.len);
	message.payload = malloc(ptr - buffer);
	memcpy(message.payload, buffer, ptr - buffer);
	return write_message(conn, &message);
}

static int read_startup_message(struct conn	       *conn,
				struct startup_message *message)
{
	u32	    len;
	u8	   *buf;
	const u8   *ptr;

	len = pgwire_read_packet_length(conn) - sizeof(len);
	buf = malloc(len);

	if (read(conn->socket, buf, len) < len)
		goto err;

	if ((ptr = ut_read_4(buf, (u32 *)&message->protocol_version)) == NULL)
		goto err;
	if (message->protocol_version.major != 3) {
		errlog(FATAL, errcode(ER_PROTOCOL_VIOLATION),
		       errmsg("Server only support protocol version 3"));
		goto err;
	}

	while (*ptr != '\0') {
		char key[1024];
		char val[1024];

		ptr = ut_read_str(ptr, key);
		ptr = ut_read_str(ptr, val);
		kvmap_put(&conn->parameters, key, val);
	}

	free(buf);
	return 0;
err:
	free(buf);
	return 1;
}

static int write_auth_ok(struct conn *conn)
{
	struct message message;

	message.type	= TAG_AUTHENTICATION_REQUEST;
	message.len	= 8;
	message.payload = malloc(4);
	ut_write_4(message.payload, 0);

	return write_message(conn, &message);
}

static int write_parameter_status(struct conn *conn, const char *key,
				  const char *val)
{
	struct message message;
	size_t	       keylen = strlen(key) + 1;
	size_t	       vallen = strlen(val) + 1;
	u8	      *ptr;

	message.type	= TAG_PARAMETER_STATUS;
	message.len	= sizeof(message.len) + keylen + vallen;
	message.payload = malloc(keylen + vallen);
	ptr		= message.payload;
	ptr		= ut_write_str(ptr, key);
	ptr		= ut_write_str(ptr, val);
	return write_message(conn, &message);
}

static int send_parameters(struct conn *conn)
{
	write_parameter_status(conn, "server_version", "1.0");
	write_parameter_status(conn, "client_encoding", "UTF8");
	return 0;
}

static int pgwire_close(struct conn *conn)
{
	struct message message;

	conn->state = CONN_CLOSED;

	message.type	= TAG_TERMINATE;
	message.payload = NULL;
	message.len	= sizeof(message.len);
	return write_message(conn, &message);
}

static int pgwire_startup(struct conn *conn)
{
	struct startup_message message;
	int		       err;

	conn->state = CONN_INIT;
	err	    = read_startup_message(conn, &message);
	if (!err)
		err = write_auth_ok(conn);
	if (!err)
		err = send_parameters(conn);

	if (err)
		pgwire_close(conn);
	else
		conn->state = CONN_IDLE;
	return err;
}

static int pgwire_ready_for_query(struct conn *conn)
{
	struct message message;

	conn->state = CONN_IDLE;

	message.type	   = TAG_READY_FOR_QUERY;
	message.len	   = 5;
	message.payload	   = malloc(1);
	message.payload[0] = 'I';
	return write_message(conn, &message);
}

int pgwire_read_query(struct conn *conn)
{
	struct message message;

	if (read_message(conn, &message))
		return 1;
	if (message.type != 'Q')
		return 1;
	conn->query = (char *)message.payload;
	return 0;
}

int pgwire_send_metadata(struct conn *conn, struct pgwire_rowdesc *row_desc)
{
	struct message message;
	u8	       buffer[1024];
	u8	      *ptr;
	int	       i;

	message.type = TAG_ROW_DESCRIPTION;

	ptr = buffer;
	ptr = ut_write_2(ptr, row_desc->numfields);
	for (i = 0; i < row_desc->numfields; ++i) {
		const struct pgwire_fielddesc *field = &row_desc->fields[i];
		ptr = ut_write_str(ptr, field->col);
		ptr = ut_write_4(ptr, field->tableoid);
		ptr = ut_write_2(ptr, field->colno);
		ptr = ut_write_4(ptr, field->typeoid);
		ptr = ut_write_2(ptr, field->typelen);
		ptr = ut_write_4(ptr, field->typmod);
		ptr = ut_write_2(ptr, field->format);
	}

	message.payload = malloc(ptr - buffer);
	memcpy(message.payload, buffer, ptr - buffer);
	message.len = (ptr - buffer) + sizeof(message.len);

	return write_message(conn, &message);
}

int pgwire_send_data(struct conn *conn, struct pgwire_datarow *row)
{
	struct message message;
	u8	       buffer[1024];
	u8	      *ptr;
	int	       i;

	message.type = TAG_DATA_ROW;

	ptr = buffer;
	ptr = ut_write_2(ptr, row->numfields);
	for (i = 0; i < row->numfields; ++i) {
		const struct pgwire_datarow_field *field = &row->fields[i];
		ptr = ut_write_4(ptr, field->fieldlen);
		memcpy(ptr, field->data, field->fieldlen);
		ptr += field->fieldlen;
	}

	message.payload = malloc(ptr - buffer);
	memcpy(message.payload, buffer, ptr - buffer);
	message.len = (ptr - buffer) + sizeof(message.len);

	return write_message(conn, &message);
}

static int pgwire_complete_command(struct conn *conn, void *query_tree)
{
	struct message message;

	message.type	= TAG_COMMAND_COMPLETE;
	if (*(u8 *)query_tree == COM_SELECT) {
		message.payload = (u8 *)"SELECT 1";
	} else {
		assert(*(u8 *)query_tree == COM_CREATE);
		message.payload = (u8 *)"CREATE TABLE";
	}
	message.len = strlen((char *)message.payload) + 1 + sizeof(message.len);

	return write_message(conn, &message);
}

static void make_row_desc(struct select *select, struct pgwire_rowdesc *rowdesc)
{
	int colno;

	rowdesc->numfields = select->select_list.size;
	rowdesc->fields	   = mem_zalloc(sizeof(struct pgwire_fielddesc) *
					rowdesc->numfields);
	for (colno = 0; colno < rowdesc->numfields; ++colno) {
		struct select_col *col =
			(struct select_col *)select->select_list.data[colno];
		if (col->name != NULL) {
			rowdesc->fields[colno].col =
				mem_alloc(strlen(col->name) + 1);
			strcpy((char *)rowdesc->fields[colno].col, col->name);
		} else if (col->type == SELECT_COL_FIELD) {
			rowdesc->fields[colno].col =
				mem_alloc(strlen(col->fieldname) + 1);
			strcpy((char *)rowdesc->fields[colno].col,
			       col->fieldname);
		} else {
			rowdesc->fields[colno].col =
				mem_alloc(sizeof("?col 123?") + 1);
			snprintf((char *)rowdesc->fields[colno].col,
				 sizeof("?col 123?"), "?col %d?", colno);
		}
		if (col->type == SELECT_COL_FIELD) {
			rowdesc->fields[colno].tableoid = col->tableoid;
			rowdesc->fields[colno].colno	= col->colno;
		}
		rowdesc->fields[colno].typeoid = col->typeoid;
		rowdesc->fields[colno].typelen = dtype_len(col->typeoid, -1);
		rowdesc->fields[colno].typmod  = col->typemod;
		rowdesc->fields[colno].format  = 0;
	}
}

static void to_text(u32 typeoid, i32 typemod, const u8 *data, char **out)
{
	*out = mem_alloc(1024);
	switch (typeoid) {
	case DTYPE_INT2:
	case DTYPE_INT4:
	case DTYPE_INT8:
		snprintf(*out, 1024, "%d", *(int *)data);
		break;
	case DTYPE_CHAR:
		snprintf(*out, typemod + 1, "%s", (char *)data);
		break;
	default:
		errlog(PANIC, errmsg("Serialization for dtype %u not implemented", typeoid));
		break;
	}
}

static void serialize_row(struct pgwire_rowdesc *rowdesc, struct row *row,
			  struct pgwire_datarow *dr)
{
	int colno;

	dr->numfields = row->nfields;
	dr->fields =
		mem_alloc(sizeof(struct pgwire_datarow_field) * row->nfields);
	for (colno = 0; colno < row->nfields; ++colno) {
		to_text(rowdesc->fields[colno].typeoid,
			rowdesc->fields[colno].typmod, row->fields[colno].data,
			(char **)&dr->fields[colno].data);
		dr->fields[colno].fieldlen =
			strlen((char *)dr->fields[colno].data);
	}
}

static int pgwire_execute_command(struct conn *conn)
{
	void		     *query_tree;
	struct cursor	      cur;
	struct pgwire_rowdesc rowdesc;

	assert(conn->query);
	assert(conn->state == CONN_IDLE);

	conn->state = CONN_RUN;

	if (parse(conn, &query_tree))
		return 1;

	if (*(u8 *)query_tree == COM_SELECT) {
		if (sql_select(query_tree, &cur))
			return 1;

		make_row_desc(query_tree, &rowdesc);
		if (pgwire_send_metadata(conn, &rowdesc))
			return 1;

		for (;;) {
			struct row	      row;
			struct pgwire_datarow pg_row;

			if (cursor_next(&cur, &row))
				break;

			serialize_row(&rowdesc, &row, &pg_row);
			if (pgwire_send_data(conn, &pg_row))
				return 1;
		}
	} else {
		assert(*(u8 *)query_tree == COM_CREATE);

		if (sql_create_table(query_tree))
			return 1;
	}

	pgwire_flush_errors(conn);

	if (pgwire_complete_command(conn, query_tree))
		return 1;

	return 0;
}

void pgwire_handle_connection(struct conn *conn)
{
	pgwire_startup(conn);
	if (conn->state == CONN_CLOSED)
		return;
	assert(conn->state == CONN_IDLE);

	for (;;) {
		mem_root_clear(&conn->mem_root);

		pgwire_ready_for_query(conn);

		if (pgwire_read_query(conn))
			break;
		errlog(DEBUG, errmsg("query: %s", conn->query));

		pgwire_execute_command(conn);

		if (pgwire_flush_errors(conn))
			break;
	}

	pgwire_close(conn);
}
