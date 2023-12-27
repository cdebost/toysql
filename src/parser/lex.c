#include "lex.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static size_t scan(const char *str, enum token_class *type)
{
	char buf[1024];
	u16  i;

	if (str == NULL || str[0] == '\0') {
		*type = TK_EOF;
		return 0;
	}

	if (isspace(str[0])) {
		for (i = 1; isspace(str[i]); ++i)
			;
		*type = TK_SPACE;
		return i;
	}

	switch (str[0]) {
	case ',':
		*type = TK_COMMA;
		return 1;
	case ';':
		*type = TK_SEMICOLON;
		return 1;
	case '.':
		*type = TK_DOT;
		return 1;
	case '(':
		*type = TK_PAREN_OPEN;
		return 1;
	case ')':
		*type = TK_PAREN_CLOSE;
		return 1;
	case '+':
		*type = TK_PLUS;
		return 1;
	case '-':
		if (isnumber(str[1])) {
			for (i = 2; isnumber(str[i]); ++i)
				;
			*type = TK_NUM;
			return i;
		} else {
			*type = TK_MINUS;
			return 1;
		}
		break;
	case '*':
		*type = TK_STAR;
		return 1;
	}

	if (isnumber(str[0])) {
		for (i = 1; isnumber(str[i]); ++i)
			;
		*type = TK_NUM;
		return i;
	}

	if (str[0] == '\'') {
		for (i = 1; str[i] != '\'' && str[i] != '\0'; ++i)
			;
		if (str[i] == '\0')
			*type = TK_INVALID;
		else {
			*type = TK_STR;
			++i;
		}
		return i;
	}

	if (str[0] == '"') {
		for (i = 1; str[i] != '"' && str[i] != '\0'; ++i)
			;
		if (str[i] == '\0')
			*type = TK_INVALID;
		else {
			*type = TK_IDENT;
			++i;
		}
		return i;
	}

	if (!isalpha(str[0])) {
		*type = TK_INVALID;
		return 1;
	}

	// TODO: growable buffer
	memset(buf, 0, 1024);
	buf[0] = toupper(str[0]);
	for (i = 1; isalnum(str[i]); ++i)
		buf[i] = toupper(str[i]);
	buf[i] = '\0';

	if (strcmp(buf, "AS") == 0)
		*type = TK_KEYWORD;
	else if (strcmp(buf, "SELECT") == 0)
		*type = TK_KEYWORD;
	else
		*type = TK_IDENT;

	return i;

}

static void evaluate(char const *str, size_t len, struct token *token)
{
	char *buf;
	int i;

	switch (token->tclass) {
	case TK_NUM:
		buf = malloc(len + 1);
		memcpy(buf, str, len);
		buf[len]       = '\0';
		token->val_int = atoi(buf);
		free(buf);
		break;
	case TK_STR:
		token->val_str.str = str + 1; /*skip open quote*/
		token->val_str.len = len - 2; /*open/close quotes*/
		break;
	case TK_IDENT:
		token->val_str.str = str;
		token->val_str.len = len;
		break;
	case TK_KEYWORD:
		buf = malloc(len + 1);
		for (i = 0; i < len; ++i)
			buf[i] = toupper(str[i]);
		buf[len] = '\0';
		if (strcmp(buf, "AS") == 0)
			token->keyword = KW_AS;
		else if (strcmp(buf, "SELECT") == 0)
			token->keyword = KW_SELECT;
		else
			assert(0);
	default:
		break;
	}
}

void lex_init(struct lex *lex, const char *str)
{
	lex->str = str;
	lex->pos = 0;
}

void lex_next_token(struct lex *lex, struct token *token)
{
	enum token_class tclass;
	size_t		 tsz;

	tsz	      = scan(lex->str, &tclass);
	token->tclass = tclass;
	evaluate(lex->str, tsz, token);
	lex->str += tsz;
	lex->pos += tsz;
}
