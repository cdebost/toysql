/* SQL datatypes */

#ifndef DTYPES_H
#define DTYPES_H

#include "univ.h"
#include "util/mem.h"

struct dtype {
	/* Object id of this type */
	u32 oid;
	/* Human friendly name of the type */
	const char *name;
	/* Length in bytes of the type, or -1 for variable length */
	i16 len;
};

extern struct dtype dtypes[];

enum dtype_oids {
	DTYPE_INVALID = 0,

	/* smallint */
	DTYPE_INT2 = 21,
	/* integer */
	DTYPE_INT4 = 23,
	/* bigint */
	DTYPE_INT8 = 20,

	/* char */
	DTYPE_CHAR = 18
};

size_t dtype_len(u32 typeoid, u32 typemod);

#endif
