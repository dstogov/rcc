/*
 * RCC - Rational C Compiler
 * (C parser)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 *
 * To generate rcc_parser.c use llk <https://github.com/dstogov/llk>:
 * php llk.php c.g
 */

%start          translation_unit
%case-sensetive true
%global-vars    false
%lineno         true
%linepos        true
%ignore-scanner true
%sym-type       yy_sym
%c-char         "char"
%check-id       C_IS_ID
%output         "rcc_parser.c"
%language       "c"
%indent         "\t"

%{
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
static void yy_read_fp(c_value *res, const char *p, size_t len);
static void yy_read_char(c_value *res, const char *p, size_t len);

%}

translation_unit:
	(	simple_asm_expr ";"
	|	"__extension__"? declaration
	)*
;

simple_asm_expr:
	("asm"|"__asm"|"__asm__") "(" STRING+ ")"              {/*???*/yy_error("asm support not implemented yet");}
;

/* Declarations */
declaration:                                               {c_dcl d0 = {0};}
                                                           {c_name name;}
                                                           {c_sym *obj;}
	(	static_assert_declaration ";"
	|	/* use "?" to support C89 defaults to int */
		(   ?{!C_IS_ID(sym) || is_typedef_name(sym)}
			declaration_specifiers(&d0)
		)?
		(	                                               {c_dcl d = d0;}
		    declarator(&d, &name, 1)
			(   &(	"__attribute__"
				|	"__attribute"
				|	"__asm__"
				|	"__asm"
				|	"asm"
				|	"="
				|	","
				|	";")
				simple_asm_expr?
				attributes(&d)?                            {if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;}
				                                           {obj = c_declare(name, &d);}
				("=" initializer(obj))?
				(
					","                                    {d = d0;}
					declarator(&d, &name, 0)
					simple_asm_expr?
					attributes(&d)?                        {if (sym == YY__EQUAL) d.flags |= C_DCL_DEFINITION;}
					                                       {obj = c_declare(name, &d);}
					("=" initializer(obj))?
				)*
				";"
			|   /* funcrion-definition */                  {ir_ctx ctx, *old_ctx = active_ctx;}
			                                               {c_scope scope;}
			                                               {if (!d.type || d.type->kind != C_TYPE_FUNC) yy_error_sym("unexpected", sym);}
				(   /* old style arguments */
					old_style_param_declaration(d.type)+   {c_validate_func_params(name, &d);}
				)?                                         {c_do_func_start(name, &d, &scope, &ctx);}
				"{"
				compound_statement(NULL)                   {c_do_func_end(name, &d, &scope, &ctx);}
                "}"                                        {active_ctx = old_ctx;}
			)
		|	";"                                            {c_empty_declaration(&d0);}
		)
	)
;

old_style_param_declaration(const c_type *t):              {c_dcl d0 = {0};}
                                                           {c_name name;}
	declaration_specifiers(&d0)                            {c_dcl d = d0;}
	declarator(&d, &name, 0)
	simple_asm_expr?
	attributes(&d)?                                        {c_declare_func_param_type(t, name, &d);}
	(	"="                                                {yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(name));}
		initializer(NULL)
	)?
	(
		","                                                {d = d0;}
		declarator(&d, &name, 0)
		simple_asm_expr?
		attributes(&d)?                                    {c_declare_func_param_type(t, name, &d);}
		(	"="                                            {yy_error_fmt("parameter \"%s\" is initialized", yy_sym2str(name));}
			initializer(NULL)
		)?
	)*
	";"
;

declaration_specifiers(c_dcl *d):
	(	?{!C_IS_ID(sym) || is_typedef_name2(sym, d)}
		(	storage_class_specifier(d)
		|	type_specifier_or_qualifier(d)
		|	function_specifier(d)
		|	alignment_specifier(d)
		|	attributes(d)
		)
	)+
;

specifier_qualifier_list(c_dcl *d):
	(	?{!C_IS_ID(sym) || is_typedef_name2(sym, d)}
		(	type_specifier_or_qualifier(d)
		|	attributes(d)
		)
	)+
;

type_qualifier_list(c_dcl *d):
	(	type_qualifier(d)
	|	attributes(d)
	)+
;

storage_class_specifier(c_dcl *d):
	(	                                                   {if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");}
		"typedef"                                          {d->flags |= C_DCL_TYPEDEF;}
	|	                                                   {if (d->flags & (C_DCL_STORAGE_CLASS-C_DCL_THREAD_LOCAL)) yy_error("multiple storage classes in declaration specifiers");}
		"extern"                                           {d->flags |= C_DCL_EXTERN;}
	|                                                      {if (d->flags & (C_DCL_STORAGE_CLASS-C_DCL_THREAD_LOCAL)) yy_error("multiple storage classes in declaration specifiers");}
		"static"                                           {d->flags |= C_DCL_STATIC;}
	|                                                      {if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");}
		"auto"                                             {d->flags |= C_DCL_AUTO;}
	|                                                      {if (d->flags & C_DCL_STORAGE_CLASS) yy_error("multiple storage classes in declaration specifiers");}
		"register"                                         {d->flags |= C_DCL_REGISTER;}
	|                                                      {if (d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC))) yy_error("multiple storage classes in declaration specifiers");}
		"_Thread_local"                                    {d->flags |= C_DCL_THREAD_LOCAL;}
	)
;

type_specifier_or_qualifier(c_dcl *d):                     {c_name name;}
	(                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		"void"                                             {d->flags |= C_TYPE_SPEC_VOID;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED))) yy_error_sym("unexpected", sym);}
		"char"                                             {d->flags |= C_TYPE_SPEC_CHAR;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT))) yy_error_sym("unexpected", sym);}
		"short"                                            {d->flags |= C_TYPE_SPEC_SHORT;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);}
		"int"                                              {d->flags |= C_TYPE_SPEC_INT;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_LONG|C_TYPE_SPEC_INT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_COMPLEX))) yy_error_sym("unexpected", sym);}
		"long"                                             {d->flags |= (d->flags & C_TYPE_SPEC_LONG) ? C_TYPE_SPEC_LONG_LONG : C_TYPE_SPEC_LONG;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-C_TYPE_SPEC_COMPLEX)) yy_error_sym("unexpected", sym);}
		"float"                                            {d->flags |= C_TYPE_SPEC_FLOAT;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_LONG|C_TYPE_SPEC_COMPLEX))) yy_error_sym("unexpected", sym);}
		"double"                                           {d->flags |= C_TYPE_SPEC_DOUBLE;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);}
		"signed"                                           {d->flags |= C_TYPE_SPEC_SIGNED;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG))) yy_error_sym("unexpected", sym);}
		"unsigned"                                         {d->flags |= C_TYPE_SPEC_UNSIGNED;}
	|                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		"_Bool"                                            {d->flags |= C_TYPE_SPEC_BOOL;}
	|                                                      {if (d->flags & (C_TYPE_SPEC_ANY-(C_TYPE_SPEC_FLOAT|C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_LONG))) yy_error_sym("unexpected", sym);}
		("_Complex"|"__complex"|"__complex__")             {d->flags |= C_TYPE_SPEC_COMPLEX;}
	|	"_Atomic"
		(	&"(" "("
                                                           {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
                                                           {d->flags |= C_TYPE_SPEC_ATOMIC;}
			type_name(&d->type) ")"
		|	/* empty - _Atomic qualifier */                {d->attr |= C_ATTR_ATOMIC;}
		)
	|                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		"typeof"                                           {d->flags |= C_TYPE_SPEC_TYPE;}
		"("
		(	?{!C_IS_ID(sym) || is_typedef_name(sym)}
			type_name(&d->type)
		|                                                  {c_value v = {0};}
			expression(&v)                                 {d->type = c_typeof_expr(&v);/*???*/}
		)
		")"
	|                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		struct_or_union_specifier(d)
	|                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		enum_specifier(d)
	|                                                      {if (d->flags & C_TYPE_SPEC_ANY) yy_error_sym("unexpected", sym);}
		ID(&name) /* typedef name */                       {d->flags |= C_TYPE_SPEC_NAME;}
                                                           {d->type = c_resolve_type_name(name);}
	/* type_qualifier: */
	|	("const"|"__const"|"__const__")                    {d->attr |= C_ATTR_CONST;}
	|	                                                   {if (!d->type || d->type->kind != C_TYPE_POINTER) yy_error("invalid use of \"restrict\"");}
		("restrict"|"__restrict"|"__restrict__")           {d->attr |= C_ATTR_RESTRICT;}
	|	("volatile"|"__volatile"|"__volatile__")           {d->attr |= C_ATTR_VOLATILE;}
	)
;

type_qualifier(c_dcl *d):
		("const"|"__const"|"__const__")                    {d->attr |= C_ATTR_CONST;}
	|	("restrict"|"__restrict"|"__restrict__")           {d->attr |= C_ATTR_RESTRICT;}
	|	("volatile"|"__volatile"|"__volatile__")           {d->attr |= C_ATTR_VOLATILE;}
	|	"_Atomic"                                          {d->attr |= C_ATTR_ATOMIC;}
;

function_specifier(c_dcl *d):
		("inline"|"__inline"|"__inline__")                 {d->attr |= C_ATTR_INLINE;}
	|	"_Noreturn"                                        {d->attr |= C_ATTR_NORETURN;}
//	|	"__cdecl"
//	|	"__stdcall"
//	|	"__fastcall"
//	|	"__thiscall"
//	|	"__vectorcall"
;

alignment_specifier(c_dcl *d):                             {c_value v = {0};}
	"_Alignas"                                             {if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");}
	"("
	(   ?{!C_IS_ID(sym) || is_typedef_name(sym)}          {const c_type *t;}
		type_name(&t)                                      {d->attr |= t->attr & C_ATTR_ALIGN_MASK;}
	|	constant_expression(&v)                            {c_alignas_expr(d, &v);}
	)
	")"
;

attributes(c_dcl *d):
	(	("__attribute"|"__attribute__")
		"(" "(" attrib(d) ( "," attrib(d) )* ")" ")"
//	|	"__declspec" "("
//		ID
//		(
//			"(" assignment_expression(NULL) ")"
//		)?
//		")"
	)++
;

attrib(c_dcl *d):
	(                                                      {c_name name = sym;}
	                                                       {c_value v = {0};}
		(ID(&name)|"const"|"__const__")
		("(" expression(&v) ")")?                          {c_gcc_attribute(d, name, &v);}
	)?
;

struct_or_union_specifier(c_dcl *d):                       {c_name name;}
	(	"struct"                                           {d->flags |= C_TYPE_SPEC_STRUCT;}
	|	"union"                                            {d->flags |= C_TYPE_SPEC_UNION;}
	)
	attributes(d)?
	(	ID(&name)
		(   /* empty */                                    {c_resolve_tag(name, d, 0);}
		|	                                               {c_type *t = c_resolve_tag(name, d, 1);}
			struct_contents(t, d)
		)
	|                                                      {c_type *t = c_make_struct_type(d, 0);}
		struct_contents(t, d)
	)
;

struct_contents(c_type *t, c_dcl *d):                      {t->record.fields = alloca(sizeof(c_field) * C_ALLOCA_FIELDS);}
	"{"                                                    {t->flags |= C_TYPE_INPROGRESS;}
	(	struct_declaration(t)
		(	";"
			(	&"}"                                       {break; /* manual conflict resolution */}
				"}"
			|	struct_declaration(t)
			)
		)+
	|	";"
	|	/* empty */
	)
	"}"
	attributes(d)?+                                        {c_finish_struct_type(t, d);}
;

struct_declaration(c_type *t):                             {c_dcl field0 = {0};}
		"__extension__"?
		specifier_qualifier_list(&field0)
		(                                                  {c_dcl field = field0;}
			struct_declarator(t, &field)
			(	","                                        {field = field0;}
				attributes(&field)?
				struct_declarator(t, &field)
			)*
		)
	|	static_assert_declaration
;

struct_declarator(c_type *t, c_dcl *field):                {c_value v = {0};}
                                                           {c_name name;}
	(	declarator(field, &name, 0) attributes(field)?
		(":" constant_expression(&v) attributes(field)?)?  {c_declare_struct_field(t, name, field, &v);}
	|   (":" constant_expression(&v) attributes(field)?)?  {c_declare_struct_field(t, 0, field, &v);}
	)
;

enum_specifier(c_dcl *d):                                  {c_name name;}
	"enum"                                                 {d->flags |= C_TYPE_SPEC_ENUM;}
	attributes(d)?
	(	ID(&name)
		(	/* empty */                                    {c_resolve_tag(name, d, 0);}
		|                                                  {c_type *t = c_resolve_tag(name, d, 1);}
			enum_contents(t, d)
		)
	|                                                      {c_type *t = c_make_enum_type(d, 0);}
		enum_contents(t, d)
	)
;

enum_contents(c_type *t, c_dcl *d):                        {int64_t min = 0;}
                                                           {uint64_t max = 0;}
                                                           {c_value last;}
                                                           {last.u.type = IR_I64; last.u.val.i64 = -1;}
	"{"
	enumerator(t, &min, &max, &last)
	(	","
		(	&"}"                                           {break; /* manual conflict resolution */ }
			"}"
		|	enumerator(t, &min, &max, &last)
		)
	)*
	"}"
	attributes(d)?+                                        {c_finish_enum_type(t, d, min, max);}
;

enumerator(const c_type *t, int64_t *min, uint64_t *max, c_value *last):
                                                           {c_value v = {0};}
                                                           {c_dcl attr = {0};}
                                                           {c_name name;}
	ID(&name) attributes(&attr)?
	("=" constant_expression(&v))?                         {c_declare_enum_val(t, name, &attr, &v, min, max, last);}
;

declarator(c_dcl *d, c_name *name, bool allow_old_func):   {c_dcl d2 = {0};}
	(	"*"                                                {c_make_pointer_type(d);}
		type_qualifier(d)*
	)*
	(	ID(name)
		arrays_and_params(d, allow_old_func, 0)?
	|	"("                                                {d2.flags = C_TYPE_SPEC_CHAR;}
	    attributes(&d2)?
		declarator(&d2, name, 0)
		")"
		arrays_and_params(d, allow_old_func, 0)?           {c_make_nested_type(d, &d2);}
	)
;

abstract_declarator(c_dcl *d):                             {c_dcl d2 = {0};}
	(	"*"                                                {c_make_pointer_type(d);}
		type_qualifier(d)*
	)*
	(	?{is_nested_declarator(sym)}                       {d2.flags = C_TYPE_SPEC_CHAR;}
	    "("
		attributes(&d2)?
		abstract_declarator(&d2)
		")"
		arrays_and_params(d, 0, 0)?                        {c_make_nested_type(d, &d2);}
	|	arrays_and_params(d, 0, 0)?
	)
;

parameter_declarator(c_dcl *d, c_name *name):              {c_dcl d2 = {0};}
	(	"*"                                                {c_make_pointer_type(d);}
		type_qualifier(d)*
	)*
	(	ID(name)
		arrays_and_params(d, 0, 1)?
	|	?{is_nested_declarator(sym)}                       {d2.flags = C_TYPE_SPEC_CHAR;}
		"("
		attributes(&d2)?
		parameter_declarator(&d2, name)
		")"
		arrays_and_params(d, 0, 1)?                        {c_make_nested_type(d, &d2);}
	|   arrays_and_params(d, 0, 1)?                        {*name = 0;}
	)
;

arrays_and_params(c_dcl *d, bool allow_old_func, bool is_param):
	(	parameters(d, allow_old_func)
	|	array_declarator(d, is_param)
	)
;

array_declarator(c_dcl *d, bool is_param):                 {c_value len = {0};}
                                                           {c_dcl dim = {0};}
                                                           {uint64_t attr = 0;}
	"["
	(	/* empty */                                        {attr |= C_ATTR_FLEXIBLE;}
	|	&"*" "*"                                           {if (!is_param) yy_error("[*] not allowed in other than function prototype scope");}
	                                                       {attr |= C_ATTR_VLA;}
	|	assignment_expression(&len)
	|	type_qualifier_list(&dim)                          {if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");}
		(	/* empty */                                    {attr |= C_ATTR_FLEXIBLE;}
		|	&"*" "*"                                       {if (!is_param) yy_error("[*] not allowed in other than function prototype scope");}
		                                                   {attr |= C_ATTR_VLA;}
		|	"static"?
			assignment_expression(&len)
		)
	|	"static"                                           {if (!is_param) yy_error("static or type qualifiers in non-parameter array declarator");}
		type_qualifier_list(&dim)?
		assignment_expression(&len)
	)
	"]"
	arrays_and_params(d, 0, is_param)?                     {c_make_array_type(d, &dim, &len, attr);}
;

parameters(c_dcl *d, bool allow_old_func):                 {bool is_variadic = 0;}
                                                           {int32_t num_params = 0;}
                                                           {c_param *params = alloca(sizeof(c_param) * C_ALLOCA_PARAMS);}
	"("
	(	?{allow_old_func && !is_typedef_name(sym)}
		identifier_list(&params, &num_params)
	|	parameter_declaration(&params, &num_params)
		(	","
			(	parameter_declaration(&params, &num_params)
			|	"..."                                      {is_variadic = 1;}
                                                           {break; /* manual conflict resolution */}
			)
		)*
	|	"..."                                              {is_variadic = 1;}
	|	/* empty */
	)
	")"
	arrays_and_params(d, 0, 0)?                            {c_make_func_type(d, params, num_params, is_variadic);}
;

parameter_declaration(c_param **params, int32_t *num_params):
                                                           {c_dcl p = {0};}
                                                           {c_name name;}
	declaration_specifiers(&p)
	parameter_declarator(&p, &name)
	attributes(&p)?                                        {c_declare_func_param(params, num_params, name, &p);}
;

identifier_list(c_param **params, int32_t *num_params):    {c_name name;}
	ID(&name)                                              {c_declare_func_param_name(params, num_params, name);}
	(	","
		ID(&name)                                          {c_declare_func_param_name(params, num_params, name);}
	)*
;

type_name(const c_type **t):                               {c_dcl d = {0};}
	specifier_qualifier_list(&d) abstract_declarator(&d)   {*t = c_resolve_type(&d);}
;

initializer(c_sym *obj):                                   {c_value v = {0};}
		assignment_expression(&v)                          {c_do_init_obj(obj, &v);}
	|	                                                   {size_t size = 0;}
		initializer_contents(obj, obj->value.type, 0, &size)
		                                                   {c_do_init_end(obj, size);}
;

nested_initializer(c_sym *obj, c_init *init, bool b, size_t *size):
                                                           {c_value v = {0};}
		assignment_expression(&v)                          {c_do_init_set(obj, init, &v, size);}
	|                                                      {size_t offset;}
	                                                       {const c_type *type = c_do_init_nested(obj, init, b, &offset);}
		initializer_contents(obj, type, offset, size)
;

initializer_contents(c_sym *obj, const c_type *t, size_t offset, size_t *size):
	"{"
	(                                                      {c_init init;}
		                                                   {c_do_init_first(obj, &init, t, offset);}
		(	nested_initializer(obj, &init, 0, size)
		|	designated_initializer(obj, &init, size)
		)
		(	","
			(	&"}"                                       {break; /* manual conflict resolution */}
				"}"
			|                                              {c_do_init_next(obj, &init);}
				nested_initializer(obj, &init, 0, size)
			|	                                           {c_do_init_first(obj, &init, t, offset);}
				designated_initializer(obj, &init, size)
			)
		)*
	)?
	"}"
;

designated_initializer(c_sym *obj, c_init *init, size_t *size):
	(                                                     {c_value v;}
		"[" constant_expression(&v) "]"                   {c_do_init_dim(obj, init, &v);}
	|	                                                  {c_name name;}
		"." ID(&name)                                     {c_do_init_field(obj, init, name);}
	)+
	"="
	nested_initializer(obj, init, 1, size)
;

static_assert_declaration:                                 {c_value cond = {0}, msg = {0};}
	"_Static_assert" "("
	constant_expression(&cond)
	("," strings(&msg))?
	")"                                                    {c_static_assert(&cond, &msg);}
;

/* Statements */
compound_statement(c_value *val):
	(                                                      {c_name name;}
		"__label__" ID(&name)                              {c_declare_local_label(name);}
			(
			","
			ID(&name)                                      {c_declare_local_label(name);}
		)* ";"
	)?
	(	?{!C_IS_ID(sym) || !is_typedef_name(sym) || is_label(sym)}
		statement(val)
	|	declaration
	)*
;

statement(c_value *last_val):                              {c_value val = {0};}
                                                           {c_name name;}
                                                           {if (last_val) c_value_clear(last_val);}
	(	?{!C_IS_ID(sym) || is_label(sym)}                  {c_label *label;}
		(	ID(&name)                                      {label = c_do_set_label(name);}
			":"
			(                                              {c_dcl attrs = {0};}
				attributes(&attrs)                         {c_do_set_label_attrs(label, &attrs);}
			)?
		|                                                  {c_value val1;}
			"case" constant_expression(&val1)
			(                                              {c_value val2;}
				"..." constant_expression(&val2)           {c_do_case_range(&val1, &val2);}
			|	/*empty*/                                  {c_do_case(&val1);}
			) ":"
		|	"default" ":"                                  {c_do_case_default();}
		)
	)*
	(	                                                   {c_scope scope;}
	    "{"                                                {c_push_scope(&scope);}
		compound_statement(NULL)                           {c_pop_scope(&scope);}
        "}"
	|                                                      {ir_ref check;}
	                                                       {bool orig_dead_code = c_dead_code;}
		"if" "("expression(&val) ")"                       {check = c_do_if(&val);}
		statement(NULL)
		(   "else"                                         {c_do_if_else(check, orig_dead_code);}
			statement(NULL)
		)?+                                                {c_do_if_end(check, orig_dead_code);}
	|                                                      {c_loop loop;}
		"switch" "(" expression(&val) ")"                  {c_do_switch(&loop, &val);}
		statement(NULL)                                    {c_do_switch_end(&loop);}
	|                                                      {c_loop loop;}
		"while"                                            {c_do_loop_start(&loop);}
		"(" expression(&val) ")"                           {c_do_loop_check(&loop, &val);}
		statement(NULL)                                    {c_do_loop_end(&loop);}
	|                                                      {c_loop loop;}
		"do"                                               {c_do_loop_start(&loop);}
		statement(NULL)                                    {c_do_loop_continue_label(&loop);}
		"while" "(" expression(&val) ")"                   {c_do_loop_check(&loop, &val);}
		";"                                                {c_do_loop_end(&loop);}
	|                                                      {c_loop loop;}
		"for" "("
		(	?{!C_IS_ID(sym) || !is_typedef_name(sym)}
			expression(&val)? ";"                          {c_do_loop_start(&loop);}
			(	expression(&val)                           {c_do_loop_check(&loop, &val);}
			)?
			";"
			(                                              {c_do_for_next_start(&loop);}
				expression(&val)                           {c_do_for_next_end(&loop);}
			)?
			")"
			statement(NULL)                                {c_do_for_end(&loop);}
		|		                                           {c_scope scope;}
		                                                   {c_push_scope(&scope);}
			declaration                                    {c_do_loop_start(&loop);/*TODO: verify storage spec???*/}
			(	expression(&val)                           {c_do_loop_check(&loop, &val);}
			)?
			";"
			(                                              {c_do_for_next_start(&loop);}
				expression(&val)                           {c_do_for_next_end(&loop);}
			)?
			")"
			statement(NULL)                                {c_do_for_end(&loop);}
			                                               {c_pop_scope(&scope);}
		)
	|	"goto"
		(	ID(&name)                                      {c_do_goto(name);}
		|	"*" expression(&val)                           {c_do_computed_goto(&val);}
		)
		";"
	|	"continue" ";"                                     {c_do_continue();}
	|	"break" ";"                                        {c_do_break();}
	|	"return" expression(&val)? ";"                     {c_do_return(&val);}
	|	expression(last_val ? last_val : &val)?
		";"
//???	|	attributes ";"
	|	"__asm__"                                          {/*???*/yy_error("asm support not implemented yet");}
		("volatile"|"inline"|"goto")*
		"("
		asm_argument
		")" ";"
	)
;

asm_argument:
	STRING
	(":" asm_operands?
		(":" asm_operands?
			(":" asm_clobbers?
				(":" asm_goto_operands)?
			)?
		)?
	)?
;

asm_operands:
	asm_operand ("," asm_operand)*
;

asm_operand:                                               {c_name name;}
                                                           {c_value v = {0};}
	(	STRING "(" expression(&v) ")"
	|	"[" ID(&name) "]" STRING "(" expression(&v) ")"
	)
;

asm_clobbers:
	STRING ("," STRING)
;

asm_goto_operands:                                         {c_name name;}
	ID(&name) ("," ID(&name))
;

/* Expressions */
strings(c_value *val):                                     {str_list list;}
	                                                       {list.str = yy_text;}
														   {list.len = yy_len;}
														   {list.next = NULL;}
	STRING strings_tail(val, &list, &list)
;

strings_tail(c_value *val, str_list *first, str_list *last):
	(	                                                   {str_list list;}
	                                                       {list.str = yy_text;}
														   {list.len = yy_len;}
														   {list.next = NULL;}
														   {last->next = &list;}
		STRING strings_tail(val, first, &list)
	|   /* *empty */                                       {yy_strings(val, first, last);}
	)
;

actual_parameters(c_value *func):                          {int32_t num_args = 0;}
	                                                       {c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);}
	(   assignment_expression(&args[num_args])             {num_args++;}
		(	","                                            {IR_ASSERT(num_args < C_ALLOCA_PARAMS);}
			assignment_expression(&args[num_args])         {num_args++;}
		)*
	)?                                                     {c_do_call(func, num_args, args);}
;

builtin_parameters(c_value *val, c_name name):             {int32_t num_args = 0;}
	                                                       {c_value *args = alloca(sizeof(c_value) * C_ALLOCA_PARAMS);}
	(   assignment_expression(&args[num_args])             {num_args++;}
		(	","                                            {IR_ASSERT(num_args < C_ALLOCA_PARAMS);}
			assignment_expression(&args[num_args])         {num_args++;}
		)*
	)?                                                     {c_do_builtin(val, name, num_args, args);}
;

generic_association:                                       {const c_type *t;}
                                                           {c_value v;}
	(	type_name(&t) ":" assignment_expression(&v)
	|	"default" ":" assignment_expression(&v)
	)
;

unary_expression(c_value *val):
                                                           {c_name name;}
                                                           {const c_type *t;}
                                                           {c_value v = {0};}
   (
		"("
		(	?{!C_IS_ID(sym) || is_typedef_name(sym)}
			type_name(&t) ")"
			(                                              {c_sym obj;}
                                                           {size_t size = 0;}
                                                           {c_do_init_expr_start(&obj, t);}
				initializer_contents(&obj, t, 0, &size)    {c_do_init_expr_end(val, &obj, size);}
			|	unary_expression(val)                      {c_do_cast(t, val);}
			)
		|   expression(val) ")"
		|                                                  {c_scope scope;}
		    "{"                                            {c_push_scope(&scope);}
			compound_statement(val)                        {c_pop_scope(&scope);}
			"}" ")"
		)
	|	ID(&name)                                          {c_resolve_sym_name(val, name, sym);}
	|	DECIMAL_NUMBER(val)
	|	OCTAL_NUMBER(val)
	|	HEXADECIMAL_NUMBER(val)
	|	FLOATING_NUMBER(val)
	|	HEXADECIMAL_FLOATING_NUMBER(val)
	|	CHARACTER(val)
	|	strings(val)
	|	"_Generic"                                         {/*???*/yy_error("_Generic not implemented yet");}
		"("
		assignment_expression(val)
		("," generic_association)+
		")"
	|	"__extension__" unary_expression(val)
	|                                                      {yy_sym op = sym;}
		("++"|"--") unary_expression(val)                  {c_do_pre_op(op, val);}
	|	(	"&" unary_expression(val)                      {c_do_addr(val);}
		|	"*" unary_expression(val)                      {c_do_deref(val);}
		|	"+" unary_expression(val)                      {c_do_unary_plus(val);}
		|	"-" unary_expression(val)                      {c_do_neg(val);}
		|	"~" unary_expression(val)                      {c_do_not(val);}
		|	"!" unary_expression(val)                      {c_do_bool_not(val);}
		)
	|	"sizeof"
		(	&"(" "("
			(	?{!C_IS_ID(sym) || is_typedef_name(sym)}
				type_name(&t)                              {c_sizeof_type(val, t);}
			|	expression(&v)                             {c_sizeof_expr(val, &v);/*???*/}
			)
			")"
		|	unary_expression(&v)                           {c_sizeof_expr(val, &v);}
		)
	|	"_Alignof"
		"(" type_name(&t) ")"                              {c_alignof_type(val, t);}
	|	("__alignof__"|"__alignof")
		(	&"(" "("
			(	?{!C_IS_ID(sym) || is_typedef_name(sym)}
				type_name(&t)                              {c_alignof_type(val, t);}
			|	expression(&v)                             {c_alignof_expr(val, &v);/*???*/}
			)
			")"
		|	unary_expression(&v)                           {c_alignof_expr(val, &v);}
		)
	|	"&&" ID(&name)                                     {c_do_label_value(val, name);}
	|                                                      {name = sym;}
		(	"__builtin_va_start"
		|	"__builtin_va_arg"
		|	"__builtin_va_end"
		|	"__builtin_va_copy"
		)
		"(" builtin_parameters(val, name) ")"
	)
	(                                                      {c_value dim = {0};}
		"[" expression(&dim) "]"                           {c_do_array_dim(val, &dim);}
	|	"(" actual_parameters(val) ")"
	|	"." ID(&name)                                      {c_do_struct_field(val, name);}
	|	"->" ID(&name)                                     {c_do_struct_field_deref(val, name);}
	|	                                                   {yy_sym op = sym;}
		("++"|"--")                                        {c_do_post_op(op, val);}
	)*+
;

multiplicative_expression(c_value *val):
	unary_expression(val)
	(                                                      {int op = sym;}
	                                                       {c_value op2 = {0};}
		("*"|"/"|"%") unary_expression(&op2)               {c_do_binary_op(op, val, &op2);}
	)*
;

additive_expression(c_value *val):
	multiplicative_expression(val)
	(                                                      {int op = sym;}
	                                                       {c_value op2 = {0};}
		("+"|"-") multiplicative_expression(&op2)          {c_do_binary_op(op, val, &op2);}
	)*
;

shift_expression(c_value *val):
	additive_expression(val)
	(                                                      {int op = sym;}
	                                                       {c_value op2 = {0};}
		("<<"|">>") additive_expression(&op2)              {c_do_binary_op(op, val, &op2);}
	)*
;

relational_expression(c_value *val):
	shift_expression(val)
	(                                                      {int op = sym;}
	                                                       {c_value op2 = {0};}
		("<"|">"|"<="|">=") shift_expression(&op2)         {c_do_binary_op(op, val, &op2);}
	)*
;

equality_expression(c_value *val):
	relational_expression(val)
	(                                                      {int op = sym;}
	                                                       {c_value op2 = {0};}
		("=="|"!=") relational_expression(&op2)            {c_do_binary_op(op, val, &op2);}
	)*
;

and_expression(c_value *val):
	equality_expression(val)
	(                                                      {c_value op2 = {0};}
		"&" equality_expression(&op2)                      {c_do_binary_op(YY__AND, val, &op2);}
	)*
;

exclusive_or_expression(c_value *val):
	and_expression(val)
	(                                                      {c_value op2 = {0};}
		"^" and_expression(&op2)                           {c_do_binary_op(YY__UPARROW, val, &op2);}
	)*
;

inclusive_or_expression(c_value *val):
	exclusive_or_expression(val)
	(                                                      {c_value op2 = {0};}
		"|" exclusive_or_expression(&op2)                  {c_do_binary_op(YY__BAR, val, &op2);}
	)*
;

logical_and_expression(c_value *val):
	inclusive_or_expression(val)
	(                                                      {bool orig_dead_code = c_dead_code;}
		(                                                  {c_value op2 = {0};}
	                                                       {ir_ref if_ref = c_do_bool_and_start(val);}
			"&&" inclusive_or_expression(&op2)             {c_do_bool_and_end(val, &op2, if_ref);}
		)+                                                 {c_dead_code = orig_dead_code;}
	)?
;

logical_or_expression(c_value *val):
	logical_and_expression(val)
	(                                                      {bool orig_dead_code = c_dead_code;}
		(                                                  {c_value op2 = {0};}
	                                                       {ir_ref if_ref = c_do_bool_or_start(val);}
			"||" logical_and_expression(&op2)              {c_do_bool_or_end(val, &op2, if_ref);}
		)+                                                 {c_dead_code = orig_dead_code;}
	)?
;

conditional_expression(c_value *val):
	logical_or_expression(val)
	(                                                      {ir_ref check;}
	                                                       {bool orig_dead_code = c_dead_code;}
                                                           {c_value op2 = {0}, op3;}
		"?"                                                {check = c_do_if(val);}
		(	expression(&op2)                               {c_value_rval(&op2);}
		)?
		":"                                                {c_do_if_else(check, orig_dead_code);}
		conditional_expression(&op3)                       {c_value_rval(&op3);}
		                                                   {c_do_if_end(check, orig_dead_code);}
		                                                   {c_do_cond_op(val, &op2, &op3);}
	)?
;

assignment_expression(c_value *val):
	(
		conditional_expression(val)
		(                                                  {int op = sym;}
			                                               {c_value op2;}
			("="|"*="|"/="|"%="|"+="|"-="|"<<="|">>="|
			 "&="|"^="|"|=")
			assignment_expression(&op2)                    {c_do_assign_op(op, val, &op2);}
		)?
	)
;

expression(c_value *val):
	assignment_expression(val)
	("," assignment_expression(val))*
;

constant_expression(c_value *val):
	conditional_expression(val)
;

/* Lexical Grammar */
ID(c_name *name):
	/[A-Za-z_][A-Za-z_0-9]*/                               {*name = sym;}
//	/([A-Za-z_]|\\u[0-9A-Fa-f]+|\\U[0-9A-Fa-f]+)([A-Za-z_0-9]|\\u[0-9A-Fa-f]+|\\U[0-9A-Fa-f]+)*/
;

DECIMAL_NUMBER(c_value *val):
	/[1-9][0-9]*([Uu](L|l|LL|ll)?|[Ll][Uu]?|(LL|ll)[Uu])?/
	{yy_read_dec(val, yy_text, yy_len);}
;

OCTAL_NUMBER(c_value *val):
	/0[0-7]*([Uu](L|l|LL|ll)?|[Ll][Uu]?|(LL|ll)[Uu])?/
	{yy_read_oct(val, yy_text, yy_len);}
;

HEXADECIMAL_NUMBER(c_value *val):
	/0[xX][0-9A-Fa-f][0-9A-Fa-f]+([Uu](L|l|LL|ll)?|[Ll][Uu]?|(LL|ll)[Uu])?/
	{yy_read_hex(val, yy_text + 2, yy_len - 2);}
;

FLOATING_NUMBER(c_value *val):
	/([0-9]*\.[0-9]+([Ee][\+\-]?[0-9]+)?|[0-9]+\.([Ee][\+\-]?[0-9]+)?|[0-9]+[Ee][\+\-]?[0-9]+)[flFL]?/
	{yy_read_fp(val, yy_text, yy_len);}
;

HEXADECIMAL_FLOATING_NUMBER(c_value *val):
	/0[xX][0-9A-Fa-f]*(\.[0-9A-Fa-f]*)?[Pp][\+\-]?[0-9]+[flFL]?/
	{yy_read_fp(val, yy_text, yy_len);}
;

CHARACTER(c_value *val):
	/[LuU]?'([^'\\\n]|\\.)*'/
//	/[LuU]?'([^'\\\n]|\\[0-7]+|\\0[xX][0-9A-Fa-f]+|\\u[0-9A-Fa-f]+|\\U[0-9A-Fa-f]+|\\.)*'/
	{yy_read_char(val, yy_text, yy_len);}
;

STRING:
	/(u8|u|U|L)?"([^"\\\n]|\\.)*"/
//	/(u8|u|U|L)?'([^"\\\n]|\\[0-7]+|\\0[xX][0-9A-Fa-f]+|\\u[0-9A-Fa-f]+|\\U[0-9A-Fa-f]+|\\.)*'/
;

EOL: /\r\n|\r|\n/;
WS: /[ \t\f\v]+/;
ONE_LINE_COMMENT: /\/\/[^\r\n]*(\r\n|\r|\n)/;
COMMENT: /\/\*([^\*]|\*+[^\*\/])*\*+\//;
CPP_DIRECTIVE:
	/* use regexp instead of "#" to check leading spaces */
	/#\/\/[^\r\n]*(\r\n|\r|\n)/
;

IGNORE: WS | ONE_LINE_COMMENT | COMMENT | CPP_DIRECTIVE | EOL;

%%
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
				*ctype = &c_type_u64;
				*type = (ir_type)YY_UNSIGNED_LONG_LONG;
			} else {
				*ctype = &c_type_u64;
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
			*ctype = &c_type_u64;
			*type = (ir_type)YY_UNSIGNED_LONG;
		} else if (ch == 'l' || ch == 'L') {
			e--;
			ch = *(e - 1);
			if (ch == 'u' || ch == 'U') {
				e--;
				*ctype = &c_type_u64;
				*type = (ir_type)YY_UNSIGNED_LONG_LONG;
			} else {
				*ctype = &c_type_i64;
				*type = (ir_type)YY_SIGNED_LONG_LONG;
			}
		} else {
			*ctype = &c_type_i64;
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

static void yy_read_char(c_value *res, const char *p, size_t len)
{
	ir_val val;
	char ch = *p++;

	if (ch == 'L' || ch == 'u' || ch == 'U') {
//???		IR_ASSERT(0 && "NIY");
		ch = *p++;
	}

	IR_ASSERT(ch == '\'');

	ch = *p++;
	if (ch == '\\') {
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
			case 'u': // TODO \uXXXX ???
			case 'U': // TODO \UXXXXXXXX ???
			default:
				yy_error("unsupported escape sequence");
				break;
		}
	}
	IR_ASSERT(*p == '\'');
	val.u64 = ch;
	c_value_set_const(res, &c_type_char, IR_CHAR, val);
}

static void yy_strings_append(yy_dyn_str *dyn_str, const char *str, size_t len)
{
	const char *s, *p = str;
	char ch;

	ch = *p++;
	if (ch == 'L' || ch == 'U') {
		ch = *p++;
//???		IR_ASSERT(0 && "NIY");
	} else if (ch == 'u') {
		ch = *p++;
		if (ch == '8') {
			ch = *p++;
		}
//???		IR_ASSERT(0 && "NIY");
	}

	IR_ASSERT(ch == '"');

	s = p;
	ch = *p++;
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
				case 'u': // TODO \uXXXX ???
				case 'U': // TODO \UXXXXXXXX ???
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
	IR_ASSERT(p == str + len);
}

static void yy_strings(c_value *res, str_list *first, str_list *last)
{
	yy_dyn_str dyn_str;
	ir_val val;

	yy_dyn_str_init(&dyn_str, "", 0);
	do {
		yy_strings_append(&dyn_str, first->str, first->len);
		first = first->next;
	} while(first);
	yy_dyn_str_append0(&dyn_str, "", 0);

	val.ptr = dyn_str.str;
	c_value_set_const(res, &c_type_string, IR_ADDR, val);
	res->u.ref = dyn_str.len; // find a better place for str len ???
}

/* CPP helper */
bool parse_pp_expr(void)
{
	bool ret;
	c_value res;
	bool old_dead_code = c_dead_code;
	ir_ctx *old_ctx = active_ctx;
	yy_sym sym;

	active_ctx = global_ctx;
	yy_flags |= PP_EVAL_EXPRESSION;
	c_dead_code = 0;

	sym = get_sym();
	sym = parse_constant_expression(sym, &res);
	IR_ASSERT(sym == YY_EOF);

	ret = c_value_is_true(&res);

	c_dead_code = old_dead_code;
	yy_flags &= ~PP_EVAL_EXPRESSION;	
	active_ctx = old_ctx;

	return ret;
}

void rcc_parse(void)
{
	parse();
}
