#include "lex.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Must match order of keywords in token_class in lex.h */
static const char *keyword_names[] = { "AS", "FROM", "SELECT" };

static size_t scan(const char *str, enum token_class *type)
{
	u16  i;
	u16  k;

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

	for (i = 1; isalnum(str[i]); ++i)
		;
	*type = TK_IDENT;

	for (k = 0; k < sizeof(keyword_names) / sizeof(keyword_names[0]); ++k) {
		if (strncasecmp(str, keyword_names[k],
				strlen(keyword_names[k])) == 0) {
			*type = TK_AS + k;
			break;
		}
	}

	return i;
}

static void evaluate(char const *str, size_t len, struct lex_token *token)
{
	char *buf;

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
	default:
		break;
	}
}

void lex_init(struct lex *lex, const char *str)
{
	lex->str = str;
	lex->pos = 0;
	memset(&lex->token, 0, sizeof(lex->token));
}

void lex_next_token(struct lex *lex)
{
	size_t		 tsz;

	tsz = scan(lex->str, &lex->token.tclass);
	evaluate(lex->str, tsz, &lex->token);
	lex->token.begin = lex->pos;
	lex->token.end	 = lex->pos + tsz;
	lex->str += tsz;
	lex->pos += tsz;
}
