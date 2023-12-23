#ifndef CONNECTION_H
#define CONNECTION_H

#include "kvmap.h"

enum conn_state {
	/*The connection is closed*/
	CONN_CLOSED = 0,
	/*The connection is being opened but handshake is not yet complete*/
	CONN_INIT,
	/*The connection is open and waiting for a query from the client*/
	CONN_IDLE,
	/*The connection is actively running a query*/
	CONN_RUN
};

struct conn {
	/* File descriptor for the tcp socket */
	int socket;

	/* Current state of the connection */
	enum conn_state state;

	/* Current query being processed */
	char *query;

	/* Map of session parameters */
	struct kvmap parameters;
};

int conn_init(struct conn *conn, int socket);

#endif // CONNECTION_H
