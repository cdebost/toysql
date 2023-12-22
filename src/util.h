#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "univ.h"

#define panic(msg)            \
	do {                  \
		printf(msg);  \
		printf("\n"); \
		exit(1);      \
	} while (0);

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

static inline void ut_print_hexdump(char *buf, size_t len)
{
	int row_start = 0;
	for (;;) {
		char   row[16];
		size_t row_len;
		int    i;

		printf("%08X: ", row_start);

		if (len - row_start < 16)
			row_len = len - row_start;
		else
			row_len = 16;

		memcpy(row, buf + row_start, row_len);

		for (i = 0; i < row_len; ++i)
			printf("%02X ", buf[row_start + i]);
		for (; i < 16; ++i)
			printf("   ");
		printf(" ");
		for (i = 0; i < row_len; ++i) {
			char c = buf[row_start + i];
			if (c == '\0')
				printf("\033[31m.\033[0m");
			else if (isprint(c))
				printf("\033[32m%c\033[0m", c);
			else
				printf(".");
		}
		printf("\n");
		if (row_len < 16)
			break;
		row_start += 16;
	}
}

#endif // UTIL_H
