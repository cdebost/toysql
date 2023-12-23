#include "connection.h"
#include "kvmap.h"

int conn_init(struct conn *conn, int socket)
{
	conn->socket = socket;
	conn->state  = CONN_CLOSED;
	conn->query  = NULL;
	kvmap_init(&conn->parameters, 10);
	return 0;
}
