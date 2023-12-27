#include "connection.h"
#include "util/mem.h"

int conn_init(struct conn *conn, int socket)
{
	conn->socket = socket;
	conn->state  = CONN_CLOSED;
	conn->query  = NULL;
	kvmap_init(&conn->parameters, 10);
	mem_root_init(&conn->mem_root);
	return 0;
}
