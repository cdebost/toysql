#ifndef LEX_H
#define LEX_H

#include "univ.h"

enum token_class {
	TK_INVALID,
	TK_EOF,
	/* Any whitespace characters */
	TK_SPACE,

	/* A number literal like 123 or -456 */
	TK_NUM,
	/* A single-quoted string constant like 'abc' */
	TK_STR,

	/* A non-keyword identifier */
	TK_IDENT,

	TK_COMMA,
	TK_SEMICOLON,
	TK_DOT,
	TK_PAREN_OPEN,
	TK_PAREN_CLOSE,

	TK_PLUS,
	TK_MINUS,
	TK_STAR,

	TK_AS,
	TK_FROM,
	TK_SELECT,
};

struct lex_str {
	const char *str;
	size_t	    len;
};

struct lex_token {
	enum token_class tclass;
	size_t		 begin;
	size_t		 end;
	union {
		struct lex_str val_str;
		u64	       val_int;
	};
};

struct lex {
	char const *str;
	i64 pos;
	struct lex_token token;
};

void lex_init(struct lex *lex, const char *str);

/* Read the next token and advance the lexer past that token */
void lex_next_token(struct lex *lex);

#endif // LEX_H
