#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

enum errlevel { LOG, INFO, DEBUG, NOTICE, WARNING, ERROR, FATAL, PANIC };

enum errtype {
	ER_SUCCESS,
	ER_NO_DATA,
	ER_PROTOCOL_VIOLATION,
	ER_FEATURE_NOT_SUPPORTED,
	ER_SYNTAX_ERROR,
	ER_INTERNAL_ERROR
};

struct err_srcloc {
	const char *file;
	size_t	    line;
	const char *routine;
};

#define err_here ((struct err_srcloc){ __FILE__, __LINE__, __func__ })

struct err {
	enum errlevel severity;
	enum errtype  type;
	/* Primary human-readable error message */
	const char *message;
	/* Optional secondary message */
	const char *detail;
	/* Optional fix suggestion */
	const char *hint;
	/* Index in the query string that the error occurred */
	size_t position;
	/* Source code location where the error occurred */
	struct err_srcloc loc;
};

const char *errcode(enum errtype e);

#endif
