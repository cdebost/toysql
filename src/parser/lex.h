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
};

struct lex_str {
	const char *str;
	size_t	    len;
};

struct token {
	enum token_class tclass;
	union {
		struct lex_str val_str;
		u64	       val_int;
	};
};

struct lex {
	char const *str;
};

void lex_init(struct lex *lex, const char *str);

/* Read the next token and advance the lexer past that token */
void lex_next_token(struct lex *lex, struct token *token);

#endif // LEX_H
