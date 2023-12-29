#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "connection.h"
#include "pgwire.h"
#include "sys.h"
#include "util/error.h"

static int create_server(struct sockaddr_un *sockname, int *sock)
{
	int	  new_sock;
	int	  opt	   = 1;
	socklen_t name_len = SUN_LEN(sockname);
	struct conn conn;

	if ((*sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		return 1;
	}
	if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		return 1;
	}
	if (setsockopt(*sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt))) {
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
	for (;;) {
		if ((new_sock = accept(*sock, (struct sockaddr *)sockname,
				       &name_len)) < 0) {
			fprintf(stderr, "accept fail\n");
			perror("accept");
			return 1;
		}
		conn_init(&conn, new_sock);
		pgwire_handle_connection(&conn);
		close(new_sock);
	}
	return 0;
}

extern void init_dummy_tables(void);

int main(void)
{
	int		   sock;
	struct sockaddr_un name;

	sys_bootstrap();
	sys_load_table_by_name("tables");

	init_dummy_tables();

	errlog(LOG, errmsg("toysqld starting as process %d", getpid()));

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
