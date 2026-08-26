/*
 * RCC - Rational C Compiler
 * (C parser)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 *
 * This file is generated from "c.g". Do not edit!
 *
 * To generate rcc_parser.c use llk <https://github.com/dstogov/llk>:
 * php llk.php c.g
 */

#include <ir.h>
#include <ir_private.h>

#include "rcc.h"

#define get_sym()            yy_next(rcc)
#define C_IS_ID(sym)         ((sym) > YY_LAST_KEYWORD)
#define yy_error_sym(m, s)   yy_error_sym_(rcc, m, s)

static IR_NEVER_INLINE void yy_error_sym_(rcc_ctx *rcc, const char *msg, int sym)
{
	yy_error_fmt("%s \"%s\"", msg, yy_sym2str(rcc, sym));
}

/* Parser Predicates */
static bool is_typedef_name(rcc_ctx *rcc, yy_sym id)
{
	if (rcc->yy_hash.data[id].sym && rcc->yy_hash.data[id].sym->kind == C_SYM_TYPE) {
		return 1;
	}
	return 0;
}

static bool is_typedef_name2(rcc_ctx *rcc, yy_sym id, c_dcl *dcl)
{
	if (dcl->flags & C_TYPE_SPEC_ANY) {
		return 0;
	}
	if (rcc->yy_hash.data[id].sym && rcc->yy_hash.data[id].sym->kind == C_SYM_TYPE) {
		return 1;
	}
	return 0;
}

static bool is_label(rcc_ctx *rcc, yy_sym id)
{
	IR_ASSERT(C_IS_ID(id));
	if (rcc->pp_stream) {
		pp_subst_stream *stream = rcc->pp_stream;

		do {
			yy_sym *tokens = stream->tokens;

			while (*tokens == YY_WS) tokens++;
			if (*tokens == YY__COLON) return 1;
			if (*tokens != YY_EOF) return 0;
			stream--;
		} while (stream >= rcc->pp_subst_stack);
	}

	if (*rcc->yy_pos == ':') {
		return 1;
	}

	const char *save_pos = rcc->yy_pos;
	const char *save_text = rcc->yy_text;
	const char *save_linepos = rcc->yy_linepos;
	int save_line = rcc->yy_line;

	rcc->yy_flags |= YY_NO_DIRECTIVE;
	bool ret = get_sym() == YY__COLON;
	rcc->yy_flags &= ~YY_NO_DIRECTIVE;

	if (rcc->pp_stream) {
		pp_subst_stream *stream = rcc->pp_stream;

		do {
			if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
			if (stream->start) pp_list_release(rcc, stream->start, stream->size);
			stream--;
		} while (stream >= rcc->pp_subst_stack);
	}

	rcc->pp_stream = NULL;
	rcc->yy_pos  = save_pos;
	rcc->yy_text = save_text;
	rcc->yy_linepos = save_linepos;
	rcc->yy_line = save_line;

	return ret;
}

static bool is_nested_declarator(rcc_ctx *rcc, yy_sym id)
{
	IR_ASSERT(id == YY__LPAREN);
	if (rcc->pp_stream) {
		pp_subst_stream *stream = rcc->pp_stream;

		do {
			yy_sym *tokens = stream->tokens;

			while (*tokens == YY_WS) tokens++;
			if (*tokens == YY___ATTRIBUTE
			 || *tokens == YY___ATTRIBUTE__
			 || *tokens == YY___CDECL
			 || *tokens == YY___FASTCALL
			 || *tokens == YY__STAR
			 || *tokens == YY__LPAREN
			 || *tokens == YY__LBRACK
			 || (C_IS_ID(*tokens) && !is_typedef_name(rcc, *tokens))) return 1;
			if (*tokens != YY_EOF) return 0;
			stream--;
		} while (stream >= rcc->pp_subst_stack);
	}

	if (*rcc->yy_pos == '*' || *rcc->yy_pos == '(' || *rcc->yy_pos == '[') {
		return 1;
	}

	const char *save_pos = rcc->yy_pos;
	const char *save_text = rcc->yy_text;
	const char *save_linepos = rcc->yy_linepos;
	int save_line = rcc->yy_line;

	rcc->yy_flags |= YY_NO_DIRECTIVE;
	yy_sym sym = get_sym();
	rcc->yy_flags &= ~YY_NO_DIRECTIVE;
	bool ret = (sym == YY___ATTRIBUTE
			|| sym == YY___ATTRIBUTE__
			|| sym == YY___CDECL
			|| sym == YY___FASTCALL
			|| sym == YY__STAR
			|| sym == YY__LPAREN
			|| sym == YY__LBRACK
			|| (C_IS_ID(sym) && !is_typedef_name(rcc, sym)));

	if (rcc->pp_stream) {
		pp_subst_stream *stream = rcc->pp_stream;

		do {
			if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
			if (stream->start) pp_list_release(rcc, stream->start, stream->size);
			stream--;
		} while (stream >= rcc->pp_subst_stack);
	}

	rcc->pp_stream = NULL;
	rcc->yy_pos  = save_pos;
	rcc->yy_text = save_text;
	rcc->yy_linepos = save_linepos;
	rcc->yy_line = save_line;

	return ret;
}

typedef struct _yy_str {
	const char *str;
	size_t      len;
} yy_str;

/* Scanner actions */
static void yy_read_string(rcc_ctx *rcc, c_value *res, const char *p, size_t len);
static void yy_read_strings(rcc_ctx *rcc, c_value *res, yy_str *strings, uint32_t num_strings);
static yy_str *yy_grow_strings(rcc_ctx *rcc, yy_str *strings, uint32_t num_strings);
static void yy_read_oct(c_value *res, const char *p, size_t len);
static void yy_read_dec(c_value *res, const char *p, size_t len);
static void yy_read_hex(c_value *res, const char *p, size_t len);
static void yy_read_bin(c_value *res, const char *p, size_t len);
static void yy_read_fp(c_value *res, const char *p, size_t len);
static void yy_read_char(rcc_ctx *rcc, c_value *res, const char *p, size_t len);
static yy_sym parse_vla_param(yy_sym sym, rcc_ctx *rcc, c_value *len);

#define YY_IN_SET(sym, set, bitset) \
	(bitset[sym>>3] & (1 << (sym & 0x7)))

static yy_sym parse_translation_unit(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_declaration(yy_sym sym, rcc_ctx *rcc, uint32_t flags);
static yy_sym parse_old_style_param_decl(yy_sym sym, rcc_ctx *rcc, const c_type *t);
static yy_sym parse_declaration_specifiers(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_type_qualifier_list(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_storage_class_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_type_specifier_or_qualifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_type_qualifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_function_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_alignment_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_attributes(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_attrib(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_asm_name(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_struct_or_union_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_struct_contents(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *d);
static yy_sym parse_struct_declaration(yy_sym sym, rcc_ctx *rcc, c_type *t);
static yy_sym parse_struct_declarator(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *field);
static yy_sym parse_enum_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_enum_contents(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *d);
static yy_sym parse_enumerator(yy_sym sym, rcc_ctx *rcc, const c_type *t, int64_t *min, uint64_t *max, c_value *last);
static yy_sym parse_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, c_name *name, bool allow_old_func);
static yy_sym parse_abstract_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d);
static yy_sym parse_parameter_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, c_name *name);
static yy_sym parse_arrays_and_params(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool allow_old_func, bool is_param);
static yy_sym parse_array_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool is_param);
static yy_sym parse_parameters(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool allow_old_func);
static yy_sym parse_parameter_declaration(yy_sym sym, rcc_ctx *rcc, c_param **params, uint32_t *num_params);
static yy_sym parse_identifier_list(yy_sym sym, rcc_ctx *rcc, c_param **params, uint32_t *num_params);
static yy_sym parse_type_name(yy_sym sym, rcc_ctx *rcc, const c_type **t);
static yy_sym parse_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj);
static yy_sym parse_initializer_contents(yy_sym sym, rcc_ctx *rcc, c_sym *obj, size_t *size);
static yy_sym parse_nested_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init, bool b);
static yy_sym parse_nested_initializer_contents(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init);
static yy_sym parse_designated_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init);
static yy_sym parse_gcc_field_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init);
static yy_sym parse_static_assert_declaration(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_compound_statement(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_expression_statement(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_statement(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_labels(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_c_statement(yy_sym sym, rcc_ctx *rcc);
static yy_sym parse_asm_argument(yy_sym sym, rcc_ctx *rcc, uint32_t asm_flags);
static yy_sym parse_asm_operands(yy_sym sym, rcc_ctx *rcc, c_asm *a, bool out, int *n);
static yy_sym parse_asm_operand(yy_sym sym, rcc_ctx *rcc, c_asm *a, bool out, int *n);
static yy_sym parse_asm_clobbers(yy_sym sym, rcc_ctx *rcc, c_asm *a);
static yy_sym parse_asm_goto_operands(yy_sym sym, rcc_ctx *rcc, c_asm *a, int *n);
static yy_sym parse_strings(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_actual_parameters(yy_sym sym, rcc_ctx *rcc, c_value *func, c_value *res);
static yy_sym parse_builtin_parameters(yy_sym sym, rcc_ctx *rcc, c_value *val, c_name name);
static yy_sym parse_dummy_value(yy_sym sym, rcc_ctx *rcc, const c_type *t);
static yy_sym parse_unary_expression(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_infix_expression(yy_sym sym, rcc_ctx *rcc, c_value *val, yy_sym prev);
static yy_sym parse_conditional_expression(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_assignment_expression(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_expression(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_constant_expression(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_ID(yy_sym sym, rcc_ctx *rcc, c_name *name);
static yy_sym parse_DECIMAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_OCTAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_HEXADECIMAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_BINARY_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_FLOATING_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_HEXADECIMAL_FLOATING_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_CHARACTER(yy_sym sym, rcc_ctx *rcc, c_value *val);
static yy_sym parse_STRING(yy_sym sym, rcc_ctx *rcc);
static int synpred_1(yy_sym sym);
static int synpred__lparen(yy_sym sym);
static int synpred__rbrace(yy_sym sym);
static int synpred__colon(yy_sym sym);
static int synpred__star(yy_sym sym);

static int synpred_1(yy_sym sym) {
	return sym == YY___ATTRIBUTE__ || sym == YY___ATTRIBUTE || sym == YY___DECLSPEC || sym == YY___ASM__ || sym == YY___ASM || sym == YY_ASM || sym == YY__EQUAL || sym == YY__COMMA || sym == YY__SEMICOLON;
}

static int synpred__lparen(yy_sym sym) {
	return sym == YY__LPAREN;
}

static int synpred__rbrace(yy_sym sym) {
	return sym == YY__RBRACE;
}

static int synpred__colon(yy_sym sym) {
	return sym == YY__COLON;
}

static int synpred__star(yy_sym sym) {
	return sym == YY__STAR;
}

static yy_sym parse_translation_unit(yy_sym sym, rcc_ctx *rcc) {
	while (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || sym == YY___EXTENSION__ || sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
		if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			c_value asm_str;
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			sym = parse_strings(sym, rcc, &asm_str);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			if (sym != YY__SEMICOLON) {
				yy_error_sym("';' expected, got", sym);
			}
			sym = get_sym();
			c_do_global_asm(rcc, &asm_str);
		} else {
			if (sym == YY___EXTENSION__) {
				sym = get_sym();
			}
			sym = parse_declaration(sym, rcc, 0);
		}
	}
	return sym;
}

static yy_sym parse_declaration(yy_sym sym, rcc_ctx *rcc, uint32_t flags) {
	c_dcl d0 = {0};
	c_name name;
	c_sym *obj;
	if (sym == YY__STATIC_ASSERT) {
		sym = parse_static_assert_declaration(sym, rcc);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
		d0.flags = flags;
		if ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
			sym = parse_declaration_specifiers(sym, rcc, &d0);
			if ((sym == YY_RETURN) && (d0.flags == C_DCL_STATEMENT && d0.attr2 == C_ATTR2_MUSTTAIL && !d0.type && !d0.alias)) {
				c_value val;
				c_value_clear(&val);
				/* Use IR_TAILCALL in val.u.proto to prevent inlining */
				val.u.proto = IR_TAILCALL;
				sym = get_sym();
				if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
					sym = parse_expression(sym, rcc, &val);
				}
				if (sym != YY__SEMICOLON) {
					yy_error_sym("';' expected, got", sym);
				}
				sym = get_sym();
				c_do_tailcall(rcc, &val);
				return sym;
			}
			if (d0.flags == C_DCL_STATEMENT && d0.attr2 == C_ATTR2_MUSTTAIL) yy_error("\"__musttail__\" attribute only applies to return statements");
		}
		if (sym == YY__STAR || C_IS_ID(sym) || sym == YY__LPAREN) {
			c_dcl d = d0;
			sym = parse_declarator(sym, rcc, &d, &name, 1);
			if ((sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__EQUAL || sym == YY__COMMA || sym == YY__SEMICOLON) && synpred_1(sym)) {
				if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
					sym = parse_asm_name(sym, rcc, &d);
				}
				if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
					sym = parse_attributes(sym, rcc, &d);
				}
				if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;
				obj = c_declare(rcc, name, &d);
				if (sym == YY__EQUAL) {
					sym = get_sym();
					sym = parse_initializer(sym, rcc, obj);
				}
				while (sym == YY__COMMA) {
					sym = get_sym();
					d = d0;
					if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
						sym = parse_attributes(sym, rcc, &d);
					}
					sym = parse_declarator(sym, rcc, &d, &name, 0);
					if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
						sym = parse_asm_name(sym, rcc, &d);
					}
					if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
						sym = parse_attributes(sym, rcc, &d);
					}
					if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;
					obj = c_declare(rcc, name, &d);
					if (sym == YY__EQUAL) {
						sym = get_sym();
						sym = parse_initializer(sym, rcc, obj);
					}
				}
				if (sym != YY__SEMICOLON) {
					yy_error_sym("';' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__LBRACE) {
				ir_ctx ctx, *old_ctx = rcc->active_ctx;
				c_scope scope;
				if (!d.type || d.type->kind != C_TYPE_FUNC) yy_error_sym("unexpected", sym);
				if ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (d.type->attr & C_ATTR_OLD_FUNC)) {
					do {
						sym = parse_old_style_param_decl(sym, rcc, d.type);
					} while (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED);
					c_validate_func_params(rcc, name, &d);
				}
				rcc->active_ctx = &ctx;
				c_do_func_start(rcc, name, &d, &scope);
				if (sym != YY__LBRACE) {
					yy_error_sym("'{' expected, got", sym);
				}
				sym = get_sym();
				void *checkpoint = ir_arena_checkpoint(rcc->c_func_arena);
				sym = parse_compound_statement(sym, rcc);
				c_do_func_end(rcc, name, &d, &scope);
				if (sym != YY__RBRACE) {
					yy_error_sym("'}' expected, got", sym);
				}
				sym = get_sym();
				ir_arena_release(&rcc->c_func_arena, checkpoint);
				rcc->active_ctx = old_ctx;
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__SEMICOLON) {
			c_empty_declaration(rcc, &d0);
			sym = get_sym();
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_old_style_param_decl(yy_sym sym, rcc_ctx *rcc, const c_type *t) {
	c_dcl d0 = {0};
	c_name name;
	sym = parse_declaration_specifiers(sym, rcc, &d0);
	c_dcl d = d0;
	if (sym == YY__STAR || C_IS_ID(sym) || sym == YY__LPAREN) {
		sym = parse_declarator(sym, rcc, &d, &name, 0);
		if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			sym = parse_asm_name(sym, rcc, &d);
		}
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, &d);
		}
		c_declare_func_param_type(rcc, t, name, &d);
		if (sym == YY__EQUAL) {
			sym = get_sym();
			yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(rcc, name));
			sym = parse_initializer(sym, rcc, NULL);
		}
		while (sym == YY__COMMA) {
			sym = get_sym();
			d = d0;
			sym = parse_declarator(sym, rcc, &d, &name, 0);
			if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
				sym = parse_asm_name(sym, rcc, &d);
			}
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_attributes(sym, rcc, &d);
			}
			c_declare_func_param_type(rcc, t, name, &d);
			if (sym == YY__EQUAL) {
				sym = get_sym();
				yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(rcc, name));
				sym = parse_initializer(sym, rcc, NULL);
			}
		}
	} else if (sym == YY__SEMICOLON) {
		yy_warning("empty declaration");
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__SEMICOLON) {
		yy_error_sym("';' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_declaration_specifiers(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	do {
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL) {
			sym = parse_storage_class_specifier(sym, rcc, d);
		} else if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T) {
			sym = parse_type_specifier_or_qualifier(sym, rcc, d);
		} else if (sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE) {
			sym = parse_function_specifier(sym, rcc, d);
		} else if (sym == YY__ALIGNAS) {
			sym = parse_alignment_specifier(sym, rcc, d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name2(rcc, sym, d)));
	return sym;
}

static yy_sym parse_type_qualifier_list(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	do {
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC) {
			sym = parse_type_qualifier(sym, rcc, d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED);
	return sym;
}

static yy_sym parse_storage_class_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	if (sym == YY_TYPEDEF) {
		if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_TYPEDEF;
	} else if (sym == YY_EXTERN) {
		if (d->flags & (C_DCL_STORAGE_CLASS-C_DCL_THREAD_LOCAL)) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_EXTERN;
	} else if (sym == YY_STATIC) {
		if (d->flags & (C_DCL_STORAGE_CLASS-C_DCL_THREAD_LOCAL)) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_STATIC;
	} else if (sym == YY_AUTO) {
		if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_AUTO;
	} else if (sym == YY_REGISTER) {
		if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_REGISTER;
	} else if (sym == YY__THREAD_LOCAL) {
		if (d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC))) yy_error("multiple storage classes in declaration specifiers");
		sym = get_sym();
		d->flags |= C_DCL_THREAD_LOCAL;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_type_specifier_or_qualifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_name name;
	if (sym == YY_VOID) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_VOID;
	} else if (sym == YY_CHAR) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_CHAR;
	} else if (sym == YY_SHORT) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_SHORT;
	} else if (sym == YY_INT) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_INT;
	} else if (sym == YY_LONG) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_LONG|C_TYPE_SPEC_INT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_COMPLEX))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= (d->flags & C_TYPE_SPEC_LONG) ? C_TYPE_SPEC_LONG_LONG : C_TYPE_SPEC_LONG;
	} else if (sym == YY_FLOAT) {
		if (d->flags & (C_TYPE_SPEC_ANY-C_TYPE_SPEC_COMPLEX)) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_FLOAT;
	} else if (sym == YY_DOUBLE) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_LONG|C_TYPE_SPEC_COMPLEX))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_DOUBLE;
	} else if (sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_SIGNED;
	} else if (sym == YY_UNSIGNED) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_UNSIGNED;
	} else if (sym == YY__BOOL) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_BOOL;
	} else if (sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_FLOAT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_LONG))) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_COMPLEX;
	} else if (sym == YY__ATOMIC) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, YY__ATOMIC);
			d->flags |= C_TYPE_SPEC_ATOMIC;
			sym = parse_type_name(sym, rcc, &d->type);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY_RETURN || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__RBRACE || sym == YY__COLON) {
			d->attr |= C_ATTR_ATOMIC;
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_TYPE;
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
			sym = parse_type_name(sym, rcc, &d->type);
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			c_value v;
			ir_ref old = c_do_nocode(rcc);
			c_value_clear(&v);
			sym = parse_expression(sym, rcc, &v);
			d->type = c_typeof_expr(rcc, &v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_STRUCT || sym == YY_UNION) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = parse_struct_or_union_specifier(sym, rcc, d);
	} else if (sym == YY_ENUM) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = parse_enum_specifier(sym, rcc, d);
	} else if (C_IS_ID(sym)) {
		if (d->flags & C_TYPE_SPEC_ANY) c_wrong_type_specifiers(rcc, d->flags, sym);
		sym = parse_ID(sym, rcc, &name);
		d->flags |= C_TYPE_SPEC_NAME;
		d->type = c_resolve_type_name(rcc, name);
	} else if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__) {
		sym = get_sym();
		d->attr |= C_ATTR_CONST;
	} else if (sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__) {
		if (!d->type || d->type->kind != C_TYPE_POINTER) yy_error("invalid use of \"restrict\"");
		sym = get_sym();
		d->attr |= C_ATTR_RESTRICT;
	} else if (sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__) {
		sym = get_sym();
		d->attr |= C_ATTR_VOLATILE;
	} else if (sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T) {
		yy_error_fmt("unsupported type \"%s\"", yy_sym2str(rcc, sym));
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_type_qualifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__) {
		sym = get_sym();
		d->attr |= C_ATTR_CONST;
	} else if (sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__) {
		sym = get_sym();
		d->attr |= C_ATTR_RESTRICT;
	} else if (sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__) {
		sym = get_sym();
		d->attr |= C_ATTR_VOLATILE;
	} else if (sym == YY__ATOMIC) {
		sym = get_sym();
		d->attr |= C_ATTR_ATOMIC;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_function_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	if (sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__) {
		sym = get_sym();
		d->attr |= C_ATTR_INLINE;
	} else if (sym == YY__NORETURN) {
		sym = get_sym();
		d->attr |= C_ATTR_NORETURN;
	} else if (sym == YY___FORCEINLINE) {
		sym = get_sym();
		d->attr |= C_ATTR_ALWAYS_INLINE;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_alignment_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_value v;
	if (sym != YY__ALIGNAS) {
		yy_error_sym("'_Alignas' expected, got", sym);
	}
	sym = get_sym();
	if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
		const c_type *t;
		sym = parse_type_name(sym, rcc, &t);
		d->attr |= t->attr & C_ATTR_ALIGN_MASK;
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		c_value_clear(&v);
		sym = parse_constant_expression(sym, rcc, &v);
		c_alignas_expr(rcc, d, &v);
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_attributes(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	do {
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			sym = parse_attrib(sym, rcc, d);
			while (sym == YY__COMMA) {
				sym = get_sym();
				sym = parse_attrib(sym, rcc, d);
			}
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY___DECLSPEC) {
			c_name name;
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			name = sym;
			if (sym == YY_ALIGN) {
				c_value v;
				c_value_clear(&v);
				sym = get_sym();
				if (sym != YY__LPAREN) {
					yy_error_sym("'(' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
				c_declspec_align(rcc, d, &v);
			} else if (sym == YY_RESTRICT) {
				sym = get_sym();
				sym = c_declspec(rcc, d, name, sym);
			} else if (C_IS_ID(sym)) {
				sym = parse_ID(sym, rcc, &name);
				sym = c_declspec(rcc, d, name, sym);
			} else {
				yy_error_sym("unexpected", sym);
			}
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY___CDECL) {
			sym = get_sym();
			if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_CDECL) yy_error("multiple calling conventions");
			d->attr |= C_ATTR_CC_CDECL;
		} else if (sym == YY___FASTCALL) {
			sym = get_sym();
			if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_FASTCALL) yy_error("multiple calling conventions");
			d->attr |= C_ATTR_CC_FASTCALL;
		} else if (sym == YY___UNALIGNED) {
			c_value v;
			ir_val val;
			val.u64 = 1;
			sym = get_sym();
			c_value_set_const(&v, &c_type_i32, IR_I32, val);
			c_declspec_align(rcc, d, &v);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED);
	return sym;
}

static yy_sym parse_attrib(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_name name = sym;
	c_value v;
	if (sym == YY_ALIAS || sym == YY___ALIAS__ || sym == YY_ALIGNED || sym == YY___ALIGNED__ || sym == YY_ALWAYS_INLINE || sym == YY___ALWAYS_INLINE__ || sym == YY_CDECL || sym == YY___CDECL__ || sym == YY_CLEANUP || sym == YY___CLEANUP__ || sym == YY_COLD || sym == YY___COLD__ || sym == YY_CONST || sym == YY___CONST__ || sym == YY_CONSTRUCTOR || sym == YY___CONSTRUCTOR__ || sym == YY_DEPRECATED || sym == YY___DEPRECATED__ || sym == YY_DESTRUCTOR || sym == YY___DESTRUCTOR__ || sym == YY_FALLTHROUGH || sym == YY___FALLTHROUGH__ || sym == YY_FASTCALL || sym == YY___FASTCALL__ || sym == YY_GCC_STRUCT || sym == YY___GCC_STRUCT__ || sym == YY_HOT || sym == YY___HOT__ || sym == YY_LEAF || sym == YY___LEAF__ || sym == YY_MODE || sym == YY___MODE__ || sym == YY_MS_STRUCT || sym == YY___MS_STRUCT__ || sym == YY_MUSTTAIL || sym == YY___MUSTTAIL__ || sym == YY_NOINLINE || sym == YY___NOINLINE__ || sym == YY_NORETURN || sym == YY___NORETURN__ || sym == YY_NOTHROW || sym == YY___NOTHROW__ || sym == YY_PACKED || sym == YY___PACKED__ || sym == YY_PRESERVE_NONE || sym == YY___PRESERVE_NONE__ || sym == YY_PURE || sym == YY___PURE__ || sym == YY_REGPARM || sym == YY___REGPARM__ || sym == YY_UNUSED || sym == YY___UNUSED__ || sym == YY_VECTOR_SIZE || sym == YY___VECTOR_SIZE__ || sym == YY_WEAK || sym == YY___WEAK__ || C_IS_ID(sym)) {
		if (sym == YY_ALIAS || sym == YY___ALIAS__) {
			sym = get_sym();
			if (sym == YY__LPAREN) {
				sym = get_sym();
				sym = parse_strings(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			}
			c_gcc_attribute_alias(rcc, d, name, &v);
		} else if (sym == YY_ALIGNED || sym == YY___ALIGNED__) {
			sym = get_sym();
			c_value_clear(&v);
			if (sym == YY__LPAREN) {
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			}
			c_gcc_attribute_aligned(rcc, d, name, &v);
		} else if (sym == YY_ALWAYS_INLINE || sym == YY___ALWAYS_INLINE__) {
			sym = get_sym();
			d->attr |= C_ATTR_ALWAYS_INLINE;
		} else if (sym == YY_CDECL || sym == YY___CDECL__) {
			sym = get_sym();
			if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_CDECL) yy_error("multiple calling conventions");
			d->attr |= C_ATTR_CC_CDECL;
		} else if (sym == YY_CLEANUP || sym == YY___CLEANUP__) {
			sym = get_sym();
			c_name func;
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			sym = parse_ID(sym, rcc, &func);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			c_gcc_attribute_cleanup(rcc, d, name, func);
		} else if (sym == YY_COLD || sym == YY___COLD__) {
			sym = get_sym();
			d->attr |= C_ATTR_COLD;
		} else if (sym == YY_CONST || sym == YY___CONST__) {
			sym = get_sym();
			d->attr |= C_ATTR_CONST_FUNC;
		} else if (sym == YY_CONSTRUCTOR || sym == YY___CONSTRUCTOR__) {
			sym = get_sym();
			d->attr2 |= C_ATTR2_CONSTRUCTOR;
		} else if (sym == YY_DEPRECATED || sym == YY___DEPRECATED__) {
			sym = get_sym();
			d->attr |= C_ATTR_DEPRECATED;
			c_value_clear(&v);
			if (sym == YY__LPAREN) {
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			}
		} else if (sym == YY_DESTRUCTOR || sym == YY___DESTRUCTOR__) {
			sym = get_sym();
			d->attr2 |= C_ATTR2_DESTRUCTOR;
		} else if (sym == YY_FALLTHROUGH || sym == YY___FALLTHROUGH__) {
			sym = get_sym();
			d->attr2 |= C_ATTR2_FALLTHROUGH;
		} else if (sym == YY_FASTCALL || sym == YY___FASTCALL__) {
			sym = get_sym();
			if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_FASTCALL) yy_error("multiple calling conventions");
			d->attr |= C_ATTR_CC_FASTCALL;
		} else if (sym == YY_GCC_STRUCT || sym == YY___GCC_STRUCT__) {
			sym = get_sym();
			d->attr |= C_ATTR_GCC_STRUCT;
		} else if (sym == YY_HOT || sym == YY___HOT__) {
			sym = get_sym();
			d->attr |= C_ATTR_HOT;
		} else if (sym == YY_LEAF || sym == YY___LEAF__) {
			sym = get_sym();
			d->attr |= C_ATTR_LEAF;
		} else if (sym == YY_MODE || sym == YY___MODE__) {
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			c_name mode;
			if (sym == YY_QI || sym == YY___QI__ || sym == YY_BYTE || sym == YY___BYTE__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_CHAR;
			} else if (sym == YY_HI || sym == YY___HI__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_SHORT;
			} else if (sym == YY_SI || sym == YY___SI__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_INT;
			} else if (sym == YY_WORD || sym == YY___WORD__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_LONG;
			} else if (sym == YY_DI || sym == YY___DI__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_INT64;
			} else if (sym == YY_SF || sym == YY___SF__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_FLOAT;
			} else if (sym == YY_DF || sym == YY___DF__) {
				sym = get_sym();
				d->flags = (d->flags & ~C_TYPE_SPEC_ANY_MODE) | C_TYPE_SPEC_DOUBLE;
			} else if (C_IS_ID(sym)) {
				sym = parse_ID(sym, rcc, &mode);
				yy_error_fmt("unsupported attribute \"%s(%s)\"", yy_sym2str(rcc, name), yy_sym2str(rcc, mode));
			} else {
				yy_error_sym("unexpected", sym);
			}
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY_MS_STRUCT || sym == YY___MS_STRUCT__) {
			sym = get_sym();
			d->attr |= C_ATTR_MS_STRUCT;
		} else if (sym == YY_MUSTTAIL || sym == YY___MUSTTAIL__) {
			sym = get_sym();
			if (!(d->flags & C_DCL_STATEMENT)) yy_error_fmt("\"%s\" attribute only applies to return statements", yy_sym2str(rcc, name));
			d->attr2 |= C_ATTR2_MUSTTAIL;
		} else if (sym == YY_NOINLINE || sym == YY___NOINLINE__) {
			sym = get_sym();
			d->attr |= C_ATTR_NOINLINE;
		} else if (sym == YY_NORETURN || sym == YY___NORETURN__) {
			sym = get_sym();
			if (!(d->flags & C_DCL_TYPEDEF) || !d->type) d->attr |= C_ATTR_NORETURN;
		} else if (sym == YY_NOTHROW || sym == YY___NOTHROW__) {
			sym = get_sym();
			d->attr |= C_ATTR_NOTHROW;
		} else if (sym == YY_PACKED || sym == YY___PACKED__) {
			sym = get_sym();
			c_gcc_attribute_packed(rcc, d, name);
		} else if (sym == YY_PRESERVE_NONE || sym == YY___PRESERVE_NONE__) {
			sym = get_sym();
			if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_PRESERVE_NONE) yy_error("multiple calling conventions");
			d->attr |= C_ATTR_CC_PRESERVE_NONE;
		} else if (sym == YY_PURE || sym == YY___PURE__) {
			sym = get_sym();
			d->attr |= C_ATTR_PURE;
		} else if (sym == YY_REGPARM || sym == YY___REGPARM__) {
			sym = get_sym();
			if (sym != YY__LPAREN) {
				yy_error_sym("'(' expected, got", sym);
			}
			sym = get_sym();
			sym = parse_constant_expression(sym, rcc, &v);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			c_gcc_attribute_regparm(rcc, d, name, &v);
		} else if (sym == YY_UNUSED || sym == YY___UNUSED__) {
			sym = get_sym();
			d->attr |= C_ATTR_UNUSED;
		} else if (sym == YY_VECTOR_SIZE || sym == YY___VECTOR_SIZE__) {
			sym = get_sym();
			c_value_clear(&v);
			if (sym == YY__LPAREN) {
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			}
			c_gcc_attribute_vector_size(rcc, d, name, &v);
		} else if (sym == YY_WEAK || sym == YY___WEAK__) {
			sym = get_sym();
			d->attr |= C_ATTR_WEAK;
		} else {
			sym = parse_ID(sym, rcc, &name);
			sym = c_gcc_attribute(rcc, d, name, sym);
		}
	}
	return sym;
}

static yy_sym parse_asm_name(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_value val;
	if (sym == YY_ASM) {
		sym = get_sym();
	} else if (sym == YY___ASM) {
		sym = get_sym();
	} else if (sym == YY___ASM__) {
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_strings(sym, rcc, &val);
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	c_asm_alias(rcc, d, &val);
	return sym;
}

static yy_sym parse_struct_or_union_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_name name;
	if (sym == YY_STRUCT) {
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_STRUCT;
	} else if (sym == YY_UNION) {
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_UNION;
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, d);
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, rcc, &name);
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY_RETURN || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__RBRACE || sym == YY__COLON) {
			c_resolve_tag(rcc, name, d, 0, NULL);
		} else if (sym == YY__LBRACE) {
			c_type *t = c_resolve_tag(rcc, name, d, 1, NULL);
			sym = parse_struct_contents(sym, rcc, t, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__LBRACE) {
		c_type *t = c_make_struct_type(rcc, d, 0);
		sym = parse_struct_contents(sym, rcc, t, d);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_struct_contents(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *d) {
	t->record.fields = alloca(sizeof(c_field) * C_ALLOCA_FIELDS);
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	t->flags |= C_TYPE_INPROGRESS;
	if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__STATIC_ASSERT) {
		sym = parse_struct_declaration(sym, rcc, t);
		while (sym == YY__SEMICOLON) {
			sym = get_sym();
			if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
				break; /* manual conflict resolution */
				sym = get_sym();
			} else if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__STATIC_ASSERT) {
				sym = parse_struct_declaration(sym, rcc, t);
			} else {
				yy_error_sym("unexpected", sym);
			}
		}
	} else if (sym == YY__SEMICOLON) {
		sym = get_sym();
	} else if (sym == YY__RBRACE) {
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RBRACE) {
		yy_error_sym("'}' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, d);
	}
	c_finish_struct_type(rcc, t, d);
	return sym;
}

static yy_sym parse_struct_declaration(yy_sym sym, rcc_ctx *rcc, c_type *t) {
	if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		c_dcl field0 = {0};
		if (sym == YY___EXTENSION__) {
			sym = get_sym();
		}
		do {
			if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T) {
				sym = parse_type_specifier_or_qualifier(sym, rcc, &field0);
			} else if (sym == YY__ALIGNAS) {
				sym = parse_alignment_specifier(sym, rcc, &field0);
			} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_attributes(sym, rcc, &field0);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} while ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name2(rcc, sym, &field0)));
		c_dcl field = field0;
		sym = parse_struct_declarator(sym, rcc, t, &field);
		while (sym == YY__COMMA) {
			sym = get_sym();
			field = field0;
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_attributes(sym, rcc, &field);
			}
			sym = parse_struct_declarator(sym, rcc, t, &field);
		}
	} else if (sym == YY__STATIC_ASSERT) {
		sym = parse_static_assert_declaration(sym, rcc);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_struct_declarator(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *field) {
	c_value v;
	c_name name;
	c_value_clear(&v);
	if (sym == YY__STAR || C_IS_ID(sym) || sym == YY__LPAREN) {
		sym = parse_declarator(sym, rcc, field, &name, 0);
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, field);
		}
		if (sym == YY__COLON) {
			sym = get_sym();
			sym = parse_constant_expression(sym, rcc, &v);
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_attributes(sym, rcc, field);
			}
		}
		c_declare_struct_field(rcc, t, name, field, &v);
	} else if (sym == YY__COLON || sym == YY__COMMA || sym == YY__SEMICOLON || sym == YY__RBRACE) {
		if (sym == YY__COLON) {
			sym = get_sym();
			sym = parse_constant_expression(sym, rcc, &v);
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_attributes(sym, rcc, field);
			}
		}
		c_declare_struct_field(rcc, t, 0, field, &v);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_enum_specifier(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_name name;
	const c_type *base_type = NULL;
	if (sym != YY_ENUM) {
		yy_error_sym("'enum' expected, got", sym);
	}
	sym = get_sym();
	d->flags |= C_TYPE_SPEC_ENUM;
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, d);
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, rcc, &name);
		if ((sym == YY__COLON) && synpred__colon(sym)) {
			c_dcl u = {0};
			sym = get_sym();
			do {
				sym = parse_type_specifier_or_qualifier(sym, rcc, &u);
			} while (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T);
			base_type = c_underlying_enum_type(rcc, &u);
		}
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY_RETURN || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__RBRACE || sym == YY__COLON) {
			c_resolve_tag(rcc, name, d, 0, base_type);
		} else if (sym == YY__LBRACE) {
			c_type *t = c_resolve_tag(rcc, name, d, 1, base_type);
			sym = parse_enum_contents(sym, rcc, t, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__COLON || sym == YY__LBRACE) {
		if (sym == YY__COLON) {
			c_dcl u = {0};
			sym = get_sym();
			do {
				sym = parse_type_specifier_or_qualifier(sym, rcc, &u);
			} while (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T);
			base_type = c_underlying_enum_type(rcc, &u);
		}
		c_type *t = c_make_enum_type(rcc, d, 0, base_type);
		sym = parse_enum_contents(sym, rcc, t, d);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_enum_contents(yy_sym sym, rcc_ctx *rcc, c_type *t, c_dcl *d) {
	int64_t min = 0;
	uint64_t max = 0;
	c_value last;
	last.u.type = IR_I64; last.u.val.i64 = -1;
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_enumerator(sym, rcc, t, &min, &max, &last);
	while (sym == YY__COMMA) {
		sym = get_sym();
		if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
			break; /* manual conflict resolution */
			sym = get_sym();
		} else if (C_IS_ID(sym)) {
			sym = parse_enumerator(sym, rcc, t, &min, &max, &last);
		} else {
			yy_error_sym("unexpected", sym);
		}
	}
	if (sym != YY__RBRACE) {
		yy_error_sym("'}' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, d);
	}
	c_finish_enum_type(rcc, t, d, min, max);
	return sym;
}

static yy_sym parse_enumerator(yy_sym sym, rcc_ctx *rcc, const c_type *t, int64_t *min, uint64_t *max, c_value *last) {
	c_value v;
	c_dcl attr = {0};
	c_name name;
	c_value_clear(&v);
	sym = parse_ID(sym, rcc, &name);
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, &attr);
	}
	if (sym == YY__EQUAL) {
		sym = get_sym();
		sym = parse_constant_expression(sym, rcc, &v);
	}
	c_declare_enum_val(rcc, t, name, &attr, &v, min, max, last);
	return sym;
}

static yy_sym parse_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, c_name *name, bool allow_old_func) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(rcc, d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_type_qualifier_list(sym, rcc, d);
		}
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, rcc, name);
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, allow_old_func, 0);
		}
	} else if (sym == YY__LPAREN) {
		sym = get_sym();
		d2.flags = C_TYPE_SPEC_CHAR;
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, &d2);
		}
		sym = parse_declarator(sym, rcc, &d2, name, 0);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, allow_old_func, 0);
		}
		c_make_nested_type(rcc, d, &d2);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_abstract_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(rcc, d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_type_qualifier_list(sym, rcc, d);
		}
	}
	if ((sym == YY__LPAREN) && (is_nested_declarator(rcc, sym))) {
		d2.flags = C_TYPE_SPEC_CHAR;
		sym = get_sym();
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, &d2);
		}
		sym = parse_abstract_declarator(sym, rcc, &d2);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, 0, 0);
		}
		c_make_nested_type(rcc, d, &d2);
	} else if (sym == YY__LPAREN || sym == YY__LBRACK || sym == YY__RPAREN || sym == YY__COMMA || sym == YY__COLON) {
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, 0, 0);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_parameter_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, c_name *name) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(rcc, d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_type_qualifier_list(sym, rcc, d);
		}
	}
	if ((sym == YY__LPAREN) && (is_nested_declarator(rcc, sym))) {
		d2.flags = C_TYPE_SPEC_CHAR;
		sym = get_sym();
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, &d2);
		}
		sym = parse_parameter_declarator(sym, rcc, &d2, name);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, 0, 1);
		}
		c_make_nested_type(rcc, d, &d2);
	} else if (C_IS_ID(sym)) {
		sym = parse_ID(sym, rcc, name);
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, 0, 1);
		}
	} else if (sym == YY__LPAREN || sym == YY__LBRACK || sym == YY__RPAREN || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__COMMA) {
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, rcc, d, 0, 1);
		}
		*name = 0;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_arrays_and_params(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool allow_old_func, bool is_param) {
	if (sym == YY__LPAREN) {
		sym = parse_parameters(sym, rcc, d, allow_old_func);
	} else if (sym == YY__LBRACK) {
		sym = parse_array_declarator(sym, rcc, d, is_param);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_array_declarator(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool is_param) {
	c_value len;
	c_dcl dim = {0};
	uint64_t attr = 0;
	c_value_clear(&len);
	if (sym != YY__LBRACK) {
		yy_error_sym("'[' expected, got", sym);
	}
	sym = get_sym();
	if ((sym == YY__STAR) && synpred__star(sym)) {
		sym = get_sym();
		if (!is_param) yy_error("[*] not allowed in other than function prototype scope");
		attr |= C_ATTR_VLA;
	} else if (sym == YY__RBRACK) {
		attr |= C_ATTR_FLEXIBLE;
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		if (!is_param || (sym = parse_vla_param(sym, rcc, &len)) != YY__RBRACK)
		sym = parse_assignment_expression(sym, rcc, &len);
	} else if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_type_qualifier_list(sym, rcc, &dim);
		if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");
		if ((sym == YY__STAR) && synpred__star(sym)) {
			sym = get_sym();
			if (!is_param) yy_error("[*] not allowed in other than function prototype scope");
			attr |= C_ATTR_VLA;
		} else if (sym == YY__RBRACK) {
			attr |= C_ATTR_FLEXIBLE;
		} else if (sym == YY_STATIC || sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			if (sym == YY_STATIC) {
				sym = get_sym();
				if (sym == YY__RBRACK) yy_error("\"static\" may not be used without an array size");
			}
			if (!is_param || (sym = parse_vla_param(sym, rcc, &len)) != YY__RBRACK)
			sym = parse_assignment_expression(sym, rcc, &len);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY_STATIC) {
		sym = get_sym();
		if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_type_qualifier_list(sym, rcc, &dim);
		}
		if (sym == YY__RBRACK) yy_error("\"static\" may not be used without an array size");
		if (!is_param || (sym = parse_vla_param(sym, rcc, &len)) != YY__RBRACK)
		sym = parse_assignment_expression(sym, rcc, &len);
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RBRACK) {
		yy_error_sym("']' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY__LPAREN || sym == YY__LBRACK) {
		sym = parse_arrays_and_params(sym, rcc, d, 0, is_param);
	}
	c_make_array_type(rcc, d, &dim, &len, attr, is_param);
	return sym;
}

static yy_sym parse_parameters(yy_sym sym, rcc_ctx *rcc, c_dcl *d, bool allow_old_func) {
	uint32_t attr = 0;
	uint32_t num_params = 0;
	c_param *params = alloca(sizeof(c_param) * C_ALLOCA_PARAMS);
	c_scope scope;
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	if ((C_IS_ID(sym)) && (allow_old_func && !is_typedef_name(rcc, sym))) {
		c_push_scope(rcc, &scope);
		sym = parse_identifier_list(sym, rcc, &params, &num_params);
		attr |= C_ATTR_OLD_FUNC;
		c_pop_scope_light(rcc, &scope);
	} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		c_push_scope(rcc, &scope);
		sym = parse_parameter_declaration(sym, rcc, &params, &num_params);
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_parameter_declaration(sym, rcc, &params, &num_params);
			} else if (sym == YY__POINT_POINT_POINT) {
				sym = get_sym();
				attr |= C_ATTR_VARIADIC;
				break; /* manual conflict resolution */
			} else {
				yy_error_sym("unexpected", sym);
			}
		}
		c_pop_scope_light(rcc, &scope);
	} else if (sym == YY__POINT_POINT_POINT) {
		sym = get_sym();
		attr |= C_ATTR_VARIADIC;
	} else if (sym == YY__RPAREN) {
		attr |= C_ATTR_OLD_FUNC;
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY__LPAREN || sym == YY__LBRACK) {
		sym = parse_arrays_and_params(sym, rcc, d, 0, 0);
	}
	c_make_func_type(rcc, d, params, num_params, attr);
	return sym;
}

static yy_sym parse_parameter_declaration(yy_sym sym, rcc_ctx *rcc, c_param **params, uint32_t *num_params) {
	c_dcl p = {0};
	c_name name;
	sym = parse_declaration_specifiers(sym, rcc, &p);
	sym = parse_parameter_declarator(sym, rcc, &p, &name);
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
		sym = parse_attributes(sym, rcc, &p);
	}
	c_declare_func_param(rcc, params, num_params, name, &p);
	return sym;
}

static yy_sym parse_identifier_list(yy_sym sym, rcc_ctx *rcc, c_param **params, uint32_t *num_params) {
	c_name name;
	sym = parse_ID(sym, rcc, &name);
	c_declare_func_param_name(rcc, params, num_params, name);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_ID(sym, rcc, &name);
		c_declare_func_param_name(rcc, params, num_params, name);
	}
	return sym;
}

static yy_sym parse_type_name(yy_sym sym, rcc_ctx *rcc, const c_type **t) {
	c_dcl d = {0};
	do {
		if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T) {
			sym = parse_type_specifier_or_qualifier(sym, rcc, &d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
			sym = parse_attributes(sym, rcc, &d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name2(rcc, sym, &d)));
	sym = parse_abstract_declarator(sym, rcc, &d);
	*t = c_resolve_type(rcc, &d);
	return sym;
}

static yy_sym parse_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj) {
	rcc->c_static_data = (obj && obj->linkage);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		c_value v;
		c_value_clear(&v);
		sym = parse_assignment_expression(sym, rcc, &v);
		c_do_init_obj(rcc, obj, &v);
	} else if (sym == YY__LBRACE) {
		size_t size;
		sym = parse_initializer_contents(sym, rcc, obj, &size);
	} else {
		yy_error_sym("unexpected", sym);
	}
	rcc->c_static_data = 0;
	return sym;
}

static yy_sym parse_initializer_contents(yy_sym sym, rcc_ctx *rcc, c_sym *obj, size_t *size) {
	c_init init;
	c_do_init_start(rcc, obj, &init);
	sym = parse_nested_initializer_contents(sym, rcc, obj, &init);
	c_do_init_end(rcc, obj, &init);
	*size = init.size;
	return sym;
}

static yy_sym parse_nested_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init, bool b) {
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		c_value v;
		c_value_clear(&v);
		sym = parse_assignment_expression(sym, rcc, &v);
		c_do_init_set(rcc, obj, init, &v);
	} else if (sym == YY__LBRACE) {
		uint32_t orig_level = init->level;
		c_do_init_nested(rcc, obj, init, b);
		sym = parse_nested_initializer_contents(sym, rcc, obj, init);
		init->level = orig_level;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_nested_initializer_contents(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init) {
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	uint32_t orig_level = init->level;
	if (C_IS_ID(sym) || sym == YY__LPAREN || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__LBRACE || sym == YY__LBRACK || sym == YY__POINT) {
		if (obj->value.type->attr & C_ATTR_VLA) yy_error("variable length array may not be initialized except with an empty initializer");
		if ((C_IS_ID(sym)) && (!C_IS_ID(sym) || is_label(rcc, sym))) {
			sym = parse_gcc_field_initializer(sym, rcc, obj, init);
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__LBRACE) {
			sym = parse_nested_initializer(sym, rcc, obj, init, 0);
		} else {
			init->level = orig_level;
			sym = parse_designated_initializer(sym, rcc, obj, init);
		}
		while (sym == YY__COMMA) {
			sym = get_sym();
			if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
				break; /* manual conflict resolution */
				sym = get_sym();
			} else if ((C_IS_ID(sym)) && (!C_IS_ID(sym) || is_label(rcc, sym))) {
				sym = parse_gcc_field_initializer(sym, rcc, obj, init);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__LBRACE) {
				c_do_init_next(rcc, obj, init);
				sym = parse_nested_initializer(sym, rcc, obj, init, 0);
			} else if (sym == YY__LBRACK || sym == YY__POINT) {
				init->level = orig_level;
				sym = parse_designated_initializer(sym, rcc, obj, init);
			} else {
				yy_error_sym("unexpected", sym);
			}
		}
	}
	if (sym != YY__RBRACE) {
		yy_error_sym("'}' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_designated_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init) {
	uint32_t level, orig_level = init->level;
	do {
		if (sym == YY__LBRACK) {
			c_value v;
			level = init->level;
			sym = get_sym();
			sym = parse_constant_expression(sym, rcc, &v);
			c_do_init_dim(rcc, obj, init, &v);
			if (sym == YY__POINT_POINT_POINT) {
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &v);
				c_do_init_range(rcc, obj, init, &v);
			}
			if (sym != YY__RBRACK) {
				yy_error_sym("']' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__POINT) {
			c_name name;
			level = init->level;
			sym = get_sym();
			sym = parse_ID(sym, rcc, &name);
			c_do_init_field(rcc, obj, init, name);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while (sym == YY__LBRACK || sym == YY__POINT);
	if (sym != YY__EQUAL) {
		yy_error_sym("'=' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_nested_initializer(sym, rcc, obj, init, 1);
	c_do_init_rollback(rcc, obj, init, orig_level, level);
	return sym;
}

static yy_sym parse_gcc_field_initializer(yy_sym sym, rcc_ctx *rcc, c_sym *obj, c_init *init) {
	uint32_t level = init->level;
	c_name name;
	sym = parse_ID(sym, rcc, &name);
	if (sym != YY__COLON) {
		yy_error_sym("':' expected, got", sym);
	}
	sym = get_sym();
	c_do_init_field(rcc, obj, init, name);
	sym = parse_nested_initializer(sym, rcc, obj, init, 1);
	c_do_init_rollback(rcc, obj, init, level, level);
	return sym;
}

static yy_sym parse_static_assert_declaration(yy_sym sym, rcc_ctx *rcc) {
	c_value cond, msg;
	c_value_clear(&cond);
	c_value_clear(&msg);
	if (sym != YY__STATIC_ASSERT) {
		yy_error_sym("'_Static_assert' expected, got", sym);
	}
	sym = get_sym();
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_constant_expression(sym, rcc, &cond);
	if (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_strings(sym, rcc, &msg);
	}
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	c_static_assert(rcc, &cond, &msg);
	return sym;
}

static yy_sym parse_compound_statement(yy_sym sym, rcc_ctx *rcc) {
	c_value val;
	c_value_clear(&val);
	while (sym == YY___LABEL__) {
		c_name name;
		sym = get_sym();
		sym = parse_ID(sym, rcc, &name);
		c_declare_local_label(rcc, name);
		while (sym == YY__COMMA) {
			sym = get_sym();
			sym = parse_ID(sym, rcc, &name);
			c_declare_local_label(rcc, name);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	}
	while (sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT || sym == YY__LPAREN || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__SEMICOLON) {
		if ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(rcc, sym))) {
			sym = parse_labels(sym, rcc);
		} else if (sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			sym = parse_c_statement(sym, rcc);
		} else {
			if (sym == YY___EXTENSION__) sym = yy_next(rcc);
			if ((sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) && (!C_IS_ID(sym) || !is_typedef_name(rcc, sym))) {
				sym = parse_expression(sym, rcc, &val);
				if (sym != YY__SEMICOLON) {
					yy_error_sym("';' expected, got", sym);
				}
				sym = get_sym();
			} else {
				sym = parse_declaration(sym, rcc, C_DCL_STATEMENT);
			}
		}
	}
	return sym;
}

static yy_sym parse_expression_statement(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	while (sym == YY___LABEL__) {
		c_name name;
		sym = get_sym();
		sym = parse_ID(sym, rcc, &name);
		c_declare_local_label(rcc, name);
		while (sym == YY__COMMA) {
			sym = get_sym();
			sym = parse_ID(sym, rcc, &name);
			c_declare_local_label(rcc, name);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	}
	while (sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT || sym == YY__LPAREN || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__SEMICOLON) {
		c_value_clear(val);
		if ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(rcc, sym))) {
			sym = parse_labels(sym, rcc);
		} else if (sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			sym = parse_c_statement(sym, rcc);
		} else {
			if (sym == YY___EXTENSION__) sym = yy_next(rcc);
			if ((sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) && (!C_IS_ID(sym) || !is_typedef_name(rcc, sym))) {
				sym = parse_expression(sym, rcc, val);
				if (sym != YY__SEMICOLON) {
					yy_error_sym("';' expected, got", sym);
				}
				sym = get_sym();
			} else {
				sym = parse_declaration(sym, rcc, C_DCL_STATEMENT);
			}
		}
	}
	return sym;
}

static yy_sym parse_statement(yy_sym sym, rcc_ctx *rcc) {
	c_value val;
	if ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(rcc, sym))) {
		sym = parse_labels(sym, rcc);
	}
	if (sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
		sym = parse_c_statement(sym, rcc);
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		c_value_clear(&val);
		sym = parse_expression(sym, rcc, &val);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY__SEMICOLON) {
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_labels(yy_sym sym, rcc_ctx *rcc) {
	do {
		c_label *label;
		c_name name;
		if (C_IS_ID(sym)) {
			sym = parse_ID(sym, rcc, &name);
			label = c_do_set_label(rcc, name);
			if (sym != YY__COLON) {
				yy_error_sym("':' expected, got", sym);
			}
			sym = get_sym();
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				c_dcl attrs = {0};
				sym = parse_attributes(sym, rcc, &attrs);
				c_do_set_label_attrs(rcc, label, &attrs);
			}
		} else if (sym == YY_CASE) {
			c_value val1;
			sym = get_sym();
			sym = parse_constant_expression(sym, rcc, &val1);
			if (sym == YY__POINT_POINT_POINT) {
				c_value val2;
				sym = get_sym();
				sym = parse_constant_expression(sym, rcc, &val2);
				c_do_case_range(rcc, &val1, &val2);
			} else if (sym == YY__COLON) {
				c_do_case(rcc, &val1);
			} else {
				yy_error_sym("unexpected", sym);
			}
			if (sym != YY__COLON) {
				yy_error_sym("':' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY_DEFAULT) {
			sym = get_sym();
			if (sym != YY__COLON) {
				yy_error_sym("':' expected, got", sym);
			}
			sym = get_sym();
			c_do_case_default(rcc);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(rcc, sym)));
	return sym;
}

static yy_sym parse_c_statement(yy_sym sym, rcc_ctx *rcc) {
	c_value val;
	c_name name;
	c_scope scope;
	c_value_clear(&val);
	if (sym == YY__LBRACE) {
		sym = get_sym();
		c_push_scope(rcc, &scope);
		sym = parse_compound_statement(sym, rcc);
		c_pop_scope(rcc, &scope);
		if (sym != YY__RBRACE) {
			yy_error_sym("'}' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_IF) {
		ir_ref check;
		bool orig_dead_code = rcc->c_dead_code;
		c_push_scope(rcc, &scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, rcc, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		check = c_do_if(rcc, &val);
		sym = parse_statement(sym, rcc);
		if (sym == YY_ELSE) {
			sym = get_sym();
			c_do_if_else(rcc, check, orig_dead_code);
			sym = parse_statement(sym, rcc);
		}
		c_do_if_end(rcc, check, orig_dead_code);
		c_pop_scope(rcc, &scope);
	} else if (sym == YY_SWITCH) {
		c_loop loop;
		c_push_scope(rcc, &scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, rcc, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_switch(rcc, &loop, &val);
		sym = parse_statement(sym, rcc);
		c_do_switch_end(rcc, &loop);
		c_pop_scope(rcc, &scope);
	} else if (sym == YY_WHILE) {
		c_loop loop;
		c_push_scope(rcc, &scope);
		sym = get_sym();
		c_do_loop_start(rcc, &loop);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, rcc, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_check(rcc, &loop, &val);
		sym = parse_statement(sym, rcc);
		c_do_loop_end(rcc, &loop);
		c_pop_scope(rcc, &scope);
	} else if (sym == YY_DO) {
		c_loop loop;
		c_push_scope(rcc, &scope);
		sym = get_sym();
		c_do_loop_start(rcc, &loop);
		sym = parse_statement(sym, rcc);
		c_do_loop_continue_label(rcc, &loop);
		if (sym != YY_WHILE) {
			yy_error_sym("'while' expected, got", sym);
		}
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, rcc, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_check(rcc, &loop, &val);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_end(rcc, &loop);
		c_pop_scope(rcc, &scope);
	} else if (sym == YY_FOR) {
		c_loop loop;
		c_push_scope(rcc, &scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if ((sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR || sym == YY__SEMICOLON) && (!C_IS_ID(sym) || !is_typedef_name(rcc, sym))) {
			if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
				sym = parse_expression(sym, rcc, &val);
			}
			if (sym != YY__SEMICOLON) {
				yy_error_sym("';' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY___FORCEINLINE || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
			sym = parse_declaration(sym, rcc, C_DCL_FOR);
		} else {
			yy_error_sym("unexpected", sym);
		}
		c_do_loop_start(rcc, &loop);
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			sym = parse_expression(sym, rcc, &val);
			c_do_loop_check(rcc, &loop, &val);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			c_do_for_next_start(rcc, &loop);
			sym = parse_expression(sym, rcc, &val);
			c_do_for_next_end(rcc, &loop);
		}
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_statement(sym, rcc);
		c_do_for_end(rcc, &loop);
		c_pop_scope(rcc, &scope);
	} else if (sym == YY_GOTO) {
		sym = get_sym();
		if (C_IS_ID(sym)) {
			sym = parse_ID(sym, rcc, &name);
			c_do_goto(rcc, name);
		} else if (sym == YY__STAR) {
			sym = get_sym();
			sym = parse_expression(sym, rcc, &val);
			c_do_computed_goto(rcc, &val);
		} else {
			yy_error_sym("unexpected", sym);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_CONTINUE) {
		sym = get_sym();
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_continue(rcc);
	} else if (sym == YY_BREAK) {
		sym = get_sym();
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_break(rcc);
	} else if (sym == YY_RETURN) {
		sym = get_sym();
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			sym = parse_expression(sym, rcc, &val);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_return(rcc, &val);
	} else if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
		uint32_t asm_flags = 0;
		sym = get_sym();
		while (sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY_GOTO) {
			if (sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__) {
				sym = get_sym();
				asm_flags |= C_ASM_VOLATILE;
			} else if (sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__) {
				sym = get_sym();
				asm_flags |= C_ASM_INLINE;
			} else {
				sym = get_sym();
				asm_flags |= C_ASM_GOTO;
			}
		}
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_asm_argument(sym, rcc, asm_flags);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_asm_argument(yy_sym sym, rcc_ctx *rcc, uint32_t asm_flags) {
	c_value asm_str;
	c_asm a;
	a.flags = asm_flags;
	a.clobbers = 0;
	int n = 0;
	sym = parse_strings(sym, rcc, &asm_str);
	if (sym == YY__COLON) {
		sym = get_sym();
		if (sym == YY__LBRACK || sym == YY_STRING) {
			sym = parse_asm_operands(sym, rcc, &a, 1, &n);
		}
		if (sym == YY__COLON) {
			sym = get_sym();
			if (sym == YY__LBRACK || sym == YY_STRING) {
				sym = parse_asm_operands(sym, rcc, &a, 0, &n);
			}
			if (sym == YY__COLON) {
				sym = get_sym();
				if (sym == YY_STRING) {
					sym = parse_asm_clobbers(sym, rcc, &a);
				}
				if (sym == YY__COLON) {
					sym = get_sym();
					sym = parse_asm_goto_operands(sym, rcc, &a, &n);
				}
			}
		}
	}
	c_do_asm(rcc, &asm_str, &a, n);
	return sym;
}

static yy_sym parse_asm_operands(yy_sym sym, rcc_ctx *rcc, c_asm *a, bool out, int *n) {
	sym = parse_asm_operand(sym, rcc, a, out, n);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_asm_operand(sym, rcc, a, out, n);
	}
	return sym;
}

static yy_sym parse_asm_operand(yy_sym sym, rcc_ctx *rcc, c_asm *a, bool out, int *n) {
	c_name name = 0;
	c_value val;
	if (*n >= C_MAX_ASM_OPERANDS) yy_error("too many asm opernads");
	if (sym == YY__LBRACK) {
		sym = get_sym();
		sym = parse_ID(sym, rcc, &name);
		if (sym != YY__RBRACK) {
			yy_error_sym("']' expected, got", sym);
		}
		sym = get_sym();
	}
	sym = parse_strings(sym, rcc, &val);
	c_do_asm_operand_constraint(rcc, a, out, *n, name, &val);
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	c_value_clear(&val);
	sym = parse_expression(sym, rcc, &val);
	c_do_asm_operand_val(rcc, a, out, (*n)++, &val);
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_asm_clobbers(yy_sym sym, rcc_ctx *rcc, c_asm *a) {
	c_value val;
	yy_read_string(rcc, &val, rcc->yy_text, rcc->yy_len);
	sym = parse_STRING(sym, rcc);
	c_do_asm_clobbers(rcc, a, &val);
	while (sym == YY__COMMA) {
		sym = get_sym();
		yy_read_string(rcc, &val, rcc->yy_text, rcc->yy_len);
		sym = parse_STRING(sym, rcc);
		c_do_asm_clobbers(rcc, a, &val);
	}
	return sym;
}

static yy_sym parse_asm_goto_operands(yy_sym sym, rcc_ctx *rcc, c_asm *a, int *n) {
	c_name name;
	if (*n >= C_MAX_ASM_OPERANDS) yy_error("too many asm opernads");
	sym = parse_ID(sym, rcc, &name);
	c_do_asm_operand_label(rcc, a, (*n)++, name);
	while (sym == YY__COMMA) {
		sym = get_sym();
		if (*n >= C_MAX_ASM_OPERANDS) yy_error("too many asm opernads");
		sym = parse_ID(sym, rcc, &name);
		c_do_asm_operand_label(rcc, a, (*n)++, name);
	}
	return sym;
}

static yy_sym parse_strings(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	const char *str = rcc->yy_text;
	size_t len = rcc->yy_len;
	sym = parse_STRING(sym, rcc);
	if (sym == YY__RPAREN || sym == YY__COLON || sym == YY__LPAREN || sym == YY__LBRACK || sym == YY__POINT || sym == YY__MINUS_GREATER || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT || sym == YY__QUERY || sym == YY__EQUAL || sym == YY__STAR_EQUAL || sym == YY__SLASH_EQUAL || sym == YY__PERCENT_EQUAL || sym == YY__PLUS_EQUAL || sym == YY__MINUS_EQUAL || sym == YY__LESS_LESS_EQUAL || sym == YY__GREATER_GREATER_EQUAL || sym == YY__AND_EQUAL || sym == YY__UPARROW_EQUAL || sym == YY__BAR_EQUAL || sym == YY__RBRACK || sym == YY__COMMA || sym == YY__SEMICOLON || sym == YY__RBRACE || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED || sym == YY__POINT_POINT_POINT) {
		yy_read_string(rcc, val, str, len);
	} else if (sym == YY_STRING) {
		uint32_t num_strings = 1;
		yy_str *strings = alloca(sizeof(yy_str) * C_ALLOCA_STRINGS);
		strings[0].str = str; strings[0].len = len;
		do {
			if (num_strings % C_ALLOCA_STRINGS == 0) strings = yy_grow_strings(rcc, strings, num_strings);
			strings[num_strings].str = rcc->yy_text; strings[num_strings].len = rcc->yy_len;
			num_strings++;
			sym = parse_STRING(sym, rcc);
		} while (sym == YY_STRING);
		yy_read_strings(rcc, val, strings, num_strings);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_actual_parameters(yy_sym sym, rcc_ctx *rcc, c_value *func, c_value *res) {
	uint32_t num_args = 0;
	c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		c_value_clear(&args[num_args]);
		sym = parse_assignment_expression(sym, rcc, &args[num_args]);
		num_args++;
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (num_args % C_ALLOCA_PARAMS == 0) args = c_do_grow_actual_parameters(rcc, args, num_args);
			c_value_clear(&args[num_args]);
			sym = parse_assignment_expression(sym, rcc, &args[num_args]);
			num_args++;
		}
	}
	c_do_call(rcc, func, num_args, args, res);
	return sym;
}

static yy_sym parse_builtin_parameters(yy_sym sym, rcc_ctx *rcc, c_value *val, c_name name) {
	uint32_t num_args = 0;
	c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		sym = parse_assignment_expression(sym, rcc, &args[num_args]);
		num_args++;
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (num_args % C_ALLOCA_PARAMS == 0) args = c_do_grow_actual_parameters(rcc, args, num_args);
			sym = parse_assignment_expression(sym, rcc, &args[num_args]);
			num_args++;
		}
	}
	c_do_builtin(rcc, val, name, num_args, args);
	return sym;
}

static yy_sym parse_dummy_value(yy_sym sym, rcc_ctx *rcc, const c_type *t) {
	ir_ref old_control = c_do_nocode(rcc);
	c_value val;
	c_sym obj;
	size_t size = t->size;
	c_do_init_expr_start(rcc, &obj, t);
	sym = parse_initializer_contents(sym, rcc, &obj, &size);
	c_do_init_expr_end(rcc, &val, &obj, size);
	c_do_end_nocode(rcc, old_control);
	return sym;
}

static yy_sym parse_unary_expression(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	c_name name;
	const c_type *t;
	c_value v;
	ir_ref old_control = IR_UNUSED;
	yy_sym op = sym;
	if (sym == YY__LPAREN) {
		sym = get_sym();
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
			sym = parse_type_name(sym, rcc, &t);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			if (sym == YY__LBRACE) {
				c_sym obj;
				size_t size = t->size;
				c_do_init_expr_start(rcc, &obj, t);
				sym = parse_initializer_contents(sym, rcc, &obj, &size);
				c_do_init_expr_end(rcc, &v, &obj, size);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
				c_value_clear(&v);
				sym = parse_unary_expression(sym, rcc, &v);
				c_do_cast(rcc, t, &v);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			v.u.optx = val->u.optx;
			sym = parse_expression(sym, rcc, &v);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__LBRACE) {
			c_scope scope;
			sym = get_sym();
			c_do_statement_expression(rcc, &scope, &v);
			sym = parse_expression_statement(sym, rcc, &v);
			c_pop_scope(rcc, &scope);
			if (sym != YY__RBRACE) {
				yy_error_sym("'}' expected, got", sym);
			}
			sym = get_sym();
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (C_IS_ID(sym)) {
		sym = parse_ID(sym, rcc, &name);
		c_resolve_sym_name(rcc, &v, name, sym);
	} else if (sym == YY_DECIMAL_NUMBER) {
		sym = parse_DECIMAL_NUMBER(sym, rcc, &v);
	} else if (sym == YY_OCTAL_NUMBER) {
		sym = parse_OCTAL_NUMBER(sym, rcc, &v);
	} else if (sym == YY_HEXADECIMAL_NUMBER) {
		sym = parse_HEXADECIMAL_NUMBER(sym, rcc, &v);
	} else if (sym == YY_BINARY_NUMBER) {
		sym = parse_BINARY_NUMBER(sym, rcc, &v);
	} else if (sym == YY_FLOATING_NUMBER) {
		sym = parse_FLOATING_NUMBER(sym, rcc, &v);
	} else if (sym == YY_HEXADECIMAL_FLOATING_NUMBER) {
		sym = parse_HEXADECIMAL_FLOATING_NUMBER(sym, rcc, &v);
	} else if (sym == YY_CHARACTER) {
		sym = parse_CHARACTER(sym, rcc, &v);
	} else if (sym == YY_STRING) {
		sym = parse_strings(sym, rcc, &v);
	} else if (sym == YY__GENERIC) {
		c_generic g;
		bool is_type;
		sym = get_sym();
		c_do_generic_start(rcc, &g);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		c_value_clear(&v);
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
			sym = parse_type_name(sym, rcc, &v.type);
			is_type = 1;
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			sym = parse_assignment_expression(sym, rcc, &v);
			is_type = 0;
		} else {
			yy_error_sym("unexpected", sym);
		}
		c_do_generic_type(rcc, &g, v.type, is_type);
		if (sym != YY__COMMA) {
			yy_error_sym("',' expected, got", sym);
		}
		do {
			sym = get_sym();
			if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) {
				sym = parse_type_name(sym, rcc, &t);
				if (sym != YY__COLON) {
					yy_error_sym("':' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_assignment_expression(sym, rcc, &v);
				c_do_generic_case(rcc, &g, t, &v);
			} else if (sym == YY_DEFAULT) {
				sym = get_sym();
				if (sym != YY__COLON) {
					yy_error_sym("':' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_assignment_expression(sym, rcc, &v);
				c_do_generic_default(rcc, &g, &v);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} while (sym == YY__COMMA);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_generic_end(rcc, &v, &g);
	} else if (sym == YY___EXTENSION__) {
		sym = get_sym();
		v.u.optx = val->u.optx;
		sym = parse_unary_expression(sym, rcc, &v);
	} else if (sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS) {
		sym = get_sym();
		c_value_clear(&v);
		sym = parse_unary_expression(sym, rcc, &v);
		c_do_pre_op(rcc, op, &v);
	} else if (sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG) {
		c_value_clear(&v);
		if (sym == YY__AND) {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_addr(rcc, &v);
		} else if (sym == YY__STAR) {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_deref(rcc, &v);
		} else if (sym == YY__PLUS) {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_unary_plus(rcc, &v);
		} else if (sym == YY__MINUS) {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_neg(rcc, &v);
		} else if (sym == YY__TILDE) {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_not(rcc, &v);
		} else {
			sym = get_sym();
			sym = parse_unary_expression(sym, rcc, &v);
			c_do_bool_not(rcc, &v);
		}
	} else if (sym == YY_SIZEOF) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
				sym = parse_type_name(sym, rcc, &t);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
				if (sym == YY__LBRACE) {
					sym = parse_dummy_value(sym, rcc, t);
				}
				c_sizeof_type(rcc, &v, t);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
				old_control = c_do_nocode(rcc);
				c_value_clear(&v);
				sym = parse_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY__LBRACE) {
				c_scope scope;
				sym = get_sym();
				c_do_statement_expression(rcc, &scope, &v);
				sym = parse_expression_statement(sym, rcc, &v);
				c_pop_scope(rcc, &scope);
				if (sym != YY__RBRACE) {
					yy_error_sym("'}' expected, got", sym);
				}
				sym = get_sym();
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			ir_ref old = c_do_nocode(rcc);
			c_value_clear(&v);
			sym = parse_unary_expression(sym, rcc, &v);
			c_sizeof_expr(rcc, op, &v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__ALIGNOF) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, rcc, &t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_alignof_type(rcc, &v, t);
	} else if (sym == YY___ALIGNOF__ || sym == YY___ALIGNOF) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
				sym = parse_type_name(sym, rcc, &t);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
				if (sym == YY__LBRACE) {
					sym = parse_dummy_value(sym, rcc, t);
				}
				c_alignof_type(rcc, &v, t);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
				old_control = c_do_nocode(rcc);
				c_value_clear(&v);
				sym = parse_expression(sym, rcc, &v);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY__LBRACE) {
				c_scope scope;
				sym = get_sym();
				c_do_statement_expression(rcc, &scope, &v);
				sym = parse_expression_statement(sym, rcc, &v);
				c_pop_scope(rcc, &scope);
				if (sym != YY__RBRACE) {
					yy_error_sym("'}' expected, got", sym);
				}
				sym = get_sym();
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			ir_ref old = c_do_nocode(rcc);
			c_value_clear(&v);
			sym = parse_unary_expression(sym, rcc, &v);
			c_sizeof_expr(rcc, op, &v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__AND_AND) {
		sym = get_sym();
		sym = parse_ID(sym, rcc, &name);
		c_do_label_value(rcc, &v, name);
	} else if (sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR) {
		name = sym;
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_builtin_parameters(sym, rcc, &v, name);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY___BUILTIN_CONSTANT_P) {
		ir_ref old = c_do_nocode(rcc);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		c_value_clear(&v);
		sym = parse_assignment_expression(sym, rcc, &v);
		c_do_end_nocode(rcc, old);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_builtin_constant_p(rcc, &v);
	} else if (sym == YY___BUILTIN_CLASSIFY_TYPE) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___INT128 || sym == YY___INT128_T || sym == YY___UINT128_T || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY___DECLSPEC || sym == YY___CDECL || sym == YY___FASTCALL || sym == YY___UNALIGNED) && (!C_IS_ID(sym) || is_typedef_name(rcc, sym))) {
			sym = parse_type_name(sym, rcc, &t);
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
			ir_ref old = c_do_nocode(rcc);
			c_value_clear(&v);
			sym = parse_assignment_expression(sym, rcc, &v);
			t = v.type;
			c_do_end_nocode(rcc, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
		c_do_builtin_classify_type(rcc, &v, t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY___BUILTIN_TYPES_COMPATIBLE_P) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, rcc, &v.type);
		if (sym != YY__COMMA) {
			yy_error_sym("',' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, rcc, &t);
		c_do_builtin_types_compatible_p(rcc, &v, t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY___BUILTIN_VA_ARG) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		c_value_clear(&v);
		sym = parse_assignment_expression(sym, rcc, &v);
		if (sym != YY__COMMA) {
			yy_error_sym("',' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, rcc, &t);
		c_do_builtin_va_arg(rcc, &v, t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY___BUILTIN_CONVERTVECTOR) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_assignment_expression(sym, rcc, &v);
		if (sym != YY__COMMA) {
			yy_error_sym("',' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, rcc, &t);
		c_do_builtin_convertvector(rcc, &v, t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	while (sym == YY__LBRACK || sym == YY__LPAREN || sym == YY__POINT || sym == YY__MINUS_GREATER || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS) {
		if (sym == YY__LBRACK) {
			c_value dim;
			c_value_clear(&dim);
			sym = get_sym();
			sym = parse_expression(sym, rcc, &dim);
			if (sym != YY__RBRACK) {
				yy_error_sym("']' expected, got", sym);
			}
			sym = get_sym();
			c_do_array_dim(rcc, &v, &dim);
		} else if (sym == YY__LPAREN) {
			sym = get_sym();
			sym = parse_actual_parameters(sym, rcc, &v, val);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__POINT) {
			sym = get_sym();
			sym = parse_ID(sym, rcc, &name);
			c_do_struct_field(rcc, &v, name);
		} else if (sym == YY__MINUS_GREATER) {
			sym = get_sym();
			sym = parse_ID(sym, rcc, &name);
			c_do_struct_field_deref(rcc, &v, name);
		} else {
			yy_sym post_op = sym;
			sym = get_sym();
			c_do_post_op(rcc, post_op, &v);
		}
	}
	if (old_control) c_sizeof_expr(rcc, op, &v, old_control);
	*val = v;
	return sym;
}

static yy_sym parse_infix_expression(yy_sym sym, rcc_ctx *rcc, c_value *val, yy_sym prev) {
	c_value op2;
	ir_ref if_ref = IR_UNUSED;
	bool orig_dead_code = 0;
	do {
		yy_sym next, op = sym;
		if (sym == YY__BAR_BAR) {
			orig_dead_code = rcc->c_dead_code;
			if_ref = c_do_bool_or_start(rcc, val);
			sym = get_sym();
			next = YY__BAR_BAR;
		} else if (sym == YY__AND_AND) {
			orig_dead_code = rcc->c_dead_code;
			if_ref = c_do_bool_and_start(rcc, val);
			sym = get_sym();
			next = YY__AND_AND;
		} else if (sym == YY__BAR) {
			sym = get_sym();
			next = YY__BAR;
		} else if (sym == YY__UPARROW) {
			sym = get_sym();
			next = YY__UPARROW;
		} else if (sym == YY__AND) {
			sym = get_sym();
			next = YY__AND;
		} else if (sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL) {
			sym = get_sym();
			next = YY__EQUAL_EQUAL;
		} else if (sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL) {
			sym = get_sym();
			next = YY__LESS;
		} else if (sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
			sym = get_sym();
			next = YY__LESS_LESS;
		} else if (sym == YY__PLUS || sym == YY__MINUS) {
			sym = get_sym();
			next = YY__PLUS;
		} else if (sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
			sym = get_sym();
			next = YY__STAR;
		} else {
			yy_error_sym("unexpected", sym);
		}
		c_value_clear(&op2);
		sym = parse_unary_expression(sym, rcc, &op2);
		if ((sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) && (sym >= YY__STAR && sym < next)) {
			sym = parse_infix_expression(sym, rcc, &op2, next - 1);
		}
		if (op == YY__BAR_BAR) {
			c_do_bool_or_end(rcc, val, &op2, if_ref);
			rcc->c_dead_code = orig_dead_code;
		} else if (op == YY__AND_AND) {
			c_do_bool_and_end(rcc, val, &op2, if_ref);
			rcc->c_dead_code = orig_dead_code;
		} else {
			c_do_binary_op(rcc, op, val, &op2);
		}
	} while ((sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) && (sym <= prev));
	return sym;
}

static yy_sym parse_conditional_expression(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	ir_ref check;
	bool orig_dead_code = rcc->c_dead_code;
	c_value op2, op3;
	c_value_clear(&op2);
	if (sym != YY__QUERY) {
		yy_error_sym("'?' expected, got", sym);
	}
	sym = get_sym();
	check = c_do_if(rcc, val);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_FFS || sym == YY___BUILTIN_FFSL || sym == YY___BUILTIN_FFSLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_PREFETCH || sym == YY___BUILTIN_UNREACHABLE || sym == YY___BUILTIN_HUGE_VAL || sym == YY___BUILTIN_HUGE_VALF || sym == YY___BUILTIN_INF || sym == YY___BUILTIN_INFF || sym == YY___BUILTIN_ISUNORDERED || sym == YY___BUILTIN_NAN || sym == YY___BUILTIN_NANF || sym == YY___BUILTIN_ADD_OVERFLOW || sym == YY___BUILTIN_ADD_OVERFLOW_P || sym == YY___BUILTIN_SADD_OVERFLOW || sym == YY___BUILTIN_SADDL_OVERFLOW || sym == YY___BUILTIN_SADDLL_OVERFLOW || sym == YY___BUILTIN_UADD_OVERFLOW || sym == YY___BUILTIN_UADDL_OVERFLOW || sym == YY___BUILTIN_UADDLL_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW || sym == YY___BUILTIN_SUB_OVERFLOW_P || sym == YY___BUILTIN_SSUB_OVERFLOW || sym == YY___BUILTIN_SSUBL_OVERFLOW || sym == YY___BUILTIN_SSUBLL_OVERFLOW || sym == YY___BUILTIN_USUB_OVERFLOW || sym == YY___BUILTIN_USUBL_OVERFLOW || sym == YY___BUILTIN_USUBLL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW || sym == YY___BUILTIN_MUL_OVERFLOW_P || sym == YY___BUILTIN_SMUL_OVERFLOW || sym == YY___BUILTIN_SMULL_OVERFLOW || sym == YY___BUILTIN_SMULLL_OVERFLOW || sym == YY___BUILTIN_UMUL_OVERFLOW || sym == YY___BUILTIN_UMULL_OVERFLOW || sym == YY___BUILTIN_UMULLL_OVERFLOW || sym == YY___BUILTIN_SHUFFLE || sym == YY___BUILTIN_SHUFFLEVECTOR || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_CLASSIFY_TYPE || sym == YY___BUILTIN_TYPES_COMPATIBLE_P || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_CONVERTVECTOR) {
		sym = parse_expression(sym, rcc, &op2);
		c_value_rval(rcc, &op2);
	}
	if (sym != YY__COLON) {
		yy_error_sym("':' expected, got", sym);
	}
	sym = get_sym();
	c_do_if_else(rcc, check, orig_dead_code);
	sym = parse_unary_expression(sym, rcc, &op3);
	if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
		sym = parse_infix_expression(sym, rcc, &op3, YY__BAR_BAR);
	}
	if (sym == YY__QUERY) {
		sym = parse_conditional_expression(sym, rcc, &op3);
	}
	c_value_rval(rcc, &op3);
	c_do_if_end(rcc, check, orig_dead_code);
	c_do_cond_op(rcc, val, &op2, &op3);
	return sym;
}

static yy_sym parse_assignment_expression(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	sym = parse_unary_expression(sym, rcc, val);
	if (sym == YY__EQUAL || sym == YY__STAR_EQUAL || sym == YY__SLASH_EQUAL || sym == YY__PERCENT_EQUAL || sym == YY__PLUS_EQUAL || sym == YY__MINUS_EQUAL || sym == YY__LESS_LESS_EQUAL || sym == YY__GREATER_GREATER_EQUAL || sym == YY__AND_EQUAL || sym == YY__UPARROW_EQUAL || sym == YY__BAR_EQUAL) {
		int op = sym;
		c_value op2;
		c_value_clear(&op2);
		sym = get_sym();
		sym = parse_assignment_expression(sym, rcc, &op2);
		c_do_assign_op(rcc, op, val, &op2);
	} else if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT || sym == YY__QUERY || sym == YY__RBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__SEMICOLON || sym == YY__RBRACE || sym == YY__COLON) {
		if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
			sym = parse_infix_expression(sym, rcc, val, YY__BAR_BAR);
		}
		if (sym == YY__QUERY) {
			sym = parse_conditional_expression(sym, rcc, val);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_expression(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	sym = parse_assignment_expression(sym, rcc, val);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_assignment_expression(sym, rcc, val);
	}
	return sym;
}

static yy_sym parse_constant_expression(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	sym = parse_unary_expression(sym, rcc, val);
	if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
		sym = parse_infix_expression(sym, rcc, val, YY__BAR_BAR);
	}
	if (sym == YY__QUERY) {
		sym = parse_conditional_expression(sym, rcc, val);
	}
	return sym;
}

static yy_sym parse_ID(yy_sym sym, rcc_ctx *rcc, c_name *name) {
	if (!C_IS_ID(sym)) {
		yy_error_sym("<ID> expected, got", sym);
	}
	*name = sym;
	sym = get_sym();
	return sym;
}

static yy_sym parse_DECIMAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_DECIMAL_NUMBER) {
		yy_error_sym("<DECIMAL_NUMBER> expected, got", sym);
	}
	yy_read_dec(val, rcc->yy_text, rcc->yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_OCTAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_OCTAL_NUMBER) {
		yy_error_sym("<OCTAL_NUMBER> expected, got", sym);
	}
	yy_read_oct(val, rcc->yy_text, rcc->yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_HEXADECIMAL_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_HEXADECIMAL_NUMBER) {
		yy_error_sym("<HEXADECIMAL_NUMBER> expected, got", sym);
	}
	yy_read_hex(val, rcc->yy_text + 2, rcc->yy_len - 2);
	sym = get_sym();
	return sym;
}

static yy_sym parse_BINARY_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_BINARY_NUMBER) {
		yy_error_sym("<BINARY_NUMBER> expected, got", sym);
	}
	yy_read_bin(val, rcc->yy_text + 2, rcc->yy_len - 2);
	sym = get_sym();
	return sym;
}

static yy_sym parse_FLOATING_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_FLOATING_NUMBER) {
		yy_error_sym("<FLOATING_NUMBER> expected, got", sym);
	}
	yy_read_fp(val, rcc->yy_text, rcc->yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_HEXADECIMAL_FLOATING_NUMBER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_HEXADECIMAL_FLOATING_NUMBER) {
		yy_error_sym("<HEXADECIMAL_FLOATING_NUMBER> expected, got", sym);
	}
	yy_read_fp(val, rcc->yy_text, rcc->yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_CHARACTER(yy_sym sym, rcc_ctx *rcc, c_value *val) {
	if (sym != YY_CHARACTER) {
		yy_error_sym("<CHARACTER> expected, got", sym);
	}
	yy_read_char(rcc, val, rcc->yy_text, rcc->yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_STRING(yy_sym sym, rcc_ctx *rcc) {
	if (sym != YY_STRING) {
		yy_error_sym("<STRING> expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

enum {
	YY_UNSIGNED_INT       = IR_U32,
	YY_UNSIGNED_LONG      = IR_ULONG,
	YY_UNSIGNED_LONG_LONG = IR_U64,
	YY_SIGNED_LONG        = IR_LONG,
	YY_SIGNED_LONG_LONG   = IR_I64,
};

static const char *yy_read_int_suffix(const c_type **ctype, ir_type *type, const char *e)
{
	char ch = *(e - 1);

	if (ch == 'u' || ch == 'U') {
		e--;
		ch = *(e - 1);
		if (ch == 'l' || ch == 'L') {
			e--;
			ch = *(e - 1);
			if (ch == 'l' || ch == 'L') {
				e--;
				*ctype = &c_type_ull;
				*type = (ir_type)YY_UNSIGNED_LONG_LONG;
			} else {
				*ctype = &c_type_ul;
				*type = (ir_type)YY_UNSIGNED_LONG;
			}
		} else {
			*ctype = &c_type_u32;
			*type = (ir_type)YY_UNSIGNED_INT;
		}
	} else if (ch == 'l' || ch == 'L') {
		e--;
		ch = *(e - 1);
		if (ch == 'u' || ch == 'U') {
			e--;
			*ctype = &c_type_ul;
			*type = (ir_type)YY_UNSIGNED_LONG;
		} else if (ch == 'l' || ch == 'L') {
			e--;
			ch = *(e - 1);
			if (ch == 'u' || ch == 'U') {
				e--;
				*ctype = &c_type_ull;
				*type = (ir_type)YY_UNSIGNED_LONG_LONG;
			} else {
				*ctype = &c_type_ill;
				*type = (ir_type)YY_SIGNED_LONG_LONG;
			}
		} else {
			*ctype = &c_type_il;
			*type = (ir_type)YY_SIGNED_LONG;
		}
	}
	return e;
}

static void yy_check_int_type(const c_type **ctype, ir_type *type, ir_val val, bool is_dec)
{
	if (!*ctype) {
		if (val.u64 > 0x7fffffffffffffff) {
			*ctype = &c_type_u64;
			*type = IR_U64;
		} else if (val.u64 > 0x7fffffff) {
			if (is_dec || val.u64 > 0xffffffff) {
				*ctype = &c_type_i64;
				*type = IR_I64;
			} else {
				*ctype = &c_type_u32;
				*type = IR_U32;
			}
		} else {
			*ctype = &c_type_i32;
			*type = IR_I32;
	    }
	} else if ((*ctype)->size == 4) {
		if (C_IS_TYPE_UNSIGNED(*ctype)) {
			if (val.u64 > 0xffffffff) {
				*ctype = &c_type_u64;
				*type = IR_U64;
			}
		} else if (C_IS_TYPE_SIGNED(*ctype)) {
			if (val.u64 > 0x7fffffff) {
				if (is_dec || val.u64 > 0xffffffff) {
					*ctype = &c_type_i64;
					*type = IR_I64;
				} else {
					*ctype = &c_type_u32;
					*type = IR_U32;
				}
			}
		}
	}
}

static void yy_read_oct(c_value *res, const char *p, size_t len)
{
	const c_type *ctype = NULL;
	ir_type type = 0;
	ir_val val;
	uint64_t ret;
	char ch;
	const char *e = yy_read_int_suffix(&ctype, &type, p + len);

	ch = *p;
	IR_ASSERT(ch >= '0' && ch <= '7');
	ret = ch - '0';
	while (++p < e) {
		ch = *p;
		IR_ASSERT(ch >= '0' && ch <= '7');
		ret = (ret << 3) + (ch - '0');
	}
	if (ret >= 0x8000000000000000ULL) {
		ctype = &c_type_u64;
		type = IR_U64;
	}
	val.u64 = ret;
	yy_check_int_type(&ctype, &type, val, 0);
	c_value_set_const(res, ctype, type, val);
}

static void yy_read_dec(c_value *res, const char *p, size_t len)
{
	const c_type *ctype = NULL;
	ir_type type = 0;
	ir_val val;
	uint64_t ret;
	char ch;
	const char *e = yy_read_int_suffix(&ctype, &type, p + len);

	ch = *p;
	IR_ASSERT(ch >= '0' && ch <= '9');
	ret = ch - '0';
	while (++p < e) {
		ch = *p;
		IR_ASSERT(ch >= '0' && ch <= '9');
		ret = (ret * 10) + (ch - '0');
	}
	val.u64 = ret;
	yy_check_int_type(&ctype, &type, val, 1);
	c_value_set_const(res, ctype, type, val);
}

static void yy_read_hex(c_value *res, const char *p, size_t len)
{
	const c_type *ctype = NULL;
	ir_type type = 0;
	ir_val val;
	uint64_t ret = 0;
	char ch;
	const char *e = yy_read_int_suffix(&ctype, &type, p + len);

	ch = *p;
	if (ch >= '0' && ch <= '9') {
		ret = ch - '0';
	} else if (ch >= 'a' && ch <= 'f') {
		ret = ch - 'a' + 10;
	} else {
		IR_ASSERT(ch >= 'A' && ch <= 'F');
		ret = ch - 'A' + 10;
	}
	while (++p < e) {
		ch = *p;
		if (ch >= '0' && ch <= '9') {
			ret = (ret << 4) | (ch - '0');
		} else if (ch >= 'a' && ch <= 'f') {
			ret = (ret << 4) | (ch - 'a' + 10);
		} else {
			IR_ASSERT(ch >= 'A' && ch <= 'F');
			ret = (ret << 4) | (ch - 'A' + 10);
		}
	}
	if (ret >= 0x8000000000000000ULL) {
		ctype = &c_type_u64;
		type = IR_U64;
	}
	val.u64 = ret;
	yy_check_int_type(&ctype, &type, val, 0);
	c_value_set_const(res, ctype, type, val);
}

static void yy_read_bin(c_value *res, const char *p, size_t len)
{
	const c_type *ctype = NULL;
	ir_type type = 0;
	ir_val val;
	uint64_t ret = 0;
	char ch;
	const char *e = yy_read_int_suffix(&ctype, &type, p + len);

	ch = *p;
	ret = ch - '0';
	while (++p < e) {
		ch = *p;
		ret = (ret << 1) | (ch - '0');
	}
	if (ret >= 0x8000000000000000ULL) {
		ctype = &c_type_u64;
		type = IR_U64;
	}
	val.u64 = ret;
	yy_check_int_type(&ctype, &type, val, 0);
	c_value_set_const(res, ctype, type, val);
}

static void yy_read_fp(c_value *res, const char *p, size_t len)
{
	const c_type *ctype = NULL;
	ir_type type = 0;
	ir_val val;
	char ch;
	const char *e = p + len;

	ch = *(e - 1);
	if (ch == 'f' || ch == 'F') {
		e--;
		ctype = &c_type_float;
		type = IR_FLOAT;
		val.f = strtof(p, NULL);
		val.u32_hi = 0;
	} else if (ch == 'l' || ch == 'L') {
		e--;
		// TODO: long double (strtold) ???
		ctype = &c_type_double;
		type = IR_DOUBLE;
		val.d = strtod(p, NULL);
	} else {
		ctype = &c_type_double;
		type = IR_DOUBLE;
		val.d = strtod(p, NULL);
	}
	c_value_set_const(res, ctype, type, val);
}

static uint32_t yy_read_unicode_character(rcc_ctx *rcc, const char *str, size_t len)
{
	char ch;
	const char *p = str;
	uint32_t n = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		ch = *p++;
		if (ch >= '0' && ch <= '9') {
			n = (n << 4) + (ch - '0');
		} else if (ch >= 'a' && ch <= 'f') {
			n = (n << 4) + (ch - 'a' + 10);
		} else if (ch >= 'A' && ch <= 'F') {
			n = (n << 4) + (ch - 'A' + 10);
		} else {
			yy_error("incomplete universal character");
		}
	}
	if (n > 0x10ffff || (n >= 0xd800 && n <= 0xdfff)) {
		yy_error_fmt("bad universal character 0x%x", n);
	}

	return n;
}

static uint32_t yy_read_escape_sequence(rcc_ctx *rcc, char first_ch, const char **str_ptr)
{
	uint32_t ch;
	const char *p = *str_ptr;

	ch = (unsigned char)first_ch;
	switch (ch) {
		case '\\': ch = '\\'; break;
		case '\'': ch = '\''; break;
		case '"':  ch = '"';  break;
		case 'a':  ch = '\a'; break;
		case 'b':  ch = '\b'; break;
		case 'e':  ch = 27;   break; /* '\e'; */
		case 'E':  ch = 27;   break; /* '\E'; */
		case 'f':  ch = '\f'; break;
		case 'n':  ch = '\n'; break;
		case 'r':  ch = '\r'; break;
		case 't':  ch = '\t'; break;
		case 'v':  ch = '\v'; break;
		case '?':  ch = 0x3f; break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			ch = ch - '0';
			if (*p >= '0' && *p <= '7') {
				ch = ch * 8 + (*p - '0');
				p++;
				if (*p >= '0' && *p <= '7') {
					ch = ch * 8 + (*p - '0');
					p++;
				}
			}
			break;
		case 'x':
			ch = *p++;
			if (ch >= '0' && ch <= '9') {
				ch = ch - '0';
			} else if (ch >= 'a' && ch <= 'f') {
				ch = ch - 'a' + 10;
			} else if (ch >= 'A' && ch <= 'F') {
				ch = ch - 'A' + 10;
			} else {
				yy_error("unsupported escape sequence");
			}
			if (*p >= '0' && *p <= '9') {
				ch = (ch << 4) + (*p - '0');
				p++;
			} else if (*p >= 'a' && *p <= 'f') {
				ch = (ch << 4) + (*p - 'a' + 10);
				p++;
			} else if (*p >= 'A' && *p <= 'F') {
				ch = (ch << 4) + (*p - 'A' + 10);
				p++;
			}
			break;
		case 'u':
			ch = yy_read_unicode_character(rcc, p, 4);
			p += 4;
			break;
		case 'U':
			ch = yy_read_unicode_character(rcc, p, 8);
			p += 8;
			break;
		default:
			yy_error("unsupported escape sequence");
			break;
	}

	*str_ptr = p;
	return ch;
}

static uint32_t yy_read_multi_char(rcc_ctx *rcc, uint32_t res, const char *p)
{
	uint32_t ch;

	while (*p != '\'') {
		ch = (unsigned char)*p++;
		if (ch == '\\') {
			ch = (unsigned char)*p++;
			if (ch == '\n') {
				ch = (unsigned char)*p++;
				continue;
			}
			ch = yy_read_escape_sequence(rcc, ch, &p);
		}
		res = (res << 8) + ch;
	}
	return res;
}

static uint32_t yy_read_utf8_char(rcc_ctx *rcc, char first_ch, const char **str_ptr)
{
	const char *p = *str_ptr;
	uint32_t uc;
	uint32_t c = (unsigned char)first_ch;

	if (c < 0xc2) goto bad_utf8;
	if (c < 0xe0) {
		uc = ((c & 0x1f) << 6);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= (c & 0x3f);
		if (uc < 0x80) goto bad_utf8;
	} else if (c < 0xf0) {
		uc = ((c & 0x0f) << 12);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= ((c & 0x3f) << 6);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= (c & 0x3f);
		if (uc < 0x800) goto bad_utf8;
		if (uc >= 0xd800 && uc <= 0xdfff) goto bad_utf8; /* surrogate */
	} else if (c < 0xf5) {
		uc = ((c & 0x07) << 18);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= ((c & 0x3f) << 12);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= ((c & 0x3f) << 6);
		c = (unsigned char)*p++;
		if (c < 0x80 || c > 0xbf) goto bad_utf8;
		uc |= (c & 0x3f);
		if (uc < 0x10000 || uc > 0x10ffff) goto bad_utf8;
	} else {
bad_utf8:
		yy_error("bad UTF-8 sequence");
	}
	*str_ptr = p;
	return uc;
}

static void yy_read_char(rcc_ctx *rcc, c_value *res, const char *p, size_t len)
{
	ir_val val;
	char prefix = 0;
	bool warn = 1;
	uint32_t ch = (unsigned char)*p++;

	if (ch == 'L' || ch == 'u' || ch == 'U') {
		prefix = ch;
		ch = (unsigned char)*p++;
	}

	IR_ASSERT(ch == '\'');

	ch = (unsigned char)*p++;
	if (ch == '\'') yy_error("empty character constant");
restart:
	if (prefix && ch > 0x7f) {
		ch = yy_read_utf8_char(rcc, ch, &p);
	} else if (ch == '\\') {
		ch = (unsigned char)*p++;
		if (ch == '\n') {
			ch = *p++;
			goto restart;
		}
		ch = yy_read_escape_sequence(rcc, ch, &p);
	}
	if (UNEXPECTED(*p != '\'')) {
		if (warn) yy_warning("multi-character character constant");
		warn = 0;
		if (prefix) goto restart;
		ch = yy_read_multi_char(rcc, ch, p);
	}
	if (!prefix) {
		val.i64 = (int)ch;
		c_value_set_const(res, &c_type_i32, IR_I32, val);
	} else if (prefix == 'L') {
		if (C_WCHAR_SIGNED) {
			val.i64 = (int)ch;
		} else {
			val.u64 = ch;
		}
		c_value_set_const(res, &c_type_wchar_t, IR_WCHAR, val);
	} else if (prefix == 'u') {
		val.u64 = ch & 0xffff;
		c_value_set_const(res, &c_type_u16, IR_U16, val);
	} else {
		IR_ASSERT(prefix == 'U');
		val.u64 = ch;
		c_value_set_const(res, &c_type_u32, IR_U32, val);
	}
}

static void yy_append_utf8(rcc_ctx *rcc, yy_dyn_str *dyn_str, uint32_t n)
{
	char buf[4];
	size_t len;

	if (n <= 0x7f) {
		buf[0] = (char)n;
		len = 1;
	} else if (n <= 0x07ff) {
		buf[0] = (char) (((n >> 6) & 0x1f) | 0xc0);
		buf[1] = (char) (((n >> 0) & 0x3f) | 0x80);
		len = 2;
	} else if (n <= 0xffff) {
		buf[0] = (char) (((n >> 12) & 0x0f) | 0xe0);
		buf[1] = (char) (((n >>  6) & 0x3f) | 0x80);
		buf[2] = (char) (((n >>  0) & 0x3f) | 0x80);
		len = 3;
	} else if (n <= 0x10ffff) {
		buf[0] = (char) (((n >> 18) & 0x07) | 0xf0);
		buf[1] = (char) (((n >> 12) & 0x3f) | 0x80);
		buf[2] = (char) (((n >>  6) & 0x3f) | 0x80);
		buf[3] = (char) (((n >>  0) & 0x3f) | 0x80);
		len = 4;
	} else {
		yy_error_fmt("bad universal character 0x%x", n);
	}

	yy_dyn_str_append(rcc, dyn_str, buf, len);
}

static void yy_append_unicode_str(rcc_ctx *rcc, yy_dyn_str *dyn_str, char prefix, const char *str, size_t len)
{
	if (prefix == 'u' || (C_WCHAR_SIZE == 2 && prefix == 'L')) {
		uint16_t *dst = (uint16_t*)yy_dyn_str_grow(rcc, dyn_str, len * 2);
		unsigned char *p = (unsigned char*)str;

		dyn_str->len += len * 2;
		while (len > 0) {
			*dst = *p;
			dst++;
			p++;
			len--;
		}
	} else {
		uint32_t *dst = (uint32_t*)yy_dyn_str_grow(rcc, dyn_str, len * 4);
		unsigned char *p = (unsigned char*)str;

		dyn_str->len += len * 4;
		while (len > 0) {
			*dst = *p;
			dst++;
			p++;
			len--;
		}
	}
}

static void yy_append_unicode_char(rcc_ctx *rcc, yy_dyn_str *dyn_str, char prefix, uint32_t uc)
{
	if (prefix == 'u' || (C_WCHAR_SIZE == 2 && prefix == 'L')) {
		if (uc < 0x10000) {
			uint16_t *dst = (uint16_t*)yy_dyn_str_grow(rcc, dyn_str, 2);
			*dst = uc;
			dyn_str->len += 2;
		} else {
			uint16_t *dst = (uint16_t*)yy_dyn_str_grow(rcc, dyn_str, 4);
			uc -= 0x10000;
			dst[0] = (uc >> 10) + 0xd800;
			dst[1] = (uc & 0x3ff) + 0xdc00;
			dyn_str->len += 4;
		}
	} else {
		uint32_t *dst = (uint32_t*)yy_dyn_str_grow(rcc, dyn_str, 4);
		*dst = uc;
		dyn_str->len += 4;
	}
}

static char yy_strings_append(rcc_ctx *rcc, yy_dyn_str *dyn_str, char prefix, const char *str, size_t len)
{
	const char *s, *p = str;
	char ch;
	uint32_t uc;

	ch = *p++;
	if (ch == 'L' || ch == 'U') {
		if (dyn_str->len && prefix != ch) {
			yy_error("unsupported non-standard concatenation of string literals");
		}
		prefix = ch;
		ch = *p++;
	} else if (ch == 'u') {
		ch = *p++;
		if (ch == '8') {
			if (prefix) yy_error("unsupported non-standard concatenation of string literals");
			ch = *p++;
		} else {
			if (dyn_str->len && prefix != ch) {
				yy_error("unsupported non-standard concatenation of string literals");
			}
			prefix = 'u';
		}
	}

	IR_ASSERT(ch == '"');

	s = p;
	ch = *p++;
	if (!prefix) {
		while (ch != '"') {
			if (ch == '\\') {
				if (s != p - 1) yy_dyn_str_append(rcc, dyn_str, s, p - s - 1);
				ch = *p++;
				if (ch == '\n') {
					s = p;
					ch = *p++;
					continue;
				}
				uc = yy_read_escape_sequence(rcc, ch, &p);
				if (uc <= 0xff) {
					ch = uc;
					yy_dyn_str_append(rcc, dyn_str, &ch, 1);
				} else {
					yy_append_utf8(rcc, dyn_str, uc);
				}
				s = p;
			}
			ch = *p++;
		}

		if (s != p - 1) yy_dyn_str_append(rcc, dyn_str, s, p - s - 1);

	} else {

		while (ch != '"') {
			if ((unsigned char)ch > 0x7f) {
				if (s != p - 1) yy_append_unicode_str(rcc, dyn_str, prefix, s, p - s - 1);
				uc = yy_read_utf8_char(rcc, ch, &p);
				yy_append_unicode_char(rcc, dyn_str, prefix, uc);
				s = p;
			} else if (ch == '\\') {
				if (s != p - 1) yy_append_unicode_str(rcc, dyn_str, prefix, s, p - s - 1);
				ch = *p++;
				if (ch == '\n') {
					s = p;
					ch = *p++;
					continue;
				}
				ch = yy_read_escape_sequence(rcc, ch, &p);
				yy_append_unicode_char(rcc, dyn_str, prefix, (uint8_t)ch);
				s = p;
			}
			ch = *p++;
		}

		if (s != p - 1) yy_append_unicode_str(rcc, dyn_str, prefix, s, p - s - 1);
	}
	IR_ASSERT(p == str + len);
	return prefix;
}

static void yy_read_string(rcc_ctx *rcc, c_value *res, const char *str, size_t len)
{
	yy_dyn_str dyn_str;
	char prefix = 0;
	const c_type *type;

	yy_dyn_str_init(rcc, &dyn_str, "", 0);
	prefix = yy_strings_append(rcc, &dyn_str, prefix, str, len);

	if (!prefix) {
		yy_dyn_str_append(rcc, &dyn_str, "\0", 1);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string : &c_type_string;
	} else if (prefix == 'L') {
		if (C_WCHAR_SIZE == 2) {
			yy_dyn_str_append(rcc, &dyn_str, "\0\0", 2);
		} else {
			yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		}
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_lstring : &c_type_lstring;
	} else if (prefix == 'u') {
		yy_dyn_str_append(rcc, &dyn_str, "\0\0", 2);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string_u16 : &c_type_string_u16;
	} else {
		IR_ASSERT(prefix == 'U');
		yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string_u32 : &c_type_string_u32;
	}

	c_value_set_const_str(res, type, IR_ADDR, dyn_str.str, dyn_str.len);
}


static void yy_read_strings(rcc_ctx *rcc, c_value *res, yy_str *strings, uint32_t num_strings)
{
	yy_dyn_str dyn_str;
	char prefix = 0;
	const c_type *type;
	uint32_t i;

	yy_dyn_str_init(rcc, &dyn_str, "", 0);
	for (i = 0; i < num_strings; i++) {
		prefix = yy_strings_append(rcc, &dyn_str, prefix, strings[i].str, strings[i].len);
	}

	if (!prefix) {
		yy_dyn_str_append(rcc, &dyn_str, "\0", 1);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string : &c_type_string;
	} else if (prefix == 'L') {
		yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_lstring : &c_type_lstring;
	} else if (prefix == 'u') {
		yy_dyn_str_append(rcc, &dyn_str, "\0\0", 2);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string_u16 : &c_type_string_u16;
	} else {
		IR_ASSERT(prefix == 'U');
		yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		type = (rcc->e_warnings & E_WRITE_STRINGS) ? &c_type_const_string_u32 : &c_type_string_u32;
	}

	c_value_set_const_str(res, type, IR_ADDR, dyn_str.str, dyn_str.len);

	if (num_strings > C_ALLOCA_STRINGS) ir_mem_free(strings);
}

static yy_str *yy_grow_strings(rcc_ctx *rcc, yy_str *strings, uint32_t num_strings)
{
	yy_str *new_strings;

	if (num_strings == C_ALLOCA_STRINGS) {
		new_strings = ir_mem_malloc(C_ALLOCA_STRINGS * 2 * sizeof(yy_str));
		if (!new_strings) yy_error("out of memory");
		memcpy(new_strings, strings, C_ALLOCA_STRINGS * sizeof(yy_str));
		return new_strings;
	} else {
		IR_ASSERT(num_strings % C_ALLOCA_STRINGS == 0);
		if ((num_strings + C_ALLOCA_STRINGS) * sizeof(yy_str) <= 4096) {
			new_strings = ir_mem_realloc(strings, (num_strings + C_ALLOCA_STRINGS) * sizeof(yy_str));
			if (!new_strings) yy_error("out of memory");
			return new_strings;
		} else if ((num_strings * sizeof(yy_str)) % 4096 == 0) {
			new_strings = ir_mem_realloc(strings, (num_strings * sizeof(yy_str)) + 4096);
			if (!new_strings) yy_error("out of memory");
			return new_strings;
		} else {
			return strings;
		}
	}
}

static yy_sym parse_vla_param(yy_sym sym, rcc_ctx *rcc, c_value *val)
{
	yy_sym first = sym;
	const char *text = rcc->yy_text;
	size_t len = rcc->yy_len;
	pp_list tokens;
	int level = 0;
	uint32_t skip;

	pp_list_init(rcc, &tokens);
	pp_list_push(&tokens, sym);
	if (PP_HAS_VAL(sym)) {
		pp_list_push_val(rcc, &tokens);
	} else if (sym == YY__LBRACK) {
		level++;
	}

	skip = tokens.len;
	while (1) {
		sym = yy_next(rcc);
		if (sym == YY__RBRACK) {
			if (level == 0) {
				break;
			}
			pp_list_push(&tokens, sym);
			level--;
		} else if (sym == YY__LBRACK) {
			pp_list_push(&tokens, sym);
			level++;
		} else if (C_IS_ID(sym)) {
			pp_list_push(&tokens, sym | PP_NOSUBST);
		} else {
			pp_list_push(&tokens, sym);
			if (PP_HAS_VAL(sym)) {
				pp_list_push_val(rcc, &tokens);
			}
		}
	}

	pp_list_push(&tokens, YY__RBRACK);

	pp_subst_stream *stream = pp_push_stream(rcc);
	stream->macro = NULL;
	stream->start = NULL;
	stream->tokens = tokens.syms + skip;
	stream->skip_eof = 0;

	sym = first;
	rcc->yy_text = text;
	rcc->yy_len = len;

	sym = parse_assignment_expression(sym, rcc, val);

	IR_ASSERT(sym == YY__RBRACK);
	rcc->pp_stream = (rcc->pp_stream == rcc->pp_subst_stack) ? NULL : (rcc->pp_stream - 1);

	if (!c_value_is_const(val)) {
		val->u.val.ptr = ir_arena_alloc(&rcc->c_arena, sizeof(yy_sym) * tokens.len);
		memcpy(val->u.val.ptr, tokens.syms, sizeof(yy_sym) * tokens.len);
	}
	pp_list_release(rcc, tokens.syms, tokens.size);

	return YY__RBRACK;
}

void parse_vla_param_again(rcc_ctx *rcc, yy_sym *vla_tokens, c_value *val)
{
	yy_sym sym = *vla_tokens++;

	if (PP_HAS_VAL(sym)) {
		vla_tokens = pp_load_val(rcc, vla_tokens);
	}

	pp_subst_stream *stream = pp_push_stream(rcc);
	stream->macro = NULL;
	stream->start = NULL;
	stream->tokens = vla_tokens;
	stream->skip_eof = 0;

	sym = parse_assignment_expression(sym, rcc, val);

	IR_ASSERT(sym == YY__RBRACK);
	rcc->pp_stream = (rcc->pp_stream == rcc->pp_subst_stack) ? NULL : (rcc->pp_stream - 1);
}

/* CPP helper */
bool parse_pp_expr(rcc_ctx *rcc)
{
	bool ret;
	c_value res;
	bool old_dead_code = rcc->c_dead_code;
	ir_ctx *old_ctx = rcc->active_ctx;
	uint32_t old_flags = rcc->yy_flags;
	yy_sym sym;

	rcc->active_ctx = rcc->global_ctx;
	rcc->yy_flags |= PP_EVAL_EXPRESSION;
	rcc->yy_flags &= ~YY_ACCEPT_PP_NUMBER;
	rcc->c_dead_code = 0;

	do {
		sym = get_sym();
		sym = parse_constant_expression(sym, rcc, &res);
	} while (sym == YY__COMMA);

	if (sym != YY_EOF) {
		if (sym >= YY_DECIMAL_NUMBER && sym <= YY_PP_NUMBER) {
			yy_error_fmt("missing operator in preprocessor expressions", yy_sym2str(rcc, sym));
		} else {
			yy_error_fmt("token \"%s\" is not valid in preprocessor expressions", yy_sym2str(rcc, sym));
		}
	}

	ret = c_value_is_true(&res);

	rcc->c_dead_code = old_dead_code;
	rcc->yy_flags = old_flags;
	rcc->active_ctx = old_ctx;

	return ret;
}

const char* parse_pp_string(rcc_ctx *rcc, size_t *len)
{
	yy_dyn_str dyn_str;
	char prefix = 0;

	yy_dyn_str_init(rcc, &dyn_str, "", 0);
	prefix = yy_strings_append(rcc, &dyn_str, prefix, rcc->yy_text, rcc->yy_len);

	if (!prefix) {
		yy_dyn_str_append(rcc, &dyn_str, "\0", 1);
	} else if (prefix == 'L') {
		yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		// TODO: convert to UTF-8 ???
		IR_ASSERT(0);
	} else if (prefix == 'u') {
		yy_dyn_str_append(rcc, &dyn_str, "\0\0", 2);
		// TODO: convert to UTF-8 ???
		IR_ASSERT(0);
	} else {
		IR_ASSERT(prefix == 'U');
		yy_dyn_str_append(rcc, &dyn_str, "\0\0\0\0", 4);
		// TODO: convert to UTF-8 ???
		IR_ASSERT(0);
	}

	*len = dyn_str.len;
	return dyn_str.str;
}

void rcc_parse(rcc_ctx *rcc)
{
	/* parse starting from yy_pos */
	yy_sym sym = parse_translation_unit(get_sym(), rcc);

	if (sym != YY_EOF) {
		yy_error_sym("<EOF> expected, got", sym);
	}

	if (rcc->pp_ifdef_level) yy_error("mising #endif");
}
