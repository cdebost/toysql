#ifndef DEBUG_H
#define DEBUG_H

#include "univ.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static inline void ut_print_hexdump(u8 *buf, size_t len)
{
	int row_start = 0;
	for (;;) {
		u8     row[16];
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
			u8 c = buf[row_start + i];
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

#endif
