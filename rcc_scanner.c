/*
 * RCC - Rational C Compiler
 * (C scanner)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <stdarg.h>
#include <assert.h>

#include <ir.h>
#include <ir_private.h>

#include "rcc.h"

/* Scanner Hash Table */
static uint32_t yy_str_hash(const char *str, size_t len)
{
	size_t i;
	uint32_t h = 5381;

    for (i = 0; i < len; i++) {
        h = ((h << 5) + h) + *(const unsigned char*)str;
        str++;
    }
    return h | 0x10000000;
}

void yy_hash_init(rcc_ctx *rcc)
{
	yy_hashtab *hash = &rcc->yy_hash;

	char *data = ir_mem_malloc(1024 * sizeof(uint32_t) + 1024 * sizeof(yy_hash_bucket));
	memset(data, 0, 1024 * sizeof(uint32_t));
	hash->data = (yy_hash_bucket*)(data + (1024 * sizeof(uint32_t)));
	hash->count = 0;
	hash->size = 1024;
	hash->mask = (uint32_t)(-(int32_t)1024);
}

void yy_hash_free(rcc_ctx *rcc)
{
	yy_hashtab *hash = &rcc->yy_hash;

	ir_mem_free((char*)hash->data - (hash->size * sizeof(uint32_t)));
}

static IR_NEVER_INLINE void yy_hash_resize(yy_hashtab *hash)
{
	uint32_t old_size = (uint32_t)(-(int32_t)hash->mask);
	yy_hash_bucket *old_data = hash->data;
	uint32_t new_size = old_size * 2;
	yy_hash_bucket *data = ir_mem_malloc(new_size * sizeof(uint32_t) + new_size * sizeof(yy_hash_bucket));
	yy_hash_bucket *p;
	uint32_t i, mask;

	memset(data, 0, new_size * sizeof(uint32_t));
	hash->data = data = (yy_hash_bucket*)((char*)data + (new_size * sizeof(uint32_t)));
	hash->mask = (uint32_t)(-(int32_t)new_size);
	hash->size = new_size;

	memcpy(hash->data, old_data, hash->count * sizeof(yy_hash_bucket));
	ir_mem_free((char*)old_data - (old_size * sizeof(uint32_t)));

	mask = hash->mask;
	for (i = YY_FIRST_KEYWORD, p = hash->data + i; i < hash->count; p++, i++) {
		uint32_t h = p->h | mask;
		p->next = ((uint32_t*)data)[(int32_t)h];
		((uint32_t*)data)[(int32_t)h] = i;
	}
}

yy_sym yy_hash_find(rcc_ctx *rcc, const char *str, size_t len)
{
	uint32_t h = yy_str_hash(str, len);
	yy_hashtab *hash = &rcc->yy_hash;
	yy_hash_bucket *data = hash->data;
	uint32_t pos = ((uint32_t*)data)[(int32_t)(h | hash->mask)];
	yy_hash_bucket *p;

	while (pos != 0) {
		p = data + pos;
		if (p->h == h
		 && p->len == len
		 && memcmp(p->str, str, len) == 0) {
			return pos;
		}
		pos = p->next;
	}

	return 0;
}

IR_ALWAYS_INLINE yy_sym _yy_hash_lookup_ex(rcc_ctx *rcc, const char *str, size_t len, uint32_t h)
{
	yy_hashtab *hash = &rcc->yy_hash;
	yy_hash_bucket *data = hash->data;
	uint32_t pos = ((uint32_t*)data)[(int32_t)(h | hash->mask)];
	yy_hash_bucket *p;

	while (pos != 0) {
		p = data + pos;
		if (p->h == h
		 && p->len == len
		 && memcmp(p->str, str, len) == 0) {
			return pos;
		}
		pos = p->next;
	}

	if (UNEXPECTED(hash->count == hash->size)) {
		yy_hash_resize(hash);
		data = hash->data;
	}

	char *holder = ir_arena_alloc(&rcc->yy_arena, len + 1);
	memcpy(holder, str, len);
	holder[len] = 0;
	str = holder;

	pos = hash->count++;
	p = data + pos;
	p->h = h;
	p->len = len;
	p->str = str;
	h |= hash->mask;
	p->next = ((uint32_t*)data)[(int32_t)h];
	((uint32_t*)data)[(int32_t)h] = pos;
	p->macro = NULL;
	p->macro_stack = NULL;
	p->sym = NULL;
	p->tag = NULL;
	p->label = NULL;
	p->link = NULL;

	return pos;
}

static IR_NEVER_INLINE yy_sym yy_hash_lookup_ex(rcc_ctx *rcc, const char *str, size_t len, uint32_t h)
{
	return _yy_hash_lookup_ex(rcc, str, len, h);
}

yy_sym yy_hash_lookup(rcc_ctx *rcc, const char *str, size_t len)
{
	return _yy_hash_lookup_ex(rcc, str, len, yy_str_hash(str, len));
}

/* Scanner */
static void yy_scanner_error(rcc_ctx *rcc) yy_noreturn;
static bool yy_at_start_of_line(rcc_ctx *rcc, const char *text);
static yy_sym yy_parse_pp_number(rcc_ctx *rcc, const char *str, size_t len);

// TODO: Trigraphs are not supported yet ???
#define _YY_TRIGRAPHS(_) \
	_("<:",                            YY__LBRACK)                \
	_(":>",                            YY__RBRACK)                \
	_("<%:",                           YY__LBRACE)                \
	_("%>",                            YY__RBRACE)                \
	_("%:",                            YY__HASH)                  \
	_("%:%:",                          YY__HASH_HASH)             \

yy_sym yy_next(rcc_ctx *rcc)
{
	uint32_t h;
	uint8_t ch, ch2;
	yy_sym ret;
	pp_macro *macro;
	const unsigned char *pos;
	const char *text;

	if (rcc->pp_stream) {
		pp_subst_stream *stream = rcc->pp_stream;

restart_stream:
		ret = *stream->tokens++;
		if (ret <= YY_WS) {
			if (ret == YY_WS) {
				if (!(rcc->yy_flags & YY_SKIP_WS)) {
					rcc->yy_text = NULL;
					rcc->yy_len = 0;
					return ret;
				}
				ret = *stream->tokens++;
				//IR_ASSERT(ret != YY_WS);
			}
			if (ret == YY_EOF) {
				if (!stream->skip_eof) {
					stream->tokens--;
					return YY_EOF;
				}
				stream = pp_pop_stream(rcc);
				if (stream) goto restart_stream;
				pos = (const unsigned char*)rcc->yy_pos;
				goto restart;
			} else if (ret <= YY_WS) {
				rcc->yy_text = NULL;
				rcc->yy_len = 0;
				return ret;
			}
		}
		if (ret < YY_ID) {
			stream->tokens = pp_load_val(rcc, stream->tokens);
			if (ret == YY_PP_NUMBER && !(rcc->yy_flags & YY_ACCEPT_PP_NUMBER)) {
				ret = yy_parse_pp_number(rcc, rcc->yy_text, rcc->yy_len);
			}
		} else {
			if (PP_IS_ID(ret)) {
				if (ret & PP_NOSUBST) {
					if (!(rcc->yy_flags & YY_ACCEPT_NOSUBST)) ret &= ~PP_NOSUBST;
				} else {
					macro = rcc->yy_hash.data[ret].macro;
					if (macro) {
						if (!(macro->flags & PP_MACRO_DISABLED)) {
							if (!(rcc->yy_flags & YY_NO_MACRO)) {
try_expand_macro:
								if (pp_macro_expand(rcc, macro, ret)) {
									stream = rcc->pp_stream;
									if (stream) goto restart_stream;
									pos = (const unsigned char*)rcc->yy_pos;
									goto restart;
								}
							}
						} else {
							if (rcc->yy_flags & YY_ACCEPT_NOSUBST) ret |= PP_NOSUBST;
						}
					}
				}
			}
		}
		return ret;
	}

	pos = (const unsigned char*)rcc->yy_pos;
restart:
	text = (const char*)pos;
	ch = *pos;
	switch (ch) {
		case ' ': case '\t': case '\f': case '\v':
			/* white space */
			do {
				ch = *++pos;
			} while (ch == '\t' || ch == '\v' || ch == '\f' || ch == ' ');
			if (rcc->yy_flags & YY_SKIP_WS) goto restart;
			ret = YY_WS;
			goto ret_ws;
		case '\r':
			ch = *++pos;
			if (ch == '\n') pos++;
			goto new_line;
		case '\n':
			pos++;
new_line:
			rcc->yy_line++;
			rcc->yy_linepos = (const char*)pos;
			if (rcc->yy_flags & YY_SKIP_EOL) goto restart;
			ret = YY_EOL;
			goto ret_ws;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
		case 'I': case 'J': case 'K':           case 'M': case 'N': case 'O': case 'P':
		case 'Q': case 'R': case 'S': case 'T':           case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
		case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
		case 'q': case 'r': case 's': case 't':           case 'v': case 'w': case 'x':
		case 'y': case 'z': case '_': case '$':
identifier:
			h = 5381;
identifier2:
			do {
				h = ((h << 5) + h) + ch;
				ch = *++pos;
			} while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$' || ch >= 0x80);

			rcc->pp_include_ifndef_state = 0;
			rcc->yy_pos = (const char*)pos;
			rcc->yy_text = text;
			rcc->yy_len = rcc->yy_pos - rcc->yy_text;
			ret = yy_hash_lookup_ex(rcc, rcc->yy_text, rcc->yy_len, h | 0x10000000);
			if (!(rcc->yy_flags & YY_NO_MACRO)) {
				macro = rcc->yy_hash.data[ret].macro;
				if (macro) {
					IR_ASSERT(!(macro->flags & PP_MACRO_DISABLED));
					goto try_expand_macro;
				}
			}
			return ret;
		case 'u':
			ch2 = *++pos;
			if (ch2 == '"') goto string;
			if (ch2 == '\'') goto character;
			if (ch2 == '8') {
				ch = *++pos;
				if (ch == '"') goto string;
				h = 5381;
				h = ((h << 5) + h) + 'u';
				ch = ch2;
				pos--;
				goto identifier2;
			}
			pos--;
			goto identifier;
		case 'L':
		case 'U':
			ch2 = *++pos;
			if (ch2 == '"') goto string;
			if (ch2 == '\'') goto character;
			pos--;
			goto identifier;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			/* decimal number */
//decimal_mumber:
			do {
				ch = *++pos;
			} while (ch >= '0' && ch <= '9');
			if (ch == '.' || ch == 'E' || ch == 'e') {
float_number:
				if (ch == '.') {
float_mumber_cont:
					ch = *++pos;
					while (ch >= '0' && ch <= '9') {
						ch = *++pos;
					}
				}
				if (ch == 'E' || ch == 'e') {
					ch = *++pos;
					if (ch == '+' || ch == '-') {
						ch = *++pos;
					}
					if (ch < '0' || ch > '9') {
						if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
							goto pp_number;
						}
wrong_number:
						pos++;
						yy_error_fmt("invalid number %.*s", (int)((const char*)pos - text), text);
						goto restart;
					}
					do {
						ch = *++pos;
					} while (ch >= '0' && ch <= '9');
				}
				if (ch == 'F' || ch == 'L' || ch == 'f' || ch == 'l') {
					ch = *++pos;
				}
				ret = YY_FLOATING_NUMBER;
				break;
			}
			ret = YY_DECIMAL_NUMBER;
number_suffix:
			if (ch == 'U' || ch == 'u') {
				ch = *++pos;
				if (ch == 'L') {
					ch = *++pos;
					if (ch == 'L') {
						ch = *++pos;
					}
				} else if (ch == 'l') {
					ch = *++pos;
					if (ch == 'l') {
						ch = *++pos;
					}
				}
			} else if (ch == 'L') {
				ch = *++pos;
				if (ch == 'L') {
					ch = *++pos;
				}
				if (ch == 'U' || ch == 'u') {
					ch = *++pos;
				}
			} else if (ch == 'l') {
				ch = *++pos;
				if (ch == 'l') {
					ch = *++pos;
				}
				if (ch == 'U' || ch == 'u') {
					ch = *++pos;
				}
			}
			if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
				if ((ch >= 'a' && ch <= 'z')
				 || (ch >= 'A' && ch <= 'Z')
				 || (ch >= '0' && ch <= '9')
				 || ch == '_' || ch == '$'
				 || ch == '.'
				 || ((ch == '+' || ch == '-')
					&& (pos[-1] == 'e' || pos[-1] == 'E' || pos[-1] == 'p' || pos[-1] == 'P'))) {
					do {
						ch = *++pos;
pp_number:;
					} while ((ch >= 'a' && ch <= 'z')
						|| (ch >= 'A' && ch <= 'Z')
						|| (ch >= '0' && ch <= '9')
						|| ch == '_' || ch == '$'
						|| ch == '.'
						|| ((ch == '+' || ch == '-')
							&& (pos[-1] == 'e' || pos[-1] == 'E' || pos[-1] == 'p' || pos[-1] == 'P')));
					ret = YY_PP_NUMBER;
				}
			}
			break;
		case '0':
			ch = *++pos;
			if (ch == 'X' || ch == 'x') {
				ch = *++pos;
				if (ch == '.' || ch == 'P' || ch == 'p') goto hex_float;
				if ((ch < '0' || ch > '9') && (ch < 'A' || ch > 'F') && (ch < 'a' || ch > 'f')) {
					if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
						goto pp_number;
					}
					goto wrong_number;
				}
				do {
					ch = *++pos;
				} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
				if (ch == '.' || ch == 'P' || ch == 'p') {
hex_float:
					if (ch == '.') {
						ch = *++pos;
						while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
							ch = *++pos;
						}
					}
					if (ch == 'P' || ch == 'p') {
						ch = *++pos;
						if (ch == '+' || ch == '-') {
							ch = *++pos;
						}
						if (ch < '0' || ch > '9') {
							if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
								goto pp_number;
							}
							goto wrong_number;
						}
						do {
							ch = *++pos;
						} while (ch >= '0' && ch <= '9');
					}
					if (ch == 'F' || ch == 'L' || ch == 'f' || ch == 'l') {
						ch = *++pos;
					}
					ret = YY_HEXADECIMAL_FLOATING_NUMBER;
					break;
				}
				ret = YY_HEXADECIMAL_NUMBER;
				goto number_suffix;
			} else if (ch == 'B' || ch == 'b') {
				ch = *++pos;
				if (ch != '0' && ch != '1') {
					if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
						goto pp_number;
					}
					goto wrong_number;
				}
				do {
					ch = *++pos;
				} while (ch == '0' || ch == '1');
				ret = YY_BINARY_NUMBER;
				goto number_suffix;
			} else {
				/* octal number */
				while (ch >= '0' && ch <= '7') {
					ch = *++pos;
				}
				if (ch == '8' || ch == '9') {
					if (rcc->yy_flags & YY_ACCEPT_PP_NUMBER) {
						goto pp_number;
					}
					goto wrong_number;
				}
				if (ch == '.' || ch == 'e' || ch ==  'E') goto float_number;
				ret = YY_OCTAL_NUMBER;
				goto number_suffix;
			}
		case '\'':
character:
			while (1) {
				ch = *++pos;
				if (ch == '\\') {
					ch = *++pos;
					if (ch == '\r') {
						if (pos[1] == '\n') pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
					} else if (ch == '\n') {
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
					}
				} else if (ch == '\'') {
					break;
				} else if (ch == '\r' || ch == '\n') {
					rcc->yy_pos = (const char*)pos;
					rcc->yy_text = text;
					rcc->yy_len = rcc->yy_pos - rcc->yy_text;
					yy_error("unterminated character");
					goto restart;
				}
			}
			pos++;
			ret = YY_CHARACTER;
			break;
		case '"':
string:
			while (1) {
				ch = *++pos;
				if (ch == '\\') {
					ch = *++pos;
					if (ch == '\r') {
						if (pos[1] == '\n') pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
					} else if (ch == '\n') {
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
					}
				} else if (ch == '"') {
					break;
				} else if (ch == '\r' || ch == '\n') {
					rcc->yy_pos = (const char*)pos;
					rcc->yy_text = text;
					rcc->yy_len = rcc->yy_pos - rcc->yy_text;
					yy_error("unterminated string");
					goto restart;
				}
			}
			pos++;
			ret = YY_STRING;
			break;
		case '(':
			pos++;
			ret = YY__LPAREN;
			break;
		case '*':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__STAR_EQUAL;
			} else {
				ret = YY__STAR;
			}
			break;
		case ')':
			pos++;
			ret = YY__RPAREN;
			break;
		case '[':
			pos++;
			ret = YY__LBRACK;
			break;
		case '.':
			ch = *++pos;
			if (ch >= '0' && ch <= '9') goto float_mumber_cont;
			if (ch == '.') {
				ch = *++pos;
				if (ch == '.') {
					pos++;
					ret = YY__POINT_POINT_POINT;
				} else {
					pos--;
					ret = YY__POINT;
				}
			} else {
				ret = YY__POINT;
			}
			break;
		case ',':
			pos++;
			ret = YY__COMMA;
			break;
		case ':':
			pos++;
			ret = YY__COLON;
			break;
		case ']':
			pos++;
			ret = YY__RBRACK;
			break;
		case '-':
			ch = *++pos;
			if (ch == '>') {
				pos++;
				ret = YY__MINUS_GREATER;
			} else if (ch == '-') {
				pos++;
				ret = YY__MINUS_MINUS;
			} else if (ch == '=') {
				pos++;
				ret = YY__MINUS_EQUAL;
			} else {
				ret = YY__MINUS;
			}
			break;
		case '+':
			ch = *++pos;
			if (ch == '+') {
				pos++;
				ret = YY__PLUS_PLUS;
			} else if (ch == '=') {
				pos++;
				ret = YY__PLUS_EQUAL;
			} else {
				ret = YY__PLUS;
			}
			break;
		case '&':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__AND_EQUAL;
			} else if (ch == '&') {
				pos++;
				ret = YY__AND_AND;
			} else {
				ret = YY__AND;
			}
			break;
		case '~':
			pos++;
			ret = YY__TILDE;
			break;
		case '!':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__BANG_EQUAL;
			} else {
				ret = YY__BANG;
			}
			break;
		case '=':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__EQUAL_EQUAL;
			} else {
				ret = YY__EQUAL;
			}
			break;
		case '/':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__SLASH_EQUAL;
			} else if (ch == '/') {
				/* one line comments */
				while (1) {
					ch = *++pos;
					if (ch == '\r') {
						ch = *++pos;
						if (ch == '\n') pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos;
						break;
					} else if (ch == '\n') {
						pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos;
						break;
					}
				}
				if (rcc->yy_flags & YY_SKIP_COMMENTS) {
					if (rcc->yy_flags & YY_SKIP_EOL) goto restart;
					ret = YY_EOL;
				} else {
					ret = YY_ONE_LINE_COMMENT;
				}
				goto ret_ws;
			} else if (ch == '*') {
				while (1) {
					ch = *++pos;
					if (ch == '*') {
						if (pos[1] == '/') {
							pos += 2;
							break;
						}
					} else if (ch == '\r') {
						if (pos[1] == '\n') pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
						if (pos[1] == '\0') goto error;
					} else if (ch == '\n') {
						if (pos[1] == '\n') pos++;
						rcc->yy_line++;
						rcc->yy_linepos = (const char*)pos + 1;
						if (pos[1] == '\0') goto error;
					}
				}
				if (rcc->yy_flags & YY_SKIP_COMMENTS) goto restart;
				ret = YY_COMMENT;
				goto ret_ws;
			} else {
				ret = YY__SLASH;
			}
			break;
		case '%':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__PERCENT_EQUAL;
			} else {
				ret = YY__PERCENT;
			}
			break;
		case '<':
			ch = *++pos;
			if (ch == '<') {
				ch = *++pos;
				if (ch == '=') {
					pos++;
					ret = YY__LESS_LESS_EQUAL;
				} else {
					ret = YY__LESS_LESS;
				}
			} else if (ch == '=') {
				pos++;
				ret = YY__LESS_EQUAL;
			} else {
				ret = YY__LESS;
			}
			break;
		case '>':
			ch = *++pos;
			if (ch == '>') {
				ch = *++pos;
				if (ch == '=') {
					pos++;
					ret = YY__GREATER_GREATER_EQUAL;
				} else {
					ret = YY__GREATER_GREATER;
				}
			} else if (ch == '=') {
				pos++;
				ret = YY__GREATER_EQUAL;
			} else {
				ret = YY__GREATER;
			}
			break;
		case '^':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__UPARROW_EQUAL;
			} else {
				ret = YY__UPARROW;
			}
			break;
		case '|':
			ch = *++pos;
			if (ch == '=') {
				pos++;
				ret = YY__BAR_EQUAL;
			} else if (ch == '|') {
				pos++;
				ret = YY__BAR_BAR;
			} else {
				ret = YY__BAR;
			}
			break;
		case '?':
			pos++;
			ret = YY__QUERY;
			break;
		case '{':
			pos++;
			ret = YY__LBRACE;
			break;
		case ';':
			pos++;
			ret = YY__SEMICOLON;
			break;
		case '}':
			pos++;
			ret = YY__RBRACE;
			break;
		case '#':
			ch = *++pos;
			if (ch == '#') {
				pos++;
				ret = YY__HASH_HASH;
			} else if (!(rcc->yy_flags & YY_NO_DIRECTIVE)
					&& (text == rcc->yy_linepos || yy_at_start_of_line(rcc, text))) {
				rcc->yy_pos = (const char*)pos;
				rcc->yy_text = text;
				rcc->yy_len = rcc->yy_pos - rcc->yy_text;
				pp_parse_directive(rcc);
				pos = (const unsigned char*)rcc->yy_pos;
				IR_ASSERT(!rcc->pp_stream);
				goto restart;
			} else {
				ret = YY__HASH;
			}
			break;
		case '\0':
			if ((const char*)pos < rcc->yy_end) goto error;
			if (rcc->pp_include_level != 0) {
				pp_pop_include(rcc);
				pos = (const unsigned char*)rcc->yy_pos;
				if (rcc->yy_flags & YY_SKIP_EOL) goto restart;
				/* insert EOL if EOF wasn't at new line */
				rcc->yy_text = (const char*)pos;
				ret = YY_EOL;
				break;
			}
			ret = YY_EOF;
			break;
		case '\\':
			if (pos[1] == '\r') {
				pos += 2;
				if (*pos == '\n') pos++;
				rcc->yy_line++;
				rcc->yy_linepos = (const char*)pos;
				if (rcc->yy_flags & YY_SKIP_WS) goto restart;
				ret = YY_WS;
				goto ret_ws;
			} else if (pos[1] == '\n') {
				pos += 2;
				rcc->yy_line++;
				rcc->yy_linepos = (const char*)pos;
				if (rcc->yy_flags & YY_SKIP_WS) goto restart;
				ret = YY_WS;
				goto ret_ws;
			}
			goto error;
		default:
			if (ch >= 0x80) goto identifier;
			if (rcc->yy_flags & YY_ACCEPT_PUNCTUATOR) {
				ret = YY_PP_PUNCTUATOR;
				pos++;
				break;
			}
error:
			if ((rcc->yy_flags & YY_ACCEPT_PUNCTUATOR) && (const char*)pos == text) {
				ret = YY_PP_PUNCTUATOR;
				pos++;
				break;
			}
			rcc->yy_pos = (const char*)pos;
			rcc->yy_text = text;
			rcc->yy_len = rcc->yy_pos - rcc->yy_text;
			yy_scanner_error(rcc);
			pos++;
			goto restart;
	}

	rcc->pp_include_ifndef_state = 0;
ret_ws:
	rcc->yy_pos = (const char*)pos;
	rcc->yy_text = text;
	rcc->yy_len = rcc->yy_pos - rcc->yy_text;
	return ret;
}

/* Scanner Helpers */
static IR_NEVER_INLINE bool yy_at_start_of_line(rcc_ctx *rcc, const char *text)
{
	if (text == rcc->yy_linepos) {
		return 1;
	} else if (text > rcc->yy_linepos) {
		const char *p = rcc->yy_linepos;
		do {
			if (*p != ' ' && *p != '\t' && *p != '\v' && *p != '\f') {
				return 0;
			}
		} while (++p != text);
		return 1;
	} else {
		/*something wrong */
		assert(0);
		return 0;
	}
}

static IR_NEVER_INLINE yy_sym yy_parse_pp_number(rcc_ctx *rcc, const char *str, size_t len)
{
	const char *p = str;
	const char *end = p + len;
	char ch = *p;
	yy_sym ret = 0;

	IR_ASSERT(len > 0);
	IR_ASSERT((ch >= '0' && ch <= '9') || ch == '.');
	if (len == 1) return YY_DECIMAL_NUMBER;
	if (ch == '0') {
		ch = *++p;
		if (ch == 'x' || ch == 'X') {
			do {
				ch = *++p;
			} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
			ret = YY_HEXADECIMAL_NUMBER;
			if (ch == '.') {
				ret = YY_HEXADECIMAL_FLOATING_NUMBER;
				do {
					ch = *++p;
				} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
			}
			if (ch == 'P' || ch == 'p') {
				ret = YY_HEXADECIMAL_FLOATING_NUMBER;
				ch = *++p;
				if (ch == '+' || ch == '-') ch = *++p;
				while (ch >= '0' && ch <= '9') {
					ch = *++p;
				}
			}
		} else if (ch == 'b' || ch == 'B') {
			do {
				ch = *++p;
			} while (ch == '0' || ch == '1');
			ret = YY_BINARY_NUMBER;
		} else {
			while (ch >= '0' && ch <= '7') {
				ch = *++p;
			}
			ret = YY_OCTAL_NUMBER;
			goto float_number;
		}
	} else if (ch == '.') {
		goto float_number;
	} else {
		while (ch >= '0' && ch <= '9') {
			ch = *++p;
		}
		ret = YY_DECIMAL_NUMBER;
float_number:
		if (ch == '.') {
			ret = YY_FLOATING_NUMBER;
			do {
				ch = *++p;
			} while (ch >= '0' && ch <= '9');
		}
		if (ch == 'E' || ch == 'e') {
			ret = YY_FLOATING_NUMBER;
			ch = *++p;
			if (ch == '+' || ch == '-') ch = *++p;
			while (ch >= '0' && ch <= '9') {
				ch = *++p;
			}
		}
	}

	if (ret == YY_FLOATING_NUMBER || ret == YY_HEXADECIMAL_FLOATING_NUMBER) {
		if (ch == 'f' || ch == 'F' || ch == 'l' || ch == 'L') ch = *++p;
	} else if (ch == 'U' || ch == 'u') {
		ch = *++p;
		if (ch == 'L' || ch == 'l') {
			ch = *++p;
			if (ch == 'L' || ch == 'l') ch = *++p;
		}
	} else if (ch == 'L' || ch == 'l') {
		ch = *++p;
		if (ch == 'L' || ch == 'l') ch = *++p;
		if (ch == 'U' || ch == 'u') ch = *++p;
	}

	if (!ret || p != end) {
		yy_error_fmt("invalid number %.*s", (int)len, str);
	}

	return ret;
}

/* Error Reporting */
static size_t yy_escape(char *buf, int ch)
{
	switch (ch) {
		case '\\': buf[0] = '\\'; buf[1] = '\\'; return 2;
		case '\'': buf[0] = '\\'; buf[1] = '\''; return 2;
		case '\"': buf[0] = '\\'; buf[1] = '\"'; return 2;
		case '\a': buf[0] = '\\'; buf[1] = '\a'; return 2;
		case '\b': buf[0] = '\\'; buf[1] = '\b'; return 2;
		case 27:   buf[0] = '\\'; buf[1] = 27; return 2;
		case '\f': buf[0] = '\\'; buf[1] = '\f'; return 2;
		case '\n': buf[0] = '\\'; buf[1] = '\n'; return 2;
		case '\r': buf[0] = '\\'; buf[1] = '\r'; return 2;
		case '\t': buf[0] = '\\'; buf[1] = '\t'; return 2;
		case '\v': buf[0] = '\\'; buf[1] = '\v'; return 2;
		case '\?': buf[0] = '\\'; buf[1] = 0x3f; return 2;
		default: break;
	}
	if (ch < 32 || ch >= 127) {
		buf[0] = '\\';
		buf[1] = '0' + ((ch >> 6) % 8);
		buf[2] = '0' + ((ch >> 3) % 8);
		buf[3] = '0' + (ch % 8);
		return 4;
	} else {
		buf[0] = ch;
		return 1;
	}
}

static const char *yy_escape_char(char *buf, int ch)
{
	size_t len = yy_escape(buf, ch);
	buf[len] = 0;
	return buf;
}

static const char *yy_escape_string(char *buf, size_t size, const unsigned char *str, size_t n)
{
	size_t i = 0;
	size_t pos = 0;
	size_t len;

	while (i < n) {
		if (pos + 8 > size) {
			buf[pos++] = '.';
			buf[pos++] = '.';
			buf[pos++] = '.';
			break;
		}
		len = yy_escape(buf + pos, str[i]);
		i++;
		pos += len;
	}
	buf[pos] = 0;
	return buf;
}

static void yy_error_line(rcc_ctx *rcc)
{
	size_t line_len, pos, i;
	const char *s;

	s = strpbrk(rcc->yy_linepos, "\r\n");
	if (s) {
		line_len = s - rcc->yy_linepos;
	} else {
		line_len = strlen(rcc->yy_linepos);
	}
	fprintf(stderr, "%5d |%.*s\n", rcc->yy_line, (int)line_len, rcc->yy_linepos);
	pos = rcc->yy_text - rcc->yy_linepos;
	if (pos <= line_len) {
		fprintf(stderr, "      |");
		for (i = 0; i < pos; i++) {
			if (rcc->yy_linepos[i] == '\t') {
				fputc('\t', stderr);
			} else {
				fputc(' ', stderr);
			}
		}
		fprintf(stderr, "^\n");
	}
}

static void yy_error_pos(rcc_ctx *rcc)
{
	fflush(stdout);
#ifdef _WIN32
	/* Convert back-slashes to slashes to make identical tests ouput */
	// TODO: find a better solution ???

	const char *s = yy_sym2str(rcc, rcc->yy_file_name);

	while (*s) {
		if (*s == '\\') {
			fputc('/', stderr);
		} else {
			fputc(*s, stderr);
		}
		s++;
	}
	if (rcc->yy_text >= rcc->yy_linepos && rcc->yy_text <= rcc->yy_pos) {
		fprintf(stderr, ":%d:%d: ",
			rcc->yy_line, (int)(rcc->yy_text - rcc->yy_linepos + 1));
	} else {
		fprintf(stderr, ":%d: ", rcc->yy_line);
	}
#else
	if (rcc->yy_text >= rcc->yy_linepos && rcc->yy_text <= rcc->yy_pos) {
		fprintf(stderr, "%s:%d:%d: ", yy_sym2str(rcc, rcc->yy_file_name),
			rcc->yy_line, (int)(rcc->yy_text - rcc->yy_linepos + 1));
	} else {
		fprintf(stderr, "%s:%d: ", yy_sym2str(rcc, rcc->yy_file_name), rcc->yy_line);
	}
#endif
}

void yy_error_(rcc_ctx *rcc, const char *msg)
{
	yy_error_pos(rcc);
	fprintf(stderr, "error: %s\n", msg);
	if (0) yy_error_line(rcc);
	exit(1);
}

void yy_error_fmt_(rcc_ctx *rcc, const char *fmt, ...)
{
	va_list args;

	yy_error_pos(rcc);
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (0) yy_error_line(rcc);
	exit(1);
}

void yy_warning_(rcc_ctx *rcc, uint32_t kind, const char *msg)
{
	if (!((rcc->e_errors | rcc->e_warnings) & kind)) return;
	yy_error_pos(rcc);
	if (rcc->e_errors & kind) {
		fprintf(stderr, "error: %s\n", msg);
	} else {
		fprintf(stderr, "warning: %s\n", msg);
	}
	if (0) yy_error_line(rcc);
	if (rcc->e_errors & kind) {
		exit(1);
	}
}

void yy_warning_fmt_(rcc_ctx *rcc, uint32_t kind, const char *fmt, ...)
{
	va_list args;

	if (!((rcc->e_errors | rcc->e_warnings) & kind)) return;
	yy_error_pos(rcc);
	va_start(args, fmt);
	if (rcc->e_errors & kind) {
		fprintf(stderr, "error: ");
	} else {
		fprintf(stderr, "warning: ");
	}
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (0) yy_error_line(rcc);
	if (rcc->e_errors & kind) {
		exit(1);
	}
}

static IR_NEVER_INLINE void yy_scanner_error(rcc_ctx *rcc)
{
	char buf[64];

	if (rcc->yy_pos >= rcc->yy_end) {
		yy_error("unexpected <EOF>");
	} else if (rcc->yy_pos == rcc->yy_text) {
		yy_error_fmt("unexpected character '%s'",  yy_escape_char(buf, rcc->yy_text[0]));
	} else {
		yy_error_fmt("unexpected sequence '%s'",
			yy_escape_string(buf, sizeof(buf), (const unsigned char*)rcc->yy_text, rcc->yy_len + 1));
	}
}
