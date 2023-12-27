#ifndef BYTES_H
#define BYTES_H

#include "univ.h"

#include <string.h>

static inline char *ut_read_2(char *bytes, u16 *val)
{
	*val = (bytes[0] << 8) + bytes[1];
	return bytes + 4;
}

static inline char *ut_read_4(char *bytes, u32 *val)
{
	*val = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];
	return bytes + 4;
}

static inline char *ut_write_2(char *bytes, u16 val)
{
	bytes[0] = (val >> 8) & 0xFF;
	bytes[1] = (val >> 0) & 0xFF;
	return bytes + 2;
}

static inline char *ut_write_4(char *bytes, u32 val)
{
	bytes[0] = (val >> 24) & 0xFF;
	bytes[1] = (val >> 16) & 0xFF;
	bytes[2] = (val >> 8) & 0xFF;
	bytes[3] = (val >> 0) & 0xFF;
	return bytes + 4;
}

static inline const char *ut_read_str(const char *bytes, char *str)
{
	size_t len = strlen(bytes);
	memcpy(str, bytes, len + 1);
	return bytes + len + 1;
}

static inline char *ut_write_str(char *bytes, const char *str)
{
	int len = strlen(str);
	memcpy(bytes, str, len + 1);
	return bytes + len + 1;
}

#endif
