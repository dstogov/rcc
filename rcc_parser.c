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

#define get_sym              yy_next
#define C_IS_ID(sym)         ((sym) > YY_LAST_KEYWORD)

static IR_NEVER_INLINE void yy_error_sym(const char *msg, int sym)
{
	yy_error_fmt("%s \"%s\"", msg, yy_sym2str(sym));
}

/* Parser Predicates */
static bool is_typedef_name(yy_sym id)
{
	if (yy_hash.data[id].sym && yy_hash.data[id].sym->kind == C_SYM_TYPE) {
		return 1;
	}
	return 0;
}

static bool is_typedef_name2(yy_sym id, c_dcl *dcl)
{
	if (dcl->flags & C_TYPE_SPEC_ANY) {
		return 0;
	}
	if (yy_hash.data[id].sym && yy_hash.data[id].sym->kind == C_SYM_TYPE) {
		return 1;
	}
	return 0;
}

static bool is_label(yy_sym id)
{
	IR_ASSERT(C_IS_ID(id));
	if (pp_subst_level > 0) {
		uint32_t level = pp_subst_level;
		do {
			pp_subst_stream *stream = &pp_subst_stack[level - 1];
			yy_sym *tokens = stream->tokens;

			while (*tokens == YY_WS) tokens++;
			if (*tokens == YY__COLON) return 1;
			if (*tokens != YY_EOF) return 0;
			level--;
		} while (level > 0);
	}

	if (*yy_pos == ':') {
		return 1;
	}

	const char *save_pos = yy_pos;
	const char *save_text = yy_text;
	const char *save_linepos = yy_linepos;
	int save_line = yy_line;
	uint32_t save_level = pp_subst_level;

	pp_subst_level = 0;
	bool ret = get_sym() == YY__COLON;

	while (pp_subst_level > save_level) {
		pp_subst_level--;
		pp_subst_stream *stream = &pp_subst_stack[pp_subst_level];
		if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
		if (stream->start) pp_list_release(stream->start, stream->size);
	}
	pp_subst_level = save_level;
	yy_pos  = save_pos;
	yy_text = save_text;
	yy_linepos = save_linepos;
	yy_line = save_line;

	return ret;
}

static bool is_nested_declarator(yy_sym id)
{
	IR_ASSERT(id == YY__LPAREN);
	if (pp_subst_level > 0) {
		uint32_t level = pp_subst_level;
		do {
			pp_subst_stream *stream = &pp_subst_stack[level - 1];
			yy_sym *tokens = stream->tokens;

			while (*tokens == YY_WS) tokens++;
			if (*tokens == YY___ATTRIBUTE
			 || *tokens == YY___ATTRIBUTE__
			 || *tokens == YY__STAR
			 || *tokens == YY__LPAREN
			 || *tokens == YY__LBRACK
			 || (C_IS_ID(*tokens) && !is_typedef_name(*tokens))) return 1;
			if (*tokens != YY_EOF) return 0;
			level--;
		} while (level > 0);
	}

	if (*yy_pos == '*' || *yy_pos == '(' || *yy_pos == '[') {
		return 1;
	}

	const char *save_pos = yy_pos;
	const char *save_text = yy_text;
	const char *save_linepos = yy_linepos;
	int save_line = yy_line;
	uint32_t save_level = pp_subst_level;

	pp_subst_level = 0;
	yy_sym sym = get_sym();
	bool ret = (sym == YY___ATTRIBUTE
			|| sym == YY___ATTRIBUTE__
			|| sym == YY__STAR
			|| sym == YY__LPAREN
			|| sym == YY__LBRACK
			|| (C_IS_ID(sym) && !is_typedef_name(sym)));

	while (pp_subst_level > save_level) {
		pp_subst_level--;
		pp_subst_stream *stream = &pp_subst_stack[pp_subst_level];
		if (stream->macro) stream->macro->flags &= ~PP_MACRO_DISABLED;
		if (stream->start) pp_list_release(stream->start, stream->size);
	}
	pp_subst_level = save_level;
	yy_pos  = save_pos;
	yy_text = save_text;
	yy_linepos = save_linepos;
	yy_line = save_line;

	return ret;
}

typedef struct _yy_str_list str_list;

struct _yy_str_list {
	const char *str;
	size_t      len;
	str_list   *next;
};

/* Scanner actions */
static void yy_strings(c_value *res, str_list *first, str_list *last);
static void yy_read_oct(c_value *res, const char *p, size_t len);
static void yy_read_dec(c_value *res, const char *p, size_t len);
static void yy_read_hex(c_value *res, const char *p, size_t len);
static void yy_read_bin(c_value *res, const char *p, size_t len);
static void yy_read_fp(c_value *res, const char *p, size_t len);
static void yy_read_char(c_value *res, const char *p, size_t len);

#define YY_IN_SET(sym, set, bitset) \
	(bitset[sym>>3] & (1 << (sym & 0x7)))

static yy_sym parse_translation_unit(yy_sym sym);
static yy_sym parse_simple_asm_expr(yy_sym sym);
static yy_sym parse_declaration(yy_sym sym, uint32_t flags);
static yy_sym parse_old_style_param_declaration(yy_sym sym, const c_type *t);
static yy_sym parse_declaration_specifiers(yy_sym sym, c_dcl *d);
static yy_sym parse_specifier_qualifier_list(yy_sym sym, c_dcl *d);
static yy_sym parse_type_qualifier_list(yy_sym sym, c_dcl *d);
static yy_sym parse_storage_class_specifier(yy_sym sym, c_dcl *d);
static yy_sym parse_type_specifier_or_qualifier(yy_sym sym, c_dcl *d);
static yy_sym parse_type_qualifier(yy_sym sym, c_dcl *d);
static yy_sym parse_function_specifier(yy_sym sym, c_dcl *d);
static yy_sym parse_alignment_specifier(yy_sym sym, c_dcl *d);
static yy_sym parse_attributes(yy_sym sym, c_dcl *d);
static yy_sym parse_attrib(yy_sym sym, c_dcl *d);
static yy_sym parse_struct_or_union_specifier(yy_sym sym, c_dcl *d);
static yy_sym parse_struct_contents(yy_sym sym, c_type *t, c_dcl *d);
static yy_sym parse_struct_declaration(yy_sym sym, c_type *t);
static yy_sym parse_struct_declarator(yy_sym sym, c_type *t, c_dcl *field);
static yy_sym parse_enum_specifier(yy_sym sym, c_dcl *d);
static yy_sym parse_enum_contents(yy_sym sym, c_type *t, c_dcl *d);
static yy_sym parse_enumerator(yy_sym sym, const c_type *t, int64_t *min, uint64_t *max, c_value *last);
static yy_sym parse_declarator(yy_sym sym, c_dcl *d, c_name *name, bool allow_old_func);
static yy_sym parse_abstract_declarator(yy_sym sym, c_dcl *d);
static yy_sym parse_parameter_declarator(yy_sym sym, c_dcl *d, c_name *name);
static yy_sym parse_arrays_and_params(yy_sym sym, c_dcl *d, bool allow_old_func, bool is_param);
static yy_sym parse_array_declarator(yy_sym sym, c_dcl *d, bool is_param);
static yy_sym parse_parameters(yy_sym sym, c_dcl *d, bool allow_old_func);
static yy_sym parse_parameter_declaration(yy_sym sym, c_param **params, int32_t *num_params);
static yy_sym parse_identifier_list(yy_sym sym, c_param **params, int32_t *num_params);
static yy_sym parse_type_name(yy_sym sym, const c_type **t);
static yy_sym parse_initializer(yy_sym sym, c_sym *obj);
static yy_sym parse_nested_initializer(yy_sym sym, c_sym *obj, c_init *init, bool b, size_t *size);
static yy_sym parse_initializer_contents(yy_sym sym, c_sym *obj, const c_type *t, size_t offset, size_t *size);
static yy_sym parse_designated_initializer(yy_sym sym, c_sym *obj, c_init *init, size_t *size);
static yy_sym parse_static_assert_declaration(yy_sym sym);
static yy_sym parse_compound_statement(yy_sym sym, c_value *val);
static yy_sym parse_statement(yy_sym sym, c_value *last_val);
static yy_sym parse_labels(yy_sym sym);
static yy_sym parse_statement2(yy_sym sym, c_value *last_val);
static yy_sym parse_asm_argument(yy_sym sym);
static yy_sym parse_asm_operands(yy_sym sym);
static yy_sym parse_asm_operand(yy_sym sym);
static yy_sym parse_asm_clobbers(yy_sym sym);
static yy_sym parse_asm_goto_operands(yy_sym sym);
static yy_sym parse_strings(yy_sym sym, c_value *val);
static yy_sym parse_strings_tail(yy_sym sym, c_value *val, str_list *first, str_list *last);
static yy_sym parse_actual_parameters(yy_sym sym, c_value *func);
static yy_sym parse_builtin_parameters(yy_sym sym, c_value *val, c_name name);
static yy_sym parse_dummy_value(yy_sym sym, const c_type *t);
static yy_sym parse_unary_expression(yy_sym sym, c_value *val);
static yy_sym parse_infix_expression(yy_sym sym, c_value *val, yy_sym prev);
static yy_sym parse_conditional_expression(yy_sym sym, c_value *val);
static yy_sym parse_assignment_expression(yy_sym sym, c_value *val);
static yy_sym parse_expression(yy_sym sym, c_value *val);
static yy_sym parse_constant_expression(yy_sym sym, c_value *val);
static yy_sym parse_ID(yy_sym sym, c_name *name);
static yy_sym parse_DECIMAL_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_OCTAL_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_HEXADECIMAL_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_BINARY_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_FLOATING_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_HEXADECIMAL_FLOATING_NUMBER(yy_sym sym, c_value *val);
static yy_sym parse_CHARACTER(yy_sym sym, c_value *val);
static yy_sym parse_STRING(yy_sym sym);
static int synpred_1(yy_sym sym);
static int synpred__lparen(yy_sym sym);
static int synpred__rbrace(yy_sym sym);
static int synpred__star(yy_sym sym);

static int synpred_1(yy_sym sym) {
	return sym == YY___ATTRIBUTE__ || sym == YY___ATTRIBUTE || sym == YY___ASM__ || sym == YY___ASM || sym == YY_ASM || sym == YY__EQUAL || sym == YY__COMMA || sym == YY__SEMICOLON;
}

static int synpred__lparen(yy_sym sym) {
	return sym == YY__LPAREN;
}

static int synpred__rbrace(yy_sym sym) {
	return sym == YY__RBRACE;
}

static int synpred__star(yy_sym sym) {
	return sym == YY__STAR;
}

static yy_sym parse_translation_unit(yy_sym sym) {
	while (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || sym == YY___EXTENSION__ || sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
		if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			sym = parse_simple_asm_expr(sym);
			if (sym != YY__SEMICOLON) {
				yy_error_sym("';' expected, got", sym);
			}
			sym = get_sym();
		} else {
			if (sym == YY___EXTENSION__) {
				sym = get_sym();
			}
			sym = parse_declaration(sym, 0);
		}
	}
	return sym;
}

static yy_sym parse_simple_asm_expr(yy_sym sym) {
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
	do {
		sym = parse_STRING(sym);
	} while (sym == YY_STRING);
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	/*???*/yy_error("asm support not implemented yet");
	return sym;
}

static yy_sym parse_declaration(yy_sym sym, uint32_t flags) {
	c_dcl d0 = {0};
	c_name name;
	c_sym *obj;
	if (sym == YY__STATIC_ASSERT) {
		sym = parse_static_assert_declaration(sym);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
		if ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
			sym = parse_declaration_specifiers(sym, &d0);
		}
		d0.flags |= flags;
		if (sym == YY__STAR || C_IS_ID(sym) || sym == YY__LPAREN) {
			c_dcl d = d0;
			sym = parse_declarator(sym, &d, &name, 1);
			if ((sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__EQUAL || sym == YY__COMMA || sym == YY__SEMICOLON) && synpred_1(sym)) {
				if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
					sym = parse_simple_asm_expr(sym);
				}
				if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
					sym = parse_attributes(sym, &d);
				}
				if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;
				obj = c_declare(name, &d);
				if (sym == YY__EQUAL) {
					sym = get_sym();
					sym = parse_initializer(sym, obj);
				}
				while (sym == YY__COMMA) {
					sym = get_sym();
					d = d0;
					if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
						sym = parse_attributes(sym, &d);
					}
					sym = parse_declarator(sym, &d, &name, 0);
					if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
						sym = parse_simple_asm_expr(sym);
					}
					if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
						sym = parse_attributes(sym, &d);
					}
					if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;
					obj = c_declare(name, &d);
					if (sym == YY__EQUAL) {
						sym = get_sym();
						sym = parse_initializer(sym, obj);
					}
				}
				if (sym != YY__SEMICOLON) {
					yy_error_sym("';' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__LBRACE) {
				ir_ctx ctx, *old_ctx = active_ctx;
				c_scope scope;
				if (!d.type || d.type->kind != C_TYPE_FUNC) yy_error_sym("unexpected", sym);
				if ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (d.type->attr & C_ATTR_OLD_FUNC)) {
					do {
						sym = parse_old_style_param_declaration(sym, d.type);
					} while (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__);
					c_validate_func_params(name, &d);
				}
				c_do_func_start(name, &d, &scope, &ctx);
				if (sym != YY__LBRACE) {
					yy_error_sym("'{' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_compound_statement(sym, NULL);
				c_do_func_end(name, &d, &scope, &ctx);
				if (sym != YY__RBRACE) {
					yy_error_sym("'}' expected, got", sym);
				}
				sym = get_sym();
				active_ctx = old_ctx;
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__SEMICOLON) {
			sym = get_sym();
			c_empty_declaration(&d0);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_old_style_param_declaration(yy_sym sym, const c_type *t) {
	c_dcl d0 = {0};
	c_name name;
	sym = parse_declaration_specifiers(sym, &d0);
	c_dcl d = d0;
	sym = parse_declarator(sym, &d, &name, 0);
	if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
		sym = parse_simple_asm_expr(sym);
	}
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, &d);
	}
	c_declare_func_param_type(t, name, &d);
	if (sym == YY__EQUAL) {
		sym = get_sym();
		yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(name));
		sym = parse_initializer(sym, NULL);
	}
	while (sym == YY__COMMA) {
		sym = get_sym();
		d = d0;
		sym = parse_declarator(sym, &d, &name, 0);
		if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
			sym = parse_simple_asm_expr(sym);
		}
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, &d);
		}
		c_declare_func_param_type(t, name, &d);
		if (sym == YY__EQUAL) {
			sym = get_sym();
			yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(name));
			sym = parse_initializer(sym, NULL);
		}
	}
	if (sym != YY__SEMICOLON) {
		yy_error_sym("';' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_declaration_specifiers(yy_sym sym, c_dcl *d) {
	do {
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL) {
			sym = parse_storage_class_specifier(sym, d);
		} else if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__) {
			sym = parse_type_specifier_or_qualifier(sym, d);
		} else if (sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN) {
			sym = parse_function_specifier(sym, d);
		} else if (sym == YY__ALIGNAS) {
			sym = parse_alignment_specifier(sym, d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name2(sym, d)));
	return sym;
}

static yy_sym parse_specifier_qualifier_list(yy_sym sym, c_dcl *d) {
	do {
		if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__) {
			sym = parse_type_specifier_or_qualifier(sym, d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name2(sym, d)));
	return sym;
}

static yy_sym parse_type_qualifier_list(yy_sym sym, c_dcl *d) {
	do {
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC) {
			sym = parse_type_qualifier(sym, d);
		} else if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__);
	return sym;
}

static yy_sym parse_storage_class_specifier(yy_sym sym, c_dcl *d) {
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

static yy_sym parse_type_specifier_or_qualifier(yy_sym sym, c_dcl *d) {
	c_name name;
	if (sym == YY_VOID) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_VOID;
	} else if (sym == YY_CHAR) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_CHAR;
	} else if (sym == YY_SHORT) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_SHORT;
	} else if (sym == YY_INT) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_INT;
	} else if (sym == YY_LONG) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_LONG|C_TYPE_SPEC_INT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_COMPLEX))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= (d->flags & C_TYPE_SPEC_LONG) ? C_TYPE_SPEC_LONG_LONG : C_TYPE_SPEC_LONG;
	} else if (sym == YY_FLOAT) {
		if (d->flags & (C_TYPE_SPEC_ANY-C_TYPE_SPEC_COMPLEX)) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_FLOAT;
	} else if (sym == YY_DOUBLE) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_LONG|C_TYPE_SPEC_COMPLEX))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_DOUBLE;
	} else if (sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_SIGNED;
	} else if (sym == YY_UNSIGNED) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_UNSIGNED;
	} else if (sym == YY__BOOL) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_BOOL;
	} else if (sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__) {
		if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_FLOAT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_LONG))) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_COMPLEX;
	} else if (sym == YY__ATOMIC) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
			d->flags |= C_TYPE_SPEC_ATOMIC;
			sym = parse_type_name(sym, &d->type);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__COLON) {
			d->attr |= C_ATTR_ATOMIC;
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = get_sym();
		d->flags |= C_TYPE_SPEC_TYPE;
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
			sym = parse_type_name(sym, &d->type);
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			c_value v = {0};
			ir_ref old = c_do_nocode();
			sym = parse_expression(sym, &v);
			d->type = c_typeof_expr(&v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_STRUCT || sym == YY_UNION) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = parse_struct_or_union_specifier(sym, d);
	} else if (sym == YY_ENUM) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = parse_enum_specifier(sym, d);
	} else if (C_IS_ID(sym)) {
		if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);
		sym = parse_ID(sym, &name);
		d->flags |= C_TYPE_SPEC_NAME;
		d->type = c_resolve_type_name(name);
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
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_type_qualifier(yy_sym sym, c_dcl *d) {
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

static yy_sym parse_function_specifier(yy_sym sym, c_dcl *d) {
	if (sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__) {
		sym = get_sym();
		d->attr |= C_ATTR_INLINE;
	} else if (sym == YY__NORETURN) {
		sym = get_sym();
		d->attr |= C_ATTR_NORETURN;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_alignment_specifier(yy_sym sym, c_dcl *d) {
	c_value v = {0};
	if (sym != YY__ALIGNAS) {
		yy_error_sym("'_Alignas' expected, got", sym);
	}
	sym = get_sym();
	if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
		const c_type *t;
		sym = parse_type_name(sym, &t);
		d->attr |= t->attr & C_ATTR_ALIGN_MASK;
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		sym = parse_constant_expression(sym, &v);
		c_alignas_expr(d, &v);
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static yy_sym parse_attributes(yy_sym sym, c_dcl *d) {
	do {
		if (sym == YY___ATTRIBUTE) {
			sym = get_sym();
		} else if (sym == YY___ATTRIBUTE__) {
			sym = get_sym();
		} else {
			yy_error_sym("unexpected", sym);
		}
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_attrib(sym, d);
		while (sym == YY__COMMA) {
			sym = get_sym();
			sym = parse_attrib(sym, d);
		}
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} while (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__);
	return sym;
}

static yy_sym parse_attrib(yy_sym sym, c_dcl *d) {
	if (C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST__) {
		c_name name = sym;
		c_value v = {0};
		if (C_IS_ID(sym)) {
			sym = parse_ID(sym, &name);
		} else if (sym == YY_CONST) {
			sym = get_sym();
		} else {
			sym = get_sym();
		}
		if (sym == YY__LPAREN) {
			sym = get_sym();
			sym = parse_expression(sym, &v);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		}
		c_gcc_attribute(d, name, &v);
	}
	return sym;
}

static yy_sym parse_struct_or_union_specifier(yy_sym sym, c_dcl *d) {
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
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, d);
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, &name);
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__COLON) {
			c_resolve_tag(name, d, 0);
		} else if (sym == YY__LBRACE) {
			c_type *t = c_resolve_tag(name, d, 1);
			sym = parse_struct_contents(sym, t, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__LBRACE) {
		c_type *t = c_make_struct_type(d, 0);
		sym = parse_struct_contents(sym, t, d);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_struct_contents(yy_sym sym, c_type *t, c_dcl *d) {
	t->record.fields = alloca(sizeof(c_field) * C_ALLOCA_FIELDS);
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	t->flags |= C_TYPE_INPROGRESS;
	if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STATIC_ASSERT) {
		sym = parse_struct_declaration(sym, t);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		do {
			sym = get_sym();
			if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
				break; /* manual conflict resolution */
				sym = get_sym();
			} else if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STATIC_ASSERT) {
				sym = parse_struct_declaration(sym, t);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} while (sym == YY__SEMICOLON);
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
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, d);
	}
	c_finish_struct_type(t, d);
	return sym;
}

static yy_sym parse_struct_declaration(yy_sym sym, c_type *t) {
	if (sym == YY___EXTENSION__ || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		c_dcl field0 = {0};
		if (sym == YY___EXTENSION__) {
			sym = get_sym();
		}
		sym = parse_specifier_qualifier_list(sym, &field0);
		c_dcl field = field0;
		sym = parse_struct_declarator(sym, t, &field);
		while (sym == YY__COMMA) {
			sym = get_sym();
			field = field0;
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				sym = parse_attributes(sym, &field);
			}
			sym = parse_struct_declarator(sym, t, &field);
		}
	} else if (sym == YY__STATIC_ASSERT) {
		sym = parse_static_assert_declaration(sym);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_struct_declarator(yy_sym sym, c_type *t, c_dcl *field) {
	c_value v = {0};
	c_name name;
	if (sym == YY__STAR || C_IS_ID(sym) || sym == YY__LPAREN) {
		sym = parse_declarator(sym, field, &name, 0);
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, field);
		}
		if (sym == YY__COLON) {
			sym = get_sym();
			sym = parse_constant_expression(sym, &v);
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				sym = parse_attributes(sym, field);
			}
		}
		c_declare_struct_field(t, name, field, &v);
	} else if (sym == YY__COLON || sym == YY__COMMA || sym == YY__SEMICOLON) {
		if (sym == YY__COLON) {
			sym = get_sym();
			sym = parse_constant_expression(sym, &v);
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				sym = parse_attributes(sym, field);
			}
		}
		c_declare_struct_field(t, 0, field, &v);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_enum_specifier(yy_sym sym, c_dcl *d) {
	c_name name;
	if (sym != YY_ENUM) {
		yy_error_sym("'enum' expected, got", sym);
	}
	sym = get_sym();
	d->flags |= C_TYPE_SPEC_ENUM;
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, d);
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, &name);
		if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON || sym == YY__LBRACK || sym == YY__COMMA || sym == YY__RPAREN || sym == YY__COLON) {
			c_resolve_tag(name, d, 0);
		} else if (sym == YY__LBRACE) {
			c_type *t = c_resolve_tag(name, d, 1);
			sym = parse_enum_contents(sym, t, d);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__LBRACE) {
		c_type *t = c_make_enum_type(d, 0);
		sym = parse_enum_contents(sym, t, d);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_enum_contents(yy_sym sym, c_type *t, c_dcl *d) {
	int64_t min = 0;
	uint64_t max = 0;
	c_value last;
	last.u.type = IR_I64; last.u.val.i64 = -1;
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_enumerator(sym, t, &min, &max, &last);
	while (sym == YY__COMMA) {
		sym = get_sym();
		if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
			break; /* manual conflict resolution */
			sym = get_sym();
		} else if (C_IS_ID(sym)) {
			sym = parse_enumerator(sym, t, &min, &max, &last);
		} else {
			yy_error_sym("unexpected", sym);
		}
	}
	if (sym != YY__RBRACE) {
		yy_error_sym("'}' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, d);
	}
	c_finish_enum_type(t, d, min, max);
	return sym;
}

static yy_sym parse_enumerator(yy_sym sym, const c_type *t, int64_t *min, uint64_t *max, c_value *last) {
	c_value v = {0};
	c_dcl attr = {0};
	c_name name;
	sym = parse_ID(sym, &name);
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, &attr);
	}
	if (sym == YY__EQUAL) {
		sym = get_sym();
		sym = parse_constant_expression(sym, &v);
	}
	c_declare_enum_val(t, name, &attr, &v, min, max, last);
	return sym;
}

static yy_sym parse_declarator(yy_sym sym, c_dcl *d, c_name *name, bool allow_old_func) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_type_qualifier_list(sym, d);
		}
	}
	if (C_IS_ID(sym)) {
		sym = parse_ID(sym, name);
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, allow_old_func, 0);
		}
	} else if (sym == YY__LPAREN) {
		sym = get_sym();
		d2.flags = C_TYPE_SPEC_CHAR;
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, &d2);
		}
		sym = parse_declarator(sym, &d2, name, 0);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, allow_old_func, 0);
		}
		c_make_nested_type(d, &d2);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_abstract_declarator(yy_sym sym, c_dcl *d) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_type_qualifier_list(sym, d);
		}
	}
	if ((sym == YY__LPAREN) && (is_nested_declarator(sym))) {
		d2.flags = C_TYPE_SPEC_CHAR;
		sym = get_sym();
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, &d2);
		}
		sym = parse_abstract_declarator(sym, &d2);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, 0, 0);
		}
		c_make_nested_type(d, &d2);
	} else if (sym == YY__LPAREN || sym == YY__LBRACK || sym == YY__RPAREN || sym == YY__COLON) {
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, 0, 0);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_parameter_declarator(yy_sym sym, c_dcl *d, c_name *name) {
	c_dcl d2 = {0};
	while (sym == YY__STAR) {
		sym = get_sym();
		c_make_pointer_type(d);
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_type_qualifier_list(sym, d);
		}
	}
	if ((sym == YY__LPAREN) && (is_nested_declarator(sym))) {
		d2.flags = C_TYPE_SPEC_CHAR;
		sym = get_sym();
		if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_attributes(sym, &d2);
		}
		sym = parse_parameter_declarator(sym, &d2, name);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, 0, 1);
		}
		c_make_nested_type(d, &d2);
	} else if (C_IS_ID(sym)) {
		sym = parse_ID(sym, name);
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, 0, 1);
		}
	} else if (sym == YY__LPAREN || sym == YY__LBRACK || sym == YY__RPAREN || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__COMMA) {
		if (sym == YY__LPAREN || sym == YY__LBRACK) {
			sym = parse_arrays_and_params(sym, d, 0, 1);
		}
		*name = 0;
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_arrays_and_params(yy_sym sym, c_dcl *d, bool allow_old_func, bool is_param) {
	if (sym == YY__LPAREN) {
		sym = parse_parameters(sym, d, allow_old_func);
	} else if (sym == YY__LBRACK) {
		sym = parse_array_declarator(sym, d, is_param);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_array_declarator(yy_sym sym, c_dcl *d, bool is_param) {
	c_value len = {0};
	c_dcl dim = {0};
	uint64_t attr = 0;
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
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		sym = parse_assignment_expression(sym, &len);
	} else if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_type_qualifier_list(sym, &dim);
		if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");
		if ((sym == YY__STAR) && synpred__star(sym)) {
			sym = get_sym();
			if (!is_param) yy_error("[*] not allowed in other than function prototype scope");
			attr |= C_ATTR_VLA;
		} else if (sym == YY__RBRACK) {
			attr |= C_ATTR_FLEXIBLE;
		} else if (sym == YY_STATIC || sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			if (sym == YY_STATIC) {
				sym = get_sym();
			}
			sym = parse_assignment_expression(sym, &len);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY_STATIC) {
		sym = get_sym();
		if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");
		if (sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY__ATOMIC || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
			sym = parse_type_qualifier_list(sym, &dim);
		}
		sym = parse_assignment_expression(sym, &len);
	} else {
		yy_error_sym("unexpected", sym);
	}
	if (sym != YY__RBRACK) {
		yy_error_sym("']' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY__LPAREN || sym == YY__LBRACK) {
		sym = parse_arrays_and_params(sym, d, 0, is_param);
	}
	c_make_array_type(d, &dim, &len, attr, is_param);
	return sym;
}

static yy_sym parse_parameters(yy_sym sym, c_dcl *d, bool allow_old_func) {
	uint32_t attr = 0;
	int32_t num_params = 0;
	c_param *params = alloca(sizeof(c_param) * C_ALLOCA_PARAMS);
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	if ((C_IS_ID(sym)) && (allow_old_func && !is_typedef_name(sym))) {
		sym = parse_identifier_list(sym, &params, &num_params);
		attr |= C_ATTR_OLD_FUNC;
	} else if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_parameter_declaration(sym, &params, &num_params);
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				sym = parse_parameter_declaration(sym, &params, &num_params);
			} else if (sym == YY__POINT_POINT_POINT) {
				sym = get_sym();
				attr |= C_ATTR_VARIADIC;
				break; /* manual conflict resolution */
			} else {
				yy_error_sym("unexpected", sym);
			}
		}
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
		sym = parse_arrays_and_params(sym, d, 0, 0);
	}
	c_make_func_type(d, params, num_params, attr);
	return sym;
}

static yy_sym parse_parameter_declaration(yy_sym sym, c_param **params, int32_t *num_params) {
	c_dcl p = {0};
	c_name name;
	sym = parse_declaration_specifiers(sym, &p);
	sym = parse_parameter_declarator(sym, &p, &name);
	if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		sym = parse_attributes(sym, &p);
	}
	c_declare_func_param(params, num_params, name, &p);
	return sym;
}

static yy_sym parse_identifier_list(yy_sym sym, c_param **params, int32_t *num_params) {
	c_name name;
	sym = parse_ID(sym, &name);
	c_declare_func_param_name(params, num_params, name);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_ID(sym, &name);
		c_declare_func_param_name(params, num_params, name);
	}
	return sym;
}

static yy_sym parse_type_name(yy_sym sym, const c_type **t) {
	c_dcl d = {0};
	sym = parse_specifier_qualifier_list(sym, &d);
	sym = parse_abstract_declarator(sym, &d);
	*t = c_resolve_type(&d);
	return sym;
}

static yy_sym parse_initializer(yy_sym sym, c_sym *obj) {
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		c_value v = {0};
		sym = parse_assignment_expression(sym, &v);
		c_do_init_obj(obj, &v);
	} else if (sym == YY__LBRACE) {
		size_t size = obj->value.type->size;
		sym = parse_initializer_contents(sym, obj, obj->value.type, 0, &size);
		c_do_init_end(obj, size);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_nested_initializer(yy_sym sym, c_sym *obj, c_init *init, bool b, size_t *size) {
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		c_value v = {0};
		sym = parse_assignment_expression(sym, &v);
		c_do_init_set(obj, init, &v, size);
	} else if (sym == YY__LBRACE) {
		size_t offset;
		const c_type *type = c_do_init_nested(obj, init, b, &offset);
		sym = parse_initializer_contents(sym, obj, type, offset, size);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_initializer_contents(yy_sym sym, c_sym *obj, const c_type *t, size_t offset, size_t *size) {
	if (sym != YY__LBRACE) {
		yy_error_sym("'{' expected, got", sym);
	}
	sym = get_sym();
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__LBRACE || sym == YY__LBRACK || sym == YY__POINT) {
		c_init init;
		c_do_init_first(obj, &init, t, offset);
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__LBRACE) {
			sym = parse_nested_initializer(sym, obj, &init, 0, size);
		} else {
			sym = parse_designated_initializer(sym, obj, &init, size);
		}
		while (sym == YY__COMMA) {
			sym = get_sym();
			if ((sym == YY__RBRACE) && synpred__rbrace(sym)) {
				break; /* manual conflict resolution */
				sym = get_sym();
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__LBRACE) {
				c_do_init_next(obj, &init);
				sym = parse_nested_initializer(sym, obj, &init, 0, size);
			} else if (sym == YY__LBRACK || sym == YY__POINT) {
				c_do_init_first(obj, &init, t, offset);
				sym = parse_designated_initializer(sym, obj, &init, size);
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

static yy_sym parse_designated_initializer(yy_sym sym, c_sym *obj, c_init *init, size_t *size) {
	do {
		if (sym == YY__LBRACK) {
			c_value v;
			sym = get_sym();
			sym = parse_constant_expression(sym, &v);
			if (sym != YY__RBRACK) {
				yy_error_sym("']' expected, got", sym);
			}
			sym = get_sym();
			c_do_init_dim(obj, init, &v);
		} else if (sym == YY__POINT) {
			c_name name;
			sym = get_sym();
			sym = parse_ID(sym, &name);
			c_do_init_field(obj, init, name);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while (sym == YY__LBRACK || sym == YY__POINT);
	if (sym != YY__EQUAL) {
		yy_error_sym("'=' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_nested_initializer(sym, obj, init, 1, size);
	return sym;
}

static yy_sym parse_static_assert_declaration(yy_sym sym) {
	c_value cond = {0}, msg = {0};
	if (sym != YY__STATIC_ASSERT) {
		yy_error_sym("'_Static_assert' expected, got", sym);
	}
	sym = get_sym();
	if (sym != YY__LPAREN) {
		yy_error_sym("'(' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_constant_expression(sym, &cond);
	if (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_strings(sym, &msg);
	}
	if (sym != YY__RPAREN) {
		yy_error_sym("')' expected, got", sym);
	}
	sym = get_sym();
	c_static_assert(&cond, &msg);
	return sym;
}

static yy_sym parse_compound_statement(yy_sym sym, c_value *val) {
	while (sym == YY___LABEL__) {
		c_name name;
		sym = get_sym();
		sym = parse_ID(sym, &name);
		c_declare_local_label(name);
		while (sym == YY__COMMA) {
			sym = get_sym();
			sym = parse_ID(sym, &name);
			c_declare_local_label(name);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	}
	while (C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT || sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY__LPAREN || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__SEMICOLON || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__ || sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
		if ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(sym))) {
			sym = parse_labels(sym);
		} else if ((sym == YY__LBRACE || sym == YY_IF || sym == YY_SWITCH || sym == YY_WHILE || sym == YY_DO || sym == YY_FOR || sym == YY_GOTO || sym == YY_CONTINUE || sym == YY_BREAK || sym == YY_RETURN || sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__SEMICOLON || sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) && (!C_IS_ID(sym) || !is_typedef_name(sym))) {
			sym = parse_statement2(sym, val);
		} else {
			sym = parse_declaration(sym, 0);
		}
	}
	return sym;
}

static yy_sym parse_statement(yy_sym sym, c_value *last_val) {
	if ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(sym))) {
		sym = parse_labels(sym);
	}
	sym = parse_statement2(sym, last_val);
	return sym;
}

static yy_sym parse_labels(yy_sym sym) {
	do {
		c_label *label;
		c_name name;
		if (C_IS_ID(sym)) {
			sym = parse_ID(sym, &name);
			label = c_do_set_label(name);
			if (sym != YY__COLON) {
				yy_error_sym("':' expected, got", sym);
			}
			sym = get_sym();
			if (sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				c_dcl attrs = {0};
				sym = parse_attributes(sym, &attrs);
				c_do_set_label_attrs(label, &attrs);
			}
		} else if (sym == YY_CASE) {
			c_value val1;
			sym = get_sym();
			sym = parse_constant_expression(sym, &val1);
			if (sym == YY__POINT_POINT_POINT) {
				c_value val2;
				sym = get_sym();
				sym = parse_constant_expression(sym, &val2);
				c_do_case_range(&val1, &val2);
			} else if (sym == YY__COLON) {
				c_do_case(&val1);
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
			c_do_case_default();
		} else {
			yy_error_sym("unexpected", sym);
		}
	} while ((C_IS_ID(sym) || sym == YY_CASE || sym == YY_DEFAULT) && (!C_IS_ID(sym) || is_label(sym)));
	return sym;
}

static yy_sym parse_statement2(yy_sym sym, c_value *last_val) {
	c_value val = {0};
	c_name name;
	c_scope scope;
	if (last_val) c_value_clear(last_val);
	if (sym == YY__LBRACE) {
		sym = get_sym();
		c_push_scope(&scope);
		sym = parse_compound_statement(sym, NULL);
		c_pop_scope(&scope);
		if (sym != YY__RBRACE) {
			yy_error_sym("'}' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_IF) {
		ir_ref check;
		bool orig_dead_code = c_dead_code;
		c_push_scope(&scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		check = c_do_if(&val);
		sym = parse_statement(sym, NULL);
		if (sym == YY_ELSE) {
			sym = get_sym();
			c_do_if_else(check, orig_dead_code);
			sym = parse_statement(sym, NULL);
		}
		c_do_if_end(check, orig_dead_code);
		c_pop_scope(&scope);
	} else if (sym == YY_SWITCH) {
		c_loop loop;
		c_push_scope(&scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_switch(&loop, &val);
		sym = parse_statement(sym, NULL);
		c_do_switch_end(&loop);
		c_pop_scope(&scope);
	} else if (sym == YY_WHILE) {
		c_loop loop;
		c_push_scope(&scope);
		sym = get_sym();
		c_do_loop_start(&loop);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_check(&loop, &val);
		sym = parse_statement(sym, NULL);
		c_do_loop_end(&loop);
		c_pop_scope(&scope);
	} else if (sym == YY_DO) {
		c_loop loop;
		c_push_scope(&scope);
		sym = get_sym();
		c_do_loop_start(&loop);
		sym = parse_statement(sym, NULL);
		c_do_loop_continue_label(&loop);
		if (sym != YY_WHILE) {
			yy_error_sym("'while' expected, got", sym);
		}
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &val);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_check(&loop, &val);
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_loop_end(&loop);
		c_pop_scope(&scope);
	} else if (sym == YY_FOR) {
		c_loop loop;
		c_push_scope(&scope);
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		if ((sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__SEMICOLON) && (!C_IS_ID(sym) || !is_typedef_name(sym))) {
			if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
				sym = parse_expression(sym, &val);
			}
			if (sym != YY__SEMICOLON) {
				yy_error_sym("';' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__STATIC_ASSERT || sym == YY_TYPEDEF || sym == YY_EXTERN || sym == YY_STATIC || sym == YY_AUTO || sym == YY_REGISTER || sym == YY__THREAD_LOCAL || sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY_INLINE || sym == YY___INLINE || sym == YY___INLINE__ || sym == YY__NORETURN || sym == YY__ALIGNAS || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__STAR || sym == YY__LPAREN || sym == YY__SEMICOLON) {
			sym = parse_declaration(sym, C_DCL_FOR);
		} else {
			yy_error_sym("unexpected", sym);
		}
		c_do_loop_start(&loop);
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			sym = parse_expression(sym, &val);
			c_do_loop_check(&loop, &val);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			c_do_for_next_start(&loop);
			sym = parse_expression(sym, &val);
			c_do_for_next_end(&loop);
		}
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_statement(sym, NULL);
		c_do_for_end(&loop);
		c_pop_scope(&scope);
	} else if (sym == YY_GOTO) {
		sym = get_sym();
		if (C_IS_ID(sym)) {
			sym = parse_ID(sym, &name);
			c_do_goto(name);
		} else if (sym == YY__STAR) {
			sym = get_sym();
			sym = parse_expression(sym, &val);
			c_do_computed_goto(&val);
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
		c_do_continue();
	} else if (sym == YY_BREAK) {
		sym = get_sym();
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_break();
	} else if (sym == YY_RETURN) {
		sym = get_sym();
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			sym = parse_expression(sym, &val);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
		c_do_return(&val);
	} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE || sym == YY__SEMICOLON) {
		if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			sym = parse_expression(sym, last_val ? last_val : &val);
		}
		if (sym != YY__SEMICOLON) {
			yy_error_sym("';' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY_ASM || sym == YY___ASM || sym == YY___ASM__) {
		sym = get_sym();
		/*???*/yy_error("asm support not implemented yet");
		while (sym == YY_VOLATILE || sym == YY_INLINE || sym == YY_GOTO) {
			sym = get_sym();
		}
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_asm_argument(sym);
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

static yy_sym parse_asm_argument(yy_sym sym) {
	sym = parse_STRING(sym);
	if (sym == YY__COLON) {
		sym = get_sym();
		if (sym == YY_STRING || sym == YY__LBRACK) {
			sym = parse_asm_operands(sym);
		}
		if (sym == YY__COLON) {
			sym = get_sym();
			if (sym == YY_STRING || sym == YY__LBRACK) {
				sym = parse_asm_operands(sym);
			}
			if (sym == YY__COLON) {
				sym = get_sym();
				if (sym == YY_STRING) {
					sym = parse_asm_clobbers(sym);
				}
				if (sym == YY__COLON) {
					sym = get_sym();
					sym = parse_asm_goto_operands(sym);
				}
			}
		}
	}
	return sym;
}

static yy_sym parse_asm_operands(yy_sym sym) {
	sym = parse_asm_operand(sym);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_asm_operand(sym);
	}
	return sym;
}

static yy_sym parse_asm_operand(yy_sym sym) {
	c_name name;
	c_value v = {0};
	if (sym == YY_STRING) {
		sym = parse_STRING(sym);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &v);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else if (sym == YY__LBRACK) {
		sym = get_sym();
		sym = parse_ID(sym, &name);
		if (sym != YY__RBRACK) {
			yy_error_sym("']' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_STRING(sym);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_expression(sym, &v);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_asm_clobbers(yy_sym sym) {
	sym = parse_STRING(sym);
	if (sym != YY__COMMA) {
		yy_error_sym("',' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_STRING(sym);
	return sym;
}

static yy_sym parse_asm_goto_operands(yy_sym sym) {
	c_name name;
	sym = parse_ID(sym, &name);
	if (sym != YY__COMMA) {
		yy_error_sym("',' expected, got", sym);
	}
	sym = get_sym();
	sym = parse_ID(sym, &name);
	return sym;
}

static yy_sym parse_strings(yy_sym sym, c_value *val) {
	str_list list;
	list.str = yy_text;
	list.len = yy_len;
	list.next = NULL;
	sym = parse_STRING(sym);
	sym = parse_strings_tail(sym, val, &list, &list);
	return sym;
}

static yy_sym parse_strings_tail(yy_sym sym, c_value *val, str_list *first, str_list *last) {
	if (sym == YY_STRING) {
		str_list list;
		list.str = yy_text;
		list.len = yy_len;
		list.next = NULL;
		last->next = &list;
		sym = parse_STRING(sym);
		sym = parse_strings_tail(sym, val, first, &list);
	} else if (sym == YY__RPAREN || sym == YY__LBRACK || sym == YY__LPAREN || sym == YY__POINT || sym == YY__MINUS_GREATER || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT || sym == YY__QUERY || sym == YY__EQUAL || sym == YY__STAR_EQUAL || sym == YY__SLASH_EQUAL || sym == YY__PERCENT_EQUAL || sym == YY__PLUS_EQUAL || sym == YY__MINUS_EQUAL || sym == YY__LESS_LESS_EQUAL || sym == YY__GREATER_GREATER_EQUAL || sym == YY__AND_EQUAL || sym == YY__UPARROW_EQUAL || sym == YY__BAR_EQUAL || sym == YY__RBRACK || sym == YY__COMMA || sym == YY__SEMICOLON || sym == YY__RBRACE || sym == YY__COLON || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__ || sym == YY__POINT_POINT_POINT) {
		yy_strings(val, first, last);
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_actual_parameters(yy_sym sym, c_value *func) {
	int32_t num_args = 0;
	c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		sym = parse_assignment_expression(sym, &args[num_args]);
		num_args++;
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (num_args % C_ALLOCA_PARAMS == 0) args = c_do_grow_actual_parameters(args, num_args);
			sym = parse_assignment_expression(sym, &args[num_args]);
			num_args++;
		}
	}
	c_do_call(func, num_args, args);
	return sym;
}

static yy_sym parse_builtin_parameters(yy_sym sym, c_value *val, c_name name) {
	int32_t num_args = 0;
	c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		sym = parse_assignment_expression(sym, &args[num_args]);
		num_args++;
		while (sym == YY__COMMA) {
			sym = get_sym();
			if (num_args % C_ALLOCA_PARAMS == 0) args = c_do_grow_actual_parameters(args, num_args);
			sym = parse_assignment_expression(sym, &args[num_args]);
			num_args++;
		}
	}
	c_do_builtin(val, name, num_args, args);
	return sym;
}

static yy_sym parse_dummy_value(yy_sym sym, const c_type *t) {
	ir_ref old_control = c_do_nocode();
	c_value val;
	c_sym obj;
	size_t size = t->size;
	c_do_init_expr_start(&obj, t);
	sym = parse_initializer_contents(sym, &obj, t, 0, &size);
	c_do_init_expr_end(&val, &obj, size);
	c_do_end_nocode(old_control);
	return sym;
}

static yy_sym parse_unary_expression(yy_sym sym, c_value *val) {
	c_name name;
	const c_type *t;
	c_value v = {0};
	ir_ref old_control = IR_UNUSED;
	yy_sym op = sym;
	if (sym == YY__LPAREN) {
		sym = get_sym();
		if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
			sym = parse_type_name(sym, &t);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
			if (sym == YY__LBRACE) {
				c_sym obj;
				size_t size = t->size;
				c_do_init_expr_start(&obj, t);
				sym = parse_initializer_contents(sym, &obj, t, 0, &size);
				c_do_init_expr_end(val, &obj, size);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
				sym = parse_unary_expression(sym, val);
				c_do_cast(t, val);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			sym = parse_expression(sym, val);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__LBRACE) {
			c_scope scope;
			sym = get_sym();
			c_do_statement_expression(&scope, val);
			sym = parse_compound_statement(sym, val);
			c_pop_scope(&scope);
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
		sym = parse_ID(sym, &name);
		c_resolve_sym_name(val, name, sym);
	} else if (sym == YY_DECIMAL_NUMBER) {
		sym = parse_DECIMAL_NUMBER(sym, val);
	} else if (sym == YY_OCTAL_NUMBER) {
		sym = parse_OCTAL_NUMBER(sym, val);
	} else if (sym == YY_HEXADECIMAL_NUMBER) {
		sym = parse_HEXADECIMAL_NUMBER(sym, val);
	} else if (sym == YY_BINARY_NUMBER) {
		sym = parse_BINARY_NUMBER(sym, val);
	} else if (sym == YY_FLOATING_NUMBER) {
		sym = parse_FLOATING_NUMBER(sym, val);
	} else if (sym == YY_HEXADECIMAL_FLOATING_NUMBER) {
		sym = parse_HEXADECIMAL_FLOATING_NUMBER(sym, val);
	} else if (sym == YY_CHARACTER) {
		sym = parse_CHARACTER(sym, val);
	} else if (sym == YY_STRING) {
		sym = parse_strings(sym, val);
	} else if (sym == YY__GENERIC) {
		c_generic g;
		sym = get_sym();
		c_do_generic_start(&g);
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_assignment_expression(sym, &v);
		c_do_generic_type(&g, v.type);
		if (sym != YY__COMMA) {
			yy_error_sym("',' expected, got", sym);
		}
		do {
			sym = get_sym();
			if (sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) {
				sym = parse_type_name(sym, &t);
				if (sym != YY__COLON) {
					yy_error_sym("':' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_assignment_expression(sym, &v);
				c_do_generic_case(&g, t, &v);
			} else if (sym == YY_DEFAULT) {
				sym = get_sym();
				if (sym != YY__COLON) {
					yy_error_sym("':' expected, got", sym);
				}
				sym = get_sym();
				sym = parse_assignment_expression(sym, &v);
				c_do_generic_default(&g, &v);
			} else {
				yy_error_sym("unexpected", sym);
			}
		} while (sym == YY__COMMA);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_do_generic_end(val, &g);
	} else if (sym == YY___EXTENSION__) {
		sym = get_sym();
		sym = parse_unary_expression(sym, val);
	} else if (sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS) {
		sym = get_sym();
		sym = parse_unary_expression(sym, val);
		c_do_pre_op(op, val);
	} else if (sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG) {
		if (sym == YY__AND) {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_addr(val);
		} else if (sym == YY__STAR) {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_deref(val);
		} else if (sym == YY__PLUS) {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_unary_plus(val);
		} else if (sym == YY__MINUS) {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_neg(val);
		} else if (sym == YY__TILDE) {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_not(val);
		} else {
			sym = get_sym();
			sym = parse_unary_expression(sym, val);
			c_do_bool_not(val);
		}
	} else if (sym == YY_SIZEOF) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
				sym = parse_type_name(sym, &t);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
				if (sym == YY__LBRACE) {
					sym = parse_dummy_value(sym, t);
				}
				c_sizeof_type(val, t);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
				old_control = c_do_nocode();
				sym = parse_expression(sym, val);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY__LBRACE) {
				c_scope scope;
				sym = get_sym();
				c_do_statement_expression(&scope, val);
				sym = parse_compound_statement(sym, val);
				c_pop_scope(&scope);
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
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			ir_ref old = c_do_nocode();
			sym = parse_unary_expression(sym, &v);
			c_sizeof_expr(val, op, &v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__ALIGNOF) {
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_type_name(sym, &t);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
		c_alignof_type(val, t);
	} else if (sym == YY___ALIGNOF__ || sym == YY___ALIGNOF) {
		sym = get_sym();
		if ((sym == YY__LPAREN) && synpred__lparen(sym)) {
			sym = get_sym();
			if ((sym == YY_VOID || sym == YY_CHAR || sym == YY_SHORT || sym == YY_INT || sym == YY_LONG || sym == YY_FLOAT || sym == YY_DOUBLE || sym == YY_SIGNED || sym == YY___SIGNED || sym == YY___SIGNED__ || sym == YY_UNSIGNED || sym == YY__BOOL || sym == YY__COMPLEX || sym == YY___COMPLEX || sym == YY___COMPLEX__ || sym == YY__ATOMIC || sym == YY_TYPEOF || sym == YY___TYPEOF || sym == YY___TYPEOF__ || sym == YY_STRUCT || sym == YY_UNION || sym == YY_ENUM || C_IS_ID(sym) || sym == YY_CONST || sym == YY___CONST || sym == YY___CONST__ || sym == YY_RESTRICT || sym == YY___RESTRICT || sym == YY___RESTRICT__ || sym == YY_VOLATILE || sym == YY___VOLATILE || sym == YY___VOLATILE__ || sym == YY___ATTRIBUTE || sym == YY___ATTRIBUTE__) && (!C_IS_ID(sym) || is_typedef_name(sym))) {
				sym = parse_type_name(sym, &t);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
				if (sym == YY__LBRACE) {
					sym = parse_dummy_value(sym, t);
				}
				c_alignof_type(val, t);
			} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
				old_control = c_do_nocode();
				sym = parse_expression(sym, val);
				if (sym != YY__RPAREN) {
					yy_error_sym("')' expected, got", sym);
				}
				sym = get_sym();
			} else if (sym == YY__LBRACE) {
				c_scope scope;
				sym = get_sym();
				c_do_statement_expression(&scope, val);
				sym = parse_compound_statement(sym, val);
				c_pop_scope(&scope);
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
		} else if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
			ir_ref old = c_do_nocode();
			sym = parse_unary_expression(sym, &v);
			c_sizeof_expr(val, op, &v, old);
		} else {
			yy_error_sym("unexpected", sym);
		}
	} else if (sym == YY__AND_AND) {
		sym = get_sym();
		sym = parse_ID(sym, &name);
		c_do_label_value(val, name);
	} else if (sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		name = sym;
		sym = get_sym();
		if (sym != YY__LPAREN) {
			yy_error_sym("'(' expected, got", sym);
		}
		sym = get_sym();
		sym = parse_builtin_parameters(sym, val, name);
		if (sym != YY__RPAREN) {
			yy_error_sym("')' expected, got", sym);
		}
		sym = get_sym();
	} else {
		yy_error_sym("unexpected", sym);
	}
	while (sym == YY__LBRACK || sym == YY__LPAREN || sym == YY__POINT || sym == YY__MINUS_GREATER || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS) {
		if (sym == YY__LBRACK) {
			c_value dim = {0};
			sym = get_sym();
			sym = parse_expression(sym, &dim);
			if (sym != YY__RBRACK) {
				yy_error_sym("']' expected, got", sym);
			}
			sym = get_sym();
			c_do_array_dim(val, &dim);
		} else if (sym == YY__LPAREN) {
			sym = get_sym();
			sym = parse_actual_parameters(sym, val);
			if (sym != YY__RPAREN) {
				yy_error_sym("')' expected, got", sym);
			}
			sym = get_sym();
		} else if (sym == YY__POINT) {
			sym = get_sym();
			sym = parse_ID(sym, &name);
			c_do_struct_field(val, name);
		} else if (sym == YY__MINUS_GREATER) {
			sym = get_sym();
			sym = parse_ID(sym, &name);
			c_do_struct_field_deref(val, name);
		} else {
			yy_sym post_op = sym;
			sym = get_sym();
			c_do_post_op(post_op, val);
		}
	}
	if (old_control) c_sizeof_expr(val, op, val, old_control);
	return sym;
}

static yy_sym parse_infix_expression(yy_sym sym, c_value *val, yy_sym prev) {
	c_value op2;
	ir_ref if_ref = IR_UNUSED;
	bool orig_dead_code = 0;
	do {
		yy_sym next, op = sym;
		if (sym == YY__BAR_BAR) {
			orig_dead_code = c_dead_code;
			if_ref = c_do_bool_or_start(val);
			sym = get_sym();
			next = YY__BAR_BAR;
		} else if (sym == YY__AND_AND) {
			orig_dead_code = c_dead_code;
			if_ref = c_do_bool_and_start(val);
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
		sym = parse_unary_expression(sym, &op2);
		if ((sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) && (sym >= YY__STAR && sym < next)) {
			sym = parse_infix_expression(sym, &op2, next - 1);
		}
		if (op == YY__BAR_BAR) {
			c_do_bool_or_end(val, &op2, if_ref);
			c_dead_code = orig_dead_code;
		} else if (op == YY__AND_AND) {
			c_do_bool_and_end(val, &op2, if_ref);
			c_dead_code = orig_dead_code;
		} else {
			c_do_binary_op(op, val, &op2);
		}
	} while ((sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) && (sym <= prev));
	return sym;
}

static yy_sym parse_conditional_expression(yy_sym sym, c_value *val) {
	ir_ref check;
	bool orig_dead_code = c_dead_code;
	c_value op2 = {0}, op3;
	if (sym != YY__QUERY) {
		yy_error_sym("'?' expected, got", sym);
	}
	sym = get_sym();
	check = c_do_if(val);
	if (sym == YY__LPAREN || C_IS_ID(sym) || sym == YY_DECIMAL_NUMBER || sym == YY_OCTAL_NUMBER || sym == YY_HEXADECIMAL_NUMBER || sym == YY_BINARY_NUMBER || sym == YY_FLOATING_NUMBER || sym == YY_HEXADECIMAL_FLOATING_NUMBER || sym == YY_CHARACTER || sym == YY_STRING || sym == YY__GENERIC || sym == YY___EXTENSION__ || sym == YY__PLUS_PLUS || sym == YY__MINUS_MINUS || sym == YY__AND || sym == YY__STAR || sym == YY__PLUS || sym == YY__MINUS || sym == YY__TILDE || sym == YY__BANG || sym == YY_SIZEOF || sym == YY__ALIGNOF || sym == YY___ALIGNOF__ || sym == YY___ALIGNOF || sym == YY__AND_AND || sym == YY___BUILTIN_VA_START || sym == YY___BUILTIN_VA_ARG || sym == YY___BUILTIN_VA_END || sym == YY___BUILTIN_VA_COPY || sym == YY___BUILTIN_ALLOCA || sym == YY___BUILTIN_ABORT || sym == YY___BUILTIN_TRAP || sym == YY___BUILTIN_DEBUGTRAP || sym == YY___BUILTIN_FRAME_ADDRESS || sym == YY___BUILTIN_CONSTANT_P || sym == YY___BUILTIN_ABS || sym == YY___BUILTIN_LABS || sym == YY___BUILTIN_LLABS || sym == YY___BUILTIN_FABS || sym == YY___BUILTIN_FABSF || sym == YY___BUILTIN_BSWAP16 || sym == YY___BUILTIN_BSWAP32 || sym == YY___BUILTIN_BSWAP64 || sym == YY___BUILTIN_POPCOUNT || sym == YY___BUILTIN_POPCOUNTL || sym == YY___BUILTIN_POPCOUNTLL || sym == YY___BUILTIN_CLZ || sym == YY___BUILTIN_CLZL || sym == YY___BUILTIN_CLZLL || sym == YY___BUILTIN_CTZ || sym == YY___BUILTIN_CTZL || sym == YY___BUILTIN_CTZLL || sym == YY___BUILTIN_MEMCPY || sym == YY___BUILTIN_MEMSET || sym == YY___BUILTIN_EXPECT || sym == YY___BUILTIN_UNREACHABLE) {
		sym = parse_expression(sym, &op2);
		c_value_rval(&op2);
	}
	if (sym != YY__COLON) {
		yy_error_sym("':' expected, got", sym);
	}
	sym = get_sym();
	c_do_if_else(check, orig_dead_code);
	sym = parse_unary_expression(sym, &op3);
	if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
		sym = parse_infix_expression(sym, &op3, YY__BAR_BAR);
	}
	if (sym == YY__QUERY) {
		sym = parse_conditional_expression(sym, &op3);
	}
	c_value_rval(&op3);
	c_do_if_end(check, orig_dead_code);
	c_do_cond_op(val, &op2, &op3);
	return sym;
}

static yy_sym parse_assignment_expression(yy_sym sym, c_value *val) {
	sym = parse_unary_expression(sym, val);
	if (sym == YY__EQUAL || sym == YY__STAR_EQUAL || sym == YY__SLASH_EQUAL || sym == YY__PERCENT_EQUAL || sym == YY__PLUS_EQUAL || sym == YY__MINUS_EQUAL || sym == YY__LESS_LESS_EQUAL || sym == YY__GREATER_GREATER_EQUAL || sym == YY__AND_EQUAL || sym == YY__UPARROW_EQUAL || sym == YY__BAR_EQUAL) {
		int op = sym;
		c_value op2;
		sym = get_sym();
		sym = parse_assignment_expression(sym, &op2);
		c_do_assign_op(op, val, &op2);
	} else if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT || sym == YY__QUERY || sym == YY__RBRACK || sym == YY__COMMA || sym == YY__SEMICOLON || sym == YY__RBRACE || sym == YY__RPAREN || sym == YY__COLON) {
		if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
			sym = parse_infix_expression(sym, val, YY__BAR_BAR);
		}
		if (sym == YY__QUERY) {
			sym = parse_conditional_expression(sym, val);
		}
	} else {
		yy_error_sym("unexpected", sym);
	}
	return sym;
}

static yy_sym parse_expression(yy_sym sym, c_value *val) {
	sym = parse_assignment_expression(sym, val);
	while (sym == YY__COMMA) {
		sym = get_sym();
		sym = parse_assignment_expression(sym, val);
	}
	return sym;
}

static yy_sym parse_constant_expression(yy_sym sym, c_value *val) {
	sym = parse_unary_expression(sym, val);
	if (sym == YY__BAR_BAR || sym == YY__AND_AND || sym == YY__BAR || sym == YY__UPARROW || sym == YY__AND || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__LESS || sym == YY__GREATER || sym == YY__LESS_EQUAL || sym == YY__GREATER_EQUAL || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER || sym == YY__PLUS || sym == YY__MINUS || sym == YY__STAR || sym == YY__SLASH || sym == YY__PERCENT) {
		sym = parse_infix_expression(sym, val, YY__BAR_BAR);
	}
	if (sym == YY__QUERY) {
		sym = parse_conditional_expression(sym, val);
	}
	return sym;
}

static yy_sym parse_ID(yy_sym sym, c_name *name) {
	if (!C_IS_ID(sym)) {
		yy_error_sym("<ID> expected, got", sym);
	}
	*name = sym;
	sym = get_sym();
	return sym;
}

static yy_sym parse_DECIMAL_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_DECIMAL_NUMBER) {
		yy_error_sym("<DECIMAL_NUMBER> expected, got", sym);
	}
	yy_read_dec(val, yy_text, yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_OCTAL_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_OCTAL_NUMBER) {
		yy_error_sym("<OCTAL_NUMBER> expected, got", sym);
	}
	yy_read_oct(val, yy_text, yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_HEXADECIMAL_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_HEXADECIMAL_NUMBER) {
		yy_error_sym("<HEXADECIMAL_NUMBER> expected, got", sym);
	}
	yy_read_hex(val, yy_text + 2, yy_len - 2);
	sym = get_sym();
	return sym;
}

static yy_sym parse_BINARY_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_BINARY_NUMBER) {
		yy_error_sym("<BINARY_NUMBER> expected, got", sym);
	}
	yy_read_bin(val, yy_text + 2, yy_len - 2);
	sym = get_sym();
	return sym;
}

static yy_sym parse_FLOATING_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_FLOATING_NUMBER) {
		yy_error_sym("<FLOATING_NUMBER> expected, got", sym);
	}
	yy_read_fp(val, yy_text, yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_HEXADECIMAL_FLOATING_NUMBER(yy_sym sym, c_value *val) {
	if (sym != YY_HEXADECIMAL_FLOATING_NUMBER) {
		yy_error_sym("<HEXADECIMAL_FLOATING_NUMBER> expected, got", sym);
	}
	yy_read_fp(val, yy_text, yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_CHARACTER(yy_sym sym, c_value *val) {
	if (sym != YY_CHARACTER) {
		yy_error_sym("<CHARACTER> expected, got", sym);
	}
	yy_read_char(val, yy_text, yy_len);
	sym = get_sym();
	return sym;
}

static yy_sym parse_STRING(yy_sym sym) {
	if (sym != YY_STRING) {
		yy_error_sym("<STRING> expected, got", sym);
	}
	sym = get_sym();
	return sym;
}

static void parse(void) {
	int sym;

	yy_pos = yy_text = yy_buf;
	yy_linepos = yy_pos;
	yy_line = 1;
	sym = parse_translation_unit(get_sym());
	if (sym != YY_EOF) {
		yy_error_sym("<EOF> expected, got", sym);
	}
}

enum {
	YY_UNSIGNED_INT       = IR_U32,
	YY_UNSIGNED_LONG      = IR_U64,
	YY_UNSIGNED_LONG_LONG = IR_U64,
	YY_SIGNED_LONG        = IR_I64,
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

static void yy_check_int_type(const c_type **ctype, ir_type *type, ir_val val)
{
	if (!*ctype) {
		if (val.u64 > 0x7fffffffffffffff) {
			*ctype = &c_type_u64;
			*type = IR_U64;
		} else if (val.u64 > 0x7fffffff) {
			*ctype = &c_type_i64;
			*type = IR_I64;
		} else {
			*ctype = &c_type_i32;
			*type = IR_I32;
	    }
	} else if (*ctype == &c_type_u32 && val.u64 > 0xffffffff) {
		*ctype = &c_type_u64;
		*type = IR_U64;
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
	yy_check_int_type(&ctype, &type, val);
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
	yy_check_int_type(&ctype, &type, val);
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
	yy_check_int_type(&ctype, &type, val);
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
	yy_check_int_type(&ctype, &type, val);
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

static uint32_t yy_unicode_character(const char *str, size_t len)
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

static void yy_read_char(c_value *res, const char *p, size_t len)
{
	ir_val val;
	char prefix = 0;
	uint32_t ch = (const unsigned char)*p++;

	if (ch == 'L' || ch == 'u' || ch == 'U') {
		prefix = ch;
		ch = (const unsigned char)*p++;
	}

	IR_ASSERT(ch == '\'');

	ch = (const unsigned char)*p++;
	if (ch == '\\') {
		ch = (const unsigned char)*p++;
		switch (ch) {
			case '\\': ch = '\\'; break;
			case '\'': ch = '\''; break;
			case '"':  ch = '"';  break;
			case 'a':  ch = '\a'; break;
			case 'b':  ch = '\b'; break;
			case 'e':  ch = 27;   break; /* '\e'; */
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
				ch = yy_unicode_character(p, 4);
				p += 4;
				break;
			case 'U':
				ch = yy_unicode_character(p, 8);
				p += 8;
				break;
			default:
				yy_error("unsupported escape sequence");
				break;
		}
	}
	if (*p != '\'') yy_error("multi-character character constant");
	val.u64 = ch;
	if (!prefix) {
		c_value_set_const(res, &c_type_i32, IR_I32, val);
	} else if (prefix == 'L') {
		c_value_set_const(res, &c_type_i32, IR_I32, val);
	} else if (prefix == 'u') {
		c_value_set_const(res, &c_type_u16, IR_U16, val);
	} else {
		IR_ASSERT(prefix == 'U');
		c_value_set_const(res, &c_type_u32, IR_U32, val);
	}
}

static void yy_append_utf8(yy_dyn_str *dyn_str, uint32_t n)
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

	yy_dyn_str_append(dyn_str, buf, len);
}

static void yy_append_unicode_str(yy_dyn_str *dyn_str, char prefix, const char *str, size_t len)
{
	if (prefix == 'u') {
		uint16_t *dst = (uint16_t*)yy_dyn_str_grow(dyn_str, len * 2);
		unsigned char *p = (unsigned char*)str;

		dyn_str->len += len * 2;
		while (len > 0) {
			*dst = *p;
			dst++;
			p++;
			len--;
		}
	} else {
		uint32_t *dst = (uint32_t*)yy_dyn_str_grow(dyn_str, len * 4);
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

static void yy_append_unicode_char(yy_dyn_str *dyn_str, char prefix, uint32_t uc)
{
	if (prefix == 'u') {
		if (uc < 0x10000) {
			uint16_t *dst = (uint16_t*)yy_dyn_str_grow(dyn_str, 2);
			*dst = uc;
			dyn_str->len += 2;
		} else {
			uint16_t *dst = (uint16_t*)yy_dyn_str_grow(dyn_str, 4);
			uc -= 0x10000;
			dst[0] = (uc >> 10) + 0xd800;
			dst[1] = (uc & 0x3ff) + 0xdc00;
			dyn_str->len += 4;
		}
	} else {
		uint32_t *dst = (uint32_t*)yy_dyn_str_grow(dyn_str, 4);
		*dst = uc;
		dyn_str->len += 4;
	}
}

static char yy_strings_append(yy_dyn_str *dyn_str, char prefix, const char *str, size_t len)
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
			if (ch != '\\') {
				ch = *p++;
			} else {
				if (s != p - 1) yy_dyn_str_append(dyn_str, s, p - s - 1);
				ch = *p++;
				switch (ch) {
					case '\\': ch = '\\'; break;
					case '\'': ch = '\''; break;
					case '"':  ch = '"';  break;
					case 'a':  ch = '\a'; break;
					case 'b':  ch = '\b'; break;
					case 'e':  ch = 27;   break; /* '\e'; */
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
					case '\n':
						s = p;
						ch = *p++;
						continue;
					case 'u':
						uc = yy_unicode_character(p, 4);
						yy_append_utf8(dyn_str, uc);
						s = p + 4;
						ch = *p++;
						continue;
					case 'U':
						uc = yy_unicode_character(p, 8);
						yy_append_utf8(dyn_str, uc);
						s = p + 8;
						ch = *p++;
						continue;
					default:
						yy_error("unsupported escape sequence");
						break;
				}
				yy_dyn_str_append(dyn_str, &ch, 1);
				s = p;
				ch = *p++;
			}
		}

		if (s != p - 1) yy_dyn_str_append(dyn_str, s, p - s - 1);

	} else {

		while (ch != '"') {
			if ((unsigned char)ch > 0x7f) {
				unsigned char c = (unsigned char)ch;
				if (s != p - 1) yy_append_unicode_str(dyn_str, prefix, s, p - s - 1);
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
bad_utf8:			yy_error("bad UTF-8 sequence");
				}
				yy_append_unicode_char(dyn_str, prefix, uc);
				s = p;
				ch = *p++;
			} else if (ch != '\\') {
				ch = *p++;
			} else {
				if (s != p - 1) yy_append_unicode_str(dyn_str, prefix, s, p - s - 1);
				ch = *p++;
				switch (ch) {
					case '\\': ch = '\\'; break;
					case '\'': ch = '\''; break;
					case '"':  ch = '"';  break;
					case 'a':  ch = '\a'; break;
					case 'b':  ch = '\b'; break;
					case 'e':  ch = 27;   break; /* '\e'; */
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
					case '\n':
						s = p;
						ch = *p++;
						continue;
					case 'u':
						uc = yy_unicode_character(p, 4);
						yy_append_unicode_char(dyn_str, prefix, uc);
						s = p + 4;
						ch = *p++;
						continue;
					case 'U':
						uc = yy_unicode_character(p, 8);
						yy_append_unicode_char(dyn_str, prefix, uc);
						s = p + 8;
						ch = *p++;
						continue;
					default:
						yy_error("unsupported escape sequence");
						break;
				}
				yy_append_unicode_char(dyn_str, prefix, (uint8_t)ch);
				s = p;
				ch = *p++;
			}
		}

		if (s != p - 1) yy_append_unicode_str(dyn_str, prefix, s, p - s - 1);
	}
	IR_ASSERT(p == str + len);
	return prefix;
}

static void yy_strings(c_value *res, str_list *first, str_list *last)
{
	yy_dyn_str dyn_str;
	char prefix = 0;
	const c_type *type;

	yy_dyn_str_init(&dyn_str, "", 0);
	do {
		prefix = yy_strings_append(&dyn_str, prefix, first->str, first->len);
		first = first->next;
	} while(first);

	if (!prefix) {
		yy_dyn_str_append(&dyn_str, "\0", 1);
		type = &c_type_string;
	} else if (prefix == 'L') {
		yy_dyn_str_append(&dyn_str, "\0\0\0\0", 4);
		type = &c_type_lstring;
	} else if (prefix == 'u') {
		yy_dyn_str_append(&dyn_str, "\0\0", 2);
		type = &c_type_string_u16;
	} else {
		IR_ASSERT(prefix == 'U');
		yy_dyn_str_append(&dyn_str, "\0\0\0\0", 4);
		type = &c_type_string_u32;
	}

	c_value_set_const_str(res, type, IR_ADDR, dyn_str.str, dyn_str.len);
}

/* CPP helper */
bool parse_pp_expr(void)
{
	bool ret;
	c_value res;
	bool old_dead_code = c_dead_code;
	ir_ctx *old_ctx = active_ctx;
	uint32_t old_flags = yy_flags;
	yy_sym sym;

	active_ctx = global_ctx;
	yy_flags |= PP_EVAL_EXPRESSION;
	yy_flags &= ~YY_ACCEPT_PP_NUMBER;
	c_dead_code = 0;

	sym = get_sym();
	sym = parse_constant_expression(sym, &res);
	if (sym != YY_EOF) yy_error_fmt("token \"%s\" is not valid in preprocessor expressions", yy_sym2str(sym));

	ret = c_value_is_true(&res);

	c_dead_code = old_dead_code;
	yy_flags = old_flags;
	active_ctx = old_ctx;

	return ret;
}

void rcc_parse(void)
{
	if (0) {
		parse();
	} else {
		/* parse starting from yy_pos */
		yy_sym sym = parse_translation_unit(get_sym());

		if (sym != YY_EOF) {
			yy_error_sym("<EOF> expected, got", sym);
		}
	}
}
