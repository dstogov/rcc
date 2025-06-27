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

const char *yy_pos;            /* pointer to current scanned character          */
const char *yy_text;           /* pointer to start of the current scanned token */
const char *yy_linepos;        /* pointer to start of the current scanned line  */
size_t      yy_len;            /* length of the value of terminal token */
int32_t     yy_line;           /* line number */
yy_sym      yy_file_name;      /* interned file name */
const char *yy_buf;
const char *yy_end;

yy_hashtab  yy_hash;
ir_arena   *yy_arena;
uint32_t    yy_flags;

/* Scanner Hash Table */
static uint32_t yy_str_hash(const char *str, size_t len)
{
	size_t i;
	uint32_t h = 5381;

    for (i = 0; i < len; i++) {
        h = ((h << 5) + h) + *str;
        str++;
    }
    return h | 0x10000000;
}

void yy_hash_init(void)
{
	char *data = ir_mem_malloc(1024 * sizeof(uint32_t) + 1024 * sizeof(yy_hash_bucket));
	memset(data, 0, 1024 * sizeof(uint32_t));
	yy_hash.data = (yy_hash_bucket*)(data + (1024 * sizeof(uint32_t)));
	yy_hash.count = 0;
	yy_hash.size = 1024;
	yy_hash.mask = (uint32_t)(-(int32_t)1024);
}

void yy_hash_free(void)
{
	ir_mem_free((char*)yy_hash.data - (yy_hash.size * sizeof(uint32_t)));
}

static IR_NEVER_INLINE void yy_hash_resize(void)
{
	uint32_t old_size = (uint32_t)(-(int32_t)yy_hash.mask);
	yy_hash_bucket *old_data = yy_hash.data;
	uint32_t new_size = old_size * 2;
	yy_hash_bucket *data = ir_mem_malloc(new_size * sizeof(uint32_t) + new_size * sizeof(yy_hash_bucket));
	yy_hash_bucket *p;
	uint32_t i, mask;

	memset(data, 0, new_size * sizeof(uint32_t));
	yy_hash.data = data = (yy_hash_bucket*)((char*)data + (new_size * sizeof(uint32_t)));
	yy_hash.mask = (uint32_t)(-(int32_t)new_size);
	yy_hash.size = new_size;

	memcpy(yy_hash.data, old_data, yy_hash.count * sizeof(yy_hash_bucket));
	ir_mem_free((char*)old_data - (old_size * sizeof(uint32_t)));

	mask = yy_hash.mask;
	for (i = YY_FIRST_KEYWORD, p = yy_hash.data + i; i < yy_hash.count; i++) {
		uint32_t h = p->h | mask;
		p->next = ((uint32_t*)data)[(int32_t)h];
		((uint32_t*)data)[(int32_t)h] = i;
		p++;
	} while (--i);
}

yy_sym yy_hash_find(const char *str, size_t len)
{
	uint32_t h = yy_str_hash(str, len);
	yy_hash_bucket *data = yy_hash.data;
	uint32_t pos = ((uint32_t*)data)[(int32_t)(h | yy_hash.mask)];
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

static yy_sym yy_hash_lookup_ex(const char *str, size_t len, uint32_t h)
{
	yy_hash_bucket *data = yy_hash.data;
	uint32_t pos = ((uint32_t*)data)[(int32_t)(h | yy_hash.mask)];
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

	if (UNEXPECTED(yy_hash.count == yy_hash.size)) {
		yy_hash_resize();
		data = yy_hash.data;
	}

	char *holder = ir_arena_alloc(&yy_arena, len + 1);
	memcpy(holder, str, len);
	holder[len] = 0;
	str = holder;

	pos = yy_hash.count++;
	p = data + pos;
	p->h = h;
	p->len = len;
	p->str = str;
	h |= yy_hash.mask;
	p->next = ((uint32_t*)data)[(int32_t)h];
	((uint32_t*)data)[(int32_t)h] = pos;
	p->macro = NULL;
	p->sym = NULL;
	p->tag = NULL;

	return pos;
}

yy_sym yy_hash_lookup(const char *str, size_t len)
{
	return yy_hash_lookup_ex(str, len, yy_str_hash(str, len));
}

/* Scanner */
static void yy_scanner_error(void) yy_noreturn;
static bool yy_at_start_of_line(void);
static yy_sym yy_parse_pp_number(const char *str, size_t len);

yy_sym yy_next(void)
{
	uint32_t h;
	int ch, ch2;
	yy_sym ret;
	pp_macro *macro;
	const unsigned char *pos;

restart_stream:
	if (pp_subst_level > 0) {
		pp_subst_stream *stream = &pp_subst_stack[pp_subst_level - 1];

		ret = *stream->tokens++;
		if ((ret == YY_WS || ret == YY_PP_PLACE_MARKER) && (yy_flags & YY_SKIP_WS)) {
			do {
				ret = *stream->tokens++;
			} while (ret == YY_WS || ret == YY_PP_PLACE_MARKER);
		}
		if (ret == YY_EOF) {
			if (!stream->skip_eof) {
				stream->tokens--;
				return YY_EOF;
			}
			if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
			if (stream->start) pp_list_release(stream->start, stream->size);
			pp_subst_level--;
			goto restart_stream;
		} else if (ret <= YY_WS) {
			yy_text = NULL;
			yy_len = 0;
		} else if (PP_HAS_VAL(ret)) {
			stream->tokens = pp_load_val(stream->tokens);
			if (ret == YY_PP_NUMBER && !(yy_flags & YY_ACCEPT_PP_NUMBER)) {
				ret = yy_parse_pp_number(yy_text, yy_len);
			}
		} else {
			if (PP_IS_ID(ret)) {
				if (ret & PP_NOSUBST) {
					if (!(yy_flags & YY_ACCEPT_NOSUBST)) ret &= ~PP_NOSUBST;
				} else if (!(yy_flags & YY_NO_MACRO)) {
					macro = yy_hash.data[ret].macro;
					if (macro) {
						if (!(macro->flags & PP_MACRO_DISABLED)) {
try_expand_macro:
							if (pp_macro_expand(macro, ret)) goto restart_stream;
						} else {
//???							if (pp_debug) {pp_debug_fprintf(stderr, "\"%s\" is disabled!\n", yy_sym2str(ret));}
							if (yy_flags & YY_ACCEPT_NOSUBST) ret |= PP_NOSUBST;
						}
					}
				}
			}
			//TODO: this is necessary only for preprocessor (-E)
			//yy_text = yy_sym2strl(ret & ~PP_NOSUBST, &yy_len);
		}
		return ret;
	}

	pos = (const unsigned char*)yy_pos;
restart:
	yy_text = (const char*)pos;
	ch = *pos;
	switch (ch) {
		case ' ': case '\t': case '\f': case '\v':
			/* white space */
			do {
				ch = *++pos;
			} while (ch == '\t' || ch == '\v' || ch == '\f' || ch == ' ');
			if (yy_flags & YY_SKIP_WS) goto restart;
			ret = YY_WS;
			goto ret_ws;
		case '\r':
			ch = *++pos;
			if (ch == '\n') pos++;
			goto new_line;
		case '\n':
			pos++;
new_line:
			yy_line++;
			yy_linepos = (const char*)pos;
			if (yy_flags & YY_SKIP_EOL) goto restart;
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

			yy_pos = (const char*)pos;
			yy_len = yy_pos - yy_text;
			ret = yy_hash_lookup_ex(yy_text, yy_len, h | 0x10000000);
			if (!(yy_flags & YY_NO_MACRO)) {
				macro = yy_hash.data[ret].macro;
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
					if (ch < '0' || ch > '9') goto error;
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
			if (yy_flags & YY_ACCEPT_PP_NUMBER) {
				if ((ch >= 'a' && ch <= 'z')
				 || (ch >= 'A' && ch <= 'Z')
				 || (ch >= '0' && ch <= '9')
				 || ch == '_' || ch == '$') {
					do {
						ch = *++pos;
					} while ((ch >= 'a' && ch <= 'z')
						|| (ch >= 'A' && ch <= 'Z')
						|| (ch >= '0' && ch <= '9')
						|| ch == '_' || ch == '$');
					ret = YY_PP_NUMBER;
				}
			}
			break;
		case '0':
			ch = *++pos;
			if (ch == 'X' || ch == 'x') {
				ch = *++pos;
				if ((ch < '0' || ch > '9') && (ch < 'A' || ch > 'F') && (ch < 'a' || ch > 'f')) goto error;
				do {
					ch = *++pos;
				} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
				if (ch == '.' || ch == 'P' || ch == 'p') {
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
						if (ch < '0' || ch > '9') goto error;
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
			} else {
				/* octal number */
				while (ch >= '0' && ch <= '7') {
					ch = *++pos;
				}
//				if (ch == '8' || ch == '9') goto decimal_mumber;
				if (ch == '8' || ch == '9') {
					do {
						ch = *++pos;
					} while (ch >= '0' && ch <= '9');
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
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\n') {
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\0') {
						goto error;
					}
				} else if (ch == '\'') {
					break;
				} else if (ch == '\r' || ch == '\n' || ch == '\0') {
					goto error;
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
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\n') {
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\0') {
						goto error;
					}
				} else if (ch == '"') {
					break;
				} else if (ch == '\r' || ch == '\n' || ch == '\0') {
					goto error;
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
				if (ch != '.') goto error;
				pos++;
				ret = YY__POINT_POINT_POINT;
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
						yy_line++;
						yy_linepos = (const char*)pos;
						break;
					} else if (ch == '\n') {
						pos++;
						yy_line++;
						yy_linepos = (const char*)pos;
						break;
					} else if (ch == '\0') {
						break;
					}
				}
				if (yy_flags & YY_SKIP_COMMENTS) {
					if (yy_flags & YY_SKIP_EOL) goto restart;
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
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\n') {
						yy_line++;
						yy_linepos = (const char*)pos + 1;
					} else if (ch == '\0') {
						goto error;
					}
				}
				if (yy_flags & YY_SKIP_COMMENTS) goto restart;
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
			} else if ((yy_flags & YY_PREPROCESS) && (yy_text == yy_linepos || yy_at_start_of_line())) {
				yy_pos = (const char*)pos;
				yy_len = yy_pos - yy_text;
				pp_parse_directive();
				pos = (const unsigned char*)yy_pos;
				goto restart;
			} else {
				ret = YY__HASH;
			}
			break;
		case '\0':
			if ((const char*)pos < yy_end) goto error;
			if (pp_include_level != 0) {
				pp_pop_include();
				pos = (const unsigned char*)yy_pos;
				goto restart;
			}
			ret = YY_EOF;
			break;
		case '\\':
			if (pos[1] == '\r') {
				pos += 2;
				if (*pos == '\n') pos++;
				yy_line++;
				yy_linepos = (const char*)pos;
				if (yy_flags & YY_SKIP_WS) goto restart;
				ret = YY_WS;
				goto ret_ws;
			} else if (pos[1] == '\n') {
				pos += 2;
				yy_line++;
				yy_linepos = (const char*)pos;
				if (yy_flags & YY_SKIP_WS) goto restart;
				ret = YY_WS;
				goto ret_ws;
			}
			goto error;
		default:
			if (ch >= 0x80) goto identifier;
			if (yy_flags & YY_ACCEPT_PUNCTUATOR) {
				ret = YY_PP_PUNCTUATOR;
				pos++;
				break;
			}
error:
			if ((yy_flags & YY_ACCEPT_PUNCTUATOR) && (const char*)pos == yy_text) {
				ret = YY_PP_PUNCTUATOR;
				pos++;
				break;
			}
			yy_pos = (const char*)pos;
			yy_len = yy_pos - yy_text;
			yy_scanner_error();
			pos++;
			goto restart;
	}

	pp_include_ifndef_state = 0;
ret_ws:
	yy_pos = (const char*)pos;
	yy_len = yy_pos - yy_text;
	return ret;
}

/* Scanner Helpers */
static IR_NEVER_INLINE bool yy_at_start_of_line(void)
{
	if (yy_text == yy_linepos) {
		return 1;
	} else if (yy_text > yy_linepos) {
		const char *p = yy_linepos;
		do {
			if (*p != ' ' && *p != '\t' && *p != '\v' && *p != '\f') {
				return 0;
			}
		} while (++p != yy_text);
		return 1;
	} else {
		/*something wrong */
		assert(0);
		return 0;
	}
}

static IR_NEVER_INLINE yy_sym yy_parse_pp_number(const char *str, size_t len)
{
	const char *p = str;
	const char *end = p + len;
	char ch = *p;
	yy_sym ret = 0;

	IR_ASSERT(len > 0);
	IR_ASSERT(ch >= '0' && ch <= '9');
	if (len == 1) return YY_DECIMAL_NUMBER;
	if (ch == '0') {
		ch = *++p;
		if (ch == 'x' || ch == 'X') {
			do {
				ch = *++p;
			} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
			ret = YY_HEXADECIMAL_NUMBER;
			if (ch == '.') {
				ret = YY_HEXADECIMAL_FLOATING_NUMBER;
				do {
					ch = *++p;
				} while ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
			}
			if (ch == 'P' || ch == 'p') {
				ret = YY_HEXADECIMAL_FLOATING_NUMBER;
				ch = *++p;
				if (ch == '+' || ch == '-') ch = *++p;
				while (ch >= '0' && ch <= '9') {
					ch = *++p;
				}
			}
		} else {
			while (ch >= '0' && ch <= '7') {
				ch = *++p;
			}
			ret = YY_OCTAL_NUMBER;
		}
	} else {
		while (ch >= '0' && ch <= '9') {
			ch = *++p;
		}
		ret = YY_DECIMAL_NUMBER;
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
		yy_error("invalid number");
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

static void yy_error_line(void)
{
	size_t line_len, pos, i;
	const char *s;

	s = strpbrk(yy_linepos, "\r\n");
	if (s) {
		line_len = s - yy_linepos;
	} else {
		line_len = strlen(yy_linepos);
	}
	fprintf(stderr, "%5d |%.*s\n", yy_line, (int)line_len, yy_linepos);
	pos = yy_text - yy_linepos;
	if (pos <= line_len) {
		fprintf(stderr, "      |");
		for (i = 0; i < pos; i++) {
			if (yy_linepos[i] == '\t') {
				fputc('\t', stderr);
			} else {
				fputc(' ', stderr);
			}
		}
		fprintf(stderr, "^\n");
	}
}

static void yy_error_pos(void)
{
	fflush(stdout);
	if (yy_text >= yy_linepos && yy_text <= yy_pos) {
		fprintf(stderr, "%s:%d:%d: ", yy_sym2str(yy_file_name), yy_line, (int)(yy_text - yy_linepos + 1));
	} else {
		fprintf(stderr, "%s:%d: ", yy_sym2str(yy_file_name), yy_line);
	}
}

void yy_error(const char *msg)
{
	yy_error_pos();
	fprintf(stderr, "error: %s\n", msg);
	if (0) yy_error_line();
	exit(1);
}

void yy_error_fmt(const char *fmt, ...)
{
	va_list args;

	yy_error_pos();
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (0) yy_error_line();
	exit(1);
}

void yy_warning(const char *msg)
{
	if (yy_flags & YY_NO_WARNINGS) return;
	yy_error_pos();
	fprintf(stderr, "warning: %s\n", msg);
	if (0) yy_error_line();
}


void yy_warning_fmt(const char *fmt, ...)
{
	va_list args;

	if (yy_flags & YY_NO_WARNINGS) return;
	yy_error_pos();
	va_start(args, fmt);
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (0) yy_error_line();
}

static IR_NEVER_INLINE void yy_scanner_error(void)
{
	char buf[64];

	if (yy_pos >= yy_end) {
		yy_error("unexpected <EOF>");
	} else if (yy_pos == yy_text) {
		yy_error_fmt("unexpected character '%s'",  yy_escape_char(buf, yy_text[0]));
	} else {
		yy_error_fmt("unexpected sequence '%s'", yy_escape_string(buf, sizeof(buf), (const unsigned char*)yy_text, yy_len + 1));
	}
}
