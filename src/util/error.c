#include "error.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static _Thread_local struct {
	struct err ring[64];
	u8	   head;
	u8	   tail;
} errbuf = { .head = 0, .tail = 0 };

static const u8 errbuf_ring_size = sizeof(errbuf.ring) / sizeof(struct err);

static u8 errbuf_ptr_prev(u8 ptr)
{
	return ptr == 0 ? errbuf_ring_size - 1 : ptr - 1;
}
static u8 errbuf_ptr_next(u8 ptr)
{
	return ptr == errbuf_ring_size - 1 ? 0 : ptr + 1;
}

#define first_err (&errbuf.ring[errbuf.head])
#define last_err (&errbuf.ring[errbuf_ptr_prev(errbuf.tail)])

void errpush(enum errlevel l)
{
	errbuf.tail = errbuf_ptr_next(errbuf.tail);
	memset(last_err, 0, sizeof(struct err));
	last_err->severity = l;
}

void errcode(enum errcode c)
{
	last_err->code = c;
}

void errmsg(const char *fmt, ...)
{
	char   *msg = malloc(4096);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, 4096, fmt, args);
	va_end(args);

	last_err->message = msg;
}

void errdetail(const char *detail)
{
	last_err->detail = detail;
}

void errhint(const char *hint)
{
	last_err->hint = hint;
}

void errpos(size_t pos)
{
	last_err->position = pos;
}

static void print_error_to_log(void);

void errfinish(const char *file, size_t line, const char *routine)
{
	last_err->loc.file    = file;
	last_err->loc.line    = line;
	last_err->loc.routine = routine;

	print_error_to_log();

	if (last_err->severity == PANIC)
		exit(EXIT_FAILURE);
}

static const char *sev_to_str(enum errlevel l)
{
	switch (l) {
	case LOG:
		return "LOG";
	case INFO:
		return "INFO";
	case DEBUG:
		return "DEBUG";
	case NOTICE:
		return "NOTICE";
	case WARNING:
		return "WARNING";
	case ERROR:
		return "ERROR";
	case FATAL:
		return "FATAL";
	case PANIC:
		return "PANIC";
	}
}

const char *errcode_to_str(enum errcode c)
{
	switch (c) {
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

static void print_timestamp(void)
{
	time_t	   now;
	struct tm *tm;
	int	   year, month, day, hour, minute, second;

	time(&now);
	tm = gmtime(&now);

	year   = tm->tm_year + 1900;
	month  = tm->tm_mon + 1;
	day    = tm->tm_mday;
	hour   = tm->tm_hour;
	minute = tm->tm_min;
	second = tm->tm_sec;

	fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour,
		minute, second);
}

static void print_error_to_log(void)
{
	print_timestamp();
	fprintf(stderr, " [%s]", sev_to_str(last_err->severity));
	if (last_err->code > ER_SUCCESS)
		fprintf(stderr, "[%s]", errcode_to_str(last_err->code));
	fprintf(stderr, " %s", last_err->message);
	if (last_err->detail)
		fprintf(stderr, ": %s", last_err->detail);
	fprintf(stderr, " (%s:%lu)\n", last_err->loc.file, last_err->loc.line);
}

struct err *errbuf_pop(void)
{
	struct err *e;

	if (errbuf.head == errbuf.tail)
		return NULL;
	e	    = first_err;
	errbuf.head = errbuf_ptr_next(errbuf.head);
	return e;
}
