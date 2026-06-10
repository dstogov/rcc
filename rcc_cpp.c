/*
 * RCC - Rational C Compiler
 * (C preprocessor)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
# include <unistd.h>
#else
# include <windows.h>
# include <io.h>
# define open _open
# define read _read
# define close _close
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#include <ir.h>
#include <ir_private.h>

#include "rcc.h"

#ifndef PP_DEBUG
# define PP_DEBUG 0
#endif

/* pp_ifdef_stack bits */
#define IFDEF_HAD_TRUE       (1<<0)
#define IFDEF_HAD_ELSE       (1<<1)

/* pp_include_ifndef_state and pp_include_state.state bits */
#define YY_INCLUDE_START     (1<<0)
#define YY_INCLUDE_END       (1<<1)

#define YY_PRAGMA_ONCE       1

typedef pp_macro pp_arg;

#if PP_DEBUG
# define pp_debug 1
static void pp_debug_print_context(rcc_ctx *rcc);
static void pp_debug_print_args(rcc_ctx *rcc, pp_macro *macro, pp_arg *args);
static void pp_debug_print_list(rcc_ctx *rcc, const char *hdr, yy_sym *tokens);
# define pp_debug_fprintf(file, format, ...) fprintf(file, format)
#else
# define pp_debug 0
# define pp_inc_recursion_level(rcc)
# define pp_dec_recursion_level(rcc)
# define pp_debug_print_context(rcc)
# define pp_debug_print_args(rcc, macro, args)
# define pp_debug_print_list(rcc, hdr, tokens)
# define pp_debug_fprintf(file, format, ...)
#endif

#define PP_ASSERT(condition, message) \
	IR_ASSERT((condition) && message)

#ifndef IS_ABSPATH
# ifdef _WIN32
#  define IS_DIRSEP(c) (c == '/' || c == '\\')
#  define IS_ABSPATH(p) (IS_DIRSEP(p[0]) || (p[0] != 0 && p[1] == ':' && IS_DIRSEP(p[2])))
# else
#  define IS_DIRSEP(c) (c == '/')
#  define IS_ABSPATH(p) IS_DIRSEP(p[0])
# endif
#endif

#ifndef MAXPATHLEN
# if defined(PATH_MAX)
#  define MAXPATHLEN PATH_MAX
# elif defined(MAX_PATH)
#  define MAXPATHLEN MAX_PATH
# else
#  define MAXPATHLEN 256
# endif
#endif

#ifdef _WIN32
# define realpath(file, buf) _fullpath(buf, file, MAXPATHLEN)
#endif

bool pp_add_include_dir(rcc_ctx *rcc, const char *path)
{
	if (rcc->pp_include_paths_count >= PP_MAX_INCLUDE_PATHS) return 0;
	rcc->pp_include_paths[rcc->pp_include_paths_count++] = path;
	return 1;
}

void pp_add_sys_include_dirs(rcc_ctx *rcc)
{
#ifndef _WIN32
	pp_add_include_dir(rcc, "/usr/local/include");
	pp_add_include_dir(rcc, "/usr/include");
#endif
#ifdef __linux__
# if defined(IR_TARGET_X64)
	pp_add_include_dir(rcc, "/usr/include/x86_64-linux-gnu");
# elif defined(IR_TARGET_X86)
	pp_add_include_dir(rcc, "/usr/include/x86-linux-gnu");
# elif defined(IR_TARGET_AARCH64)
	pp_add_include_dir(rcc, "/usr/include/aarch64-linux-gnu");
# endif
#endif
}

void pp_start(rcc_ctx *rcc)
{
	rcc->pp_recursion_level = 0;
	rcc->pp_counter = 0;
	rcc->pp_ifdef_level = 0;
	rcc->pp_include_level = 0;
	rcc->pp_include_ifdef_level = 0;
	rcc->pp_include_ifndef_state = 0; /* don't set YY_INCLUDE_START -> don't detect "#ifndef X" in the main file */
	rcc->pp_include_ifndef_macro = 0;
	rcc->pp_include_hash = NULL;
	rcc->pp_stream = NULL;
	rcc->pp_pack = 0;
	rcc->pp_pack_stack_pos = 0;
	rcc->pp_last_search_dir = 0;
	rcc->pp_next_search_dir = 0;

	rcc->pp_out_file_name = 0;
	rcc->pp_out_level = 0;
	rcc->pp_out_line = 0;
	rcc->pp_out_file = NULL;
}

void pp_dtor(rcc_ctx *rcc)
{
	if (rcc->pp_include_hash) {
		ir_hashtab_free(rcc->pp_include_hash);
		ir_mem_free(rcc->pp_include_hash);
		rcc->pp_include_hash = NULL;
	}
}

static bool pp_macro_is_defined(rcc_ctx *rcc, yy_sym id)
{
	return rcc->yy_hash.data[id].macro != NULL;
}

/* Dynamic Strings */
void yy_dyn_str_init(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len)
{
	dyn_str->str = ir_arena_alloc(&rcc->yy_arena, len);
	rcc->yy_arena->ptr = dyn_str->str + len;
	dyn_str->len = len;
	memcpy(dyn_str->str, str, len);
}

void yy_dyn_str_init0(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len)
{
	dyn_str->str = ir_arena_alloc(&rcc->yy_arena, len + 1);
	rcc->yy_arena->ptr = dyn_str->str + len + 1;
	dyn_str->len = len;
	memcpy(dyn_str->str, str, len);
	dyn_str->str[len] = 0;
}

char *yy_dyn_str_grow(rcc_ctx *rcc, yy_dyn_str *dyn_str, size_t len)
{
	IR_ASSERT(rcc->yy_arena && dyn_str->str + dyn_str->len == rcc->yy_arena->ptr);
	if (len >= (size_t)(rcc->yy_arena->end - rcc->yy_arena->ptr)) {
		size_t size = dyn_str->len + len < 4096 - IR_ALIGNED_SIZE(sizeof(ir_arena), 8) ?
			4096 : IR_ALIGNED_SIZE(dyn_str->len + len + IR_ALIGNED_SIZE(sizeof(ir_arena), 8), 4096);
		if (dyn_str->str == (char*)rcc->yy_arena + IR_ALIGNED_SIZE(sizeof(ir_arena), 8)) {
			rcc->yy_arena = ir_mem_realloc(rcc->yy_arena, size);
			dyn_str->str = (char*)rcc->yy_arena + IR_ALIGNED_SIZE(sizeof(ir_arena), 8);
			rcc->yy_arena->ptr = dyn_str->str + dyn_str->len;
			rcc->yy_arena->end = (char*)rcc->yy_arena + size;
		} else {
			rcc->yy_arena->ptr -= dyn_str->len;
			char *new_str = ir_arena_alloc(&rcc->yy_arena, size);
			rcc->yy_arena->ptr = new_str + dyn_str->len;
			memcpy(new_str, dyn_str->str, dyn_str->len);
			dyn_str->str = new_str;
		}
	}

	char *tail = rcc->yy_arena->ptr;
	rcc->yy_arena->ptr += len;
	return tail;
}

void yy_dyn_str_append(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len)
{
	char *tail = yy_dyn_str_grow(rcc, dyn_str, len);
	memcpy(tail, str, len);
	dyn_str->len += len;
}

void yy_dyn_str_append0(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len)
{
	yy_dyn_str_append(rcc, dyn_str, str, len + 1);
	dyn_str->str[--dyn_str->len] = '\0';
}

/* Lists */
void pp_list_grow(pp_list *l, uint32_t size)
{
	IR_ASSERT(size > l->size);
	if (size >= 256) {
		size = IR_ALIGNED_SIZE(size, 256);
	} else {
		/* Use big enough power of 2 */
		size -= 1;
		size |= (size >> 1);
		size |= (size >> 2);
		size |= (size >> 4);
//		size |= (size >> 8);
//		size |= (size >> 16);
		size += 1;
	}
	l->syms = ir_mem_realloc(l->syms, size * sizeof(yy_sym));
	l->size = size;
}

/* C Preprocessor */
static void pp_debug_tokens(rcc_ctx *rcc, FILE *f, yy_sym *tokens, const pp_macro *macro);
static void pp_debug_include(rcc_ctx *rcc, yy_sym inc_sym, const char *name, size_t len, bool is_user);
static void pp_debug_macro(rcc_ctx *rcc, yy_sym sym, yy_sym name, pp_macro *macro);
static void pp_print_pragma(rcc_ctx *rcc, yy_sym sym);
static void pp_parse_pragma(rcc_ctx *rcc, bool operator);

pp_subst_stream *pp_push_stream(rcc_ctx *rcc)
{
	if (!rcc->pp_stream) {
		return rcc->pp_stream = rcc->pp_subst_stack;
	} else {
		if (rcc->pp_stream >= rcc->pp_subst_stack + (PP_SUBST_STACK_SIZE - 1)) yy_error("too deep macro substitution level");
		return ++rcc->pp_stream;
	}
}

pp_subst_stream *pp_pop_stream(rcc_ctx *rcc)
{
	pp_subst_stream *stream = rcc->pp_stream;

	IR_ASSERT(stream);
	if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
	if (stream->start) pp_list_release(rcc, stream->start, stream->size);
	return rcc->pp_stream = (stream == rcc->pp_subst_stack) ? NULL : (stream - 1);
}

static bool pp_needs_space(yy_sym sym1, yy_sym sym2)
{
	if (PP_IS_ID(sym1)) {
		if (PP_IS_ID(sym2) || (sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER)) return 1;
		if (sym1 == YY_E && (sym2 == YY__PLUS || sym2 == YY__MINUS)) return 1;
	} else if (sym1 >= YY_DECIMAL_NUMBER && sym1 <= YY_PP_NUMBER) {
		if ((sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER) || PP_IS_ID(sym2)) return 1;
		if (sym2 == YY__PLUS || sym2 == YY__MINUS) return 1; /* this is not necessary, but used for compatibiluty */
	} else if (sym1 == YY__MINUS) {
		if (sym2 == YY__GREATER || sym2 == YY__MINUS_GREATER
		 || sym2 == YY__MINUS || sym2 == YY__MINUS_MINUS
		 || sym2 == YY__EQUAL || sym2 == YY__MINUS_EQUAL) return 1;
	} else if (sym1 == YY__PLUS) {
		if (sym2 == YY__PLUS || sym2 == YY__PLUS_PLUS
		 || sym2 == YY__EQUAL || sym2 == YY__PLUS_EQUAL) return 1;
	} else if (sym1 == YY__LESS) {
		if (sym2 == YY__EQUAL || sym2 == YY__LESS_EQUAL
		 || sym2 == YY__LESS || sym2 == YY__LESS_LESS
		 || sym2 == YY__LESS_EQUAL || sym2 == YY__LESS_LESS_EQUAL) return 1;
	} else if (sym1 == YY__GREATER) {
		if (sym2 == YY__EQUAL || sym2 == YY__GREATER_EQUAL
		 || sym2 == YY__GREATER || sym2 == YY__GREATER_GREATER
		 || sym2 == YY__GREATER_EQUAL || sym2 == YY__GREATER_GREATER_EQUAL) return 1;
	} else if (sym1 == YY__LESS_LESS) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__GREATER_GREATER) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__EQUAL) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__BANG) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__AND) {
		if (sym2 == YY__AND || sym2 == YY__AND_AND
		 || sym2 == YY__EQUAL || sym2 == YY__AND_EQUAL) return 1;
	} else if (sym1 == YY__BAR) {
		if (sym2 == YY__BAR || sym2 == YY__BAR_BAR
		 || sym2 == YY__EQUAL || sym2 == YY__BAR_EQUAL) return 1;
	} else if (sym1 == YY__STAR) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__SLASH) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__PERCENT) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__UPARROW) {
		if (sym2 == YY__EQUAL) return 1;
	} else if (sym1 == YY__HASH) {
		if (sym2 == YY__HASH) return 1;
	}
	return 0;
}

void pp_macro_define(rcc_ctx *rcc, yy_sym name, uint32_t flags, uint32_t num_args, yy_sym *tokens)
{
	pp_macro *macro;

	PP_ASSERT(PP_IS_ID(name), "<ID> expected");

	macro = ir_arena_alloc(&rcc->yy_arena, sizeof(pp_macro));
	macro->flags = flags;
	macro->num_args = num_args;
	macro->tokens = tokens;

	rcc->yy_hash.data[name].macro = macro;

	if ((rcc->yy_flags & PP_DUMP_MACROS) && !(flags & PP_MACRO_BUILTIN)) {
		pp_debug_macro(rcc, YY_DEFINE, name, macro);
	}
}

static void pp_macro_undef(rcc_ctx *rcc, yy_sym name)
{
	PP_ASSERT(PP_IS_ID(name), "<ID> expected");
	rcc->yy_hash.data[name].macro = NULL;

	if (rcc->yy_flags & PP_DUMP_MACROS) {
		pp_debug_macro(rcc, YY_UNDEF, name, NULL);
	}
}

#if PP_DEBUG
static void pp_inc_recursion_level(rcc_ctx *rcc)
{
	rcc->pp_recursion_level++;
}

static void pp_dec_recursion_level(rcc_ctx *rcc)
{
	rcc->pp_recursion_level--;
}

static void pp_debug_print_context(rcc_ctx *rcc)
{
	pp_subst_stream *stream;

	fprintf(stderr, "%*s  Context: ", rcc->pp_recursion_level * 2, "");
	if (rcc->pp_stream) {
		for (stream = rcc->pp_stream; stream >= rcc->pp_subst_stack; stream--) {
			pp_debug_tokens(rcc, stderr, stream->tokens, stream->macro);
			fprintf(stderr, "<EOF>");
		}
	}
	fprintf(stderr, "\n");
}

static void pp_debug_print_args(rcc_ctx *rcc, pp_macro *macro, pp_arg *args)
{
	int i;
	bool first = 1;

	fprintf(stderr, "%*s  Arguments: (", rcc->pp_recursion_level * 2, "");
	for (i = 0; i < macro->num_args; i++) {
		if (!first) fprintf(stderr, ",");
		first = 0;
		pp_debug_tokens(rcc, stderr, args[i].tokens, NULL);
	}
	fprintf(stderr, ")\n");
}

static void pp_debug_print_list(rcc_ctx *rcc, const char *hdr, yy_sym *tokens)
{
	fprintf(stderr, "%*s  %s: ", rcc->pp_recursion_level * 2, "", hdr);
	pp_debug_tokens(rcc, stderr, tokens, NULL);
	fprintf(stderr, "\n");
}
#endif

static yy_sym pp_paste(rcc_ctx *rcc, yy_sym sym1, const char *s1, size_t len1, yy_sym sym2, const char *s2, size_t len2)
{
	yy_dyn_str dyn_str;
	yy_sym sym;

	IR_ASSERT(sym1 > YY_WS && sym2 > YY_WS);
	if (sym1 == YY_PP_PLACE_MARKER) {
		rcc->yy_text = s2;
		rcc->yy_len = len2;
		return sym2;
	} else if (sym2 == YY_PP_PLACE_MARKER) {
		rcc->yy_text = s1;
		rcc->yy_len = len1;
		return sym1;
	} else if (PP_IS_ID(sym1)) {
		if (PP_IS_ID(sym2) || (sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER)) {
			bool ok = 1;
			size_t i;

			if (sym2 >= YY_FLOATING_NUMBER && sym2 <= YY_PP_NUMBER) {
				for (i = 0; i < len2; i++) {
					if (s2[i] == '.' || s2[i] == '+' || s2[i] == '-') {
						ok = 0;
					}
				}
			}
			if (ok) {
				yy_dyn_str_init(rcc, &dyn_str, s1, len1);
				yy_dyn_str_append0(rcc, &dyn_str, s2, len2);
				sym = yy_hash_lookup(rcc, dyn_str.str, dyn_str.len);
				return sym;
			}
		} else if (sym2 == YY_STRING && s2[0] == '"'
				&& ((len1 == 1 && (s1[0] == 'L' || s1[0] == 'U' || s1[0] == 'u'))
				 || (len1 == 2 && s1[0] == 'u' && s1[1] == '8'))) {
			yy_dyn_str_init(rcc, &dyn_str, s1, len1);
			yy_dyn_str_append0(rcc, &dyn_str, s2, len2);
			rcc->yy_text = dyn_str.str;
			rcc->yy_len = dyn_str.len;
			return YY_STRING;
		} else if (sym2 == YY_CHARACTER && s2[0] == '\''
				&& len1 == 1 && (s1[0] == 'L' || s1[0] == 'U' || s1[0] == 'u')) {
			yy_dyn_str_init(rcc, &dyn_str, s1, len1);
			yy_dyn_str_append0(rcc, &dyn_str, s2, len2);
			rcc->yy_text = dyn_str.str;
			rcc->yy_len = dyn_str.len;
			return YY_CHARACTER;
		}
	} else if (sym1 >= YY_DECIMAL_NUMBER && sym1 <= YY_PP_NUMBER) {
		if ((sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER) || PP_IS_ID(sym2) || sym2 == YY__POINT
		 || ((sym2 == YY__PLUS || sym2 == YY__MINUS)
		  && (s1[len1-1] == 'e' || s1[len1-1] == 'E' || s1[len1-1] == 'p' || s1[len1-1] == 'P'))) {
			yy_dyn_str_init(rcc, &dyn_str, s1, len1);
			yy_dyn_str_append0(rcc, &dyn_str, s2, len2);
			rcc->yy_text = dyn_str.str;
			rcc->yy_len = dyn_str.len;
			return YY_PP_NUMBER;
		}
	} else if (sym1 == YY__POINT && sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER) {
		yy_dyn_str_init(rcc, &dyn_str, s1, len1);
		yy_dyn_str_append0(rcc, &dyn_str, s2, len2);
		rcc->yy_text = dyn_str.str;
		rcc->yy_len = dyn_str.len;
		return YY_PP_NUMBER;
	} else if (sym1 == YY__MINUS) {
		if (sym2 == YY__GREATER) return YY__MINUS_GREATER;
		if (sym2 == YY__MINUS) return YY__MINUS_MINUS;
		if (sym2 == YY__EQUAL) return YY__MINUS_EQUAL;
	} else if (sym1 == YY__PLUS) {
		if (sym2 == YY__PLUS) return YY__PLUS_PLUS;
		if (sym2 == YY__EQUAL) return YY__PLUS_EQUAL;
	} else if (sym1 == YY__LESS) {
		if (sym2 == YY__EQUAL) return YY__LESS_EQUAL;
		if (sym2 == YY__LESS) return YY__LESS_LESS;
		if (sym2 == YY__LESS_EQUAL) return YY__LESS_LESS_EQUAL;
	} else if (sym1 == YY__GREATER) {
		if (sym2 == YY__EQUAL) return YY__GREATER_EQUAL;
		if (sym2 == YY__GREATER) return YY__GREATER_GREATER;
		if (sym2 == YY__GREATER_EQUAL) return YY__GREATER_GREATER_EQUAL;
	} else if (sym1 == YY__LESS_LESS) {
		if (sym2 == YY__EQUAL) return YY__LESS_LESS_EQUAL;
	} else if (sym1 == YY__GREATER_GREATER) {
		if (sym2 == YY__EQUAL) return YY__GREATER_GREATER_EQUAL;
	} else if (sym1 == YY__EQUAL) {
		if (sym2 == YY__EQUAL) return YY__EQUAL_EQUAL;
	} else if (sym1 == YY__BANG) {
		if (sym2 == YY__EQUAL) return YY__BANG_EQUAL;
	} else if (sym1 == YY__AND) {
		if (sym2 == YY__AND) return YY__AND_AND;
		if (sym2 == YY__EQUAL) return YY__AND_EQUAL;
	} else if (sym1 == YY__BAR) {
		if (sym2 == YY__BAR) return YY__BAR_BAR;
		if (sym2 == YY__EQUAL) return YY__BAR_EQUAL;
	} else if (sym1 == YY__STAR) {
		if (sym2 == YY__EQUAL) return YY__STAR_EQUAL;
	} else if (sym1 == YY__SLASH) {
		if (sym2 == YY__EQUAL) return YY__SLASH_EQUAL;
	} else if (sym1 == YY__PERCENT) {
		if (sym2 == YY__EQUAL) return YY__PERCENT_EQUAL;
	} else if (sym1 == YY__UPARROW) {
		if (sym2 == YY__EQUAL) return YY__UPARROW_EQUAL;
	} else if (sym1 == YY__HASH) {
		if (sym2 == YY__HASH) return YY__HASH_HASH;
	}
	yy_error_fmt("pasting \"%.*s\" and \"%.*s\" does not give a valid preprocessing token",
		(int)len1, s1, (int) len2, s2);
	return 0;
}

static void pp_macro_join(rcc_ctx *rcc, yy_sym *tokens)
{
	yy_sym sym, *src, *dst, *prev = NULL;

	src = dst = (yy_sym*)tokens;
	while (1) {
		sym = *src++;
		if (sym == YY_EOF) {
			break;
		} else if (sym == YY_PP_JOIN) {
			if (!prev) continue; /* skip ## if it's the first or the last in the list */
			do {
				sym = *src++;
			} while (sym == YY_WS || sym == YY_PP_JOIN);
			if (sym == YY_EOF) break; /* skip ## if it's the first or the last in the list */

			yy_sym next;
			const char *s1 = NULL, *s2 = NULL;
			size_t len1 = 0, len2 = 0;

			if (PP_HAS_VAL(*prev)) {
				pp_load_str(prev + 1, &s1, &len1);
			} else {
				s1 = yy_sym2strl(rcc, *prev & ~PP_NOSUBST, &len1);
			}
			if (PP_HAS_VAL(sym)) {
				pp_load_str(src, &s2, &len2);
			} else {
				sym &= ~PP_NOSUBST;
				s2 = yy_sym2strl(rcc, sym, &len2);
			}
			next = sym;
			sym = pp_paste(rcc, *prev, s1, len1, next, s2, len2);
			if (sym) {
				if (PP_HAS_VAL(next)) src += sizeof(void*)/sizeof(int32_t) + 1;
				if (sym != YY_PP_PLACE_MARKER || *src == YY_PP_JOIN) {
					*prev = sym;
					dst = prev + 1;
					if (PP_HAS_VAL(sym)) {
						IR_ASSERT(PP_HAS_VAL(*prev) || PP_HAS_VAL(next));
						dst = pp_save_val(rcc, dst);
					}
				} else {
					dst = prev;
				}
				continue;
			}
			/* skip ## in case of error */
			sym = next;
		}

		if (sym != YY_WS) prev = dst;
		if (src != dst + 1) {
			*dst++ = sym;
			if (PP_HAS_VAL(sym)) {
				/* copy value */
				if (sizeof(void*) == sizeof(int32_t)) {
					*dst++ = *src++;
					*dst++ = *src++;
				} else {
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src++;
				}
			}
		} else {
			dst++;
			if (PP_HAS_VAL(sym)) {
				if (sizeof(void*) == sizeof(int32_t)) {
					src += 2; dst += 2; /* skip value */
				} else {
					src += 3; dst += 3; /* skip value */
				}
			}
		}
	}

	if (src != dst) *dst = YY_EOF;
}

static void pp_macro_stringize(rcc_ctx *rcc, yy_sym *tokens)
{
	yy_dyn_str dyn_str;
	int spaces = 0;

	yy_dyn_str_init(rcc, &dyn_str, "\"", 1);
	while (1) {
		yy_sym sym = *tokens++;
		if (sym == YY_EOF) break;
		if (sym == YY_WS || sym == YY_EOL) {
			spaces++;
			continue;
		}
		while (spaces) {
			yy_dyn_str_append(rcc, &dyn_str, " ", 1);
			spaces--;
		}

		if (PP_HAS_VAL(sym)) {
			tokens = pp_load_val(rcc, tokens);
			if (sym == YY_STRING || sym == YY_CHARACTER) {
				const char *s = rcc->yy_text;
				while (rcc->yy_len) {
					char c = *s;
				    if ((c < 32 && c != '\t') || c == '\"' || c == '\\') {
						yy_dyn_str_append(rcc, &dyn_str, "\\", 1);
				    }
				    if (c >= 32 || c == '\t' /*&& c <= 126*/) {
						yy_dyn_str_append(rcc, &dyn_str, s, 1);
				    } else {
				        if (c == '\n') {
				            yy_dyn_str_append(rcc, &dyn_str, "n", 1);
				        } else {
							char buf[4];

				            buf[0] = '0' + ((c >> 6) & 7);
				            buf[1] = '0' + ((c >> 3) & 7);
				            buf[2] = '0' + (c & 7);
							yy_dyn_str_append(rcc, &dyn_str, buf, 3);
				        }
				    }
					s++;
					rcc->yy_len--;
				}
			}
		} else {
			rcc->yy_text = yy_sym2strl(rcc, sym & ~PP_NOSUBST, &rcc->yy_len);
		}
		yy_dyn_str_append(rcc, &dyn_str, rcc->yy_text, rcc->yy_len);
	}
	if (dyn_str.str[dyn_str.len - 1] == '\\') {
		int count = 0;
		size_t pos = dyn_str.len - 1;
		do {
			pos--;
			count++;
		} while(dyn_str.str[pos] == '\\');
		if (count % 2 != 0) {
			yy_warning("invalid string literal, ignoring final \"\\\"");
			dyn_str.len--;
			rcc->yy_arena->ptr--;
		}
	}
	yy_dyn_str_append0(rcc, &dyn_str, "\"", 1);

	rcc->yy_text = dyn_str.str;
	rcc->yy_len = dyn_str.len;
}

static void pp_macro_subst_args(rcc_ctx *rcc, pp_macro *macro, pp_arg *args, pp_list *replacement)
{
	int i;
	yy_sym *macro_tokens = macro->tokens + macro->num_args;

	if (!(macro->flags & PP_MACRO_HAS_JOIN)) {
		while (1) {
			int arg;
			yy_sym sym = *macro_tokens++;

			if (sym & PP_MACRO_ARG) {
				arg = sym & ~(PP_MACRO_ARG|PP_STRINGIZE);

				IR_ASSERT(arg >= 0);
				if (sym & PP_STRINGIZE) {
					PP_ASSERT(arg >= 0, "'#' is not followed by a macro parameter");
					pp_macro_stringize(rcc, args[arg].tokens);
					pp_list_push(replacement, YY_STRING);
					pp_list_push_val(rcc, replacement);
				} else {
					yy_sym *tokens = args[arg].tokens;

					if (!(args[arg].flags & PP_MACRO_EXPANDED)) {
						pp_list expansion;
						pp_subst_stream *this_stream, *stream;

						this_stream = stream = pp_push_stream(rcc);
						stream->macro = NULL;
						stream->start = NULL;
						stream->tokens = tokens;
						stream->skip_eof = 0;

						pp_list_init(rcc, &expansion);
						while (1) {
							sym = *stream->tokens++;
							if (sym == YY_EOF) {
								if (stream == this_stream) break;
								if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
								if (stream->start) pp_list_release(rcc, stream->start, stream->size);
								stream--;
								continue;
							}
							if (PP_IS_ID(sym) && !(sym & PP_NOSUBST)) {
								pp_macro *macro = rcc->yy_hash.data[sym].macro;

								if (macro) {
									if (!(macro->flags & PP_MACRO_DISABLED)) {
										bool ok;

										rcc->pp_stream = stream;
										pp_inc_recursion_level(rcc);
										ok = pp_macro_expand(rcc, macro, sym);
										pp_dec_recursion_level(rcc);
										stream = rcc->pp_stream;
										if (ok) continue;
									} else {
										if (pp_debug) {pp_debug_fprintf(stderr, "\"%s\" is disabled!\n", yy_sym2str(rcc, sym));}
										sym |= PP_NOSUBST;
									}
								}
							}
							pp_list_push(&expansion, sym);
							if (PP_HAS_VAL(sym)) {
								stream->tokens = pp_list_push_val_from(&expansion, stream->tokens);
							}
						}
						rcc->pp_stream = (stream == rcc->pp_subst_stack) ? NULL : (stream - 1);
						if (UNEXPECTED((args[arg].flags & PP_MACRO_EXPANDED) != 0)) {
							pp_list_release(rcc, expansion.syms, expansion.size);
							tokens = args[arg].tokens;
						} else {
							pp_list_push(&expansion, YY_EOF);

							args[arg].flags |= PP_MACRO_EXPANDED;
							args[arg].size = expansion.size;
							args[arg].tokens = tokens = expansion.syms;
						}
					}

					while (1) {
						sym = *tokens++;
						if (sym == YY_EOF) break;
						if (sym == YY_WS
						 && replacement->len
						 && replacement->syms[replacement->len - 1] == YY_WS) continue;
						pp_list_push(replacement, sym);
						if (PP_HAS_VAL(sym)) {
							tokens = pp_list_push_val_from(replacement, tokens);
						}
					}
				}
			} else {
				if (sym <= YY_WS) {
					if (sym == YY_WS
					 && replacement->len
					 && replacement->syms[replacement->len - 1] == YY_WS) continue;
					if (sym == YY_EOF) break;
				}
				pp_list_push(replacement, sym);
				if (PP_HAS_VAL(sym)) {
					macro_tokens = pp_list_push_val_from(replacement, macro_tokens);
				}
			}
		}
	} else {
		yy_sym prev = 0;

		while (1) {
			int arg;
			yy_sym sym = *macro_tokens++;

			if (sym == YY_EOF) {
				break;
			} else if (sym & PP_MACRO_ARG) {
				arg = sym & ~(PP_MACRO_ARG|PP_STRINGIZE);

				IR_ASSERT(arg >= 0);
				if (sym & PP_STRINGIZE) {
					PP_ASSERT(arg >= 0, "'#' is not followed by a macro parameter");
					pp_macro_stringize(rcc, args[arg].tokens);
					pp_list_push(replacement, YY_STRING);
					pp_list_push_val(rcc, replacement);
					prev = 0;
				} else {
					yy_sym *tokens = args[arg].tokens;

					if (prev == YY_PP_JOIN) {
						if ((macro->flags & PP_MACRO_VAR_ARG)
						 && arg == macro->num_args - 1
						 && replacement->len > 1
						 && replacement->syms[replacement->len - 2] == YY__COMMA) {
							/* GNU extension: remove ", ##" or replace it by "," */
							prev = 0;
							if (*tokens == YY_EOF) {
								replacement->len -= 2;
							} else {
								replacement->len -= 1;
							}
						}
					} else if (*macro_tokens != YY_PP_JOIN && !(args[arg].flags & PP_MACRO_EXPANDED)) {
						pp_list expansion;
						pp_subst_stream *this_stream, *stream;

						this_stream = stream = pp_push_stream(rcc);
						stream->macro = NULL;
						stream->start = NULL;
						stream->tokens = tokens;
						stream->skip_eof = 0;

						pp_list_init(rcc, &expansion);
						while (1) {
							sym = *stream->tokens++;
							if (sym == YY_EOF) {
								if (stream == this_stream) break;
								if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
								if (stream->start) pp_list_release(rcc, stream->start, stream->size);
								stream--;
								continue;
							}
							if (PP_IS_ID(sym) && !(sym & PP_NOSUBST)) {
								pp_macro *macro = rcc->yy_hash.data[sym].macro;

								if (macro) {
									if (!(macro->flags & PP_MACRO_DISABLED)) {
										bool ok;

										rcc->pp_stream = stream;
										pp_inc_recursion_level(rcc);
										ok = pp_macro_expand(rcc, macro, sym);
										pp_dec_recursion_level(rcc);
										stream = rcc->pp_stream;
										if (ok) continue;
									} else {
										if (pp_debug) {pp_debug_fprintf(stderr, "\"%s\" is disabled!\n", yy_sym2str(rcc, sym));}
										sym |= PP_NOSUBST;
									}
								}
							}
							pp_list_push(&expansion, sym);
							if (PP_HAS_VAL(sym)) {
								stream->tokens = pp_list_push_val_from(&expansion, stream->tokens);
							}
						}
						rcc->pp_stream = (stream == rcc->pp_subst_stack) ? NULL : (stream - 1);
						if (UNEXPECTED((args[arg].flags & PP_MACRO_EXPANDED))) {
							pp_list_release(rcc, expansion.syms, expansion.size);
						    tokens = args[arg].tokens;
						} else {
							pp_list_push(&expansion, YY_EOF);

							args[arg].flags |= PP_MACRO_EXPANDED;
							args[arg].size = expansion.size;
							args[arg].tokens = tokens = expansion.syms;
						}
					}

					if (*tokens == YY_EOF) {
						/* empty arg - insert placemarker */
						if (prev == YY_PP_JOIN || *macro_tokens == YY_PP_JOIN) {
							pp_list_push(replacement, YY_PP_PLACE_MARKER);
							prev = 0;
						}
					} else {
						while (1) {
							sym = *tokens++;
							if (sym == YY_EOF) break;
							if (sym == YY_WS && prev == YY_WS) continue;
							prev = sym;
							pp_list_push(replacement, sym);
							if (PP_HAS_VAL(sym)) {
								tokens = pp_list_push_val_from(replacement, tokens);
							}
						}
					}
				}
			} else {
				if (sym == YY_WS && prev == YY_WS) continue;
				prev = sym;
				pp_list_push(replacement, sym);
				if (PP_HAS_VAL(sym)) {
					macro_tokens = pp_list_push_val_from(replacement, macro_tokens);
				}
			}
		}
	}

	for (i = 0; i < macro->num_args; i++) {
		if (args[i].flags & PP_MACRO_EXPANDED) {
			pp_list_release(rcc, args[i].tokens, args[i].size);
		}
	}
}

static void pp_macro_read_args(rcc_ctx *rcc, pp_macro *macro, yy_sym name, pp_arg *args, pp_list *list)
{
	int num_args = 0;
	yy_sym sym, prev = 0;
	uint32_t save_flags = rcc->yy_flags;

	rcc->yy_flags |= YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR | YY_NO_MACRO;
	rcc->yy_flags &= ~YY_SKIP_WS;

	args[0].num_args = 0;
	do {
		sym = yy_next(rcc);
	} while (sym == YY_WS || sym == YY_EOL);
	while (1) {
		if (sym == YY_EOL) sym = YY_WS;
		if (sym == YY_WS) {
			if (prev != YY_WS) pp_list_push(list, sym);
		} else if (sym == YY__RPAREN) {
			num_args++;
			break;
		} else if (sym == YY__COMMA) {
			num_args++;
			if (num_args < macro->num_args) {
				pp_list_push(list, YY_EOF);
				args[num_args].num_args = list->len;
				do {
					sym = yy_next(rcc);
				} while (sym == YY_WS || sym == YY_EOL);
			} else {
				if (macro->flags & PP_MACRO_VAR_ARG) pp_list_push(list, sym);
				sym = yy_next(rcc);
			}
			continue;
		} else if (sym == YY__LPAREN) {
			uint32_t level = 1;

			pp_list_push(list, sym);
			while (1) {
				prev = sym;
				sym = yy_next(rcc);
				if (sym == YY_EOL) sym = YY_WS;
				if (sym == YY_WS) {
					if (prev != YY_WS) pp_list_push(list, sym);
				} else if (sym == YY__RPAREN) {
					pp_list_push(list, sym);
					level--;
					if (level == 0) break;
				} else if (sym == YY__LPAREN) {
					level++;
					pp_list_push(list, sym);
				} else if (sym == YY_EOF) {
					yy_error("unexpected <EOF>");
					break;
				} else {
					pp_list_push(list, sym);
					if (PP_HAS_VAL(sym)) {
						pp_list_push_val(rcc, list);
					}
				}
			}
		} else if (sym == YY_EOF) {
			yy_error("unexpected <EOF>");
			break;
		} else {
			pp_list_push(list, sym);
			if (PP_HAS_VAL(sym)) {
				pp_list_push_val(rcc, list);
			}
		}
		prev = sym;
		sym = yy_next(rcc);
	}

	rcc->yy_flags = save_flags;

	if (num_args == macro->num_args) {
		if ((macro->flags & PP_MACRO_VAR_ARG)
		 && num_args != 1
		 && (uint32_t)args[num_args - 1].num_args == list->len) {
			/* __VA_ARGS__ is not empty, COMMA ## __VA_ARGS__ should be expaned to COMMA */
			pp_list_push(list, YY_WS);
		}
	} else if (num_args > macro->num_args) {
		if (!(macro->flags & PP_MACRO_VAR_ARG)) {
			if (macro->num_args == 0 && num_args == 1 && list->len == 0) {
				/* Function macro withiout parameters */
			} else {
				yy_error_fmt("macro \"%s\" passed %d arguments, but takes just %d",
					yy_sym2str(rcc, name), num_args, macro->num_args);
			}
		}
	} else {
		if ((macro->flags & PP_MACRO_VAR_ARG) && num_args == macro->num_args - 1) {
			/* empty variadic argument */
			args[num_args].num_args = list->len;
		} else {
			yy_error_fmt("macro \"%s\" requires %d arguments, but only %d given",
				yy_sym2str(rcc, name), macro->num_args, num_args);
		}
	}

	pp_list_push(list, YY_EOF);

	for (num_args = 0; num_args <  macro->num_args; num_args++) {
		args[num_args].tokens = list->syms + args[num_args].num_args;
		args[num_args].flags = 0;
		args[num_args].num_args = 0;
	}
}

bool pp_macro_expand(rcc_ctx *rcc, pp_macro *macro, yy_sym name)
{
	yy_sym sym;
	pp_list replacement;
	uint32_t save_flags = rcc->yy_flags;

	rcc->yy_flags |= YY_ACCEPT_NOSUBST;
	replacement.syms = NULL;
	replacement.size = 0;
	replacement.len = 0;

	if (macro->flags & PP_MACRO_FUNCTION) {
		pp_list tmp;
		pp_arg *args;
		uint32_t ws = 0;

		if (pp_debug) {
			pp_debug_fprintf(stderr, "%*sExpand function macro: \"%s\"\n",
				rcc->pp_recursion_level * 2, "", yy_sym2str(rcc, name));
			pp_debug_print_context(rcc);
		}

		rcc->yy_flags |= YY_NO_MACRO;
		sym = yy_next(rcc);
		while (sym == YY_WS || sym == YY_EOL) {
			if (sym == YY_WS) ws |= 1;
			if (sym == YY_EOL) ws |= 2;
			sym = yy_next(rcc);
		}
		rcc->yy_flags = save_flags | YY_ACCEPT_NOSUBST;
		if (sym != YY__LPAREN) {
			/* not a function macro, backtrack */
			if (ws || sym != YY_EOF) {
				pp_subst_stream *stream;

				pp_list_init(rcc, &replacement);

				if (ws & 2) {
					pp_list_push(&replacement, YY_EOL);
				} else if (ws & 1) {
					pp_list_push(&replacement, YY_WS);
				}
				if (sym != YY_EOF) {
					pp_list_push(&replacement, sym);
					if (PP_HAS_VAL(sym)) {
						pp_list_push_val(rcc, &replacement);
					}
				}

				pp_list_push(&replacement, YY_EOF);
				stream = pp_push_stream(rcc);
				stream->macro = NULL;
				stream->size = replacement.size;
				stream->start = replacement.syms;
				stream->tokens = replacement.syms;
				stream->skip_eof = 1;
			}

			if (pp_debug) {pp_debug_fprintf(stderr, "%*s  Backtrack\n", rcc->pp_recursion_level * 2, "");}

			rcc->yy_flags = save_flags;
			return 0;
		}

		args = alloca(sizeof(pp_arg) * (macro->num_args + 1)); /* "+ 1" for macro->num_args == 0 case */
		pp_list_init(rcc, &tmp);

		pp_macro_read_args(rcc, macro, name, args, &tmp);

		if (pp_debug) {pp_debug_print_args(rcc, macro, args);}

		if (macro->flags & PP_MACRO_EMPTY) {
			pp_list_release(rcc, tmp.syms, tmp.size);
			rcc->yy_flags = save_flags;
			return 1;
		}

		pp_list_init(rcc, &replacement);
		pp_macro_subst_args(rcc, macro, args, &replacement);
		pp_list_release(rcc, tmp.syms, tmp.size);
	} else if (macro->flags & PP_MACRO_BUILTIN) {
		if (name == YY___COUNTER__ || name == YY___INCLUDE_LEVEL__ || name == YY___LINE__) {
			char buf[16], *s;
			int i = sizeof(buf);
			uint32_t n;

			if (name == YY___COUNTER__) {
				n = rcc->pp_counter++;
			} else if (name == YY___INCLUDE_LEVEL__) {
				n = rcc->pp_include_level;
			} else {
				IR_ASSERT(name == YY___LINE__);
				n = rcc->yy_line;
			}
			do {
				buf[--i] = '0' + n % 10;
				n = n / 10;
			} while (n != 0);

			rcc->yy_len = sizeof(buf) - i ;
			rcc->yy_text = s = ir_arena_alloc(&rcc->yy_arena, sizeof(buf) - i);
			memcpy(s, buf + i, sizeof(buf) - i);

			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_DECIMAL_NUMBER);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY___DATE__ || name == YY___TIME__) {
			time_t t;
			struct tm *tm;
			size_t len;
			char str[64], *s;

			time(&t);
			tm = localtime(&t);
			if (name == YY___DATE__) {
				len = strftime(str, sizeof(str), "\"%b %d %Y\"", tm);
			} else {
				len = strftime(str, sizeof(str), "\"%H:%M:%S\"", tm);
			}

			rcc->yy_len = len;
			rcc->yy_text = s = ir_arena_alloc(&rcc->yy_arena, len);
			memcpy(s, str, len);

			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY___FILE__ || name == YY___BASE_FILE__) {
			size_t len;
			const char *str;
			char *s;

			if (name == YY___FILE__ || rcc->pp_include_level == 0) {
				str = yy_sym2strl(rcc, rcc->yy_file_name, &len);
			} else {
				str = yy_sym2strl(rcc, rcc->pp_include_stack[rcc->pp_include_level].file_name, &len);
			}

			// TODO: intern the quoted string ???
			rcc->yy_len = len + 2;
			rcc->yy_text = s = ir_arena_alloc(&rcc->yy_arena, len + 2);
			s[0] = '"';
			memcpy(s + 1, str, len);
			s[len + 1] = '"';

			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY___FUNCTION__ || name == YY___FUNC__ || name == YY___PRETTY_FUNCTION__) {
			yy_sym func_name = c_get_current_func_name(rcc);

			if (func_name) {
				size_t len;
				const char *name = yy_sym2strl(rcc, func_name, &len);
				char *s;

				rcc->yy_len = len + 2;
				rcc->yy_text = s = ir_arena_alloc(&rcc->yy_arena, len + 2);
				s[0] = '"';
				memcpy(s + 1, name, len);
				s[len + 1] = '"';
			} else {
				rcc->yy_text = "\"\"";
				rcc->yy_len = 2;
			}

			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY___HAS_ATTRIBUTE) {
			bool b;

			sym = yy_next(rcc);
			rcc->yy_flags &= ~YY_ACCEPT_NOSUBST;
			if (sym != YY__LPAREN) yy_error("'(' expected");
			sym = yy_next(rcc);
			if (!PP_IS_ID(sym)) yy_error("<ID> expected");
			b = YY_HAS_ATTRIBUTE(sym);
			sym = yy_next(rcc);
			if (sym != YY__RPAREN) yy_error("')' expected");

			rcc->yy_text = b ? "1" : "0";
			rcc->yy_len = 1;
			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_DECIMAL_NUMBER);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY___HAS_BUILTIN) {
			bool b;

			sym = yy_next(rcc);
			rcc->yy_flags &= ~YY_ACCEPT_NOSUBST;
			if (sym != YY__LPAREN) yy_error("'(' expected");
			sym = yy_next(rcc);
			if (!PP_IS_ID(sym)) yy_error("<ID> expected");
			b = YY_HAS_BUILTIN(sym);
			sym = yy_next(rcc);
			if (sym != YY__RPAREN) yy_error("')' expected");

			rcc->yy_text = b ? "1" : "0";
			rcc->yy_len = 1;
			pp_list_init(rcc, &replacement);
			pp_list_push(&replacement, YY_DECIMAL_NUMBER);
			pp_list_push_val(rcc, &replacement);
		} else if (name == YY__PRAGMA) {
			/* C99 _Pragma() operator */
			char *buf;
			const char *str;
			size_t len, i, j;

			rcc->yy_flags &= ~YY_ACCEPT_NOSUBST;
			rcc->yy_flags |= YY_SKIP_WS;
			sym = yy_next(rcc);
			if (sym != YY__LPAREN) yy_error("'(' expected");
			sym = yy_next(rcc);
			if (sym != YY_STRING) yy_error("<STRING> expected");
			str = rcc->yy_text;
			len = rcc->yy_len;
			sym = yy_next(rcc);
			if (sym != YY__RPAREN) yy_error("')' expected");

			/* Create a stream with unescaped string and pass it to pp_parse_pragma() */
			buf = alloca(len);
			len = len - 1;
			for (i = 0, j = 1; j < len; j++) {
				if (str[j] == '\\' && j + 1 < len && (str[j + 1] == '\\' || str[j + 1] == '\"')) {
					j++;
				}
				buf[i++] = str[j];
			}
			len = i;
			buf[i++] = '\n';
			buf[i] = 0;

			const char      *old_pos     = rcc->yy_pos;
			const char      *old_text    = rcc->yy_text;
			const char      *old_linepos = rcc->yy_linepos;
			size_t           old_len     = rcc->yy_len;
			uint32_t         old_line    = rcc->yy_line;
			const char      *old_buf     = rcc->yy_buf;
			const char      *old_end     = rcc->yy_end;
			pp_subst_stream *old_stream  = rcc->pp_stream;

			rcc->yy_pos = rcc->yy_text = rcc->yy_linepos = rcc->yy_buf = buf;
			rcc->yy_len = 0;
			rcc->yy_end = buf + len;

			rcc->yy_flags &= ~YY_SKIP_EOL;
			rcc->yy_flags |= YY_SKIP_WS | YY_NO_MACRO | YY_ACCEPT_PUNCTUATOR | YY_NO_DIRECTIVE;

			rcc->pp_stream = NULL;

			pp_parse_pragma(rcc, 1);

			rcc->yy_pos     = old_pos;
			rcc->yy_text    = old_text;
			rcc->yy_linepos = old_linepos;
			rcc->yy_len     = old_len;
			rcc->yy_line    = old_line;
			rcc->yy_buf     = old_buf;
			rcc->yy_end     = old_end;
			rcc->pp_stream  = old_stream;

			rcc->yy_flags = save_flags;
			return 1;
		} else if (name == YY___PRAGMA) {
			/* MSVC __pragma() operator */
			pp_subst_stream *stream;
			int level = 0;

			rcc->yy_flags &= ~YY_ACCEPT_NOSUBST;
			rcc->yy_flags |= YY_SKIP_WS | YY_SKIP_EOL;
			sym = yy_next(rcc);
			if (sym != YY__LPAREN) yy_error("'(' expected");
			pp_list_init(rcc, &replacement);
			while (1) {
				sym = yy_next(rcc);
				if (sym == YY__RPAREN) {
					if (level == 0) break;
					level--;
				} else if (sym == YY__LPAREN) {
					level++;
				}
				pp_list_push(&replacement, sym);
				if (PP_HAS_VAL(sym)) {
					pp_list_push_val(rcc, &replacement);
				}
			}
			pp_list_push(&replacement, YY_EOL);
			pp_list_push(&replacement, YY_EOF);

			stream = pp_push_stream(rcc);
			stream->macro = NULL;
			stream->size = replacement.size;
			stream->start = NULL;
			stream->tokens = replacement.syms;
			stream->skip_eof = 0;

			rcc->yy_flags &= ~YY_SKIP_EOL;
			rcc->yy_flags |= YY_SKIP_WS | YY_NO_MACRO | YY_ACCEPT_PUNCTUATOR | YY_NO_DIRECTIVE;

			pp_parse_pragma(rcc, 1);

			rcc->pp_stream = (stream == rcc->pp_subst_stack) ? NULL : (stream - 1);

			rcc->yy_flags = save_flags;
			return 1;
		} else {
			yy_error_fmt("bad builtin macro \"%.*s\"", rcc->yy_len, rcc->yy_text);
		}
	} else if (macro->flags & PP_MACRO_EMPTY) {
		rcc->yy_flags = save_flags;
		return 1;
	} else {
		yy_sym *tokens = macro->tokens;

		if (pp_debug) {
			pp_debug_fprintf(stderr, "%*sExpand object macro: \"%s\"\n",
				rcc->pp_recursion_level * 2, "", yy_sym2str(rcc, name));
		}

		if (macro->flags & PP_MACRO_HAS_JOIN) {
			pp_list_init(rcc, &replacement);
			while (1) {
				sym = *tokens++;
				if (sym == YY_EOF) break;
				pp_list_push(&replacement, sym);
				if (PP_HAS_VAL(sym)) {
					tokens = pp_list_push_val_from(&replacement, tokens);
				}
			}
		} else {
			pp_subst_stream *stream;

			stream = pp_push_stream(rcc);
			stream->macro = macro;
			stream->start = NULL;
			stream->tokens = tokens;
			stream->skip_eof = 1;

			macro->flags |= PP_MACRO_DISABLED;
			rcc->yy_flags = save_flags;
			return 1;
		}
	}

	if (replacement.len != 0) {
		yy_sym *tokens;
		pp_subst_stream *stream;

		pp_list_push(&replacement, YY_EOF);
		tokens = replacement.syms;

		if (pp_debug) {pp_debug_print_list(rcc, "Replacement", tokens);}

		if (macro->flags & PP_MACRO_HAS_JOIN) {
			pp_macro_join(rcc, tokens);

			if (pp_debug) {pp_debug_print_list(rcc, "Joined Replacement", tokens);}
		}

		stream = pp_push_stream(rcc);
		stream->macro = macro;
		stream->size = replacement.size;
		stream->start = tokens;
		stream->tokens = tokens;
		stream->skip_eof = 1;

		macro->flags |= PP_MACRO_DISABLED;
	} else {
		pp_list_release(rcc, replacement.syms, replacement.size);
	}

	rcc->yy_flags = save_flags;
	return 1;
}

static void pp_skip_until_eol(rcc_ctx *rcc)
{
	yy_sym sym;

	do {
		sym = yy_next(rcc);
	} while (sym != YY_EOL && sym != YY_EOF);
}

static void pp_skip_asm_comments(rcc_ctx *rcc)
{
	const unsigned char *pos;
	uint8_t ch;

	IR_ASSERT(!rcc->pp_stream);
	pos = (const unsigned char*)rcc->yy_pos;
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
	rcc->yy_pos = (const char*)pos;
}

static bool pp_eval_ifdef(rcc_ctx *rcc, bool ifdef, bool start_of_include)
{
	yy_sym id, sym = yy_next(rcc);

	if (!PP_IS_ID(sym)) {
		if (sym == YY_EOL) {
			yy_error("mising macro name");
		} else {
			yy_error("macro name must be an identifier");
			pp_skip_until_eol(rcc);
		}
		return !(ifdef ^ 0);
	}

	id = sym;
	rcc->pp_include_ifndef_macro = (start_of_include && !ifdef) ? id : 0;
	sym = yy_next(rcc);
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", ifdef ? "ifdef" : "ifndef");
		pp_skip_until_eol(rcc);
	}

	return !(ifdef ^ (pp_macro_is_defined(rcc, id)
		|| id == YY___HAS_INCLUDE
		|| id == YY___HAS_INCLUDE_NEXT));
}

static void pp_push_include(rcc_ctx *rcc, yy_sym file_name, const char *buf, size_t size)
{
	pp_include_state *old_state;

	IR_ASSERT(rcc->pp_include_level < INCLUDE_STACK_SIZE);
	old_state = &rcc->pp_include_stack[rcc->pp_include_level++];
	old_state->pos       = rcc->yy_pos;
	old_state->text      = rcc->yy_text;
	old_state->linepos   = rcc->yy_linepos;
	old_state->len       = rcc->yy_len;
	old_state->line      = rcc->yy_line;
	old_state->buf       = rcc->yy_buf;
	old_state->end       = rcc->yy_end;
	old_state->file_name = rcc->yy_file_name;
	old_state->if_level  = rcc->pp_include_ifdef_level;
	old_state->state     = rcc->pp_include_ifndef_state;
	old_state->macro     = rcc->pp_include_ifndef_macro;
	old_state->next_dir  = rcc->pp_next_search_dir;

	rcc->yy_pos = rcc->yy_text = rcc->yy_linepos = rcc->yy_buf = buf;
	rcc->yy_len = 0;
	rcc->yy_line = 1;
	rcc->yy_end = buf + size;
	rcc->yy_file_name = file_name;

	rcc->pp_include_ifdef_level = rcc->pp_ifdef_level;
	rcc->pp_include_ifndef_state = YY_INCLUDE_START;
	rcc->pp_include_ifndef_macro = 0;

	rcc->pp_next_search_dir = rcc->pp_last_search_dir + 1;
}

void pp_pop_include(rcc_ctx *rcc)
{
	pp_include_state *old_state;

	IR_ASSERT(rcc->pp_include_level > 0);

	if (rcc->pp_include_ifdef_level != rcc->pp_ifdef_level) {
		yy_error("missign #endif");
	}

	if (rcc->pp_include_ifndef_state & YY_INCLUDE_END) {
		if (!rcc->pp_include_hash) {
			rcc->pp_include_hash = ir_mem_malloc(sizeof(ir_hashtab));
			ir_hashtab_init(rcc->pp_include_hash, 32);
		}
		ir_hashtab_add(rcc->pp_include_hash, rcc->yy_file_name, rcc->pp_include_ifndef_macro);

		char buf[MAXPATHLEN];
		char *path_str = realpath(yy_sym2str(rcc, rcc->yy_file_name), buf);
		if (path_str) {
			yy_sym path_sym = yy_hash_lookup(rcc, path_str, strlen(path_str));
			if (path_sym != rcc->yy_file_name) {
				ir_hashtab_add(rcc->pp_include_hash, path_sym, rcc->pp_include_ifndef_macro);
			}
			if (path_str != buf) free(path_str);
		}
	}

	ir_mem_free((void*)rcc->yy_buf);

	old_state = &rcc->pp_include_stack[--rcc->pp_include_level];
	rcc->yy_pos       = old_state->pos;
	rcc->yy_text      = old_state->text;
	rcc->yy_linepos   = old_state->linepos;
	rcc->yy_len       = old_state->len ;
	rcc->yy_line      = old_state->line;
	rcc->yy_buf       = old_state->buf;
	rcc->yy_end       = old_state->end;
	rcc->yy_file_name = old_state->file_name;
	rcc->pp_include_ifdef_level  = old_state->if_level;
	rcc->pp_include_ifndef_state = old_state->state;
	rcc->pp_include_ifndef_macro = old_state->macro;
	rcc->pp_next_search_dir      = old_state->next_dir;
}

static const char *pp_read_file(rcc_ctx *rcc, yy_sym file_name, int fd, size_t *size_ptr)
{
	size_t size, ret;
	char *buf;
	struct stat stat_buf;

	if (fstat(fd, &stat_buf) != 0) {
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(rcc, file_name));
		return NULL;
	}
	size = stat_buf.st_size;

	buf = ir_mem_malloc(size + 1);
	if (!buf) {
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(rcc, file_name));
		return NULL;
	}

	ret = read(fd, buf, size);
	if (ret != size) {
		ir_mem_free(buf);
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(rcc, file_name));
		return NULL;
	}
	buf[size] = '\0'; /* End marker */

	*size_ptr = size;
	return buf;
}

static yy_sym pp_find_included_ex(rcc_ctx *rcc, yy_sym resolved_name)
{
	yy_sym macro_name = ir_hashtab_find(rcc->pp_include_hash, resolved_name);

	if (macro_name != IR_INVALID_VAL
	 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(rcc, macro_name))) {
		return resolved_name;
	}

	return 0;
}

static yy_sym pp_find_included(rcc_ctx *rcc, const char *name, size_t len)
{
	yy_sym macro_name, resolved_name = yy_hash_find(rcc, name, len);

	if (resolved_name) {
		macro_name = ir_hashtab_find(rcc->pp_include_hash, resolved_name);
		if (macro_name != IR_INVALID_VAL
		 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(rcc, macro_name))) {
			return resolved_name;
		}
	}

	return 0;
}

static bool pp_find_included_realpath(rcc_ctx *rcc, const char *name)
{
	char buf[MAXPATHLEN];
	char *path = realpath(name, buf);
	yy_sym macro_name, resolved_name;

	if (path) {
		resolved_name = yy_hash_find(rcc, path, strlen(path));
		if (resolved_name) {
			macro_name = ir_hashtab_find(rcc->pp_include_hash, resolved_name);
			if (macro_name != IR_INVALID_VAL
			 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(rcc, macro_name))) {
				if (path != buf) free(path);
				return 1;
			}
		}
		if (path != buf) free(path);
	}
	return 0;
}

static yy_sym pp_find_include(rcc_ctx *rcc, yy_dyn_str *name, int start_search_dir, const char **buf_ptr, size_t *size_ptr)
{
	int fd;
	int i;
	yy_sym resolved_name;

	if (IS_ABSPATH(name->str)) {
		if (rcc->pp_include_hash) {
			resolved_name = pp_find_included(rcc, name->str, name->len);
			if (resolved_name) {
				if (buf_ptr) *buf_ptr = NULL;
				return resolved_name;
			}
		}
		fd = open(name->str, O_RDONLY | O_BINARY);
		if (fd >= 0) {
			resolved_name = yy_hash_lookup(rcc, name->str, name->len);
			if (rcc->pp_include_hash && pp_find_included_realpath(rcc, name->str)) {
				close(fd);
				if (buf_ptr) *buf_ptr = NULL;
				return resolved_name;
			}
			goto read_file;
		}
	} else {
		if (!start_search_dir) {
			size_t len, j;
			const char *file_name = yy_sym2strl(rcc, rcc->yy_file_name, &len);

			for (j = len; j > 0; j--) {
				if (IS_DIRSEP(file_name[j-1])) {
					break;
				}
			}
			if (j == 0) {
				if (rcc->pp_include_hash) {
					resolved_name = pp_find_included(rcc, name->str, name->len);
					if (resolved_name) {
						rcc->pp_last_search_dir = 0;
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
				}
				fd = open(name->str, O_RDONLY | O_BINARY);
				if (fd >= 0) {
					rcc->pp_last_search_dir = 0;
					resolved_name = yy_hash_lookup(rcc, name->str, name->len);
					if (rcc->pp_include_hash && pp_find_included_realpath(rcc, name->str)) {
						close(fd);
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
					goto read_file;
				}
			} else {
				yy_dyn_str buf;
				void *checkpoint = ir_arena_checkpoint(rcc->yy_arena);
				yy_dyn_str_init(rcc, &buf, file_name, j);
				yy_dyn_str_append0(rcc, &buf, name->str, name->len);
				if (rcc->pp_include_hash) {
					resolved_name = pp_find_included(rcc, buf.str, buf.len);
					if (resolved_name) {
						rcc->pp_last_search_dir = 0;
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
				}
				fd = open(buf.str, O_RDONLY | O_BINARY);
				if (fd >= 0) {
					rcc->pp_last_search_dir = 0;
					resolved_name = yy_hash_lookup(rcc, buf.str, buf.len);
					if (rcc->pp_include_hash && pp_find_included_realpath(rcc, buf.str)) {
						close(fd);
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
					goto read_file;
				}
				ir_arena_release(&rcc->yy_arena, checkpoint);
			}
		}

		for (i = start_search_dir ? start_search_dir - 1 : 0; i < rcc->pp_include_paths_count; i++) {
			yy_dyn_str buf;
			void *checkpoint = ir_arena_checkpoint(rcc->yy_arena);
			yy_dyn_str_init(rcc, &buf, rcc->pp_include_paths[i], strlen(rcc->pp_include_paths[i]));
			yy_dyn_str_append(rcc, &buf, "/", 1);
			yy_dyn_str_append0(rcc, &buf, name->str, name->len);
			if (rcc->pp_include_hash) {
				resolved_name = pp_find_included(rcc, buf.str, buf.len);
				if (resolved_name) {
					rcc->pp_last_search_dir = i + 1;
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
			}
			fd = open(buf.str, O_RDONLY | O_BINARY);
			if (fd >= 0) {
				rcc->pp_last_search_dir = i + 1;
				resolved_name = yy_hash_lookup(rcc, buf.str, buf.len);
				if (rcc->pp_include_hash && pp_find_included_realpath(rcc, buf.str)) {
					close(fd);
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
				goto read_file;
			}
			ir_arena_release(&rcc->yy_arena, checkpoint);
		}

		resolved_name = yy_hash_find(rcc, name->str, name->len);
		if (resolved_name) {
			size_t len;
			const char *content = c_stdinc_find(rcc, resolved_name, &len);

			if (content) {
				rcc->pp_last_search_dir = 1 + rcc->pp_include_paths_count;
				if (rcc->pp_include_hash) {
					if (pp_find_included_ex(rcc, resolved_name)) {
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
				}
				if (buf_ptr) {
					char *buf = ir_mem_malloc(len + 1);
					memcpy(buf, content, len + 1);
					*buf_ptr = buf;
					*size_ptr = len;
				}
				return resolved_name;
			}
		}
	}

	if (buf_ptr) *buf_ptr = NULL;
	return 0;

read_file:
	if (buf_ptr) *buf_ptr = pp_read_file(rcc, resolved_name, fd, size_ptr);
	close(fd);
	return resolved_name;
}

static bool pp_parse_include_filename(rcc_ctx *rcc, yy_dyn_str *name, bool *is_user)
{
	yy_sym sym;

	while (1) {
		sym = yy_next(rcc);
		if (sym == YY_STRING) {
			yy_dyn_str_init0(rcc, name, rcc->yy_text + 1, rcc->yy_len - 2);
			*is_user = 1;
			break;
		} else if (sym == YY__LESS) {
			if (!rcc->pp_stream) {
				const char *save_yy_pos = rcc->yy_pos;

				while (1) {
					char ch = *rcc->yy_pos++;
					if (ch == '>') {
						break;
					} else if (ch == '\0' || ch == '\r' || ch == '\n') {
						rcc->yy_pos = save_yy_pos;
						goto try_expand;
//						yy_error("missing terminating > character");
					}
				}
				rcc->yy_len = rcc->yy_pos - rcc->yy_text;
				yy_dyn_str_init0(rcc, name, rcc->yy_text + 1, rcc->yy_len - 2);
				*is_user = 0;
				break;
			} else {
				pp_list list;
				yy_sym *tokens;

try_expand:
				pp_list_init(rcc, &list);
				while (1) {
					sym = yy_next(rcc);
					if (sym == YY__GREATER) {
						pp_list_push(&list, YY_EOF);
						break;
					} else if (sym == YY_EOL) {
						yy_error("missing terminating > character");
					} else {
						pp_list_push(&list, sym);
						if (PP_HAS_VAL(sym)) {
							pp_list_push_val(rcc, &list);
						}
					}
				}

				yy_dyn_str_init(rcc, name, "", 0);
				tokens = list.syms;
				while (*tokens) {
					sym = *tokens;
					tokens++;
					if (PP_HAS_VAL(sym)) {
						tokens = pp_load_val(rcc, tokens);
					} else {
						rcc->yy_text = yy_sym2strl(rcc, sym, &rcc->yy_len);
					}
					yy_dyn_str_append(rcc, name, rcc->yy_text, rcc->yy_len);
				}
				yy_dyn_str_append0(rcc, name, "", 0);
				pp_list_release(rcc, list.syms, list.size);
				*is_user = 0;
				break;
			}
			break;
		} else {
			return 0;
		}
	}

	return 1;
}

static void pp_parse_include(rcc_ctx *rcc, yy_sym inc_sym)
{
	yy_sym sym;
	yy_dyn_str name;
	bool is_user;
	int start_search_dir;
	const char *buf;
	size_t size;

	if (!pp_parse_include_filename(rcc, &name, &is_user)) {
		yy_error("#include expects \"FILENAME\" or <FILENAME>");
		return;
	}

	sym = yy_next(rcc);
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "include");
		pp_skip_until_eol(rcc);
	}

	if (inc_sym == YY_INCLUDE_NEXT) {
		start_search_dir = rcc->pp_next_search_dir;
	} else if (is_user) {
		start_search_dir = 0;
	} else {
		start_search_dir = 1;
	}

	if (rcc->pp_include_level >= INCLUDE_STACK_SIZE) yy_error("too deep include level");

	yy_sym resolved_name = pp_find_include(rcc, &name, start_search_dir, &buf, &size);

	if (!resolved_name) {
		yy_error_fmt("%.*s: No such file or directory", (int)name.len, name.str);
	}

	if (!buf) {
		return;
	}

	if (rcc->yy_flags & PP_DUMP_INCLUDES) {
		pp_debug_include(rcc, inc_sym, name.str, name.len, is_user);
	}

	pp_push_include(rcc, resolved_name, buf, size);
}

static bool pp_eval_has_include(rcc_ctx *rcc, yy_sym inc_sym)
{
	yy_sym sym;
	yy_dyn_str name;
	bool is_user;
	int start_search_dir;

	sym = yy_next(rcc);
	if (sym != YY__LPAREN) yy_error("'(' expected");

	if (!pp_parse_include_filename(rcc, &name, &is_user)) {
		yy_error("expected \"FILENAME\" or <FILENAME>");
		return 0;
	}

	sym = yy_next(rcc);
	if (sym != YY__RPAREN) yy_error("')' expected");

	if (inc_sym == YY___HAS_INCLUDE_NEXT) {
		start_search_dir = rcc->pp_next_search_dir;
	} else if (is_user) {
		start_search_dir = 0;
	} else {
		start_search_dir = 1;
	}

	return pp_find_include(rcc, &name, start_search_dir, NULL, NULL) != 0;
}

static bool pp_eval_expr(rcc_ctx *rcc)
{
	yy_sym sym;
	pp_list tokens;
	bool ret;

	pp_list_init(rcc, &tokens);
	while (1) {
		sym = yy_next(rcc);
next:
		if (sym == YY_EOL) {
			break;
		} else if (sym == YY_DEFINED) {
			yy_sym id;
			uint32_t save_flags = rcc->yy_flags;

			rcc->yy_flags |= YY_NO_MACRO | YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
			id = yy_next(rcc);
			if (id == YY__LPAREN) {
				id = yy_next(rcc);
				if (!PP_IS_ID(id)) yy_error("??");
				sym = yy_next(rcc);
				if (sym != YY__RPAREN) yy_error("??");
			} else if (!PP_IS_ID(id)) {
				yy_error("??");
			}
			rcc->yy_flags = save_flags;
			rcc->yy_text = (pp_macro_is_defined(rcc, id)
				|| id == YY___HAS_INCLUDE
				|| id == YY___HAS_INCLUDE_NEXT) ? "1" : "0";
			rcc->yy_len = 1;
			pp_list_push(&tokens, YY_DECIMAL_NUMBER);
			pp_list_push_val(rcc, &tokens);
		} else if (sym == YY___HAS_INCLUDE || sym == YY___HAS_INCLUDE_NEXT) {
			uint32_t save_flags = rcc->yy_flags;
			bool ret;

			rcc->yy_flags |= YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
			ret = pp_eval_has_include(rcc, sym);
			rcc->yy_flags = save_flags;
			rcc->yy_text = ret ? "1" : "0";
			rcc->yy_len = 1;
			pp_list_push(&tokens, YY_DECIMAL_NUMBER);
			pp_list_push_val(rcc, &tokens);
		} else if (PP_IS_ID(sym)) {
			/* undefined macro */
			yy_sym sym2;
			uint32_t save_flags = rcc->yy_flags;

			rcc->yy_flags |= YY_NO_MACRO;
			sym2 = yy_next(rcc);
			rcc->yy_flags = save_flags;
			if (sym2 == YY__LPAREN) {
				pp_list_push(&tokens, sym);
			} else {
				rcc->yy_text = "0";
				rcc->yy_len = 1;
				pp_list_push(&tokens, YY_DECIMAL_NUMBER);
				pp_list_push_val(rcc, &tokens);
			}
			sym = sym2;
			goto next;
		} else {
			pp_list_push(&tokens, sym);
			if (PP_HAS_VAL(sym)) {
				pp_list_push_val(rcc, &tokens);
			}
		}
	}
	pp_list_push(&tokens, YY_EOF);

	if (pp_debug) {
		pp_debug_fprintf(stderr, "Evaluate expression: ");
		pp_debug_tokens(rcc, stderr, (yy_sym*)tokens.syms, NULL);
		pp_debug_fprintf(stderr, "\n");
	}

	pp_subst_stream *stream = pp_push_stream(rcc);
	stream->macro = NULL;
	stream->start = NULL;
	stream->tokens = tokens.syms;
	stream->skip_eof = 0;

	ret = parse_pp_expr(rcc);

	rcc->pp_stream = (rcc->pp_stream == rcc->pp_subst_stack) ? NULL : (rcc->pp_stream - 1);
	pp_list_release(rcc, tokens.syms, tokens.size);

	if (pp_debug) {pp_debug_fprintf(stdout, "#res %d\n", ret);}

	return ret;
}

static bool pp_macro_same(rcc_ctx *rcc, pp_macro *macro, uint32_t flags, int32_t num_args, yy_sym *tokens)
{
	yy_sym *s1, *s2, sym1, sym2;
	const char *text;
	size_t len;

	if (macro->flags != flags || macro->num_args != num_args) return 0;
	if (macro->flags & PP_MACRO_EMPTY) return 1;

	/* compare names [0..num_args] and macro body ... */
	s1 = macro->tokens;
	s2 = tokens;
	while (1) {
		sym1 = *s1++;
		sym2 = *s2++;
		if (sym1 != sym2) {
			return 0;
		} else if (sym1 == YY_EOF) {
			return 1;
		} else if (PP_HAS_VAL(sym1)) {
			s1 = pp_load_val(rcc, s1);
			len = rcc->yy_len;
			text = rcc->yy_text;
			s2 = pp_load_val(rcc, s2);
			if (len != rcc->yy_len || memcmp(text, rcc->yy_text, len) != 0) {
				return 0;
			}
		}
	}
}

static void pp_parse_define(rcc_ctx *rcc)
{
	yy_sym id, sym;
	const char *end;
	uint32_t flags = 0;
	int32_t num_args = 0;
	pp_list tokens;
	int id_line;
	const char *id_text;
	const char *id_linepos;
	pp_macro *old;
	uint32_t save_flags;
	const char *define_linepos = rcc->yy_linepos;

	sym = yy_next(rcc);
	if (PP_IS_ID(sym)) {
		end = rcc->yy_pos;
	} else if (sym == YY_EOL) {
		rcc->yy_linepos = define_linepos;
		rcc->yy_pos = rcc->yy_text;
		rcc->yy_line--;
		yy_error("no macro name given in #define directive");
		return;
	} else {
		yy_error("macro names must be identifiers");
		return;
	}

	id = sym;
	if (id == YY_DEFINED) yy_error("\"defined\" cannot be used as a macro name");

	id_line = rcc->yy_line;
	id_text = rcc->yy_text;
	id_linepos = rcc->yy_linepos;

	save_flags = rcc->yy_flags;
	rcc->yy_flags |= YY_NO_MACRO | YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
	tokens.syms = NULL;

	sym = yy_next(rcc);
	if (sym == YY__LPAREN && rcc->yy_text == end) {
		flags |= PP_MACRO_FUNCTION;
		/* parse function macro parameters */
		sym = yy_next(rcc);
		while (sym != YY__RPAREN && sym != YY__POINT_POINT_POINT ) {
			if (!PP_IS_ID(sym)) {
				if (sym == YY_EOL) {
					rcc->yy_linepos = define_linepos;
					rcc->yy_pos = rcc->yy_text;
					rcc->yy_line--;
					yy_error("expected parameter name before end of line");
				} else {
					yy_error_fmt("expected parameter name, found \"%s\"", yy_sym2str(rcc, sym));
				}
			}
			if (sym == YY___VA_ARGS__) yy_warning("__VA_ARGS__ can only appear in the expansion of a C99 variadic macro");
			if (!num_args) {
				pp_list_init(rcc, &tokens);
			} else {
				uint32_t n = num_args;

				do {
					n--;
					if (tokens.syms[n] == sym) {
						yy_error_fmt("duplicate macro parameter \"%.*s\"", (int)rcc->yy_len, rcc->yy_text);
					}
				} while (n > 0);
			}
			num_args++;
			pp_list_push(&tokens, sym);
			sym = yy_next(rcc);
			if (sym == YY__POINT_POINT_POINT) {
				flags |= PP_MACRO_VAR_ARG;
				sym = yy_next(rcc);
				break;
			} else {
				if (sym != YY__COMMA) break;
				sym = yy_next(rcc);
			}
		}
		if (sym == YY__POINT_POINT_POINT && !(flags & PP_MACRO_VAR_ARG)) {
			if (!num_args) {
				pp_list_init(rcc, &tokens);
			} else {
				uint32_t n = num_args;

				do {
					n--;
					if (tokens.syms[n] == YY___VA_ARGS__) {
						yy_error("duplicate macro parameter \"__VA_ARGS__\"");
					}
				} while (n > 0);
			}
			num_args++;
			pp_list_push(&tokens, YY___VA_ARGS__);
			flags |= PP_MACRO_VAR_ARG;
			sym = yy_next(rcc);
		}
		if (sym != YY__RPAREN) {
			if (sym == YY_EOL) {
				rcc->yy_linepos = define_linepos;
				rcc->yy_pos = rcc->yy_text;
				rcc->yy_line--;
				yy_error("expected ')' before end of line");
			} else if (flags & PP_MACRO_VAR_ARG) {
				yy_error("expected ')' after \"...\"");
			} else {
				yy_error_fmt("expected ',' or ')', found \"%s\"", yy_sym2str(rcc, sym));
			}
		}
		sym = yy_next(rcc);
	}

	/* parse macro replacement tokens */
	if (sym != YY_EOL) {
		yy_sym prev = YY_EOF;

		if (!num_args) {
			pp_list_init(rcc, &tokens);
		}
		rcc->yy_flags &= ~YY_SKIP_WS;

		for (; sym != YY_EOL; sym = yy_next(rcc)) {
next:
			if (sym == YY_WS) {
				if (prev != YY_WS) {
					prev = sym;
					pp_list_push(&tokens, sym);
				}
			} else if (PP_IS_ID(sym)) {
				int j;

				for (j = 0; j < num_args; j++) {
					if (tokens.syms[j] == sym) {
						sym = j | PP_MACRO_ARG;
						break;
					}
				}
				prev = sym;
				pp_list_push(&tokens, sym);
			} else if (sym == YY__HASH) {
				if (flags & PP_MACRO_FUNCTION) {
					int j;

					do {
						sym = yy_next(rcc);
					} while (sym == YY_WS);
					if (!PP_IS_ID(sym)) yy_error("'#' is not followed by a macro parameter");
					for (j = 0; j < num_args; j++) {
						if (tokens.syms[j] == sym) {
							sym = j | PP_MACRO_ARG | PP_STRINGIZE;
							break;
						}
					}
					if (j >= num_args) yy_error("'#' is not followed by a macro parameter");
				}
				prev = sym;
				pp_list_push(&tokens, sym);
			} else if (sym == YY__HASH_HASH) {
				if (prev == YY_EOF) yy_error("##' cannot appear at either end of a macro expansion");
				if (prev == YY_WS) {
					tokens.len--;
				}
				do {
					sym = yy_next(rcc);
				} while (sym == YY_WS || sym == YY__HASH_HASH);
				if (sym == YY_EOL) yy_error("##' cannot appear at either end of a macro expansion");
				flags |= PP_MACRO_HAS_JOIN;
				pp_list_push(&tokens, YY_PP_JOIN);
				goto next;
			} else {
				prev = sym;
				pp_list_push(&tokens, sym);
				if (PP_HAS_VAL(sym)) {
					char *s;

					s = ir_arena_alloc(&rcc->yy_arena, rcc->yy_len + 1);
					memcpy(s, rcc->yy_text, rcc->yy_len);
					s[rcc->yy_len] = 0; /* this trailing zero is only necessary for fp-number parsing */
					rcc->yy_text = s;
					pp_list_push_val(rcc, &tokens);
				}
			}
		}

		if (prev == YY_WS) {
			tokens.len--;
		}
		pp_list_push(&tokens, YY_EOF);
	} else {
		flags |= PP_MACRO_EMPTY;
	}

	rcc->yy_flags = save_flags;

	old = rcc->yy_hash.data[id].macro;
	if (old) {
		if (pp_macro_same(rcc, old, flags, num_args, tokens.syms)) {
			if (tokens.syms) pp_list_release(rcc, tokens.syms, tokens.size);
			return;
		} else {
			int save_line = rcc->yy_line;
			const char *save_text = rcc->yy_text;
			const char *save_linepos = rcc->yy_linepos;

			rcc->yy_line = id_line;
			rcc->yy_text = id_text;
			rcc->yy_linepos = id_linepos;
			yy_warning_fmt("\"%s\" redefined", yy_sym2str(rcc, id));
			rcc->yy_line = save_line;
			rcc->yy_text = save_text;
			rcc->yy_linepos = save_linepos;
		}
	}

	yy_sym *p = NULL;
	if (tokens.syms) {
		p = ir_arena_alloc(&rcc->yy_arena, sizeof(yy_sym) * tokens.len);
		memcpy(p, tokens.syms, sizeof(yy_sym) * tokens.len);
		pp_list_release(rcc, tokens.syms, tokens.size);
	}
	pp_macro_define(rcc, id, flags, num_args, p);
}

static void pp_parse_undef(rcc_ctx *rcc)
{
	yy_sym id, sym = yy_next(rcc);

	if (!PP_IS_ID(sym)) {
		if (sym == YY_EOL) {
			yy_error("mising macro name");
		} else {
			yy_error("macro name must be an identifier");
			pp_skip_until_eol(rcc);
		}
		return;
	}

	id = sym;
	if (id == YY_DEFINED) yy_error("\"defined\" cannot be used as a macro name");

	sym = yy_next(rcc);
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "undef");
		pp_skip_until_eol(rcc);
	}

	pp_macro_undef(rcc, id);
}

// #error pp-tokens? new-line
static void pp_parse_error(rcc_ctx *rcc, bool warning)
{
	yy_sym sym = yy_next(rcc);
	const char *msg, *end = rcc->yy_pos;

	msg = rcc->yy_text;
	if (sym != YY_EOL) {
		uint32_t save_flags = rcc->yy_flags;

		rcc->yy_flags &= ~YY_SKIP_WS;
		while (1) {
			sym = yy_next(rcc);
			if (sym == YY_EOL) break;
			if (sym != YY_WS) end = rcc->yy_pos;
		}
		rcc->yy_flags = save_flags;
	}

	if (warning) {
		yy_warning_fmt("#warning %.*s", end - msg, msg);
	} else {
		yy_error_fmt("#error %.*s", end - msg, msg);
	}
}

// #line digit-sequence new-line
// #line digit-sequence "s-char-sequence?" new-line
// #line pp-tokens new-line
static void pp_parse_line(rcc_ctx *rcc, yy_sym sym)
{
	if (sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_PP_NUMBER) {
		const char *s = rcc->yy_text;
		const char *e = s + rcc->yy_len;
		uint32_t n = 0;
		while (s != e && *s >= '0' && *s <= '9') {
			n = n * 10 + (*s - '0');
			s++;
		}
		rcc->yy_line = n - 1;
		sym = yy_next(rcc);
		if (sym == YY_STRING) {
			rcc->yy_file_name = yy_hash_lookup(rcc, rcc->yy_text + 1, rcc->yy_len - 2);
			sym = yy_next(rcc);
		}
		if (sym != YY_EOL) {
			pp_skip_until_eol(rcc);
		}
	} else {
		yy_error("#line directive requires a positive integer argument");
	}
}

static void pp_parse_pragma(rcc_ctx *rcc, bool operator)
{
	yy_sym name, sym = yy_next(rcc);

	if (sym == YY_ONCE) {
		if (!rcc->pp_include_hash) {
			rcc->pp_include_hash = ir_mem_malloc(sizeof(ir_hashtab));
			ir_hashtab_init(rcc->pp_include_hash, 32);
		}
		ir_hashtab_add(rcc->pp_include_hash, rcc->yy_file_name, YY_PRAGMA_ONCE);

		char buf[MAXPATHLEN];
		char *path_str = realpath(yy_sym2str(rcc, rcc->yy_file_name), buf);
		if (path_str) {
			yy_sym path_sym = yy_hash_lookup(rcc, path_str, strlen(path_str));
			if (path_sym != rcc->yy_file_name) {
				ir_hashtab_add(rcc->pp_include_hash, path_sym, YY_PRAGMA_ONCE);
			}
			if (path_str != buf) free(path_str);
		}

		rcc->pp_include_ifndef_state = 0;
		rcc->pp_include_ifndef_macro = 0;
	} else if (sym == YY_PUSH_MACRO) {
		pp_macro_list *p;

		sym = yy_next(rcc);
		if (sym != YY__LPAREN) goto error;
		sym = yy_next(rcc);
		if (sym != YY_STRING) goto error;
		name = yy_hash_lookup(rcc, rcc->yy_text + 1, rcc->yy_len - 2);
		sym = yy_next(rcc);
		if (sym != YY__RPAREN) goto error;
		p = ir_arena_alloc(&rcc->yy_arena, sizeof(pp_macro_list));
		p->macro = rcc->yy_hash.data[name].macro;
		p->next = rcc->yy_hash.data[name].macro_stack;
		rcc->yy_hash.data[name].macro_stack = p;
	} else if (sym == YY_POP_MACRO) {
		pp_macro_list *p;

		sym = yy_next(rcc);
		if (sym != YY__LPAREN) goto error;
		sym = yy_next(rcc);
		if (sym != YY_STRING) goto error;
		name = yy_hash_lookup(rcc, rcc->yy_text + 1, rcc->yy_len - 2);
		sym = yy_next(rcc);
		if (sym != YY__RPAREN) goto error;
		p = rcc->yy_hash.data[name].macro_stack;
		if (p) {
			rcc->yy_hash.data[name].macro = p->macro;
			rcc->yy_hash.data[name].macro_stack = p->next;
		} else {
			yy_warning_fmt("pragma pop_macro could not pop \"%s\"", yy_sym2str(rcc, name));
		}
	} else if (rcc->yy_flags & PP_PREPROCESS) {
		if (!(rcc->yy_flags & PP_NO_OUTPUT)) pp_print_pragma(rcc, sym);
		return;
	} else if (sym == YY_PACK) {
		if (!operator) {
			rcc->yy_flags &= ~YY_NO_MACRO;
		}
		sym = yy_next(rcc);
		if (sym != YY__LPAREN) goto error;
		sym = yy_next(rcc);
		if (sym == YY_OCTAL_NUMBER || sym == YY_DECIMAL_NUMBER || sym == YY_PP_NUMBER) {
pack_set:
			const char *s = rcc->yy_text;
			const char *e = s + rcc->yy_len;
			uint32_t n = 0;

			while (s != e && *s >= '0' && *s <= '9') {
				n = n * 10 + (*s - '0');
				s++;
			}
			if (n < 1 || n > 16 || (n & (n - 1)) != 0) yy_error_fmt("invalid \"pragma pack(%d)\" value", n);
			rcc->pp_pack = n;
			sym = yy_next(rcc);
		} else if (sym == YY_PUSH) {
			if (rcc->pp_pack_stack_pos < PACK_STACK_SIZE) {
				rcc->pp_pack_stack[rcc->pp_pack_stack_pos++] = rcc->pp_pack;
			} else {
				yy_error("too deep \"#pragma pack (push)\" level");
			}
			sym = yy_next(rcc);
			if (sym == YY__COMMA) {
				sym = yy_next(rcc);
				if (sym != YY_OCTAL_NUMBER && sym != YY_DECIMAL_NUMBER && sym != YY_PP_NUMBER) goto error;
				goto pack_set;
			}
		} else if (sym == YY_POP) {
			if (rcc->pp_pack_stack_pos) {
				rcc->pp_pack = rcc->pp_pack_stack[--rcc->pp_pack_stack_pos];
			} else {
				yy_warning("\"#pragma pack (pop)\" encountered without matching \"#pragma pack (push)\"");
			}
			sym = yy_next(rcc);
		} else {
			// default
			rcc->pp_pack = 0;
		}
		if (sym != YY__RPAREN) goto error;
	} else if (sym == YY__COMMENT) {
		yy_sym type;
		const char *str;
		size_t len;

		sym = yy_next(rcc);
		if (sym != YY__LPAREN) goto error;
		type = yy_next(rcc);
		sym = yy_next(rcc);
		if (sym != YY__COMMA) goto error;
		sym = yy_next(rcc);
		if (sym != YY_STRING) goto error;
		str = parse_pp_string(rcc, &len);
		sym = yy_next(rcc);
		if (sym != YY__RPAREN) goto error;
		if (type == YY_OPTION) {
			rcc_parse_options(rcc, str, len);
//		} else if (type == YY_LIB) { // TODO: add support for #pragma( comment(lib, ...) ???
		} else {
			/* ignore without any warning */
		}
#ifdef _WIN32
	} else if (sym == YY_REGION || sym == YY_ENDREGION || sym == YY_WARNING) {
		/* silenttly ignore */
		pp_skip_until_eol(rcc);
		return;
#endif
	} else if (sym == YY_EOL) {
		yy_warning("ignoring \"#pragma\"");
		return;
	} else {
		yy_warning_ex_fmt(E_UNSUPPORTED, "ignoring unsupported \"#pragma %s\"", yy_sym2str(rcc, sym));
		pp_skip_until_eol(rcc);
		return;
	}

	sym = yy_next(rcc);
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "pragma");
		pp_skip_until_eol(rcc);
	}
	return;

error:
	yy_error("malformed #pragma directive");
}

static yy_sym pp_skip_block(rcc_ctx *rcc)
{
	int skip_level = 0;
	int ch;
	const unsigned char *pos;

	pos = (const unsigned char*)rcc->yy_pos;
	ch = *pos;
	goto start;
	while (1) {
		switch (ch) {
			case '\r':
cr:
				ch = *++pos;
				if (ch == '\n') ch = *++pos;
				goto new_line;
			case '\n':
lf:
				ch = *++pos;
new_line:
				rcc->yy_line++;
				rcc->yy_linepos = (const char*)pos;
start:
				while (ch == ' ' || ch == '\t' || ch == '\v') {
					ch = *++pos;
				}
				if (ch == '#') {
					ch = *++pos;
					while (ch == ' ' || ch == '\t' || ch == '\v') {
						ch = *++pos;
					}
					if (ch == 'i') {
						ch = *++pos;
						if (ch != 'f') break;
						ch = *++pos;
						if (ch == 'd') {
							ch = *++pos;
							if (ch != 'e') break;
							ch = *++pos;
							if (ch != 'f') break;
							ch = *++pos;
						} else if (ch == 'n') {
							ch = *++pos;
							if (ch != 'd') break;
							ch = *++pos;
							if (ch != 'e') break;
							ch = *++pos;
							if (ch != 'f') break;
							ch = *++pos;
						}
						if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') break;
						skip_level++;
					} else if (ch == 'e') {
						ch = *++pos;
						if (ch == 'n') {
							ch = *++pos;
							if (ch != 'd') break;
							ch = *++pos;
							if (ch != 'i') break;
							ch = *++pos;
							if (ch != 'f') break;
							ch = *++pos;
							if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') break;
							if (skip_level == 0) {
								rcc->yy_pos = (const char*)pos;
								return YY_ENDIF;
							}
							skip_level--;
						} else if (ch == 'l') {
							ch = *++pos;
							if (ch == 's') {
								ch = *++pos;
								if (ch != 'e') break;
								ch = *++pos;
								if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') break;
								if (skip_level == 0/* && !pp_ifdef_stack[pp_ifdef_level]*/) {
									rcc->yy_pos = (const char*)pos;
									return YY_ELSE;
								}
							} else if (ch == 'i') {
								ch = *++pos;
								if (ch != 'f') break;
								ch = *++pos;
								if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') break;
								if (skip_level == 0/* && !pp_ifdef_stack[pp_ifdef_level]*/) {
									rcc->yy_pos = (const char*)pos;
									return YY_ELIF;
								}
							}
						}
					}
				}
				break;
			case '\'':
				ch = *++pos;
				while (1) {
					if (ch == '\\') {
						ch = *++pos;
						if (ch == '\r') {
							ch = *++pos;
							if (ch == '\n') ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
						} else {
							ch = *++pos;
						}
					} else if (ch == '\'') {
						ch = *++pos;
						break;
					} else if (ch == '\r') {
						goto cr;
					} else if (ch == '\n') {
						goto lf;
					} else {
						ch = *++pos;
					}
				}
				break;
			case '"':
				ch = *++pos;
				while (1) {
					if (ch == '\\') {
						ch = *++pos;
						if (ch == '\r') {
							ch = *++pos;
							if (ch == '\n') ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
						} else {
							ch = *++pos;
						}
					} else if (ch == '"') {
						ch = *++pos;
						break;
					} else if (ch == '\r') {
						goto cr;
					} else if (ch == '\n') {
						goto lf;
					} else {
						ch = *++pos;
					}
				}
				break;
			case '/':
				ch = *++pos;
				if (ch == '/') {
					/* one line comments */
					ch = *++pos;
					while (1) {
						if (ch == '\r') {
							goto cr;
						} else if (ch == '\n') {
							goto lf;
						} else {
							ch = *++pos;
						}
					}
				} else if (ch == '*') {
					ch = *++pos;
					while (1) {
						if (ch == '*') {
							ch = *++pos;
							if (ch == '/') {
								ch = *++pos;
								break;
							}
						} else if (ch == '\r') {
							ch = *++pos;
							if (ch == '\n') ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							rcc->yy_line++;
							rcc->yy_linepos = (const char*)pos;
							if (ch == '\0') goto eof;
						} else {
							ch = *++pos;
						}
					}
				}
				break;
			case '\\':
				ch = *++pos;
				if (ch == '\r') {
					ch = *++pos;
					if (ch == '\n') ch = *++pos;
					rcc->yy_line++;
					rcc->yy_linepos = (const char*)pos;
				} else if (ch == '\n') {
					ch = *++pos;
					rcc->yy_line++;
					rcc->yy_linepos = (const char*)pos;
				} else {
					ch = *++pos;
				}
				break;
			case '\0':
eof:
				rcc->yy_pos = (const char*)pos - 1;
				return YY_EOF;
			default:
				ch = *++pos;
				break;
		}
	}
}

void pp_parse_directive(rcc_ctx *rcc)
{
	yy_sym sym;
	bool skip, is_true;
	uint32_t save_flags = rcc->yy_flags;
	bool start_of_include = (rcc->pp_include_ifndef_state & YY_INCLUDE_START) != 0;

	rcc->yy_flags &= ~YY_SKIP_EOL;
	rcc->yy_flags |= YY_SKIP_WS | YY_NO_MACRO | YY_ACCEPT_PUNCTUATOR | YY_NO_DIRECTIVE;
	sym = yy_next(rcc);
	while (1) {
		skip = 0;
		switch (sym) {
			case YY_IFNDEF:
			case YY_IFDEF:
				is_true = pp_eval_ifdef(rcc, sym == YY_IFDEF, start_of_include);
				if (rcc->pp_ifdef_level >= IFDEF_STACK_SIZE) yy_error("too many nested #if directives");
				rcc->pp_ifdef_stack[rcc->pp_ifdef_level++] = is_true ? IFDEF_HAD_TRUE : 0;
				skip = !is_true;
				break;
			case YY_IF:
				rcc->yy_flags &= ~YY_NO_MACRO;
				is_true = pp_eval_expr(rcc);
				rcc->yy_flags |= YY_NO_MACRO;
				if (rcc->pp_ifdef_level >= IFDEF_STACK_SIZE) yy_error("too many nested #if directives");
				rcc->pp_ifdef_stack[rcc->pp_ifdef_level] = is_true ? IFDEF_HAD_TRUE : 0;
				rcc->pp_ifdef_level++;
				skip = !is_true;
				break;
			case YY_ELIF:
				if (rcc->pp_ifdef_level == rcc->pp_include_ifdef_level) yy_error("#elif without #if");
				if (rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] & IFDEF_HAD_ELSE) yy_error("#elif after #else");
				if ((rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] & IFDEF_HAD_TRUE) == 0) {
					rcc->yy_flags &= ~YY_NO_MACRO;
					is_true = pp_eval_expr(rcc);
					rcc->yy_flags |= YY_NO_MACRO;
					rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] |= is_true ? IFDEF_HAD_TRUE : 0;
					skip = !is_true;
				} else {
					pp_skip_until_eol(rcc);
					skip = 1;
				}
				if (rcc->pp_include_ifndef_macro && rcc->pp_ifdef_level - 1 == rcc->pp_include_ifdef_level) {
					rcc->pp_include_ifndef_macro = 0;
				}
				break;
			case YY_ELSE:
				if (rcc->pp_ifdef_level == rcc->pp_include_ifdef_level) yy_error("#else without #if");
				if (rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] & IFDEF_HAD_ELSE) yy_error("#else after #else");
				skip = (rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] & IFDEF_HAD_TRUE) != 0;
				rcc->pp_ifdef_stack[rcc->pp_ifdef_level - 1] |= IFDEF_HAD_ELSE;
				sym = yy_next(rcc);
				if (sym != YY_EOL) {
					yy_warning("extra tokens at end of #else directive");
					pp_skip_until_eol(rcc);
				}
				if (rcc->pp_include_ifndef_macro && rcc->pp_ifdef_level - 1 == rcc->pp_include_ifdef_level) {
					rcc->pp_include_ifndef_macro = 0;
				}
				break;
			case YY_ENDIF:
				if (rcc->pp_ifdef_level == rcc->pp_include_ifdef_level) yy_error("#endif without #if");
				rcc->pp_ifdef_level--;
				sym = yy_next(rcc);
				if (sym != YY_EOL) {
					yy_warning("extra tokens at end of #endif directive");
					pp_skip_until_eol(rcc);
				}
				skip = 0;
				if (rcc->pp_include_ifndef_macro && rcc->pp_ifdef_level == rcc->pp_include_ifdef_level) {
					rcc->pp_include_ifndef_state = YY_INCLUDE_END;
				}
				break;
			case YY_ERROR:
			case YY_WARNING:
				rcc->yy_flags &= ~YY_NO_MACRO;
				pp_parse_error(rcc, sym == YY_WARNING);
				break;
			case YY_INCLUDE:
			case YY_INCLUDE_NEXT:
				rcc->yy_flags &= ~YY_NO_MACRO;
				rcc->pp_include_ifndef_state = 0;
				pp_parse_include(rcc, sym);
				rcc->yy_flags = save_flags;
				return;
			case YY_DEFINE:
				pp_parse_define(rcc);
				break;
			case YY_UNDEF:
				pp_parse_undef(rcc);
				break;
			case YY_LINE:
				rcc->yy_flags &= ~YY_NO_MACRO;
				pp_parse_line(rcc, yy_next(rcc));
				break;
			case YY_PRAGMA:
				pp_parse_pragma(rcc, 0);
				break;
			case YY_DECIMAL_NUMBER:
			case YY_OCTAL_NUMBER:
			case YY_PP_NUMBER:
				rcc->yy_flags &= ~YY_NO_MACRO;
				pp_parse_line(rcc, sym);
				break;
			case YY_EOL:
				break;
			case YY_EOF:
				rcc->yy_flags = save_flags;
				return;
			default:
				if (rcc->yy_flags & PP_ASM_COMMENTS) {
					pp_skip_asm_comments(rcc);
				} else {
					yy_warning("invalid preprocessing directive");
					pp_skip_until_eol(rcc);
				}
				break;
		}

		start_of_include = 0;
		if (!skip) {
			break;
		}
		sym = pp_skip_block(rcc);
	}

	rcc->yy_flags = save_flags;
}

#ifdef fwrite_unlocked
# define fputc  putc_unlocked
# define fwrite fwrite_unlocked
# define fflush fflush_unlocked
#endif

static void pp_print_line(rcc_ctx *rcc, FILE *f, int line, yy_sym name)
{
	char buf[16], *s;
	const char *str;
	size_t len;

	fwrite("# ", sizeof("# ")-1, 1, f);

	s = buf + sizeof(buf);
	len = 0;
	do {
		s--;
		len++;
		*s = '0' + line % 10;
		line = line / 10;
	} while (line != 0);
	fwrite(s, len, 1, f);

	fwrite(",\"", sizeof(",\"")-1, 1, f);
	str = yy_sym2strl(rcc, name, &len);
	fwrite(str, len, 1, f);
	fwrite("\"\n", sizeof("\"\n")-1, 1, f);
}

static void pp_debug_line(rcc_ctx *rcc, FILE *f)
{
	if (rcc->pp_out_level != rcc->pp_include_level
	 || rcc->pp_out_file_name != rcc->yy_file_name) {
		uint32_t i;

		if (rcc->pp_out_level < rcc->pp_include_level) {
			for (i = rcc->pp_out_level; i < rcc->pp_include_level; i++) {
				pp_print_line(rcc, f, rcc->pp_include_stack[i].line - 1, rcc->pp_include_stack[i].file_name);
			}
		}
		pp_print_line(rcc, f, rcc->yy_line, rcc->yy_file_name);
		rcc->pp_out_file_name = rcc->yy_file_name;
		rcc->pp_out_level = rcc->pp_include_level;
		rcc->pp_out_line = rcc->yy_line;
	} else if (rcc->pp_out_line != rcc->yy_line) {
		if (rcc->pp_out_line > rcc->yy_line || rcc->yy_line - rcc->pp_out_line > 4) {
			pp_print_line(rcc, f, rcc->yy_line, rcc->yy_file_name);
		} else {
			uint32_t i = rcc->yy_line - rcc->pp_out_line;
			for (;i > 0; i--) fputc('\n', f);
		}
		rcc->pp_out_line = rcc->yy_line;
	}
}

static void pp_debug_include(rcc_ctx *rcc, yy_sym inc_sym, const char *name, size_t name_len, bool is_user)
{
	FILE *f = rcc->pp_out_file ? rcc->pp_out_file : stdout;
	const char *str;
	size_t len;

	if (!(rcc->yy_flags & PP_NO_LINEMARKERS)) {
		if (rcc->pp_out_level != rcc->pp_include_level
		 || rcc->pp_out_file_name != rcc->yy_file_name
		 || rcc->pp_out_line != rcc->yy_line) {
			rcc->yy_line--;
			pp_debug_line(rcc, f);
			rcc->pp_out_line = ++rcc->yy_line;
		}
	}

	fputc('#', f);
	str = yy_sym2strl(rcc, inc_sym, &len);
	fwrite(str, len, 1, f);
	fputc(' ', f);
	if (is_user) {
		fputc('"', f);
		fwrite(name, name_len, 1, f);
		fputc('"', f);
	} else {
		fputc('<', f);
		fwrite(name, name_len, 1, f);
		fputc('>', f);
	}
	fputc('\n', f);
	rcc->pp_out_level++;
}

static void pp_debug_tokens(rcc_ctx *rcc, FILE *f, yy_sym *tokens, const pp_macro *macro)
{
	yy_sym sym, *p = tokens;
	yy_sym prev = YY_WS;

	while (1) {
		sym = *p++;
		if (sym == YY_EOF) break;
		if (sym == YY_EOL || sym == YY_WS) {
			prev = YY_WS;
			continue;
		}
		if (prev == YY_WS || pp_needs_space(prev, sym)) {
			fputc(' ', f);
		}
		prev = sym;
		if (PP_HAS_VAL(sym)) {
			p = pp_load_val(rcc, p);
			fwrite(rcc->yy_text, rcc->yy_len, 1, f);
		} else {
			size_t len;
			const char *str;

			if (macro) {
				if (sym & PP_MACRO_ARG) {
					if (sym & PP_STRINGIZE) {
						fputc('#', f);
					}
					sym = macro->tokens[sym & ~(PP_MACRO_ARG|PP_STRINGIZE)];
				}
			} else if (sym & PP_NOSUBST) {
				fwrite("<NOSUBST>", sizeof("<NOSUBST>")-1, 1, f);
				sym &= ~PP_NOSUBST;
			}
			str = yy_sym2strl(rcc, sym, &len);
			if (len == 1) {
				fputc(str[0], f);
			} else {
				fwrite(str, len, 1, f);
			}
		}
	}
}

static void pp_debug_macro(rcc_ctx *rcc, yy_sym sym, yy_sym name, pp_macro *macro)
{
	FILE *f = rcc->pp_out_file ? rcc->pp_out_file : stdout;
	const char *str;
	size_t len;

	if (!(rcc->yy_flags & PP_NO_LINEMARKERS)) {
		if (rcc->pp_out_level != rcc->pp_include_level
		 || rcc->pp_out_file_name != rcc->yy_file_name
		 || rcc->pp_out_line != rcc->yy_line) {
			rcc->yy_line--;
			pp_debug_line(rcc, f);
			rcc->pp_out_line = ++rcc->yy_line;
		}
	}

	if (sym == YY_DEFINE) {
		fwrite("#define ", sizeof("#define ")-1, 1, f);
		str = yy_sym2strl(rcc, name, &len);
		fwrite(str, len, 1, f);
		if (macro->flags & PP_MACRO_FUNCTION) {
			bool first = 1;
			int32_t i;

			fputc('(', f);
			for (i = 0; i < macro->num_args; i++) {
				if (!first) fputc(',', f);
				str = yy_sym2strl(rcc, macro->tokens[i], &len);
				fwrite(str, len, 1, f);
				first = 0;
			}
			if (macro->flags & PP_MACRO_VAR_ARG) {
				fwrite("...", sizeof("...")-1, 1, f);
			}
			fputc(')', f);
		}
		if (!(macro->flags & PP_MACRO_EMPTY)) {
			pp_debug_tokens(rcc, f, macro->tokens + macro->num_args, macro);
		}
		fputc('\n', f);
	} else if (sym == YY_UNDEF) {
		fwrite("#undef ", sizeof("#undef ")-1, 1, f);
		str = yy_sym2strl(rcc, name, &len);
		fwrite(str, len, 1, f);
		fputc('\n', f);
	}
}

static void pp_print_pragma(rcc_ctx *rcc, yy_sym sym)
{
	FILE *f = rcc->pp_out_file ? rcc->pp_out_file : stdout;
	yy_sym prev = YY_PRAGMA;

	if (!(rcc->yy_flags & PP_NO_LINEMARKERS)) {
		if (rcc->pp_out_level != rcc->pp_include_level
		 || rcc->pp_out_file_name != rcc->yy_file_name
		 || rcc->pp_out_line != rcc->yy_line) {
			rcc->yy_line--;
			pp_debug_line(rcc, f);
			rcc->pp_out_line = ++rcc->yy_line;
		}
	}

	fwrite("#pragma", sizeof("#pragma")-1, 1, f);

	while (sym != YY_EOL && sym != YY_EOF) {
		if (pp_needs_space(prev, sym)) fputc(' ', f);
		if (PP_HAS_VAL(sym)) {
			fwrite(rcc->yy_text, rcc->yy_len, 1, f);
		} else {
			size_t len;
			const char *str = yy_sym2strl(rcc, sym, &len);
			fwrite(str, len, 1, f);
		}
		prev = sym;
		sym = yy_next(rcc);
	}
	fputc('\n', f);
}

/* cpp -E */
void pp_preprocess(rcc_ctx *rcc, FILE *f)
{
	yy_sym sym, prev = 0;
	bool empty_line = 1;
	uint32_t spaces= 0;

	rcc->pp_out_file = f;

	rcc->pp_out_file_name = 0;
	rcc->pp_out_level = rcc->pp_include_level;
	rcc->pp_out_line = rcc->yy_line;

	if (rcc->yy_flags & PP_NO_OUTPUT) {
		while (1) {
			sym = yy_next(rcc);
			if (sym == YY_EOF) break;
		}
	} else {
		while (1) {
			sym = yy_next(rcc);
			if (sym == YY_WS) {
				if (prev != YY_WS) {
					spaces = (empty_line && rcc->yy_len) ? rcc->yy_len : 1;
					prev = YY_WS;
				} else if (empty_line) {
					spaces += rcc->yy_len;
				}
				continue;
			} else if (sym == YY_EOL) {
				if (!empty_line) {
					fputc('\n', f);
					rcc->pp_out_line++;
				}
				prev = YY_EOL;
				empty_line = 1;
				continue;
			} else if (sym == YY_EOF) {
				if (!empty_line) fputc('\n', f);
				break;
			} else if (!PP_HAS_VAL(sym)) {
				rcc->yy_text = yy_sym2strl(rcc, sym, &rcc->yy_len);
			} 

			if (!(rcc->yy_flags & PP_NO_LINEMARKERS)) {
				if (rcc->pp_out_level != rcc->pp_include_level
				 || rcc->pp_out_file_name != rcc->yy_file_name) {
					if (!empty_line) fputc('\n', f);
					pp_debug_line(rcc, f);
					prev = YY_EOL;
					empty_line = 1;
				} else if (rcc->pp_out_line != rcc->yy_line) {
					pp_debug_line(rcc, f);
					if (!empty_line || prev != YY_WS) prev = YY_EOL;
					empty_line = 1;
				}
			}

			if (prev == YY_WS) {
				uint32_t n = spaces;
				while (n-- > 0) fputc(' ', f);
			} else if (prev != YY_EOL && pp_needs_space(prev, sym)) {
				fputc(' ', f);
			}
			empty_line = 0;
			prev = ((sym == YY_PP_NUMBER || sym == YY_HEXADECIMAL_NUMBER)
				&& (rcc->yy_text[rcc->yy_len-1] == 'E' || rcc->yy_text[rcc->yy_len-1] == 'e')) ? YY_E : sym;
			if (rcc->yy_len == 1) {
				fputc(rcc->yy_text[0], f);
			} else {
				fwrite(rcc->yy_text, rcc->yy_len, 1, f);
			}
		}
	}

	fflush(f);
	if (rcc->pp_ifdef_level) yy_error("mising #endif");
}
