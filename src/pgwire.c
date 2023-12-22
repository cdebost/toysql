#include "pgwire.h"

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

static int read_message(int socket, struct message *message)
{
	ssize_t nread;
	char	buffer[1024];
	u32	payload_len;

	nread = read(socket, &message->type, 1);
	if (nread < 1)
		return 1;
	nread = read(socket, &buffer, 4);
	if (nread < 4)
		return 1;
	ut_read_4(buffer, &message->len);
	payload_len	 = message->len - sizeof(message->len);
	message->payload = malloc(payload_len);
	nread		 = read(socket, message->payload, payload_len);
	if (nread < payload_len)
		return 1;
	return 0;
}

static int write_message(int socket, struct message *message)
{
	char buffer[1024];

	write(socket, &message->type, 1);
	ut_write_4(buffer, message->len);
	write(socket, buffer, 4);
	write(socket, message->payload, message->len - sizeof(message->len));

	return 0;
}

static int read_startup_message(int socket, struct startup_message *message)
{
	char	    buffer[1024];
	const char *ptr;
	ssize_t	    nread;
	u32	    len;

	nread = read(socket, buffer, 4);
	if (nread < 4)
		return 1;
	if (!ut_read_4(buffer, &len))
		return 1;

	nread = read(socket, buffer, 4);
	if (nread < 4)
		return 1;
	if (!ut_read_4(buffer, (u32 *)&message->protocol_version))
		return 1;

	nread = read(socket, buffer, len - 8);

	ptr = buffer;
	while (*ptr != '\0') {
		char key[1024];
		char val[1024];

		ptr = ut_read_str(ptr, key);
		ptr = ut_read_str(ptr, val);
		printf("%s: %s\n", key, val);
	}

	return 0;
}

static int write_auth_ok(int socket)
{
	struct message message;

	message.type	= TAG_AUTHENTICATION_REQUEST;
	message.len	= 8;
	message.payload = malloc(4);
	ut_write_4(message.payload, 0);

	return write_message(socket, &message);
}

static int write_parameter_status(int socket, const char *key, const char *val)
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
	return write_message(socket, &message);
}

static int send_parameters(int socket)
{
	write_parameter_status(socket, "server_version", "1.0");
	write_parameter_status(socket, "client_encoding", "UTF8");
	return 0;
}

int pgwire_handshake(int sock)
{
	struct startup_message message;

	if (read_startup_message(sock, &message))
		return 1;
	if (message.protocol_version.major != 3) {
		struct pgwire_error error;
		memset(&error, 0, sizeof(error));
		error.severity = PGWIRE_FATAL;
		error.code     = "08P01";
		error.message  = "Only supports protocol version 3";
		pgwire_send_error(sock, &error);
		return 1;
	}
	if (write_auth_ok(sock))
		return 1;
	if (send_parameters(sock))
		return 1;
	return 0;
}

int pgwire_send_error(int socket, struct pgwire_error *error)
{
	struct message message;
	char	       buffer[1024];
	char	      *ptr;

	message.type = TAG_ERROR_RESPONSE;

	ptr    = buffer;
	*ptr++ = 'S';
	ptr    = ut_write_str(ptr, error->severity);
	*ptr++ = 'V';
	ptr    = ut_write_str(ptr, error->severity);
	*ptr++ = 'C';
	ptr    = ut_write_str(ptr, error->code);
	*ptr++ = 'M';
	ptr    = ut_write_str(ptr, error->message);
	if (error->detail) {
		*ptr++ = 'D';
		ptr    = ut_write_str(ptr, error->detail);
	}

	message.len	= (ptr - buffer) + sizeof(message.len);
	message.payload = malloc(ptr - buffer);
	memcpy(message.payload, buffer, ptr - buffer);
	return write_message(socket, &message);
}

static int write_ready_for_query(int socket)
{
	struct message message;

	message.type	   = TAG_READY_FOR_QUERY;
	message.len	   = 5;
	message.payload	   = malloc(1);
	message.payload[0] = 'I';

	return write_message(socket, &message);
}

int pgwire_read_query(int socket, char **query)
{
	struct message message;

	if (write_ready_for_query(socket))
		return 1;

	if (read_message(socket, &message))
		return 1;
	if (message.type != 'Q')
		return 1;
	*query = message.payload;
	return 0;
}

int pgwire_send_metadata(int socket, struct pgwire_rowdesc *row_desc)
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

	return write_message(socket, &message);
}

int pgwire_send_data(int socket, struct pgwire_datarow *row)
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

	return write_message(socket, &message);
}

int pgwire_complete_command(int socket)
{
	struct message message;

	message.type	= TAG_COMMAND_COMPLETE;
	message.payload = "SELECT 1";
	message.len	= strlen(message.payload) + 1 + sizeof(message.len);

	return write_message(socket, &message);
}

int pgwire_terminate(int socket)
{
	struct message message;

	message.type	= TAG_TERMINATE;
	message.payload = NULL;
	message.len	= sizeof(message.len);

	return write_message(socket, &message);
}
