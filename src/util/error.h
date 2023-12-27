#ifndef ERROR_H
#define ERROR_H

#include "univ.h"

#include <stddef.h>

enum errlevel { DEBUG, LOG, INFO, NOTICE, WARNING, ERROR, FATAL, PANIC };

enum errcode {
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

struct err {
	enum errlevel severity;
	enum errcode  code;
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

#define errlog(sev, ...)                                 \
	do {                                             \
		errpush(sev);                            \
		__VA_ARGS__;                             \
		errfinish(__FILE__, __LINE__, __func__); \
	} while (0)

void errpush(enum errlevel l);
void errcode(enum errcode c);
void errmsg(const char *fmt, ...);
void errdetail(const char *detail);
void errhint(const char *hint);
void errpos(size_t pos);
void errfinish(const char *file, size_t line, const char *routine);

const char *errcode_to_str(enum errcode c);

struct err *errbuf_pop(void);

#endif
