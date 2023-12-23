#include "pgwire.h"

#include "connection.h"
#include "error.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "util.h"

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
	char  type;
	u32   len;
	char *payload;
};

static u32 pgwire_read_packet_length(struct conn *conn)
{
	char buf[4];
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
	char buf[4];
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

int pgwire_send_error(struct conn *conn, struct err *err)
{
	struct message message;
	char	       buffer[1024];
	char	      *ptr;

	message.type = TAG_ERROR_RESPONSE;

	ptr    = buffer;
	*ptr++ = 'S';
	ptr    = ut_write_str(ptr, severity_str(err->severity));
	*ptr++ = 'V';
	ptr    = ut_write_str(ptr, severity_str(err->severity));
	*ptr++ = 'C';
	ptr    = ut_write_str(ptr, errcode(err->type));
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

	fprintf(stderr, "[%s] %s (%s:%lu)\n", severity_str(err->severity),
		err->message, err->loc.file, err->loc.line);

	message.len	= (ptr - buffer) + sizeof(message.len);
	message.payload = malloc(ptr - buffer);
	memcpy(message.payload, buffer, ptr - buffer);
	return write_message(conn, &message);
}

static int read_startup_message(struct conn	       *conn,
				struct startup_message *message)
{
	u32	    len;
	char	   *buf;
	const char *ptr;

	len = pgwire_read_packet_length(conn) - sizeof(len);
	buf = malloc(len);

	if (read(conn->socket, buf, len) < len)
		goto err;

	if ((ptr = ut_read_4(buf, (u32 *)&message->protocol_version)) == NULL)
		goto err;
	if (message->protocol_version.major != 3) {
		struct err err;
		memset(&err, 0, sizeof(err));
		err.severity = FATAL;
		err.type     = ER_PROTOCOL_VIOLATION;
		err.message  = "Server only supports protocol version 3";
		pgwire_send_error(conn, &err);
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
	char	      *ptr;

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
	conn->query = message.payload;
	return 0;
}

int pgwire_send_metadata(struct conn *conn, struct pgwire_rowdesc *row_desc)
{
	struct message message;
	char	       buffer[1024];
	char	      *ptr;
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
	char	       buffer[1024];
	char	      *ptr;
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

int pgwire_complete_command(struct conn *conn)
{
	struct message message;

	message.type	= TAG_COMMAND_COMPLETE;
	message.payload = "SELECT 1";
	message.len	= strlen(message.payload) + 1 + sizeof(message.len);

	return write_message(conn, &message);
}

void pgwire_handle_connection(struct conn *conn)
{
	pgwire_startup(conn);
	if (conn->state == CONN_CLOSED)
		return;
	assert(conn->state == CONN_IDLE);

	for (;;) {
		struct pgwire_rowdesc rowdesc;
		struct pgwire_datarow row;

		pgwire_ready_for_query(conn);

		if (pgwire_read_query(conn))
			break;
		printf("query: %s\n", conn->query);

		conn->state = CONN_RUN;

		rowdesc.numfields = 1;
		rowdesc.fields	  = malloc(sizeof(struct pgwire_fielddesc) * 1);
		rowdesc.fields[0].col	   = "id";
		rowdesc.fields[0].tableoid = 0;
		rowdesc.fields[0].colno	   = 0;
		rowdesc.fields[0].typeoid  = 18; /*char*/
		rowdesc.fields[0].typelen  = 20;
		rowdesc.fields[0].typmod   = 0;
		rowdesc.fields[0].format   = 0;
		if (pgwire_send_metadata(conn, &rowdesc))
			break;
		free(rowdesc.fields);

		row.numfields = 1;
		row.fields    = malloc(sizeof(struct pgwire_datarow_field) * 1);
		row.fields[0].fieldlen = 5;
		row.fields[0].data     = malloc(5);
		memcpy(row.fields[0].data, "Dummy", 5);
		if (pgwire_send_data(conn, &row))
			break;
		free(row.fields[0].data);
		free(row.fields);

		if (pgwire_complete_command(conn))
			break;
	}

	pgwire_close(conn);
}
