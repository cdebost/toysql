#include "error.h"

#include <assert.h>

const char *errcode(enum errtype e)
{
	switch (e) {
	case ER_SUCCESS:
		return "00000";
	case ER_NO_DATA:
		return "02000";
	case ER_PROTOCOL_VIOLATION:
		return "08P01";
	case ER_FEATURE_NOT_SUPPORTED:
		return "0A000";
	case ER_SYNTAX_ERROR:
		return "42601";
	case ER_INTERNAL_ERROR:
		return "XX000";
	default:
		assert(0);
		return "XX000";
	}
}
