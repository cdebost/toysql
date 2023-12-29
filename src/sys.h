/* System schema */

#ifndef SYS_H
#define SYS_H

#include "table.h"

/* Initialize the system schema with basic tables. */
void sys_bootstrap(void);

/* Add a table to the table catalog */
int sys_add_table(struct table *tab);

struct table *sys_load_table_by_name(const char *name);

#endif // SYS_H
