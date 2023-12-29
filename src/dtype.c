#include "dtype.h"

#include <assert.h>

struct dtype dtypes[] = {
	[DTYPE_INT2] = {DTYPE_INT2, "int2", 2},
	[DTYPE_INT4] = {DTYPE_INT4, "int4", 4},
	[DTYPE_INT8] = {DTYPE_INT8, "int8", 8},
	[DTYPE_CHAR] = {DTYPE_CHAR, "char", -1},
};

size_t dtype_len(u32 typeoid, u32 typemod)
{
	struct dtype *dtype;

	assert(typeoid < sizeof(dtypes) / sizeof(struct dtype));
	dtype = &dtypes[typeoid];
	if (dtype->len >= 0)
		return dtype->len;
	else
		return typemod;
}
