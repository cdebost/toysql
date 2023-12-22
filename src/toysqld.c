#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "pgwire.h"

static void handle_connection(int sock)
{
	if (pgwire_handshake(sock)) {
		pgwire_terminate(sock);
		return;
	}
	for (;;) {
		char		     *query;
		struct pgwire_rowdesc rowdesc;
		struct pgwire_datarow row;

		if (pgwire_read_query(sock, &query))
			break;
		printf("query: %s\n", query);

		rowdesc.numfields = 1;
		rowdesc.fields	  = malloc(sizeof(struct pgwire_fielddesc) * 1);
		rowdesc.fields[0].col	   = "id";
		rowdesc.fields[0].tableoid = 0;
		rowdesc.fields[0].colno	   = 0;
		rowdesc.fields[0].typeoid  = 18; /*char*/
		rowdesc.fields[0].typelen  = 20;
		rowdesc.fields[0].typmod   = 0;
		rowdesc.fields[0].format   = 0;
		if (pgwire_send_metadata(sock, &rowdesc))
			break;
		free(rowdesc.fields);

		row.numfields = 1;
		row.fields    = malloc(sizeof(struct pgwire_datarow_field) * 1);
		row.fields[0].fieldlen = 5;
		row.fields[0].data     = malloc(5);
		memcpy(row.fields[0].data, "Dummy", 5);
		if (pgwire_send_data(sock, &row))
			break;
		free(row.fields[0].data);
		free(row.fields);

		if (pgwire_complete_command(sock))
			break;
	}
	pgwire_terminate(sock);
}

static int create_server(struct sockaddr_un *sockname, int *sock)
{
	int	  new_sock;
	int	  opt	   = 1;
	socklen_t name_len = SUN_LEN(sockname);

	if ((*sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		return 1;
	}
	if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		return 1;
	}
	if (bind(*sock, (struct sockaddr *)sockname, name_len) < 0) {
		perror("bind failed");
		return 1;
	}
	if (listen(*sock, 3) < 0) {
		perror("listen");
		return 1;
	}
	if ((new_sock = accept(*sock, (struct sockaddr *)sockname, &name_len)) <
	    0) {
		perror("accept");
		return 1;
	}
	handle_connection(new_sock);
	close(new_sock);
	return 0;
}

int main(void)
{
	int		   sock;
	struct sockaddr_un name;

	name.sun_family = AF_LOCAL;
	strcpy(name.sun_path, "./.s.PGSQL.5432");

	if (unlink(name.sun_path) && errno != ENOENT) {
		perror("unlink");
		exit(EXIT_FAILURE);
	}

	if (create_server(&name, &sock))
		exit(EXIT_FAILURE);

	close(sock);
	return 0;
}
