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
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#include <ir.h>
#include <ir_private.h>

#include "rcc.h"

//#define PP_DEBUG 1

#ifndef PP_DEBUG
# define PP_DEBUG 0
#endif

#define INCLUDE_STACK_SIZE   32
#define IFDEF_STACK_SIZE     256
#define PACK_STACK_SIZE      16

/* pp_ifdef_stack bits */
#define IFDEF_HAD_TRUE       (1<<0)
#define IFDEF_HAD_ELSE       (1<<1)

/* pp_include_ifndef_state and pp_include_state.state bits */
#define YY_INCLUDE_START     (1<<0)
#define YY_INCLUDE_END       (1<<1)

#define YY_PRAGMA_ONCE       1

typedef struct {
	const char              *pos;
	const char              *text;
	const char              *linepos;
	size_t                   len;
	int                      line;
	yy_sym                   file_name;
	const char              *buf;
	const char              *end;
	uint32_t                 if_level;
	uint32_t                 state;
	yy_sym                   macro;
} pp_include_state;

typedef pp_macro pp_arg;

static pp_include_state      pp_include_stack[INCLUDE_STACK_SIZE];
static uint8_t               pp_ifdef_stack[IFDEF_STACK_SIZE];

static uint32_t              pp_recursion_level = 0; /* used for debug only */

static uint32_t              pp_counter;              /* __COUNTER__ value */
       uint32_t              pp_ifdef_level;          /* ifdef nesting level */
       uint32_t              pp_include_level;        /* include nesting level */
static uint32_t              pp_include_ifdef_level;  /* ifdef nesting level for the current include */

       uint32_t              pp_include_ifndef_state; /* state to catch includes protected by #ifndef macro */
static yy_sym                pp_include_ifndef_macro; /* macro that protects the current include */
static ir_hashtab           *pp_include_hash = NULL;  /* map include file-name -> protection macro */

       pp_subst_stream       pp_subst_stack[PP_SUBST_STACK_SIZE];
       uint32_t              pp_subst_level = 0;

       pp_list               pp_list_cache[PP_LIST_CACHE_SIZE];
       uint32_t              pp_list_cache_idx = 0;

       uint8_t               pp_pack = 0;
static uint8_t               pp_pack_stack_pos = 0;
static uint8_t               pp_pack_stack[PACK_STACK_SIZE];

#if PP_DEBUG
# define pp_debug 1
static void pp_debug_print_context(void);
static void pp_debug_print_args(pp_macro *macro, pp_arg *args);
static void pp_debug_print_list(const char *hdr, yy_sym *tokens);
# define pp_debug_fprintf(file, format...) fprintf(file, format)
#else
# define pp_debug 0
# define pp_debug_print_context()
# define pp_debug_print_args(macro, args)
# define pp_debug_print_list(hdr, tokens)
# define pp_debug_fprintf(file, format...)
#endif

#define PP_ASSERT(condition, message) \
	IR_ASSERT((condition) && message)

#ifndef IS_ABSPATH
# ifdef _WIN32
#  define IS_DIRSEP(c) (c == '/' || c == '\\')
#  define IS_ABSPATH(p) (IS_DIRSEP(p[0]) || (p[0] != 0 && p[1] == ':' && IS_DIRSEP(p[2])))
# else
#  define IS_ABSPATH(p) (p[0] == '/')
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

#define PP_MAX_INCLUDE_PATHS 31

static int pp_include_paths_count = 0;

const char *pp_include_paths[PP_MAX_INCLUDE_PATHS + 1] = {
	NULL
};

const char *pp_sys_include_paths[] = {
	"/usr/local/include",
	"/usr/include",
#ifdef __linux__
# if defined(IR_TARGET_X64)
	"/usr/include/x86_64-linux-gnu",
# elif defined(IR_TARGET_X86)
	"/usr/include/x86-linux-gnu",
# elif defined(IR_TARGET_AARCH64)
	"/usr/include/aarch64-linux-gnu",
# endif
#endif
	NULL
};

bool pp_add_include_dir(const char *path)
{
	if (pp_include_paths_count >= PP_MAX_INCLUDE_PATHS) return 0;
	pp_include_paths[pp_include_paths_count++] = path;
	return 1;
}

void pp_start(void)
{
	pp_recursion_level = 0;
	pp_counter = 0;
	pp_ifdef_level = 0;
	pp_include_level = 0;
	pp_include_ifdef_level = 0;
	pp_include_ifndef_state = 0; /* don't set YY_INCLUDE_START -> don't detect "#ifndef X" in the main file */
	pp_include_ifndef_macro = 0;
	pp_include_hash = NULL;
	pp_subst_level = 0;
	pp_pack = 0;
	pp_pack_stack_pos = 0;
}

void pp_dtor(void)
{
	if (pp_include_hash) {
		ir_hashtab_free(pp_include_hash);
		ir_mem_free(pp_include_hash);
		pp_include_hash = NULL;
	}
}

static bool pp_macro_is_defined(yy_sym id)
{
	return yy_hash.data[id].macro != NULL;
}

/* Dynamic Strings */
void yy_dyn_str_init(yy_dyn_str *dyn_str, const char *str, size_t len)
{
	dyn_str->str = ir_arena_alloc(&yy_arena, len);
	yy_arena->ptr = dyn_str->str + len;
	dyn_str->len = len;
	memcpy(dyn_str->str, str, len);
}

void yy_dyn_str_init0(yy_dyn_str *dyn_str, const char *str, size_t len)
{
	dyn_str->str = ir_arena_alloc(&yy_arena, len + 1);
	yy_arena->ptr = dyn_str->str + len + 1;
	dyn_str->len = len;
	memcpy(dyn_str->str, str, len);
	dyn_str->str[len] = 0;
}

char *yy_dyn_str_grow(yy_dyn_str *dyn_str, size_t len)
{
	IR_ASSERT(yy_arena && dyn_str->str + dyn_str->len == yy_arena->ptr);
	if (len >= (size_t)(yy_arena->end - yy_arena->ptr)) {
		size_t size = dyn_str->len + len < 4096 - IR_ALIGNED_SIZE(sizeof(ir_arena), 8) ?
			4096 : IR_ALIGNED_SIZE(dyn_str->len + len + IR_ALIGNED_SIZE(sizeof(ir_arena), 8), 4096);
		if (dyn_str->str == (char*)yy_arena + IR_ALIGNED_SIZE(sizeof(ir_arena), 8)) {
			yy_arena = ir_mem_realloc(yy_arena, size);
			dyn_str->str = (char*)yy_arena + IR_ALIGNED_SIZE(sizeof(ir_arena), 8);
			yy_arena->ptr = dyn_str->str + dyn_str->len;
			yy_arena->end = (char*)yy_arena + size;
		} else {
			yy_arena->ptr -= dyn_str->len;
			char *new_str = ir_arena_alloc(&yy_arena, size);
			yy_arena->ptr = new_str + dyn_str->len;
			memcpy(new_str, dyn_str->str, dyn_str->len);
			dyn_str->str = new_str;
		}
	}

	char *tail = yy_arena->ptr;
	yy_arena->ptr += len;
	return tail;
}

void yy_dyn_str_append(yy_dyn_str *dyn_str, const char *str, size_t len)
{
	char *tail = yy_dyn_str_grow(dyn_str, len);
	memcpy(tail, str, len);
	dyn_str->len += len;
}

void yy_dyn_str_append0(yy_dyn_str *dyn_str, const char *str, size_t len)
{
	yy_dyn_str_append(dyn_str, str, len + 1);
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
static void pp_debug_tokens(FILE *f, yy_sym *tokens);
static void pp_debug_include(const char *name, bool is_user);
static void pp_debug_macro(yy_sym sym, yy_sym name, pp_macro *macro);

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

void pp_macro_define(yy_sym name, uint32_t flags, uint32_t num_args, yy_sym *tokens)
{
	pp_macro *macro;

	PP_ASSERT(PP_IS_ID(name), "<ID> expected");

	macro = ir_arena_alloc(&yy_arena, sizeof(pp_macro));
	macro->flags = flags;
	macro->num_args = num_args;
	macro->tokens = tokens;

	yy_hash.data[name].macro = macro;

	if ((yy_flags & PP_DUMP_MACROS) && !(flags & PP_MACRO_BUILTIN)) {
		pp_debug_macro(YY_DEFINE, name, macro);
	}
}

static void pp_macro_undef(yy_sym name)
{
	PP_ASSERT(PP_IS_ID(name), "<ID> expected");
	yy_hash.data[name].macro = NULL;

	if (yy_flags & PP_DUMP_MACROS) {
		pp_debug_macro(YY_UNDEF, name, NULL);
	}
}

#if PP_DEBUG
static void pp_debug_print_context(void)
{
	uint32_t i;

	fprintf(stderr, "%*s  Context: ", pp_recursion_level * 2, "");
	for (i = pp_subst_level; i > 0;) {
		i--;
		pp_debug_tokens(stderr, pp_subst_stack[i].tokens);
		fprintf(stderr, "<EOF>");
	}
	fprintf(stderr, "\n");
}

static void pp_debug_print_args(pp_macro *macro, pp_arg *args)
{
	int i;
	bool first = 1;

	fprintf(stderr, "%*s  Arguments: (", pp_recursion_level * 2, "");
	for (i = 0; i < macro->num_args; i++) {
		if (!first) fprintf(stderr, ",");
		first = 0;
		pp_debug_tokens(stderr, args[i].tokens);
	}
	fprintf(stderr, ")\n");
}

static void pp_debug_print_list(const char *hdr, yy_sym *tokens)
{
	fprintf(stderr, "%*s  %s: ", pp_recursion_level * 2, "", hdr);
	pp_debug_tokens(stderr, tokens);
	fprintf(stderr, "\n");
}
#endif

static int pp_macro_find_arg(pp_macro *macro, yy_sym id)
{
	yy_sym *arg = macro->tokens;
	int i;

	for (i = 0; i < macro->num_args; arg++, i++) {
		if (*arg == id) {
			return i;
		}
	}
	return -1;
}

static yy_sym pp_paste(yy_sym sym1, const char *s1, size_t len1, yy_sym sym2, const char *s2, size_t len2)
{
	yy_dyn_str dyn_str;
	yy_sym sym;

	IR_ASSERT(sym1 > YY_WS && sym2 > YY_WS);
	if (sym1 == YY_PP_PLACE_MARKER) {
		return sym2;
	} else if (sym2 == YY_PP_PLACE_MARKER) {
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
				yy_dyn_str_init(&dyn_str, s1, len1);
				yy_dyn_str_append0(&dyn_str, s2, len2);
				sym = yy_hash_lookup(dyn_str.str, dyn_str.len);
				return sym;
			}
		} else if (sym2 == YY_STRING && s2[0] == '"'
				&& ((len1 == 1 && (s1[0] == 'L' || s1[0] == 'U' || s1[0] == 'u'))
				 || (len1 == 2 && s1[0] == 'u' && s1[1] == '8'))) {
			yy_dyn_str_init(&dyn_str, s1, len1);
			yy_dyn_str_append0(&dyn_str, s2, len2);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			return YY_STRING;
		} else if (sym2 == YY_CHARACTER && s2[0] == '\''
				&& len1 == 1 && (s1[0] == 'L' || s1[0] == 'U' || s1[0] == 'u')) {
			yy_dyn_str_init(&dyn_str, s1, len1);
			yy_dyn_str_append0(&dyn_str, s2, len2);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			return YY_CHARACTER;
		}
	} else if (sym1 >= YY_DECIMAL_NUMBER && sym1 <= YY_PP_NUMBER) {
		if ((sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER) || PP_IS_ID(sym2) || sym2 == YY__POINT
		 || ((sym2 == YY__PLUS || sym2 == YY__MINUS)
		  && (s1[len1-1] == 'e' || s1[len1-1] == 'E' || s1[len1-1] == 'p' || s1[len1-1] == 'P'))) {
			yy_dyn_str_init(&dyn_str, s1, len1);
			yy_dyn_str_append0(&dyn_str, s2, len2);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			return YY_PP_NUMBER;
		}
	} else if (sym1 == YY__POINT && sym2 >= YY_DECIMAL_NUMBER && sym2 <= YY_PP_NUMBER) {
		yy_dyn_str_init(&dyn_str, s1, len1);
		yy_dyn_str_append0(&dyn_str, s2, len2);
		yy_text = dyn_str.str;
		yy_len = dyn_str.len;
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

static void pp_macro_join(yy_sym *tokens)
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
				pp_load_val(prev + 1);
				s1 = yy_text;
				len1 = yy_len;
			} else {
				s1 = yy_sym2strl(*prev & ~PP_NOSUBST, &len1);
			}
			if (PP_HAS_VAL(sym)) {
				pp_load_val(src);
				s2 = yy_text;
				len2= yy_len;
			} else {
				sym &= ~PP_NOSUBST;
				s2 = yy_sym2strl(sym, &len2);
			}
			next = sym;
			sym = pp_paste(*prev, s1, len1, next, s2, len2);
			if (sym) {
				*prev = sym;
				dst = prev + 1;
				if (PP_HAS_VAL(sym)) {
					IR_ASSERT(PP_HAS_VAL(*prev) || PP_HAS_VAL(next));
					dst = pp_save_val(dst);
				}
				if (PP_HAS_VAL(next)) src += (sizeof(void*) == sizeof(int32_t)) ? 2 : 3;
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

static void pp_macro_stringize(yy_sym *tokens)
{
	yy_dyn_str dyn_str;
	int spaces = 0;

	yy_dyn_str_init(&dyn_str, "\"", 1);
	while (1) {
		yy_sym sym = *tokens++;
		if (sym == YY_EOF) break;
		if (sym == YY_WS || sym == YY_EOL) {
			spaces++;
			continue;
		}
		while (spaces) {
			yy_dyn_str_append(&dyn_str, " ", 1);
			spaces--;
		}
		if (sym == YY_PP_PLACE_MARKER) {
			continue;
		} else {
			if (PP_HAS_VAL(sym)) {
				tokens = pp_load_val(tokens);
				if (sym == YY_STRING || sym == YY_CHARACTER) {
					const char *s = yy_text;
					while (yy_len) {
						char c = *s;
					    if ((c < 32 && c != '\t') || c == '\"' || c == '\\') {
							yy_dyn_str_append(&dyn_str, "\\", 1);
					    }
					    if (c >= 32 || c == '\t' /*&& c <= 126*/) {
							yy_dyn_str_append(&dyn_str, s, 1);
					    } else {
					        if (c == '\n') {
					            yy_dyn_str_append(&dyn_str, "n", 1);
					        } else {
								char buf[4];

					            buf[0] = '0' + ((c >> 6) & 7);
					            buf[1] = '0' + ((c >> 3) & 7);
					            buf[2] = '0' + (c & 7);
								yy_dyn_str_append(&dyn_str, buf, 3);
					        }
					    }
						s++;
						yy_len--;
					}
				}
			} else {
				yy_text = yy_sym2strl(sym & ~PP_NOSUBST, &yy_len);
			}
			yy_dyn_str_append(&dyn_str, yy_text, yy_len);
		}
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
			yy_arena->ptr--;
		}
	}
	yy_dyn_str_append0(&dyn_str, "\"", 1);

	yy_text = dyn_str.str;
	yy_len = dyn_str.len;
}

static void pp_macro_subst_args(pp_macro *macro, pp_arg *args, pp_list *replacement)
{
	int i;
	yy_sym prev = 0, prev2 = 0;
	yy_sym *macro_tokens = macro->tokens + macro->num_args;

	while (1) {
		int arg;
		yy_sym sym = *macro_tokens++;

		if (sym == YY_EOF) {
			break;
		} else if (sym == YY__HASH) {
			sym = *macro_tokens++;
			PP_ASSERT(PP_IS_ID(sym), "'#' is not followed by a macro parameter");
			arg = pp_macro_find_arg(macro, sym);
			PP_ASSERT(arg >= 0, "'#' is not followed by a macro parameter");

			pp_macro_stringize(args[arg].tokens);
			pp_list_push(replacement, YY_STRING);
			pp_list_push_val(replacement);
		} else if (PP_IS_ID(sym)) {
			arg = pp_macro_find_arg(macro, sym);
			if (arg >= 0) {
				yy_sym *tokens = args[arg].tokens;

				if (!(args[arg].flags & PP_MACRO_EXPANDED)
				 && prev != YY_PP_JOIN
				 && *macro_tokens != YY_PP_JOIN) {
					pp_list expansion;
					uint32_t old_level = pp_subst_level;
					pp_subst_stream *stream = &pp_subst_stack[pp_subst_level];

					if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
					stream = &pp_subst_stack[pp_subst_level];
					pp_subst_level++;
					stream->macro = NULL;
					stream->start = NULL;
					stream->tokens = tokens;
					stream->skip_eof = 0;

					pp_list_init(&expansion);
					while (1) {
						sym = *stream->tokens++;
						if (sym == YY_EOF) {
							pp_subst_level--;
							if (pp_subst_level == old_level) break;
							if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
							if (stream->start) pp_list_release(stream->start, stream->size);
							stream = &pp_subst_stack[pp_subst_level - 1];
							continue;
						}
						if (PP_IS_ID(sym) && !(sym & PP_NOSUBST)) {
							pp_macro *macro = yy_hash.data[sym].macro;

							if (macro) {
								if (!(macro->flags & PP_MACRO_DISABLED)) {
									if (pp_macro_expand(macro, sym)) {
										stream = &pp_subst_stack[pp_subst_level - 1];
										continue;
									}
									stream = &pp_subst_stack[pp_subst_level - 1];
								} else {
									if (pp_debug) {pp_debug_fprintf(stderr, "\"%s\" is disabled!\n", yy_sym2str(sym));}
									sym |= PP_NOSUBST;
								}
							}
						}
						pp_list_push(&expansion, sym);
						if (PP_HAS_VAL(sym)) {
							stream->tokens = pp_load_val(stream->tokens);
							pp_list_push_val(&expansion);
						}
					}
					if (args[arg].flags & PP_MACRO_EXPANDED) {
						pp_list_release(expansion.syms, expansion.size);
					    tokens = args[arg].tokens;
					} else {
						pp_list_push(&expansion, YY_EOF);

						args[arg].flags |= PP_MACRO_EXPANDED;
						args[arg].size = expansion.size;
						args[arg].tokens = tokens = expansion.syms;
					}
				} else if (prev == YY_PP_JOIN && prev2 == YY__COMMA) {
					/* GNU extension: remove ", ##" or replace it by "," */
					if (*tokens == YY_EOF || *tokens == YY_PP_PLACE_MARKER) {
						replacement->len -= 2;
					} else {
						replacement->len -= 1;
					}
				}

				if (*tokens == YY_EOF) {
					/* empty arg - insert placemarker */
					pp_list_push(replacement, YY_PP_PLACE_MARKER);
				} else
				while (1) {
					sym = *tokens++;
					if (sym == YY_EOF) break;
					if (sym == YY_WS && prev == YY_WS) continue;
					prev2 = prev;
					prev = sym;
					pp_list_push(replacement, sym);
					if (PP_HAS_VAL(sym)) {
						tokens = pp_load_val(tokens);
						pp_list_push_val(replacement);
					}
				}
			} else {
				pp_list_push(replacement, sym);
			}
		} else {
			if (sym == YY_WS && prev == YY_WS) continue;
			pp_list_push(replacement, sym);
			if (PP_HAS_VAL(sym)) {
				macro_tokens = pp_load_val(macro_tokens);
				pp_list_push_val(replacement);
			}
		}
		prev2 = prev;
		prev = sym;
	}

	for (i = 0; i < macro->num_args; i++) {
		if (args[i].flags & PP_MACRO_EXPANDED) {
			pp_list_release(args[i].tokens, args[i].size);
		}
	}
}

static void pp_macro_read_args(pp_macro *macro, yy_sym name, pp_arg *args, pp_list *list)
{
	uint32_t level = 0;
	int num_args = 0;
	yy_sym sym, prev = 0;
	bool skip_ws = 1;
	uint32_t save_flags = yy_flags;

	yy_flags |= YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR | YY_NO_MACRO;
	yy_flags &= ~YY_SKIP_WS;

	args[0].num_args = 0;
	while (1) {
		do {
			sym = yy_next();
		} while ((skip_ws && (sym == YY_WS || sym == YY_EOL)) || sym == YY_PP_PLACE_MARKER);
		skip_ws = 0;
		if (sym == YY_EOL) sym = YY_WS;
		if (sym == YY_WS && prev == YY_WS) {
			continue;
		} else
		if (sym == YY__RPAREN) {
			if (level == 0) {
				num_args++;
				break;
			}
			level--;
		} else if (sym == YY__COMMA && level == 0) {
			num_args++;
			if (num_args < macro->num_args) {
				pp_list_push(list, YY_EOF);
				skip_ws = 1;
				args[num_args].num_args = list->len;
			} else if (macro->flags & PP_MACRO_VAR_ARG) {
				pp_list_push(list, sym);
			}
			continue;
		} else if (sym == YY__LPAREN) {
			level++;
		} else if (sym == YY_EOF) {
			yy_error("unexpected <EOF>");
			break;
		}
		prev = sym;
		if (PP_IS_ID(sym)
		 && !(sym & PP_NOSUBST)
		 && yy_hash.data[sym].macro
		 && (yy_hash.data[sym].macro->flags & PP_MACRO_DISABLED)) {
			sym |= PP_NOSUBST;
		}
		pp_list_push(list, sym);
		if (PP_HAS_VAL(sym)) {
			pp_list_push_val(list);
		}
	}

	yy_flags = save_flags;

	if (num_args < macro->num_args) {
		if ((macro->flags & PP_MACRO_VAR_ARG) && num_args == macro->num_args - 1) {
			/* empty variadic argument */
			args[num_args].num_args = list->len;
		} else {
			yy_error_fmt("macro \"%s\" requires %d arguments, but only %d given",
				yy_sym2str(name), macro->num_args, num_args);
		}
	} else if (num_args > macro->num_args && (macro->flags & PP_MACRO_VAR_ARG) == 0) {
		if (macro->num_args == 0 && num_args == 1 && list->len == 0) {
			/* Function macro withiout parameters */
		} else {
			yy_error_fmt("macro \"%s\" passed %d arguments, but takes just %d",
				yy_sym2str(name), num_args, macro->num_args);
		}
	} else if (num_args == macro->num_args
	 && (macro->flags & PP_MACRO_VAR_ARG)
	 && num_args != 1
	 && (uint32_t)args[num_args - 1].num_args == list->len) {
		/* __VA_ARGS__ is not empty, COMMA ## __VA_ARGS__ should be expaned to COMMA */
		pp_list_push(list, YY_WS);
	}

	pp_list_push(list, YY_EOF);

	for (num_args = 0; num_args <  macro->num_args; num_args++) {
		args[num_args].tokens = list->syms + args[num_args].num_args;
		args[num_args].flags = 0;
		args[num_args].num_args = 0;
	}
}

bool pp_macro_expand(pp_macro *macro, yy_sym name)
{
	yy_sym sym;
	pp_list replacement;
	uint32_t save_flags = yy_flags;

	yy_flags |= YY_ACCEPT_NOSUBST;
	replacement.syms = NULL;
	replacement.size = 0;
	replacement.len = 0;

	pp_recursion_level++;
	if (macro->flags & PP_MACRO_FUNCTION) {
		pp_list tmp;
		pp_arg *args;
		uint32_t ws = 0;

		if (pp_debug) {
			pp_debug_fprintf(stderr, "%*sExpand function macro: \"%s\" %d %d\n",
				pp_recursion_level * 2, "", yy_sym2str(name), pp_recursion_level, pp_subst_level);
			pp_debug_print_context();
		}

		yy_flags |= YY_NO_MACRO;
		sym = yy_next();
		while (sym == YY_WS || sym == YY_EOL) {
			if (sym == YY_WS) ws |= 1;
			if (sym == YY_EOL) ws |= 2;
			sym = yy_next();
		}
		yy_flags = save_flags | YY_ACCEPT_NOSUBST;
		if (sym != YY__LPAREN) {
			/* not a function macro, backtrack */
			if (ws || sym != YY_EOF) {
				pp_list_init(&replacement);

				if (ws & 2) {
					pp_list_push(&replacement, YY_EOL);
				} else if (ws & 1) {
					pp_list_push(&replacement, YY_WS);
				}
				if (sym != YY_EOF) {
					pp_list_push(&replacement, sym);
					if (PP_HAS_VAL(sym)) {
						pp_list_push_val(&replacement);
					}
				}

				pp_list_push(&replacement, YY_EOF);
				if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
				pp_subst_stack[pp_subst_level].macro = NULL;
				pp_subst_stack[pp_subst_level].size = replacement.size;
				pp_subst_stack[pp_subst_level].start = replacement.syms;
				pp_subst_stack[pp_subst_level].tokens = replacement.syms;
				pp_subst_stack[pp_subst_level].skip_eof = 1;
				pp_subst_level++;
			}

			if (pp_debug) {pp_debug_fprintf(stderr, "%*s  Backtrack\n", pp_recursion_level * 2, "");}

			pp_recursion_level--;
			yy_flags = save_flags;
			return 0;
		}

		args = alloca(sizeof(pp_arg) * (macro->num_args + 1)); /* "+ 1" for macro->num_args == 0 case */
		pp_list_init(&tmp);

		pp_macro_read_args(macro, name, args, &tmp);

		if (pp_debug) {pp_debug_print_args(macro, args);}

		if (macro->flags & PP_MACRO_EMPTY) {
			pp_list_release(tmp.syms, tmp.size);
			pp_recursion_level--;
			yy_flags = save_flags;
			return 1;
		}

		pp_list_init(&replacement);
		pp_macro_subst_args(macro, args, &replacement);
		pp_list_release(tmp.syms, tmp.size);
	} else if (macro->flags & PP_MACRO_BUILTIN) {
		if (name == YY___COUNTER__ || name == YY___INCLUDE_LEVEL__) {
			char buf[16];
			int i = sizeof(buf);
			uint32_t n;
			yy_dyn_str dyn_str;

			if (name == YY___COUNTER__) {
				n = pp_counter++;
			} else {
				n = pp_include_level;
			}
			buf[--i] = 0;
			do {
				buf[--i] = '0' + n % 10;
				n = n / 10;
			} while (n != 0);
			yy_dyn_str_init(&dyn_str, buf + i, sizeof(buf) - i);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len - 1;
			pp_list_init(&replacement);
			pp_list_push(&replacement, YY_DECIMAL_NUMBER);
			pp_list_push_val(&replacement);
		} else if (name == YY___DATE__ || name == YY___TIME__) {
			time_t t;
			struct tm *tm;
			size_t len;
			yy_dyn_str dyn_str;
			char str[64];

			time(&t);
			tm = localtime(&t);
			if (name == YY___DATE__) {
				len = strftime(str, sizeof(str), "\"%b %d %Y\"", tm);
			} else {
				len = strftime(str, sizeof(str), "\"%H:%M:%S\"", tm);
			}
			yy_dyn_str_init0(&dyn_str, str, len);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			pp_list_init(&replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(&replacement);
		} else if (name == YY___FILE__ || name == YY___BASE_FILE__) {
			yy_dyn_str dyn_str;
			size_t len;
			const char *str;

			if (name == YY___FILE__ || pp_include_level == 0) {
				str = yy_sym2strl(yy_file_name, &len);
			} else {
				str = yy_sym2strl(pp_include_stack[pp_include_level].file_name, &len);
			}

			// TODO: intern the quoted string ???
			yy_dyn_str_init(&dyn_str, "\"", 1);
			yy_dyn_str_append(&dyn_str, str, len);
			yy_dyn_str_append0(&dyn_str, "\"", 1);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			pp_list_init(&replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(&replacement);
		} else if (name == YY___FUNCTION__ || name == YY___FUNC__) {
			yy_dyn_str dyn_str;
			yy_sym func_name = c_get_current_func_name();

			yy_dyn_str_init(&dyn_str, "\"", 1);
			if (func_name) {
				size_t len;
				const char *name = yy_sym2strl(func_name, &len);
				yy_dyn_str_append(&dyn_str, name, len);
			}
			yy_dyn_str_append0(&dyn_str, "\"", 1);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len;
			pp_list_init(&replacement);
			pp_list_push(&replacement, YY_STRING);
			pp_list_push_val(&replacement);
		} else if (name == YY___LINE__) {
			char buf[16];
			int i = sizeof(buf);
			uint32_t n = yy_line;
			yy_dyn_str dyn_str;

			buf[--i] = 0;
			do {
				buf[--i] = '0' + n % 10;
				n = n / 10;
			} while (n != 0);
			yy_dyn_str_init(&dyn_str, buf + i, sizeof(buf) - i);
			yy_text = dyn_str.str;
			yy_len = dyn_str.len - 1;
			pp_list_init(&replacement);
			pp_list_push(&replacement, YY_DECIMAL_NUMBER);
			pp_list_push_val(&replacement);
		} else {
			yy_error_fmt("bad builtin macro \"%.*s\"", yy_len, yy_text);
		}
	} else if (macro->flags & PP_MACRO_EMPTY) {
		pp_recursion_level--;
		yy_flags = save_flags;
		return 1;
	} else {
		yy_sym *tokens = macro->tokens;

		if (pp_debug) {pp_debug_fprintf(stderr, "%*sExpand object macro: \"%s\"\n", pp_recursion_level * 2, "", yy_sym2str(name));}

		if (macro->flags & PP_MACRO_HAS_JOIN) {
			pp_list_init(&replacement);
			while (1) {
				sym = *tokens++;
				if (sym == YY_EOF) break;
				pp_list_push(&replacement, sym);
				if (PP_HAS_VAL(sym)) {
					tokens = pp_load_val(tokens);
					pp_list_push_val(&replacement);
				}
			}
		} else {
			if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
			macro->flags |= PP_MACRO_DISABLED;
			pp_subst_stack[pp_subst_level].macro = macro;
			pp_subst_stack[pp_subst_level].start = NULL;
			pp_subst_stack[pp_subst_level].tokens = tokens;
			pp_subst_stack[pp_subst_level].skip_eof = 1;
			pp_subst_level++;

			pp_recursion_level--;
			yy_flags = save_flags;
			return 1;
		}
	}

	if (replacement.len != 0) {
		yy_sym *tokens;

		pp_list_push(&replacement, YY_EOF);
		tokens = replacement.syms;

		if (pp_debug) {pp_debug_print_list("Replacement", tokens);}

		if (macro->flags & PP_MACRO_HAS_JOIN) {
			pp_macro_join(tokens);

			if (pp_debug) {pp_debug_print_list("Joined Replacement", tokens);}
		}

		if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
		macro->flags |= PP_MACRO_DISABLED;
		pp_subst_stack[pp_subst_level].macro = macro;
		pp_subst_stack[pp_subst_level].size = replacement.size;
		pp_subst_stack[pp_subst_level].start = tokens;
		pp_subst_stack[pp_subst_level].tokens = tokens;
		pp_subst_stack[pp_subst_level].skip_eof = 1;
		pp_subst_level++;
	} else {
		pp_list_release(replacement.syms, replacement.size);
	}

	pp_recursion_level--;
	yy_flags = save_flags;
	return 1;
}

static void pp_skip_until_eol(void)
{
	yy_sym sym;

	do {
		sym = yy_next();
	} while (sym != YY_EOL);
}

static bool pp_eval_ifdef(bool ifdef)
{
	yy_sym id, sym = yy_next();

	if (!PP_IS_ID(sym)) {
		if (sym == YY_EOL) {
			yy_error("mising macro name");
		} else {
			yy_error("macro name must be an identifier");
			pp_skip_until_eol();
		}
		return !(ifdef ^ 0);
	}

	id = sym;
	pp_include_ifndef_macro = ((pp_include_ifndef_state & YY_INCLUDE_START) && !ifdef) ? id : 0;
	sym = yy_next();
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", ifdef ? "ifdef" : "ifndef");
		pp_skip_until_eol();
	}

	return !(ifdef ^ (pp_macro_is_defined(id) || id == YY___HAS_INCLUDE));
}

static void pp_push_include(yy_sym file_name, const char *buf, size_t size)
{
	IR_ASSERT(pp_include_level < INCLUDE_STACK_SIZE);
	pp_include_stack[pp_include_level].pos       = yy_pos;
	pp_include_stack[pp_include_level].text      = yy_text;
	pp_include_stack[pp_include_level].linepos   = yy_linepos;
	pp_include_stack[pp_include_level].len       = yy_len;
	pp_include_stack[pp_include_level].line      = yy_line;
	pp_include_stack[pp_include_level].buf       = yy_buf;
	pp_include_stack[pp_include_level].end       = yy_end;
	pp_include_stack[pp_include_level].file_name = yy_file_name;
	pp_include_stack[pp_include_level].if_level  = pp_include_ifdef_level;
	pp_include_stack[pp_include_level].state     = pp_include_ifndef_state;
	pp_include_stack[pp_include_level].macro     = pp_include_ifndef_macro;
	pp_include_level++;

	yy_pos = yy_text = yy_linepos = yy_buf = buf;
	yy_len = 0;
	yy_line = 1;
	yy_end = buf + size;
	yy_file_name = file_name;

	pp_include_ifdef_level = pp_ifdef_level;
	pp_include_ifndef_state = YY_INCLUDE_START;
	pp_include_ifndef_macro = 0;
}

void pp_pop_include(void)
{
	IR_ASSERT(pp_include_level > 0);

	if (pp_include_ifdef_level != pp_ifdef_level) {
		yy_error("missign #endif");
	}

	if (pp_include_ifndef_state & YY_INCLUDE_END) {
		if (!pp_include_hash) {
			pp_include_hash = ir_mem_malloc(sizeof(ir_hashtab));
			ir_hashtab_init(pp_include_hash, 32);
		}
		ir_hashtab_add(pp_include_hash, yy_file_name, pp_include_ifndef_macro);

		char buf[PATH_MAX];
		char *path_str = realpath(yy_sym2str(yy_file_name), buf);
		if (path_str) {
			yy_sym path_sym = yy_hash_lookup(path_str, strlen(path_str));
			if (path_sym != yy_file_name) {
				ir_hashtab_add(pp_include_hash, path_sym, pp_include_ifndef_macro);
			}
			if (path_str != buf) free(path_str);
		}
	}

	ir_mem_free((void*)yy_buf);

	pp_include_level--;
	yy_pos       = pp_include_stack[pp_include_level].pos;
	yy_text      = pp_include_stack[pp_include_level].text;
	yy_linepos   = pp_include_stack[pp_include_level].linepos;
	yy_len       = pp_include_stack[pp_include_level].len ;
	yy_line      = pp_include_stack[pp_include_level].line;
	yy_buf       = pp_include_stack[pp_include_level].buf;
	yy_end       = pp_include_stack[pp_include_level].end;
	yy_file_name = pp_include_stack[pp_include_level].file_name;
	pp_include_ifdef_level  = pp_include_stack[pp_include_level].if_level;
	pp_include_ifndef_state = pp_include_stack[pp_include_level].state;
	pp_include_ifndef_macro = pp_include_stack[pp_include_level].macro;
}

static const char *pp_read_file(yy_sym file_name, int fd, size_t *size_ptr)
{
	size_t size, ret;
	char *buf;
	struct stat stat_buf;

	if (fstat(fd, &stat_buf) != 0) {
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(file_name));
		return NULL;
	}
	size = stat_buf.st_size;

	buf = ir_mem_malloc(size + 1);
	if (!buf) {
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(file_name));
		return NULL;
	}

	ret = read(fd, buf, size);
	if (ret != size) {
		ir_mem_free(buf);
		yy_error_fmt("cannot read file \"%s\"", yy_sym2str(file_name));
		return NULL;
	}
	buf[size] = '\0'; /* End marker */

	*size_ptr = size;
	return buf;
}

static yy_sym pp_find_included_ex(yy_sym resolved_name)
{
	yy_sym macro_name = ir_hashtab_find(pp_include_hash, resolved_name);

	if (macro_name != IR_INVALID_VAL
	 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(macro_name))) {
		return resolved_name;
	}

	return 0;
}

static yy_sym pp_find_included(const char *name, size_t len)
{
	yy_sym macro_name, resolved_name = yy_hash_find(name, len);

	if (resolved_name) {
		macro_name = ir_hashtab_find(pp_include_hash, resolved_name);
		if (macro_name != IR_INVALID_VAL
		 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(macro_name))) {
			return resolved_name;
		}
	}

	return 0;
}

static bool pp_find_included_realpath(const char *name)
{
	char buf[PATH_MAX];
	char *path = realpath(name, buf);
	yy_sym macro_name, resolved_name;

	if (path) {
		resolved_name = yy_hash_find(path, strlen(path));
		if (resolved_name) {
			macro_name = ir_hashtab_find(pp_include_hash, resolved_name);
			if (macro_name != IR_INVALID_VAL
			 && (macro_name == YY_PRAGMA_ONCE || pp_macro_is_defined(macro_name))) {
				if (path != buf) free(path);
				return 1;
			}
		}
		if (path != buf) free(path);
	}
	return 0;
}

static yy_sym pp_find_include(yy_dyn_str *name, bool is_user, const char **buf_ptr, size_t *size_ptr)
{
	int fd;
	int i;
	yy_sym resolved_name;

	if (IS_ABSPATH(name->str)) {
		if (pp_include_hash) {
			resolved_name = pp_find_included(name->str, name->len);
			if (resolved_name) {
				if (buf_ptr) *buf_ptr = NULL;
				return resolved_name;
			}
		}
		fd = open(name->str, O_RDONLY | O_BINARY);
		if (fd >= 0) {
			resolved_name = yy_hash_lookup(name->str, name->len);
			if (pp_include_hash && pp_find_included_realpath(name->str)) {
				close(fd);
				if (buf_ptr) *buf_ptr = NULL;
				return resolved_name;
			}
			goto read_file;
		}
	} else {
		if (is_user) {
			size_t len, j;
			const char *file_name = yy_sym2strl(yy_file_name, &len);

			for (j = len; j > 0; j--) {
				if (file_name[j-1] == '/') {
					break;
				}
			}
			if (j == 0) {
				if (pp_include_hash) {
					resolved_name = pp_find_included(name->str, name->len);
					if (resolved_name) {
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
				}
				fd = open(name->str, O_RDONLY | O_BINARY);
				if (fd >= 0) {
					resolved_name = yy_hash_lookup(name->str, name->len);
					if (pp_include_hash && pp_find_included_realpath(name->str)) {
						close(fd);
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
					goto read_file;
				}
			} else {
				yy_dyn_str buf;
				void *checkpoint = ir_arena_checkpoint(yy_arena);
				yy_dyn_str_init(&buf, file_name, j);
				yy_dyn_str_append0(&buf, name->str, name->len);
				if (pp_include_hash) {
					resolved_name = pp_find_included(buf.str, buf.len);
					if (resolved_name) {
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
				}
				fd = open(buf.str, O_RDONLY | O_BINARY);
				if (fd >= 0) {
					resolved_name = yy_hash_lookup(buf.str, buf.len);
					if (pp_include_hash && pp_find_included_realpath(buf.str)) {
						close(fd);
						if (buf_ptr) *buf_ptr = NULL;
						return resolved_name;
					}
					goto read_file;
				}
				ir_arena_release(&yy_arena, checkpoint);
			}
		}

		resolved_name = yy_hash_find(name->str, name->len);
		if (resolved_name) {
			size_t len;
			const char *content = c_stdinc_find(resolved_name, &len);

			if (content) {
				if (pp_include_hash) {
					if (pp_find_included_ex(resolved_name)) {
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

		for (i = 0; pp_include_paths[i]; i++) {
			yy_dyn_str buf;
			void *checkpoint = ir_arena_checkpoint(yy_arena);
			yy_dyn_str_init(&buf, pp_include_paths[i], strlen(pp_include_paths[i]));
			yy_dyn_str_append(&buf, "/", 1);
			yy_dyn_str_append0(&buf, name->str, name->len);
			if (pp_include_hash) {
				resolved_name = pp_find_included(buf.str, buf.len);
				if (resolved_name) {
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
			}
			fd = open(buf.str, O_RDONLY | O_BINARY);
			if (fd >= 0) {
				resolved_name = yy_hash_lookup(buf.str, buf.len);
				if (pp_include_hash && pp_find_included_realpath(buf.str)) {
					close(fd);
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
				goto read_file;
			}
			ir_arena_release(&yy_arena, checkpoint);
		}

		for (i = 0; pp_sys_include_paths[i]; i++ ) {
			yy_dyn_str buf;
			void *checkpoint = ir_arena_checkpoint(yy_arena);
			yy_dyn_str_init(&buf, pp_sys_include_paths[i], strlen(pp_sys_include_paths[i]));
			yy_dyn_str_append(&buf, "/", 1);
			yy_dyn_str_append0(&buf, name->str, name->len);
			if (pp_include_hash) {
				resolved_name = pp_find_included(buf.str, buf.len);
				if (resolved_name) {
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
			}
			fd = open(buf.str, O_RDONLY | O_BINARY);
			if (fd >= 0) {
				resolved_name = yy_hash_lookup(buf.str, buf.len);
				if (pp_include_hash && pp_find_included_realpath(buf.str)) {
					close(fd);
					if (buf_ptr) *buf_ptr = NULL;
					return resolved_name;
				}
				goto read_file;
			}
			ir_arena_release(&yy_arena, checkpoint);
		}
	}

	if (buf_ptr) *buf_ptr = NULL;
	return 0;

read_file:
	if (buf_ptr) *buf_ptr = pp_read_file(resolved_name, fd, size_ptr);
	close(fd);
	return resolved_name;
}

static void pp_parse_include(void)
{
	yy_sym sym;
	yy_dyn_str name;
	bool is_user;

	while (1) {
		sym = yy_next();
		if (sym == YY_STRING) {
			yy_dyn_str_init0(&name, yy_text + 1, yy_len - 2);
			is_user = 1;
			sym = yy_next();
			break;
		} else if (sym == YY__LESS) {
			if (!pp_subst_level) {
				const char *save_yy_pos = yy_pos;

				while (1) {
					char ch = *yy_pos++;
					if (ch == '>') {
						break;
					} else if (ch == '\0' || ch == '\r' || ch == '\n') {
						yy_pos = save_yy_pos;
						goto try_expand;
//						yy_error("missing terminating > character");
					}
				}
				yy_len = yy_pos - yy_text;
				yy_dyn_str_init0(&name, yy_text + 1, yy_len - 2);
				is_user = 0;
				sym = yy_next();
				break;
			} else {
				pp_list list;
				yy_sym *tokens;

try_expand:
				pp_list_init(&list);
				while (1) {
					sym = yy_next();
					if (sym == YY__GREATER) {
						pp_list_push(&list, YY_EOF);
						break;
					} else if (sym == YY_EOL) {
						yy_error("missing terminating > character");
					} else {
						pp_list_push(&list, sym);
						if (PP_HAS_VAL(sym)) {
							pp_list_push_val(&list);
						}
					}
				}

				yy_dyn_str_init(&name, "", 0);
				tokens = list.syms;
				while (*tokens) {
					sym = *tokens;
					tokens++;
					if (PP_HAS_VAL(sym)) {
						tokens = pp_load_val(tokens);
					} else {
						yy_text = yy_sym2strl(sym, &yy_len);
					}
					yy_dyn_str_append(&name, yy_text, yy_len);
				}
				yy_dyn_str_append0(&name, "", 0);
				pp_list_release(list.syms, list.size);

				sym = yy_next();
				is_user = 0;
				break;
			}
			break;
		} else {
			yy_error("#include expects \"FILENAME\" or <FILENAME>");
			return;
		}
	}

	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "include");
		pp_skip_until_eol();
	}

	if (pp_include_level >= INCLUDE_STACK_SIZE) yy_error("too deep include level");

	const char *buf;
	size_t size;

	yy_sym resolved_name = pp_find_include(&name, is_user, &buf, &size);

	if (!resolved_name) {
		yy_error_fmt("%.*s: No such file or directory", (int)name.len, name.str);
	}

	if (!buf) {
		return;
	}

	if (yy_flags & PP_DUMP_INCLUDES) {
		pp_debug_include(name.str, is_user);
	}

	pp_push_include(resolved_name, buf, size);
}

static bool pp_eval_has_include(void)
{
	yy_sym sym;
	yy_dyn_str name;
	bool is_user;

	sym = yy_next();
	if (sym != YY__LPAREN) yy_error("'(' expected");
	while (1) {
		sym = yy_next();
		if (sym == YY_STRING) {
			yy_dyn_str_init0(&name, yy_text + 1, yy_len - 2);
			is_user = 1;
			sym = yy_next();
			break;
		} else if (sym == YY__LESS) {
			if (!pp_subst_level) {
				while (1) {
					char ch = *yy_pos++;
					if (ch == '>') {
						break;
					} else if (ch == '\0' || ch == '\r' || ch == '\n') {
						yy_error("missing terminating > character");
					}
				}
				yy_len = yy_pos - yy_text;
				yy_dyn_str_init0(&name, yy_text + 1, yy_len - 2);
				is_user = 0;
				sym = yy_next();
				break;
			} else {
				yy_dyn_str_init(&name, "", 0);
				while (1) {
					sym = yy_next();
					if (sym == YY__GREATER) {
						yy_dyn_str_append0(&name, "", 0);
						break;
					} else if (sym == YY_EOL) {
						yy_error("missing terminating > character");
					} else {
						yy_dyn_str_append(&name, yy_text, yy_len);
					}
				}
				sym = yy_next();
				is_user = 0;
				break;
			}
			break;
		} else {
			yy_error("expected \"FILENAME\" or <FILENAME>");
			return 0;
		}
	}

	if (sym != YY__RPAREN) yy_error("')' expected");

	return pp_find_include(&name, is_user, NULL, NULL) != 0;
}

static bool pp_eval_expr(void)
{
	yy_sym sym;
	pp_list tokens;
	bool ret;

	pp_list_init(&tokens);
	while (1) {
		sym = yy_next();
next:
		if (sym == YY_EOL) {
			break;
		} else if (sym == YY_DEFINED) {
			yy_sym id;
			uint32_t save_flags = yy_flags;

			yy_flags |= YY_NO_MACRO | YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
			id = yy_next();
			if (id == YY__LPAREN) {
				id = yy_next();
				if (!PP_IS_ID(id)) yy_error("??");
				sym = yy_next();
				if (sym != YY__RPAREN) yy_error("??");
			} else if (!PP_IS_ID(id)) {
				yy_error("??");
			}
			yy_flags = save_flags;
			yy_text = (pp_macro_is_defined(id) || id == YY___HAS_INCLUDE) ? "1" : "0";
			yy_len = 1;
			pp_list_push(&tokens, YY_DECIMAL_NUMBER);
			pp_list_push_val(&tokens);
		} else if (sym == YY___HAS_INCLUDE) {
			uint32_t save_flags = yy_flags;
			bool ret;

			yy_flags |= YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
			ret = pp_eval_has_include();
			yy_flags = save_flags;
			yy_text = ret ? "1" : "0";
			yy_len = 1;
			pp_list_push(&tokens, YY_DECIMAL_NUMBER);
			pp_list_push_val(&tokens);
		} else if (PP_IS_ID(sym)) {
			/* undefined macro */
			yy_sym sym2;
			uint32_t save_flags = yy_flags;

			yy_flags |= YY_NO_MACRO;
			sym2 = yy_next();
			yy_flags = save_flags;
			if (sym2 == YY__LPAREN) {
				pp_list_push(&tokens, sym);
			} else {
				yy_text = "0";
				yy_len = 1;
				pp_list_push(&tokens, YY_DECIMAL_NUMBER);
				pp_list_push_val(&tokens);
			}
			sym = sym2;
			goto next;
		} else {
			pp_list_push(&tokens, sym);
			if (PP_HAS_VAL(sym)) {
				pp_list_push_val(&tokens);
			}
		}
	}
	pp_list_push(&tokens, YY_EOF);

	if (pp_debug) {
		pp_debug_fprintf(stderr, "Evaluate expression: ");
		pp_debug_tokens(stderr, (yy_sym*)tokens.syms);
		pp_debug_fprintf(stderr, "\n");
	}

	if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
	pp_subst_stack[pp_subst_level].macro = NULL;
	pp_subst_stack[pp_subst_level].start = NULL;
	pp_subst_stack[pp_subst_level].tokens = tokens.syms;
	pp_subst_stack[pp_subst_level].skip_eof = 0;
	pp_subst_level++;

	ret = parse_pp_expr();

	pp_subst_level--;
	pp_list_release(tokens.syms, tokens.size);

	if (pp_debug) {pp_debug_fprintf(stdout, "#res %d\n", ret);}

	return ret;
}

static bool pp_macro_same(pp_macro *macro, uint32_t flags, int32_t num_args, yy_sym *tokens)
{
	yy_sym *s1, *s2, sym1, sym2;
	const char *text;
	size_t len;

	if (macro->flags != flags || macro->num_args != num_args) return 0;
	if (macro->flags & PP_MACRO_EMPTY) return 1;

	s1 = macro->tokens + num_args;
	s2 = tokens + num_args;
	while (1) {
		sym1 = *s1++;
		sym2 = *s2++;
		if (sym1 != sym2) {
#if 0
			//int n;
			//if (!PP_IS_ID(sym1) || !PP_IS_ID(sym2)) return 0;
			//n = pp_macro_find_arg(macro, sym1);
			//if (n < 0 || tokens[n] != sym2) return 0;
#else
			return 0;
#endif
		} else if (sym1 == YY_EOF) {
			return 1;
		} else if (PP_HAS_VAL(sym1)) {
			s1 = pp_load_val(s1);
			len = yy_len;
			text = yy_text;
			s2 = pp_load_val(s2);
			if (len != yy_len || memcmp(text, yy_text, len) != 0) {
				return 0;
			}
		}
	}
}

static void pp_parse_define(void)
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
	const char *define_linepos = yy_linepos;

	sym = yy_next();
	if (PP_IS_ID(sym)) {
		end = yy_pos;
	} else if (sym == YY_EOL) {
		yy_linepos = define_linepos;
		yy_pos = yy_text;
		yy_line--;
		yy_error("no macro name given in #define directive");
		return;
	} else {
		yy_error("macro names must be identifiers");
		return;
	}

	id = sym;
	if (id == YY_DEFINED) yy_error("\"defined\" cannot be used as a macro name");

	id_line = yy_line;
	id_text = yy_text;
	id_linepos = yy_linepos;

	save_flags = yy_flags;
	yy_flags |= YY_NO_MACRO | YY_ACCEPT_PP_NUMBER | YY_ACCEPT_PUNCTUATOR;
	tokens.syms = NULL;

	sym = yy_next();
	if (sym == YY__LPAREN && yy_text == end) {
		flags |= PP_MACRO_FUNCTION;
		/* parse function macro parameters */
		sym = yy_next();
		while (sym != YY__RPAREN && sym != YY__POINT_POINT_POINT ) {
			if (!PP_IS_ID(sym)) {
				if (sym == YY_EOL) {
					yy_linepos = define_linepos;
					yy_pos = yy_text;
					yy_line--;
					yy_error("expected parameter name before end of line");
				} else {
					yy_error_fmt("expected parameter name, found \"%s\"", yy_sym2str(sym));
				}
			}
			if (sym == YY___VA_ARGS__) yy_warning("__VA_ARGS__ can only appear in the expansion of a C99 variadic macro");
			if (!num_args) {
				pp_list_init(&tokens);
			} else {
				uint32_t n = num_args;

				do {
					n--;
					if (tokens.syms[n] == sym) {
						yy_error_fmt("duplicate macro parameter \"%.*s\"", (int)yy_len, yy_text);
					}
				} while (n > 0);
			}
			num_args++;
			pp_list_push(&tokens, sym);
			sym = yy_next();
			if (sym == YY__POINT_POINT_POINT) {
				flags |= PP_MACRO_VAR_ARG;
				sym = yy_next();
				break;
			} else {
				if (sym != YY__COMMA) break;
				sym = yy_next();
			}
		}
		if (sym == YY__POINT_POINT_POINT && !(flags & PP_MACRO_VAR_ARG)) {
			if (!num_args) {
				pp_list_init(&tokens);
			} else {
				uint32_t n = num_args;

				do {
					n--;
					if (tokens.syms[n] == YY___VA_ARGS__) {
						yy_error_fmt("duplicate macro parameter \"__VA_ARGS__\"");
					}
				} while (n > 0);
			}
			num_args++;
			pp_list_push(&tokens, YY___VA_ARGS__);
			flags |= PP_MACRO_VAR_ARG;
			sym = yy_next();
		}
		if (sym != YY__RPAREN) {
			if (sym == YY_EOL) {
				yy_linepos = define_linepos;
				yy_pos = yy_text;
				yy_line--;
				yy_error("expected ')' before end of line");
			} else if (flags & PP_MACRO_VAR_ARG) {
				yy_error("expected ')' after \"...\"");
			} else {
				yy_error_fmt("expected ',' or ')', found \"%s\"", yy_sym2str(sym));
			}
		}
		sym = yy_next();
	}

	/* parse macro replacement tokens */
	if (sym != YY_EOL) {
		yy_sym prev = YY_EOF;

		if (!num_args) {
			pp_list_init(&tokens);
		}
		yy_flags &= ~YY_SKIP_WS;
		do {
			if (sym == YY__HASH) {
				if (flags & PP_MACRO_FUNCTION) {
					int j, arg = -1;

					pp_list_push(&tokens, sym);
					do {
						sym = yy_next();
					} while (sym == YY_WS);
					if (!PP_IS_ID(sym)) yy_error("'#' is not followed by a macro parameter");
					for (j = 0; j < num_args; j++) {
						if (tokens.syms[j] == sym) {
							arg = j;
							break;
						}
					}
					if (arg < 0) yy_error("'#' is not followed by a macro parameter");
				}
			} else if (sym == YY__HASH_HASH) {
				if (prev == YY_EOF) yy_error("##' cannot appear at either end of a macro expansion");
				if (prev == YY_WS) {
					tokens.len--;
				}
				do {
					sym = yy_next();
				} while (sym == YY_WS || sym == YY__HASH_HASH);
				if (sym == YY_EOL) yy_error("##' cannot appear at either end of a macro expansion");
				flags |= PP_MACRO_HAS_JOIN;
				pp_list_push(&tokens, YY_PP_JOIN);
			} else if (sym == YY_WS && prev == YY_WS) {
				goto skip;
			}
			prev = sym;
			pp_list_push(&tokens, sym);
			if (PP_HAS_VAL(sym)) {
				yy_dyn_str dyn_str;
				yy_dyn_str_init0(&dyn_str, yy_text, yy_len);
				yy_text = dyn_str.str;
				yy_len = dyn_str.len;
				pp_list_push_val(&tokens);
			}
skip:
			sym = yy_next();
		} while (sym != YY_EOL);
		if (prev == YY_WS) {
			tokens.len--;
		}
		pp_list_push(&tokens, YY_EOF);
	} else {
		flags |= PP_MACRO_EMPTY;
	}

	yy_flags = save_flags;

	old = yy_hash.data[id].macro;
	if (old) {
		if (pp_macro_same(old, flags, num_args, tokens.syms)) {
			if (tokens.syms) pp_list_release(tokens.syms, tokens.size);
			return;
		} else {
			int save_line = yy_line;
			const char *save_text = yy_text;
			const char *save_linepos = yy_linepos;

			yy_line = id_line;
			yy_text = id_text;
			yy_linepos = id_linepos;
			yy_warning_fmt("\"%s\" redefined", yy_sym2str(id));
			yy_line = save_line;
			yy_text = save_text;
			yy_linepos = save_linepos;
		}
	}

	yy_sym *p = NULL;
	if (tokens.syms) {
		p = ir_arena_alloc(&yy_arena, sizeof(yy_sym) * tokens.len);
		memcpy(p, tokens.syms, sizeof(yy_sym) * tokens.len);
		pp_list_release(tokens.syms, tokens.size);
	}
	pp_macro_define(id, flags, num_args, p);
}

static void pp_parse_undef(void)
{
	yy_sym id, sym = yy_next();

	if (!PP_IS_ID(sym)) {
		if (sym == YY_EOL) {
			yy_error("mising macro name");
		} else {
			yy_error("macro name must be an identifier");
			pp_skip_until_eol();
		}
		return;
	}

	id = sym;
	if (id == YY_DEFINED) yy_error("\"defined\" cannot be used as a macro name");

	sym = yy_next();
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "undef");
		pp_skip_until_eol();
	}

	pp_macro_undef(id);
}

// #error pp-tokens? new-line
static void pp_parse_error(bool warning)
{
	yy_sym sym = yy_next();
	const char *msg, *end = yy_pos;

	msg = yy_text;
	if (sym != YY_EOL) {
		uint32_t save_flags = yy_flags;

		yy_flags &= ~YY_SKIP_WS;
		while (1) {
			sym = yy_next();
			if (sym == YY_EOL) break;
			if (sym != YY_WS) end = yy_pos;
		}
		yy_flags = save_flags;
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
static void pp_parse_line(yy_sym sym)
{
	if (sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_PP_NUMBER) {
		const char *s = yy_text;
		const char *e = s + yy_len;
		uint32_t n = 0;
		while (s != e && *s >= '0' && *s <= '9') {
			n = n * 10 + (*s - '0');
			s++;
		}
		yy_line = n - 1;
		sym = yy_next();
		if (sym == YY_STRING) {
			yy_file_name = yy_hash_lookup(yy_text + 1, yy_len - 2);
			sym = yy_next();
		}
		if (sym != YY_EOL) {
			pp_skip_until_eol();
		}
	} else {
		yy_error("#line directive requires a positive integer argument");
	}
}

static void pp_parse_pragma(void)
{
	yy_sym name, sym = yy_next();

	if (sym == YY_ONCE) {
		if (!pp_include_hash) {
			pp_include_hash = ir_mem_malloc(sizeof(ir_hashtab));
			ir_hashtab_init(pp_include_hash, 32);
		}
		ir_hashtab_add(pp_include_hash, yy_file_name, YY_PRAGMA_ONCE);

		char buf[PATH_MAX];
		char *path_str = realpath(yy_sym2str(yy_file_name), buf);
		if (path_str) {
			yy_sym path_sym = yy_hash_lookup(path_str, strlen(path_str));
			if (path_sym != yy_file_name) {
				ir_hashtab_add(pp_include_hash, path_sym, YY_PRAGMA_ONCE);
			}
			if (path_str != buf) free(path_str);
		}

		pp_include_ifndef_state = 0;
		pp_include_ifndef_macro = 0;
	} else if (sym == YY_PUSH_MACRO) {
		pp_macro_list *p;

		sym = yy_next();
		if (sym != YY__LPAREN) goto error;
		sym = yy_next();
		if (sym != YY_STRING) goto error;
		name = yy_hash_lookup(yy_text + 1, yy_len - 2);
		sym = yy_next();
		if (sym != YY__RPAREN) goto error;
		p = ir_arena_alloc(&yy_arena, sizeof(pp_macro_list));
		p->macro = yy_hash.data[name].macro;
		p->next = yy_hash.data[name].macro_stack;
		yy_hash.data[name].macro_stack = p;
	} else if (sym == YY_POP_MACRO) {
		pp_macro_list *p;

		sym = yy_next();
		if (sym != YY__LPAREN) goto error;
		sym = yy_next();
		if (sym != YY_STRING) goto error;
		name = yy_hash_lookup(yy_text + 1, yy_len - 2);
		sym = yy_next();
		if (sym != YY__RPAREN) goto error;
		p = yy_hash.data[name].macro_stack;
		if (p) {
			yy_hash.data[name].macro = p->macro;
			yy_hash.data[name].macro_stack = p->next;
		} else {
			yy_warning_fmt("pragma pop_macro could not pop \"%s\"", yy_sym2str(name));
		}
	} else if (yy_flags & PP_PREPROCESS) {
		pp_list tokens;

		pp_list_init(&tokens);
		pp_list_push(&tokens, YY_EOL);
		pp_list_push(&tokens, YY__HASH);
		pp_list_push(&tokens, YY_PRAGMA);
		pp_list_push(&tokens, sym);
		if (PP_HAS_VAL(sym)) pp_list_push_val(&tokens);
		pp_list_push(&tokens, YY_EOF);

		if (pp_subst_level >= PP_SUBST_STACK_SIZE) yy_error("too deep macro substitution level");
		pp_subst_stack[pp_subst_level].macro = NULL;
		pp_subst_stack[pp_subst_level].size = tokens.size;
		pp_subst_stack[pp_subst_level].start = tokens.syms;
		pp_subst_stack[pp_subst_level].tokens = tokens.syms;
		pp_subst_stack[pp_subst_level].skip_eof = 1;
		pp_subst_level++;
		return;
	} else if (sym == YY_PACK) {
		sym = yy_next();
		if (sym != YY__LPAREN) goto error;
		sym = yy_next();
		if (sym == YY_OCTAL_NUMBER || sym == YY_DECIMAL_NUMBER || sym == YY_PP_NUMBER) {
pack_set:
			const char *s = yy_text;
			const char *e = s + yy_len;
			uint32_t n = 0;

			while (s != e && *s >= '0' && *s <= '9') {
				n = n * 10 + (*s - '0');
				s++;
			}
            if (n < 1 || n > 16 || (n & (n - 1)) != 0) yy_error_fmt("invalid \"pragma pack(%d)\" value", n);
            pp_pack = n;
			sym = yy_next();
		} else if (sym == YY_PUSH) {
			if (pp_pack_stack_pos < PACK_STACK_SIZE) {
				pp_pack_stack[pp_pack_stack_pos++] = pp_pack;
			} else {
				yy_error("too deep \"#pragma pack (push)\" level");
			}
			sym = yy_next();
			if (sym == YY__COMMA) {
				sym = yy_next();
				if (sym != YY_OCTAL_NUMBER && sym != YY_DECIMAL_NUMBER && sym != YY_PP_NUMBER) goto error;
				goto pack_set;
			}
		} else if (sym == YY_POP) {
			if (pp_pack_stack_pos) {
				pp_pack = pp_pack_stack[--pp_pack_stack_pos];
			} else {
				yy_warning("\"#pragma pack (pop)\" encountered without matching \"#pragma pack (push)\"");
			}
			sym = yy_next();
		} else {
			// default
			pp_pack = 0;
		}
		if (sym != YY__RPAREN) goto error;
	} else if (sym == YY_EOL) {
		yy_warning("ignoring \"#pragma\"");
		return;
	} else {
		yy_warning_fmt("ignoring \"#pragma %s\"", yy_sym2str(sym));
		pp_skip_until_eol();
		return;
	}

	sym = yy_next();
	if (sym != YY_EOL) {
		yy_warning_fmt("extra tokens at the end of #%s directive", "pragma");
		pp_skip_until_eol();
	}
	return;

error:
	yy_error("malformed #pragma directive");
}

static yy_sym pp_skip_block(void)
{
	int skip_level = 0;
	int ch;
	const unsigned char *pos;

	pos = (const unsigned char*)yy_pos;
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
				yy_line++;
				yy_linepos = (const char*)pos;
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
								yy_pos = (const char*)pos;
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
									yy_pos = (const char*)pos;
									return YY_ELSE;
								}
							} else if (ch == 'i') {
								ch = *++pos;
								if (ch != 'f') break;
								ch = *++pos;
								if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') break;
								if (skip_level == 0/* && !pp_ifdef_stack[pp_ifdef_level]*/) {
									yy_pos = (const char*)pos;
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
							yy_line++;
							yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							yy_line++;
							yy_linepos = (const char*)pos;
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
							yy_line++;
							yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							yy_line++;
							yy_linepos = (const char*)pos;
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
							yy_line++;
							yy_linepos = (const char*)pos;
						} else if (ch == '\n') {
							ch = *++pos;
							yy_line++;
							yy_linepos = (const char*)pos;
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
					yy_line++;
					yy_linepos = (const char*)pos;
				} else if (ch == '\n') {
					ch = *++pos;
					yy_line++;
					yy_linepos = (const char*)pos;
				} else {
					ch = *++pos;
				}
				break;
			case '\0':
eof:
				yy_pos = (const char*)pos - 1;
				return YY_EOF;
			default:
				ch = *++pos;
				break;
		}
	}
}

void pp_parse_directive(void)
{
	yy_sym sym;
	bool skip, is_true;
	uint32_t save_flags = yy_flags;

	yy_flags &= ~YY_SKIP_EOL;
	yy_flags |= YY_SKIP_WS | YY_NO_MACRO | YY_ACCEPT_PUNCTUATOR;
	sym = yy_next();
	while (1) {
		skip = 0;
		switch (sym) {
			case YY_IFNDEF:
			case YY_IFDEF:
				is_true = pp_eval_ifdef(sym == YY_IFDEF);
				if (pp_ifdef_level >= IFDEF_STACK_SIZE) yy_error("too many nested #if directives");
				pp_ifdef_stack[pp_ifdef_level++] = is_true ? IFDEF_HAD_TRUE : 0;
				skip = !is_true;
				break;
			case YY_IF:
				yy_flags &= ~YY_NO_MACRO;
				is_true = pp_eval_expr();
				yy_flags |= YY_NO_MACRO;
				if (pp_ifdef_level >= IFDEF_STACK_SIZE) yy_error("too many nested #if directives");
				pp_ifdef_stack[pp_ifdef_level] = is_true ? IFDEF_HAD_TRUE : 0;
				pp_ifdef_level++;
				skip = !is_true;
				break;
			case YY_ELIF:
				if (pp_ifdef_level == pp_include_ifdef_level) yy_error("#elif without #if");
				if (pp_ifdef_stack[pp_ifdef_level - 1] & IFDEF_HAD_ELSE) yy_error("#elif after #else");
				if ((pp_ifdef_stack[pp_ifdef_level - 1] & IFDEF_HAD_TRUE) == 0) {
					yy_flags &= ~YY_NO_MACRO;
					is_true = pp_eval_expr();
					yy_flags |= YY_NO_MACRO;
					pp_ifdef_stack[pp_ifdef_level - 1] |= is_true ? IFDEF_HAD_TRUE : 0;
					skip = !is_true;
				} else {
					pp_skip_until_eol();
					skip = 1;
				}
				if (pp_include_ifndef_macro && pp_ifdef_level - 1 == pp_include_ifdef_level) {
					pp_include_ifndef_macro = 0;
				}
				break;
			case YY_ELSE:
				if (pp_ifdef_level == pp_include_ifdef_level) yy_error("#else without #if");
				if (pp_ifdef_stack[pp_ifdef_level - 1] & IFDEF_HAD_ELSE) yy_error("#else after #else");
				skip = (pp_ifdef_stack[pp_ifdef_level - 1] & IFDEF_HAD_TRUE) != 0;
				pp_ifdef_stack[pp_ifdef_level - 1] |= IFDEF_HAD_ELSE;
				sym = yy_next();
				if (sym != YY_EOL) {
					yy_warning("extra tokens at end of #else directive");
					pp_skip_until_eol();
				}
				if (pp_include_ifndef_macro && pp_ifdef_level - 1 == pp_include_ifdef_level) {
					pp_include_ifndef_macro = 0;
				}
				break;
			case YY_ENDIF:
				if (pp_ifdef_level == pp_include_ifdef_level) yy_error("#endif without #if");
				pp_ifdef_level--;
				sym = yy_next();
				if (sym != YY_EOL) {
					yy_warning("extra tokens at end of #endif directive");
					pp_skip_until_eol();
				}
				skip = 0;
				if (pp_include_ifndef_macro && pp_ifdef_level == pp_include_ifdef_level) {
					pp_include_ifndef_state = YY_INCLUDE_END;
				}
				break;
			case YY_ERROR:
			case YY_WARNING:
				yy_flags &= ~YY_NO_MACRO;
				pp_parse_error(sym == YY_WARNING);
				break;
			case YY_INCLUDE:
				yy_flags &= ~YY_NO_MACRO;
				pp_parse_include();
				break;
			case YY_DEFINE:
				pp_parse_define();
				break;
			case YY_UNDEF:
				pp_parse_undef();
				break;
			case YY_LINE:
				yy_flags &= ~YY_NO_MACRO;
				pp_parse_line(yy_next());
				break;
			case YY_PRAGMA:
				pp_parse_pragma();
				break;
			case YY_DECIMAL_NUMBER:
			case YY_OCTAL_NUMBER:
			case YY_PP_NUMBER:
				yy_flags &= ~YY_NO_MACRO;
				pp_parse_line(sym);
				break;
			case YY_EOL:
				break;
			case YY_EOF:
				yy_flags = save_flags;
				return;
			default:
				yy_warning("invalid preprocessing directive");
				pp_skip_until_eol();
				break;
		}

		pp_include_ifndef_state = 0;
		if (!skip) {
			break;
		}
		sym = pp_skip_block();
	}

	yy_flags = save_flags;
}

static yy_sym      out_file_name = 0;
static uint32_t    out_level = 0;
static int32_t     out_line = 0;
static FILE       *out_file = NULL;

static void pp_debug_line(FILE *f)
{
	if (out_level != pp_include_level || out_file_name != yy_file_name) {
		uint32_t i;

		if (out_level < pp_include_level) {
			for (i = out_level; i < pp_include_level; i++) {
				fprintf(f, "# %d \"%s\"\n", pp_include_stack[i].line - 1, yy_sym2str(pp_include_stack[i].file_name));
			}
		}
		fprintf(f, "# %d \"%s\"\n", yy_line, yy_sym2str(yy_file_name));
		out_file_name = yy_file_name;
		out_level = pp_include_level;
		out_line = yy_line;
	} else if (out_line != yy_line) {
		if (out_line > yy_line || yy_line - out_line > 4) {
			fprintf(f, "# %d,\"%s\"\n", yy_line, yy_sym2str(yy_file_name));
		} else {
			uint32_t i = yy_line - out_line;
			for (;i > 0; i--) fputc('\n', f);
		}
		out_line = yy_line;
	}
}

static void pp_debug_include(const char *name, bool is_user)
{
	FILE *f = out_file ? out_file : stdout;

	if (!(yy_flags & PP_NO_LINEMARKERS)) {
		if (out_level != pp_include_level || out_file_name != yy_file_name || out_line != yy_line) {
			yy_line--;
			pp_debug_line(f);
			out_line = ++yy_line;
		}
	}

	if (is_user) {
		fprintf(f, "#include \"%s\"\n", name);
	} else {
		fprintf(f, "#include <%s>\n", name);
	}
	out_level++;
}

static void pp_debug_tokens(FILE *f, yy_sym *tokens)
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
			fprintf(f, " ");
		}
		prev = sym;
		if (PP_HAS_VAL(sym)) {
			p = pp_load_val(p);
			fprintf(f, "%.*s", (int)yy_len, yy_text);
		} else {
			if (sym & PP_NOSUBST) {
				fprintf(f, "<NOSUBST>");
				sym &= ~PP_NOSUBST;
			}
			fprintf(f, "%s", yy_sym2str(sym));
		}
	}
}

static void pp_debug_macro(yy_sym sym, yy_sym name, pp_macro *macro)
{
	FILE *f = out_file ? out_file : stdout;

	if (!(yy_flags & PP_NO_LINEMARKERS)) {
		if (out_level != pp_include_level || out_file_name != yy_file_name || out_line != yy_line) {
			yy_line--;
			pp_debug_line(f);
			out_line = ++yy_line;
		}
	}

	if (sym == YY_DEFINE) {
		fprintf(f, "#define %s", yy_sym2str(name));
		if (macro->flags & PP_MACRO_FUNCTION) {
			bool first = 1;
			int32_t i;

			fprintf(f, "(");
			for (i = 0; i < macro->num_args; i++) {
				fprintf(f, "%s%s", first ? "" : ",", yy_sym2str(macro->tokens[i]));
				first = 0;
			}
			if (macro->flags & PP_MACRO_VAR_ARG) {
				fprintf(f, "...");
			}
			fprintf(f, ")");
		}
		if (!(macro->flags & PP_MACRO_EMPTY)) {
			pp_debug_tokens(f, macro->tokens + macro->num_args);
		}
		fprintf(f, "\n");
	} else if (sym == YY_UNDEF) {
		fprintf(f, "#undef %s\n", yy_sym2str(name));
	}
}

/* cpp -E */
void pp_preprocess(FILE *f)
{
	yy_sym sym, prev = 0;
	bool empty_line = 1;
	uint32_t spaces= 0;

	out_file = f;

	out_file_name = 0;
	out_level = pp_include_level;
	out_line = yy_line;

	if (yy_flags & PP_NO_OUTPUT) {
		while (1) {
			sym = yy_next();
			if (sym == YY_EOF) break;
		}
	} else {
		while (1) {
			sym = yy_next();
			if (sym == YY_WS) {
				if (prev != YY_WS) {
					spaces = (empty_line && yy_len) ? yy_len : 1;
					prev = YY_WS;
				} else if (empty_line) {
					spaces += yy_len;
				}
				continue;
			} else if (sym == YY_EOL) {
				if (!empty_line) {
					fputc('\n', f);
					out_line++;
				}
				prev = YY_EOL;
				empty_line = 1;
				continue;
			} else if (sym == YY_EOF) {
				if (!empty_line) fputc('\n', f);
				break;
			} else if (sym == YY_PP_PLACE_MARKER) {
				continue;
			} else if (!PP_HAS_VAL(sym)) {
				yy_text = yy_sym2strl(sym, &yy_len);
			} 

			if (!(yy_flags & PP_NO_LINEMARKERS)) {
				if (out_level != pp_include_level || out_file_name != yy_file_name) {
					if (!empty_line) fputc('\n', f);
					pp_debug_line(f);
					prev = YY_EOL;
					empty_line = 1;
				} else if (out_line != yy_line) {
					pp_debug_line(f);
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
				&& (yy_text[yy_len-1] == 'E' || yy_text[yy_len-1] == 'e')) ? YY_E : sym;
			fwrite(yy_text, yy_len, 1, f);
		}
	}

	fflush(f);
	if (pp_ifdef_level) yy_error("mising #endif");
}
