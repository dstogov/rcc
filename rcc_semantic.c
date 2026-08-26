/*
 * RCC - Rational C Compiler
 * (Semantic analysis and intermediate code (IR) generation)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <ir.h>

#if defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
# include "ir_x86.h"
#elif defined(IR_TARGET_AARCH64)
# include "ir_aarch64.h"
#else
# error "Unknown IR target"
#endif

#include <ir_private.h>
#include <ir_builder.h>

#include <math.h>

#include "rcc.h"

#undef _ir_CTX
#define _ir_CTX rcc->active_ctx

const c_type c_type_void                = {.kind = C_TYPE_VOID,   .flags = C_TYPE_GLOBAL, .size = 0,  .attr = 1};
const c_type c_type_bool                = {.kind = C_TYPE_BOOL,   .flags = C_TYPE_GLOBAL, .size = 1,  .attr = 1};
const c_type c_type_char                = {.kind = C_TYPE_CHAR,   .flags = C_TYPE_GLOBAL, .size = 1,  .attr = 1};
const c_type c_type_u8                  = {.kind = C_TYPE_U8,     .flags = C_TYPE_GLOBAL, .size = 1,  .attr = 1};
const c_type c_type_i8                  = {.kind = C_TYPE_I8,     .flags = C_TYPE_GLOBAL, .size = 1,  .attr = 1};
const c_type c_type_u16                 = {.kind = C_TYPE_U16,    .flags = C_TYPE_GLOBAL, .size = 2,  .attr = 2};
const c_type c_type_i16                 = {.kind = C_TYPE_I16,    .flags = C_TYPE_GLOBAL, .size = 2,  .attr = 2};
const c_type c_type_u32                 = {.kind = C_TYPE_U32,    .flags = C_TYPE_GLOBAL, .size = 4,  .attr = 3};
const c_type c_type_i32                 = {.kind = C_TYPE_I32,    .flags = C_TYPE_GLOBAL, .size = 4,  .attr = 3};
const c_type c_type_ul                  = {.kind = C_TYPE_UL,     .flags = C_TYPE_GLOBAL, .size = C_LONG_SIZE,  .attr = C_LONG_ALIGN};
const c_type c_type_il                  = {.kind = C_TYPE_IL,     .flags = C_TYPE_GLOBAL, .size = C_LONG_SIZE,  .attr = C_LONG_ALIGN};
#if defined(IR_64) || (defined(IR_TARGET_X86) && defined(_WIN32))
const c_type c_type_ull                 = {.kind = C_TYPE_ULL,    .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 4};
const c_type c_type_ill                 = {.kind = C_TYPE_ILL,    .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 4};
#elif defined(IR_TARGET_X86)
/* System V ABI for i386 specifies 4-byte alignment for 64-bit integers */
const c_type c_type_ull                 = {.kind = C_TYPE_ULL,    .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 3};
const c_type c_type_ill                 = {.kind = C_TYPE_ILL,    .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 3};
#else
# error "unknown targer"
#endif
const c_type c_type_float               = {.kind = C_TYPE_FLOAT,  .flags = C_TYPE_GLOBAL, .size = 4,  .attr = 3};
const c_type c_type_double              = {.kind = C_TYPE_DOUBLE, .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 4};
// TODO: long double support ???
const c_type c_type_long_double         = {.kind = C_TYPE_DOUBLE,              .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 4};
const c_type c_type_float_complex       = {.kind = C_TYPE_FLOAT_COMPLEX,       .flags = C_TYPE_GLOBAL, .size = 8,  .attr = 3};
const c_type c_type_double_complex      = {.kind = C_TYPE_DOUBLE_COMPLEX,      .flags = C_TYPE_GLOBAL, .size = 16, .attr = 5};
const c_type c_type_long_double_complex = {.kind = C_TYPE_LONG_DOUBLE_COMPLEX, .flags = C_TYPE_GLOBAL, .size = 32, .attr = 5};

const c_type c_type_string = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 1 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_char,
	.array.length = 0
};

const c_type c_type_lstring = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_wchar_t,
	.array.length = 0
};

const c_type c_type_string_u16 = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 2 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_u16,
	.array.length = 0
};

const c_type c_type_string_u32 = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_u32,
	.array.length = 0
};

const c_type c_type_ptr = {
	.kind = C_TYPE_POINTER,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3,
	.pointer.type = &c_type_void,
};

const c_type c_type_const_void = {
	.kind = C_TYPE_VOID,
	.flags = C_TYPE_GLOBAL,
	.size = 0,
	.attr = 1 | C_ATTR_CONST
};

const c_type c_type_const_ptr = {
	.kind = C_TYPE_POINTER,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3,
	.pointer.type = &c_type_const_void,
};

const c_type c_type_const_char          = {.kind = C_TYPE_CHAR,   .flags = C_TYPE_GLOBAL, .size = 1,  .attr = 1 | C_ATTR_CONST};
const c_type c_type_const_u16           = {.kind = C_TYPE_U16,    .flags = C_TYPE_GLOBAL, .size = 2,  .attr = 2 | C_ATTR_CONST};
const c_type c_type_const_u32           = {.kind = C_TYPE_U32,    .flags = C_TYPE_GLOBAL, .size = 4,  .attr = 3 | C_ATTR_CONST};
const c_type c_type_const_wchar_t       = {.kind = C_TYPE_WCHAR_T,.flags = C_TYPE_GLOBAL, .size = C_WCHAR_SIZE,  .attr = C_WCHAR_ALIGN | C_ATTR_CONST};

const c_type c_type_const_string = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 1 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_const_char,
	.array.length = 0
};

const c_type c_type_const_lstring = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_const_wchar_t,
	.array.length = 0
};

const c_type c_type_const_string_u16 = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 2 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_const_u16,
	.array.length = 0
};

const c_type c_type_const_string_u32 = {
	.kind = C_TYPE_ARRAY,
	.flags = C_TYPE_GLOBAL,
	.size = sizeof(void*),
	.attr = 3 | C_ATTR_FLEXIBLE,
	.array.type = &c_type_const_u32,
	.array.length = 0
};

static ir_ref c_value_ref(rcc_ctx *rcc, c_value *val);
static void c_do_cvt(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v);
static void ir_memzero(rcc_ctx *rcc, ir_ref dst, ir_ref size, uint32_t align);

static bool c_valid_alignment(rcc_ctx *rcc, c_value *val, const char *prefix, c_name sym)
{
	if (!c_value_is_const(val) || !C_IS_TYPE_INT(val->type)) {
		yy_error_fmt("%s\"%s\" value must be an integer constant", prefix, yy_sym2str(rcc, sym));
		return 0;
	} else if (!((C_IS_TYPE_UNSIGNED(val->type) || val->u.val.i64 >= 0)
			&& val->u.val.u64 != 0
			&& (val->u.val.u64 & (val->u.val.u64 - 1)) == 0)) {
		yy_error_fmt("%s\"%s\" value must be a power of two", prefix, yy_sym2str(rcc, sym));
		return 0;
	} else if (val->u.val.u64 > 0x10000000) {
		yy_error_fmt("%s\"%s\" value is too big", prefix, yy_sym2str(rcc, sym));
		return 0;
	}
	return 1;
}

ir_type c_type2ir(rcc_ctx *rcc, const c_type *t)
{
	c_type_kind kind = t->kind;

repeat:
	switch (kind) {
		case C_TYPE_BOOL:    return IR_BOOL;
		case C_TYPE_I8:      return IR_I8;
		case C_TYPE_I16:     return IR_I16;
		case C_TYPE_I32:     return IR_I32;
		case C_TYPE_IL:      return IR_LONG;
		case C_TYPE_ILL:     return IR_I64;
		case C_TYPE_U8:      return IR_U8;
		case C_TYPE_U16:     return IR_U16;
		case C_TYPE_U32:     return IR_U32;
		case C_TYPE_UL:      return IR_ULONG;
		case C_TYPE_ULL:     return IR_U64;
		case C_TYPE_FLOAT:   return IR_FLOAT;
		case C_TYPE_DOUBLE:  return IR_DOUBLE;
		case C_TYPE_POINTER: return IR_ADDR;
		case C_TYPE_ARRAY:   return IR_ADDR;
		case C_TYPE_CHAR:    return IR_CHAR;
		case C_TYPE_VOID:    return IR_VOID;
		case C_TYPE_ENUM:    kind = t->enumeration.kind; goto repeat;
		case C_TYPE_FUNC:    return IR_ADDR;
		case C_TYPE_STRUCT:  return IR_ADDR;
		case C_TYPE_UNION:   return IR_ADDR;
#if IR_SIMD
		case C_TYPE_VECTOR:
			switch (t->vec.type->kind) {
				case C_TYPE_I8:      return IR_MAKE_VECTOR_TYPE(IR_I8,     t->vec.length);
				case C_TYPE_I16:     return IR_MAKE_VECTOR_TYPE(IR_I16,    t->vec.length);
				case C_TYPE_I32:     return IR_MAKE_VECTOR_TYPE(IR_I32,    t->vec.length);
				case C_TYPE_IL:      return IR_MAKE_VECTOR_TYPE(IR_LONG,   t->vec.length);
				case C_TYPE_ILL:     return IR_MAKE_VECTOR_TYPE(IR_I64,    t->vec.length);
				case C_TYPE_U8:      return IR_MAKE_VECTOR_TYPE(IR_U8,     t->vec.length);
				case C_TYPE_U16:     return IR_MAKE_VECTOR_TYPE(IR_U16,    t->vec.length);
				case C_TYPE_U32:     return IR_MAKE_VECTOR_TYPE(IR_U32,    t->vec.length);
				case C_TYPE_UL:      return IR_MAKE_VECTOR_TYPE(IR_ULONG,  t->vec.length);
				case C_TYPE_ULL:     return IR_MAKE_VECTOR_TYPE(IR_U64,    t->vec.length);
				case C_TYPE_FLOAT:   return IR_MAKE_VECTOR_TYPE(IR_FLOAT,  t->vec.length);
				case C_TYPE_DOUBLE:  return IR_MAKE_VECTOR_TYPE(IR_DOUBLE, t->vec.length);
				case C_TYPE_CHAR:    return IR_MAKE_VECTOR_TYPE(IR_I8,     t->vec.length);
				default:
					break;
			}
			break;
#endif
		case C_TYPE_FLOAT_COMPLEX:
		case C_TYPE_DOUBLE_COMPLEX:
		case C_TYPE_LONG_DOUBLE_COMPLEX:
			yy_error("complex numbers are not supported yet"); //???
		default:
			break;
	}
	IR_ASSERT(0);
	return IR_VOID;
}

#define MAX_ABI_TYPES 2

static int c_abi_lower_struct(const c_type *t, ir_type *types)
{
#ifdef IR_TARGET_X86
	return 0; /* always pass structires on stack */
#elif defined(IR_TARGET_X64) && defined(_WIN64)
	if (t->size == 1) {
		types[0] = IR_U8;
		return 1;
	} else if (t->size == 2) {
		types[0] = IR_U16;
		return 1;
	} else if (t->size == 4) {
		types[0] = IR_U32;
		return 1;
	} else if (t->size == 8) {
		types[0] = IR_U64;
		return 1;
	}
	/* pass copy of value thorough hidden pointer */
	return 0;
#else
	// TODO: Full support for different ABIs is not implemented yet ???
	if (t->size <= sizeof(void*)) {
		if (t->size == 1) {
			types[0] = IR_U8;
		} else if (t->size == 2) {
			types[0] = IR_U16;
		} else if (t->size <= 4) {
			types[0] = IR_U32;
		} else {
			types[0] = IR_U64;
		}
		return 1; /* return number of registers */
	}
	/* pass value on stack */
	return 0;
#endif
}

static int c_abi_lower_struct_arg(const c_type *t, ir_type *types)
{
	return c_abi_lower_struct(t, types);
}

static int c_abi_lower_struct_ret(const c_type *t, ir_type *types)
{
#ifdef IR_TARGET_X86
	/* short struct may be returned in register(s) (See GCC -freg-struct-return) */
	if (t->size <= sizeof(void*) && 0) {
		types[0] = IR_U32;
		return 1; /* return number of registers */
	}
#endif
	return c_abi_lower_struct(t, types);
}

static uint32_t c_type_call_conv(rcc_ctx *rcc, const c_type *t)
{
	uint32_t flags = IR_CC_DEFAULT;

	if (t->attr & C_ATTR_CALL_CONV) {
		switch (t->attr & C_ATTR_CALL_CONV) {
			case C_ATTR_CC_CDECL:
				break;
			case C_ATTR_CC_FASTCALL:
				flags = IR_CC_FASTCALL;
				break;
			case C_ATTR_CC_PRESERVE_NONE:
				flags = IR_CC_PRESERVE_NONE;
				break;
#if defined(IR_TARGET_X86)
			case C_ATTR_CC_REGPARM_1:
			case C_ATTR_CC_REGPARM_2:
			case C_ATTR_CC_REGPARM_3:
				// TODO: IR doesn't support "regparm" calling conventions yet ???
#endif
			default:
				yy_error("unsupported calling convention");
		}
	}

	return flags;
}

void c_type2proto_ex(rcc_ctx *rcc, const c_type *t,
                     uint32_t *flags_ptr, ir_type *ret_type_ptr,
                     uint32_t *params_count_ptr, uint8_t *param_types)
{
	uint8_t flags = 0;
	ir_type ret_type;
	uint32_t params_count;
	uint32_t i, j = 0;

	IR_ASSERT(t->kind == C_TYPE_FUNC);
	if (t->func.ret_type->kind == C_TYPE_STRUCT || t->func.ret_type->kind == C_TYPE_UNION) {
		ir_type types[MAX_ABI_TYPES];
		int n = c_abi_lower_struct_ret(t->func.ret_type, types);

		if (n == 1) {
			ret_type = types[0];
		} else {
			IR_ASSERT(n == 0);
			ret_type = IR_ADDR;
			param_types[0] = IR_ADDR;
			j = 1;
		}
	} else {
		ret_type = c_type2ir(rcc, t->func.ret_type);
	}
	if (t->func.num_params > 0) {
		params_count = t->func.num_params + j;
		for (i = 0; i < t->func.num_params; i++) {
			const c_type *param_type = t->func.params[i].type;

			if (param_type->kind == C_TYPE_STRUCT || param_type->kind == C_TYPE_UNION) {
				ir_type types[MAX_ABI_TYPES];
				int n = c_abi_lower_struct_arg(param_type, types);

				if (n == 1) {
					param_types[i + j] = types[0];
				} else {
					IR_ASSERT(n == 0);
					/* pass struct arg on stack */
					param_types[i + j] = IR_ADDR;
				}
			} else {
				param_types[i + j] = c_type2ir(rcc, param_type);
			}
		}
	} else if (j) {
		params_count = 1;
		param_types[0] = IR_ADDR;
	} else {
		params_count = 0;
	}
	if (t->attr & C_ATTR_VARIADIC) {
		flags |= IR_VARARG_FUNC;
	}
	flags |= c_type_call_conv(rcc, t);
	if (t->attr & C_ATTR_CONST_FUNC) {
		flags |= IR_CONST_FUNC;
	} else if (t->attr & C_ATTR_PURE) {
		flags |= IR_PURE_FUNC;
	}

	*flags_ptr = flags;
	*ret_type_ptr = ret_type;
	*params_count_ptr = params_count;
}

static ir_ref c_type2proto(rcc_ctx *rcc, const c_type *t, uint32_t linkage)
{
	uint32_t flags;
	uint32_t params_count;
	uint8_t *param_types;
	ir_type ret_type;

	IR_ASSERT(t->kind == C_TYPE_FUNC);
	param_types = alloca(t->func.num_params + 16);
	c_type2proto_ex(rcc, t, &flags, &ret_type, &params_count, param_types);
	if (linkage == C_LINK_INTERNAL) {
		flags |= IR_STATIC;
	} else if (linkage == C_LINK_BUILTIN) {
		flags |= IR_CC_BUILTIN;
	}
	return ir_proto(rcc->active_ctx, flags, ret_type, params_count, param_types);
}

static bool c_fix_incomplete_type(rcc_ctx *rcc, const c_type *type)
{
	c_tag *tag;

	IR_ASSERT((type->flags & C_TYPE_INCOMPLETE) && type->tag);
	tag = rcc->yy_hash.data[type->tag].tag;
	if (tag && tag->type != type && !(tag->type->flags & C_TYPE_INCOMPLETE)) {
		/* The incomplete type reffers to a complete tag. */
		uint32_t attr = type->attr;
		c_type *t = (c_type*)type;
		*t = *tag->type;
		t->attr |= (attr & C_TYPE_ATTRS);
		return 1;
	}
	return 0;
}

void c_push_scope(rcc_ctx *rcc, c_scope *scope)
{
	scope->list.syms = NULL;
	scope->list.size = 0;
	scope->list.len = 0;
	scope->checkpoint = ir_arena_checkpoint(rcc->c_arena);
	scope->vla_block = IR_UNUSED;
	if (rcc->active_scope) {
		scope->last_vla_block = rcc->active_scope->last_vla_block;
		scope->cleanup_sym = rcc->active_scope->cleanup_sym;
	} else {
		scope->last_vla_block = IR_UNUSED;
		scope->cleanup_sym = NULL;
	}
	scope->prev = rcc->active_scope;
	rcc->active_scope = scope;
}

static void c_do_cleanup_var(rcc_ctx *rcc, c_sym *sym)
{
	c_value dummy, func, param;

	IR_ASSERT(sym->kind == C_SYM_VAR && c_value_is_ref(&sym->value) && sym->cleanup_func);

	c_resolve_sym_name(rcc, &func, sym->cleanup_func, YY__LPAREN);
	param = sym->value;
	c_do_addr(rcc, &param);
	c_do_call(rcc, &func, 1, &param, &dummy);
}

static void c_do_cleanup_vars(rcc_ctx *rcc, c_sym *from, c_sym *to)
{
	while (from != to) {
		c_do_cleanup_var(rcc, from);
		from = from->cleanup_next;
	}
}

void c_pop_scope_light(rcc_ctx *rcc, c_scope *scope)
{
	if (scope->list.syms) {
		yy_sym id, *p = scope->list.syms;
		uint32_t n = scope->list.len;
		void *ptr;
		uintptr_t kind;

		while (n) {
			id = *p++;
			n -= 1 + sizeof(void*)/sizeof(uint32_t);
			p = pp_load_ptr(p, &ptr);
			kind = ((uintptr_t)ptr) & C_POP_MASK;
			ptr = (void*)(((uintptr_t)ptr) & ~C_POP_MASK);
			switch (kind) {
				case C_POP_SYM:
					rcc->yy_hash.data[id].sym = ptr;
					break;
				case C_POP_TAG:
					rcc->yy_hash.data[id].tag = ptr;
					break;
				default:
					IR_ASSERT(0);
			}
		}
		pp_list_release(rcc, scope->list.syms, scope->list.size);
	}
	rcc->active_scope = scope->prev;
}

void c_pop_scope(rcc_ctx *rcc, c_scope *scope)
{
	if (scope->cleanup_sym
	 && rcc->active_ctx->control
	 && (rcc->active_ctx->ir_base[rcc->active_ctx->control].op != IR_BEGIN
	  || rcc->active_ctx->ir_base[rcc->active_ctx->control].op1)) {
		c_do_cleanup_vars(rcc, scope->cleanup_sym,
			rcc->active_scope->prev ? rcc->active_scope->prev->cleanup_sym : NULL);
	}
	if (scope->vla_block
	 && rcc->active_ctx->control
	 && (rcc->active_ctx->ir_base[rcc->active_ctx->control].op != IR_BEGIN
	  || rcc->active_ctx->ir_base[rcc->active_ctx->control].op1)) {
		ir_BLOCK_END(scope->vla_block);
	}
	if (scope->list.syms) {
		yy_sym id, *p = scope->list.syms;
		uint32_t n = scope->list.len;
		void *ptr;
		uintptr_t kind;

		while (n) {
			id = *p++;
			n -= 1 + sizeof(void*)/sizeof(uint32_t);
			p = pp_load_ptr(p, &ptr);
			kind = ((uintptr_t)ptr) & C_POP_MASK;
			ptr = (void*)(((uintptr_t)ptr) & ~C_POP_MASK);
			switch (kind) {
				case C_POP_SYM:
					rcc->yy_hash.data[id].sym = ptr;
					break;
				case C_POP_TAG:
					rcc->yy_hash.data[id].tag = ptr;
					break;
				case C_POP_LABEL:
					c_do_finish_label(rcc, id, rcc->yy_hash.data[id].label);
					rcc->yy_hash.data[id].label = ptr;
					break;
				default:
					IR_ASSERT(0);
			}
		}
		pp_list_release(rcc, scope->list.syms, scope->list.size);
	}
	if (scope->checkpoint) {
		ir_arena_release(&rcc->c_arena, scope->checkpoint);
	}
	rcc->active_scope = scope->prev;
	if (scope == rcc->active_func_scope) {
		rcc->active_func_scope = rcc->active_scope;
	}
}

static void c_leave_scope(rcc_ctx *rcc, c_scope *to)
{
	ir_ref vla_block = IR_UNUSED;
	c_scope *scope = rcc->active_scope;
	c_sym *cleanup_sym = scope->cleanup_sym;

	while (scope != to) {
		IR_ASSERT(scope);
		if (scope->vla_block) vla_block = scope->vla_block;
		scope = scope->prev;
	}
	if (cleanup_sym != scope->cleanup_sym) {
		c_do_cleanup_vars(rcc, cleanup_sym, scope->cleanup_sym);
	}
	if (vla_block) ir_BLOCK_END(vla_block);
}

const c_type *c_resolve_type_name(rcc_ctx *rcc, c_name name)
{
	c_sym *s = rcc->yy_hash.data[name].sym;
	if (!s || s->kind != C_SYM_TYPE) yy_error_fmt("\"%s\" is not a type name", rcc->yy_hash.data[name].str);
	return s->value.type;
}

void c_resolve_sym_name(rcc_ctx *rcc, c_value *res, c_name name, yy_sym sym)
{
	c_sym *s = rcc->yy_hash.data[name].sym;
	if (!s) {
		if (sym == YY__LPAREN) {
			c_dcl dcl;

			if (rcc->yy_flags & PP_EVAL_EXPRESSION) {
				ir_val val;
				if (!rcc->c_dead_code) yy_error_fmt("undefined function macro \"%s\"", rcc->yy_hash.data[name].str);
				val.u64 = 0;
				c_value_set_const(res, &c_type_void, IR_VOID, val);
				return;
			}
			yy_warning_ex_fmt(E_IMPLICIT_FUNC_DCL, "implicit declaration of function \"%s\"", rcc->yy_hash.data[name].str);

			/* Function in going to be declared in the global scope */
			memset(&dcl, 0, sizeof(dcl));
			dcl.flags = C_DCL_EXTERN | C_TYPE_SPEC_TYPE;
			c_type *type = type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
			type->kind = C_TYPE_FUNC;
			type->flags = 0;
			type->attr = C_ATTR_OLD_FUNC;
			type->size = sizeof(void*);
			type->func.ret_type = (name == YY_ALLOCA) ? &c_type_ptr : &c_type_i32;
			type->func.num_params = 0;
			type->func.params = NULL;
			dcl.type = type;
			s = c_declare(rcc, name, &dcl);
		} else {
			yy_error_fmt("undefined identifier \"%s\"", rcc->yy_hash.data[name].str);
		}
	}
	if (s->kind == C_SYM_TYPE) yy_error_fmt("\"%s\" is a type name", rcc->yy_hash.data[name].str);
	if (s->kind == C_SYM_CONST) {
		c_value_set_const(res, s->value.type, c_type2ir(rcc, s->value.type), s->value.u.val);
	} else if (s->kind == C_SYM_VAR || s->kind == C_SYM_FUNC) {
		if (c_value_is_ref(&s->value)) {
			IR_ASSERT(s->kind != C_SYM_FUNC);
			*res = s->value;
		} else if (s->linkage == C_LINK_EXTERNAL || s->linkage == C_LINK_INTERNAL || s->linkage == C_LINK_BUILTIN) {
			ir_ref ref;
			ir_str str = s->alias ? IR_EXT_STR(s->alias) : IR_EXT_STR(name);

			if (s->kind == C_SYM_FUNC) {
				ref = ir_const_func(rcc->active_ctx, str,
					c_type2proto(rcc, s->value.type, s->linkage));
			} else {
				ref = ir_const_sym(rcc->active_ctx, str);
			}
			if (s->kind == C_SYM_FUNC) {
				c_value_set_rval(res, s->value.type, IR_ADDR, ref);
				if (s->linkage == C_LINK_BUILTIN) {
					res->u.op |= C_VAL_BUILTIN;
				}
				if (s == rcc->active_func) {
					/* recursive function - disable inlining */
					c_type *type = (c_type*)s->value.type;
					type->attr |= C_ATTR_NOINLINE;
				} else if (s->value.u.op & C_VAL_INLINE) {
					res->u.op |= C_VAL_INLINE;
					res->u.val.ptr = s->ctx;
				}
			} else if (s->value.type->kind != C_TYPE_ARRAY) {
				c_value_set_lval(res, s->value.type, c_type2ir(rcc, s->value.type), ref);
				res->u.val.ptr = c_value_is_const(&s->value) ? s->value.u.val.ptr : NULL;
			} else {
				c_value_set_rval(res, s->value.type, c_type2ir(rcc, s->value.type), ref);
				res->u.val.ptr = NULL;
			}
		} else {
			IR_ASSERT(0);
		}
	} else {
		IR_ASSERT(0);
	}
}

void c_wrong_type_specifiers(rcc_ctx *rcc, uint32_t flags, yy_sym sym)
{
	if (sym) {
		if (flags & C_TYPE_SPEC_COMPLEX) {
			if (sym == YY_CHAR
			 || sym == YY_SHORT
			 || sym == YY_INT
			 || sym == YY_LONG
			 || sym == YY_SIGNED
			 || sym == YY___SIGNED
			 || sym == YY___SIGNED__
			 || sym == YY_UNSIGNED) {
				flags |= C_TYPE_SPEC_INT;
			}
		} else if (sym == YY__COMPLEX
		 || sym == YY___COMPLEX
		 || sym == YY___COMPLEX__) {
			flags |= C_TYPE_SPEC_COMPLEX;
		}
	}

	if ((flags & C_TYPE_SPEC_COMPLEX)
	 && !(flags & (C_TYPE_SPEC_FLOAT|C_TYPE_SPEC_DOUBLE))
	 && (flags & (C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT|
			C_TYPE_SPEC_LONG|C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_UNSIGNED))) {
		if (sym) {
			yy_error_fmt("unexpected \"%s\", complex integer types are not supported", yy_sym2str(rcc, sym));
		} else {
			yy_error("complex integer types are not supported");
		}
	} else {
		if (sym) {
			yy_error_fmt("unexpected \"%s\", unsupported type specifier combination", yy_sym2str(rcc, sym));
		} else {
			yy_error("unsupported type specifier combination");
		}
	}
}

static void c_resolve_type_spec(rcc_ctx *rcc, c_dcl *d)
{
	IR_ASSERT(!d->type);
	switch (d->flags & C_TYPE_SPEC_ANY) {
		case C_TYPE_SPEC_VOID:
			d->type = &c_type_void;
			break;
		case C_TYPE_SPEC_CHAR:
			d->type = &c_type_char;
			break;
		case C_TYPE_SPEC_BOOL:
			d->type = &c_type_bool;
			break;
		case C_TYPE_SPEC_CHAR|C_TYPE_SPEC_SIGNED:
			d->type = &c_type_i8;
			break;
		case C_TYPE_SPEC_CHAR|C_TYPE_SPEC_UNSIGNED:
			d->type = &c_type_u8;
			break;
		case C_TYPE_SPEC_SHORT:
		case C_TYPE_SPEC_SHORT|C_TYPE_SPEC_SIGNED:
		case C_TYPE_SPEC_SHORT|C_TYPE_SPEC_INT:
		case C_TYPE_SPEC_SHORT|C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_i16;
			break;
		case C_TYPE_SPEC_SHORT|C_TYPE_SPEC_UNSIGNED:
		case C_TYPE_SPEC_SHORT|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_u16;
			break;
		case C_TYPE_SPEC_INT:
		case C_TYPE_SPEC_SIGNED:
		case C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_i32;
			break;
		case C_TYPE_SPEC_UNSIGNED:
		case C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_u32;
			break;
		case C_TYPE_SPEC_LONG:
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_SIGNED:
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_INT:
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_il;
			break;
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_UNSIGNED:
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_ul;
			break;
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG:
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG|C_TYPE_SPEC_SIGNED:
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG|C_TYPE_SPEC_INT:
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG|C_TYPE_SPEC_SIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_ill;
			break;
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG|C_TYPE_SPEC_UNSIGNED:
		case C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG|C_TYPE_SPEC_UNSIGNED|C_TYPE_SPEC_INT:
			d->type = &c_type_ull;
			break;
		case C_TYPE_SPEC_FLOAT:
			d->type = &c_type_float;
			break;
		case C_TYPE_SPEC_DOUBLE:
			d->type = &c_type_double;
			break;
		case C_TYPE_SPEC_LONG|C_TYPE_SPEC_DOUBLE:
			d->type = &c_type_long_double;
			break;
		case C_TYPE_SPEC_FLOAT|C_TYPE_SPEC_COMPLEX:
			d->type = &c_type_float_complex;
			break;
		case C_TYPE_SPEC_COMPLEX:
		case C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_COMPLEX:
			d->type = &c_type_double_complex;
			break;
		case C_TYPE_SPEC_DOUBLE|C_TYPE_SPEC_LONG|C_TYPE_SPEC_COMPLEX:
			d->type = &c_type_long_double_complex;
			break;
		default:
			if ((d->flags & C_TYPE_SPEC_ANY) == 0) {
				yy_warning("type defaults to \"int\"");
				d->type = &c_type_i32;
				break;
			}
			c_wrong_type_specifiers(rcc, d->flags, 0);
			break;
	}
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
}

static void c_validate_dcl(rcc_ctx *rcc, c_dcl *d)
{
	IR_ASSERT(d->type);
	if (d->attr) {
		if (d->attr & C_ATTR_RESTRICT) {
			if (d->type->kind != C_TYPE_POINTER) yy_error("invalid use of \"restrict\"");
		}
		if (d->attr & C_ATTR_ATOMIC) {
			if (d->type->kind == C_TYPE_ARRAY) yy_error("\"_Atomic\"-qualified array type");
			if (d->type->kind == C_TYPE_FUNC) yy_error("\"_Atomic\"-qualified function type");
		}
		if (d->attr & C_ATTR_ALIGN_MASK) {
// TODO: __Alignas() is prohibited, but __attribute__((aligned()))) is allowed ???
//			if (d->type->kind == C_TYPE_FUNC) yy_error("invalid use of \"_Alignas\" for a function");
// TODO: __Alignas() is prohibited, but __attribute__((aligned()))) is allowed ???
//			if (d->flags & C_DCL_TYPEDEF) yy_error("invalid use of \"_Alignas\" with \"typedef\"");
			if (d->flags & C_DCL_REGISTER) yy_error("invalid use of \"_Alignas\" with \"register\"");
		}
	}
}

static void c_merge_type_attr(rcc_ctx *rcc, c_dcl *d)
{
	c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	*type = *d->type;
	if (rcc->active_scope) type->flags &= ~C_TYPE_GLOBAL;
	type->attr |= (d->attr & C_TYPE_ATTRS);
	if ((d->attr & C_ATTR_ALIGN_MASK)
	 && (d->attr & C_ATTR_ALIGN_MASK) != (d->type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr = (type->attr & ~C_ATTR_ALIGN_MASK) | (d->attr & C_ATTR_ALIGN_MASK);
	}
	d->type = type;
}

static void c_finalize_type(rcc_ctx *rcc, c_dcl *d)
{
	if (!d->type) c_resolve_type_spec(rcc, d);
	if (d->flags & C_TYPE_SPEC_ATOMIC) {
		d->attr |= C_ATTR_ATOMIC;
		d->flags &= ~C_TYPE_SPEC_ANY;
		d->flags |= C_TYPE_SPEC_TYPE;
	}

	c_validate_dcl(rcc, d);

	if (d->vector_size) {
		c_type *type;

		if (d->type->kind < C_TYPE_U8 || d->type->kind > C_TYPE_DOUBLE) {
			yy_error("invalid vector type for attribute \"vector_size\")");
#if IR_SIMD
		} else if (d->vector_size / d->type->size <= 64) {
			/* General IR limitation */
#endif
		} else {
			yy_error_fmt("unsupported attribute \"vector_size(%u)\"", d->vector_size);
		}

		type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
		type->size = d->vector_size;
		type->kind = C_TYPE_VECTOR;
		type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
		type->attr = c_align2attr(IR_MIN(d->vector_size, 16)); /* 16 byte allgnment */
		type->vec.type = d->type;
		type->vec.length = d->vector_size / d->type->size;
		d->type = type;
		d->vector_size = 0;
	}

	if ((d->flags & C_TYPE_SPEC_NAME)
	 && (d->attr & C_TYPE_ATTRS)
	 && (d->type->kind == C_TYPE_ARRAY)) {
		const c_type *t = d->type;

		do {
			t = t->array.type;
		} while (t->kind == C_TYPE_ARRAY);
		if ((t->attr & C_TYPE_ATTRS) != (d->attr & C_TYPE_ATTRS)) {
			c_type *tmp = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));

			*tmp = *d->type;
			if (rcc->active_scope) tmp->flags &= ~C_TYPE_GLOBAL;
			d->type = tmp;
			t = d->type;
			do {
				tmp = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
				*tmp = *t->array.type;
				if (rcc->active_scope) tmp->flags &= ~C_TYPE_GLOBAL;
				((c_type*)(t))->array.type = tmp;
				t = tmp;
			} while (t->kind == C_TYPE_ARRAY);
			tmp->attr |= (d->attr & C_TYPE_ATTRS);
		}
		d->attr &= ~C_TYPE_ATTRS;
	}

	if ((d->attr & (C_TYPE_ATTRS|C_ATTR_ALIGN_MASK))
	 && (d->attr & (C_TYPE_ATTRS|C_ATTR_ALIGN_MASK))
	 != (d->type->attr & (C_TYPE_ATTRS|C_ATTR_ALIGN_MASK))) {
		c_merge_type_attr(rcc, d);
	}
	d->attr &= ~C_TYPE_ATTRS;
}

const c_type *c_resolve_type(rcc_ctx *rcc, c_dcl *d)
{
	c_finalize_type(rcc, d);
	return d->type;
}

static bool c_compatible_types(const c_type *t1, const c_type *t2, bool unqualified, bool func)
{
	uint32_t attr1, attr2;
	c_type_kind t1_kind = t1->kind;
	c_type_kind t2_kind = t2->kind;

	if (t1 == t2) return 1;

	attr1 = t1->attr & ~((C_ATTR_ALIGN_MASK|C_ATTR_FLEXIBLE|C_ATTR_VLA|C_ATTR_VMT|C_FUNC_TYPE_ATTRS) - C_ATTR_VARIADIC);
	attr2 = t2->attr & ~((C_ATTR_ALIGN_MASK|C_ATTR_FLEXIBLE|C_ATTR_VLA|C_ATTR_VMT|C_FUNC_TYPE_ATTRS) - C_ATTR_VARIADIC);
	if (unqualified || func) {
		attr1 &= ~(C_ATTR_CONST|C_ATTR_RESTRICT|C_ATTR_VOLATILE|C_ATTR_ATOMIC);
		attr2 &= ~(C_ATTR_CONST|C_ATTR_RESTRICT|C_ATTR_VOLATILE|C_ATTR_ATOMIC);
	}
	if (attr1 != attr2) return 0;

	if (t1_kind != t2_kind) {
		if (t1_kind == C_TYPE_ENUM) {
			t1_kind = t1->enumeration.kind;
			if (t2_kind == C_TYPE_ENUM) {
				t2_kind = t2->enumeration.kind;
			}
			return t1_kind == t2_kind;
		} else if (t2_kind == C_TYPE_ENUM) {
			t2_kind = t2->enumeration.kind;
			return t1_kind == t2_kind;
		} else if ((t1_kind == C_TYPE_POINTER && t2_kind == C_TYPE_ARRAY)
				|| (t1_kind == C_TYPE_ARRAY && t2_kind == C_TYPE_POINTER)) {
			return c_compatible_types(t1->pointer.type, t2->pointer.type, 0, 0);
		}
		return 0;
	}

	if (t1->kind == C_TYPE_ENUM) {
		if (t1->enumeration.tag != t2->enumeration.tag) return 0;
		if (t1->enumeration.tag && t2->enumeration.tag) return 1;
		if (t1->enumeration.values != t2->enumeration.values) return 0;
	} else if (t1->kind == C_TYPE_ARRAY) {
		if (!(t1->attr & (C_ATTR_FLEXIBLE|C_ATTR_VLA))
		 && !(t2->attr & (C_ATTR_FLEXIBLE|C_ATTR_VLA))
		 && t1->array.length != t2->array.length) return 0;
		if (!c_compatible_types(t1->array.type, t2->array.type, 0, 0)) return 0;
	} else if (t1->kind == C_TYPE_POINTER) {
		if (!c_compatible_types(t1->pointer.type, t2->pointer.type, 0, 0)) return 0;
	} else if (t1->kind == C_TYPE_VECTOR) {
		if (t1->vec.length != t2->vec.length) return 0;
		if (!c_compatible_types(t1->vec.type, t2->vec.type, 0, 0)) return 0;
	} else if (t1->kind == C_TYPE_STRUCT || t1->kind == C_TYPE_UNION) {
		if (t1->record.tag != t2->record.tag) return 0;
		if (t1->record.tag && t2->record.tag) return 1;
		if (t1->record.fields != t2->record.fields) return 0;
	} else if (t1->kind == C_TYPE_FUNC) {
		c_param *p1, *p2;
		uint32_t n;

		if (!c_compatible_types(t1->func.ret_type, t2->func.ret_type, 0, 0)) return 0;
		if (func) {
			if ((t1->attr & C_ATTR_OLD_FUNC) || (t2->attr & C_ATTR_OLD_FUNC)) return 1;
		} else {
			if ((t1->attr & C_ATTR_OLD_FUNC) != (t2->attr & C_ATTR_OLD_FUNC)) return 0;
		}
		if (t1->func.num_params != t2->func.num_params) return 0;
		p1 = t1->func.params;
		p2 = t2->func.params;
		for (n = t1->func.num_params; n > 0; p1++, p2++, n--) {
			if (!c_compatible_types(p1->type, p2->type, 1, 0)) return 0;
		}
	};
	return 1;
}

static bool c_is_flexible(const c_type *type)
{
	if (type->attr & C_ATTR_FLEXIBLE) {
		return 1;
	} else if (type->kind == C_TYPE_STRUCT
	 && type->record.num_fields > 0
	 && c_is_flexible(type->record.fields[type->record.num_fields-1].type)) {
		return 1;
	}
	return 0;
}

static void c_do_grow_flexible(rcc_ctx *rcc, c_sym *obj, size_t old_size, size_t size)
{
	IR_ASSERT(obj->tmp_data);
	IR_ASSERT(obj->value.u.type == IR_ADDR);
	IR_ASSERT(size > old_size);
	obj->value.u.val.ptr = ir_mem_realloc(obj->value.u.val.ptr, size);
	if (!obj->value.u.val.ptr) yy_error("not enough memory to allocate data");
	memset((char*)obj->value.u.val.ptr + old_size, 0, size - old_size);
	if (c_value_is_ref(&obj->value)) {
		ir_str name = rcc->active_ctx->ir_base[obj->value.u.ref].val.name;

		IR_ASSERT(IR_IS_EXT_STR(name));
		rcc->yy_hash.data[IR_EXT_STR(name)].sym->value.u.val.ptr = obj->value.u.val.ptr;
	}
}

static void c_do_end_flexible(rcc_ctx *rcc, c_sym *obj, size_t size)
{
	void *addr;

	IR_ASSERT(obj->tmp_data);
	if (c_value_is_ref(&obj->value)) {
		ir_str name = rcc->active_ctx->ir_base[obj->value.u.ref].val.name;
		c_name sym;

		IR_ASSERT(IR_IS_EXT_STR(name));
		sym = IR_EXT_STR(name);
		addr = c_linker_allocate_data(rcc, sym, size, c_attr2align(obj->value.type->attr), 1);
		memcpy(addr, obj->value.u.val.ptr, size);
		ir_mem_free(obj->value.u.val.ptr);
		obj->value.u.val.ptr = addr;
		obj->tmp_data = 0;

		/* Fix global symbol */
		rcc->yy_hash.data[sym].sym->value.u.val.ptr = obj->value.u.val.ptr;
		if (rcc->yy_hash.data[sym].sym->value.type->attr & C_ATTR_FLEXIBLE) {
			c_type *type = (c_type*)rcc->yy_hash.data[sym].sym->value.type;
			type->array.length = obj->value.type->array.length;
			type->size = obj->value.type->size;
			type->attr &= ~C_ATTR_FLEXIBLE;
		}
	} else {
		addr = c_linker_allocate_data(rcc, obj->value.u.ref, size,
			c_attr2align(obj->value.type->attr), 1);
		memcpy(addr, obj->value.u.val.ptr, size);
		ir_mem_free(obj->value.u.val.ptr);
		obj->value.u.val.ptr = addr;
		obj->tmp_data = 0;
	}
}

static ir_ref c_do_cast_ref(rcc_ctx *rcc, ir_type dst_type, ir_ref ref)
{
	ir_type src_type = rcc->active_ctx->ir_base[ref].type;

	if (src_type != dst_type) {
		if (IR_IS_TYPE_INT(dst_type)) {
			if (IR_IS_TYPE_INT(src_type)) {
				if (ir_type_size[dst_type] < ir_type_size[src_type]) {
					ref = ir_TRUNC(dst_type, ref);
				} else if (ir_type_size[dst_type] == ir_type_size[src_type]) {
					ref = ir_BITCAST(dst_type, ref);
				} else if (IR_IS_TYPE_SIGNED(src_type)) {
					ref = ir_SEXT(dst_type, ref);
				} else {
					ref = ir_ZEXT(dst_type, ref);
				}
			} else if (IR_IS_TYPE_FP(src_type)) {
				ref = ir_FP2INT(dst_type, ref);
			} else {
				IR_ASSERT(0);
			}
		} else if (IR_IS_TYPE_FP(dst_type)) {
			if (IR_IS_TYPE_INT(src_type)) {
				ref = ir_INT2FP(dst_type, ref);
			} else if (IR_IS_TYPE_FP(src_type)) {
				ref = ir_FP2FP(dst_type, ref);
			} else {
				IR_ASSERT(0);
			}
		} else {
			IR_ASSERT(0);
		}
	}

	return ref;
}

static ir_ref c_type_size(rcc_ctx *rcc, const c_type *type)
{
	if (type->attr & C_ATTR_VLA) {
		IR_ASSERT(type->kind == C_TYPE_ARRAY);
		ir_ref ref = type->array.length;
		if (rcc->active_ctx->ir_base[ref].type != IR_SIZE_T) {
			ref = c_do_cast_ref(rcc, IR_SIZE_T, ref);
		}
		return ir_MUL(IR_SIZE_T, ref, c_type_size(rcc, type->array.type));
	} else {
		return ir_const_size_t(rcc->active_ctx, type->size);
	}
}

static ir_ref c_type_ssize(rcc_ctx *rcc, const c_type *type)
{
	if (type->attr & C_ATTR_VLA) {
		IR_ASSERT(type->kind == C_TYPE_ARRAY);
		ir_ref ref = type->array.length;
		if (rcc->active_ctx->ir_base[ref].type != IR_SIZE_T) {
			ref = c_do_cast_ref(rcc, IR_SIZE_T, ref);
		}
		return ir_BITCAST(IR_SSIZE_T,
			ir_MUL(IR_SIZE_T, ref, c_type_size(rcc, type->array.type)));
	} else {
		return ir_const_ssize_t(rcc->active_ctx, type->size);
	}
}

static void c_validate_redeclaration(rcc_ctx *rcc, c_name name, c_dcl *d, c_sym *sym)
{
	if (d->flags & C_DCL_TYPEDEF) {
		if (sym->kind != C_SYM_TYPE) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
		} else if (!c_compatible_types(d->type, sym->value.type, 0, 0)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
		}
	} else if (d->flags & C_DCL_ENUM_CONST) {
		yy_error_fmt("redeclaration of \"%s\"", yy_sym2str(rcc, name));
	} else if (d->type->kind == C_TYPE_FUNC) {
		if (sym->kind != C_SYM_FUNC) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
		} else if (!c_compatible_types(d->type, sym->value.type, 0, 1)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
		} else if ((d->flags & C_DCL_DEFINITION) && sym->is_implemented) {
			yy_error_fmt("redefinition of \"%s\"", yy_sym2str(rcc, name));
		} else if ((d->flags & C_DCL_STATIC) && sym->linkage != C_LINK_INTERNAL) {
			yy_error_fmt("static declaration of \"%s\" follows non-static declaration", yy_sym2str(rcc, name));
		} else {
			if (d->alias) {
				if (!sym->alias) {
					if (d->alias != name) {
						if ((d->flags & C_DCL_HAS_ASM_NAME) && sym->is_implemented) {
							yy_warning("\"__asm__\" declaration ignored due to conflict with the previous name");
						} else {
							sym->alias = d->alias;
							sym->has_asm_name = (d->flags & C_DCL_HAS_ASM_NAME) != 0;
						}
					}
				} else if (d->alias != sym->alias) {
					yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
				}
			}
			if ((sym->value.type->attr & C_ATTR_OLD_FUNC)) {
				if (!(d->type->attr & C_ATTR_OLD_FUNC)) {
					if (!sym->is_implemented) {
						c_type *t = (c_type*)sym->value.type;
						t->attr &= ~C_ATTR_OLD_FUNC;
						t->func.num_params = d->type->func.num_params;
						t->func.params = d->type->func.params;
						sym->value.type = t;
					} else if (sym->value.type->func.num_params != d->type->func.num_params) {
						yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
					}
				} else if (d->type->func.num_params == 0 && !(d->flags & C_DCL_DEFINITION)) {
					/* pass */
				} else if (sym->value.type->func.num_params == 0 && d->type->func.num_params != 0) {
					c_type *t = (c_type*)sym->value.type;
					t->func.num_params = d->type->func.num_params;
					t->func.params = d->type->func.params;
					sym->value.type = t;
				} else if (sym->value.type->func.num_params != d->type->func.num_params) {
					yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
				}
			} else if (d->type->attr & C_ATTR_OLD_FUNC) {
				if (d->type->func.num_params == 0 && !(d->flags & C_DCL_DEFINITION)) {
					/* pass */
				} else if (sym->value.type->func.num_params != d->type->func.num_params) {
					yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
				}
			}

			if (d->flags & C_DCL_DEFINITION) {
				sym->is_implemented = 1;
			}
		}
	} else {
		if (sym->kind != C_SYM_VAR || !c_compatible_types(d->type, sym->value.type, 0, 0)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
		} else if ((d->flags & C_DCL_DEFINITION) && sym->is_implemented) {
			yy_error_fmt("redefinition of \"%s\"", yy_sym2str(rcc, name));
		} else if ((d->flags & C_DCL_STATIC) && sym->linkage != C_LINK_INTERNAL) {
			yy_error_fmt("static declaration of \"%s\" follows non-static declaration", yy_sym2str(rcc, name));
		} else if (!(d->flags & (C_DCL_STATIC|C_DCL_EXTERN)) && sym->linkage == C_LINK_INTERNAL) {
			yy_error_fmt("non-static declaration of \"%s\" follows static declaration", yy_sym2str(rcc, name));
		} else if ((d->flags & C_DCL_THREAD_LOCAL) && !sym->is_thread_local) {
			yy_error_fmt("thread-local declaration of \"%s\" follows non-thread-local declaration", yy_sym2str(rcc, name));
		} else if (!(d->flags & C_DCL_THREAD_LOCAL) && sym->is_thread_local) {
			yy_error_fmt("non-thread-local declaration of \"%s\" follows thread-local declaration", yy_sym2str(rcc, name));
		} else if ((d->flags & (C_DCL_DEFINITION|C_DCL_EXTERN)) == (C_DCL_DEFINITION|C_DCL_EXTERN)) {
			yy_warning_fmt("\"%s\" initialized and declared \"extern\"", yy_sym2str(rcc, name));
			d->flags &= ~C_DCL_EXTERN;
		}
		if (sym->value.type->kind == C_TYPE_ARRAY && (sym->value.type->attr & C_ATTR_FLEXIBLE)
		 && d->type->kind == C_TYPE_ARRAY && !(d->type->attr & (C_ATTR_FLEXIBLE|C_ATTR_VLA))) {
			c_type *t = (c_type*)sym->value.type;
			t->attr &= ~C_ATTR_FLEXIBLE;
			t->array.length = d->type->array.length;
			t->size = d->type->size;
		}
		if (d->alias) {
			if (!sym->alias) {
				if (d->alias != name) {
					if ((d->flags & C_DCL_HAS_ASM_NAME) && sym->is_implemented) {
						yy_warning("\"__asm__\" declaration ignored due to conflict with the previous name");
					} else {
						sym->alias = d->alias;
						sym->has_asm_name = (d->flags & C_DCL_HAS_ASM_NAME) != 0;
					}
				}
			} else if (d->alias != sym->alias) {
				yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
			}
		}
		if (d->flags & C_DCL_DEFINITION) {
			sym->is_implemented = 1;
		}
		if (!c_value_is_const(&sym->value)
		 && !(d->flags & C_DCL_EXTERN)
		 && ((d->flags & C_DCL_DEFINITION) || !(d->type->attr & C_ATTR_FLEXIBLE))) {
			sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
			sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
			if (c_is_flexible(sym->value.type)) {
				sym->tmp_data = 1;
				sym->value.u.val.ptr = ir_mem_calloc(1, sym->value.type->size);
				if (!sym->value.u.val.ptr) yy_error("not enough memory to allocate data");
				sym->value.u.ref = name; /* keep name in addition to address */
			} else {
				sym->value.u.val.ptr = c_linker_allocate_data(rcc, name,
					sym->value.type->size, c_attr2align(sym->value.type->attr), sym->value.type->kind == C_TYPE_ARRAY);
			}
		}
	}
}

c_sym *c_global_sym(rcc_ctx *rcc, c_sym *sym)
{
	c_scope *scope = sym->scope;
	yy_sym id, *p = scope->list.syms;
	uint32_t n = scope->list.len;
	void *ptr;

	while (n) {
		id = *p++;
		(void)id;
		n -= 1 + sizeof(void*)/sizeof(uint32_t);
		p = pp_load_ptr(p, &ptr);
		if ((((uintptr_t)ptr) & C_POP_MASK) == C_POP_SYM) {
			sym = ptr;
			if (!sym) return NULL;
			if (!sym->scope) return sym;
			scope = sym->scope;
			p = scope->list.syms;
			n = scope->list.len;
		}
	}
	return NULL;
}

static const c_type *c_create_global_type(rcc_ctx *rcc, const c_type *type)
{
	c_type *t;
	uint32_t i;

	if (type->flags & C_TYPE_GLOBAL) return type;
	t = ir_arena_alloc(&rcc->yy_arena, sizeof(c_type));
	*t = *type;
	t->flags |= C_TYPE_GLOBAL;
	if (t->kind == C_TYPE_POINTER) {
		t->pointer.type = c_create_global_type(rcc, t->pointer.type);
	} else if (t->kind == C_TYPE_ARRAY) {
		t->array.type = c_create_global_type(rcc, t->array.type);
	} else if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
		t->record.fields = ir_arena_alloc(&rcc->yy_arena, sizeof(c_field) * t->record.num_fields);
		memcpy(t->record.fields, type->record.fields, sizeof(c_field) * t->record.num_fields);
		for (i = 0; i < t->record.num_fields; i++) {
			t->record.fields[i].type = c_create_global_type(rcc, t->record.fields[i].type);
		}
	} else if (t->kind == C_TYPE_FUNC) {
		t->func.ret_type = c_create_global_type(rcc, t->func.ret_type);
		t->func.params = ir_arena_alloc(&rcc->yy_arena, sizeof(c_param) * t->func.num_params);
		memcpy(t->func.params, type->func.params, sizeof(c_param) * t->func.num_params);
		for (i = 0; i < t->func.num_params; i++) {
			t->func.params[i].type = c_create_global_type(rcc, t->func.params[i].type);
		}
	} else if (t->kind == C_TYPE_ENUM) {
		t->enumeration.values = NULL;
	}
	return t;
}

static const c_type *c_create_in_func_type(rcc_ctx *rcc, const c_type *type)
{
	c_type *t;
	uint32_t i;

	if (type->flags & (C_TYPE_GLOBAL|C_TYPE_IN_FUNC)) return type;
	t = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_type));
	*t = *type;
	t->flags |= C_TYPE_IN_FUNC;
	if (t->kind == C_TYPE_POINTER) {
		t->pointer.type = c_create_in_func_type(rcc, t->pointer.type);
	} else if (t->kind == C_TYPE_ARRAY) {
		t->array.type = c_create_in_func_type(rcc, t->array.type);
	} else if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
		t->record.fields = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_field) * t->record.num_fields);
		memcpy(t->record.fields, type->record.fields, sizeof(c_field) * t->record.num_fields);
		for (i = 0; i < t->record.num_fields; i++) {
			t->record.fields[i].type = c_create_in_func_type(rcc, t->record.fields[i].type);
		}
	} else if (t->kind == C_TYPE_FUNC) {
		t->func.ret_type = c_create_in_func_type(rcc, t->func.ret_type);
		t->func.params = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_param) * t->func.num_params);
		memcpy(t->func.params, type->func.params, sizeof(c_param) * t->func.num_params);
		for (i = 0; i < t->func.num_params; i++) {
			t->func.params[i].type = c_create_in_func_type(rcc, t->func.params[i].type);
		}
	} else if (t->kind == C_TYPE_ENUM) {
		t->enumeration.values = NULL;
	}
	return t;
}

static c_name c_create_static_var(rcc_ctx *rcc, c_name name, c_dcl *d)
{
	yy_dyn_str  dyn_str;
	const char *name_str;
	size_t name_len;
	uint32_t i, n;
	char buf[16];
	c_sym *sym;

	name_str = yy_sym2strl(rcc, name, &name_len);
	yy_dyn_str_init(rcc, &dyn_str, name_str, name_len);
	yy_dyn_str_append(rcc, &dyn_str, ".", 1);

	i = sizeof(buf);
	n = ++rcc->c_static_var_num;
	buf[--i] = 0;
	do {
		buf[--i] = '0' + n % 10;
		n = n / 10;
	} while (n != 0);
	yy_dyn_str_append0(rcc, &dyn_str, buf + i, sizeof(buf) - i - 1);
	name = yy_hash_lookup(rcc, dyn_str.str, dyn_str.len);

	/* Create a global symbol in yy_arena */
	sym = ir_arena_alloc(&rcc->yy_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	sym->kind = C_SYM_VAR;
	sym->linkage = C_LINK_INTERNAL;
	sym->is_thread_local = 0;
	sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
	sym->value.type = c_create_global_type(rcc, d->type);
	rcc->yy_hash.data[name].sym = sym;

	return name;
}

static ir_ref c_create_str_sym(rcc_ctx *rcc, c_value *res)
{
	const c_type *type = res->type;
	const void *str = c_value_str_addr(res);
	size_t size = c_value_str_size(res);
	void *addr;
	char buf[32];
	uint32_t i, n;
	c_name name;
	c_sym *sym;
	ir_ref ref;

	name = ir_strtab_find(&rcc->c_strtab, str, size);
	if (name) {
		ref = ir_const_sym(rcc->active_ctx, IR_EXT_STR(name));

		c_value_set_rval(res, type, IR_ADDR, ref);
		res->u.val.ptr = rcc->yy_hash.data[name].sym->value.u.val.ptr;

		return ref;
	}

	i = sizeof(buf);
	n = ++rcc->c_static_str_num;
	buf[--i] = 0;
	do {
		buf[--i] = '0' + n % 10;
		n = n / 10;
	} while (n != 0);
	buf[--i] = '.';
	buf[--i] = 'r';
	buf[--i] = 't';
	buf[--i] = 's';

	name = yy_hash_lookup(rcc, buf + i, sizeof(buf) - 1 - i);
	addr = c_linker_allocate_data(rcc, name, size, 8, 1);
	memcpy(addr, str, size);

	/* Create a global symbol in yy_arena */
	sym = ir_arena_alloc(&rcc->yy_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	sym->kind = C_SYM_VAR;
	sym->linkage = C_LINK_INTERNAL;
	sym->is_thread_local = 0;
	sym->is_implemented = 1;
	sym->is_string = 1;
	rcc->yy_hash.data[name].sym = sym;

	sym->value.type = type;
	sym->value.u.optx = IR_OPT(C_SYM_CONST, IR_ADDR);
	sym->value.u.val.ptr = addr;
	sym->value.u.ref = size;

	ir_strtab_lookup(&rcc->c_strtab, str, size, name);

	ref = ir_const_sym(rcc->active_ctx, IR_EXT_STR(name));

	c_value_set_rval(res, type, IR_ADDR, ref);
	res->u.val.ptr = addr;

	return ref;
}

static ir_ref c_create_label_str(rcc_ctx *rcc, ir_ref label_num)
{
	char buf[32];
	uint32_t i, n;
	c_name name;

	i = sizeof(buf);
	n = label_num;
	buf[--i] = 0;
	do {
		buf[--i] = '0' + n % 10;
		n = n / 10;
	} while (n != 0);
	buf[--i] = '.';
	buf[--i] = 'l';
	buf[--i] = 'e';
	buf[--i] = 'b';
	buf[--i] = 'a';
	buf[--i] = 'l';

	name = yy_hash_lookup(rcc, buf + i, sizeof(buf) - 1 - i);

	return IR_EXT_STR(name);
}

static bool c_is_builtin_func_name(c_name name)
{
	return name >= YY_BUILTIN_FIRST && name <= YY_BUILTIN_LAST;
}

c_sym *c_declare(rcc_ctx *rcc, c_name name, c_dcl *d)
{
	c_sym *sym;
	c_scope *scope = rcc->active_scope;

	c_finalize_type(rcc, d);
	if (d->attr) {
		if (d->attr & C_ATTR_INLINE) {
			if (d->type->kind != C_TYPE_FUNC) yy_error("invalid use of \"inline\"");
		}
		if (d->attr & C_ATTR_NORETURN) {
			if (d->type->kind != C_TYPE_FUNC) yy_error("invalid use of \"_Noreturn\"");
		}
		if (d->type->kind == C_TYPE_FUNC) {
			((c_type*)d->type)->attr |= d->attr & C_FUNC_TYPE_ATTRS;
		}
	}
	IR_ASSERT(name);

	if (scope) {
		if (d->type->kind == C_TYPE_FUNC && (d->flags & C_DCL_DEFINITION)) {
			yy_error("nested functions are not implemented yet"); //???
		} else if (d->type->kind == C_TYPE_FUNC || (d->flags & C_DCL_EXTERN)) {
			/* function and external declation inside other function assumes declaration in the global scope */
			while (scope != NULL) {
				scope->checkpoint = NULL; /* prevent deallocations in c_pop_scope() */
				scope = scope->prev;
			}
			scope = NULL;
		}
	}

	if (d->flags & C_DCL_FOR) {
		if ((d->flags & C_DCL_TYPEDEF) || d->type->kind == C_TYPE_FUNC) {
			yy_error("non-variable declaration in \"for\" loop");
		} else if (d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN|C_DCL_STATIC|C_DCL_THREAD_LOCAL)) {
			yy_error("declaration of non-local variable in \"for\" loop");
		}
	}

	sym = rcc->yy_hash.data[name].sym;
	if (sym) {
		if (d->flags & C_DCL_EXTERN) {
			if (!sym->scope) {
				c_validate_redeclaration(rcc, name, d, sym);
				return sym;
			} else {
				c_sym *gsym = c_global_sym(rcc, sym);
				if (gsym) {
					c_validate_redeclaration(rcc, name, d, gsym);
				}
				if (rcc->active_scope) {
					if (!rcc->active_scope->list.syms) pp_list_init(rcc, &rcc->active_scope->list);
					pp_list_push(&rcc->active_scope->list, name);
					pp_list_push_ptr(&rcc->active_scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));
				}
				if (gsym) {
					rcc->yy_hash.data[name].sym = gsym;
					return gsym;
				}
			}
		}
		if (sym->scope == scope) {
			if (!scope) {
				c_validate_redeclaration(rcc, name, d, sym);
				return sym;
			} else {
				if (d->flags & C_DCL_TYPEDEF) {
					if (sym->kind == C_SYM_TYPE
					 && c_compatible_types(d->type, sym->value.type, 0, 0)) {
						return sym;
					}
					yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(rcc, name));
				} else {
					yy_error_fmt("redeclaration of \"%s\"", yy_sym2str(rcc, name));
				}
				return NULL;
			}
		}
	}

	if (scope) {
		if (!scope->list.syms) pp_list_init(rcc, &scope->list);
		pp_list_push(&scope->list, name);
		pp_list_push_ptr(&scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));
	}

	if (((d->type->flags & C_TYPE_INCOMPLETE)
	  && !(d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN))
	  && !c_fix_incomplete_type(rcc, d->type))
	 || ((d->type->attr & C_ATTR_FLEXIBLE)
	  && !(d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN|C_DCL_DEFINITION))
	  && scope)
	 || ((d->type->kind == C_TYPE_VOID)
	  && !(d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN)))
	  ) {
		yy_error_fmt("storage size of \"%s\" isn't known", yy_sym2str(rcc, name));
		return NULL;
	}

	if (d->cleanup_func
	 && scope
	 && !(d->flags & (C_DCL_TYPEDEF|C_DCL_ENUM_CONST|C_DCL_STATIC|C_DCL_EXTERN|C_DCL_REG_VAR))
	 && d->type->kind != C_TYPE_FUNC) {
		sym = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_sym));
	} else {
		sym = ir_arena_alloc(&rcc->c_arena, sizeof(c_sym));
	}
	memset(sym, 0, sizeof(c_sym));
	if (d->flags & C_DCL_TYPEDEF) {
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-C_DCL_TYPEDEF)) == 0);
		sym->kind = C_SYM_TYPE;

		if (!scope && UNEXPECTED(d->type->attr & (C_ATTR_VLA|C_ATTR_VMT))) {
			if (d->type->attr & C_ATTR_VLA) {
				yy_error("variable length array declaration not allowed in file scope");
			} else {
				yy_error("variable modified type declaration not allowed in file scope");
			}
		}
	} else if (d->flags & C_DCL_ENUM_CONST) {
		IR_ASSERT((d->flags & C_DCL_STORAGE_CLASS) == 0);
		sym->kind = C_SYM_CONST;
		/* the value will be set in c_declare_enum_val() */
	} else if (d->type->kind == C_TYPE_FUNC) {
		if ((d->flags & (C_DCL_THREAD_LOCAL|C_DCL_AUTO|C_DCL_REGISTER))
		 || ((d->flags & C_DCL_STATIC) && rcc->active_scope)) {
			if ((d->flags & (C_DCL_THREAD_LOCAL|C_DCL_AUTO|C_DCL_REGISTER|C_DCL_STATIC)) == C_DCL_AUTO
			 && rcc->active_scope) {
				yy_error("nested functions are not implemented yet (storage class \"auto\")"); //???
			} else {
				yy_error_fmt("invalid storage class for function \"%s\"", yy_sym2str(rcc, name));
			}
		}
		if (!(d->flags & C_DCL_DEFINITION)
		 && (d->type->attr & C_ATTR_OLD_FUNC)
		 && d->type->func.num_params != 0) {
			yy_error("parameter names (without types) in function declaration");
		}
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC))) == 0);
		sym->kind = C_SYM_FUNC;
		if (!(d->flags & (C_DCL_STATIC|C_DCL_DEFINITION))
		 && c_is_builtin_func_name(name)) {
			/* TODO: verify prototype ??? */
			sym->linkage = C_LINK_BUILTIN;
		} else {
			sym->linkage = (d->flags & C_DCL_STATIC) ? C_LINK_INTERNAL : C_LINK_EXTERNAL;
			if (d->alias != name)  sym->alias = d->alias;
		}
		sym->is_thread_local = 0;
		sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
		sym->has_asm_name = (d->flags & C_DCL_HAS_ASM_NAME) != 0;
	} else {
		if (!scope) {
			if (d->flags & C_DCL_AUTO) yy_error_fmt("file-scope declaration of \"%s\" specifies \"auto\"", yy_sym2str(rcc, name));
		} else {
			if ((d->flags & (C_DCL_THREAD_LOCAL|C_DCL_STATIC|C_DCL_EXTERN)) == C_DCL_THREAD_LOCAL) {
				yy_error_fmt("function-scope \"%s\" declared \"_Thread_local\"", yy_sym2str(rcc, name));
			}
		}
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC|C_DCL_THREAD_LOCAL|C_DCL_AUTO|C_DCL_REGISTER))) == 0);
		sym->kind = C_SYM_VAR;
		if (!scope && (d->flags & C_DCL_REGISTER)) {
			/* global register variable */
			if (!(d->flags & C_DCL_REG_VAR)) yy_error_fmt("register name not specified for global register variable \"%s\"", yy_sym2str(rcc, name));
			if (C_IS_TYPE_INT_OR_PTR(d->type)) {
				if (d->reg >= IR_REG_FP_FIRST) yy_error_fmt("data type of \"%s\" is not suitable for a floating point register", yy_sym2str(rcc, name));
			} else if (C_IS_TYPE_FP(d->type)) {
				if (d->reg < IR_REG_FP_FIRST) yy_error_fmt("data type of \"%s\" is not suitable for a general purpose register", yy_sym2str(rcc, name));
			} else {
				yy_error_fmt("data type of \"%s\" is not suitable for a register", yy_sym2str(rcc, name));
			}
			// TODO: Global register variables shouldn't be clobbered by any function ???
			const ir_call_conv_dsc *cc = ir_get_call_conv_dsc(rcc->active_ctx->flags);
			if (!IR_REGSET_IN(cc->preserved_regs, d->reg)) {
				yy_warning_fmt("call-clobbered register used for global register variable \"%s\"", yy_sym2str(rcc, name));
			}
			if (rcc->c_fixed_regset & (1ULL << d->reg)) {
				yy_error_fmt("register \"%s\" is already used", ir_reg_name(d->reg, c_type2ir(rcc, d->type)));
			}
			rcc->c_fixed_regset |= 1ULL << d->reg;
			c_value_set_reg(&sym->value, d->type, c_type2ir(rcc, d->type), d->reg);
			sym->linkage = C_LINK_NONE;
			sym->is_implemented = 1;
		} else if (!scope || (d->flags & (C_DCL_STATIC|C_DCL_EXTERN))) {
			sym->linkage = (d->flags & C_DCL_STATIC) ? C_LINK_INTERNAL : C_LINK_EXTERNAL;
			if (d->alias != name) sym->alias = d->alias;
			sym->is_thread_local = (d->flags & C_DCL_THREAD_LOCAL) != 0;
			sym->has_asm_name = (d->flags & C_DCL_HAS_ASM_NAME) != 0;

			if (UNEXPECTED(d->type->attr & (C_ATTR_VLA|C_ATTR_VMT))) {
				if (d->type->attr & C_ATTR_VLA) {
					if (d->flags & C_DCL_EXTERN) {
						yy_error("variable length array declaration cannot have \"extern\" linkage");
					} else if (d->flags & C_DCL_STATIC) {
						yy_error("variable length array declaration cannot have \"static\" linkage");
					} else {
						yy_error("variable length array declaration not allowed in file scope");
					}
				} else {
					if (d->flags & C_DCL_EXTERN) {
						yy_error("variable modified type declaration cannot have \"extern\" linkage");
					} else if (d->flags & C_DCL_STATIC) {
						yy_error("variable modified type declaration cannot have \"static\" linkage");
					} else {
						yy_error("variable modified type declaration not allowed in file scope");
					}
				}
			}
			if ((d->flags & C_DCL_EXTERN) && (d->flags & C_DCL_DEFINITION)) {
				yy_warning_fmt("\"%s\" initialized and declared \"extern\"", yy_sym2str(rcc, name));
				d->flags &= ~C_DCL_EXTERN;
			}
			if (!(d->flags & C_DCL_EXTERN)
			 && ((d->flags & C_DCL_DEFINITION) || !(d->type->attr & C_ATTR_FLEXIBLE))) {
				void *addr;

				sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
				if (!scope || !(d->flags & C_DCL_STATIC)) {
					if (c_is_flexible(d->type)) {
						sym->tmp_data = 1;
						addr = ir_mem_calloc(1, d->type->size);
						if (!addr) yy_error("not enough memory to allocate data");
					} else {
						addr = c_linker_allocate_data(rcc, name,
							d->type->size, c_attr2align(d->type->attr), d->type->kind == C_TYPE_ARRAY);
					}
					sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
					sym->value.u.val.ptr = addr;
					sym->value.u.ref = name; /* keep name in addition to address */
				} else {
					c_name sym_name = sym->alias ? sym->alias : c_create_static_var(rcc, name, d);
					ir_ref ref;

					if (c_is_flexible(d->type)) {
						sym->tmp_data = 1;
						addr = ir_mem_calloc(1, d->type->size);
						if (!addr) yy_error("not enough memory to allocate data");
					} else {
						addr = c_linker_allocate_data(rcc, sym_name,
							d->type->size, c_attr2align(d->type->attr), d->type->kind == C_TYPE_ARRAY);
					}
					rcc->yy_hash.data[sym_name].sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
					rcc->yy_hash.data[sym_name].sym->value.u.val.ptr = addr;
					ref = ir_const_sym(rcc->active_ctx, IR_EXT_STR(sym_name));
					if (d->type->kind == C_TYPE_ARRAY) {
						c_value_set_rval(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
					} else {
						c_value_set_lval(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
					}
					sym->value.u.val.ptr = addr; /* keep address in addition to ref */
				}
			}
		} else {
			ir_ref ref;

			sym->linkage = C_LINK_NONE;
			sym->is_thread_local = 0;
			if (d->type->kind == C_TYPE_ARRAY) {
				if (d->type->attr & C_ATTR_VLA) {
					ir_ref size = c_type_size(rcc, d->type);

					IR_ASSERT(scope);
					if (!scope->vla_block) {
						if (scope->last_vla_block) {
							scope->last_vla_block = scope->vla_block = ir_BLOCK_BEGIN();
						} else {
							IR_ASSERT(rcc->c_prologue_end);
							scope->last_vla_block = scope->vla_block = rcc->c_prologue_end;
						}
					}
					ref = ir_ALLOCA(size);
					if (d->flags & C_DCL_DEFINITION) {
						ir_memzero(rcc, ref, size, c_attr2align(d->type->attr));
					}
					c_value_set_rval(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
				} else {
					size_t size = (d->type->attr & C_ATTR_FLEXIBLE) ? (size_t)-1 : d->type->size;
					ref = c_do_alloca(rcc, size, c_attr2align(d->type->attr), (d->flags & C_DCL_DEFINITION) != 0);
					c_value_set_rval(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
				}
			} else if (d->type->kind == C_TYPE_STRUCT || d->type->kind == C_TYPE_UNION) {
				if (d->flags == C_DCL_PARAM) {
					ir_type types[MAX_ABI_TYPES];
					int n = c_abi_lower_struct_arg(d->type, types);

					if (n == 1) {
						ref = c_do_alloca(rcc, d->type->size, c_attr2align(d->type->attr), (d->flags & C_DCL_DEFINITION) != 0);
						c_value_set_lval(&sym->value, d->type, types[0], ref);
					} else {
						IR_ASSERT(n == 0);
						c_value_set_lval(&sym->value, d->type, IR_ADDR, IR_UNUSED);
					}
				} else {
					ref = c_do_alloca(rcc, d->type->size, c_attr2align(d->type->attr), (d->flags & C_DCL_DEFINITION) != 0);
					c_value_set_lval(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
				}
			} else if (d->flags & C_DCL_REG_VAR) {
				/* local register variables */
				if (C_IS_TYPE_INT_OR_PTR(d->type)) {
					if (d->reg >= IR_REG_FP_FIRST) yy_error_fmt("data type of \"%s\" is not suitable for a floating point register", yy_sym2str(rcc, name));
				} else if (C_IS_TYPE_FP(d->type)) {
					if (d->reg < IR_REG_FP_FIRST) yy_error_fmt("data type of \"%s\" is not suitable for a general purpose register", yy_sym2str(rcc, name));
				} else {
					yy_error_fmt("data type of \"%s\" is not suitable for a register", yy_sym2str(rcc, name));
				}
				const ir_call_conv_dsc *cc = ir_get_call_conv_dsc(rcc->active_ctx->flags);
				if (IR_REGSET_IN(cc->preserved_regs, d->reg)) {
					if (rcc->active_ctx->fixed_regset & (1ULL << d->reg)) {
						yy_error_fmt("register \"%s\" is already used", ir_reg_name(d->reg, c_type2ir(rcc, d->type)));
					}
					rcc->active_ctx->fixed_regset |= 1ULL << d->reg;
					rcc->active_ctx->fixed_save_regset |= 1ULL << d->reg;
				} else {
					if (rcc->active_ctx->fixed_regset & (1ULL << d->reg)) {
						yy_error_fmt("register \"%s\" is already used", ir_reg_name(d->reg, c_type2ir(rcc, d->type)));
					}
					rcc->active_ctx->fixed_regset |= 1ULL << d->reg;
				}
				c_value_set_reg(&sym->value, d->type, c_type2ir(rcc, d->type), d->reg);
			} else {
				ref = ir_var_ex(rcc->active_ctx, c_type2ir(rcc, d->type), 1, IR_EXT_STR(name));
				c_value_set_var(&sym->value, d->type, c_type2ir(rcc, d->type), ref);
			}
		}
	}
	sym->value.type = d->type;
	sym->scope = scope;

	rcc->yy_hash.data[name].sym = sym;


	if (d->cleanup_func) {
		c_sym *f = rcc->yy_hash.data[d->cleanup_func].sym;

		if (sym->kind != C_SYM_VAR || !sym->scope || sym->linkage != C_LINK_NONE) {
			yy_warning_fmt("__attribure__((cleanup(%s))) ignored, it only applies to local variables",
				yy_sym2str(rcc, d->cleanup_func));
		} else if (c_value_is_reg(&sym->value)) {
			yy_error_fmt("__attribure__((cleanup(%s))) cannot be applyed to register variable",
				yy_sym2str(rcc, d->cleanup_func));
		} else if (!f || f->value.type->kind != C_TYPE_FUNC) {
			yy_error_fmt("__attribure__((cleanup(%s))) argument is not a function",
				yy_sym2str(rcc, d->cleanup_func));
		} else if (f->value.type->func.num_params != 1) {
			yy_error_fmt("__attribure__((cleanup(%s))) function must take 1 parameter",
				yy_sym2str(rcc, d->cleanup_func));
		} else if (!f->value.type->func.params[0].type
		 || f->value.type->func.params[0].type->kind != C_TYPE_POINTER
		 || (f->value.type->func.params[0].type->pointer.type->kind != C_TYPE_VOID
		  && !c_compatible_types(f->value.type->func.params[0].type->pointer.type, sym->value.type, 1, 0))) {
			yy_error_fmt("__attribure__((cleanup(%s))) function parameter is incompatible with type of variable",
				yy_sym2str(rcc, d->cleanup_func));
		} else {
			sym->cleanup_func = d->cleanup_func;
			sym->cleanup_next = scope->cleanup_sym;
			scope->cleanup_sym = sym;
		}
	}

	return sym;
}

void c_empty_declaration(rcc_ctx *rcc, c_dcl *d)
{
	if (d->flags & C_DCL_STORAGE_CLASS) yy_warning("useless storage class specifier in empty declaration");
	if (d->type) {
		if (d->type->kind == C_TYPE_ENUM) {
			/* pass */
		} else if (d->type->kind == C_TYPE_STRUCT || d->type->kind == C_TYPE_UNION) {
			if (!d->type->record.tag) yy_warning("unnamed struct/union that defines no instances");
		} else {
		    yy_warning("useless type in empty declaration");
		}
	} else if (d->flags & C_TYPE_SPEC_ANY) {
		yy_warning("useless type specifier in empty declaration");
	}
	if (d->cleanup_func) {
		yy_warning_fmt("__attribure__((cleanup(%s))) ignored, it only applies to local variables",
			yy_sym2str(rcc, d->cleanup_func));
	}
}

static const char* c_type_kind2str(c_type_kind kind)
{
	switch (kind) {
		case C_TYPE_STRUCT: return "struct";
		case C_TYPE_UNION: return "union";
		case C_TYPE_ENUM: return "enum";
		default: IR_ASSERT(0); return NULL;
	}
}

static const char* c_tag2str(c_tag *tag)
{
	return c_type_kind2str(tag->type->kind);
}

c_type *c_resolve_tag(rcc_ctx *rcc, c_name name, c_dcl *d, bool define, const c_type *underlying_type)
{
	c_type *type;
	c_tag *tag;

	IR_ASSERT(name);
	tag = rcc->yy_hash.data[name].tag;
	if (tag) {
		if (((d->flags & C_TYPE_SPEC_ENUM) && (tag->type->kind != C_TYPE_ENUM))
		 || ((d->flags & C_TYPE_SPEC_STRUCT) && (tag->type->kind != C_TYPE_STRUCT))
		 || ((d->flags & C_TYPE_SPEC_UNION) && (tag->type->kind != C_TYPE_UNION))) {
			yy_error_fmt("\"%s\" defined as wrong kind of tag", yy_sym2str(rcc, name));
		}
		if (define) {
			if (tag->type->flags & C_TYPE_INCOMPLETE) {
				if (tag->type->flags & C_TYPE_INPROGRESS) yy_error_fmt("nested redefinition of \"%s %s\"", c_tag2str(tag), yy_sym2str(rcc, name));
				if (tag->scope == rcc->active_scope) {
					d->type = tag->type;
					return (c_type*)tag->type;
				}
			} else {
				if (tag->scope == rcc->active_scope) yy_error_fmt("redefinition of \"%s %s\"", c_tag2str(tag), yy_sym2str(rcc, name));
			}
		} else {
			d->type = tag->type;
			d->flags &= ~C_TYPE_SPEC_ANY;
			d->flags |= C_TYPE_SPEC_TYPE;
			return (c_type*)tag->type;
		}
	}

	if (rcc->active_scope) {
		if (!rcc->active_scope->list.syms) pp_list_init(rcc, &rcc->active_scope->list);
		pp_list_push(&rcc->active_scope->list, name);
		pp_list_push_ptr(&rcc->active_scope->list, (void*)(((uintptr_t)tag) | C_POP_TAG));
	}

	if (d->flags & C_TYPE_SPEC_ENUM) {
		type = c_make_enum_type(rcc, d, name, underlying_type);
	} else {
		type = c_make_struct_type(rcc, d, name);
	}

	tag = ir_arena_alloc(&rcc->c_arena, sizeof(c_tag));
	tag->scope = rcc->active_scope;
	tag->type = d->type;

	rcc->yy_hash.data[name].tag = tag;
	return type;
}

static void c_validate_pointer_type(const c_type *t)
{
	//???
}

static c_type *c_create_pointer_type(rcc_ctx *rcc, const c_type *element_type)
{
	c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	type->kind = C_TYPE_POINTER;
	type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
	type->attr = c_align2attr(_Alignof(void*));
	if (element_type->attr & (C_ATTR_VLA|C_ATTR_VMT)) {
		type->attr |= C_ATTR_VMT;
	}
	type->size = sizeof(void*);
	type->pointer.type = element_type;
	return type;
}

void c_make_pointer_type(rcc_ctx *rcc, c_dcl *d)
{
	c_type *type;

	c_finalize_type(rcc, d);
	c_validate_pointer_type(d->type);

	type = c_create_pointer_type(rcc, d->type);
	type->attr |= d->attr & C_POINTER_ATTRS;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_POINTER_ATTRS;
}

static void c_validate_array_element_type(rcc_ctx *rcc, const c_type *t)
{
	if (t->kind == C_TYPE_VOID) yy_error("array of voids");
	if (t->kind == C_TYPE_FUNC) yy_error("array of functions");
	if ((t->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, t)) {
		yy_error("array type has incomplete element type");
	}
	if (t->kind == C_TYPE_ARRAY && (t->attr & C_ATTR_FLEXIBLE)) {
		yy_error("array type has element type with undefined size");
	}
}

void c_make_array_type(rcc_ctx *rcc, c_dcl *d, c_dcl *dim, c_value *len, uint64_t attr, bool is_param)
{
	c_type *type;
	size_t length;
	uint32_t align_attr = d->attr & C_ATTR_ALIGN_MASK;
	yy_sym *vla_tokens = NULL;

	d->attr &= ~C_ATTR_ALIGN_MASK;
	c_finalize_type(rcc, d);
	c_validate_array_element_type(rcc, d->type);
	d->attr |= align_attr;

	length = 0;
	if (!(attr & (C_ATTR_FLEXIBLE|C_ATTR_VLA)) && len && c_value_is_set(len)) {
		if (!C_IS_TYPE_INT(len->type) && len->type->kind != C_TYPE_ENUM) {
			yy_error("size of array has non-integer type");
		} else if (!c_value_is_const(len)) {
			if (!rcc->active_scope) {
				yy_error("array size must be a constant expression");
			} else {
				attr |= C_ATTR_VLA;
				if (!is_param) {
					c_value_ref(rcc, len);
					if (len->type->kind != c_type_size_t.kind) {
						c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, len);
					}
				} else {
					vla_tokens = len->u.val.ptr;
				}
				length = len->u.ref;
			}
		} else {
			if (IR_IS_TYPE_SIGNED(len->u.type) && len->u.val.i64 < 0) yy_error("array size is negative");
			if (d->type->attr & C_ATTR_VLA) {
				if (len->u.val.u64 > SIZE_MAX) yy_error("array is too large");
				attr |= C_ATTR_VLA;
				length = ir_const_size_t(rcc->active_ctx, len->u.val.u64);
			} else {
				if (len->u.val.u64 != 0 && d->type->size > SIZE_MAX / len->u.val.u64) yy_error("array is too large");
				length = len->u.val.u64;
			}
		}
	}

	type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	type->kind = C_TYPE_ARRAY;
	type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
	if ((d->type->attr & C_ATTR_ALIGN_MASK) && d->type->size && c_attr2align(d->type->attr) > d->type->size) {
		yy_error("alignment of array elements is greater than element size");
	}
	if (attr & C_ATTR_VLA) {
		type->size = d->type->size;
		type->array.length = length;
		type->array.vla_tokens = vla_tokens;
	} else {
		type->size = d->type->size * length;
		type->array.length = length;
		attr |= d->type->attr & C_ATTR_VMT;
	}
	type->attr = attr | (d->attr & C_ARRAY_ATTRS);
	if ((d->type->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= d->type->attr & C_ATTR_ALIGN_MASK;
	}
	type->array.type = d->type;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_ARRAY_ATTRS;
}

const c_type *c_underlying_enum_type(rcc_ctx *rcc, c_dcl *dcl)
{
	const c_type *t = c_resolve_type(rcc, dcl);
    if (!C_IS_TYPE_INT(t)) yy_error("invalid \"enum\" underlying type");
	return t;
}

c_type *c_make_enum_type(rcc_ctx *rcc, c_dcl *d, c_name tag, const c_type *underlying_type)
{
	c_type *type;
	type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	type->kind = C_TYPE_ENUM;
	type->flags = C_TYPE_INCOMPLETE | (rcc->active_scope ? 0 : C_TYPE_GLOBAL);
	type->attr = (d->attr & C_ENUM_ATTRS);
	type->size = 0;
	type->enumeration.tag = tag;
	if (underlying_type) {
		IR_ASSERT(C_IS_TYPE_INT(underlying_type));
		type->enumeration.kind = underlying_type->kind;
		type->size = underlying_type->size;
		type->attr |= underlying_type->attr & C_ATTR_ALIGN_MASK;
	} else {
		type->enumeration.kind = C_TYPE_VOID; /* this is going to be fixed in c_finish_enum_type(); */
	}
	type->enumeration.values = (c_name*)type; /* fake pointer for comparison only */

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_ENUM_ATTRS;

	return type;
}

void c_declare_enum_val(rcc_ctx *rcc, const c_type *type, c_name name, c_dcl *attr, c_value *val,
                        int64_t *min, uint64_t *max, c_value *last)
{
	c_sym *obj;
	const c_type *const_type;

	if (val && c_value_is_set(val)) {
		if (!c_value_is_const(val) || (!C_IS_TYPE_INT(val->type) && val->type->kind != C_TYPE_ENUM)) {
			yy_error_fmt("enumerator value for \"%s\" is not an integer constant", yy_sym2str(rcc, name));
		}
		last->u.type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I64 : IR_U64;
		last->u.val.i64 = val->u.val.i64;
		const_type = val->type;
	} else {
		if (last->u.type == IR_I64) {
			if (last->u.val.i64 == 0x7fffffffffffffffLL) {
				last->u.type = IR_U64;
			}
		} else {
			IR_ASSERT(last->u.type == IR_U64);
			if (last->u.val.u64 == 0xffffffffffffffffULL) {
				yy_warning("incremented enumerator value is not representable in the largest integer type");
			}
		}
		last->u.val.i64++;
		if (last->u.val.i64 <= 0x7fffffffLL) {
			const_type = &c_type_i32;
		} else if (last->u.val.i64 <= 0x7fffffffffffffffLL) {
			const_type = &c_type_i64;
		} else {
			const_type = &c_type_u64;
		}
	}

	if (last->u.type == IR_I64 && last->u.val.i64 < *min) *min = last->u.val.i64;
	if ((last->u.type == IR_U64 || last->u.val.i64 > 0) && last->u.val.u64 > *max) *max = last->u.val.u64;

	attr->type = const_type;
	attr->flags |= C_DCL_ENUM_CONST;
	obj = c_declare(rcc, name, attr);
	IR_ASSERT(obj && obj->kind == C_SYM_CONST);
	c_value_set_const(&obj->value, const_type, last->u.type, last->u.val);
}

void c_finish_enum_type(rcc_ctx *rcc, c_type *type, c_dcl *d, int64_t min, uint64_t max)
{
	IR_ASSERT(type && type->kind == C_TYPE_ENUM);
	type->attr |= d->attr & C_ENUM_ATTRS;
	if (!type->enumeration.kind) {
		if ((type->attr & C_ATTR_PACKED) && min >= 0 && max <= 0xFFULL) {
			type->enumeration.kind = C_TYPE_U8;
			type->size = sizeof(uint8_t);
			type->attr |= c_align2attr(_Alignof(uint8_t));
		} else if ((type->attr & C_ATTR_PACKED) && min >= -0x7FLL-1 && max <= 0x7FULL) {
			type->enumeration.kind = C_TYPE_I8;
			type->size = sizeof(int8_t);
			type->attr |= c_align2attr(_Alignof(int8_t));
		} else if ((type->attr & C_ATTR_PACKED) && min >= 0 && max <= 0xFFFFULL) {
			type->enumeration.kind = C_TYPE_U16;
			type->size = sizeof(uint16_t);
			type->attr |= c_align2attr(_Alignof(uint16_t));
		} else if ((type->attr & C_ATTR_PACKED) && min >= -0x7FFFLL-1 && max <= 0x7FFFULL) {
			type->enumeration.kind = C_TYPE_I16;
			type->size = sizeof(int16_t);
			type->attr |= c_align2attr(_Alignof(int16_t));
		} else if (min >= 0 && max <= 0xFFFFFFFFULL) {
			type->enumeration.kind = C_TYPE_U32;
			type->size = sizeof(uint32_t);
			type->attr |= c_align2attr(_Alignof(uint32_t));
		} else if (min >= 0) {
			type->enumeration.kind = C_TYPE_U64;
			type->size = sizeof(uint64_t);
			type->attr |= c_align2attr(_Alignof(uint64_t));
		} else if (min >= -0x7FFFFFFFLL-1 && max <= 0x7FFFFFFFULL) {
			type->enumeration.kind = C_TYPE_I32;
			type->size = sizeof(int32_t);
			type->attr |= c_align2attr(_Alignof(int32_t));
		} else if (max <= 0x7FFFFFFFFFFFFFFFULL) {
			type->enumeration.kind = C_TYPE_I64;
			type->size = sizeof(int64_t);
			type->attr |= c_align2attr(_Alignof(int64_t));
		} else {
			yy_warning("enumeration values exceed range of largest integer");
			type->enumeration.kind = C_TYPE_I64;
			type->size = sizeof(int64_t);
			type->attr |= c_align2attr(_Alignof(int64_t));
		}
	}

	type->attr &= ~C_ATTR_PACKED;
	if ((d->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= (d->attr & C_ATTR_ALIGN_MASK);
	}
	type->flags &= ~C_TYPE_INCOMPLETE;

	d->type = type;
}

c_type *c_make_struct_type(rcc_ctx *rcc, c_dcl *d, c_name tag)
{
	c_type *type;
	type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	type->kind = (d->flags & C_TYPE_SPEC_UNION) ? C_TYPE_UNION : C_TYPE_STRUCT;
	type->flags = C_TYPE_INCOMPLETE | (rcc->active_scope ? 0 : C_TYPE_GLOBAL);
	type->attr = d->attr & C_STRUCT_ATTRS;
	type->size = 0;
	type->record.tag = tag;
	type->record.fields = NULL;
	type->record.num_fields = 0;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_STRUCT_ATTRS;

	return type;
}

static void c_grow_struct_fields(rcc_ctx *rcc, c_type *type)
{
	if (type->record.num_fields == C_ALLOCA_FIELDS) {
		c_field *ptr = ir_mem_malloc(type->record.num_fields * 2 * sizeof(c_field));
		if (!ptr) yy_error("out of memory");
		memcpy(ptr, type->record.fields, type->record.num_fields * sizeof(c_field));
		type->record.fields = ptr;
	} else if (type->record.num_fields % C_ALLOCA_FIELDS == 0) {
		type->record.fields = ir_mem_realloc(type->record.fields, IR_ALIGNED_SIZE(type->record.num_fields + 1, C_ALLOCA_FIELDS) * sizeof(c_field));
		if (!type->record.fields) yy_error("out of memory");
	}
}

static c_field *c_find_struct_field(const c_type *type, c_name name, size_t *offset)
{
	uint32_t i;
	c_field *f;

	for (i = 0, f = type->record.fields; i < type->record.num_fields; f++, i++) {
		if (f->name == name) {
			*offset = f->offset;
			return f;
		} else if (!f->name && (f->type->kind == C_TYPE_STRUCT || f->type->kind == C_TYPE_UNION)) {
			c_field *f2 = c_find_struct_field(f->type, name, offset);
			if (f2) {
				*offset += f->offset;
				return f2;
			}
		}
	}
	return NULL;
}

void c_static_assert(rcc_ctx *rcc, c_value *expr, c_value *msg)
{
	if (!c_value_is_const(expr)) yy_error("expression in static assertion is not constant");
	if (expr->type->kind == C_TYPE_VOID || expr->type->kind == C_TYPE_STRUCT || expr->type->kind == C_TYPE_UNION) {
		yy_error("expression in static assertion is not scalar");
	}
	if (!c_value_is_true(expr)) {
		if (msg && c_value_is_const(msg)) {
			yy_error_fmt("static assertion failed \"%s\"", (const char *)msg->u.val.ptr);
		} else {
			yy_error("static assertion failed");
		}
	}
}

void c_declare_struct_field(rcc_ctx *rcc, c_type *type, c_name name, c_dcl *field, c_value *bits)
{
	uint32_t i;
	size_t offset;

	IR_ASSERT(type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION);

#ifndef _WIN32
	// TODO: this behavior should be enabled/disabled by -f[no]-ms-extensions (see: gcc/testsuite/gcc.dg/20020527-1.c) ???
	if (!name
	 && field->type
	 && (field->type->kind == C_TYPE_STRUCT || field->type->kind == C_TYPE_UNION)
	 && ((field->flags & C_TYPE_SPEC_NAME) || field->type->tag)) {
		yy_warning("declaration does not declare anything");
		return;
	}
#endif

	c_finalize_type(rcc, field);
	if (field->type->kind == C_TYPE_VOID) yy_error_fmt("field \"%s\" declared void", yy_sym2str(rcc, name));
	if (field->type->kind == C_TYPE_FUNC) yy_error_fmt("field \"%s\" declared as a function", yy_sym2str(rcc, name));
	if (field->type->attr & (C_ATTR_VLA|C_ATTR_VMT)) {
		if (field->type->attr & C_ATTR_VLA) {
			yy_error_fmt("field \"%s\" declared as a variable length array", yy_sym2str(rcc, name));
		} else {
			yy_error_fmt("field \"%s\" declared as a variable modified type", yy_sym2str(rcc, name));
		}
	}
	if ((field->type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, field->type)) {
		yy_error_fmt("field \"%s\" has incomplete type", yy_sym2str(rcc, name));
	}
	if (name && c_find_struct_field(type, name, &offset)) yy_error_fmt("duplicate member \"%s\"", yy_sym2str(rcc, name));

	if (type->record.num_fields >= C_ALLOCA_FIELDS) c_grow_struct_fields(rcc, type);

	i = type->record.num_fields++;
	type->record.fields[i].name = name;
	type->record.fields[i].type = field->type;
	type->record.fields[i].offset = 0;
	if (field->attr & C_ATTR_ALIGN_MASK) {
		type->record.fields[i].offset = c_attr2align(field->attr);
	}
	if (!bits || !c_value_is_set(bits)) {
		type->record.fields[i].bit_field = 0;
	} else {
		if (!c_value_is_const(bits)) yy_error_fmt("bit-field \"%s\" width not an integer constant", yy_sym2str(rcc, name));
		if (!C_IS_TYPE_INT(bits->type)) yy_error_fmt("bit-field \"%s\" width not an integer constant", yy_sym2str(rcc, name));
		if (C_IS_TYPE_SIGNED(bits->type) && bits->u.val.i64 < 0) yy_error_fmt("negative width in bit-field \"%s\"", yy_sym2str(rcc, name));
		if (bits->u.val.i64 == 0 && name) yy_error_fmt("zero width for bit-field \"%s\"", yy_sym2str(rcc, name));
		if (!C_IS_TYPE_INT(field->type) && field->type->kind != C_TYPE_ENUM) {
			yy_error_fmt("bit-field \"%s\" has invalid type", yy_sym2str(rcc, name));
		}
		if (bits->u.val.u64 > field->type->size * 8) yy_error_fmt("width of \"%s\" exceeds its type", yy_sym2str(rcc, name));
		if (field->type->kind == C_TYPE_BOOL && bits->u.val.u64 > 1) yy_error_fmt("width of \"%s\" exceeds its type", yy_sym2str(rcc, name));
		if (bits->u.val.u64 < field->type->size * 8) {
			IR_ASSERT(bits->u.val.u64 < 64);
			type->record.fields[i].bit_field = C_BIT_FIELD(0, bits->u.val.u64);
		} else {
			type->record.fields[i].bit_field = 0;
		}
	}
}

static void c_do_check_nested_redeclarations(rcc_ctx *rcc, const c_type *type, const c_type *nested_type)
{
	uint32_t i;

	for (i = 0; i < nested_type->record.num_fields; i++) {
		c_field *field = &nested_type->record.fields[i];

		if (field->name) {
			size_t offset;
			c_field *field2 = c_find_struct_field(type, field->name, &offset);

			if (field2 && field2 != field) {
				yy_error_fmt("duplicate member \"%s\"", yy_sym2str(rcc, field->name));
			}
		} else if (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT) {
			c_do_check_nested_redeclarations(rcc, type, field->type);
		}
	}
}

#ifdef _WIN32
# define IS_GCC_STRUCT(attr) (((attr) & C_ATTR_GCC_STRUCT) != 0)
#else
# define IS_GCC_STRUCT(attr) (((attr) & C_ATTR_MS_STRUCT) == 0)
#endif

static size_t c_gcc_field_alignment(rcc_ctx *rcc, c_type *type, c_field *field, bool *packed)
{
	size_t align = c_attr2align(field->type->attr);
	size_t a = field->offset;

#ifdef IR_TARGET_X86
# ifdef _WIN32
	if (field->type->size == 8 && C_IS_TYPE_INT(field->type)) {
		align = 4;
	}
# endif
#endif

	if (!align) align = 1;
	if (!C_IS_BIT_FIELD(field->bit_field)
	 || C_BIT_FIELD_SIZE(field->bit_field) != 0) {
		if ((type->attr & C_ATTR_PACKED) || (field->type->attr & C_ATTR_PACKED)) {
			align = 1;
			*packed = 1;
		}
		if (rcc->pp_pack) {
			*packed = 1;
			if (rcc->pp_pack < align) align = rcc->pp_pack;
			if (rcc->pp_pack < a) a = 0;
		}
	}
	if (a) align = a;
	return align;
}

static size_t c_ms_field_alignment(rcc_ctx *rcc, c_type *type, c_field *field)
{
	size_t align = c_attr2align(field->type->attr);
	size_t a = field->offset;

#ifdef IR_TARGET_X86
# ifndef _WIN32
	if (field->type->size == 8 && C_IS_TYPE_INT(field->type)) {
		align = 8;
	}
# endif
#endif

	if (!align) align = 1;
	if ((type->attr & C_ATTR_PACKED) || (field->type->attr & C_ATTR_PACKED)) {
		align = 1;
	}
	if (rcc->pp_pack) {
		if (rcc->pp_pack < align) align = rcc->pp_pack;
		if (rcc->pp_pack < a) a = 0;
	}
	if (a) align = a;
	return align;
}

void c_finish_struct_type(rcc_ctx *rcc, c_type *type, c_dcl *d)
{
	IR_ASSERT(type && (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION));
	type->attr |= d->attr & C_STRUCT_ATTRS;
	if ((d->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= (d->attr & C_ATTR_ALIGN_MASK);
	}

	if (type->record.num_fields) {
		c_field *fields = ir_arena_alloc(&rcc->c_arena, sizeof(c_field) * type->record.num_fields);
		uint32_t i;
		size_t size = 0;
		size_t field_align, struct_align = c_attr2align(type->attr);

		memcpy(fields, type->record.fields, sizeof(c_field) * type->record.num_fields);
		if (type->record.num_fields > C_ALLOCA_FIELDS) ir_mem_free(type->record.fields);
		type->record.fields = fields;
		if (type->kind == C_TYPE_UNION) {
			if (IS_GCC_STRUCT(type->attr)) {
				for (i = 0; i < type->record.num_fields; i++) {
					c_field *field = &type->record.fields[i];
					size_t field_size;
					bool packed = 0;

					if (!field->name
					 && (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT)) {
						c_do_check_nested_redeclarations(rcc, type, field->type);
					}

					field_align = c_gcc_field_alignment(rcc, type, field, &packed);
					field_size = field->type->size;
//???					field_size = C_IS_BIT_FIELD(field->bit_field) ?
//						(size_t)(C_BIT_FIELD_SIZE(field->bit_field) + 7) / 8 : field->type->size;
					field->offset = 0;
					if (field_align > struct_align) struct_align = field_align;
					if (field_size > size) size = field_size;
				}
			} else {
				for (i = 0; i < type->record.num_fields; i++) {
					c_field *field = &type->record.fields[i];

					if (!field->name
					 && (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT)) {
						c_do_check_nested_redeclarations(rcc, type, field->type);
					}

					field_align = c_ms_field_alignment(rcc, type, field);
					field->offset = 0;
					if (field_align > struct_align) struct_align = field_align;
					if (field->type->size > size) size = field->type->size;
				}
			}
			if (struct_align) {
				type->attr &= ~C_ATTR_ALIGN_MASK;
				type->attr |= c_align2attr(struct_align);
			}
			type->size = size;
		} else {
			uint32_t last_offset = 0;
			uint32_t last_bit = 0;

			if (IS_GCC_STRUCT(type->attr)) {
				for (i = 0; i < type->record.num_fields; i++) {
					c_field *field = &type->record.fields[i];
					bool packed = 0;

					if (!field->name
					 && (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT)) {
						c_do_check_nested_redeclarations(rcc, type, field->type);
					}
					if (field->type->attr & C_ATTR_FLEXIBLE) {
						if (type->kind == C_TYPE_UNION) yy_error("flexible array member in union");
						if (i != type->record.num_fields - 1) yy_error("flexible array member not at the end of struct");
					}

					field_align = c_gcc_field_alignment(rcc, type, field, &packed);
					if (!C_IS_BIT_FIELD(field->bit_field)) {
						field->offset = IR_ALIGNED_SIZE(size, field_align);
						last_offset = size = field->offset + field->type->size;
						last_bit = 0;
					} else {
						uint32_t bits = C_BIT_FIELD_SIZE(field->bit_field);
						uint32_t first_bit = 0;

						if (bits == 0 || field->offset) {
							last_offset = IR_ALIGNED_SIZE(size, field_align);
							last_bit = bits;
							if (bits == 0
							 && ((type->attr & C_ATTR_PACKED) || rcc->pp_pack)) {
								/* prevent modification of the struct alignment */
								field_align = 1;
							}
						} else {
							if (!packed
							 && ((last_offset * 8 + last_bit) % (field_align * 8) + bits + (field_align * 8) - 1)
									/ (field_align * 8) > field->type->size / field_align) {
								last_offset = IR_ALIGNED_SIZE(size, field_align);
								last_bit = bits;
							} else {
								first_bit = last_bit;
								while (first_bit > field_align * 8) {
									first_bit -= field_align * 8;
									last_offset += field_align;
								}
								field->bit_field = C_BIT_FIELD(first_bit, bits);
								last_bit = first_bit + bits;
							}
						}
						if (field->type->size == 8 && bits <= 32) {
							field->type = (C_IS_TYPE_SIGNED(field->type)) ? &c_type_i32 : &c_type_u32;
						}
						field->offset = last_offset;
						size = last_offset + ((last_bit) + 7) / 8;
						if (bits
						 && (field->offset * 8 + first_bit) / (field->type->size * 8)
						  != (field->offset * 8 + last_bit - 1) / (field->type->size * 8)) {
							// TODO: try to use a bigger aligned type ???
							C_SET_BIT_FIELD_PACKED(field->bit_field);
						}
					}
					if (field_align > struct_align) struct_align = field_align;
				}
			} else {
				size_t prev_size = (size_t)-1;

				for (i = 0; i < type->record.num_fields; i++) {
					c_field *field = &type->record.fields[i];

					if (!field->name
					 && (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT)) {
						c_do_check_nested_redeclarations(rcc, type, field->type);
					}
					if (field->type->attr & C_ATTR_FLEXIBLE) {
						if (type->kind == C_TYPE_UNION) yy_error("flexible array member in union");
						if (i != type->record.num_fields - 1) yy_error("flexible array member not at the end of struct");
					}

					field_align = c_ms_field_alignment(rcc, type, field);
					if (!C_IS_BIT_FIELD(field->bit_field)) {
						field->offset = IR_ALIGNED_SIZE(size, field_align);
						last_offset = size = field->offset + field->type->size;
						last_bit = 0;
					} else {
						uint32_t bits = C_BIT_FIELD_SIZE(field->bit_field);

						if (last_bit + bits > field->type->size * 8
						 || (bits && (field->type->size != prev_size))) {
							last_offset = IR_ALIGNED_SIZE(size, field_align);
							size = last_offset + field->type->size;
							last_bit = bits;
						} else {
							field->bit_field = C_BIT_FIELD(last_bit, bits);
							last_bit += bits;
						}
						if (!bits) field_align = 1;
						field->offset = last_offset;
						prev_size = bits ? field->type->size : (size_t)-1;
						if (field->type->size == 8 && bits <= 32) {
							field->type = (C_IS_TYPE_SIGNED(field->type)) ? &c_type_i32 : &c_type_u32;
						}
					}
					if (field_align > struct_align) struct_align = field_align;
				}
			}

			if (struct_align) {
				type->attr &= ~C_ATTR_ALIGN_MASK;
				type->attr |= c_align2attr(struct_align);
			}
			type->size = struct_align ? IR_ALIGNED_SIZE(size, struct_align) : size;
		}
	}
	type->flags &= ~(C_TYPE_INCOMPLETE|C_TYPE_INPROGRESS);

	d->type = type;
}

void c_validate_func_params(rcc_ctx *rcc, c_name name, c_dcl *d)
{
	uint32_t i;
	const c_type *type = d->type;

	for (i = 0; i < type->func.num_params; i++) {
		if (!type->func.params[i].type) {
			yy_warning_fmt("type of \"%s\" defaults to \"int\"", yy_sym2str(rcc, type->func.params[i].name));
			type->func.params[i].type = &c_type_i32;
		}
	}
}

static void c_grow_func_params(rcc_ctx *rcc, c_param **params, uint32_t *num_params)
{
	if (*num_params == C_ALLOCA_PARAMS) {
		c_param *ptr = ir_mem_malloc(*num_params * 2 * sizeof(c_param));
		if (!ptr) yy_error("out of memory");
		memcpy(ptr, *params, *num_params * sizeof(c_param));
		*params = ptr;
	} else if (*num_params % C_ALLOCA_FIELDS == 0) {
		*params = ir_mem_realloc(*params, IR_ALIGNED_SIZE(*num_params + 1, C_ALLOCA_PARAMS) * sizeof(c_param));
		if (!*params) yy_error("out of memory");
	}
}

void c_declare_func_param(rcc_ctx *rcc, c_param **params, uint32_t *num_params, c_name name, c_dcl *param)
{
	c_finalize_type(rcc, param);
	if (param->flags & (C_DCL_STORAGE_CLASS-C_DCL_REGISTER)) {
		if (name) {
			yy_error_fmt("storage class specified for parameter \"%s\"", yy_sym2str(rcc, name));
		} else {
			yy_error("storage class specified for parameter");
		}
	} else if (param->type->kind == C_TYPE_VOID) {
		if (name) {
			yy_error_fmt("parameter \"%s\" has void type", yy_sym2str(rcc, name));
		} else if (*num_params) {
			yy_error("\"void\" must be the only parameter");
		}
	} else if (param->attr & C_ATTR_ALIGN_MASK) {
		yy_error("invalid use of \"_Alignas\" for a function parameter");
	} else if (param->type->kind == C_TYPE_FUNC) {
		c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
		type->kind = C_TYPE_POINTER;
		type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
		type->attr = c_align2attr(_Alignof(void*));
		type->size = sizeof(void*);
		type->pointer.type = param->type;
		param->type = type;
	} else if (param->type->kind == C_TYPE_ARRAY) {
		c_type *t = c_create_pointer_type(rcc, param->type->array.type);
		t->attr |= (param->type->attr & C_ATTR_VLA);
		t->array.length = param->type->array.length;
		t->array.vla_tokens = param->type->array.vla_tokens;
		param->type = t;
	}

	if (name) {
		c_sym *sym;

		sym = rcc->yy_hash.data[name].sym;
		if (sym && sym->scope == rcc->active_scope && sym->kind == C_SYM_PARAM) {
			yy_error_fmt("redefinition of parameter \"%s\"", yy_sym2str(rcc, name));
		}

		if (!rcc->active_scope->list.syms) pp_list_init(rcc, &rcc->active_scope->list);
		pp_list_push(&rcc->active_scope->list, name);
		pp_list_push_ptr(&rcc->active_scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));

		sym = ir_arena_alloc(&rcc->c_arena, sizeof(c_sym));
		memset(sym, 0, sizeof(c_sym));
		sym->kind = C_SYM_PARAM;
		sym->scope = rcc->active_scope;
		/* First PARAM has ir_ref == 2 */
		c_value_set_lval(&sym->value, param->type, c_type2ir(rcc, param->type), *num_params + 2);
		rcc->yy_hash.data[name].sym = sym;
	}

	if (*num_params >= C_ALLOCA_PARAMS) c_grow_func_params(rcc, params, num_params);

	(*params)[*num_params].name = name;
	(*params)[*num_params].type = param->type;
	(*num_params)++;
}

void c_declare_func_param_name(rcc_ctx *rcc, c_param **params, uint32_t *num_params, c_name name)
{
	c_sym *sym;

	IR_ASSERT(name);
	sym = rcc->yy_hash.data[name].sym;
	if (sym && sym->scope == rcc->active_scope && sym->kind == C_SYM_PARAM) {
		yy_error_fmt("multiple parameters named \"%s\"", yy_sym2str(rcc, name));
	}

	if (!rcc->active_scope->list.syms) pp_list_init(rcc, &rcc->active_scope->list);
	pp_list_push(&rcc->active_scope->list, name);
	pp_list_push_ptr(&rcc->active_scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));

	sym = ir_arena_alloc(&rcc->c_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	sym->kind = C_SYM_PARAM;
	sym->scope = rcc->active_scope;
	rcc->yy_hash.data[name].sym = sym;

	if (*num_params >= C_ALLOCA_PARAMS) c_grow_func_params(rcc, params, num_params);

	(*params)[*num_params].name = name;
	(*params)[*num_params].type = NULL;
	(*num_params)++;
}

static void c_validate_func_ret_type(rcc_ctx *rcc, const c_type *t)
{
	if (t->kind == C_TYPE_FUNC) yy_error("function returning a function");
	if (t->kind == C_TYPE_ARRAY) yy_error("function returning an array");
}

void c_make_func_type(rcc_ctx *rcc, c_dcl *d, c_param *params, uint32_t num_params, uint32_t attr)
{
	c_type *type;

	c_finalize_type(rcc, d);
	c_validate_func_ret_type(rcc, d->type);

	if (num_params) {
		if (params[0].type && params[0].type->kind == C_TYPE_VOID) {
			if (num_params != 1) yy_error("\"void\" must be the only parameter");
			num_params = 0;
		}
	}
	if (num_params > 0) {
		c_param *ptr = ir_arena_alloc(&rcc->c_arena, sizeof(c_param) * num_params);
		memcpy(ptr, params, sizeof(c_param) * num_params);
		if (num_params > C_ALLOCA_PARAMS) ir_mem_free(params);
		params = ptr;
	} else {
		params = NULL;
	}

	type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
	type->size = sizeof(void*);
	type->attr = d->attr & C_FUNC_TYPE_ATTRS;
	type->attr |= attr & C_FUNC_TYPE_ATTRS;
	type->func.ret_type = d->type;
	type->func.num_params = num_params;
	type->func.params = params;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_FUNC_TYPE_ATTRS;
}

void c_declare_func_param_type(rcc_ctx *rcc, const c_type *type, c_name name, c_dcl *param)
{
	uint32_t i;

	IR_ASSERT(type->kind == C_TYPE_FUNC);
	IR_ASSERT(name);
	c_finalize_type(rcc, param);
	if (param->flags & (C_DCL_STORAGE_CLASS-C_DCL_REGISTER)) {
		yy_error_fmt("storage class specified for parameter \"%s\"", yy_sym2str(rcc, name));
	} else if (param->type->kind == C_TYPE_VOID) {
		yy_error_fmt("parameter \"%s\" has void type", yy_sym2str(rcc, name));
	} else if (param->attr & C_ATTR_ALIGN_MASK) {
		yy_error("invalid use of \"_Alignas\" for a function parameter");
	} else if (param->type->kind == C_TYPE_FUNC) {
		c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
		type->kind = C_TYPE_POINTER;
		type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
		type->attr = c_align2attr(_Alignof(void*));
		type->size = sizeof(void*);
		type->pointer.type = param->type;
		param->type = type;
	} else if (param->type->kind == C_TYPE_ARRAY) {
		param->type = c_create_pointer_type(rcc, param->type->array.type);
	}
    if (type->func.num_params > 0) {
		for (i = 0; i < type->func.num_params; i++) {
			if (type->func.params[i].name == name) {
				if (type->func.params[i].type) {
					yy_error_fmt("redefinition of parameter \"%s\"", yy_sym2str(rcc, name));
				}
				type->func.params[i].type = param->type;
				return;
			}
		}
	}
	yy_error_fmt("declaration for parameter \"%s\" but no such parameter", yy_sym2str(rcc, name));
}

static void c_fix_nested_type(rcc_ctx *rcc, const c_type *t, c_type *nested)
{
	switch (nested->kind) {
		case C_TYPE_POINTER:
			/* "char" is used as a terminator of nested declaration */
			if (nested->pointer.type == &c_type_char) {
				nested->pointer.type = t;
				c_validate_pointer_type(t);
				if ((t->attr & (C_ATTR_VLA|C_ATTR_VMT)) && !(nested->attr & C_ATTR_VMT)) {
					nested->attr |= C_ATTR_VMT;
				}
			} else {
				c_fix_nested_type(rcc, t, (c_type*)nested->pointer.type);
				if ((nested->pointer.type->attr & (C_ATTR_VLA|C_ATTR_VMT)) && !(nested->attr & C_ATTR_VMT)) {
					nested->attr |= C_ATTR_VMT;
				}
			}
			break;
		case C_TYPE_ARRAY:
			/* "char" is used as a terminator of nested declaration */
			if (nested->array.type == &c_type_char) {
				nested->array.type = t;
				c_validate_array_element_type(rcc, t);
				if (t->attr & C_ATTR_VLA) {
					if (!(nested->attr & C_ATTR_VLA)) {
						nested->attr |= C_ATTR_VLA;
						nested->array.length = ir_const_size_t(rcc->active_ctx, nested->array.length);
					}
				} else if ((t->attr & C_ATTR_VMT) && !(nested->attr & C_ATTR_VMT)) {
					nested->attr |= C_ATTR_VMT;
				}
			} else {
				c_fix_nested_type(rcc, t, (c_type*)nested->array.type);
				if (nested->array.type->attr & C_ATTR_VLA) {
					if (!(nested->attr & C_ATTR_VLA)) {
						nested->attr |= C_ATTR_VLA;
						nested->array.length = ir_const_size_t(rcc->active_ctx, nested->array.length);
					}
				} else if ((nested->array.type->attr & C_ATTR_VMT) && !(nested->attr & C_ATTR_VMT)) {
					nested->attr |= C_ATTR_VMT;
				}
			}
			if (!(nested->attr & C_ATTR_VLA)) {
				nested->size = nested->array.length * nested->array.type->size;
			} else {
				nested->size = nested->array.type->size;
			}
			nested->attr &= ~C_ATTR_ALIGN_MASK;
			nested->attr |= nested->array.type->attr & C_ATTR_ALIGN_MASK;
			break;
		case C_TYPE_FUNC:
			/* "char" is used as a terminator of nested declaration */
			if (nested->func.ret_type == &c_type_char) {
				nested->func.ret_type = t;
			} else {
				c_fix_nested_type(rcc, t, (c_type*)nested->func.ret_type);
			}
			break;
		default:
			IR_ASSERT(0);
	}
}

void c_make_nested_type(rcc_ctx *rcc, c_dcl *d, c_dcl *nested)
{
	c_finalize_type(rcc, d);
	if (nested->type && nested->type != &c_type_char) {
		c_fix_nested_type(rcc, d->type, (c_type*)nested->type);
		d->type = nested->type;
	}
}

void c_gcc_attribute_aligned(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *val)
{
	if (!c_value_is_set(val)) {
		// TODO: ???
	} else if (c_valid_alignment(rcc, val, "attribute ", YY_ALIGNED)) {
		if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");
		d->attr |= c_align2attr(val->u.val.u64);
	}
}

void c_gcc_attribute_packed(rcc_ctx *rcc, c_dcl *d, c_name attr)
{
	if ((d->flags & (C_TYPE_SPEC_ENUM|C_TYPE_SPEC_STRUCT|C_TYPE_SPEC_UNION))
	 || ((d->flags & C_TYPE_SPEC_ANY) == C_TYPE_SPEC_TYPE
	  && (d->type->kind == C_TYPE_ENUM || d->type->kind == C_TYPE_STRUCT || d->type->kind == C_TYPE_UNION)
	  && (d->type->flags & C_TYPE_INCOMPLETE))) {
		d->attr |= C_ATTR_PACKED;
	} else {
		yy_warning_fmt("\"%s\" attribure ignored", yy_sym2str(rcc, attr));
	}
}

void c_gcc_attribute_cleanup(rcc_ctx *rcc, c_dcl *d, c_name attr, c_name func)
{
	if (d->cleanup_func && d->cleanup_func != func) {
		yy_error_fmt("duplicate __attribure__((%s))", yy_sym2str(rcc, attr));
	}

	d->cleanup_func = func;
}

void c_gcc_attribute_regparm(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *val)
{
	if (!c_value_is_set(val) || !c_value_is_const(val) || !C_IS_TYPE_INT(val->type)) {
		yy_warning("attribute \"regparm\" value must be an integer constant");
	}
#if defined(IR_TARGET_X86)
	if (val->u.val.u32 == 1) {
		if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_REGPARM_1) {
			yy_error("multiple calling conventions");
		}
		d->attr |= C_ATTR_CC_REGPARM_1;
		return;
	} else if (val->u.val.u32 == 2) {
		if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_REGPARM_2) {
			yy_error("multiple calling conventions");
		}
		d->attr |= C_ATTR_CC_REGPARM_2;
		return;
	} else if (val->u.val.u32 == 3) {
		if ((d->attr & C_ATTR_CALL_CONV) && (d->attr & C_ATTR_CALL_CONV) != C_ATTR_CC_REGPARM_3) {
			yy_error("multiple calling conventions");
		}
		d->attr |= C_ATTR_CC_REGPARM_3;
		return;
	}
#endif
	yy_warning_ex_fmt(E_UNSUPPORTED, "unsupported attribure \"%s(%d)\"", yy_sym2str(rcc, attr), val->u.val.u32);
}

void c_gcc_attribute_vector_size(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *val)
{
	if (!c_value_is_set(val)
	 || !c_value_is_const(val)
	 || !C_IS_TYPE_INT(val->type)
	 || val->u.val.u64 == 0
	 || (C_IS_TYPE_SIGNED(val->type) && val->u.val.i64 < 0)) {
		yy_error_fmt("attribute \"%s\" value must be a positive integer constant", yy_sym2str(rcc, attr));
	} else if ((val->u.val.u64 & (val->u.val.u64 - 1)) != 0) {
		yy_error_fmt("attribute \"%s\" value must be a power of two", yy_sym2str(rcc, attr));
	}

	d->vector_size = val->u.val.u32;
}

yy_sym c_gcc_attribute(rcc_ctx *rcc, c_dcl *d, c_name attr, yy_sym sym)
{
	if (attr == YY_FORMAT
	 || attr == YY___FORMAT__
	 || attr == YY_FORMAT_ARG
	 || attr == YY___FORMAT_ARG__
	 || attr == YY_MALLOC
	 || attr == YY___MALLOC__
	 || attr == YY_NONNULL
	 || attr == YY___NONNULL__
	 || attr == YY_WARN_UNUSED_RESULT
	 || attr == YY___WARN_UNUSED_RESULT__
	) {
		/* silently ignore some known attributes */
	} else {
		yy_warning_ex_fmt(E_UNSUPPORTED, "unsupported attribure \"%s\"", yy_sym2str(rcc, attr));
	}

	if (sym == YY__LPAREN) {
		int level = 0;

		while (1) {
			sym = yy_next(rcc);
			if (sym == YY__RPAREN) {
				if (level == 0) return yy_next(rcc);
				level--;
			} else if (sym == YY__LPAREN) {
				level++;
			} else if (sym == YY_EOF) {
				yy_error("unexpected <EOF>");
				return  YY_EOF;
			}
		}
	}
	return sym;
}

void c_gcc_attribute_alias(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *val)
{
	if (d->alias) {
		yy_error("duplicate __attribure__((alias())) or __asm__() specifiers");
	}

	if ((d->flags & C_DCL_STORAGE_CLASS) == C_DCL_EXTERN
	 || (!(d->flags & C_DCL_STORAGE_CLASS) && d->type && d->type->kind == C_TYPE_FUNC)) {
		c_name id = yy_hash_find(rcc, (const char*)val->u.val.ptr, val->u.op1 - 1);

		if (!id
		 || !rcc->yy_hash.data[id].sym
		 || (rcc->yy_hash.data[id].sym->kind != C_SYM_FUNC && rcc->yy_hash.data[id].sym->kind != C_SYM_VAR)
		 || (rcc->yy_hash.data[id].sym->linkage != C_LINK_EXTERNAL && rcc->yy_hash.data[id].sym->linkage != C_LINK_INTERNAL)) {
			yy_error_fmt("alias to undefined symbol \"%s\"", (const char*)val->u.val.ptr);
		} else if (!rcc->yy_hash.data[id].sym->is_implemented) {
			yy_error("alias must point to a variable or function defined in the same file");
		}
		d->alias = rcc->yy_hash.data[id].sym->alias ? rcc->yy_hash.data[id].sym->alias : id;
	} else {
		yy_warning_fmt("ignoring __attribute__((%s(\"%s\")))", yy_sym2str(rcc, attr), (const char*)val->u.val.ptr);
	}
}

static int8_t c_parse_reg_name(rcc_ctx *rcc, const char *str, const char *end, bool variable)
{
	int8_t reg = 0;
	const char *s = str;

#if defined(IR_TARGET_X64)
	if (*s == '%') s++;
	if (*s == 'r') {
		s++;
		if (*s >= '0' && *s <= '9') {
			reg = *s - '0';
			s++;
			if (*s >= '0' && *s <= '9') {
				reg = reg * 10 + (*s - '0');
				if (reg > 15) goto error;
				s++;
				if (*s == 'd' || *s == 'w' || *s == 'b') s++;
			}
		} else if (s[0] == 's' && s[1] == 'p') {
			reg = 4;
			s += 2;
		} else if (s[0] == 'a' && s[1] == 'x') {
			reg = 0;
			s += 2;
		} else if (s[0] == 'c' && s[1] == 'x') {
			reg = 1;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'x') {
			reg = 2;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'x') {
			reg = 3;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'p') {
			reg = 5;
			s += 2;
		} else if (s[0] == 's' && s[1] == 'i') {
			reg = 6;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'i') {
			reg = 7;
			s += 2;
		} else {
			goto error;
		}
	} else if (s[0] == 'x' && s[1] == 'm' && s[2] == 'm' && s[3] >= '0' && s[3] <= '9') {
		reg = s[3] - '0';
		s += 4;
		if (*s >= '0' && *s <= '9') {
			reg = reg * 10 + (*s - '0');
			if (reg > 15) goto error;
			s++;
		}
		reg += IR_REG_FP_FIRST;
	} else if (*s == 'e') {
		s++;
		if (s[0] == 's' && s[1] == 'p') {
			reg = 4;
			s += 2;
		} else if (s[0] == 'a' && s[1] == 'x') {
			reg = 0;
			s += 2;
		} else if (s[0] == 'c' && s[1] == 'x') {
			reg = 1;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'x') {
			reg = 2;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'x') {
			reg = 3;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'p') {
			reg = 5;
			s += 2;
		} else if (s[0] == 's' && s[1] == 'i') {
			reg = 6;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'i') {
			reg = 7;
			s += 2;
		} else {
			goto error;
		}
	} else if (s[0] == 'a' && s[1] == 'x') {
		reg = 0;
		s += 2;
	} else if (s[0] == 'c' && s[1] == 'x') {
		reg = 1;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'x') {
		reg = 2;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'x') {
		reg = 3;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'p') {
		reg = 5;
		s += 2;
	} else if (s[0] == 's' && s[1] == 'i') {
		reg = 6;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'i') {
		reg = 7;
		s += 2;
	} else if (s[0] == 'a' && s[1] == 'l') {
		reg = 0;
		s += 2;
	} else if (s[0] == 'c' && s[1] == 'l') {
		reg = 1;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'l') {
		reg = 2;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'l') {
		reg = 3;
		s += 2;
	} else {
		goto error;
	}
	if (s != end && *s != 0) goto error;

	if (variable) {
		if (reg == 4) yy_error_fmt("cannot use register \"%s\" for variable", str);
	}

	return reg;

error:
#elif defined(IR_TARGET_X86)
	if (*s == '%') s++;
	if (*s == 'r' && s[1] >= '0' && s[1] <= '9') {
		reg = s[1] - '0';
		s += 2;
		if (reg > 7) goto error;
	} else if (s[0] == 'x' && s[1] == 'm' && s[2] == 'm' && s[3] >= '0' && s[3] <= '9') {
		reg = s[3] - '0';
		s += 4;
		if (reg > 7) goto error;
		reg += IR_REG_FP_FIRST;
	} else if (*s == 'e') {
		s++;
		if (s[0] == 's' && s[1] == 'p') {
			reg = 4;
			s += 2;
		} else if (s[0] == 'a' && s[1] == 'x') {
			reg = 0;
			s += 2;
		} else if (s[0] == 'c' && s[1] == 'x') {
			reg = 1;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'x') {
			reg = 2;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'x') {
			reg = 3;
			s += 2;
		} else if (s[0] == 'b' && s[1] == 'p') {
			reg = 5;
			s += 2;
		} else if (s[0] == 's' && s[1] == 'i') {
			reg = 6;
			s += 2;
		} else if (s[0] == 'd' && s[1] == 'i') {
			reg = 7;
			s += 2;
		} else {
			goto error;
		}
	} else if (s[0] == 'a' && s[1] == 'x') {
		reg = 0;
		s += 2;
	} else if (s[0] == 'c' && s[1] == 'x') {
		reg = 1;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'x') {
		reg = 2;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'x') {
		reg = 3;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'p') {
		reg = 5;
		s += 2;
	} else if (s[0] == 's' && s[1] == 'i') {
		reg = 6;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'i') {
		reg = 7;
		s += 2;
	} else if (s[0] == 'a' && s[1] == 'l') {
		reg = 0;
		s += 2;
	} else if (s[0] == 'c' && s[1] == 'l') {
		reg = 1;
		s += 2;
	} else if (s[0] == 'd' && s[1] == 'l') {
		reg = 2;
		s += 2;
	} else if (s[0] == 'b' && s[1] == 'l') {
		reg = 3;
		s += 2;
	} else {
		goto error;
	}
	if (s != end && *s != 0) goto error;

	if (variable) {
		if (reg == 4) yy_error_fmt("cannot use register \"%s\" for variable", str);
	}

	return reg;

error:
#elif defined(IR_TARGET_AARCH64)
	if ((s[0] == 'x' || s[0] == 'w')  && s[1] >= '0' && s[1] <= '9') {
		reg = s[1] - '0';
		s += 2;
		if (*s >= '0' && *s <= '9') {
			reg = reg * 10 + (*s - '0');
			if (reg > 31) goto error;
			s++;
		}
	} else if ((s[0] == 'd' || s[0] == 's' || s[0] == 'h' || s[0] == 'b')  && s[1] >= '0' && s[1] <= '9') {
		reg = s[1] - '0';
		s += 2;
		if (*s >= '0' && *s <= '9') {
			reg = reg * 10 + (*s - '0');
			if (reg > 31) goto error;
			s++;
		}
		reg += IR_REG_FP_FIRST;
	} else {
		goto error;
	}
	if (s != end && *s != 0) goto error;

	if (variable) {
		if (reg == IR_REG_INT_TMP
		 || reg == IR_REG_X18
		 || (reg >= IR_REG_X29 && reg <= IR_REG_X31)) {
			yy_error_fmt("cannot use register \"%s\" for variable", str);
		}
	}

	return reg;

error:
#endif
	if (!end) yy_error_fmt("invalid register \"%s\"", str);
	return IR_REG_NONE;
}

void c_asm_alias(rcc_ctx *rcc, c_dcl *d, c_value *val)
{
	if (d->alias) {
		yy_error("duplicate __attribure__((alias())) or __asm__() specifiers");
	}

	if ((d->flags & C_DCL_STORAGE_CLASS) == C_DCL_REGISTER) {
		d->flags |= C_DCL_REG_VAR;
		d->reg = c_parse_reg_name(rcc, (const char*)val->u.val.ptr, NULL, 1);
	} else if ((d->flags & (C_DCL_EXTERN|C_DCL_STATIC))
	 || !rcc->active_scope
	 || (d->type && d->type->kind == C_TYPE_FUNC)) {
		c_name id = yy_hash_lookup(rcc, (const char*)val->u.val.ptr, val->u.op1 - 1);

		d->flags |= C_DCL_HAS_ASM_NAME;
		d->alias = id;
		if (rcc->yy_hash.data[id].link) {
			rcc->yy_hash.data[id].link->is_asm_name = 1;
		} else {
			c_linker_sym *link = ir_arena_alloc(&rcc->yy_arena, sizeof(c_linker_sym));
			link->addr = NULL;
			link->reloc = NULL;
			link->is_thunk = 0;
			link->is_asm_name = 1;
			rcc->yy_hash.data[id].link = link;
		}
	} else {
		yy_warning_fmt("ignoring __asm__(\"%s\") specifier for non-static local variable", (const char*)val->u.val.ptr);
	}
}

void c_declspec_align(rcc_ctx *rcc, c_dcl *d, c_value *val)
{
	if (c_value_is_set(val) && c_valid_alignment(rcc, val, "declspec ", YY_ALIGN)) {
		if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");
		d->attr |= c_align2attr(val->u.val.u64);
	}
}

yy_sym c_declspec(rcc_ctx *rcc, c_dcl *d, c_name attr, yy_sym sym)
{
	if (attr == YY_DEPRECATED) {
		d->attr |= C_ATTR_DEPRECATED;
		/* ignore error message */
	} else if (attr == YY_NOINLINE) {
		d->attr |= C_ATTR_NOINLINE;
		return sym;
	} else if (attr == YY_NORETURN) {
		if (!(d->flags & C_DCL_TYPEDEF) || !d->type) d->attr |= C_ATTR_NORETURN;
		return sym;
	} else if (attr == YY_NOTHROW) {
		d->attr |= C_ATTR_NOTHROW;
		return sym;
	} else if (attr == YY_RESTRICT) {
		// TODO: temporary commentd to avoid "invalid use of restrict" error ???
		//d->attr |= C_ATTR_RESTRICT;
		return sym;
	} else if (attr == YY_DLLEXPORT) {
		/* silently ignore */
		return sym;
	} else if (attr == YY_DLLIMPORT) {
		/* silently ignore */
		return sym;
	} else {
		yy_warning_ex_fmt(E_UNSUPPORTED, "unsupported \"__declspec(%s)\"", yy_sym2str(rcc, attr));
	}

	if (sym == YY__LPAREN) {
		int level = 0;

		while (1) {
			sym = yy_next(rcc);
			if (sym == YY__RPAREN) {
				if (level == 0) return yy_next(rcc);
				level--;
			} else if (sym == YY__LPAREN) {
				level++;
			} else if (sym == YY_EOF) {
				yy_error("unexpected <EOF>");
				return  YY_EOF;
			}
		}
	}
	return sym;
}

void c_sizeof_type(rcc_ctx *rcc, c_value *res, const c_type *type)
{
	ir_val val;

	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type)) {
		yy_error_fmt("invalid application of \"%s\" to incomplete type", "sizeof");
	} else if (type->attr & C_ATTR_VLA) {
		IR_ASSERT(type->kind == C_TYPE_ARRAY);
		c_value_set_rval(res, &c_type_size_t, IR_SIZE_T, c_type_size(rcc, type));
		return;
	}
	val.u64 = type->size;
	c_value_set_const(res, &c_type_size_t, IR_SIZE_T, val);
}

ir_ref c_do_nocode(rcc_ctx *rcc)
{
	ir_ref old_control = rcc->active_ctx->control;
	rcc->active_ctx->control = IR_UNUSED;
	ir_BEGIN(IR_UNUSED);
	return old_control;
}

void c_do_end_nocode(rcc_ctx *rcc, ir_ref old_control)
{
	if (rcc->active_ctx->control == rcc->active_ctx->insns_count - 1
	 && rcc->active_ctx->ir_base[rcc->active_ctx->control].op == IR_BEGIN
	 && rcc->active_ctx->ir_base[rcc->active_ctx->control].op1 == IR_UNUSED) {
		rcc->active_ctx->insns_count--;
	} else {
		ir_END();
		// TODO: cleanup dead code ???
	}
	rcc->active_ctx->control = old_control;
}

void c_sizeof_expr(rcc_ctx *rcc, yy_sym op, c_value *expr, ir_ref old_control)
{
	ir_val val;

	if (op == YY_SIZEOF) {
		if (C_IS_BIT_FIELD(expr->u.proto)) {
			yy_error("\"sizeof\" applied to a bit-field");
		} else if (c_value_is_const_str(expr)) {
			val.u64 = c_value_str_size(expr);
		} else if (expr->type->attr & C_ATTR_FLEXIBLE) {
			yy_error_fmt("invalid application of \"%s\" to incomplete type", "sizeof");
		} else if (expr->type->attr & C_ATTR_VLA) {
			IR_ASSERT(expr->type->kind == C_TYPE_ARRAY);
			c_do_end_nocode(rcc, old_control);
			c_value_set_rval(expr, &c_type_size_t, IR_SIZE_T, c_type_size(rcc, expr->type));
			return;
		} else {
			if (expr->type->kind == C_TYPE_BOOL
			 && c_value_is_ref(expr)
			 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_LOAD
			 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_LOAD_v
			 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_VLOAD
			 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_VLOAD_v) {
				/* IR uses 1-byte "bool" for computation, but C assumes 4-byte "int" */
				val.u64 = 4;
			} else {
				val.u64 = expr->type->size;
			}
		}
	} else {
		IR_ASSERT(op == YY__ALIGNOF || op == YY___ALIGNOF || op == YY___ALIGNOF__);
		if (C_IS_BIT_FIELD(expr->u.proto)) {
			yy_error_fmt("\"%s\" applied to a bit-field", yy_sym2str(rcc, op));
		} else if (expr->type->kind == C_TYPE_BOOL
		 && c_value_is_ref(expr)
		 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_LOAD
		 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_LOAD_v
		 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_VLOAD
		 && rcc->active_ctx->ir_base[expr->u.ref].op != IR_VLOAD_v) {
			/* IR uses 1-byte "bool" for computation, but C assumes 4-byte "int" */
			val.u64 = 4;
		} else {
			val.u64 = c_attr2align(expr->type->attr);
		}
	}
	c_value_set_const(expr, &c_type_size_t, IR_SIZE_T, val);
	c_do_end_nocode(rcc, old_control);
}

void c_alignof_type(rcc_ctx *rcc, c_value *res, const c_type *type)
{
	ir_val val;

	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type)) {
		yy_error_fmt("invalid application of \"%s\" to incomplete type", "_Alignof");
	}
	val.u64 = c_attr2align(type->attr);
	c_value_set_const(res, &c_type_size_t, IR_SIZE_T, val);
}

void c_alignas_expr(rcc_ctx *rcc, c_dcl *dcl, c_value *expr)
{
	if (c_valid_alignment(rcc, expr, "", YY__ALIGNAS)) {
		dcl->attr |= c_align2attr(expr->u.val.u64);
	}
}

const c_type *c_typeof_expr(rcc_ctx *rcc, c_value *expr, ir_ref old_control)
{
	if (C_IS_BIT_FIELD(expr->u.proto)) {
		yy_error("\"typeof\" applied to a bit-field");
	}
	c_do_end_nocode(rcc, old_control);
	return expr->type;
}

static c_label *c_new_label(rcc_ctx *rcc, c_name name, c_scope *scope, c_label *label, bool local)
{
	IR_ASSERT(scope);
	if (!scope->list.syms) pp_list_init(rcc, &scope->list);
	pp_list_push(&scope->list, name);
	pp_list_push_ptr(&scope->list, (void*)(((uintptr_t)label) | C_POP_LABEL));

	if (local) {
		label = ir_arena_alloc(&rcc->c_arena, sizeof(c_label));
		label->is_local = 1;
	} else {
		label = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_label));
		label->is_local = 0;
	}
	label->is_unused = 0;
	label->dst = IR_UNUSED;
	label->src_list = IR_UNUSED;
	label->vla_block = IR_UNUSED;
	label->value_sym = IR_UNUSED;
	label->value_block = IR_UNUSED;
	label->cleanup_sym = NULL;
	label->scope = scope;
	rcc->yy_hash.data[name].label = label;
	return label;
}

void c_declare_local_label(rcc_ctx *rcc, c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = rcc->yy_hash.data[name].label;
	if (label && label->scope == rcc->active_scope) {
		yy_error_fmt("duplicate label declaration \"%s\"", yy_sym2str(rcc, name));
		return;
	}

	c_new_label(rcc, name, rcc->active_scope, label, 1);
}

static const c_type *c_type_by_kind(c_type_kind kind)
{
	switch (kind) {
		case C_TYPE_VOID:    return &c_type_void;
		case C_TYPE_BOOL:    return &c_type_bool;
		case C_TYPE_U8:      return &c_type_u8;
		case C_TYPE_U16:     return &c_type_u16;
		case C_TYPE_U32:     return &c_type_u32;
		case C_TYPE_UL:      return &c_type_ul;
		case C_TYPE_ULL:     return &c_type_ull;
		case C_TYPE_CHAR:    return &c_type_char;
		case C_TYPE_I8:      return &c_type_i8;
		case C_TYPE_I16:     return &c_type_i16;
		case C_TYPE_I32:     return &c_type_i32;
		case C_TYPE_IL:      return &c_type_il;
		case C_TYPE_ILL:     return &c_type_ill;
		case C_TYPE_FLOAT:   return &c_type_float;
		case C_TYPE_DOUBLE:  return &c_type_double;
		default: IR_ASSERT(0); return NULL;
	}
}

static void ir_memcpy(rcc_ctx *rcc, ir_ref dst, ir_ref src, ir_ref size, uint32_t align)
{
	if (IR_IS_CONST_REF(size)) {
		ir_insn *size_insn = &rcc->active_ctx->ir_base[size];

		if (!IR_IS_SYM_CONST(size_insn->op)) {
			if (size_insn->val.u64 == 1) {
				ir_STORE(dst, ir_LOAD_U8(src));
				return;
			} else if (size_insn->val.u64 == 2) {
				if (align >= 2) {
					ir_STORE(dst, ir_LOAD_U16(src));
					return;
				}
			} else if (size_insn->val.u64 == 4) {
				if (align >= 4) {
					ir_STORE(dst, ir_LOAD_U32(src));
					return;
				}
			} else if (size_insn->val.u64 == 8) {
				if (align >= 8) {
					ir_STORE(dst, ir_LOAD_U64(src));
					return;
				}
			}
		}
	}
	ir_CALL_3(IR_VOID,
		ir_const_func(rcc->active_ctx, IR_EXT_STR(YY_MEMCPY),
			ir_proto_3(rcc->active_ctx, 0, IR_ADDR, IR_ADDR, IR_ADDR, IR_SIZE_T)),
		dst, src, size);
	rcc->c_last_call_func_type = NULL;
}

static void ir_memzero(rcc_ctx *rcc, ir_ref dst, ir_ref size, uint32_t align)
{
	if (IR_IS_CONST_REF(size)) {
		ir_insn *size_insn = &rcc->active_ctx->ir_base[size];

		if (!IR_IS_SYM_CONST(size_insn->op)) {
			if (size_insn->val.u64 == 1) {
				ir_STORE(dst, ir_const_u8(rcc->active_ctx, 0));
				return;
			} else if (size_insn->val.u64 == 2) {
				if (align >= 2) {
					ir_STORE(dst, ir_const_u16(rcc->active_ctx, 0));
					return;
				}
			} else if (size_insn->val.u64 == 4) {
				if (align >= 4) {
					ir_STORE(dst, ir_const_u32(rcc->active_ctx, 0));
					return;
				}
			} else if (size_insn->val.u64 == 8) {
				if (align >= 8) {
					ir_STORE(dst, ir_const_u64(rcc->active_ctx, 0));
					return;
				}
			}
		}
	}
	ir_CALL_3(IR_VOID,
		ir_const_func(rcc->active_ctx, IR_EXT_STR(YY_MEMSET),
			ir_proto_3(rcc->active_ctx, 0, IR_ADDR, IR_ADDR, IR_I32, IR_SIZE_T)),
		dst, ir_const_i32(rcc->active_ctx, 0), size);
	rcc->c_last_call_func_type = NULL;
}

ir_ref c_do_alloca(rcc_ctx *rcc, size_t size, uint32_t align, bool zero)
{
	ir_ref size_ref = (size == (size_t)-1) ? IR_UNUSED : ir_const_size_t(rcc->active_ctx, size);
	ir_ref ref;
	ir_ref old_control = IR_UNUSED;

	if (rcc->c_prologue_end) {
		old_control = rcc->active_ctx->control;
		rcc->active_ctx->control = rcc->active_ctx->ir_base[rcc->c_prologue_end].op1;
	}

	ref = ir_ALLOCA(size_ref);
	if (zero) {
		ir_memzero(rcc, ref, size_ref, align);
	}

	if (rcc->c_prologue_end) {
		rcc->active_ctx->ir_base[rcc->c_prologue_end].op1 = rcc->active_ctx->control;
		rcc->active_ctx->control = old_control;
	}

	return ref;
}

static void c_do_load_bit_field(rcc_ctx *rcc, c_value *val, uint32_t first_bit, uint32_t bits)
{
	ir_type type;
	ir_ref ref;
	ir_val v;

	if (first_bit + bits <= 8) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I8 : IR_U8;
	} else if (first_bit + bits <= 16) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I16 : IR_U16;
	} else if (first_bit + bits <= 32) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I32 : IR_U32;
	} else {
		IR_ASSERT(first_bit + bits <= 64);
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I64 : IR_U64;
	}

	ref = ir_LOAD(type, val->u.ref);
	if (IR_IS_TYPE_SIGNED(val->u.type) && val->type->kind != C_TYPE_ENUM) {
		if (ir_type_size[val->u.type] > ir_type_size[type]) {
			ref = ir_SEXT(val->u.type, ref);
			type = val->u.type;
		}
		if (ir_type_size[type] * 8 != (first_bit + bits)) {
			v.u64 = ir_type_size[type] * 8 - (first_bit + bits);
			IR_ASSERT(v.u64 < ir_type_size[type] * 8);
			ref = ir_SHL(type, ref, ir_const(rcc->active_ctx, v, type));
		}
		if (ir_type_size[type] * 8 != bits) {
			v.u64 = ir_type_size[type] * 8 - bits;
			IR_ASSERT(v.u64 < ir_type_size[type] * 8);
			ref = ir_SAR(type, ref, ir_const(rcc->active_ctx, v, type));
		}
	} else {
		if (ir_type_size[val->u.type] > ir_type_size[type]) {
			ref = ir_ZEXT(val->u.type, ref);
			type = val->u.type;
		}
		if (ir_type_size[type] * 8 == (first_bit + bits)) {
			if (ir_type_size[type] * 8 != bits) {
				v.u64 = ir_type_size[type] * 8 - bits;
				IR_ASSERT(v.u64 < ir_type_size[type] * 8);
				ref = ir_SHR(type, ref, ir_const(rcc->active_ctx, v, type));
			}
		} else if (first_bit == 0 && bits <= 32) { // use AND instead of SHL+SHR if small immediate (AArch64) ???
			v.u64 = (1ULL<<bits)-1;
			ref = ir_AND(type, ref, ir_const(rcc->active_ctx, v, type));
		} else {
			v.u64 = ir_type_size[type] * 8 - (first_bit + bits);
			IR_ASSERT(v.u64 < ir_type_size[type] * 8);
			ref = ir_SHL(type, ref, ir_const(rcc->active_ctx, v, type));
			if (ir_type_size[type] * 8 != bits) {
				v.u64 = ir_type_size[type] * 8 - bits;
				IR_ASSERT(v.u64 < ir_type_size[type] * 8);
				ref = ir_SHR(type, ref, ir_const(rcc->active_ctx, v, type));
			}
		}
	}

	if (val->u.type != type) {
		if (ir_type_size[val->u.type] == ir_type_size[type]) {
			ref = ir_BITCAST(val->u.type, ref);
		} else {
			ref = ir_TRUNC(val->u.type, ref);
		}
	}

	val->u.ref = ref;

#if 0
	// TODO: use signed type to avoud magic in c_common_type() ???
	if (val->type->kind != C_TYPE_ENUM && bits < 32 && IR_IS_TYPE_UNSIGNED(val->u.type)) {
		if (val->u.type == IR_U8) {
			val->type = &c_type_i8;
			val->u.type = IR_I8;
		} else if (val->u.type == IR_U16) {
			val->type = &c_type_i16;
			val->u.type = IR_I16;
		} else if (val->u.type == IR_U32) {
			val->type = &c_type_i32;
			val->u.type = IR_I32;
		}
	}
	val->u.proto = 0; /* reset bit-field */
#endif
}

static void c_do_load_bit_field_packed(rcc_ctx *rcc, c_value *val, uint32_t first_bit, uint32_t bits)
{
	ir_type type = val->u.type;
	ir_ref addr = val->u.ref;
	ir_val shift;
	ir_ref ret = IR_UNUSED, ref, a;
	size_t offset = 0;
	uint8_t mask;
	uint32_t orig_bits = bits;

	while (first_bit >= 8) {
		first_bit -= 8;
		offset++;
	}

	shift.i64 = 0;
	if (first_bit) {
		a = offset ? ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset)) : addr;
		if (ir_type_size[type] > 1) {
			ret = ir_ZEXT(type, ir_LOAD_U8(a));
		} else {
			ret = ir_LOAD(type, a);
		}
		shift.u64 = first_bit;
		IR_ASSERT(shift.u64 < ir_type_size[type] * 8);
		ret = ir_SHR(type, ret, ir_const(rcc->active_ctx, shift, type));
		shift.u64 = 8 - first_bit;
		bits -= 8 - first_bit;
		offset++;
	}

	while (bits >= 8) {
		a = offset ? ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset)) : addr;
		if (ir_type_size[type] > 1) {
			ref = ir_ZEXT(type, ir_LOAD_U8(a));
		} else {
			ref = ir_LOAD(type, a);
		}
		IR_ASSERT(shift.u64 < ir_type_size[type] * 8);
		ref = ir_SHL(type, ref, ir_const(rcc->active_ctx, shift, type));
		ret = ret ? ir_OR(type, ret, ref) : ref;
		shift.u64 += 8;
		bits -= 8;
		offset++;
	}

	if (bits) {
		a = offset ? ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset)) : addr;
		mask = ((1UL<<bits)-1);
		if (ir_type_size[type] > 1) {
			ref = ir_ZEXT(type, ir_AND_U8(ir_LOAD_U8(a), ir_const_u8(rcc->active_ctx, mask)));
		} else {
			ir_val v;
			v.u64 = mask;
			ref = ir_AND(type, ir_LOAD(type, a), ir_const(rcc->active_ctx, v, type));
		}
		IR_ASSERT(shift.u64 < ir_type_size[type] * 8);
		ref = ir_SHL(type, ref, ir_const(rcc->active_ctx, shift, type));
		ret = ret ? ir_OR(type, ret, ref) : ref;
	}

	if (IR_IS_TYPE_SIGNED(val->u.type) && val->type->kind != C_TYPE_ENUM) {
		/* sign extend */
		shift.u64 = ir_type_size[val->u.type] * 8 - orig_bits;
		if (shift.u64) {
			IR_ASSERT(shift.u64 < ir_type_size[type] * 8);
			ir_ref c = ir_const(rcc->active_ctx, shift, val->u.type);
			ret = ir_SHL(val->u.type, ret, c);
			ret = ir_SAR(val->u.type, ret, c);
		}
	}

	val->u.ref = ret;
}

void c_value_rval(rcc_ctx *rcc, c_value *val)
{
	if (c_value_is_lval(val)) {
		IR_ASSERT(val->type->kind != C_TYPE_ARRAY && val->type->kind != C_TYPE_FUNC);
		if ((val->type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, val->type)) {
			yy_error_fmt("invalid use of object with incomplete type \"%s %s\"",
				c_type_kind2str(val->type->kind), yy_sym2str(rcc, val->type->tag));
		}
		if (c_value_is_var(val)) {
			if ((val->type->attr & C_ATTR_VOLATILE) || (val->u.op & C_VAL_VOLATILE)) {
				val->u.ref = ir_VLOAD_v(val->u.type, val->u.ref);
			} else {
				val->u.ref = ir_VLOAD(val->u.type, val->u.ref);
			}
		} else if (c_value_is_reg(val)) {
			val->u.ref = ir_RLOAD(val->u.type, val->u.ref);
		} else if (val->type->kind != C_TYPE_STRUCT && val->type->kind != C_TYPE_UNION) {
			if (C_IS_SIMPLE_VAL(val->u.proto)) {
				if (IR_IS_CONST_REF(val->u.ref)
				 && ((rcc->active_ctx->ir_base[val->u.ref].op == IR_SYM
				   && ((val->type->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) == C_ATTR_CONST))
				  || rcc->active_ctx->ir_base[val->u.ref].op == IR_STR)
				 && (C_IS_TYPE_INT(val->type) || C_IS_TYPE_FP(val->type))
				 && val->u.val.ptr) {
					const void *p = val->u.val.ptr;
					ir_type t = c_type2ir(rcc, val->type);
					ir_val v;

					v.u64 = 0;
					switch (t) {
						case IR_BOOL:	v.b    = *(bool*)p;      break;
						case IR_U8:     v.u8   = *(uint8_t*)p;   break;
						case IR_U16:    v.u16  = *(uint16_t*)p;  break;
						case IR_U32:    v.u32  = *(uint32_t*)p;  break;
						case IR_U64:    v.u64  = *(uint64_t*)p;  break;
						case IR_ADDR:   v.addr = *(uintptr_t*)p; break;
						case IR_CHAR:   v.i64  = *(char*)p;      break;
						case IR_I8:     v.i64  = *(int8_t*)p;    break;
						case IR_I16:    v.i64  = *(int16_t*)p;   break;
						case IR_I32:    v.i64  = *(int32_t*)p;   break;
						case IR_I64:    v.i64  = *(int64_t*)p;   break;
						case IR_DOUBLE: v.d    = *(double*)p;    break;
						case IR_FLOAT:  v.f    = *(float*)p;     break;
						default: IR_ASSERT(0);
					}
					c_value_set_const(val, val->type, t, v);
					return;
				}
				if ((val->type->attr & C_ATTR_VOLATILE) || (val->u.op & C_VAL_VOLATILE)) {
					val->u.ref = ir_LOAD_v(val->u.type, val->u.ref);
				} else {
					val->u.ref = ir_LOAD(val->u.type, val->u.ref);
				}
			} else if (C_IS_BIT_FIELD(val->u.proto)) {
				if (!C_IS_BIT_FIELD_PACKED(val->u.proto)) {
					c_do_load_bit_field(rcc, val, C_BIT_FIELD_START(val->u.proto), C_BIT_FIELD_SIZE(val->u.proto));
				} else {
					c_do_load_bit_field_packed(rcc, val, C_BIT_FIELD_START(val->u.proto), C_BIT_FIELD_SIZE(val->u.proto));
				}
			} else {
				ir_ref ref;
				ir_type t;

				IR_ASSERT(C_IS_VECTOR_DIM(val->u.proto));
				if (c_value_is_lval(val)) {
					t = C_VECTOR_DIM_TYPE(val->u.proto);
					if (rcc->active_ctx->ir_base[val->u.ref].op == IR_VAR) {
						ref = ir_VLOAD(t, val->u.ref);
					} else {
						IR_ASSERT(rcc->active_ctx->ir_base[val->u.ref].type == IR_ADDR);
						ref = ir_LOAD(t, val->u.ref);
					}
				} else {
					ref = val->u.ref;
				}
				t = c_type2ir(rcc, val->type);
				c_value_set_rval(val, val->type, t, ir_EXTRACT(t, ref, val->u.op2));
				return;
			}
		}
		val->u.op &= ~(C_VAL_LVAL|C_VAL_VAR|C_VAL_REG|C_VAL_VOLATILE);
	}
}

static ir_ref c_value_ref(rcc_ctx *rcc, c_value *val)
{
	if (c_value_is_const(val)) {
		if (c_value_is_const_str(val)) {
			return c_create_str_sym(rcc, val);
		}
		ir_type t = (val->type->kind == C_TYPE_ENUM) ? c_type2ir(rcc, val->type) : val->u.type;
		return ir_const(rcc->active_ctx, val->u.val, t);
	} else {
		if (c_value_is_lval(val)) {
			c_value_rval(rcc, val);
		}
		return val->u.ref;
	}
}

static void c_do_trunc(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT_OR_PTR(t)
		&& ((C_IS_TYPE_INT_OR_PTR(v->type) && t->size < v->type->size)
		 || ((v->type->kind == C_TYPE_ARRAY || v->type->kind == C_TYPE_FUNC) && t->size < sizeof(void*))));
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] < ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_TRUNC(type, c_value_ref(rcc, v)));
	} else {
		switch (type) {
			case IR_I8:  val.i64 = v->u.val.i8; break;
			case IR_U8:  val.u64 = v->u.val.u8; break;
			case IR_I16: val.i64 = v->u.val.i16; break;
			case IR_U16: val.u64 = v->u.val.u16; break;
			case IR_I32: val.i64 = v->u.val.i32; break;
			case IR_U32: val.u64 = v->u.val.u32; break;
			case IR_CHAR:val.i64 = v->u.val.i8; break; /* assume "char is signed ??? */
			case IR_BOOL:val.u64 = v->u.val.u8 != 0; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(v, t, type, val);
	}
}

static void c_do_bitcast(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	IR_ASSERT(t->size == v->type->size || (t->size == sizeof(void*) && v->type->kind == C_TYPE_ARRAY));
	IR_ASSERT(ir_get_type_size(type) == ir_get_type_size(v->u.type));
	if (c_value_is_ref(v) || c_value_is_const_str(v) || t->kind == C_TYPE_VECTOR) {
		c_value_set_rval(v, t, type, ir_BITCAST(type, c_value_ref(rcc, v)));
	} else {
		switch (type) {
			case IR_BOOL: v->u.val.u64 = v->u.val.i64 != 0; break;
			case IR_CHAR:
			case IR_I8:   v->u.val.i64 = v->u.val.i8; break;
			case IR_U8:   v->u.val.u64 = v->u.val.u8; break;
			case IR_I16:  v->u.val.i64 = v->u.val.i16; break;
			case IR_U16:  v->u.val.u64 = v->u.val.u16; break;
			case IR_I32:  v->u.val.i64 = v->u.val.i32; break;
#ifndef IR_64
			case IR_ADDR:
#endif
			case IR_U32:  v->u.val.u64 = v->u.val.u32; break;
			case IR_FLOAT: v->u.val.u32_hi = 0; break;
#ifdef IR_64
			case IR_ADDR:
#endif
			case IR_I64:
			case IR_U64:
			case IR_DOUBLE: break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(v, t, type, v->u.val);
	}
}

static void c_do_sext(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT_OR_PTR(t) && C_IS_TYPE_INT_OR_PTR(v->type) && t->size > v->type->size);
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] > ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_SEXT(type, c_value_ref(rcc, v)));
	} else {
		switch (v->u.type) {
			case IR_I8:
			case IR_U8:
			case IR_CHAR:
			case IR_BOOL: val.i64 = (int64_t)v->u.val.i8; break;
			case IR_I16:
			case IR_U16:  val.i64 = (int64_t)v->u.val.i16; break;
			case IR_I32:
			case IR_U32:  val.i64 = (int64_t)v->u.val.i32; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(v, t, type, val);
	}
}

static void c_do_zext(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT_OR_PTR(t) && C_IS_TYPE_INT_OR_PTR(v->type) && t->size > v->type->size);
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] > ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_ZEXT(type, c_value_ref(rcc, v)));
	} else {
		switch (v->u.type) {
			case IR_I8:
			case IR_U8:
			case IR_CHAR:
			case IR_BOOL: val.u64 = (uint64_t)v->u.val.u8; break;
			case IR_I16:
			case IR_U16:  val.u64 = (uint64_t)v->u.val.u16; break;
			case IR_I32:
			case IR_U32:  val.u64 = (uint64_t)v->u.val.u32; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(v, t, type, val);
	}
}

static void c_do_fp2int(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT((C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) && C_IS_TYPE_FP(v->type));
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_FP(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_FP2INT(type, c_value_ref(rcc, v)));
	} else {
		if (v->u.type == IR_FLOAT) {
			switch (type) {
				case IR_CHAR:
				case IR_I8:  val.i64 = (int8_t)v->u.val.f; break;
				case IR_BOOL:
				case IR_U8:  val.u64 = (uint8_t)v->u.val.f; break;
				case IR_I16: val.i64 = (int16_t)v->u.val.f; break;
				case IR_U16: val.u64 = (uint16_t)v->u.val.f; break;
				case IR_I32: val.i64 = (int32_t)v->u.val.f; break;
				case IR_U32: val.u64 = (uint32_t)v->u.val.f; break;
				case IR_I64: val.i64 = (int64_t)v->u.val.f; break;
				case IR_U64: val.u64 = (uint64_t)v->u.val.f; break;
				default: IR_ASSERT(0); return;
			}
			c_value_set_const(v, t, type, val);
		} else if (v->u.type == IR_DOUBLE) {
			switch (type) {
				case IR_CHAR:
				case IR_I8:  val.i64 = (int8_t)v->u.val.d; break;
				case IR_BOOL:
				case IR_U8:  val.u64 = (uint8_t)v->u.val.d; break;
				case IR_I16: val.i64 = (int16_t)v->u.val.d; break;
				case IR_U16: val.u64 = (uint16_t)v->u.val.d; break;
				case IR_I32: val.i64 = (int32_t)v->u.val.d; break;
				case IR_U32: val.u64 = (uint32_t)v->u.val.d; break;
				case IR_I64: val.i64 = (int64_t)v->u.val.d; break;
				case IR_U64: val.u64 = (uint64_t)v->u.val.d; break;
				default: IR_ASSERT(0); return;
			}
			c_value_set_const(v, t, type, val);
		} else {
			IR_ASSERT(0);
		}
	}
}

static void c_do_int2fp(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_FP(t) && (C_IS_TYPE_INT(v->type) || v->type->kind == C_TYPE_ENUM));
	IR_ASSERT(IR_IS_TYPE_FP(type) && IR_IS_TYPE_INT(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_INT2FP(type, c_value_ref(rcc, v)));
	} else {
		if (type == IR_FLOAT) {
			switch (v->u.type) {
				case IR_CHAR:
				case IR_I8:  val.f = (float)v->u.val.i8; break;
				case IR_BOOL:
				case IR_U8:  val.f = (float)v->u.val.u8; break;
				case IR_I16: val.f = (float)v->u.val.i16; break;
				case IR_U16: val.f = (float)v->u.val.u16; break;
				case IR_I32: val.f = (float)v->u.val.i32; break;
				case IR_U32: val.f = (float)v->u.val.u32; break;
				case IR_I64: val.f = (float)v->u.val.i64; break;
				case IR_U64: val.f = (float)v->u.val.u64; break;
				default: IR_ASSERT(0);
			}
			val.u32_hi = 0;
		} else {
			switch (v->u.type) {
				case IR_CHAR:
				case IR_I8:  val.d = (double)v->u.val.i8; break;
				case IR_BOOL:
				case IR_U8:  val.d = (double)v->u.val.u8; break;
				case IR_I16: val.d = (double)v->u.val.i16; break;
				case IR_U16: val.d = (double)v->u.val.u16; break;
				case IR_I32: val.d = (double)v->u.val.i32; break;
				case IR_U32: val.d = (double)v->u.val.u32; break;
				case IR_I64: val.d = (double)v->u.val.i64; break;
				case IR_U64: val.d = (double)v->u.val.u64; break;
				default: IR_ASSERT(0);
			}
		}
		c_value_set_const(v, t, type, val);
	}
}

static void c_do_fp2fp(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_FP(t) && C_IS_TYPE_FP(v->type));
	IR_ASSERT(IR_IS_TYPE_FP(type) && IR_IS_TYPE_FP(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_FP2FP(type, c_value_ref(rcc, v)));
	} else {
		if (type == IR_FLOAT) {
			val.f = (float)v->u.val.d;
			val.u32_hi = 0;
		} else {
			val.d = (double)v->u.val.f;
		}
		c_value_set_const(v, t, type, val);
	}
}

static void c_do_cvt(rcc_ctx *rcc, const c_type *t, ir_type type, c_value *v)
{
	if (type != v->u.type) {
		if (IR_IS_TYPE_INT(type)) {
			if (type == IR_BOOL) {
				ir_val val;

				if (c_value_is_ref(v)) {
					val.u64 = 0;
					c_value_set_rval(v, t, type,
						ir_NE(c_value_ref(rcc, v), ir_const(rcc->active_ctx, val, v->u.type)));
				} else if (IR_IS_TYPE_INT(v->u.type)) {
					val.u64 = v->u.val.u64 != 0;
					c_value_set_const(v, t, type, val);
				} else if (v->u.type == IR_FLOAT) {
					val.u64 = v->u.val.f != 0;
					c_value_set_const(v, t, type, val);
				} else if (v->u.type == IR_DOUBLE) {
					val.u64 = v->u.val.d != 0;
					c_value_set_const(v, t, type, val);
				} else {
					IR_ASSERT(0);
				}
			} else if (IR_IS_TYPE_INT(v->u.type)) {
				if (ir_type_size[type] < ir_type_size[v->u.type]) {
					c_do_trunc(rcc, t, type, v);
				} else if (ir_type_size[type] == ir_type_size[v->u.type]) {
					c_do_bitcast(rcc, t, type, v);
				} else if (IR_IS_TYPE_SIGNED(v->u.type)) {
					c_do_sext(rcc, t, type, v);
				} else {
					c_do_zext(rcc, t, type, v);
				}
			} else if (IR_IS_TYPE_FP(v->u.type)) {
				c_do_fp2int(rcc, t, type, v);
			} else {
				IR_ASSERT(0);
			}
		} else if (IR_IS_TYPE_FP(type)) {
			if (IR_IS_TYPE_INT(v->u.type)) {
				c_do_int2fp(rcc, t, type, v);
			} else if (IR_IS_TYPE_FP(v->u.type)) {
				c_do_fp2fp(rcc, t, type, v);
			} else {
				IR_ASSERT(0);
			}
		} else {
			IR_ASSERT(0);
		}
	} else if (t != v->type) {
		if (c_value_is_const_str(v)) {
			c_create_str_sym(rcc, v);
		}
		v->type = t;
	}
}

void c_do_addr(rcc_ctx *rcc, c_value *v)
{
	c_type *type;
	ir_ref ref;

	if (!c_value_is_lval(v)) {
		if (c_value_is_const(v) && (v->type->kind == C_TYPE_STRUCT || v->type->kind == C_TYPE_UNION)) {
			/* pass */
		} else if (v->type->kind != C_TYPE_FUNC && v->type->kind != C_TYPE_ARRAY) {
			yy_error("lvalue required as unary \"&\" operand");
		}
	} else if (C_IS_BIT_FIELD(v->u.proto)) {
		yy_error("cannot take address of bit-field");
	} else if (C_IS_VECTOR_DIM(v->u.proto)) {
		yy_error("cannot take address of vector element");
	}

	type = c_create_pointer_type(rcc, v->type);
	if (c_value_is_ref(v)) {
		if (c_value_is_var(v)) {
			ref = ir_VADDR(v->u.ref);
		} else if (c_value_is_reg(v)) {
			yy_error("cannot take address of register variable");
		} else {
			ref = v->u.ref;
		}
		if (!IR_IS_CONST_REF(ref) || IR_IS_SYM_CONST(rcc->active_ctx->ir_base[ref].op)) {
			c_value_set_rval(v, type, IR_ADDR, ref);
		} else {
			c_value_set_const(v, type, IR_ADDR, rcc->active_ctx->ir_base[ref].val);
		}
	} else {
		if (c_value_is_const_str(v)) {
			c_create_str_sym(rcc, v);
		}
		v->type = type;
	}
}

static ir_ref c_do_store_bit_field(rcc_ctx *rcc, ir_ref addr, uint32_t first_bit, uint32_t bits, c_value *val)
{
	ir_type type;
	ir_val v;
	ir_ref ret, ref;

	if (first_bit + bits <= 8) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I8 : IR_U8;
	} else if (first_bit + bits <= 16) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I16 : IR_U16;
	} else if (first_bit + bits <= 32) {
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I32 : IR_U32;
	} else {
		IR_ASSERT(first_bit + bits <= 64);
		type = IR_IS_TYPE_SIGNED(val->u.type) ? IR_I64 : IR_U64;
	}

	ret = ref = c_value_ref(rcc, val);
	if (val->u.type != type) {
		if (ir_type_size[type] < ir_type_size[val->u.type]) {
			ref = ir_TRUNC(type, ref);
		} else if (ir_type_size[type] == ir_type_size[val->u.type]) {
			ref = ir_BITCAST(type, ref);
		} else {
			ref = ir_ZEXT(type, ref);
		}
	}

	if (first_bit + bits != ir_type_size[type] * 8) {
		v.u64 = (1ULL<<bits)-1;
		ref = ir_AND(type, ref, ir_const(rcc->active_ctx, v, type));
	}

	if (val->u.type == type) ret = ref;

	if (first_bit) {
		v.u64 = first_bit;
		IR_ASSERT(v.u64 < ir_type_size[type] * 8);
		ref = ir_SHL(type, ref, ir_const(rcc->active_ctx, v, type));
	}

	v.u64 = ~(((1ULL<<bits)-1)<<first_bit);

	if (IR_IS_CONST_REF(ref) && rcc->active_ctx->ir_base[ref].val.u64 == 0) {
		if (ir_type_size[type] < 8) {
			v.u64 &= (1ULL << (ir_type_size[type] * 8)) - 1;
		}
		ir_STORE(
			addr,
			ir_AND(type,
				ir_LOAD(type, addr),
				ir_const(rcc->active_ctx, v, type)));
	} else if (IR_IS_CONST_REF(ref) && rcc->active_ctx->ir_base[ref].val.u64 == ~v.u64) {
		ir_STORE(
			addr,
			ir_OR(type,
				ir_LOAD(type, addr),
				ref));
	} else {
		if (ir_type_size[type] < 8) {
			v.u64 &= (1ULL << (ir_type_size[type] * 8)) - 1;
		}
		ir_STORE(
			addr,
			ir_OR(type,
				ir_AND(type,
					ir_LOAD(type, addr),
					ir_const(rcc->active_ctx, v, type)),
			ref));
	}

	if (IR_IS_TYPE_SIGNED(val->u.type) && val->type->kind != C_TYPE_ENUM) {
		v.u64 = ir_type_size[val->u.type] * 8 - bits;
		if (v.u64) {
			ir_ref c = ir_const(rcc->active_ctx, v, val->u.type);
			IR_ASSERT(v.u64 < ir_type_size[val->u.type] * 8);
			ret = ir_SHL(val->u.type, ret, c);
			ret = ir_SAR(val->u.type, ret, c);
		}
	} else if (val->u.type != type) {
		v.u64 = (1ULL<<bits)-1;
		ret = ir_AND(val->u.type, ret, ir_const(rcc->active_ctx, v, val->u.type));
	}
	return ret;
}

static ir_ref c_do_store_bit_field_packed(rcc_ctx *rcc, ir_ref addr, uint32_t first_bit, uint32_t bits, c_value *val)
{
	ir_type type = (ir_type_size[val->u.type] != 1) ? IR_U8 : val->u.type;
	ir_val shift, mask;
	size_t offset = 0;
	ir_ref ret, ref, a;

	while (first_bit >= 8) {
		first_bit -= 8;
		offset++;
	}

	ret = ref = c_value_ref(rcc, val);

	shift.i64 = 0;
	if (first_bit) {
		mask.u64 = ~(((1ULL<<(8-first_bit))-1)<<first_bit);
		shift.u64 = first_bit;
		IR_ASSERT(shift.u64 < ir_type_size[val->u.type] * 8);
		ref = ir_SHL(val->u.type, ret, ir_const(rcc->active_ctx, shift, val->u.type));
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}
		a = offset ? ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset)) : addr;
		ir_STORE(
			a,
			ir_OR(type,
				ir_AND(type, ir_LOAD(type, a), ir_const(rcc->active_ctx, mask, type)),
				ref));
		shift.u64 = 8 - first_bit;
		bits -= 8 - first_bit;
		offset++;
	}

	while (bits >= 8) {
		a = offset ? ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset)) : addr;
		if (shift.u64) {
			IR_ASSERT(shift.u64 < ir_type_size[val->u.type] * 8);
			ref = ir_SHR(val->u.type, ret, ir_const(rcc->active_ctx, shift, val->u.type));
		} else {
			ref = ret;
		}
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}
		ir_STORE(a, ref);
		shift.i64 += 8;
		bits -= 8;
		offset++;
	}

	if (bits) {
		a = ir_ADD_A(addr, ir_const_size_t(rcc->active_ctx, offset));
		IR_ASSERT(shift.u64 < ir_type_size[val->u.type] * 8);
		ref = ir_SHR(val->u.type, ret, ir_const(rcc->active_ctx, shift, val->u.type));
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}

		mask.u64 = (1ULL<<bits)-1;
		ref = ir_AND(type, ref, ir_const(rcc->active_ctx, mask, type));

		mask.u64 = ~((1ULL<<bits)-1);
		ir_STORE(
			a,
				ir_OR(type,
				ir_AND(type,
					ir_LOAD(type, a),
					ir_const(rcc->active_ctx, mask, type)),
				ref));
	}

	return ret;
}

static ir_ref c_do_store(rcc_ctx *rcc, c_value *addr, c_value *val)
{
	ir_ref ref;

	if (C_IS_SIMPLE_VAL(addr->u.proto)) {
		ref = c_value_ref(rcc, val);
		if (c_value_is_var(addr)) {
			if ((addr->type->attr & C_ATTR_VOLATILE) || (addr->u.op & C_VAL_VOLATILE)) {
				ir_VSTORE_v(addr->u.ref, ref);
			} else {
				ir_VSTORE(addr->u.ref, ref);
			}
		} else if (c_value_is_reg(addr)) {
			ir_RSTORE(addr->u.ref, ref);
		} else {
			if ((addr->type->attr & C_ATTR_VOLATILE) || (addr->u.op & C_VAL_VOLATILE)) {
				ir_STORE_v(addr->u.ref, ref);
			} else {
				ir_STORE(addr->u.ref, ref);
			}
		}
		return ref;
	} else if (C_IS_BIT_FIELD(addr->u.proto)) {
		if (!C_IS_BIT_FIELD_PACKED(addr->u.proto)) {
			return c_do_store_bit_field(rcc, addr->u.ref,
				C_BIT_FIELD_START(addr->u.proto), C_BIT_FIELD_SIZE(addr->u.proto), val);
		} else {
			return c_do_store_bit_field_packed(rcc, addr->u.ref,
				C_BIT_FIELD_START(addr->u.proto), C_BIT_FIELD_SIZE(addr->u.proto), val);
		}
	} else {
		ir_type vt;
		ir_ref vref;

		IR_ASSERT(C_IS_VECTOR_DIM(addr->u.proto) && c_value_is_lval(addr));
		vt = C_VECTOR_DIM_TYPE(addr->u.proto);
		if (rcc->active_ctx->ir_base[addr->u.ref].op == IR_VAR) {
			vref = ir_VLOAD(vt, addr->u.ref);
		} else {
			IR_ASSERT(rcc->active_ctx->ir_base[addr->u.ref].type == IR_ADDR);
			vref = ir_LOAD(vt, addr->u.ref);
		}

		ref = c_value_ref(rcc, val);
		vref = ir_REPLACE(vt, vref, addr->u.op2, ref);

		if (rcc->active_ctx->ir_base[addr->u.ref].op == IR_VAR) {
			ir_VSTORE(addr->u.ref, vref);
		} else {
			IR_ASSERT(rcc->active_ctx->ir_base[addr->u.ref].type == IR_ADDR);
			ir_STORE(addr->u.ref, vref);
		}

		return ref;
	}
}

static const c_type *c_opaque_vector_type(rcc_ctx *rcc, const c_type *src_type)
{
	c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));

	type->size = src_type->size;
	type->kind = C_TYPE_VECTOR;
	type->flags = (src_type->flags & ~C_TYPE_GLOBAL) | (rcc->active_scope ? 0 : C_TYPE_GLOBAL) | C_TYPE_OPAQUE;
	type->attr = src_type->attr;
	type->vec.length = src_type->vec.length;

	if (src_type->vec.type->size == 1) {
		type->vec.type = &c_type_i8;
	} else if (src_type->vec.type->size == 2) {
		type->vec.type = &c_type_i16;
	} else if (src_type->vec.type->size == 4) {
		type->vec.type = &c_type_i32;
	} else if (src_type->vec.type->size == 8) {
		type->vec.type = &c_type_i64;
	} else {
		IR_ASSERT(0);
	}

	return type;
}

static void c_opaque_vector_cast(rcc_ctx *rcc, const c_type *type, c_value *val)
{
	ir_type t = c_type2ir(rcc, type);

#if 0
	/* Update type of comparison */
	rcc->active_ctx->ir_base[val->u.ref].type = t;
	val->type = type;
	val->u.type = t;
#else
	/* Implicit vector BITCAST */
	c_value_set_rval(val, type, t, ir_BITCAST(t, val->u.ref));
#endif
}


/* arg >   0 - means real argument number
 * arg ==  0 - return value
 * arg == -1 - assign
 * arg == -2 - init
 */
static void c_do_check_cvt(rcc_ctx *rcc, const c_type *type, c_value *val, int32_t arg)
{
	const c_type *val_type = val->type;
	uint32_t attr;

	if (C_IS_TYPE_NUM(type) || type->kind == C_TYPE_ENUM) {
		if (!C_IS_TYPE_NUM(val_type) && val_type->kind != C_TYPE_ENUM) {
			if ((val_type->kind == C_TYPE_POINTER || val_type->kind == C_TYPE_ARRAY || val_type->kind == C_TYPE_FUNC)
			 && !C_IS_TYPE_FP(type)) {
				if (arg < 0) {
					yy_warning("assignment makes integer from pointer without a cast");
					if (arg == -2 && !rcc->active_scope) {
						yy_error("initializer element is not computable at load time");
					}
				} else if (arg > 0) {
					yy_warning_fmt("passing argument %d makes integer from pointer without a cast", arg);
				} else {
					yy_warning("return makes integer from pointer without a cast");
				}
			} else {
incompatible:
				if (arg < 0) {
					yy_error("incompatible types in assignment");
				} else if (arg > 0) {
					yy_error_fmt("incompatible type for argument %d", arg);
				} else {
					yy_error("return incompatible type");
				}
			}
		}
	} else if (type->kind == C_TYPE_POINTER) {
		if (C_IS_TYPE_INT(val_type) && c_value_is_const(val) && val->u.val.u64 == 0) {
			/* pass */
		} else if (val_type->kind == C_TYPE_POINTER || val_type->kind == C_TYPE_ARRAY) {
			if (type->pointer.type->kind != C_TYPE_VOID
			 && val_type->pointer.type->kind != C_TYPE_VOID
			 && !c_compatible_types(type->pointer.type, val_type->pointer.type, 1, type->pointer.type->kind == C_TYPE_FUNC)
			 && (val_type->pointer.type->kind != C_TYPE_ARRAY
			  || !c_compatible_types(type->pointer.type, val_type->pointer.type->array.type, 1, type->pointer.type->kind == C_TYPE_FUNC))) {
				if (type->pointer.type->size == val_type->pointer.type->size
				 && C_IS_TYPE_SIGNED(type->pointer.type) != C_IS_TYPE_SIGNED(val_type->pointer.type)
				 && type->pointer.type->kind != C_TYPE_BOOL && C_IS_TYPE_INT(type->pointer.type)
				 && val_type->pointer.type->kind != C_TYPE_BOOL && C_IS_TYPE_INT(val_type->pointer.type)) {
					if (arg < 0) {
						yy_warning("pointer targets in assignment differ in signedness");
				    } else if (arg > 0) {
						yy_warning_fmt("pointer targets in in passing argument %d differ in signedness", arg);
					} else {
						yy_warning("pointer targets in return differ in signedness");
					}
				} else {
					if (arg < 0) {
						yy_warning("assignment from incompatible pointer type");
					} else if (arg > 0) {
						yy_warning_fmt("passing argument %d from incompatible pointer type", arg);
					} else {
						yy_warning("return from incompatible pointer type");
					}
				}
			} else {
				attr = (val_type->pointer.type->attr & ~type->pointer.type->attr)
					& (C_ATTR_CONST|C_ATTR_VOLATILE|C_ATTR_ATOMIC);
				if (attr) {
check_qualifiers:
					if (attr & C_ATTR_CONST) {
						if (type->pointer.type->kind == C_TYPE_FUNC) {
							/* assignement of "const void*" to a pointer to function is allowed */
						} else if (arg < 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "assignment discards \"%s\" qualifier from pointer target type", "const");
						} else if (arg > 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "passing argument %d discards \"%s\" qualifier from pointer target type", arg, "const");
						} else {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "return discards \"%s\" qualifier from pointer target type", "const");
						}
					} else if (attr & C_ATTR_VOLATILE) {
						if (arg < 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "assignment discards \"%s\" qualifier from pointer target type", "volatile");
						} else if (arg > 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "passing argument %d discards \"%s\" qualifier from pointer target type", arg, "volatile");
						} else {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "return discards \"%s\" qualifier from pointer target type", "volatile");
						}
					} else if (attr & C_ATTR_ATOMIC) {
						if (arg < 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "assignment discards \"%s\" qualifier from pointer target type", "atomic");
						} else if (arg > 0) {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "passing argument %d discards \"%s\" qualifier from pointer target type", arg, "atomic");
						} else {
							yy_warning_ex_fmt(E_DISCARDED_QUALIFIERS, "return discards \"%s\" qualifier from pointer target type", "atomic");
						}
					}
				}
			}
		} else if (val_type->kind == C_TYPE_FUNC
		 && (type->pointer.type->kind == C_TYPE_VOID
		  || c_compatible_types(type->pointer.type, val_type, 1, 1))) {
			attr = (val_type->attr & ~type->pointer.type->attr) & (C_ATTR_CONST|C_ATTR_VOLATILE|C_ATTR_ATOMIC);
			if (attr) goto check_qualifiers;
		} else if (C_IS_TYPE_INT(val_type) || val_type->kind == C_TYPE_ENUM) {
			if (arg < 0) {
				yy_warning("assignment makes pointer from integer without a cast");
			} else if (arg > 0) {
				yy_warning_fmt("passing argument %d makes pointer from integer without a cast", arg);
			} else {
				yy_warning("return makes pointer from integer without a cast");
			}
		} else {
			goto incompatible;
		}
	} else if ((type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION || type->kind == C_TYPE_ARRAY)
	 && c_compatible_types(type, val_type, 1, 0)) {
		val->type = type;
		return;
	} else if (type->kind == C_TYPE_VECTOR
	 && val->type->kind == C_TYPE_VECTOR
	 && type->vec.length == val->type->vec.length) {
		if (type->vec.type->kind == val->type->vec.type->kind) {
			/* identicl vector types */
			return;
		} else if (type->size == val->type->size && (val->type->flags & C_TYPE_OPAQUE)) {
			c_opaque_vector_cast(rcc, type, val);
			return;
		} else {
			goto incompatible;
		}
	} else {
		goto incompatible;
	}
	c_value_rval(rcc, val);
	c_do_cvt(rcc, type, c_type2ir(rcc, type), val);
}

void c_do_cast(rcc_ctx *rcc, const c_type *t, c_value *v)
{
	if (t == v->type) {
	} else if (t->kind == C_TYPE_VOID) {
		c_value_set_rval(v, &c_type_void, IR_VOID, IR_NULL);
	} else if (!C_IS_TYPE_SCALAR_OR_PTR(t)) {
		if (t->kind == C_TYPE_UNION) {
			uint32_t i;
			c_field *f;

			for (i = 0, f = t->record.fields; i < t->record.num_fields; f++, i++) {
				if (f->type == v->type
				 || c_compatible_types(f->type, v->type, 1, 0)) {
					ir_ref addr = c_do_alloca(rcc, t->size, c_attr2align(t->attr), 0);
					if (C_IS_TYPE_SCALAR_OR_PTR(v->type) || v->type->kind == C_TYPE_VECTOR) {
						ir_STORE(addr, c_value_ref(rcc, v));
					} else {
						IR_ASSERT(v->type->size);
						ir_memcpy(rcc, addr, c_value_ref(rcc, v),
							ir_const_size_t(rcc->active_ctx, v->type->size), c_attr2align(v->type->attr));
					}
					c_value_set_rval(v, t, IR_ADDR, addr);
					return;
				}
			}
		} else if (t->kind == C_TYPE_VECTOR) {
			if (t->size == v->type->size
			 && (v->type->kind == C_TYPE_VECTOR || C_IS_TYPE_KIND_SCALAR(v->type->kind))) {
				c_do_bitcast(rcc, t, c_type2ir(rcc, t), v);
				return;
			}
			yy_error("cannot convert a value to vector of different size");
		}
		yy_error("conversion to non-scalar type requested");
	} else if (t->flags & C_TYPE_INCOMPLETE) {
		yy_error("conversion to incomplete type");
	} else if (v->type->kind == C_TYPE_VOID || v->type->kind == C_TYPE_STRUCT || v->type->kind == C_TYPE_UNION) {
		yy_error("conversion of non-scalar type requested");
	} else if (v->type->kind == C_TYPE_VECTOR) {
		if (t->size == v->type->size
		 && (t->kind == C_TYPE_VECTOR || C_IS_TYPE_KIND_SCALAR(t->kind))) {
			c_do_bitcast(rcc, t, c_type2ir(rcc, t), v);
			return;
		}
		yy_error("cannot convert a vector to type of different size");
	} else if (t->kind == C_TYPE_POINTER) {
		if (C_IS_TYPE_FP(v->type)) {
			yy_error("cannot convert floating point value to a pointer");
		} else if (t->size != v->type->size
		 && (C_IS_TYPE_INT(v->type) || v->type->kind == C_TYPE_ENUM)
		 && !c_value_is_const(v)) {
			yy_warning("cast to pointer from integer of different size");
		}
		v->u.op &= ~C_VAL_INLINE; /* Disable inlining. See: gcc/testsuite/gcc.dg/pr60647-2.c */
	} else if (v->type->kind == C_TYPE_POINTER || v->type->kind == C_TYPE_ARRAY) {
		if (C_IS_TYPE_FP(t)) {
			yy_error("cannot convert pointer to a floating point");
		} else if (t->size != sizeof(void*)
		 && (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM)
		 && !c_value_is_const(v)) {
			yy_warning("cast from pointer to integer of different size");
		}
	}
	c_value_rval(rcc, v);
	if (t->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) {
		/* remove top-level qualifiers */
		c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
		*type = *t;
		if (rcc->active_scope) type->flags &= ~C_TYPE_GLOBAL;
		type->attr &= ~(C_ATTR_CONST|C_ATTR_VOLATILE);
		t = type;
	}
	c_do_cvt(rcc, t, c_type2ir(rcc, t), v);
	v->u.proto = 0; /*reset bit-field */
}

void c_do_post_op(rcc_ctx *rcc, yy_sym sym, c_value *v)
{
	c_value val, tmp;
	ir_type res_type, type;
	ir_val one;
	ir_ref ref;

	if (!c_value_is_lval(v) || !C_IS_TYPE_SCALAR_OR_PTR(v->type)) {
		yy_error_fmt("lvalue required as \"%s\" operand", yy_sym2str(rcc, sym));
	} else if (v->type->attr & C_ATTR_CONST) {
		yy_error_fmt("% of read-only location",
			(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
	}
	val = *v;
	c_value_rval(rcc, &val);
	res_type = type = val.u.type;
	if (v->type->kind == C_TYPE_POINTER) {
		if (v->type->pointer.type->kind == C_TYPE_VOID) {
			one.u64 = 1;
		} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, v->type->pointer.type)) {
			yy_error_fmt("%s of pointer to an incomplete type",
				(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
		} else {
			one.u64 = v->type->pointer.type->size;
		}
		res_type = IR_ADDR;
		type = IR_SIZE_T;
	} else if (IR_IS_TYPE_INT(type)) {
		one.u64 = 1;
	} else if (type == IR_DOUBLE) {
		one.d = 1;
	} else if (type == IR_FLOAT) {
		one.f = 1;
		one.u32_hi = 0;
	} else {
		IR_ASSERT(0);
	}
	if (sym == YY__PLUS_PLUS) {
		ref = ir_ADD(res_type, val.u.ref, ir_const(rcc->active_ctx, one, type));
	} else {
		IR_ASSERT(sym == YY__MINUS_MINUS);
		ref = ir_SUB(res_type, val.u.ref, ir_const(rcc->active_ctx, one, type));
	}
	c_value_set_rval(&tmp, val.type, type, ref);
	c_do_store(rcc, v, &tmp);
	*v = val;
}

void c_do_pre_op(rcc_ctx *rcc, yy_sym sym, c_value *v)
{
	c_value val;
	ir_type res_type, type;
	ir_val one;

	if (!c_value_is_lval(v) || !C_IS_TYPE_SCALAR_OR_PTR(v->type)) {
		yy_error_fmt("lvalue required as \"%s\" operand", yy_sym2str(rcc, sym));
	} else if (v->type->attr & C_ATTR_CONST) {
		yy_error_fmt("% of read-only location",
			(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
	}
	val = *v;
	c_value_rval(rcc, &val);
	res_type = type = val.u.type;
	if (v->type->kind == C_TYPE_POINTER) {
		if (v->type->pointer.type->kind == C_TYPE_VOID) {
			one.u64 = 1;
		} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, v->type->pointer.type)) {
			yy_error_fmt("%s of pointer to an incomplete type",
				(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
		} else {
			one.u64 = v->type->pointer.type->size;
		}
		res_type = IR_ADDR;
		type = IR_SIZE_T;
	} else if (IR_IS_TYPE_INT(type)) {
		one.u64 = 1;
	} else if (type == IR_DOUBLE) {
		one.d = 1;
	} else if (type == IR_FLOAT) {
		one.f = 1;
		one.u32_hi = 0;
	} else {
		IR_ASSERT(0);
	}
	if (sym == YY__PLUS_PLUS) {
		val.u.ref = ir_ADD(res_type, val.u.ref, ir_const(rcc->active_ctx, one, type));
	} else {
		IR_ASSERT(sym == YY__MINUS_MINUS);
		val.u.ref = ir_SUB(res_type, val.u.ref, ir_const(rcc->active_ctx, one, type));
	}
	c_do_store(rcc, v, &val);
	*v = val;
}

void c_do_deref(rcc_ctx *rcc, c_value *v)
{
	c_value_rval(rcc, v);
	if (v->type->kind != C_TYPE_POINTER && v->type->kind != C_TYPE_ARRAY) {
		if (v->type->kind == C_TYPE_FUNC) return;
		yy_error("invalid type argument of unary \"*\"");
	} else if (v->type->pointer.type->kind == C_TYPE_VOID) {
		yy_warning("dereferencing \"void *\" pointer");
		c_value_set_rval(v, &c_type_void, IR_VOID, IR_UNUSED);
		return;
	} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, v->type->pointer.type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(v->type->pointer.type->kind), yy_sym2str(rcc, v->type->pointer.type->tag));
	}
	if (v->type->pointer.type->kind != C_TYPE_FUNC
	 && v->type->pointer.type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, v->type->pointer.type, c_type2ir(rcc, v->type->pointer.type), c_value_ref(rcc, v));
	} else {
		c_value_set_rval(v, v->type->pointer.type, c_type2ir(rcc, v->type->pointer.type), c_value_ref(rcc, v));
	}
}

void c_do_unary_plus(rcc_ctx *rcc, c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(rcc, v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(rcc, &c_type_i32, IR_I32, v);
		}
	} else if (!C_IS_TYPE_FP(t) && t->kind != C_TYPE_VECTOR) {
		yy_error("invalid type argument of unary \"+\"");
	}
}

void c_do_neg(rcc_ctx *rcc, c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(rcc, v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(rcc, &c_type_i32, IR_I32, v);
		}
	} else if (!C_IS_TYPE_FP(t) && t->kind != C_TYPE_VECTOR) {
		yy_error("invalid type argument of unary \"-\"");
	}

	if (c_value_is_ref(v) || t->kind == C_TYPE_VECTOR) {
		// TODO: constant folding for vectors ???
		v->u.ref = ir_NEG(v->u.type, c_value_ref(rcc, v));
	} else {
		switch (v->u.type) {
			case IR_I32:    v->u.val.i64 = -v->u.val.i32; break;
			case IR_U32:    v->u.val.u64 = -v->u.val.u32; break;
			case IR_I64:
			case IR_U64:    v->u.val.i64 = -v->u.val.i64; break;
			case IR_FLOAT:  v->u.val.f = -v->u.val.f; break;
			case IR_DOUBLE: v->u.val.d = -v->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
	}
}

void c_do_not(rcc_ctx *rcc, c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(rcc, v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(rcc, &c_type_i32, IR_I32, v);
		}
	} else if (t->kind != C_TYPE_VECTOR || !C_IS_TYPE_INT(t->vec.type)) {
		yy_error("invalid type argument of unary \"~\"");
	}
	if (c_value_is_ref(v) || t->kind == C_TYPE_VECTOR) {
		// TODO: constant folding for vectors ???
		v->u.ref = ir_NOT(v->u.type, c_value_ref(rcc, v));
	} else {
		switch (v->u.type) {
			case IR_I32: v->u.val.i64 = ~v->u.val.i32; break;
			case IR_U32: v->u.val.u64 = ~v->u.val.u32; break;
			case IR_I64:
			case IR_U64: v->u.val.u64 = ~v->u.val.i64; break;
			default: IR_ASSERT(0); return;
		}
	}
}

void c_do_bool_not(rcc_ctx *rcc, c_value *v)
{
	ir_val val;

	c_value_rval(rcc, v);
	if (v->type->kind == C_TYPE_VOID
	 || v->type->kind == C_TYPE_STRUCT
	 || v->type->kind == C_TYPE_UNION
	 || v->type->kind == C_TYPE_VECTOR) {
		yy_error("invalid type argument of unary \"!\"");
	}
	if (c_value_is_ref(v)) {
		if (v->u.type == IR_BOOL) {
			c_value_set_rval(v, &c_type_bool, IR_BOOL, ir_NOT_B(c_value_ref(rcc, v)));
		} else {
			val.u64 = 0;
			c_value_set_rval(v, &c_type_bool, IR_BOOL,
				ir_EQ(c_value_ref(rcc, v), ir_const(rcc->active_ctx, val, v->u.type)));
		}
	} else {
		val.u64 = (v->u.val.u64 == 0);
		c_value_set_const(v, &c_type_bool, IR_BOOL, val);
	}
}

void c_do_array_dim(rcc_ctx *rcc, c_value *v, c_value *dim)
{
	const c_type *type;
	ir_ref ref;
	bool is_volatile;

	type = v->type;
	if (type->kind == C_TYPE_ARRAY) {
		type = type->array.type;
		ref = c_value_ref(rcc, v);
	} else if (type->kind == C_TYPE_POINTER) {
		if (type->pointer.type->kind == C_TYPE_VOID) yy_error("dereferencing \"void *\" pointer");
		if ((type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(type->pointer.type->kind), yy_sym2str(rcc, type->pointer.type->tag));
		}
		type = type->pointer.type;
		ref = c_value_ref(rcc, v);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type vt = c_type2ir(rcc, type);

		if (!C_IS_TYPE_INT(dim->type) && dim->type->kind != C_TYPE_ENUM) yy_error("array subscript is not an integer");
		// TODO: constant range check ???
		c_value_rval(rcc, dim);
		if (c_value_is_lval(v)) {
			c_value_set_lval(v, type->vec.type, c_type2ir(rcc, type->vec.type), v->u.ref);
			v->u.proto = C_VECTOR_DIM(vt);
			v->u.op2 = c_value_ref(rcc, dim);
		} else {
			ir_type	t = c_type2ir(rcc, type->vec.type);
			c_value_set_rval(v, type->vec.type, t, ir_EXTRACT(t, c_value_ref(rcc, v), c_value_ref(rcc, dim)));
		}
		return;
	} else {
		type = dim->type;
		if (type->kind == C_TYPE_ARRAY) {
			IR_ASSERT(type->array.type->size);
			type = type->array.type;
		} else if (type->kind == C_TYPE_POINTER) {
			if (type->pointer.type->kind == C_TYPE_VOID) yy_error("dereferencing \"void *\" pointer");
			if ((type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type->pointer.type)) {
				yy_error_fmt("invalid use of undefined \"%s %s\"",
					c_type_kind2str(type->pointer.type->kind), yy_sym2str(rcc, type->pointer.type->tag));
			}
			type = type->pointer.type;
		} else {
			yy_error("subscripted value is neither array nor pointer");
		}
		/* turn X[Y] into Y[X] */
		ref = c_value_ref(rcc, dim);
		dim = v;
	}
	if (!C_IS_TYPE_INT(dim->type) && dim->type->kind != C_TYPE_ENUM) yy_error("array subscript is not an integer");
	c_value_rval(rcc, dim);
	if (!c_value_is_const(dim) || dim->u.val.u64 != 0) {
		ir_ref dim_ref;

		if (C_IS_TYPE_SIGNED(dim->type)) {
			if (dim->type->kind != c_type_ssize_t.kind) {
				c_do_cvt(rcc, &c_type_ssize_t, IR_SSIZE_T, dim);
			}
			if (!(type->attr & C_ATTR_VLA)) {
				if (c_value_is_const(dim)) {
					dim_ref = ir_const_ssize_t(rcc->active_ctx, dim->u.val.i64 * type->size);
				} else {
					dim_ref = c_value_ref(rcc, dim);
					if (type->size != 1) {
						dim_ref = ir_MUL(IR_SSIZE_T, dim_ref, ir_const_ssize_t(rcc->active_ctx, type->size));
					}
				}
			} else {
				dim_ref = ir_MUL(IR_SSIZE_T, c_value_ref(rcc, dim), c_type_ssize(rcc, type));
			}
		} else {
			if (dim->type->kind != c_type_size_t.kind) {
				c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, dim);
			}
			if (!(type->attr & C_ATTR_VLA)) {
				if (c_value_is_const(dim)) {
					dim_ref = ir_const_size_t(rcc->active_ctx, dim->u.val.u64 * type->size);
				} else {
					dim_ref = c_value_ref(rcc, dim);
					if (type->size != 1) {
						dim_ref = ir_MUL(IR_SIZE_T, dim_ref, ir_const_size_t(rcc->active_ctx, type->size));
					}
				}
			} else {
				dim_ref = ir_MUL(IR_SIZE_T, c_value_ref(rcc, dim), c_type_size(rcc, type));
			}
		}
		ref = ir_ADD_A(ref, dim_ref);
	}
	is_volatile = (v->type->kind == C_TYPE_ARRAY)
		&& ((v->type->attr & C_ATTR_VOLATILE) || (v->u.op & C_VAL_VOLATILE));
	if (type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, type, c_type2ir(rcc, type), ref);
	} else {
		c_value_set_rval(v, type, c_type2ir(rcc, type), ref);
	}
	if (is_volatile) {
		v->u.op |= C_VAL_VOLATILE;
	}
}

void c_do_struct_field(rcc_ctx *rcc, c_value *v, c_name field_name)
{
	c_field *field;
	size_t offset;
	bool is_volatile;

	if (v->type->kind != C_TYPE_STRUCT && v->type->kind != C_TYPE_UNION) {
		yy_error_fmt("request for member \"%s\" in something not a structure or union", yy_sym2str(rcc, field_name));
	} else if ((v->type->flags & C_TYPE_INCOMPLETE)) {
		IR_ASSERT(v->type->tag);
		if (!c_fix_incomplete_type(rcc, v->type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, v->type->pointer.type->record.tag));
		}
	}
	field = c_find_struct_field(v->type, field_name, &offset);
	if (!field) {
		if (v->type->record.tag) {
			yy_error_fmt("\"%s %s\" has no member named \"%s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, v->type->record.tag),
				yy_sym2str(rcc, field_name));
		} else {
			yy_error_fmt("%s has no member named \"%s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, field_name));
		}
	}
	ir_ref ref = v->u.ref;
	if (offset) {
		ref = ir_ADD_A(ref, ir_const_size_t(rcc->active_ctx, offset));
	}
	is_volatile = ((v->type->attr & C_ATTR_VOLATILE) || (v->u.op & C_VAL_VOLATILE));
	if (field->type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, field->type, c_type2ir(rcc, field->type), ref);
		v->u.proto = field->bit_field;
	} else {
		c_value_set_rval(v, field->type, c_type2ir(rcc, field->type), ref);
	}
	if (is_volatile) {
		v->u.op |= C_VAL_VOLATILE;
	}
}

void c_do_struct_field_deref(rcc_ctx *rcc, c_value *v, c_name field_name)
{
	c_field *field;
	size_t offset;
	bool is_volatile;

	if (v->type->kind != C_TYPE_POINTER && v->type->kind != C_TYPE_ARRAY) {
		yy_error("invalid type argument of \"->\"");
	} else if (v->type->pointer.type->kind != C_TYPE_STRUCT && v->type->pointer.type->kind != C_TYPE_UNION) {
		yy_error_fmt("request for member \"%s\" in something not a structure or union", yy_sym2str(rcc, field_name));
	} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE)) {
		IR_ASSERT(v->type->pointer.type->tag);
		if (!c_fix_incomplete_type(rcc, v->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, v->type->pointer.type->tag));
		}
	}
	field = c_find_struct_field(v->type->pointer.type, field_name, &offset);
	if (!field) {
		if (v->type->pointer.type->record.tag) {
			yy_error_fmt("\"%s %s\" has no member named \"%s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, v->type->pointer.type->record.tag),
				yy_sym2str(rcc, field_name));
		} else {
			yy_error_fmt("%s has no member named \"%s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(rcc, field_name));
		}
	}
	ir_ref ref = c_value_ref(rcc, v);
	if (offset) {
		ref = ir_ADD_A(ref, ir_const_size_t(rcc->active_ctx, offset));
	}
	is_volatile = (v->type->pointer.type->attr & C_ATTR_VOLATILE) != 0;
	if (field->type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, field->type, c_type2ir(rcc, field->type), ref);
		v->u.proto = field->bit_field;
	} else {
		c_value_set_rval(v, field->type, c_type2ir(rcc, field->type), ref);
	}
	if (is_volatile) {
		v->u.op |= C_VAL_VOLATILE;
	}
}

c_value *c_do_grow_actual_parameters(rcc_ctx *rcc, c_value *args, uint32_t num_args)
{
	if (num_args == C_ALLOCA_PARAMS) {
		c_value *new_args = ir_mem_malloc(C_ALLOCA_PARAMS * 2 * sizeof(c_value));
		if (!new_args) yy_error("out of memory");
		memcpy(new_args, args, C_ALLOCA_PARAMS * sizeof(c_value));
		return new_args;
	} else {
		IR_ASSERT(num_args % C_ALLOCA_FIELDS == 0);
		c_value *new_args = ir_mem_realloc(args, (num_args + C_ALLOCA_PARAMS) * sizeof(c_value));
		if (!new_args) yy_error("out of memory");
		return new_args;
	}
}

static ir_ref c_va_list_addr(rcc_ctx *rcc, c_value *val)
{
#if defined(__i386__) || defined(_WIN32) || defined(__APPLE__)
	if (!c_value_is_lval(val)) yy_error("lvalue required");
	return c_value_is_var(val) ? ir_VADDR(val->u.ref) : val->u.ref;
#else
	return c_value_ref(rcc, val);
#endif
}

void c_do_builtin(rcc_ctx *rcc, c_value *val, c_name name, uint32_t num_args, c_value *args)
{
	rcc->c_last_call_func_type = NULL;
	if (name == YY___BUILTIN_VA_START) {
		if (num_args != 1 && num_args != 2) yy_error("wrong number of arguments in __builtin_va_start() call");
		// TODO: arg type check ???
		ir_VA_START(c_va_list_addr(rcc, &args[0]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_VA_END) {
		if (num_args != 1) yy_error("wrong number of arguments in __builtin_va_end() call");
		ir_VA_END(c_va_list_addr(rcc, &args[0]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_VA_COPY) {
		if (num_args != 2) yy_error("wrong number of arguments in __builtin_va_copy() call");
		ir_VA_COPY(c_va_list_addr(rcc, &args[0]), c_va_list_addr(rcc, &args[1]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_ALLOCA) {
		ir_ref ref;

		if (num_args != 1) yy_error("wrong number of arguments in __builtin_alloca() call");
		ref = ir_ALLOCA(c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, &c_type_ptr, IR_ADDR, ref);
	} else if (name == YY___BUILTIN_ABORT || name == YY___BUILTIN_TRAP) {
		if (num_args != 0) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (!rcc->yy_hash.data[YY_ABORT].sym) {
			c_dcl dcl;
			c_type *type;

			memset(&dcl, 0, sizeof(dcl));
			dcl.flags = C_DCL_EXTERN | C_TYPE_SPEC_TYPE;

			type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
			if (!type) yy_error("out of memory");
			memset(type, 0, sizeof(c_type));
			type->kind = C_TYPE_FUNC;
			type->func.ret_type = &c_type_void;
			type->func.num_params = 0;
			type->func.params = NULL;
			dcl.type = type;

			c_declare(rcc, YY_ABORT, &dcl);
		}
		ir_CALL(IR_VOID,
			ir_const_func(rcc->active_ctx, IR_EXT_STR(YY_ABORT),
				ir_proto_0(rcc->active_ctx, 0, IR_VOID)));
//???		ir_UNREACHABLE();
//???		ir_BEGIN(IR_UNUSED);
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_UNREACHABLE) {
		if (num_args != 0) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
//???		ir_UNREACHABLE();
//???		ir_BEGIN(IR_UNUSED);
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_DEBUGTRAP) {
		if (num_args != 0) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		ir_TRAP();
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_FRAME_ADDRESS) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (!c_value_is_const(&args[0]) || !C_IS_TYPE_INT(args[0].type) || args[0].u.val.u64 != 0) {
			yy_error_fmt("wrong level in %s() call", yy_sym2str(rcc, name));
		}
		c_value_set_rval(val, &c_type_ptr, IR_ADDR, ir_FRAME_ADDR());
		c_type *t = (c_type*)rcc->active_func->value.type;
		t->attr |= C_ATTR_NOINLINE;
	} else if (name == YY___BUILTIN_MEMCPY) {
		ir_ref ref;

		if (num_args != 3) yy_error("wrong number of arguments in __builtin_memcpy() call");
		if ((args[0].type->kind != C_TYPE_POINTER && args[0].type->kind != C_TYPE_ARRAY)
		 || (args[0].type->attr & C_ATTR_CONST)) {
			c_do_check_cvt(rcc, &c_type_ptr, &args[0], 1);
		}
		if (args[1].type->kind != C_TYPE_POINTER && args[1].type->kind != C_TYPE_ARRAY) {
			c_do_check_cvt(rcc, &c_type_const_ptr, &args[1], 2);
		}
		if (args[2].type->kind != C_TYPE_SIZE_T) {
			c_do_check_cvt(rcc, &c_type_size_t, &args[2], 3);
		}
		ref = ir_CALL_3(IR_ADDR,
			ir_const_func(rcc->active_ctx, IR_EXT_STR(YY_MEMCPY),
				ir_proto_3(rcc->active_ctx, 0, IR_ADDR, IR_ADDR, IR_ADDR, IR_SIZE_T)),
			c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]), c_value_ref(rcc, &args[2]));
		c_value_set_rval(val, &c_type_ptr, IR_ADDR, ref);
	} else if (name == YY___BUILTIN_MEMSET) {
		ir_ref ref;

		if (num_args != 3) yy_error("wrong number of arguments in __builtin_memset() call");
		if (args[0].type->kind != C_TYPE_POINTER && args[0].type->kind != C_TYPE_ARRAY) {
			c_do_check_cvt(rcc, &c_type_ptr, &args[0], 1);
		}
		if (args[1].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[1], 2);
		}
		if (args[2].type->kind != C_TYPE_SIZE_T) {
			c_do_check_cvt(rcc, &c_type_size_t, &args[2], 3);
		}
		ref = ir_CALL_3(IR_ADDR,
			ir_const_func(rcc->active_ctx, IR_EXT_STR(YY_MEMSET),
				ir_proto_3(rcc->active_ctx, 0, IR_ADDR, IR_ADDR, IR_I32, IR_SIZE_T)),
			c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]), c_value_ref(rcc, &args[2]));
		c_value_set_rval(val, &c_type_ptr, IR_ADDR, ref);
	} else if (name == YY___BUILTIN_ABS) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[0], 1);
		}
		ref = ir_ABS(args[0].u.type, c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, args[0].type, args[0].u.type, ref);
	} else if (name == YY___BUILTIN_LABS) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_IL) {
			c_do_check_cvt(rcc, &c_type_il, &args[0], 1);
		}
		ref = ir_ABS(args[0].u.type, c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, args[0].type, args[0].u.type, ref);
	} else if (name == YY___BUILTIN_LLABS) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_ILL) {
			c_do_check_cvt(rcc, &c_type_ill, &args[0], 1);
		}
		ref = ir_ABS(args[0].u.type, c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, args[0].type, args[0].u.type, ref);
	} else if (name == YY___BUILTIN_FABS) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_DOUBLE) {
			c_do_check_cvt(rcc, &c_type_double, &args[0], 1);
		}
		ref = ir_ABS(args[0].u.type, c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, args[0].type, args[0].u.type, ref);
	} else if (name == YY___BUILTIN_FABSF) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_FLOAT) {
			c_do_check_cvt(rcc, &c_type_float, &args[0], 1);
		}
		ref = ir_ABS(args[0].u.type, c_value_ref(rcc, &args[0]));
		c_value_set_rval(val, args[0].type, args[0].u.type, ref);
	} else if (name == YY___BUILTIN_BSWAP16) {
		ir_val v;

		if (args[0].type->kind != C_TYPE_U16 && args[0].type->kind != C_TYPE_I16) {
			c_do_check_cvt(rcc, &c_type_u16, &args[0], 1);
		}
		v.u64 = 8;
		c_value_set_rval(val, args[0].type, args[0].u.type,
			ir_ROL(args[0].u.type, c_value_ref(rcc, &args[0]), ir_const(rcc->active_ctx, v, args[0].u.type)));
	} else if (name == YY___BUILTIN_BSWAP32) {
		if (args[0].type->kind != C_TYPE_U32 && args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_u32, &args[0], 1);
		}
		c_value_set_rval(val, args[0].type, args[0].u.type, ir_BSWAP(args[0].u.type, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_BSWAP64) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_U64 && args[0].type->kind != C_TYPE_I64) {
			c_do_check_cvt(rcc, &c_type_u64, &args[0], 1);
		}
		c_value_set_rval(val, args[0].type, args[0].u.type, ir_BSWAP(args[0].u.type, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_POPCOUNT) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTPOP(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_POPCOUNTL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_IL) {
			c_do_check_cvt(rcc, &c_type_il, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTPOP(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_POPCOUNTLL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_ILL) {
			c_do_check_cvt(rcc, &c_type_ill, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTPOP(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CLZ) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTLZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CLZL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_IL) {
			c_do_check_cvt(rcc, &c_type_il, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTLZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CLZLL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_ILL) {
			c_do_check_cvt(rcc, &c_type_ill, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTLZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CTZ) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTTZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CTZL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_IL) {
			c_do_check_cvt(rcc, &c_type_il, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTTZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_CTZLL) {
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_ILL) {
			c_do_check_cvt(rcc, &c_type_ill, &args[0], 1);
		}
		c_value_set_rval(val, &c_type_i32, IR_I32, ir_CTTZ(IR_I32, c_value_ref(rcc, &args[0])));
	} else if (name == YY___BUILTIN_FFS) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_I32) {
			c_do_check_cvt(rcc, &c_type_i32, &args[0], 1);
		}
		ref = c_value_ref(rcc, &args[0]);
		c_value_set_rval(val, &c_type_i32, IR_I32,
			ref = ir_COND(IR_I32, ref,
				ir_ADD(IR_I32, ir_CTTZ(IR_I32, ref), ir_const_i32(rcc->active_ctx, 1)),
				ir_const_i32(rcc->active_ctx, 0)));
	} else if (name == YY___BUILTIN_FFSL) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_IL) {
			c_do_check_cvt(rcc, &c_type_il, &args[0], 1);
		}
		ref = c_value_ref(rcc, &args[0]);
		c_value_set_rval(val, &c_type_i32, IR_I32,
			ref = ir_COND(IR_I32, ref,
				ir_ADD(IR_I32, ir_CTTZ(IR_I32, ref), ir_const_i32(rcc->active_ctx, 1)),
				ir_const_i32(rcc->active_ctx, 0)));
	} else if (name == YY___BUILTIN_FFSLL) {
		ir_ref ref;

		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_ILL) {
			c_do_check_cvt(rcc, &c_type_ill, &args[0], 1);
		}
		ref = c_value_ref(rcc, &args[0]);
		c_value_set_rval(val, &c_type_i32, IR_I32,
			ref = ir_COND(IR_I32, ref,
				ir_ADD(IR_I32, ir_CTTZ(IR_I32, ref), ir_const_i32(rcc->active_ctx, 1)),
				ir_const_i32(rcc->active_ctx, 0)));
	} else if (name >= YY___BUILTIN_ADD_OVERFLOW && name <= YY___BUILTIN_UMULLL_OVERFLOW) {
		ir_ref ref, overflow;
		const c_type *t;

		if (num_args != 3) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (name == YY___BUILTIN_ADD_OVERFLOW || name == YY___BUILTIN_SUB_OVERFLOW || name == YY___BUILTIN_MUL_OVERFLOW) {
			if (args[2].type->kind != C_TYPE_POINTER || !C_IS_TYPE_INT(args[2].type->pointer.type)) {
				yy_error_fmt("incompatible types of arguments in %s() call", yy_sym2str(rcc, name));
			}
			t = args[2].type->pointer.type;
			// TODO: support for cases when operands and/or result have different size and/or signess ???
		} else if (name == YY___BUILTIN_ADD_OVERFLOW_P || name == YY___BUILTIN_SUB_OVERFLOW_P || name == YY___BUILTIN_MUL_OVERFLOW_P) {
			if (!C_IS_TYPE_INT(args[2].type)) {
				yy_error_fmt("incompatible types of arguments in %s() call", yy_sym2str(rcc, name));
			}
			t = args[2].type;
			// TODO: support for cases when operands and/or result have different size and/or signess ???
		} else {
			if (name == YY___BUILTIN_SADD_OVERFLOW || name == YY___BUILTIN_SSUB_OVERFLOW || name == YY___BUILTIN_SMUL_OVERFLOW) {
				t = &c_type_i32;
			} else if (name == YY___BUILTIN_SADDL_OVERFLOW || name == YY___BUILTIN_SSUBL_OVERFLOW || name == YY___BUILTIN_SMULL_OVERFLOW) {
				t = &c_type_il;
			} else if (name == YY___BUILTIN_SADDLL_OVERFLOW || name == YY___BUILTIN_SSUBLL_OVERFLOW || name == YY___BUILTIN_SMULLL_OVERFLOW) {
				t = &c_type_ill;
			} else if (name == YY___BUILTIN_UADD_OVERFLOW || name == YY___BUILTIN_USUB_OVERFLOW || name == YY___BUILTIN_UMUL_OVERFLOW) {
				t = &c_type_u32;
			} else if (name == YY___BUILTIN_UADDL_OVERFLOW || name == YY___BUILTIN_USUBL_OVERFLOW || name == YY___BUILTIN_UMULL_OVERFLOW) {
				t = &c_type_ul;
			} else if (name == YY___BUILTIN_UADDLL_OVERFLOW || name == YY___BUILTIN_USUBLL_OVERFLOW || name == YY___BUILTIN_UMULLL_OVERFLOW) {
				t = &c_type_ull;
			} else {
				IR_ASSERT(0);
				t = NULL;
			}
			if (args[2].type->kind != C_TYPE_POINTER || args[2].type->pointer.type->kind != t->kind) {
				yy_error_fmt("incompatible types of arguments in %s() call", yy_sym2str(rcc, name));
			}
		}

		if (args[0].type->kind != t->kind) {
			c_do_check_cvt(rcc, t, &args[0], 1);
		}
		if (args[1].type->kind != t->kind) {
			c_do_check_cvt(rcc, t, &args[1], 2);
		}

		if (name >= YY___BUILTIN_ADD_OVERFLOW && name <= YY___BUILTIN_UADDLL_OVERFLOW) {
			ref = ir_ADD_OV(args[0].u.type, c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]));
		} else if (name >= YY___BUILTIN_SUB_OVERFLOW && name <= YY___BUILTIN_USUBLL_OVERFLOW) {
			ref = ir_SUB_OV(args[0].u.type, c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]));
		} else {
			IR_ASSERT(name >= YY___BUILTIN_MUL_OVERFLOW && name <= YY___BUILTIN_UMULLL_OVERFLOW);
			ref = ir_MUL_OV(args[0].u.type, c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]));
		}
		overflow = ir_OVERFLOW(ref);

		if (name != YY___BUILTIN_ADD_OVERFLOW_P && name != YY___BUILTIN_SUB_OVERFLOW_P && name != YY___BUILTIN_MUL_OVERFLOW_P) {
			if (t->kind != args[2].type->pointer.type->kind) {
				if (t->size > args[2].type->pointer.type->size) {
					ref = ir_TRUNC(c_type2ir(rcc, args[2].type->pointer.type), ref);
				} else if (t->size == args[2].type->pointer.type->size) {
					ref = ir_BITCAST(c_type2ir(rcc, args[2].type->pointer.type), ref);
				} else if (C_IS_TYPE_SIGNED(t)) {
					ref = ir_SEXT(c_type2ir(rcc, args[2].type->pointer.type), ref);
				} else {
					ref = ir_ZEXT(c_type2ir(rcc, args[2].type->pointer.type), ref);
				}
			}

			if (c_value_is_ref(&args[2]) && rcc->active_ctx->ir_base[args[2].u.ref].op == IR_VADDR) {
				ir_VSTORE(rcc->active_ctx->ir_base[args[2].u.ref].op1, ref);
			} else {
				ir_STORE(c_value_ref(rcc, &args[2]), ref);
			}
		}

		c_value_set_rval(val, &c_type_bool, IR_BOOL, overflow);
	} else if (name == YY___BUILTIN_EXPECT) {
		if (num_args != 2) yy_error("wrong number of arguments in __builtin_expect() call");
		c_value_set_rval(val, args[0].type, args[0].u.type, c_value_ref(rcc, &args[0]));
		if (c_value_is_const(&args[1]) && C_IS_TYPE_INT(args[1].type)) {
			rcc->c_last_expect_ref = val->u.ref;
			rcc->c_last_expect_val = args[1].u.val.u64 != 0;
		} else {
			yy_warning("second argument of __builtin_expect() must be an integer constant");
		}
	} else if (name == YY___BUILTIN_PREFETCH) {
		// TODO: IR misses PREFETCH instruction(s) ???
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_HUGE_VAL || name == YY___BUILTIN_INF) {
		ir_val v;
		if (num_args != 0) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
#ifdef INFINITY
		v.d = INFINITY;
#else
		v.d = DBL_MAX;
#endif
		c_value_set_const(val, &c_type_double, IR_DOUBLE, v);
	} else if (name == YY___BUILTIN_HUGE_VALF || name == YY___BUILTIN_INFF) {
		ir_val v;
		if (num_args != 0) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
#ifdef INFINITY
		v.f = INFINITY;
#else
		v.f = FLT_MAX;
#endif
		v.u32_hi = 0;
		c_value_set_const(val, &c_type_float, IR_FLOAT, v);
	} else if (name == YY___BUILTIN_NAN) {
		ir_val v;
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type != &c_type_string && args[0].type != &c_type_const_string) {
			yy_error_fmt("wrong argument in %s() call", yy_sym2str(rcc, name));
		}
		v.d = nan((char*)args[0].u.val.ptr);
		c_value_set_const(val, &c_type_double, IR_DOUBLE, v);
	} else if (name == YY___BUILTIN_NANF) {
		ir_val v;
		if (num_args != 1) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type != &c_type_string && args[0].type != &c_type_const_string) {
			yy_error_fmt("wrong argument in %s() call", yy_sym2str(rcc, name));
		}
		v.f = nanf((char*)args[0].u.val.ptr);
		v.u32_hi = 0;
		c_value_set_const(val, &c_type_float, IR_FLOAT, v);
	} else if (name == YY___BUILTIN_ISUNORDERED) {
		ir_ref ref;

		if (num_args != 2) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (!C_IS_TYPE_FP(args[0].type) || !C_IS_TYPE_FP(args[1].type)) yy_error_fmt("wrong arguments in %s() call", yy_sym2str(rcc, name));
		ref = ir_fold2(rcc->active_ctx, IR_OPT(IR_UNORDERED, IR_BOOL), c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]));
		c_value_set_rval(val, &c_type_bool, IR_BOOL, ref);
	} else if (name == YY___BUILTIN_SHUFFLE) {
		uint32_t len;
		ir_ref op1, op2, op3;
		ir_type t;
		c_type *type;

		if (num_args != 2 && num_args != 3) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_VECTOR) yy_error("first argument of __builtin_shuffle() must be a vector");
		if (num_args == 2) {
			if (args[1].type->kind != C_TYPE_VECTOR || !C_IS_TYPE_INT(args[1].type->vec.type)) {
				yy_error("second argument of __builtin_shuffle() must be an integer vector");
			}
			len = args[1].type->vec.length;
			op1 = op2 = c_value_ref(rcc, &args[0]);
			op3 = c_value_ref(rcc, &args[1]);
		} else {
			if (args[1].type->kind != C_TYPE_VECTOR) yy_error("second argument of __builtin_shuffle() must be a vector");
			if (args[0].type->vec.type->kind != args[1].type->vec.type->kind) yy_error("first and second arguments of __builtin_shuffle() are vectors of different types");
			if (args[2].type->kind != C_TYPE_VECTOR || !C_IS_TYPE_INT(args[2].type->vec.type)) {
				yy_error("third argument of __builtin_shuffle() must be an integer vector");
			}
			len = args[2].type->vec.length;
			op1 = c_value_ref(rcc, &args[0]);
			op2 = c_value_ref(rcc, &args[1]);
			op3 = c_value_ref(rcc, &args[2]);
		}

		if ((uint32_t)args[0].type->vec.length == len) {
			type = (c_type*)args[0].type;
		} else {
			type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
			type->size = IR_VECTOR_SIZE(t);
			type->kind = C_TYPE_VECTOR;
			type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
			type->attr = c_align2attr(IR_MIN(type->size, 16)); /* 16 byte allgnment */
			type->vec.type = args[0].type->vec.type;
			type->vec.length = len;
		}
		t = c_type2ir(rcc, type);

		c_value_set_rval(val, type, t, ir_SHUFFLE(t, op1, op2, op3));
	} else if (name == YY___BUILTIN_SHUFFLEVECTOR) {
		ir_ref ref;
		ir_type t;
		uint32_t len1, len2, len, i;
		int8_t *ptr;
		c_type *type;

		if (num_args < 3) yy_error_fmt("wrong number of arguments in %s() call", yy_sym2str(rcc, name));
		if (args[0].type->kind != C_TYPE_VECTOR) yy_error("first argument of __builtin_shufflevector() must be a vector");
		if (args[1].type->kind != C_TYPE_VECTOR) yy_error("second argument of __builtin_shufflevector() must be a vector");
		if (args[0].type->vec.type->kind != args[1].type->vec.type->kind) yy_error("first and second arguments of __builtin_shufflevector() are vectors of different types");
		len = num_args - 2;
		if (len > 64 || (len & (len - 1)) != 0) yy_error("unsupported numver of vector elments in __builtin_shufflevector()");

		len1 = args[0].type->vec.length;
		len2 = args[1].type->vec.length;
		t = IR_MAKE_VECTOR_TYPE(IR_I8, len);
		ref = ir_const_vector(rcc->active_ctx, t);
		ptr = ir_long_const_ptr(rcc->active_ctx, ref);
		for (i = 0; i < len; i++) {
			if (!C_IS_TYPE_INT(args[i + 2].type)
			 || !c_value_is_const(&args[i + 2])
			 || (args[i + 2].u.val.u64 > len1 + len2
			  && (!C_IS_TYPE_SIGNED(args[i + 2].type)
			   || args[i + 2].u.val.i64 == -1))) {
				yy_error_fmt("%d-th argument of __builtin_shufflevector() is an invalid vector index", i + 3);
			}
			*ptr = (int8_t)args[i + 2].u.val.i8;
			ptr++;
		}
		ref = ir_long_const_commit(rcc->active_ctx, ref);

		if ((uint32_t)args[0].type->vec.length == len) {
			type = (c_type*)args[0].type;
		} else {
			type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
			type->size = IR_VECTOR_SIZE(t);
			type->kind = C_TYPE_VECTOR;
			type->flags = rcc->active_scope ? 0 : C_TYPE_GLOBAL;
			type->attr = c_align2attr(IR_MIN(type->size, 16)); /* 16 byte allgnment */
			type->vec.type = args[0].type->vec.type;
			type->vec.length = len;
		}
		t = c_type2ir(rcc, type);

		c_value_set_rval(val, type, t,
			ir_SHUFFLE(t, c_value_ref(rcc, &args[0]), c_value_ref(rcc, &args[1]), ref));
	} else {
		IR_ASSERT(0);
	}
	if (num_args > C_ALLOCA_PARAMS) ir_mem_free(args);
}

void c_do_builtin_constant_p(rcc_ctx *rcc, c_value *val)
{
	ir_val v;

	if (c_value_is_const(val)) {
		v.u64 = 1;
	} else {
		v.u64 = 0;
		if (c_value_is_lval(val)) c_value_rval(rcc, val);
		if (IR_IS_CONST_REF(val->u.ref) && !IR_IS_SYM_CONST(rcc->active_ctx->ir_base[val->u.ref].op)) {
			v.u64 = 1;
		}
	}
	c_value_set_const(val, &c_type_i32, IR_I32, v);
}

void c_do_builtin_classify_type(rcc_ctx *rcc, c_value *val, const c_type *type)
{
	enum gcc_type_class {
		no_type_class = -1,
		void_type_class, integer_type_class, char_type_class,
		enumeral_type_class, boolean_type_class,
		pointer_type_class, reference_type_class, offset_type_class,
		real_type_class, complex_type_class,
		function_type_class, method_type_class,
		record_type_class, union_type_class,
		array_type_class, string_type_class,
		lang_type_class, opaque_type_class,
		bitint_type_class, vector_type_class
	};
	ir_val v;

	switch (type->kind) {
		case C_TYPE_VOID:
			v.i64 = void_type_class;
			break;
		case C_TYPE_U8:
		case C_TYPE_U16:
		case C_TYPE_U32:
		case C_TYPE_UL:
		case C_TYPE_ULL:
		case C_TYPE_I8:
		case C_TYPE_I16:
		case C_TYPE_I32:
		case C_TYPE_IL:
		case C_TYPE_ILL:
			v.i64 = integer_type_class;
			break;
		case C_TYPE_CHAR:
			v.i64 = char_type_class;
			break;
		case C_TYPE_ENUM:
			v.i64 = enumeral_type_class;
			break;
		case C_TYPE_BOOL:
			v.i64 = boolean_type_class;
			break;
		case C_TYPE_POINTER:
			v.i64 = pointer_type_class;
			break;
		case C_TYPE_FLOAT:
		case C_TYPE_DOUBLE:
		case C_TYPE_LONG_DOUBLE:
			v.i64 = real_type_class;
			break;
		case C_TYPE_FLOAT_COMPLEX:
		case C_TYPE_DOUBLE_COMPLEX:
		case C_TYPE_LONG_DOUBLE_COMPLEX:
			v.i64 = complex_type_class;
			break;
		case C_TYPE_FUNC:
			v.i64 = function_type_class;
			break;
		case C_TYPE_STRUCT:
			v.i64 = record_type_class;
			break;
		case C_TYPE_UNION:
			v.i64 = union_type_class;
			break;
		case C_TYPE_ARRAY:
			if (type == &c_type_string
			 || type == &c_type_lstring
			 || type == &c_type_string_u16
			 || type == &c_type_string_u32) {
				v.i64 = string_type_class;
			} else {
				v.i64 = array_type_class;
			}
			break;
		case C_TYPE_VECTOR:
			v.i64 = vector_type_class;
			break;
		default:
			v.i64 = -1;
			break;
	}
	c_value_set_const(val, &c_type_i32, IR_I32, v);
}

void c_do_builtin_types_compatible_p(rcc_ctx *rcc, c_value *val, const c_type *type)
{
	ir_val v;

	v.i64 = c_compatible_types(val->type, type, 1, 0);
	if (v.i64) {
		c_type_kind t1_kind = val->type->kind;
		c_type_kind t2_kind = type->kind;
		if ((t1_kind == C_TYPE_ARRAY && t2_kind == C_TYPE_POINTER)
		 || (t1_kind == C_TYPE_POINTER && t2_kind == C_TYPE_ARRAY)) {
			v.i64 = 0;
		}
	}
	c_value_set_const(val, &c_type_i32, IR_I32, v);
}

void c_do_builtin_va_arg(rcc_ctx *rcc, c_value *val, const c_type *type)
{
	ir_type t;
	ir_ref ref;

	if (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION) {
		ir_ref alloca;
		ir_type types[MAX_ABI_TYPES];
		int n;

		if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type)) {
			yy_error("second argument to \"__builtin_va_arg\" is of incomplete type");
		}
		n = c_abi_lower_struct_arg(type, types);
		if (n == 1) {
			t = types[0];
			ref = ir_VA_ARG(c_va_list_addr(rcc, val), t);
			alloca = c_do_alloca(rcc, type->size, c_attr2align(type->attr), 0);
			ir_STORE(alloca, ref);
			c_value_set_lval(val, type, IR_ADDR, alloca);
		} else {
			uint32_t align = c_attr2align(type->attr);

			IR_ASSERT(n == 0);
			if (align < sizeof(void*)) align = sizeof(void*);
			if (align > 128) yy_error("algnment must be less than 128");
			if (align > 16) {
				yy_warning("passing structure with alignnment greater than 16 is not implemented yet");
				align = 16;
			}
			ref = ir_VA_ARG_EX(c_va_list_addr(rcc, val), IR_ADDR, type->size, align);
			c_value_set_lval(val, type, IR_ADDR, ref);
		}
	} else {
		if (type->kind == C_TYPE_VOID) yy_error("second argument to \"__builtin_va_arg\" is of incomplete type \"void\"");
		t = c_type2ir(rcc, type);
		ref = ir_VA_ARG(c_va_list_addr(rcc, val), t);
		c_value_set_rval(val, type, t, ref);
	}
}

void c_do_builtin_convertvector(rcc_ctx *rcc, c_value *val, const c_type *type)
{
	ir_type t;
	ir_op op = IR_NOP;

	if (val->type->kind != C_TYPE_VECTOR) {
		yy_error("first argument of __builtin_convertvector() must be a vector");
	} else if (type->kind != C_TYPE_VECTOR) {
		yy_error("second argument of __builtin_convertvector() must be a vector type");
	} else if (val->type->vec.length != type->vec.length) {
		yy_error("vector and type arguments of __builtin_convertvector() have different number of elements");
	} else if (val->type->vec.type->kind == type->vec.type->kind) {
		/* convert to the same type */
		return;
	}

	if (C_IS_TYPE_INT(type->vec.type)) {
		if (C_IS_TYPE_INT(val->type->vec.type)) {
			if (type->vec.type->size < val->type->vec.type->size) {
				op = IR_TRUNC;
			} else if (type->vec.type->size == val->type->vec.type->size) {
				op = IR_BITCAST;
			} else if (C_IS_TYPE_SIGNED(val->type->vec.type)) {
				op = IR_SEXT;
			} else {
				op = IR_ZEXT;
			}
		} else if (C_IS_TYPE_FP(val->type->vec.type)) {
			op = IR_FP2INT;
		} else {
			IR_ASSERT(0);
		}
	} else if (C_IS_TYPE_FP(type->vec.type)) {
		if (C_IS_TYPE_INT(val->type->vec.type)) {
			op = IR_INT2FP;
		} else if (C_IS_TYPE_FP(val->type->vec.type)) {
			op = IR_FP2FP;
		} else {
			IR_ASSERT(0);
		}
	} else {
		IR_ASSERT(0);
	}

	IR_ASSERT(op != IR_NOP);
	t = c_type2ir(rcc, type);
	c_value_set_rval(val, type, t, ir_fold1(rcc->active_ctx, IR_OPT(op, t), c_value_ref(rcc, val)));
}

static bool c_do_convert_builtin(rcc_ctx *rcc, c_value *func, int32_t num_args, ir_ref *arg_refs)
{
	if (c_value_is_ref(func)) {
		const ir_insn *func_insn = &rcc->active_ctx->ir_base[func->u.ref];
		c_name sym_name;
		ir_ref ref;

		IR_ASSERT(IR_IS_EXT_STR(func_insn->val.name));
		sym_name = IR_EXT_STR(func_insn->val.name);
		if (sym_name == YY_ALLOCA) {
			if (num_args == 1) {
				ref = ir_ALLOCA(arg_refs[0]);
				c_value_set_rval(func, &c_type_ptr, IR_ADDR, ref);
				return 1;
			}
		} else if (sym_name == YY_ABS) {
			if (num_args == 1) {
				ref = c_do_cast_ref(rcc, IR_I32, arg_refs[0]);
				ref = ir_ABS_I32(ref);
				c_value_set_rval(func, &c_type_i32, IR_I32, ref);
				return 1;
			}
		} else if (sym_name == YY_LABS) {
			if (num_args == 1) {
				ref = c_do_cast_ref(rcc, IR_LONG, arg_refs[0]);
				ref = ir_ABS(IR_LONG, ref);
				c_value_set_rval(func, &c_type_il, IR_LONG, ref);
				return 1;
			}
		} else if (sym_name == YY_LLABS) {
			if (num_args == 1) {
				ref = c_do_cast_ref(rcc, IR_I64, arg_refs[0]);
				ref = ir_ABS_I64(ref);
				c_value_set_rval(func, &c_type_i64, IR_I64, ref);
				return 1;
			}
		} else if (sym_name == YY_FABS) {
			if (num_args == 1) {
				ref = c_do_cast_ref(rcc, IR_DOUBLE, arg_refs[0]);
				ref = ir_ABS_D(ref);
				c_value_set_rval(func, &c_type_double, IR_DOUBLE, ref);
				return 1;
			}
		} else if (sym_name == YY_FABSF) {
			if (num_args == 1) {
				ref = c_do_cast_ref(rcc, IR_FLOAT, arg_refs[0]);
				ref = ir_ABS_F(ref);
				c_value_set_rval(func, &c_type_float, IR_FLOAT, ref);
				return 1;
			}
#ifdef _WIN32
		} else if (sym_name == YY___VA_START) {
			if (num_args == 1 || num_args == 2) {
				ir_VA_START(arg_refs[0]);
				c_value_set_rval(func, &c_type_void, IR_VOID, IR_VOID);
				return 1;
			}
#endif
		}
	}

	return 0;
}

static ir_ref ir_inline_call(rcc_ctx *rcc, ir_ctx *ctx, ir_ctx *func_ctx, uint32_t num_args, ir_ref *args)
{
	ir_ref *buf = alloca(sizeof(ir_ref) * (func_ctx->consts_count + func_ctx->insns_count * 2 - 1));
	ir_ref *xlat = buf + func_ctx->consts_count - 1;
	ir_ref *xlat2 = xlat + func_ctx->insns_count;
	ir_ref ret = IR_UNUSED;
	ir_ref i, j, op1, op2, op3;
	ir_ref start = IR_UNUSED, block_begin = IR_UNUSED;
	bool has_var = 0, has_alloca = 0, has_copy = 0;
	ir_insn *insn;
	bool add_phi = 0;
	ir_list bp_list;

	/* Copy costants */
	for (i = 1 - func_ctx->consts_count, insn = func_ctx->ir_base + i; i < IR_TRUE; i++, insn++) {
		ir_val val = insn->val;
		uint32_t optx = insn->optx;
		ir_op op = optx & IR_OPT_OP_MASK;

		if (op == IR_FUNC || op == IR_SYM || op == IR_STR) {
			if (!IR_IS_EXT_STR(val.str)) {
				size_t len;
				const char *str = ir_get_strl(func_ctx, val.str, &len);

				val.str = ir_stringl(ctx, str, len);
			}
		} else if (op == IR_LABEL) {
			val.u64 = c_create_label_str(rcc, ++rcc->c_label_num);
		}
		if (op == IR_FUNC || op == IR_FUNC_ADDR) {
			ir_ref proto = insn->proto;

			if (proto) {
				size_t len;
				const char *str = ir_get_strl(func_ctx, proto, &len);

				proto = ir_stringl(ctx, str, len);
				optx = IR_OPTX(op, IR_OPT_TYPE(optx), proto);
			}
		}
		if (op == IR_LONG_CONST) {
			xlat[i] = ir_long_const(ctx, insn->type, insn->long_const_size);
			memcpy(ir_long_const_ptr(ctx, xlat[i]), insn + 1, insn->long_const_size);
			i += IR_ALIGNED_SIZE(insn->long_const_size, sizeof(ir_insn)) / sizeof(ir_insn);
			insn += IR_ALIGNED_SIZE(insn->long_const_size, sizeof(ir_insn)) / sizeof(ir_insn);
		} else {
			xlat[i] = ir_const_ex(ctx, val, IR_OPT_TYPE(optx), optx);
		}
	}
	xlat[IR_TRUE] = IR_TRUE;
	xlat[IR_FALSE] = IR_FALSE;
	xlat[IR_NULL] = IR_NULL;
	xlat2[IR_UNUSED] = xlat[IR_UNUSED] = IR_UNUSED;

	/* Link argemnts and parameters */
	i = 2;
	insn = func_ctx->ir_base + i;
	while (insn->op == IR_PARAM) {
		IR_ASSERT((uint32_t)i < num_args + 2);
		ir_ref arg = args[i - 2];
		ir_insn *arg_insn = &ctx->ir_base[arg];

		if (arg_insn->op == IR_ARGVAL) {
			arg = arg_insn->op1;
			has_copy = 1;
		}
		xlat2[i] = xlat[i] = arg;
		insn++;
		i++;
	}
	j = i;

	/* Check if the inlined function expands stack through VAR or ALLOCA */
	while (i < func_ctx->insns_count) {
		ir_ref n = insn->inputs_count;
		if (n <= 3) {
			if (insn->op == IR_VAR) {
				if (insn->op1 == 1) {
					/* VAR linked to START */
					has_var = 1;
				} else {
					has_alloca = 1;
				}
				break;
			} else if (insn->op == IR_ALLOCA) {
				has_alloca = 1;
				break;
			}
			insn++;
			i++;
		} else {
			n = ir_insn_inputs_to_len(n);
			insn += n;
			i += n;
		}
	}

	/* Wrap inlined code with BLOCK_BEGIN/BLOCK_END if necessary */
	if (has_var) {
		block_begin = ir_emit1(ctx, IR_OPT(IR_BLOCK_BEGIN, IR_ADDR), ctx->control);
		xlat2[1] = xlat[1] = ctx->control = start = ir_emit1(ctx, IR_BEGIN, ir_emit1(ctx, IR_END, block_begin));
	} else if (has_alloca || has_copy) {
		start = block_begin = xlat2[1] = xlat[1] = ctx->control = ir_emit1(ctx, IR_OPT(IR_BLOCK_BEGIN, IR_ADDR), ctx->control);
	} else {
		start = xlat2[1] = xlat[1] = ctx->control;
	}

	if (has_copy) {
		i = 2;
		insn = func_ctx->ir_base + i;
		while (insn->op == IR_PARAM) {
			IR_ASSERT((uint32_t)i < num_args + 2);
			ir_ref arg = args[i - 2];
			ir_insn *arg_insn = &ctx->ir_base[arg];

			if (arg_insn->op == IR_ARGVAL) {
				/* copy struct passed by value */
				int size = ir_const_size_t(ctx, arg_insn->op2);
				ir_ref dst = ir_ALLOCA(size);
				ir_ref src = arg_insn->op1;
				ir_ref op3 = arg_insn->op3;
				MAKE_NOP(arg_insn);
				ir_memcpy(rcc, dst, src, size, op3);
				xlat2[i] = xlat[i] = dst;
				xlat2[1] = xlat[1] = ctx->control;
			}
			insn++;
			i++;
		}
	}

	/* Copy instuctions */
	bp_list.len = 0;
	i = j;
	insn = func_ctx->ir_base + i;
	while (i < func_ctx->insns_count) {
		ir_op op = insn->op;
		uint32_t flags = ir_op_flags[op];

		if (!IR_OP_HAS_VAR_INPUTS(flags)) {
			op1 = insn->op1;
			op2 = insn->op2;
			op3 = insn->op3;
			if (insn->inputs_count >= 1) {
				IR_ASSERT(op1 < i);
				op1 = IR_OPND_KIND(flags, 1) == IR_OPND_DATA ? xlat[op1] : xlat2[op1];
				if (insn->inputs_count >= 2) {
					IR_ASSERT(op2 < i);
					op2 = IR_OPND_KIND(flags, 2) == IR_OPND_DATA ? xlat[op2] : xlat2[op2];
					if (insn->inputs_count >= 3) {
						IR_ASSERT(op3 < i);
						op3 = IR_OPND_KIND(flags, 3) == IR_OPND_DATA ? xlat[op3] : xlat2[op3];
						IR_ASSERT(insn->inputs_count <= 3);
					}
				}
			}
			if (IR_IS_FOLDABLE_OP(op)) {
				if (op == IR_PROTO) {
					size_t len;
					const char *str = ir_get_strl(func_ctx, op2, &len);
					op2 = ir_stringl(ctx, str, len);
				}
				xlat2[i] = xlat[i] = ir_fold(ctx, insn->opt, op1, op2, op3);
			} else if (op == IR_RETURN) {
				ctx->control = op1;
				if (insn->op2) {
					IR_ASSERT(func_ctx->ret_type);
					op2 = insn->op2;
					IR_ASSERT(op2 < i);
					op2 = xlat[op2];
					ir_END_PHI_list(ret, op2);
					add_phi = 1;
				} else {
					IR_ASSERT(!func_ctx->ret_type);
					ir_END_list(ret);
				}
				ctx->control = IR_UNUSED;
				xlat2[i] = xlat[i] = IR_UNUSED;
			} else if (op == IR_UNREACHABLE) {
				ctx->control = op1;
				_ir_UNREACHABLE(ctx);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_IJMP) {
				ctx->control = op1;
				_ir_IJMP(ctx, op2);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_BEGIN) {
				ctx->control = IR_UNUSED;
				if (op2) {
					op2 = xlat[op2];
					ctx->control = ir_emit2(ctx, IR_BEGIN, op1, op2);
				} else if (func_ctx->use_lists[i].count != 1) {
					ctx->control = ir_emit1(ctx, IR_BEGIN, op1);
				} else {
					_ir_BEGIN(ctx, op1);
				}
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_IF) {
				ctx->control = op1;
				xlat2[i] = xlat[i] = _ir_IF(ctx, op2);
				ctx->control = IR_UNUSED;
			} else if (op == IR_GUARD) {
				ctx->control = op1;
				_ir_GUARD(ctx, op2, op3);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_GUARD_NOT) {
				ctx->control = op1;
				_ir_GUARD_NOT(ctx, op2, op3);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_VLOAD) {
				ctx->control = op1;
				xlat[i] = _ir_VLOAD(ctx, insn->type, op2);
				xlat2[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_VSTORE) {
				ctx->control = op1;
				_ir_VSTORE(ctx, op2, op3);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_LOAD) {
				// TODO: constant folding ???
				ctx->control = op1;
				xlat[i] = _ir_LOAD(ctx, insn->type, op2);
				xlat2[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_STORE) {
				ctx->control = op1;
				_ir_STORE(ctx, op2, op3);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_VAR) {
				if (!IR_IS_EXT_STR(op2)) {
					size_t len;
					const char *str = ir_get_strl(func_ctx, op2, &len);

					op2 = ir_stringl(ctx, str, len);
				}
				if (insn->op1 == 1) op1 = start;
				xlat2[i] = xlat[i] = ir_emit(ctx, insn->opt, op1, op2, op3);
			} else {
				IR_ASSERT(op != IR_VA_START);
				xlat2[i] = xlat[i] = ir_emit(ctx, insn->opt, op1, op2, op3);
				if (flags & (IR_OP_FLAG_BB_END|IR_OP_FLAG_TERMINATOR)) {
					ctx->control = IR_UNUSED;
				}
			}
			insn++;
			i++;
		} else {
			ir_ref ref, *p, input, n = insn->inputs_count;
			ir_insn *new_insn;

			xlat2[i] = xlat[i] = ref = ir_emit_N(ctx, insn->opt, insn->inputs_count);
			new_insn = &ctx->ir_base[ref];
			memcpy(new_insn->ops + 1, insn->ops + 1, sizeof(ir_ref) * insn->inputs_count);
			j = n;
			p = new_insn->ops + 1;
			for (j = 1; j <= n; p++, j++) {
				input = *p;
				if (input <= i) {
					*p = IR_OPND_KIND(flags, j) == IR_OPND_DATA ? xlat[input] : xlat2[input];
				} else {
					/* backward references are going to be updated trough xlat/xlat2 on the next step */
					IR_ASSERT(insn->op == IR_LOOP_BEGIN || insn->op == IR_MERGE || insn->op == IR_PHI);
					if (!bp_list.len) {
						ir_list_init(&bp_list, 32);
					}
					ir_list_push(&bp_list, ref);
					ir_list_push(&bp_list, j);
				}
			}
			if (n <= 3) {
				insn++;
				i++;
			} else {
				n = ir_insn_inputs_to_len(n);
				insn += n;
				i += n;
			}
			if (op == IR_TAILCALL) {
				ctx->control = ref;
				if (new_insn->type) {
					IR_ASSERT(func_ctx->ret_type);
					ir_END_PHI_list(ret, ref);
					add_phi = 1;
				} else if (func_ctx->ret_type) {
					ir_val val;

					val.u64 = 0;
					ref = ir_const(rcc->active_ctx, val, func_ctx->ret_type);
					ir_END_PHI_list(ret, ref);
					add_phi = 1;
				} else {
					ir_END_list(ret);
				}
				ctx->control = IR_UNUSED;
				xlat2[i] = xlat[i] = IR_UNUSED;
				new_insn->op = IR_CALL;
				IR_ASSERT(insn->op == IR_UNREACHABLE);
				insn++;
				i++;
			}
		}
	}

	/* Update inputs in LOOP_BEGIN and PHI (they may be backward references) */
	if (ir_list_len(&bp_list)) {
		do {
			ir_ref ref, *p;
			uint32_t flags;

			j = ir_list_pop(&bp_list);
			ref = ir_list_pop(&bp_list);
			insn = &ctx->ir_base[ref];
			flags = ir_op_flags[insn->op];
			p = insn->ops + j;
			*p = IR_OPND_KIND(flags, j) == IR_OPND_DATA ? xlat[*p] : xlat2[*p];
		} while (ir_list_len(&bp_list));
		ir_list_free(&bp_list);
	}

	/* Merge all RETURN values */
	if (ret) {
		if (add_phi) {
			ret = ir_PHI_list(ret);
		} else  {
			ir_MERGE_list(ret);
			ret = IR_UNUSED;
		}
	} else {
		ir_BEGIN(IR_UNUSED);
		ret = IR_UNUSED;
		if (func_ctx->ret_type) {
			ir_val val;

			val.u64 = 0;
			ret = ir_const(rcc->active_ctx, val, func_ctx->ret_type);
		}
	}

	/* Add BLOCK_END if necessary */
	if (block_begin) {
		ctx->control = ir_emit2(ctx, IR_BLOCK_END, ctx->control, block_begin);
	}

	rcc->c_last_call_func_type = NULL;
	return ret;
}

void c_do_call(rcc_ctx *rcc, c_value *func, uint32_t num_args, c_value *args, c_value *res)
{
	const c_type *func_type, *ret_type;
	ir_type _ret_type;
	ir_ref ref, ret_struct = IR_UNUSED;
	ir_ref *arg_refs = NULL;
	int j = 0;
	uint8_t inlining = C_VAL_INLINE;

	rcc->c_last_call_func_type = func->type;
	c_value_rval(rcc, func);
	if (func->type->kind == C_TYPE_FUNC) {
		func_type = func->type;
	} else if (func->type->kind == C_TYPE_POINTER
	 && func->type->pointer.type->kind == C_TYPE_FUNC) {
		func_type = func->type->pointer.type;
	} else {
		if (rcc->yy_flags & PP_EVAL_EXPRESSION) {
			ir_val val;
			val.u64 = 0;
			c_value_set_const(func, &c_type_i32, IR_I32, val);
			return;
		}
		yy_error("called object is not a function or function pointer");
	}
	IR_ASSERT(func->u.type == IR_ADDR);
	if ((func_type->func.ret_type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, func_type->func.ret_type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(func_type->func.ret_type->kind), yy_sym2str(rcc, func_type->func.ret_type->tag));
	}
	ret_type = func_type->func.ret_type;
	if (ret_type->kind == C_TYPE_STRUCT || ret_type->kind == C_TYPE_UNION) {
		ir_type types[MAX_ABI_TYPES];
		int n = c_abi_lower_struct_ret(ret_type, types);

		if (n == 1) {
			_ret_type = types[0];
			ret_struct = c_do_alloca(rcc, ret_type->size, c_attr2align(ret_type->attr), 0);
		} else {
			IR_ASSERT(n == 0);
			_ret_type = IR_ADDR;
			j = 1;
			ret_struct = c_do_alloca(rcc, ret_type->size, c_attr2align(ret_type->attr), 0);
		}
	} else {
		_ret_type = c_type2ir(rcc, ret_type);
	}
	if (num_args != func_type->func.num_params) {
		if (func_type->attr & C_ATTR_OLD_FUNC) {
			inlining = 0;
		} else if (num_args < func_type->func.num_params) {
			if (c_value_is_ref(func)
			 && IR_IS_CONST_REF(func->u.ref)
			 && rcc->active_ctx->ir_base[func->u.ref].op == IR_FUNC) {
				yy_error_fmt("too few arguments to function \"%s\"",
					ir_get_str(rcc->active_ctx, rcc->active_ctx->ir_base[func->u.ref].val.str));
			} else {
				yy_error("too few arguments");
			}
		} else if (!(func_type->attr & C_ATTR_VARIADIC)) {
			if (c_value_is_ref(func)
			 && IR_IS_CONST_REF(func->u.ref)
			 && rcc->active_ctx->ir_base[func->u.ref].op == IR_FUNC) {
				yy_error_fmt("too many arguments to function \"%s\"",
					ir_get_str(rcc->active_ctx, rcc->active_ctx->ir_base[func->u.ref].val.str));
			} else {
				yy_error("too many arguments");
			}
		}
	}
	if (num_args > 0) {
		uint32_t i;

		arg_refs = alloca(sizeof(ir_ref) * (num_args + j));
		if (j) {
			arg_refs[0] = ret_struct;
		}
		for (i = 0; i < num_args; i++) {
			c_value_rval(rcc, &args[i]);
			if (i < func_type->func.num_params) {
				if (func_type->func.params[i].type != args[i].type) {
					c_do_check_cvt(rcc, func_type->func.params[i].type, &args[i], i + 1);
				}
			} else {
				if (args[i].type->kind == C_TYPE_FLOAT) {
					c_do_fp2fp(rcc, &c_type_double, IR_DOUBLE, &args[i]);
				} else if ((C_IS_TYPE_INT(args[i].type) || args[i].type->kind == C_TYPE_ENUM)
				 && args[i].type->size < 4) {
					if (C_IS_TYPE_SIGNED(args[i].type)) {
						c_do_sext(rcc, &c_type_i32, IR_I32, &args[i]);
					} else {
						c_do_zext(rcc, &c_type_u32, IR_U32, &args[i]);
					}
				} else if (args[i].type->kind == C_TYPE_VOID) {
					yy_error("invalid use of void expression");
				}
			}
			if (args[i].type->kind == C_TYPE_STRUCT || args[i].type->kind == C_TYPE_UNION) {
				ir_type types[MAX_ABI_TYPES];
				int n = c_abi_lower_struct_arg(args[i].type, types);

				if (n == 1) {
					if (IR_IS_TYPE_INT(types[0])) {
						if (ir_type_size[types[0]] == 1) {
							arg_refs[i + j] = ir_LOAD_U8(args[i].u.ref);
						} else if (ir_type_size[types[0]] == 2) {
							arg_refs[i + j] = ir_LOAD_U16(args[i].u.ref);
						} else if (ir_type_size[types[0]] <= 4) {
							arg_refs[i + j] = ir_LOAD_U32(args[i].u.ref);
						} else {
							arg_refs[i + j] = ir_LOAD_U64(args[i].u.ref);
						}
					} else {
						arg_refs[i + j] = ir_LOAD(types[0], args[i].u.ref);
					}
				} else {
					uint32_t align;

					IR_ASSERT(n == 0);
					align = c_attr2align(args[i].type->attr);
					if (align > 16) {
						yy_warning("passing structure with alignnment greater than 16 is not implemented yet");
						align = 16;
					}
					arg_refs[i + j] = ir_emit3(rcc->active_ctx, IR_OPT(IR_ARGVAL, IR_ADDR), args[i].u.ref,
						args[i].type->size, align);
				}
			} else {
				arg_refs[i + j] = c_value_ref(rcc, &args[i]);
			}
		}
	} else if (j) {
		arg_refs = alloca(sizeof(ir_ref));
		arg_refs[0] = ret_struct;
	}
	if ((func->u.op & inlining)
	 && res->u.proto != IR_TAILCALL
	 && (!(func->type->attr & C_ATTR_NORETURN) || func->type->func.ret_type->kind == C_TYPE_VOID)
	 && rcc->active_ctx->fixed_regset == ((ir_ctx*)func->u.val.ptr)->fixed_regset
	 && rcc->active_ctx->fixed_save_regset == ((ir_ctx*)func->u.val.ptr)->fixed_save_regset) {
		ref = ir_inline_call(rcc, rcc->active_ctx, (ir_ctx*)func->u.val.ptr, num_args + j, arg_refs);
	} else if (!(func->u.op & C_VAL_BUILTIN)) {
		ref = c_value_ref(rcc, func);
		if (rcc->active_ctx->ir_base[ref].op != IR_FUNC
		 && rcc->active_ctx->ir_base[ref].op != IR_FUNC_ADDR
		 && rcc->active_ctx->ir_base[ref].op != IR_PROTO) {
			const c_type *type = func->type;
			if (type->kind == C_TYPE_POINTER) type = type->pointer.type;
			ref = ir_emit2(rcc->active_ctx, IR_OPT(IR_PROTO, IR_ADDR), ref, c_type2proto(rcc, type, 0));
		}
		ref = ir_CALL_N(_ret_type, ref, num_args + j, arg_refs);
		rcc->c_last_call_func_type = func->type;
		if (func->type->attr & C_ATTR_NORETURN) {
			ir_val val;

			ir_UNREACHABLE();
			ir_BEGIN(IR_UNUSED);
			val.u64 = 0;
			c_value_set_const(func, ret_type, _ret_type, val);
			return;
		}
	} else {
		if (c_do_convert_builtin(rcc, func, num_args + j, arg_refs)) {
			if (num_args > C_ALLOCA_PARAMS) ir_mem_free(args);
			return;
		}

		ref = c_value_ref(rcc, func);
		if (rcc->active_ctx->ir_base[ref].op != IR_FUNC
		 && rcc->active_ctx->ir_base[ref].op != IR_FUNC_ADDR
		 && rcc->active_ctx->ir_base[ref].op != IR_PROTO) {
			const c_type *type = func->type;
			if (type->kind == C_TYPE_POINTER) type = type->pointer.type;
			ref = ir_emit2(rcc->active_ctx, IR_OPT(IR_PROTO, IR_ADDR), ref, c_type2proto(rcc, type, 0));
		}
		ref = ir_CALL_N(_ret_type, ref, num_args + j, arg_refs);
		rcc->c_last_call_func_type = func->type;
	}
	if (ret_type->kind == C_TYPE_STRUCT || ret_type->kind == C_TYPE_UNION) {
		if (!j) {
			ir_STORE(ret_struct, ref);
			c_value_set_lval(func, ret_type, _ret_type, ret_struct);
		} else {
			c_value_set_lval(func, ret_type, _ret_type, ret_struct);
		}
	} else {
		c_value_set_rval(func, ret_type, _ret_type, ref);
	}
	if (num_args > C_ALLOCA_PARAMS) ir_mem_free(args);
}

static bool c_try_convert_const_fp2fp(rcc_ctx *rcc, const c_type *type, c_value *val)
{
	ir_val v;

	IR_ASSERT(type->kind == C_TYPE_FLOAT && val->type->kind == C_TYPE_DOUBLE);
	v.f = (float)val->u.val.d;
	v.u32_hi = 0;
	if ((double)v.f != val->u.val.d) return 0;

	c_value_set_const(val, type, IR_FLOAT, v);
	return 1;
}

static bool c_try_convert_const_int2fp(rcc_ctx *rcc, const c_type *type, c_value *val)
{
	ir_val v;

	switch (type->kind) {
		case C_TYPE_FLOAT:
			if (C_IS_TYPE_SIGNED(val->type)) {
				v.f = (float)val->u.val.i64;
				v.u32_hi = 0;
				if ((int64_t)v.f != val->u.val.i64) return 0;
			} else {
				v.f = (float)val->u.val.u64;
				v.u32_hi = 0;
				if ((uint64_t)v.f != val->u.val.u64) return 0;
			}
			c_value_set_const(val, type, IR_FLOAT, v);
			return 1;
		case C_TYPE_DOUBLE:
			if (C_IS_TYPE_SIGNED(val->type)) {
				v.d = (double)val->u.val.i64;
				if ((int64_t)v.d != val->u.val.i64) return 0;
			} else {
				v.d = (double)val->u.val.u64;
				if ((uint64_t)v.d != val->u.val.u64) return 0;
			}
			c_value_set_const(val, type, IR_DOUBLE, v);
			return 1;
		default:
			IR_ASSERT(0);
			return 0;
	}
}

static bool c_try_convert_const_int2int(rcc_ctx *rcc, const c_type *type, c_value *val)
{
	ir_val v;

	v.u64 = val->u.val.u64;
	if (type->size < 8) {
		uint32_t shift = (8 - type->size) * 8;

		if (C_IS_TYPE_SIGNED(type)) {
			v.i64 = (int64_t)(v.u64 << shift) >> shift;
		} else {
			v.u64 = (v.u64 << shift) >> shift;
		}
	}
	if (v.u64 != val->u.val.u64) return 0;
	c_value_set_const(val, type, c_type2ir(rcc, type), v);
	return 1;
}

static bool c_do_splat(rcc_ctx *rcc, const c_type *type, c_value *val)
{
	ir_type t;

	IR_ASSERT(type->kind == C_TYPE_VECTOR);

	if (C_IS_TYPE_KIND_FP(type->vec.type->kind)) {
		if (C_IS_TYPE_KIND_FP(val->type->kind)) {
			if (val->type->size > type->vec.type->size) {
				if (!c_value_is_const(val) || !c_try_convert_const_fp2fp(rcc, type->vec.type, val)) {
					yy_error("cannot convert value to a vector (conversion involves truncation)");
				}
			} else if (val->type->size < type->vec.type->size) {
				c_do_fp2fp(rcc, type->vec.type, c_type2ir(rcc, type->vec.type), val);
			}
		} else if (C_IS_TYPE_KIND_INT(val->type->kind)) {
			if (val->type->size >= type->vec.type->size) {
				if (!c_value_is_const(val) || !c_try_convert_const_int2fp(rcc, type->vec.type, val)) {
					yy_error("cannot convert value to a vector (conversion involves truncation)");
				}
			} else {
				c_do_int2fp(rcc, type->vec.type, c_type2ir(rcc, type->vec.type), val);
			}
		} else {
			return 0;
		}
	} else {
		IR_ASSERT(C_IS_TYPE_KIND_INT(type->vec.type->kind));
		if (C_IS_TYPE_KIND_FP(val->type->kind)) {
			yy_error("cannot convert value to a vector");
		} else if (C_IS_TYPE_KIND_INT(val->type->kind)) {
			if (val->type->size > type->vec.type->size) {
				if (!c_value_is_const(val) || !c_try_convert_const_int2int(rcc, type->vec.type, val)) {
					yy_error("cannot convert value to a vector (conversion involves truncation)");
				}
			} else if (val->type->size < type->vec.type->size) {
				if (C_IS_TYPE_KIND_SIGNED(val->type->kind)) {
					c_do_sext(rcc, type->vec.type, c_type2ir(rcc, type->vec.type), val);
				} else {
					c_do_zext(rcc, type->vec.type, c_type2ir(rcc, type->vec.type), val);
				}
			} else if (val->type->kind != type->vec.type->kind) {
				c_do_bitcast(rcc, type->vec.type, c_type2ir(rcc, type->vec.type), val);
			}
		} else {
			return 0;
		}
	}

	t = c_type2ir(rcc, type);
	c_value_set_rval(val, type, t, ir_SPLAT(t, c_value_ref(rcc, val)));

	return 1;
}

static const c_type *c_common_type(rcc_ctx *rcc, yy_sym sym, c_value *op1, c_value *op2)
{
	const c_type *op1_type = op1->type;
	const c_type *op2_type = op2->type;
	c_type_kind t1 = op1_type->kind;
	c_type_kind t2 = op2_type->kind;

	if (t1 == C_TYPE_POINTER || t1 == C_TYPE_ARRAY) {
		if (sym == YY__PLUS) {
			if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
				if (op2_type->size != op1_type->size) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op2);
				}
				if (op1_type->kind == C_TYPE_ARRAY) return c_create_pointer_type(rcc, op1_type->array.type);
				return op1_type;
			}
		} else if (sym == YY__MINUS) {
			if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
				if (op2_type->size != op1_type->size) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op2);
				}
				if (op1_type->kind == C_TYPE_ARRAY) return c_create_pointer_type(rcc, op1_type->array.type);
				return op1_type;
			} else if ((t2 == C_TYPE_POINTER || t2 == C_TYPE_ARRAY)
			 && c_compatible_types(op1_type->pointer.type, op2_type->pointer.type, 1, 0)) {
				if (op1_type->kind == C_TYPE_ARRAY) return c_create_pointer_type(rcc, op1_type->array.type);
				return op1_type;
			}
		} else if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
			|| sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__COLON) {
			if (t2 == C_TYPE_POINTER || t2 == C_TYPE_ARRAY) {
				if (op1_type->pointer.type->kind == C_TYPE_VOID) {
					return op2_type;
				} else if (op2_type->pointer.type->kind == C_TYPE_VOID) {
					return op1_type;
				} else if (!c_compatible_types(op1_type->pointer.type, op2_type->pointer.type, 1, 0)) {
					if (sym == YY__COLON) return NULL;
					yy_warning("comparison of distinct pointer types lacks a cast");
				}
				if (sym == YY__COLON && op1_type->kind == C_TYPE_ARRAY) {
					return c_create_pointer_type(rcc, op1_type->array.type);
				}
				// TODO: select best type ???
				return op1_type;
			} else if (C_IS_TYPE_INT(op2_type)) {
				if (c_value_is_const(op2) && op2->u.val.u64 == 0) {
					op2->type = &c_type_ptr;
					op2->u.type = IR_ADDR;
					return op1_type;
				}
				if (sym == YY__COLON) return NULL;
				yy_warning("comparison between pointer and integer");
				return op1_type;
			} else if (t2 == C_TYPE_FUNC
			 && (op1_type->pointer.type->kind == C_TYPE_VOID
			  || c_compatible_types(op1_type->pointer.type, op2_type, 1, 0))) {
				return op1_type;
			}
		}
		return NULL;
	} else if (t2 == C_TYPE_POINTER || t2 == C_TYPE_ARRAY) {
		if (sym == YY__PLUS) {
			if (C_IS_TYPE_KIND_INT(t1) || t1 == C_TYPE_ENUM) {
				if (op1_type->size != op2_type->size) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op1);
				}
				if (op2_type->kind == C_TYPE_ARRAY) return c_create_pointer_type(rcc, op2_type->array.type);
				return op2_type;
			}
		} else if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
			|| sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__COLON) {
			if (C_IS_TYPE_INT(op1_type)) {
				if (c_value_is_const(op1) && op1->u.val.u64 == 0) {
					op1->type = &c_type_ptr;
					op1->u.type = IR_ADDR;
					return op2_type;
				}
				if (sym == YY__COLON) return NULL;
				yy_warning("comparison between pointer and integer");
				return op2_type;
			} else if (t1 == C_TYPE_FUNC
			 && (op2_type->pointer.type->kind == C_TYPE_VOID
			  || c_compatible_types(op2_type->pointer.type, op1_type, 1, 0))) {
				return op1_type;
			}
		}
		return NULL;
	} else if (t1 == C_TYPE_FUNC) {
		if (t2 == C_TYPE_FUNC && c_compatible_types(op1_type, op2_type, 1, 0)) {
			if (sym == YY__COLON) return c_create_pointer_type(rcc, op1_type);
			return op1_type;
		} else if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
			|| sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__COLON) {
			if (C_IS_TYPE_INT(op2_type)) {
				if (c_value_is_const(op2) && op2->u.val.u64 == 0) {
					op2->type = &c_type_ptr;
					op2->u.type = IR_ADDR;
					return op1_type;
				}
				if (sym == YY__COLON) return NULL;
				yy_warning("comparison between pointer and integer");
				return op1_type;
			}
		}
		return NULL;
	} else if (t1 == C_TYPE_VECTOR) {
		if (C_IS_TYPE_FP(op1->type->vec.type)) {
			if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR
			 || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
				return NULL;
			}
		}
		if (t2 == C_TYPE_VECTOR) {
			if (op1->type->size == op2->type->size) {
				if (op1->type->vec.type != op2->type->vec.type) {
					if (op2->type->flags & C_TYPE_OPAQUE) {
						c_opaque_vector_cast(rcc, op1->type, op2);
					} else if (op1->type->flags & C_TYPE_OPAQUE) {
						c_opaque_vector_cast(rcc, op2->type, op1);
					}
				}
				if (op1->type->vec.type == op2->type->vec.type) {
					if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
					  || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL) {
						return c_opaque_vector_type(rcc, op1->type);
					}
					return op1->type;
				}
			}
		} else if (C_IS_TYPE_KIND_SCALAR(t2)) {
			if (sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
				if (!C_IS_TYPE_KIND_INT(t2)) return NULL;
				return op1->type;
			} else if (c_do_splat(rcc, op1->type, op2)) {
				if ((sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
				  || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL)
				 && !C_IS_TYPE_SIGNED(op1->type->vec.type)) {
					return c_opaque_vector_type(rcc, op1->type);
				}
				return op1->type;
			}
		}
	} else if (t2 == C_TYPE_VECTOR && C_IS_TYPE_KIND_SCALAR(t1)) {
		if (C_IS_TYPE_FP(op2->type->vec.type)) {
			if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR
			 || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
				return NULL;
			}
		}
		if (c_do_splat(rcc, op2->type, op1)) {
				if ((sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
				  || sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL)
				 && !C_IS_TYPE_SIGNED(op2->type->vec.type)) {
				return c_opaque_vector_type(rcc, op2->type);
			}
			return op2->type;
		}
	} else if (t2 == C_TYPE_FUNC) {
		if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
			|| sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || sym == YY__COLON) {
			if (C_IS_TYPE_INT(op1_type)) {
				if (c_value_is_const(op1) && op1->u.val.u64 == 0) {
					op1->type = &c_type_ptr;
					op1->u.type = IR_ADDR;
					return op2_type;
				}
				if (sym == YY__COLON) return NULL;
				yy_warning("comparison between pointer and integer");
				return op2_type;
			}
		}
		return NULL;
	} else if (C_IS_TYPE_KIND_FP(t1)) {
		if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR
		 || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
			return NULL;
		} else if (t1 == t2) {
			return op1_type;
		} else if (C_IS_TYPE_KIND_FP(t2)) {
			if (op1_type->size >= op2_type->size) {
				c_do_fp2fp(rcc, op1_type, op1->u.type, op2);
				return op1_type;
			} else {
				c_do_fp2fp(rcc, op2_type, op2->u.type, op1);
				return op2_type;
			}
		} else if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
			c_do_int2fp(rcc, op1_type, op1->u.type, op2);
			return op1_type;
		}
	} else if (C_IS_TYPE_KIND_FP(t2)) {
		if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR
		 || sym == YY__LESS_LESS || sym == YY__GREATER_GREATER) {
			return NULL;
		} else if (C_IS_TYPE_KIND_INT(t1) || t1 == C_TYPE_ENUM) {
			c_do_int2fp(rcc, op2_type, op2->u.type, op1);
			return op2_type;
		}
	} else if (t1 == C_TYPE_ENUM) {
		if (t2 == C_TYPE_ENUM) {
			if (op1_type == op2_type || c_compatible_types(op1_type, op2_type, 1, 0)) {
				// TODO: select best type ???
				return op1_type;
			}
			t1 = op1_type->enumeration.kind;
			op1_type = c_type_by_kind(t1);
			IR_ASSERT(C_IS_TYPE_KIND_INT(t1));
			t2 = op2_type->enumeration.kind;
			op2_type = c_type_by_kind(t2);
			IR_ASSERT(C_IS_TYPE_KIND_INT(t2));
			if (t1 == t2) return op1_type;
			goto common_int_type;
		} else if (C_IS_TYPE_KIND_INT(t2)) {
			t1 = op1_type->enumeration.kind;
			op1_type = c_type_by_kind(t1);
			IR_ASSERT(C_IS_TYPE_KIND_INT(t1));
			if (t1 == t2) return op1_type;
			goto common_int_type;
		}
	} else if (t2 == C_TYPE_ENUM && C_IS_TYPE_KIND_INT(t1)) {
		t2 = op2_type->enumeration.kind;
		op2_type = c_type_by_kind(t2);
		IR_ASSERT(C_IS_TYPE_KIND_INT(t2));
		if (t1 == t2) return op1_type;
		goto common_int_type;
	} else if (C_IS_TYPE_KIND_INT(t1) && C_IS_TYPE_KIND_INT(t2)) {
common_int_type:
		if (op1_type->size > 4
		 || (sym != YY__LESS_LESS && sym != YY__GREATER_GREATER && op2_type->size > 4)) {
			if ((op1_type->size > 4 && C_IS_TYPE_KIND_UNSIGNED(t1)
			  && (!C_IS_BIT_FIELD(op1->u.proto) || C_BIT_FIELD_SIZE(op1->u.proto) >= 32))
			 || (sym != YY__LESS_LESS && sym != YY__GREATER_GREATER
			  && op2_type->size > 4 && C_IS_TYPE_KIND_UNSIGNED(t2)
			  && (!C_IS_BIT_FIELD(op2->u.proto) || C_BIT_FIELD_SIZE(op2->u.proto) >= 32))) {
				if (op1_type->size != 8 || C_IS_TYPE_KIND_SIGNED(t1)) c_do_cvt(rcc, &c_type_u64, IR_U64, op1);
				if (op2_type->size != 8 || C_IS_TYPE_KIND_SIGNED(t2)) c_do_cvt(rcc, &c_type_u64, IR_U64, op2);
				if (sizeof(long long) == sizeof(uint64_t)
				 && (op1_type->kind == C_TYPE_ILL
				  || op1_type->kind == C_TYPE_ULL
				  || op1_type->kind == C_TYPE_ILL
				  || op1_type->kind == C_TYPE_ULL)) {
					return &c_type_ull;
				} else {
					return &c_type_u64;
				}
			} else {
				if (op1_type->size != 8 || C_IS_TYPE_KIND_UNSIGNED(t1)) c_do_cvt(rcc, &c_type_i64, IR_I64, op1);
				if (op2_type->size != 8 || C_IS_TYPE_KIND_UNSIGNED(t2)) c_do_cvt(rcc, &c_type_i64, IR_I64, op2);
				if (sizeof(long long) == sizeof(int64_t)
				 && (op1_type->kind == C_TYPE_ILL
				  || op1_type->kind == C_TYPE_ULL
				  || op1_type->kind == C_TYPE_ILL
				  || op1_type->kind == C_TYPE_ULL)) {
					return &c_type_ill;
				} else {
					return &c_type_i64;
				}
			}
		} else {
			if ((op1_type->size == 4 && C_IS_TYPE_KIND_UNSIGNED(t1)
			  && (!C_IS_BIT_FIELD(op1->u.proto) || C_BIT_FIELD_SIZE(op1->u.proto) >= 32))
			 || (sym != YY__LESS_LESS && sym != YY__GREATER_GREATER
			  && op2_type->size == 4 && C_IS_TYPE_KIND_UNSIGNED(t2)
			  && (!C_IS_BIT_FIELD(op2->u.proto) || C_BIT_FIELD_SIZE(op2->u.proto) >= 32))) {
				if (op1_type->size != 4 || C_IS_TYPE_KIND_SIGNED(t1)) c_do_cvt(rcc, &c_type_u32, IR_U32, op1);
				if (op2_type->size != 4 || C_IS_TYPE_KIND_SIGNED(t2)) c_do_cvt(rcc, &c_type_u32, IR_U32, op2);
				if (sizeof(long) == sizeof(uint32_t)
				 && (op1_type->kind == C_TYPE_IL
				  || op1_type->kind == C_TYPE_UL
				  || op2_type->kind == C_TYPE_IL
				  || op2_type->kind == C_TYPE_UL)) {
					return &c_type_ul;
				} else {
					return &c_type_u32;
				}
			} else {
				if (op1_type->size != 4 || C_IS_TYPE_KIND_UNSIGNED(t1)) c_do_cvt(rcc, &c_type_i32, IR_I32, op1);
				if (op2_type->size != 4 || C_IS_TYPE_KIND_UNSIGNED(t2)) c_do_cvt(rcc, &c_type_i32, IR_I32, op2);
				if (sizeof(long) == sizeof(int32_t)
				 && (op1_type->kind == C_TYPE_IL
				  || op1_type->kind == C_TYPE_UL
				  || op2_type->kind == C_TYPE_IL
				  || op2_type->kind == C_TYPE_UL)) {
					return &c_type_il;
				} else {
					return &c_type_i32;
				}
			}
		}
	} else if (sym == YY__COLON) {
		if (t1 == C_TYPE_VOID || t2 == C_TYPE_VOID) {
			return &c_type_void;
		} else if (c_compatible_types(op1_type, op2_type, 1, 0)) {
			return op1_type;
		}
	}
	return NULL;
}

static void c_do_add(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	ir_val val;
	ir_ref ref;
	const c_type *element_type;
	size_t element_size;

	if (op1->type->kind == C_TYPE_POINTER || op1->type->kind == C_TYPE_ARRAY) {
		element_type = op1->type->pointer.type;
		if (element_type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op1->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(rcc, op1->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op1->type->pointer.type->kind), yy_sym2str(rcc, op1->type->pointer.type->tag));
		} else {
			element_type = op1->type->pointer.type;
			element_size = element_type->size;
		}
		IR_ASSERT(C_IS_TYPE_INT(op2->type) || op2->type->kind == C_TYPE_ENUM);
		if (c_value_is_const(op1) && !c_value_is_const_str(op1) && c_value_is_const(op2)
		 && !(element_type->attr & C_ATTR_VLA)) {
			val.addr = op1->u.val.addr + op2->u.val.u64 * element_size;
			c_value_set_const(op1, type, IR_ADDR, val);
		} else {
			if (C_IS_TYPE_SIGNED(op2->type)) {
				if (op2->type->kind != c_type_ssize_t.kind) {
					c_do_cvt(rcc, &c_type_ssize_t, IR_SSIZE_T, op2);
				}
				if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
					ref = c_value_ref(rcc, op2);
				} else {
					ref = ir_MUL(IR_SSIZE_T, c_value_ref(rcc, op2), c_type_ssize(rcc, element_type));
				}
			} else {
				if (op2->type->kind != c_type_size_t.kind) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op2);
				}
				if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
					ref = c_value_ref(rcc, op2);
				} else {
					ref = ir_MUL(IR_SIZE_T, c_value_ref(rcc, op2), c_type_size(rcc, element_type));
				}
			}
			ref = ir_ADD_A(c_value_ref(rcc, op1), ref);
			c_value_set_rval(op1, type, IR_ADDR, ref);
		}
	} else if (op2->type->kind == C_TYPE_POINTER || op2->type->kind == C_TYPE_ARRAY) {
		element_type = op2->type->pointer.type;
		if (element_type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op2->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(rcc, op2->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op2->type->pointer.type->kind), yy_sym2str(rcc, op2->type->pointer.type->tag));
		} else {
			element_type = op2->type->pointer.type;
			element_size = element_type->size;
		}
		IR_ASSERT(C_IS_TYPE_INT(op1->type) || op1->type->kind == C_TYPE_ENUM);
		if (c_value_is_const(op1) && c_value_is_const(op2) && !c_value_is_const_str(op2)
		 && !(element_type->attr & C_ATTR_VLA)) {
			val.addr = op2->u.val.addr + op1->u.val.u64 * element_size;
			c_value_set_const(op1, type, IR_ADDR, val);
		} else {
			if (C_IS_TYPE_SIGNED(op1->type)) {
				if (op1->type->kind != c_type_ssize_t.kind) {
					c_do_cvt(rcc, &c_type_ssize_t, IR_SSIZE_T, op1);
				}
				if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
					ref = c_value_ref(rcc, op1);
				} else {
					ref = ir_MUL(IR_SSIZE_T, c_value_ref(rcc, op1), c_type_ssize(rcc, element_type));
				}
			} else {
				if (op1->type->kind != c_type_size_t.kind) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op1);
				}
				if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
					ref = c_value_ref(rcc, op1);
				} else {
					ref = ir_MUL(IR_SIZE_T, c_value_ref(rcc, op1), c_type_size(rcc, element_type));
				}
			}
			ref = ir_ADD_A(c_value_ref(rcc, op2), ref);
			c_value_set_rval(op1, type, IR_ADDR, ref);
		}
	} else if (c_value_is_const(op1) && c_value_is_const(op2)) {
		switch (op1->u.type) {
			case IR_I32:    val.i64 = (int32_t)(op1->u.val.u32 + op2->u.val.u32); break;
			case IR_U32:    val.u64 = op1->u.val.u32 + op2->u.val.u32; break;
			case IR_I64:
			case IR_U64:    val.u64 = op1->u.val.u64 + op2->u.val.u64; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr + op2->u.val.addr; break;
			case IR_FLOAT:  val.f = op1->u.val.f + op2->u.val.f; val.u32_hi = 0; break;
			case IR_DOUBLE: val.d = op1->u.val.d + op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);
		ref = ir_ADD(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2));
		c_value_set_rval(op1, type, t, ref);
	}
}

static void c_do_sub(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	ir_val val;
	ir_ref ref;
	const c_type *element_type;
	size_t element_size;

	if (op1->type->kind == C_TYPE_POINTER || op1->type->kind == C_TYPE_ARRAY) {
		element_type = op1->type->pointer.type;
		if (element_type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op1->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(rcc, op1->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op1->type->pointer.type->kind), yy_sym2str(rcc, op1->type->pointer.type->tag));
		} else {
			element_type = op1->type->pointer.type;
			element_size = element_type->size;
		}
		if (op2->type->kind == C_TYPE_POINTER || op2->type->kind == C_TYPE_ARRAY) {
			IR_ASSERT(op1->type->pointer.type->size == op2->type->pointer.type->size);
			if (c_value_is_const(op1) && !c_value_is_const_str(op1)
			 && c_value_is_const(op2) && !c_value_is_const_str(op2)
			 && !(element_type->attr & C_ATTR_VLA)) {
				val.i64 = (op1->u.val.addr - op2->u.val.addr) / element_size;
				c_value_set_const(op1, &c_type_ssize_t, IR_SSIZE_T, val);
			 } else {
				ref = ir_SUB(IR_SSIZE_T, c_value_ref(rcc, op1), c_value_ref(rcc, op2));
				if (element_size != 1 || (element_type->attr & C_ATTR_VLA)) {
					ref = ir_DIV(IR_SSIZE_T, ref, c_type_ssize(rcc, element_type));
				}
				type = &c_type_ssize_t;
				c_value_set_rval(op1, type, IR_SSIZE_T, ref);
			 }
		} else {
			IR_ASSERT(C_IS_TYPE_INT(op2->type) || op2->type->kind == C_TYPE_ENUM);
			if (c_value_is_const(op1) && !c_value_is_const_str(op1) && c_value_is_const(op2)
			 && !(element_type->attr & C_ATTR_VLA)) {
				val.addr = op1->u.val.addr - op2->u.val.u64 * element_size;
				c_value_set_const(op1, type, IR_ADDR, val);
			} else {
				if (C_IS_TYPE_SIGNED(op2->type)) {
					if (op2->type->kind != c_type_ssize_t.kind) {
						c_do_cvt(rcc, &c_type_ssize_t, IR_SSIZE_T, op2);
					}
					if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
						ref = c_value_ref(rcc, op2);
					} else {
						ref = ir_MUL(IR_SSIZE_T, c_value_ref(rcc, op2), c_type_ssize(rcc, element_type));
					}
				} else {
					if (op2->type->kind != c_type_size_t.kind) {
						c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, op2);
					}
					if (element_size == 1 && !(element_type->attr & C_ATTR_VLA)) {
						ref = c_value_ref(rcc, op2);
					} else {
						ref = ir_MUL(IR_SIZE_T, c_value_ref(rcc, op2), c_type_size(rcc, element_type));
					}
				}
				ref = ir_SUB_A(c_value_ref(rcc, op1), ref);
				c_value_set_rval(op1, type, IR_ADDR, ref);
			}
		}
		return;
	} else if (c_value_is_const(op1) && c_value_is_const(op2)) {
		switch (op1->u.type) {
			case IR_I32:    val.i64 = (int32_t)(op1->u.val.u32 - op2->u.val.u32); break;
			case IR_U32:    val.u64 = op1->u.val.u32 - op2->u.val.u32; break;
			case IR_I64:    val.i64 = op1->u.val.u64 - op2->u.val.u64; break;
			case IR_U64:    val.u64 = op1->u.val.u64 - op2->u.val.u64; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr - op2->u.val.addr; break;
			case IR_FLOAT:  val.f = op1->u.val.f - op2->u.val.f; val.u32_hi = 0; break;
			case IR_DOUBLE: val.d = op1->u.val.d - op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);
		ref = ir_SUB(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2));
		c_value_set_rval(op1, type, t, ref);
	}
}

static void c_do_mul(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32:    val.i64 = (int32_t)(op1->u.val.u32 * op2->u.val.u32); break;
			case IR_U32:    val.u64 = op1->u.val.u32 * op2->u.val.u32; break;
			case IR_I64:
			case IR_U64:    val.u64 = op1->u.val.u64 * op2->u.val.u64; break;
			case IR_FLOAT:  val.f = op1->u.val.f * op2->u.val.f; val.u32_hi = 0; break;
			case IR_DOUBLE: val.d = op1->u.val.d * op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);

		c_value_set_rval(op1, type, t, ir_MUL(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_div(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		if (IR_IS_TYPE_INT(op2->u.type) && op2->u.val.u64 == 0) {
			if (rcc->active_scope) {
				goto emit_code;
			} else if (rcc->c_dead_code) {
				val.u64 = 0;
				c_value_set_const(op1, type, c_type2ir(rcc, type), val);
				return;
			}
			yy_error("division by zero");
		}
		switch (op1->u.type) {
			case IR_I32:
				if (UNEXPECTED(op2->u.val.i32 == -1 && op1->u.val.u32 == 0x80000000)) {
					if (rcc->active_scope) {
						goto emit_code;
					} else if (rcc->c_dead_code) {
						val.u64 = 0;
						c_value_set_const(op1, type, c_type2ir(rcc, type), val);
						return;
					}
					yy_warning("integer overflow in expression");
					val.i64 = op1->u.val.i32;
				} else {
				    val.i64 = op1->u.val.i32 / op2->u.val.i32;
				}
			    break;
			case IR_U32:
				val.u64 = op1->u.val.u32 / op2->u.val.u32;
				break;
			case IR_I64:
				if (UNEXPECTED(op2->u.val.i64 == -1 && op1->u.val.u64 == 0x8000000000000000ULL)) {
					if (rcc->active_scope) {
						goto emit_code;
					} else if (rcc->c_dead_code) {
						val.u64 = 0;
						c_value_set_const(op1, type, c_type2ir(rcc, type), val);
						return;
					}
					yy_warning("integer overflow in expression");
					val.i64 = op1->u.val.i64;
				} else {
				    val.i64 = op1->u.val.i64 / op2->u.val.i64;
				}
				break;
			case IR_U64:
				val.u64 = op1->u.val.u64 / op2->u.val.u64;
				break;
			case IR_FLOAT:
				val.f = op1->u.val.f / op2->u.val.f;
				val.u32_hi = 0;
				break;
			case IR_DOUBLE:
				val.d = op1->u.val.d / op2->u.val.d;
				break;
			default:
				IR_ASSERT(0);
				return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t;

emit_code:
		t = c_type2ir(rcc, type);
		c_value_set_rval(op1, type, t, ir_DIV(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_mod(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		if (op2->u.val.u64 == 0) {
			if (rcc->active_scope) {
				goto emit_code;
			} else if (rcc->c_dead_code) {
				val.u64 = 0;
				c_value_set_const(op1, type, c_type2ir(rcc, type), val);
				return;
			}
			yy_error("division by zero");
		}
		switch (op1->u.type) {
			case IR_I32:
				if (op2->u.val.i32 == -1 && op1->u.val.u32 == 0x80000000) {
					if (rcc->active_scope) {
						goto emit_code;
					} else if (rcc->c_dead_code) {
						val.u64 = 0;
						c_value_set_const(op1, type, c_type2ir(rcc, type), val);
						return;
					}
					yy_warning("integer overflow in expression");
					val.i64 = op1->u.val.i32;
				} else {
					val.i64 = op1->u.val.i32 % op2->u.val.i32;
				}
				break;
			case IR_U32:
				val.u64 = op1->u.val.u32 % op2->u.val.u32;
				break;
			case IR_I64:
				val.i64 = op1->u.val.i64 % op2->u.val.i64;
				break;
			case IR_U64:
				if (op2->u.val.i64 == -1 && op1->u.val.u64 == 0x8000000000000000) {
					if (rcc->active_scope) {
						goto emit_code;
					} else if (rcc->c_dead_code) {
						val.u64 = 0;
						c_value_set_const(op1, type, c_type2ir(rcc, type), val);
						return;
					}
					yy_warning("integer overflow in expression");
					val.i64 = op1->u.val.i64;
				} else {
					val.u64 = op1->u.val.u64 % op2->u.val.u64;
				}
				break;
			default:
				IR_ASSERT(0);
				return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t;

emit_code:
		t = c_type2ir(rcc, type);
		c_value_set_rval(op1, type, t, ir_MOD(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_shl(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2) && op1->type->kind != C_TYPE_VECTOR) {
		ir_val val;
		uint32_t mask = (op2->type->size == 8) ? 63 : 31;

		if (IR_IS_TYPE_SIGNED(op2->u.type) && op2->u.val.i64 < 0) {
			yy_warning("shift count is negative");
		} else if (op2->u.val.u64 > mask) {
			yy_warning("shift count >= width of type");
		}
		switch (op1->u.type) {
			case IR_I32: val.i64 = (int32_t)(op1->u.val.u32 << (op2->u.val.u32 & mask)); break;
			case IR_U32: val.u64 = op1->u.val.u32 << (op2->u.val.u32 & mask); break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 << (op2->u.val.u64 & mask); break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);
		c_value_set_rval(op1, type, t, ir_SHL(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_shr(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2) && op1->type->kind != C_TYPE_VECTOR) {
		ir_val val;
		uint32_t mask = (op2->type->size == 8) ? 63 : 31;

		if (IR_IS_TYPE_SIGNED(op2->u.type) && op2->u.val.i64 < 0) {
			yy_warning("shift count is negative");
		} else if (op2->u.val.u64 > mask) {
			yy_warning("shift count >= width of type");
		}
		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 >> (op2->u.val.i32 & mask); break;
			case IR_U32: val.u64 = op1->u.val.u32 >> (op2->u.val.u32 & mask); break;
			case IR_I64: val.i64 = op1->u.val.i64 >> (op2->u.val.i64 & mask); break;
			case IR_U64: val.u64 = op1->u.val.u64 >> (op2->u.val.u64 & mask); break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);

		if (type->kind == C_TYPE_VECTOR) {
			if (C_IS_TYPE_SIGNED(type->vec.type)) {
				c_value_set_rval(op1, type, t, ir_SAR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
			} else {
				c_value_set_rval(op1, type, t, ir_SHR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
			}
		} else if (C_IS_TYPE_SIGNED(type)) {
			c_value_set_rval(op1, type, t, ir_SAR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_SHR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	}
}

static void c_do_and(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 & op2->u.val.i32; break;
			case IR_U32: val.u64 = op1->u.val.u32 & op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 & op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);

		c_value_set_rval(op1, type, t, ir_AND(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_xor(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 ^ op2->u.val.i32; break;
			case IR_U32: val.u64 = op1->u.val.u32 ^ op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 ^ op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);

		c_value_set_rval(op1, type, t, ir_XOR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_or(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 | op2->u.val.i32; break;
			case IR_U32: val.u64 = op1->u.val.u32 | op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 | op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(rcc, type), val);
	} else {
		ir_type t = c_type2ir(rcc, type);

		c_value_set_rval(op1, type, t, ir_OR(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_lt(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:    val.u64 = op1->u.val.i64 < op2->u.val.i64; break;
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:    val.u64 = op1->u.val.u64 < op2->u.val.u64; break;
			case IR_CHAR:   val.u64 = op1->u.val.c < op2->u.val.c; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr < op2->u.val.addr; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f < op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d < op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		if (C_IS_TYPE_SIGNED(op1->type->vec.type) || C_IS_TYPE_FP(op1->type->vec.type)) {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_LT, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_ULT, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_LT(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_ULT(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	}
}

static void c_do_gt(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:    val.u64 = op1->u.val.i64 > op2->u.val.i64; break;
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:    val.u64 = op1->u.val.u64 > op2->u.val.u64; break;
			case IR_CHAR:   val.u64 = op1->u.val.c > op2->u.val.c; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr > op2->u.val.addr; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f > op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d > op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		if (C_IS_TYPE_SIGNED(op1->type->vec.type) || C_IS_TYPE_FP(op1->type->vec.type)) {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_GT, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_UGT, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_GT(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_UGT(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	}
}

static void c_do_le(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:    val.u64 = op1->u.val.i64 <= op2->u.val.i64; break;
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:    val.u64 = op1->u.val.u64 <= op2->u.val.u64; break;
			case IR_CHAR:   val.u64 = op1->u.val.c <= op2->u.val.c; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr <= op2->u.val.addr; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f <= op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d <= op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		if (C_IS_TYPE_SIGNED(op1->type->vec.type) || C_IS_TYPE_FP(op1->type->vec.type)) {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_LE, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_ULE, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_LE(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_ULE(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	}
}

static void c_do_ge(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:    val.u64 = op1->u.val.i64 >= op2->u.val.i64; break;
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:    val.u64 = op1->u.val.u64 >= op2->u.val.u64; break;
			case IR_CHAR:   val.u64 = op1->u.val.c >= op2->u.val.c; break;
			case IR_ADDR:   val.u64 = op1->u.val.addr >= op2->u.val.addr; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f >= op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d >= op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		if (C_IS_TYPE_SIGNED(op1->type->vec.type) || C_IS_TYPE_FP(op1->type->vec.type)) {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_GE, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_UGE, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_GE(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_UGE(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
		}
	}
}

static void c_do_eq(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:
			case IR_CHAR:
			case IR_ADDR:   val.u64 = op1->u.val.u64 == op2->u.val.u64; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f == op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d == op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_EQ, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	} else {
		c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_EQ(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

static void c_do_ne(rcc_ctx *rcc, const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I8:
			case IR_I16:
			case IR_I32:
			case IR_I64:
			case IR_U8:
			case IR_U16:
			case IR_U32:
			case IR_U64:
			case IR_CHAR:
			case IR_ADDR:   val.u64 = op1->u.val.u64 != op2->u.val.u64; break;
			case IR_FLOAT:  val.u64 = op1->u.val.f != op2->u.val.f; break;
			case IR_DOUBLE: val.u64 = op1->u.val.d != op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
	} else if (type->kind == C_TYPE_VECTOR) {
		ir_type t = c_type2ir(rcc, type);
		c_value_set_rval(op1, type, t, ir_BINARY_OP(IR_NE, t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	} else {
		c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_NE(c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
	}
}

void c_do_binary_op(rcc_ctx *rcc, yy_sym sym, c_value *op1, c_value *op2)
{
	const c_type *type;

	c_value_rval(rcc, op1);
	c_value_rval(rcc, op2);
	type = c_common_type(rcc, sym, op1, op2);
	if (!type) yy_error_fmt("invalid operands to binary \"%s\"", yy_sym2str(rcc, sym));
	switch (sym) {
		case YY__PLUS:            c_do_add(rcc, type, op1, op2); break;
		case YY__MINUS:           c_do_sub(rcc, type, op1, op2); break;
		case YY__STAR:            c_do_mul(rcc, type, op1, op2); break;
		case YY__SLASH:           c_do_div(rcc, type, op1, op2); break;
		case YY__PERCENT:         c_do_mod(rcc, type, op1, op2); break;
		case YY__LESS_LESS:       c_do_shl(rcc, type, op1, op2); break;
		case YY__GREATER_GREATER: c_do_shr(rcc, type, op1, op2); break;
		case YY__AND:             c_do_and(rcc, type, op1, op2); break;
		case YY__UPARROW:         c_do_xor(rcc, type, op1, op2); break;
		case YY__BAR:             c_do_or(rcc, type, op1, op2); break;
		case YY__LESS:            c_do_lt(rcc, type, op1, op2); break;
		case YY__GREATER:         c_do_gt(rcc, type, op1, op2); break;
		case YY__LESS_EQUAL:      c_do_le(rcc, type, op1, op2); break;
		case YY__GREATER_EQUAL:   c_do_ge(rcc, type, op1, op2); break;
		case YY__EQUAL_EQUAL:     c_do_eq(rcc, type, op1, op2); break;
		case YY__BANG_EQUAL:      c_do_ne(rcc, type, op1, op2); break;
		default: IR_ASSERT(0);
	}
}

void c_do_assign_op(rcc_ctx *rcc, yy_sym sym, c_value *op1, c_value *op2)
{
	if (!c_value_is_lval(op1) || op1->type->kind == C_TYPE_FUNC) {
		yy_error("lvalue required as left operand of assignment");
	}
	if (sym != YY__EQUAL) {
		c_value tmp = *op1;

		c_value_rval(rcc, &tmp);
		switch (sym) {
			case YY__STAR_EQUAL:            sym = YY__STAR; break;
			case YY__SLASH_EQUAL:           sym = YY__SLASH; break;
			case YY__PERCENT_EQUAL:         sym = YY__PERCENT; break;
			case YY__PLUS_EQUAL:            sym = YY__PLUS; break;
			case YY__MINUS_EQUAL:           sym = YY__MINUS; break;
			case YY__LESS_LESS_EQUAL:       sym = YY__LESS_LESS; break;
			case YY__GREATER_GREATER_EQUAL: sym = YY__GREATER_GREATER; break;
			case YY__AND_EQUAL:             sym = YY__AND; break;
			case YY__UPARROW_EQUAL:         sym = YY__UPARROW; break;
			case YY__BAR_EQUAL:             sym = YY__BAR; break;
			default: IR_ASSERT(0);
		}
		c_do_binary_op(rcc, sym, &tmp, op2);
		*op2 = tmp;
	} else {
		c_value_rval(rcc, op2);
	}
	if (op1->type != op2->type) c_do_check_cvt(rcc, op1->type, op2, -1);
	if (op1->type->attr & C_ATTR_CONST) yy_error_fmt("%s of read-only location", "assignment");
	if (C_IS_TYPE_SCALAR_OR_PTR(op1->type) || op1->type->kind == C_TYPE_VECTOR) {
		ir_ref ref = c_do_store(rcc, op1, op2);

		if (!IR_IS_CONST_REF(ref) || IR_IS_SYM_CONST(rcc->active_ctx->ir_base[ref].op)) {
			c_value_set_rval(op1, op1->type, op1->u.type, ref);
		} else {
			c_value_set_const(op1, op1->type, op1->u.type, rcc->active_ctx->ir_base[ref].val);
		}
	} else {
		IR_ASSERT(op1->type->size == op2->type->size);
		if (op1->type->size) {
			ir_memcpy(rcc, op1->u.ref, c_value_ref(rcc, op2),
				ir_const_size_t(rcc->active_ctx, op2->type->size), c_attr2align(op2->type->attr));
		}
	}
}


static void c_do_bool(rcc_ctx *rcc, c_value *res, c_value *op1)
{
	ir_val val;

	if (op1->type->kind == C_TYPE_BOOL) {
		*res = *op1;
	} else if (c_value_is_const(op1)) {
		val.u64 = op1->u.val.u64 != 0;
		c_value_set_const(res, &c_type_bool, IR_BOOL, val);
	} else {
		val.u64 = 0;
		c_value_set_rval(res, &c_type_bool, IR_BOOL,
			ir_NE(c_value_ref(rcc, op1), ir_const(rcc->active_ctx, val, op1->u.type)));
	}
}

/* wrappers to support __builtin_expect() */
static void c_ir_IF_TRUE(rcc_ctx *rcc, ir_ref ref)
{
	if (!rcc->c_last_expect_ref
	 || rcc->c_last_expect_ref != rcc->active_ctx->ir_base[ref].op2
	 || rcc->c_last_expect_val) {
		ir_IF_TRUE(ref);
	} else {
		ir_IF_TRUE_cold(ref);
	}
}

static void c_ir_IF_FALSE(rcc_ctx *rcc, ir_ref ref)
{
	if (!rcc->c_last_expect_ref
	 || rcc->c_last_expect_ref != rcc->active_ctx->ir_base[ref].op2
	 || !rcc->c_last_expect_val) {
		ir_IF_FALSE(ref);
	} else {
		ir_IF_FALSE_cold(ref);
	}
}

#define c_ir_MERGE_WITH_EMPTY_TRUE(_if)     do {ir_ref end = ir_END(); c_ir_IF_TRUE(rcc, _if); ir_MERGE_2(end, ir_END());} while (0)
#define c_ir_MERGE_WITH_EMPTY_FALSE(_if)    do {ir_ref end = ir_END(); c_ir_IF_FALSE(rcc, _if); ir_MERGE_2(end, ir_END());} while (0)

ir_ref c_do_bool_and_start(rcc_ctx *rcc, c_value *op1)
{
	if (op1->type->kind == C_TYPE_VOID
	 || op1->type->kind == C_TYPE_STRUCT
	 || op1->type->kind == C_TYPE_UNION
	 || op1->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	}
	if (c_value_is_const(op1)) {
		if (c_value_is_true(op1)) return IR_UNUSED;
		rcc->c_dead_code = 1;
	}
	c_do_bool(rcc, op1, op1);
	ir_ref ref = ir_IF(c_value_ref(rcc, op1));
	c_ir_IF_TRUE(rcc, ref);
	return ref;
}

void c_do_bool_and_end(rcc_ctx *rcc, c_value *op1, c_value *op2, ir_ref if_ref)
{
	ir_val val;

	if (op2->type->kind == C_TYPE_VOID
	 || op2->type->kind == C_TYPE_STRUCT
	 || op2->type->kind == C_TYPE_UNION
	 || op2->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	}
	if (if_ref) {
		ir_ref ref;

		c_do_bool(rcc, op2, op2);
		ref = c_value_ref(rcc, op2);
		c_ir_MERGE_WITH_EMPTY_FALSE(if_ref);
		if (c_value_is_const(op1) && c_value_is_const(op2)) {
			if (c_value_is_true(op1) && c_value_is_true(op2)) {
				val.u64 = 1;
				c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
			} else {
				val.u64 = 0;
				c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
			}
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_PHI_2(IR_BOOL, ref, IR_FALSE));
		}
	} else {
		c_do_bool(rcc, op1, op2);
	}
}

ir_ref c_do_bool_or_start(rcc_ctx *rcc, c_value *op1)
{
	if (op1->type->kind == C_TYPE_VOID
	 || op1->type->kind == C_TYPE_STRUCT
	 || op1->type->kind == C_TYPE_UNION
	 || op1->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	}
	if (c_value_is_const(op1)) {
		if(!c_value_is_true(op1)) return IR_UNUSED;
		rcc->c_dead_code = 1;
	}
	c_do_bool(rcc, op1, op1);
	ir_ref ref = ir_IF(c_value_ref(rcc, op1));
	c_ir_IF_FALSE(rcc, ref);
	return ref;
}

void c_do_bool_or_end(rcc_ctx *rcc, c_value *op1, c_value *op2, ir_ref if_ref)
{
	ir_val val;

	if (op2->type->kind == C_TYPE_VOID
	 || op2->type->kind == C_TYPE_STRUCT
	 || op2->type->kind == C_TYPE_UNION
	 || op2->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	}
	if (if_ref) {
		ir_ref ref;

		c_do_bool(rcc, op2, op2);
		ref = c_value_ref(rcc, op2);
		c_ir_MERGE_WITH_EMPTY_TRUE(if_ref);
		if ((c_value_is_const(op1) && c_value_is_true(op1))
		 || (c_value_is_const(op2) && c_value_is_true(op2))) {
			val.u64 = 1;
			c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
		} else if (c_value_is_const(op1) && !c_value_is_true(op1)
		 && c_value_is_const(op2) && !c_value_is_true(op2)) {
			val.u64 = 0;
			c_value_set_const(op1, &c_type_bool, IR_BOOL, val);
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_PHI_2(IR_BOOL, ref, IR_TRUE));
		}
	} else {
		c_do_bool(rcc, op1, op2);
	}
}

void c_do_cond_op(rcc_ctx *rcc, c_value *cond, c_value *op1, c_value *op2)
{
	const c_type *type;

	if (!c_value_is_set(op1)) {
		*op1 = *cond;
	}
	if (op1->type->kind == op2->type->kind && op1->type->kind == C_TYPE_VOID) return;
	type = c_common_type(rcc, YY__COLON, op1, op2);
	if (!type) yy_error("type mismatch in conditional expression");

	if (type != &c_type_void && (op1->type != type || op2->type != type)) {
		if (op1->type->kind == C_TYPE_POINTER && op2->type->kind == C_TYPE_POINTER) {
			const c_type *t1 = op1->type->pointer.type;
			const c_type *t2 = op2->type->pointer.type;

			/* Prefer pointer non-NULL */
			if (c_value_is_const(op1)
			 && op1->u.val.u64 == 0
			 && t1->kind == C_TYPE_VOID
			 && (t1->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) == 0) {
				type = op2->type;
			} else if (c_value_is_const(op2)
			 && op2->u.val.u64 == 0
			 && t2->kind == C_TYPE_VOID
			 && (t2->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) == 0) {
				type = op1->type;
			/* Prefer pointer to void */
			} else if (t1->kind == C_TYPE_VOID) {
				type = op1->type;
			} else if (t2->kind == C_TYPE_VOID) {
				type = op2->type;
			/* Prefer pointer to a non-flexible array */
			} else if (t1->kind == C_TYPE_ARRAY && t2->kind == C_TYPE_ARRAY) {
				if ((t1->attr & C_ATTR_FLEXIBLE) && !(t2->attr & C_ATTR_FLEXIBLE)) {
					type = op2->type;
				} else if (!(t1->attr & C_ATTR_FLEXIBLE) && (t2->attr & C_ATTR_FLEXIBLE)) {
					type = op1->type;
				}
			}
			/* Merge qualifiers */
			if ((t1->attr & (C_ATTR_CONST|C_ATTR_VOLATILE))
			 != (t2->attr & (C_ATTR_CONST|C_ATTR_VOLATILE))) {
				c_type *t = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
				*t = *type;
				t->pointer.type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
				*((c_type*)(t->pointer.type)) = *type->pointer.type;
				((c_type*)(t->pointer.type))->attr |= (t1->attr | t2->attr) & (C_ATTR_CONST|C_ATTR_VOLATILE);
				if (rcc->active_scope) {
					t->flags &= ~C_TYPE_GLOBAL;
					((c_type*)(t->pointer.type))->flags &= ~C_TYPE_GLOBAL;
				}
				type = t;
		    }
		}
		if (op1->type != type) c_do_cvt(rcc, type, c_type2ir(rcc, type), op1);
		if (op2->type != type) c_do_cvt(rcc, type, c_type2ir(rcc, type), op2);
	}
	// TODO: We might need PHI decause of dominance ???
	if (type == &c_type_void) {
		c_value_set_rval(cond, type, IR_VOID, IR_UNUSED);
	} else if (c_value_is_const(cond)) {
		if (c_value_is_true(cond)) {
			if (c_value_is_const(op1)) {
				*cond = *op1;
				cond->type = type;
			} else {
				c_value_set_rval(cond, type, c_type2ir(rcc, type), c_value_ref(rcc, op1));
			}
		} else {
			if (c_value_is_const(op2)) {
				*cond = *op2;
				cond->type = type;
			} else {
				c_value_set_rval(cond, type, c_type2ir(rcc, type), c_value_ref(rcc, op2));
			}
		}
	} else {
		ir_type t = c_type2ir(rcc, type);
#if 1
		c_value_set_rval(cond, type, t, ir_PHI_2(t, c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
#else
		c_value_set_rval(cond, type, t, ir_COND(t, c_value_ref(rcc, cond), c_value_ref(rcc, op1), c_value_ref(rcc, op2)));
#endif
	}
}

void c_do_statement_expression(rcc_ctx *rcc, c_scope *scope, c_value *val)
{
	if (!rcc->active_func_scope) yy_error("statement expression allowed only inside a function");
	c_push_scope(rcc, scope);
	scope->checkpoint = NULL; /* we must not free type of result value */
	c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
}

ir_ref c_do_if(rcc_ctx *rcc, c_value *cond)
{
	ir_ref ref;

	if (cond->type->kind == C_TYPE_VOID
	 || cond->type->kind == C_TYPE_STRUCT
	 || cond->type->kind == C_TYPE_UNION
	 || cond->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	} else if (C_IS_TYPE_FP(cond->type)) {
		ir_val val;
		val.u64 = 0;
		ref = ir_NE(c_value_ref(rcc, cond), ir_const(rcc->active_ctx, val, cond->u.type));
	} else {
		ref = c_value_ref(rcc, cond);
	}
	if (IR_IS_CONST_REF(ref) && !ir_const_is_true(&rcc->active_ctx->ir_base[ref])) rcc->c_dead_code = 1;
	ref = ir_IF(ref);
	rcc->active_ctx->ir_base[ref].op3 = IR_UNUSED;
	c_ir_IF_TRUE(rcc, ref);
	return ref;
}

void c_do_if_else(rcc_ctx *rcc, ir_ref if_ref, bool orig_dead_code)
{
	ir_ref end_true_ref = ir_END();
	rcc->active_ctx->ir_base[if_ref].op3 = end_true_ref;
	c_ir_IF_FALSE(rcc, if_ref);
	if (!orig_dead_code) {
		ir_ref cond = rcc->active_ctx->ir_base[if_ref].op2;
		rcc->c_dead_code = (IR_IS_CONST_REF(cond) && ir_const_is_true(&rcc->active_ctx->ir_base[cond]));
	}
}

void c_do_if_end(rcc_ctx *rcc, ir_ref if_ref, bool orig_dead_code)
{
	ir_ref end_true_ref = rcc->active_ctx->ir_base[if_ref].op3;

	if (end_true_ref) {
		rcc->active_ctx->ir_base[if_ref].op3 = IR_UNUSED;
		ir_MERGE_2(end_true_ref, ir_END());
	} else {
		c_ir_MERGE_WITH_EMPTY_FALSE(if_ref);
	}
	rcc->c_dead_code = orig_dead_code;
}

void c_do_switch(rcc_ctx *rcc, c_loop *loop, c_value *cond)
{
	const c_type *t;

	c_value_rval(rcc, cond);
	t = cond->type;
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(rcc, &c_type_i32, IR_I32, cond);
		}
	} else {
		yy_error("switch quantity not an integer");
	}
	loop->is_switch = 1;
	loop->switch_type = cond->type;
	loop->start = IR_UNUSED;
	loop->check = ir_SWITCH(c_value_ref(rcc, cond));
	loop->next = IR_UNUSED;
	loop->break_list = IR_UNUSED;
	loop->continue_list = IR_UNUSED;
	loop->scope = rcc->active_scope;
	loop->prev = rcc->active_loop;
	loop->case_labels = NULL;
	rcc->active_loop = loop;
	ir_BEGIN(IR_UNUSED);
}

static c_loop *c_find_switch(rcc_ctx *rcc)
{
	c_loop *loop = rcc->active_loop;

	while (loop) {
		if (loop->is_switch) return loop;
		loop = loop->prev;
	}
	return NULL;
}

/* Detect duplicate case labels through Red-Black Tree */
struct _c_case_labels {
	ir_val         min;
	ir_val         max;
	c_case_labels *parent;
	c_case_labels *left;
	c_case_labels *right;
	uint8_t        color;
};

static void c_case_labels_rotateleft(c_loop *loop, c_case_labels *p)
{
	c_case_labels *r = p->right;

	p->right = r->left;
	if (r->left) {
		r->left->parent = p;
	}
	r->parent = p->parent;
	if (p->parent == NULL) {
		loop->case_labels = r;
	} else if (p->parent->left == p) {
		p->parent->left = r;
	} else {
		p->parent->right = r;
	}
	r->left = p;
	p->parent = r;
}

static void c_case_labels_rotateright(c_loop *loop, c_case_labels *p)
{
	c_case_labels *l = p->left;

	p->left = l->right;
	if (l->right) {
		l->right->parent = p;
	}
	l->parent = p->parent;
	if (p->parent == NULL) {
		loop->case_labels = l;
	} else if (p->parent->right == p) {
		p->parent->right = l;
	} else {
		p->parent->left = l;
	}
	l->right = p;
	p->parent = l;
}

static void c_case_labels_balance(c_loop *loop, c_case_labels *q)
{
	c_case_labels *p;

	while (q && q->parent && q->parent->color == 1) {
		if (q->parent == q->parent->parent->left) {
			p = q->parent->parent->right;
			if (p && p->color == 1) {
				q->parent->color = 0;
				p->color = 0;
				q->parent->parent->color = 1;
				q = q->parent->parent;
			} else {
				if (q == q->parent->right) {
					q = q->parent;
					c_case_labels_rotateleft(loop, q);
				}
				q->parent->color = 0;
				q->parent->parent->color = 1;
				c_case_labels_rotateright(loop, q->parent->parent);
			}
		} else {
			p = q->parent->parent->left;
			if (p && p->color == 1) {
				q->parent->color = 0;
				p->color = 0;
				q->parent->parent->color = 1;
				q = q->parent->parent;
			} else {
				if (q == q->parent->left) {
					q = q->parent;
					c_case_labels_rotateright(loop, q);
				}
				q->parent->color = 0;
				q->parent->parent->color = 1;
				c_case_labels_rotateleft(loop, q->parent->parent);
			}
		}
	}
	loop->case_labels->color = 0;
}

static void c_case_labels_add_i(rcc_ctx *rcc, c_loop *loop, ir_val min, ir_val max)
{
	c_case_labels *q, *p = loop->case_labels;

	IR_ASSERT(min.i64 <= max.i64);
	if (p) {
		while (1) {
			if (min.i64 > p->min.i64) {
				if (min.i64 <= p->max.i64) yy_error("duplicate case value");
				if (p->right) {
					p = p->right;
				} else if (min.i64 == p->max.i64 + 1) {
					p->max.i64 = max.i64;
					return;
				} else {
					q = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
					p->right = q;
					break;
				}
			} else if (min.i64 < p->min.i64) {
				if (max.i64 >= p->min.i64) yy_error("duplicate case value");
				if (p->left) {
					p = p->left;
				} else if (max.i64 + 1 == p->min.i64) {
					p->min.i64 = min.i64;
					return;
				} else {
					q = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
					p->left = q;
					break;
				}
			} else {
				yy_error("duplicate case value");
			}
		}
		q->min = min;
		q->max = max;
		q->parent = p;
		q->left = NULL;
		q->right = NULL;
		q->color = 1;
		c_case_labels_balance(loop, q);
	} else {
		loop->case_labels = p = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
		p->min = min;
		p->max = max;
		p->parent = NULL;
		p->left = NULL;
		p->right = NULL;
		p->color = 0;
	}
}

static void c_case_labels_add_u(rcc_ctx *rcc, c_loop *loop, ir_val min, ir_val max)
{
	c_case_labels *q, *p = loop->case_labels;

	IR_ASSERT(min.u64 <= max.u64);
	if (p) {
		while (1) {
			if (min.u64 > p->min.u64) {
				if (min.u64 <= p->max.u64) yy_error("duplicate case value");
				if (p->right) {
					p = p->right;
				} else if (min.u64 == p->max.u64 + 1) {
					p->max.u64 = max.u64;
					return;
				} else {
					q = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
					p->right = q;
					break;
				}
			} else if (min.u64 < p->min.u64) {
				if (max.u64 >= p->min.u64) yy_error("duplicate case value");
				if (p->left) {
					p = p->left;
				} else if (max.u64 + 1 == p->min.u64) {
					p->min.u64 = min.u64;
					return;
				} else {
					q = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
					p->left = q;
					break;
				}
			} else {
				yy_error("duplicate case value");
			}
		}
		q->min = min;
		q->max = max;
		q->parent = p;
		q->left = NULL;
		q->right = NULL;
		q->color = 1;
		c_case_labels_balance(loop, q);
	} else {
		loop->case_labels = p = ir_arena_alloc(&rcc->c_func_arena, sizeof(c_case_labels));
		p->min = min;
		p->max = max;
		p->parent = NULL;
		p->left = NULL;
		p->right = NULL;
		p->color = 0;
	}
}

void c_do_case(rcc_ctx *rcc, c_value *v)
{
	c_loop *loop = c_find_switch(rcc);
	ir_ref prev = IR_UNUSED;

	if (!loop) yy_error("case label not within a switch statement");
	if (!c_value_is_const(v) || (!C_IS_TYPE_INT(v->type) && v->type->kind != C_TYPE_ENUM)) {
		yy_error("case label does not reduce to an integer constant");
	}
	if (rcc->active_ctx->control) {
		prev = ir_END();
	}
	if (loop->switch_type != v->type) {
		c_do_cvt(rcc, loop->switch_type, c_type2ir(rcc, loop->switch_type), v);
	}
	if (C_IS_TYPE_SIGNED(loop->switch_type)) {
		c_case_labels_add_i(rcc, loop, v->u.val, v->u.val);
	} else {
		c_case_labels_add_u(rcc, loop, v->u.val, v->u.val);
	}
	ir_CASE_VAL(loop->check, c_value_ref(rcc, v));
	if (prev) {
		ir_MERGE_2(prev, ir_END());
	}
}

void c_do_case_range(rcc_ctx *rcc, c_value *v1, c_value *v2)
{
	c_loop *loop = c_find_switch(rcc);
	ir_ref list = IR_UNUSED;

	if (!loop) yy_error("case label not within a switch statement");
	if (!c_value_is_const(v1) || (!C_IS_TYPE_INT(v1->type) && v1->type->kind != C_TYPE_ENUM)
	 || !c_value_is_const(v2) || (!C_IS_TYPE_INT(v2->type) && v2->type->kind != C_TYPE_ENUM)) {
		yy_error("case labels do not reduce to integer constants");
	}
	if (rcc->active_ctx->control) {
		ir_END_list(list);
	}
	if (loop->switch_type != v1->type) {
		c_do_cvt(rcc, loop->switch_type, c_type2ir(rcc, loop->switch_type), v1);
	}
	if (loop->switch_type != v2->type) {
		c_do_cvt(rcc, loop->switch_type, c_type2ir(rcc, loop->switch_type), v2);
	}
	if (C_IS_TYPE_SIGNED(loop->switch_type)) {
		if (v1->u.val.i64 <= v2->u.val.i64) {
			c_case_labels_add_i(rcc, loop, v1->u.val, v2->u.val);
			if (v2->u.val.i64 - v1->u.val.i64 < 64) {
				int64_t i;

				for (i = v1->u.val.i64; i <= v2->u.val.i64; i++) {
					v1->u.val.i64 = i;
					ir_CASE_VAL(loop->check, c_value_ref(rcc, v1));
					ir_END_list(list);
				}
			} else {
				ir_CASE_RANGE(loop->check, c_value_ref(rcc, v1), c_value_ref(rcc, v2));
				ir_END_list(list);
			}
		} else {
			yy_warning("empty range specified");
		}
	} else {
		if (v1->u.val.u64 <= v2->u.val.u64) {
			c_case_labels_add_u(rcc, loop, v1->u.val, v2->u.val);
			if (v2->u.val.u64 - v1->u.val.u64 < 64) {
				uint64_t i;

				for (i = v1->u.val.u64; i <= v2->u.val.u64; i++) {
					v1->u.val.u64 = i;
					ir_CASE_VAL(loop->check, c_value_ref(rcc, v1));
					ir_END_list(list);
				}
			} else {
				ir_CASE_RANGE(loop->check, c_value_ref(rcc, v1), c_value_ref(rcc, v2));
				ir_END_list(list);
			}
		} else {
			yy_warning("empty range specified");
		}
	}
	ir_MERGE_list(list);
}

void c_do_case_default(rcc_ctx *rcc)
{
	c_loop *loop = c_find_switch(rcc);
	ir_ref prev = IR_UNUSED;

	if (!loop) yy_error("\"default\" label not within a switch statement");
	if (loop->next) yy_error("multiple default labels in one switch");
	if (rcc->active_ctx->control) {
		prev = ir_END();
	}
	ir_CASE_DEFAULT(loop->check);
	if (prev) {
		ir_MERGE_2(prev, ir_END());
	}
	loop->next = rcc->active_ctx->control;
}

void c_do_switch_end(rcc_ctx *rcc, c_loop *loop)
{
	if (!loop->next) {
		if (rcc->active_ctx->control) {
			ir_END_list(loop->break_list);
		}
		ir_CASE_DEFAULT(loop->check);
		ir_END_list(loop->break_list);
	}
	if (loop->break_list) {
		if (rcc->active_ctx->control) {
			ir_END_list(loop->break_list);
		}
		ir_MERGE_list(loop->break_list);
	}
	rcc->active_loop = loop->prev;
}

void c_do_loop_start(rcc_ctx *rcc, c_loop *loop)
{
	loop->is_switch = 0;
	loop->start = ir_LOOP_BEGIN(ir_END());
	loop->check = IR_UNUSED;
	loop->next = IR_UNUSED;
	loop->break_list = IR_UNUSED;
	loop->continue_list = IR_UNUSED;
	loop->scope = rcc->active_scope;
	loop->prev = rcc->active_loop;
	rcc->active_loop = loop;
}

void c_do_loop_check(rcc_ctx *rcc, c_loop *loop, c_value *cond)
{
	ir_ref ref;

	if (cond->type->kind == C_TYPE_VOID
	 || cond->type->kind == C_TYPE_STRUCT
	 || cond->type->kind == C_TYPE_UNION
	 || cond->type->kind == C_TYPE_VECTOR) {
		yy_error("scalar is required");
	} else if (C_IS_TYPE_FP(cond->type)) {
		ir_val val;
		val.u64 = 0;
		ref = ir_NE(c_value_ref(rcc, cond), ir_const(rcc->active_ctx, val, cond->u.type));
	} else {
		ref = c_value_ref(rcc, cond);
	}
	loop->check = ir_IF(ref);
	c_ir_IF_TRUE(rcc, loop->check);
}

void c_do_loop_continue_label(rcc_ctx *rcc, c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
		loop->continue_list = IR_UNUSED;
	}
}

void c_do_loop_end(rcc_ctx *rcc, c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
	}
	ir_ref end = ir_LOOP_END();
	rcc->active_ctx->ir_base[loop->start].op2 = end;
	c_ir_IF_FALSE(rcc, loop->check);
	if (loop->break_list) {
		ir_END_list(loop->break_list);
		ir_MERGE_list(loop->break_list);
	}
	rcc->active_loop = loop->prev;
}

void c_do_for_next_start(rcc_ctx *rcc, c_loop *loop)
{
	/* store "control" link in BEGIN.op2 */
	loop->next = rcc->active_ctx->control =
		ir_emit3(rcc->active_ctx, IR_BEGIN, IR_UNUSED, rcc->active_ctx->control, rcc->active_ctx->flags);
	/* disable FOLDING */
	rcc->active_ctx->flags &= ~IR_OPT_FOLDING;
}

void c_do_for_next_end(rcc_ctx *rcc, c_loop *loop)
{
	ir_ref end = ir_END();
	/* restore "control" link from BEGIN.op2 */
	rcc->active_ctx->control = rcc->active_ctx->ir_base[loop->next].op2;
	/* restore FOLDING */
	rcc->active_ctx->flags = rcc->active_ctx->ir_base[loop->next].op3;
	/* store END of "next" block in BEGIN.op2 */
	rcc->active_ctx->ir_base[loop->next].op2 = end;
			rcc->active_ctx->ir_base[loop->next].op3 = IR_UNUSED;
}

/* This function is usef to move FOR NEXT cofe "for(;;NEXT)" to the end of the loop body */
// TODO: Think about a simpler solution ???
static ir_ref ir_repeat_code_block(ir_ctx *ctx, ir_ref start, ir_ref end, ir_ref control)
{
	ir_ref *xlat = alloca(sizeof(ir_ref) * (end - start));
	uint32_t flags;
	ir_ref n, j, *p, input, ref = start + 1;
	ir_insn *insn = &ctx->ir_base[start];
	ir_list bp_list;

	bp_list.len = 0;
	IR_ASSERT(insn->op == IR_BEGIN);
	xlat[0] = control;
	ref = start + 1;
	while (ref != end) {
		bool may_fold = 1;

		insn = &ctx->ir_base[ref];
		flags = ir_op_flags[insn->op];
		if (UNEXPECTED(IR_OP_HAS_VAR_INPUTS(flags))) {
			n = insn->inputs_count;
		} else {
			n = IR_INPUT_EDGES_COUNT(flags);
		}
		for (j = n, p = insn->ops + 1; j > 0; j--, p++) {
			input = *p;
			if (input >= start) {
				if (input < ref) {
					*p = xlat[input - start];
				} else {
					IR_ASSERT(insn->op == IR_LOOP_BEGIN || insn->op == IR_MERGE || insn->op == IR_PHI);
					if (!bp_list.len) {
						ir_list_init(&bp_list, 32);
					}
					ir_list_push(&bp_list, ctx->insns_count);
					ir_list_push(&bp_list, n + 1 - j);
					may_fold = 0;
				}
			}
		}
		if (n <= 3 && IR_IS_FOLDABLE_OP(insn->op) && may_fold) {
			xlat[ref - start] = ir_fold(ctx, insn->opt, insn->op1, insn->op2, insn->op3);
			ref++;
		} else if (n <= 3) {
			ir_ref new_ref = ir_emit(ctx, insn->opt, insn->op1, insn->op2, insn->op3);
			xlat[ref - start] = new_ref;
			if (IR_OP_HAS_VAR_INPUTS(flags)) {
				ctx->ir_base[new_ref].inputs_count = n;
			}
			ref++;
		} else {
			ir_ref new_ref = ir_emit_N(ctx, insn->opt, n);
			xlat[ref - start] = new_ref;
			n = ir_insn_inputs_to_len(n);
			memcpy(&ctx->ir_base[new_ref], &ctx->ir_base[ref], sizeof(ir_insn) * n);
			ref += n;
		}
	}

	if (ir_list_len(&bp_list)) {
		do {
			j = ir_list_pop(&bp_list);
			ref = ir_list_pop(&bp_list);
			p = ctx->ir_base[ref].ops;
			p += j;
			input = *p;
			if (input >= start) {
				IR_ASSERT(input < end);
				*p = xlat[input - start];
			}
		} while (ir_list_len(&bp_list));
		ir_list_free(&bp_list);
	}

	insn = &ctx->ir_base[end];
	IR_ASSERT((insn->op == IR_END || insn->op == IR_LOOP_END) && insn->op1 >= start);
	return xlat[insn->op1 - start];
}

void c_do_for_end(rcc_ctx *rcc, c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
	}

	if (loop->next) {
		ir_ref start = loop->next;
		ir_ref end = rcc->active_ctx->ir_base[loop->next].op2;
		rcc->active_ctx->control = ir_repeat_code_block(rcc->active_ctx, start, end, rcc->active_ctx->control);
		memset(&rcc->active_ctx->ir_base[start], 0, sizeof(ir_insn) * ((end - start) + 1));
	}

	ir_ref end = ir_LOOP_END();
	rcc->active_ctx->ir_base[loop->start].op2 = end;

	if (loop->check) {
		c_ir_IF_FALSE(rcc, loop->check);
		if (loop->break_list) {
			ir_END_list(loop->break_list);
			ir_MERGE_list(loop->break_list);
		}
	} else if (loop->break_list) {
		ir_MERGE_list(loop->break_list);
	} else {
		ir_BEGIN(IR_UNUSED);
	}
	rcc->active_loop = loop->prev;
}

static c_loop *c_find_loop(rcc_ctx *rcc)
{
	c_loop *loop = rcc->active_loop;

	while (loop) {
		if (!loop->is_switch) return loop;
		loop = loop->prev;
	}
	return NULL;
}

void c_do_continue(rcc_ctx *rcc)
{
	c_loop *loop = c_find_loop(rcc);

	if (!loop) yy_error("continue statement not within a loop");
	c_leave_scope(rcc, loop->scope);
	ir_END_list(loop->continue_list);
	ir_BEGIN(IR_UNUSED);
}

void c_do_break(rcc_ctx *rcc)
{
	if (!rcc->active_loop) yy_error("break statement not within loop or switch");
	c_leave_scope(rcc, rcc->active_loop->scope);
	ir_END_list(rcc->active_loop->break_list);
	ir_BEGIN(IR_UNUSED);
}

static bool c_is_local_address(const ir_insn *insn)
{
	if (insn->op == IR_VADDR || insn->op == IR_ALLOCA) {
		return 1;
	}
	return 0;
}

static bool c_is_no_local_address(const ir_insn *insn)
{
	if (insn->op == IR_PARAM || insn->op == IR_CALL) {
		return 1;
	}
	return 0;
}

static bool c_may_tailcall(rcc_ctx *rcc, ir_ref ref, bool musttail)
{
	const ir_insn *insn = &rcc->active_ctx->ir_base[ref];
	const c_type *t1;
	const c_type *t2;
	const c_param *p1, *p2;
	uint32_t n;
	uint32_t attr1, attr2;

	if (!rcc->c_last_call_func_type || insn->op != IR_CALL) {
		if (musttail) yy_error("cannot tail-call: return value must be a call");
		return 0;
	}

	t1 = rcc->c_last_call_func_type;
	t2 = rcc->active_func->value.type;
	if (t1->kind == C_TYPE_POINTER) t1 = t1->pointer.type;
	IR_ASSERT(t1->kind == C_TYPE_FUNC && t2->kind == C_TYPE_FUNC);

	attr1 = t1->attr;
	attr2 = t2->attr;
	if ((attr1 | attr2) & C_ATTR_OLD_FUNC) {
		if (musttail) yy_error("cannot tail-call: functions must have prototypes");
		return 0;
	} else if ((attr1 | attr2) & C_ATTR_VARIADIC) {
		if (musttail) yy_error("cannot tail-call: variadic functions are not supported");
		return 0;
	} else if ((attr1 & C_ATTR_CALL_CONV) != (attr2 & C_ATTR_CALL_CONV)) {
		if (musttail) yy_error("cannot tail-call: incompatible function prototypes");
		return 0;
	}

	if (!c_compatible_types(t1->func.ret_type, t2->func.ret_type, 0, 0)) {
		if (musttail) yy_error("cannot tail-call: incompatible function prototypes");
		return 0;
	}

	if (t1->func.num_params != t2->func.num_params) {
		if (musttail) yy_error("cannot tail-call: incompatible function prototypes");
		return 0;
	}
	p1 = t1->func.params;
	p2 = t2->func.params;
	for (n = t1->func.num_params; n > 0; p1++, p2++, n--) {
		if (!c_compatible_types(p1->type, p2->type, 1, 0)) {
			if (musttail) yy_error("cannot tail-call: incompatible function prototypes");
			return 0;
		}
	}

	/* check for VLA arrays */
	c_scope *scope = rcc->active_scope;
	while (scope != rcc->active_func_scope) {
		IR_ASSERT(scope);
		if (scope->vla_block) {
			if (musttail) yy_error("cannot tail-call: blocked by VLA");
			return 0;
		} else if (scope->cleanup_sym) {
			if (musttail) yy_error("cannot tail-call: blocked by variable with __attribute__((cleanup))");
			return 0;
		}
		scope = scope->prev;
	}

	if (scope->cleanup_sym) {
		if (musttail) yy_error("cannot tail-call: blocked by variable with __attribute__((cleanup))");
		return 0;
	}

	if (insn->inputs_count != t2->func.num_params + 2) {
		if (musttail) yy_error("cannot tail-call: other reasons");
		return 0;
	}

	n = ir_insn_inputs_to_len(insn->inputs_count);
	if (rcc->active_ctx->insns_count != ref + (ir_ref)n) {
		if (musttail) yy_error("cannot tail-call: other reasons");
		return 0;
	}

	/* check for passing addresses of local variable */
	n = insn->inputs_count;
	for (uint32_t i = 3; i <= n; i++) {
		ir_ref input = ir_insn_op(insn, i);
		if (!IR_IS_CONST_REF(input) && rcc->active_ctx->ir_base[input].type == IR_ADDR) {
			if (musttail) {
				if (c_is_local_address(&rcc->active_ctx->ir_base[input])) {
					yy_error("cannot tail-call: passing address of local variable");
					return 0;
				}
			} else if (!c_is_no_local_address(&rcc->active_ctx->ir_base[input])) {
				return 0;
			}
		}
	}

	return 1;
}

void c_do_return(rcc_ctx *rcc, c_value *val)
{
	IR_ASSERT(rcc->active_func);
	c_leave_scope(rcc, rcc->active_func_scope);
	if (rcc->active_func_scope->cleanup_sym) {
		c_do_cleanup_vars(rcc, rcc->active_func_scope->cleanup_sym, NULL);
	}
	if (val && c_value_is_set(val) && val->type->kind != C_TYPE_VOID) {
		if (!rcc->active_ctx->ret_type) {
			yy_error("\"return\" with a value, in function returning void");
		} else if (rcc->active_func->value.type->func.ret_type != val->type) {
			c_do_check_cvt(rcc, rcc->active_func->value.type->func.ret_type, val, 0);
		}
		if ((rcc->c_opt_flags & C_OPT_TAILCALL)
		 && c_value_is_ref(val)
		 && rcc->active_ctx->ir_base[val->u.ref].op == IR_CALL
		 && rcc->active_ctx->insns_count ==
			val->u.ref + (ir_ref)ir_insn_inputs_to_len(rcc->active_ctx->ir_base[val->u.ref].inputs_count)
		 && c_may_tailcall(rcc, val->u.ref, 0)) {
			rcc->active_ctx->ir_base[val->u.ref].op = IR_TAILCALL;
			ir_UNREACHABLE();
		} else if (val->type->kind == C_TYPE_STRUCT || val->type->kind == C_TYPE_UNION) {
			ir_type types[MAX_ABI_TYPES];
			int n = c_abi_lower_struct_ret(val->type, types);

			if (n == 1) {
				ir_RETURN(ir_LOAD(types[0], val->u.ref));
			} else {
				IR_ASSERT(n == 0);
				ir_ref ret_param = 2; /* 2 - reference to the first IR_PARAM */
				ir_memcpy(rcc, ret_param, val->u.ref,
					ir_const_size_t(rcc->active_ctx, val->type->size), c_attr2align(val->type->attr));
				ir_RETURN(ret_param);
			}
		} else {
			ir_RETURN(c_value_ref(rcc, val));
		}
	} else {
		if (rcc->active_ctx->ret_type) yy_error("\"return\" with no value, in function returning non-void");
		if ((rcc->c_opt_flags & C_OPT_TAILCALL)
		 && rcc->active_ctx->ir_base[rcc->active_ctx->control].op == IR_CALL
		 && rcc->active_ctx->insns_count ==
			rcc->active_ctx->control + (ir_ref)ir_insn_inputs_to_len(rcc->active_ctx->ir_base[rcc->active_ctx->control].inputs_count)
		 && c_may_tailcall(rcc, rcc->active_ctx->control, 0)) {
			rcc->active_ctx->ir_base[rcc->active_ctx->control].op = IR_TAILCALL;
			ir_UNREACHABLE();
		} else {
			ir_RETURN(IR_UNUSED);
		}
	}
	/* start an unreachable block  (it's going to be optimized out) */
	ir_BEGIN(IR_UNUSED);
}

void c_do_tailcall(rcc_ctx *rcc, c_value *val)
{
	IR_ASSERT(rcc->active_func);
	if (!val || !c_value_is_ref(val)) {
		yy_error("cannot tail-call: return value must be a call");
	} else if (!c_may_tailcall(rcc, val->u.ref, 1)) {
		c_do_return(rcc, val);
		return;
	}
	c_leave_scope(rcc, rcc->active_func_scope);
	if (rcc->active_func_scope->cleanup_sym) {
		c_do_cleanup_vars(rcc, rcc->active_func_scope->cleanup_sym, NULL);
	}

	IR_ASSERT(val
		&& c_value_is_ref(val)
		&& rcc->active_ctx->ir_base[val->u.ref].op == IR_CALL
		&& rcc->active_ctx->insns_count ==
			val->u.ref + (ir_ref)ir_insn_inputs_to_len(rcc->active_ctx->ir_base[val->u.ref].inputs_count));

	rcc->active_ctx->ir_base[val->u.ref].op = IR_TAILCALL;
	ir_UNREACHABLE();

	/* start an unreachable block  (it's going to be optimized out) */
	ir_BEGIN(IR_UNUSED);
}

static void c_do_cleanup_vars_goto(rcc_ctx *rcc, c_sym *from, c_sym *to)
{
	c_sym *sym;
	uint32_t goto_depth = 0, label_depth = 0;

	for (sym = from; sym != NULL; sym = sym->cleanup_next) goto_depth++;
	for (sym = to; sym != NULL; sym = sym->cleanup_next) label_depth++;
	sym = from;
	while (goto_depth > label_depth) {
		 sym = sym->cleanup_next;
		 goto_depth--;
	}
	while (goto_depth < label_depth) {
		to = to->cleanup_next;
		label_depth--;
	}
	while (sym != to) {
		sym = sym->cleanup_next;
		to = to->cleanup_next;
	}
	if (from != to) {
		c_do_cleanup_vars(rcc, from, to);
	}
}

static ir_ref c_cleanup_sym_to_ref(rcc_ctx *rcc, c_sym *sym)
{
	if (sizeof(ir_ref) == sizeof(c_sym)) {
		return (ir_ref)(uintptr_t)sym;
	} else {
		ir_arena *arena = rcc->c_func_arena;

		while (arena) {
			if ((char*)sym > (char*)arena && (char*)sym < (char*)arena->ptr) {
				size_t offset = (char*)sym - (char*)arena;
				arena = arena->prev;
				while (arena) {
					offset += (char*)arena->end - (char*)arena;
					arena = arena->prev;
				}
				IR_ASSERT(offset < 0x7fffffff);
				return offset;
			}
			arena = arena->prev;
		}
		IR_ASSERT(0);
		return IR_UNUSED;
	}
}

static c_sym *c_ref_to_cleanup_sym(rcc_ctx *rcc, ir_ref ref)
{
	if (sizeof(ir_ref) == sizeof(c_sym)) {
		return (c_sym*)(uintptr_t)ref;
	} else {
		ir_arena *arena = rcc->c_func_arena;
		size_t offset = (char*)arena->ptr - (char*)arena;

		arena = arena->prev;
		while (arena) {
			offset += (char*)arena->end - (char*)arena;
			arena = arena->prev;
		}
		IR_ASSERT(offset < 0x7fffffff && (ir_ref)offset > ref);

		arena = rcc->c_func_arena;
		offset -= (char*)arena->ptr - (char*)arena;
		while (1) {
			if ((ir_ref)offset < ref) {
				return (c_sym*)((char*)arena + (size_t)ref - offset);
			}
			arena = arena->prev;
			offset -= (char*)arena->end - (char*)arena;
		}
		IR_ASSERT(0);
		return NULL;
	}
}

void c_do_goto(rcc_ctx *rcc, c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = rcc->yy_hash.data[name].label;
	if (!label) {
		label = c_new_label(rcc, name, rcc->active_func_scope, NULL, rcc->active_scope == rcc->active_func_scope);
	}

	if (rcc->active_scope->cleanup_sym) {
		if (label->dst && label->cleanup_sym != rcc->active_scope->cleanup_sym) {
			c_do_cleanup_vars_goto(rcc, rcc->active_scope->cleanup_sym, label->cleanup_sym);
		}
	}

	if (rcc->active_scope->last_vla_block) {
		ir_ref goto_vla_block = rcc->active_scope->last_vla_block;

		if (!label->dst) {
			/* Forward GOTO. Insert BLOCK_END that may be patched or removed later */
			ir_BLOCK_END(goto_vla_block);
		} else if (!label->vla_block) {
			/* GOTO to label at non-VLA scope => free all VLA data */
			IR_ASSERT(rcc->c_prologue_end);
			ir_BLOCK_END(rcc->c_prologue_end);
		} else if (goto_vla_block != label->vla_block) {
			/* GOTO to label at the other VLA scope. ... */
			ir_ref vla_block = goto_vla_block;
			c_scope *scope = rcc->active_scope;

			while (scope) {
				if (scope->vla_block) {
					if (scope->vla_block == label->vla_block) break;
					vla_block = scope->vla_block;
				}
				scope = scope->prev;
			}
			if (!scope) {
				yy_error_fmt("jump to label \"%s\" into scope with variable modified type", yy_sym2str(rcc, name));
			} else {
				/* GOTO to label at some enclosing VLA scope => free nested VLA */
				ir_BLOCK_END(vla_block);
			}
		}
	} else if (label->vla_block) {
		yy_error_fmt("jump to label \"%s\" into scope with variable modified type", yy_sym2str(rcc, name));
	}

	ir_END_list(label->src_list);
	if (rcc->active_scope->cleanup_sym && !label->dst) {
		/* Remember last cleanup variable of the current scope */
		c_sym *sym = rcc->active_scope->cleanup_sym;
		while (sym) {
			if (!(sym->value.type->flags & (C_TYPE_GLOBAL|C_TYPE_IN_FUNC))) {
				sym->value.type = c_create_in_func_type(rcc, sym->value.type);
			}
			sym = sym->cleanup_next;
		}
		rcc->active_ctx->ir_base[label->src_list].op3 = c_cleanup_sym_to_ref(rcc, rcc->active_scope->cleanup_sym);
	}
	ir_BEGIN(IR_UNUSED);
}

c_label *c_do_set_label(rcc_ctx *rcc, c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = rcc->yy_hash.data[name].label;
	if (!label) {
		label = c_new_label(rcc, name, rcc->active_func_scope, NULL, rcc->active_scope == rcc->active_func_scope);
	} else if (label->dst) {
		yy_error_fmt("duplicate label \"%s\"", yy_sym2str(rcc, name));
		return NULL;
	}

	label->vla_block = rcc->active_scope->last_vla_block;
	if (label->src_list) {
		ir_ref ref = label->src_list;
		uint32_t n = 0;
		ir_ref *srcs;

		/* count inputs count */
		do {
			ir_insn *insn = &rcc->active_ctx->ir_base[ref];

			IR_ASSERT(insn->op == IR_END);
			if (insn->op3) {
				/* Cleanup Variables */
				ir_ref orig_control = rcc->active_ctx->control;

				rcc->active_ctx->control =
					rcc->active_ctx->ir_base[insn->op1].op == IR_BLOCK_END ?
						rcc->active_ctx->ir_base[insn->op1].op1 : insn->op1;
				c_do_cleanup_vars_goto(rcc, c_ref_to_cleanup_sym(rcc, insn->op3), rcc->active_scope->cleanup_sym);
				insn = &rcc->active_ctx->ir_base[ref];
				insn->op1 = rcc->active_ctx->control;
				insn->op3 = IR_UNUSED;
				rcc->active_ctx->control = orig_control;
			}
			/* fix BLOCK_END instructions inseted by forward GOTO */
			ir_insn *prev = &rcc->active_ctx->ir_base[insn->op1];
			if (prev->op == IR_BLOCK_END) {
				ir_ref goto_vla_block = prev->op2;

				if (!label->vla_block) {
					IR_ASSERT(rcc->c_prologue_end);
					prev->op2 = rcc->c_prologue_end;
				} else if (label->vla_block > goto_vla_block) {
					/* label at a VLA scope started after the GOTO => error */
					yy_error_fmt("jump to label \"%s\" into scope with variable modified type", yy_sym2str(rcc, name));
				} else if (label->vla_block == goto_vla_block) {
					/* label at the same VLA scope as GOTO => delete BLOCK_END */
					insn->op1 = prev->op1;
					prev->optx = IR_NOP;
					prev->op1 = IR_UNUSED;
				} else {
					/* label at some VLA scope enclosing the GOTO => patch to free nested VLA */
					prev->op2 = label->vla_block;
				}
			} else if (label->vla_block) {
				yy_error_fmt("jump to label \"%s\" into scope with variable modified type", yy_sym2str(rcc, name));
			}
			ref = insn->op2;
			n++;
		} while (ref != IR_UNUSED);

		srcs = alloca(sizeof(ir_ref) * (n + 2));

		ref = label->src_list;
		n = 0;
		srcs[n++] = ir_END();
		do {
			ir_insn *insn = &rcc->active_ctx->ir_base[ref];

			srcs[n] = ref;
			IR_ASSERT(insn->op == IR_END);
			ref = insn->op2;
			n++;
		} while (ref != IR_UNUSED);
		srcs[n++] = IR_UNUSED;

		ir_MERGE_N(n, srcs);

		label->src_list = IR_UNUSED;
	} else {
		ir_MERGE_2(ir_END(), IR_UNUSED);
	}
	label->dst = rcc->active_ctx->control;

	if (label->value_block) {
		/* remember label block strat in BEGIN node corresponding to label address */
		IR_ASSERT(rcc->active_ctx->ir_base[label->value_block].op == IR_BEGIN);
		rcc->active_ctx->ir_base[label->value_block].op1 = label->dst;
	}

	label->cleanup_sym = rcc->active_scope->cleanup_sym;

	return label;
}

void c_do_set_label_attrs(rcc_ctx *rcc, c_label *label, c_dcl *attrs)
{
	if (attrs->attr & C_ATTR_UNUSED) {
		label->is_unused = 1;
	}
}

void c_do_finish_label(rcc_ctx *rcc, c_name name, c_label *label)
{
	if (label->dst) {
		ir_ref end, *ops;
		ir_insn *insn = &rcc->active_ctx->ir_base[label->dst];

		IR_ASSERT(insn->op == IR_MERGE);
		if (!label->src_list) {
			insn->inputs_count--;
			if (insn->inputs_count == 1) {
				if (!label->is_unused) {
					yy_warning_fmt("label \"%s\" defined but not used", yy_sym2str(rcc, name));
				}
				insn->op = IR_BEGIN;
			}
		} else {
			if (!rcc->active_ctx->ir_base[label->src_list].op2) {
				/* one element list */
				IR_ASSERT(rcc->active_ctx->ir_base[label->src_list].op == IR_END);
				end = label->src_list;
			} else {
				ir_ref prev = IR_UNUSED;

				if (rcc->active_ctx->control) {
					prev = ir_END(); // TODO: try to avoid this contol split ???
				}
				ir_MERGE_list(label->src_list);
				end = ir_END();
				if (prev) {
					ir_BEGIN(prev);
				}
			}
			insn = &rcc->active_ctx->ir_base[label->dst];
			ops = insn->ops;
			ops[insn->inputs_count] = end;
		}
	} else if (label->src_list) {
		yy_error_fmt("label \"%s\" used but not defined", yy_sym2str(rcc, name));
	}
}

void c_do_label_value(rcc_ctx *rcc, c_value *res, c_name label_name)
{
	c_label *label;

	IR_ASSERT(label_name);
	if (!rcc->active_func_scope) {
		yy_error_fmt("label \"%s\" referenced outside of any function", yy_sym2str(rcc, label_name));
	}
	label = rcc->yy_hash.data[label_name].label;
	if (!label) {
		label = c_new_label(rcc, label_name, rcc->active_func_scope, NULL, rcc->active_scope == rcc->active_func_scope);
	}
	if (!label->value_sym) {
		ir_ref end = ir_END();

		label->value_sym = ir_const_label(rcc->active_ctx, c_create_label_str(rcc, ++rcc->c_label_num));
		rcc->c_computed_goto_targets = rcc->active_ctx->control = label->value_block =
			ir_emit3(rcc->active_ctx, IR_BEGIN, label->dst, label->value_sym, rcc->c_computed_goto_targets);
		ir_END_list(label->src_list);
		ir_BEGIN(end);
	}
	c_value_set_rval(res, &c_type_ptr, IR_ADDR, label->value_sym);
}

void c_do_computed_goto(rcc_ctx *rcc, c_value *v)
{
	ir_END_PHI_list(rcc->c_computed_goto, c_value_ref(rcc, v));
	ir_BEGIN(IR_UNUSED);
}

static void c_do_init_vector(rcc_ctx *rcc, void *addr, c_value *val)
{
	IR_ASSERT(c_value_is_ref(val) && IR_IS_CONST_REF(val->u.ref));
	if (rcc->active_ctx->ir_base[val->u.ref].op == IR_SYM) {
		IR_ASSERT(val->u.val.ptr);
		memcpy(addr, val->u.val.ptr, val->type->size);
	} else {
		IR_ASSERT(rcc->active_ctx->ir_base[val->u.ref].op == IR_LONG_CONST);
		memcpy(addr, ir_long_const_ptr(rcc->active_ctx, val->u.ref), val->type->size);
	}
}

static void c_do_init(rcc_ctx *rcc, void *addr, c_value *val)
{
	const c_type *type = val->type;
	c_type_kind kind = type->kind;
repeat:
	switch (kind) {
		case C_TYPE_BOOL:
		case C_TYPE_CHAR:
		case C_TYPE_I8:
		case C_TYPE_U8:       memcpy(addr, &val->u.val.u8, type->size); break;
		case C_TYPE_I16:
		case C_TYPE_U16:      memcpy(addr, &val->u.val.u16, type->size); break;
#if C_LONG_SIZE == 4
		case C_TYPE_IL:
		case C_TYPE_UL:
#endif
		case C_TYPE_I32:
		case C_TYPE_U32:      memcpy(addr, &val->u.val.u32, type->size); break;
#if C_LONG_SIZE == 8
		case C_TYPE_IL:
		case C_TYPE_UL:
#endif
		case C_TYPE_ILL:
		case C_TYPE_ULL:      memcpy(addr, &val->u.val.u64, type->size); break;
		case C_TYPE_FLOAT:    memcpy(addr, &val->u.val.f, type->size); break;
		case C_TYPE_DOUBLE:   memcpy(addr, &val->u.val.d, type->size); break;
		case C_TYPE_POINTER:  memcpy(addr, &val->u.val.addr, type->size); break;
		case C_TYPE_ARRAY:
		case C_TYPE_STRUCT:
		case C_TYPE_UNION:    memcpy(addr, val->u.val.ptr, type->size); break;
		case C_TYPE_ENUM:     kind = type->enumeration.kind; goto repeat;
		case C_TYPE_VECTOR:   c_do_init_vector(rcc, addr, val); break;
		default: IR_ASSERT(0);
	}
}

static void c_do_init_patch_flexible_alloca(rcc_ctx *rcc, ir_ref ref, size_t len)
{
	ir_insn *insn = &rcc->active_ctx->ir_base[ref];
	ir_ref size_ref;

	IR_ASSERT(ref > 0);
	IR_ASSERT(insn->op == IR_ALLOCA);
	insn->op2 = size_ref = ir_const_size_t(rcc->active_ctx, len);
	insn++;
	if (insn->op == IR_CALL
	 && insn->inputs_count == 5
	 && insn->op1 == ref
//???	 && insn->op2 == memset
	 && insn->op3 == ref) {
		ir_ref *ops = insn->ops;
		ops[5] = size_ref;
	}
}

void c_do_init_obj(rcc_ctx *rcc, c_sym *obj, c_value *val)
{
	if (obj->kind != C_SYM_VAR) {
		if (obj->kind == C_SYM_FUNC) yy_error("function is initialized like a variable");
		if (obj->kind == C_SYM_TYPE) yy_error("typedef is initialized");
		IR_ASSERT(0);
	}
	if (obj->value.type->attr & C_ATTR_VLA) {
		IR_ASSERT(obj->value.type->kind == C_TYPE_ARRAY);
		yy_error("variable length array may not be initialized except with an empty initializer");
	}
	c_value_rval(rcc, val);
	if (obj->value.type != val->type) {
		if (c_value_is_const_str(val)
		 && obj->value.type->kind == C_TYPE_ARRAY
		 && (obj->value.type->array.type->kind == val->type->array.type->kind
		  || (val->type->array.type->size == 1
		   && (obj->value.type->array.type->kind == C_TYPE_U8
		    || obj->value.type->array.type->kind == C_TYPE_I8)))) {
			const void *str = c_value_str_addr(val);
			size_t len = c_value_str_size(val);
			if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
				/* Convert "flexible" array to regular */
				c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));

				*type = *obj->value.type;
				if (rcc->active_scope) type->flags &= ~C_TYPE_GLOBAL;
				type->size = len;
				type->array.length = len / val->type->array.type->size;
				type->attr &= ~C_ATTR_FLEXIBLE;
				obj->value.type = type;
				if (c_value_is_const(&obj->value)
				 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
					c_do_grow_flexible(rcc, obj, 0, obj->value.type->size);
				} else {
					c_do_init_patch_flexible_alloca(rcc, obj->value.u.ref, len);
				}
			} else if (len > obj->value.type->array.length * val->type->array.type->size) {
				if (len - val->type->array.type->size == obj->value.type->array.length * val->type->array.type->size) {
					len -= val->type->array.type->size;
				} else if (val->type->array.type->size == 1) {
					yy_error("initializer-string for array of \"char\" is too long");
				} else {
					yy_error("initializer-string for array is too long");
				}
			}
			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				if (!c_value_is_const(val) && !c_linker_fix_reloc(rcc, obj, 0, val)) {
					yy_error("initializer element is not constant");
				}
				IR_ASSERT(obj->value.u.type == IR_ADDR);
				memcpy((char*)obj->value.u.val.ptr, str, len);
				if (obj->tmp_data) {
					c_do_end_flexible(rcc, obj, obj->value.type->size);
				}
			} else {
				IR_ASSERT(obj->value.u.ref > 0);
				IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
				ir_memcpy(rcc,
					obj->value.u.ref,
					c_create_str_sym(rcc, val),
					ir_const_size_t(rcc->active_ctx, len),
					c_attr2align(obj->value.type->attr));
			}
			return;
		}

		const c_type *val_type = val->type;
		c_do_check_cvt(rcc, obj->value.type, val, -2);
		if ((obj->value.type->attr & C_ATTR_FLEXIBLE) && !(val_type->attr & C_ATTR_FLEXIBLE)) {
			size_t size = val_type->size;

			if (obj->value.type->kind == C_TYPE_ARRAY) {
				/* Convert "flexible" array to regular */
				c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));

				*type = *obj->value.type;
				if (rcc->active_scope) type->flags |= C_TYPE_GLOBAL;
				type->array.length = size / type->array.type->size;
				type->size = size;
				type->attr &= ~C_ATTR_FLEXIBLE;
				val->type = obj->value.type = type;
			}
			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				c_do_grow_flexible(rcc, obj, 0, obj->value.type->size);
			} else {
				c_do_init_patch_flexible_alloca(rcc, obj->value.u.ref, size);
			}
		}
	}
	if (obj->linkage == C_LINK_EXTERNAL || obj->linkage == C_LINK_INTERNAL) {
		IR_ASSERT((c_value_is_const(&obj->value)
				&& obj->value.u.type == IR_ADDR
				&& obj->value.u.val.ptr)
			|| (c_value_is_ref(&obj->value)
				&& rcc->active_scope
				&& IR_IS_CONST_REF(obj->value.u.ref)
				&& rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_SYM
				&& obj->value.u.val.ptr));
		if (!c_value_is_const(val) && !c_linker_fix_reloc(rcc, obj, 0, val)) {
			yy_error("initializer element is not constant");
		}
		c_do_init(rcc, obj->value.u.val.ptr, val);
	} else if (C_IS_TYPE_SCALAR_OR_PTR(obj->value.type) || obj->value.type->kind == C_TYPE_VECTOR) {
		IR_ASSERT(c_value_is_ref(&obj->value));
		if (rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_VAR) {
			ir_VSTORE(obj->value.u.ref, c_value_ref(rcc, val));
		} else if (c_value_is_reg(&obj->value)) {
			ir_RSTORE(obj->value.u.ref, c_value_ref(rcc, val));
		} else {
			ir_STORE(obj->value.u.ref, c_value_ref(rcc, val));
		}
	} else {
		IR_ASSERT(obj->value.type->size == val->type->size);
		if (c_value_is_const(&obj->value)
		 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
			if (!c_value_is_const(val) && !c_linker_fix_reloc(rcc, obj, 0, val)) {
				yy_error("initializer element is not constant");
			}
			IR_ASSERT(obj->value.u.type == IR_ADDR);
			IR_ASSERT(val->u.type == IR_ADDR);
			memcpy((char*)obj->value.u.val.ptr, val->u.val.ptr, val->type->size);
		} else {
			IR_ASSERT(obj->value.u.ref > 0);
			IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			ir_memcpy(rcc,
				obj->value.u.ref,
				c_value_ref(rcc, val),
				ir_const_size_t(rcc->active_ctx, val->type->size),
				c_attr2align(val->type->attr));
		}
	}
	if (obj->tmp_data) {
		c_do_end_flexible(rcc, obj, obj->value.type->size);
	}
}

void c_do_init_start(rcc_ctx *rcc, c_sym *obj, c_init *init)
{
	const c_type *type = obj->value.type;

	init->size = type->size;
	init->level = 0;
	init->ranges = 0;
	init->stack[0].type = type;
	init->stack[0].pos = 0;
	init->stack[0].last = 0;

	if (type->kind == C_TYPE_VECTOR
	 && !c_value_is_const(&obj->value) && !(c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
		ir_val v;
		ir_ref ref;

		v.u64 = 0;
		ref = ir_SPLAT(c_type2ir(rcc, type), ir_const(rcc->active_ctx, v, c_type2ir(rcc, type->vec.type)));

		if (rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_VAR) {
			ir_VSTORE(obj->value.u.ref, ref);
		} else {
			IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			ir_STORE(obj->value.u.ref, ref);
		}
	}
}

void c_do_init_dim(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *dim)
{
	const c_type *type = init->stack[init->level].type;

	if (type->kind != C_TYPE_ARRAY) yy_error("array index in non-array initializer");
	if (!c_value_is_const(dim) || !C_IS_TYPE_INT(dim->type)) yy_error("array index in initializer not an integer constant");
	if (C_IS_TYPE_SIGNED(dim->type) && dim->u.val.i64 < 0) yy_error("array index in initializer exceeds array bounds");
	if (dim->u.val.u64 >= type->array.length && !(type->attr & C_ATTR_FLEXIBLE)) yy_error("array index in initializer exceeds array bounds");

	if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
	init->stack[init->level].pos = dim->u.val.i64;
	init->stack[init->level].last = 0;
	init->level++;
	init->stack[init->level].type = type->array.type;
	init->stack[init->level].pos = 0;
	init->stack[init->level].last = 0;
}

void c_do_init_range(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *last)
{
	const c_type *type = init->stack[init->level - 1].type;

	if (!c_value_is_const(last) || !C_IS_TYPE_INT(last->type)) yy_error("array index in initializer not an integer constant");
	if (C_IS_TYPE_SIGNED(last->type) && last->u.val.i64 < 0) yy_error("array index in initializer exceeds array bounds");
	if (last->u.val.u64 >= type->array.length && !(type->attr & C_ATTR_FLEXIBLE)) yy_error("array index in initializer exceeds array bounds");
	if (last->u.val.u64 < init->stack[init->level - 1].pos) yy_error("empty index range in initializer");

	if (init->stack[init->level - 1].pos != last->u.val.u64) {
		init->stack[init->level - 1].last = last->u.val.u64;
		init->ranges++;
	}
}

static bool c_find_struct_field_ex(rcc_ctx *rcc, const c_type *type, c_name name, c_init *init)
{
	uint32_t i;
	c_field *f;

	for (i = 0, f = type->record.fields; i < type->record.num_fields; f++, i++) {
		if (f->name == name) {
			if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
			init->stack[init->level].pos = i;
			init->stack[init->level].last = 0;
			init->level++;
			init->stack[init->level].type = f->type;
			init->stack[init->level].pos = 0;
			init->stack[init->level].last = 0;
			return 1;
		} else if (!f->name && (f->type->kind == C_TYPE_STRUCT || f->type->kind == C_TYPE_UNION)) {
			if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
			init->stack[init->level].pos = i;
			init->stack[init->level].last = 0;
			init->level++;
			init->stack[init->level].type = f->type;
			if (c_find_struct_field_ex(rcc, f->type, name, init)) return 1;
			init->level--;
		}
	}
	return 0;
}

void c_do_init_field(rcc_ctx *rcc, c_sym *obj, c_init *init, c_name field_name)
{
	const c_type *type = init->stack[init->level].type;

	if (type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) {
		yy_error("field name not in struct or union initializer");
	} else if (!c_find_struct_field_ex(rcc, type, field_name, init)) {
		yy_error_fmt("struct or union has no member named \"%s\"", yy_sym2str(rcc, field_name));
	}
}

void c_do_init_next(rcc_ctx *rcc, c_sym *obj, c_init *init)
{
	const c_type *type;
	uint64_t pos;

	while (1) {
		type = init->stack[init->level].type;
		if (type->kind == C_TYPE_ARRAY) {
			pos = init->stack[init->level].pos;
			if (++pos < type->array.length || (type->attr & C_ATTR_FLEXIBLE)) {
				init->stack[init->level].pos = pos;
				return;
			}
			if (init->level == 0) yy_error("excess elements in array initializer");
		} else if (type->kind == C_TYPE_STRUCT) {
			pos = init->stack[init->level].pos;
			if (++pos < type->record.num_fields) {
				init->stack[init->level].pos = pos;
				return;
			}
			if (init->level == 0) yy_error("excess elements in struct initializer");
		} else if (type->kind == C_TYPE_UNION) {
			if (init->level == 0) {
				yy_warning("excess elements in union initializer");
				return;
			}
		} else if (type->kind == C_TYPE_VECTOR) {
			pos = init->stack[init->level].pos;
			if (++pos < type->vec.length) {
				init->stack[init->level].pos = pos;
				return;
			}
			if (init->level == 0) yy_error("excess elements in vector initializer");
		} else {
			if (init->level == 0) yy_error("excess elements in scalar initializer");
		}
		init->level--;
	}
}

void c_do_init_rollback(rcc_ctx *rcc, c_sym *obj, c_init *init, uint32_t orig_level, uint32_t level)
{
	if (init->ranges) {
		while (init->level > level) {
			if (init->stack[init->level].last) {
				init->ranges--;
			}
			init->level--;
		}
		while(1) {
			if (init->stack[level].last) {
				init->stack[level].pos = init->stack[level].last;
				init->stack[level].last = 0;
				init->ranges--;
			}
			if (level == orig_level) break;
			level--;
		}
	}
	init->level = level;
}

void c_do_init_set(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *val)
{
	const c_type *type = init->stack[init->level].type;
	const c_type *last_array_type = NULL;
	size_t last_array_offset = 0;
	size_t offset, new_size;
	uint32_t i;
	uint16_t bit_field = 0;

	if (init->ranges) {
		typedef struct {
			int64_t  orig_pos;
			uint32_t level;
		} range_pos_t;
		uint32_t i, j, ranges = init->ranges;
		range_pos_t *range_pos = alloca(sizeof(range_pos_t) * ranges);
		bool done;

		/* remember range positions */
		for (i = 0, j = 0; i <= init->level; i++) {
			if (init->stack[i].last != 0) {
				range_pos[j].orig_pos = init->stack[i].pos;
				range_pos[j].level = i;
				j++;
			}
		}
		IR_ASSERT(j == ranges);

		/* Call c_do_init_set() for all combinations of ranges */
		init->ranges = 0;
		done = 0;
		do {
			c_do_init_set(rcc, obj, init, val);
			for (i = ranges; i > 0; i--) {
				j = range_pos[i - 1].level;
				if (init->stack[j].pos == init->stack[j].last) {
					init->stack[j].pos = range_pos[i - 1].orig_pos;
					if (i == 1) done = 1;
				} else {
					init->stack[j].pos++;
					break;
				}
			}
		} while (!done);

		init->ranges = ranges;
		return;
	}

	while (1) {
		if (type == val->type) {
			break;
		} else if (type->kind == C_TYPE_ARRAY) {
			if (type->array.length == 0 && !(type->attr & C_ATTR_FLEXIBLE)) {
				yy_error("excess elements in array initializer");
			}
			if (c_value_is_const_str(val)
			 && (type->array.type->kind == val->type->array.type->kind
			  || (val->type->array.type->size == 1
			   && (type->array.type->kind == C_TYPE_U8
			    || type->array.type->kind == C_TYPE_I8)))) {
				break;
			}
			type = type->array.type;
			if (type->kind != C_TYPE_ARRAY && type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) break;
		} else if (type->kind == C_TYPE_STRUCT) {
			c_field *f;

			if (type->record.num_fields == 0) {
				yy_error("excess elements in struct initializer");
			}
			if (init->stack[init->level].pos == type->record.num_fields) {
				if (init->level == 0) yy_error("excess elements in struct initializer");
				init->level--;
				type = init->stack[init->level].type;
				continue;
			}
			f = &type->record.fields[init->stack[init->level].pos];
			if (f->type->kind != C_TYPE_ARRAY && f->type->kind != C_TYPE_STRUCT && f->type->kind != C_TYPE_UNION) {
				if (!f->name) {
					init->stack[init->level].pos++;
					continue;
				}
				type = f->type;
				break;
			}
			type = f->type;
		} else if (type->kind == C_TYPE_UNION) {
			// TODO: select best type ???
			if (type->record.num_fields == 0) {
				yy_error("excess elements in union initializer");
			}
			type = type->record.fields[0].type;
			if (type->kind != C_TYPE_ARRAY && type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) break;
		} else if (type->kind == C_TYPE_VECTOR) {
			type = type->vec.type;
			break;
		} else {
			break;
		}

		if (type == val->type || c_compatible_types(type, val->type, 1, 0)) {
			break;
		}

		if (c_value_is_const_str(val)
		 && type->kind == C_TYPE_ARRAY
		 && (type->array.type->kind == val->type->array.type->kind
		  || (val->type->array.type->size == 1
		   && (type->array.type->kind == C_TYPE_U8
		    || type->array.type->kind == C_TYPE_I8)))) {
			break;
		}

		if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
		init->level++;
		init->stack[init->level].type = type;
		init->stack[init->level].pos = 0;
		init->stack[init->level].last = 0;
	}

	/* recalculate offset */
	offset = 0;
	for (i = 0; i <= init->level; i++) {
		const c_type *t = init->stack[i].type;
		if (t->kind == C_TYPE_ARRAY) {
			offset += t->array.type->size * init->stack[i].pos;
			if (t->attr & C_ATTR_FLEXIBLE) {
				last_array_type = t;
				last_array_offset = offset;
			}
		} else if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
			if (init->stack[i].pos < t->record.num_fields) {
				c_field *f = &t->record.fields[init->stack[i].pos];
				offset += f->offset;
				bit_field = f->bit_field;
			} else {
				IR_ASSERT(i == init->level);
			}
		} else if (t->kind == C_TYPE_VECTOR) {
			offset += t->vec.type->size * init->stack[i].pos;
		} else {
			IR_ASSERT(i == init->level);
		}
	}

	if (val->type != type) {
		if (c_value_is_const_str(val)
		 && type->kind == C_TYPE_ARRAY
		 && (type->array.type->kind == val->type->array.type->kind
		  || (val->type->array.type->size == 1
		   && (type->array.type->kind == C_TYPE_U8
		    || type->array.type->kind == C_TYPE_I8)))) {
			const void *str = c_value_str_addr(val);
			size_t len = c_value_str_size(val);

			if (len > type->array.length * type->array.type->size && !(type->attr & C_ATTR_FLEXIBLE)) {
				if (len - type->array.type->size == type->array.length * type->array.type->size) {
					len -= type->array.type->size;
				} else if (val->type->array.type->size == 1) {
					yy_error("initializer-string for array of \"char\" is too long");
				} else {
					yy_error("initializer-string for array is too long");
				}
			}

			new_size = init->size;
			if (type->attr & C_ATTR_FLEXIBLE) {
				if (obj->value.type == type) {
					new_size = len;
				} else if (obj->value.type->kind == C_TYPE_STRUCT
				 && obj->value.type->record.fields[obj->value.type->record.num_fields-1].type == type) {
					/* last element of struct */
					new_size = offset + len;
				} else {
					yy_error("initialization of flexible array member in a nested context");
				}
			} else if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
				/* element of flexible array */
				new_size = offset + obj->value.type->array.type->size;
			}
			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				if (new_size > init->size) {
					c_do_grow_flexible(rcc, obj, init->size, new_size);
					init->size = new_size;
				}
				memcpy((char*)obj->value.u.val.ptr + offset, str, len);
			} else {
				IR_ASSERT(obj->value.u.ref > 0);
				IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
				if (new_size > init->size) init->size = new_size;
				ir_memcpy(rcc,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
					c_create_str_sym(rcc, val),
					ir_const_size_t(rcc->active_ctx, len),
					c_attr2align(type->attr));
			}
			return;
		}
		c_do_check_cvt(rcc, type, val, -1);
	}

	new_size = init->size;
	if (obj->value.type->kind == C_TYPE_ARRAY && (obj->value.type->attr & C_ATTR_FLEXIBLE)) {
		if (last_array_type && last_array_type != obj->value.type) {
			yy_error("initialization of flexible array member in a nested context");
		}
		if (offset + type->size > obj->value.type->size) {
			size_t len =
				(offset + type->size + obj->value.type->array.type->size - 1) / obj->value.type->array.type->size;
			if (obj->value.type->array.type->size * len > init->size) {
				new_size = obj->value.type->array.type->size * len;
			}
		}
	} else if (last_array_type) {
		if (obj->value.type->kind != C_TYPE_STRUCT
		 || obj->value.type->record.fields[obj->value.type->record.num_fields-1].type != last_array_type) {
			yy_error("initialization of flexible array member in a nested context");
		}
		/* last element of struct */
		if (last_array_offset + last_array_type->array.type->size > init->size) {
			new_size = last_array_offset + last_array_type->array.type->size;
		}
	} else {
		IR_ASSERT(offset + type->size <= obj->value.type->size || C_IS_BIT_FIELD(bit_field));
	}
	if (c_value_is_const(&obj->value) || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
		if (type->size == sizeof(void*)) {
			/* Using designators it's possible to initialie the same element multiple times.
			 * We have to remove relocations added previously. */
			c_linker_del_reloc(rcc, obj, offset);
		}
		if (!c_value_is_const(val)
		 && !c_linker_fix_reloc(rcc, obj, offset, val)
		 && !(IR_IS_CONST_REF(val->u.ref) && val->type->kind == C_TYPE_VECTOR)) {
			yy_error("initializer element is not constant");
		}
		IR_ASSERT(/*obj->value.u.type == IR_ADDR &&*/ obj->value.u.val.ptr);
		if (new_size > init->size) {
			c_do_grow_flexible(rcc, obj, init->size, new_size);
			init->size = new_size;
		}
		if (!C_IS_BIT_FIELD(bit_field)) {
			c_do_init(rcc, (char*)obj->value.u.val.ptr + offset, val);
		} else {
			uint32_t first_bit = C_BIT_FIELD_START(bit_field);
			uint32_t bits = C_BIT_FIELD_SIZE(bit_field);
			uint64_t mask;
			uint64_t data[2];
			uint64_t bits_val = val->u.val.u64;

			memcpy(data, (char*)obj->value.u.val.ptr + offset, (first_bit + bits + 7) / 8);
			// TODO: support for big endian byte order ???
			if (first_bit + bits <= 64) {
				if (first_bit) {
					bits_val <<= first_bit;
				}
				mask = (((1ULL<<bits)-1)<<first_bit);
				data[0] &= ~mask;
				data[0] |= bits_val & mask;
			} else {
				mask = (((1ULL<<(64-first_bit))-1)<<first_bit);
				data[0] &= ~mask;
				data[0] |= (bits_val << first_bit) & mask;
				mask = (1ULL<<(bits-64+first_bit))-1;
				data[1] &= ~mask;
				data[1] |= (bits_val >> (64 - first_bit)) & mask;
			}
			memcpy((char*)obj->value.u.val.ptr + offset, data, (first_bit + bits + 7) / 8);
		}
	} else {
		IR_ASSERT(obj->value.u.ref > 0);
		if (new_size > init->size) init->size = new_size;
		if (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION) {
			IR_ASSERT(!C_IS_BIT_FIELD(bit_field));
			IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			if (type->size) {
				ir_memcpy(rcc,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
					c_value_ref(rcc, val),
					ir_const_size_t(rcc->active_ctx, type->size),
					c_attr2align(type->attr));
			}
		} else if (rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_VAR) {
			IR_ASSERT(!C_IS_BIT_FIELD(bit_field));

			if (init->stack[init->level].type->kind == C_TYPE_VECTOR) {
				const c_type *t = init->stack[init->level].type;
				ir_type vt = c_type2ir(rcc, t);
				ir_ref ref;

				offset -= t->vec.type->size * init->stack[init->level].pos;
				IR_ASSERT(offset == 0);
				ref = ir_VLOAD(vt, obj->value.u.ref);
				ref = ir_REPLACE(vt, ref, ir_const_u8(rcc->active_ctx, init->stack[init->level].pos),
						c_value_ref(rcc, val));
				ir_VSTORE(obj->value.u.ref, ref);
			} else {
				ir_VSTORE(obj->value.u.ref, c_value_ref(rcc, val));
			}
		} else if (c_value_is_reg(&obj->value)) {
			IR_ASSERT(!C_IS_BIT_FIELD(bit_field));
			ir_RSTORE(obj->value.u.ref, c_value_ref(rcc, val));
		} else {
			IR_ASSERT(rcc->active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			if (!C_IS_BIT_FIELD(bit_field)) {
				if (init->stack[init->level].type->kind == C_TYPE_VECTOR) {
					const c_type *t = init->stack[init->level].type;
					ir_type vt = c_type2ir(rcc, t);
					ir_ref ref;

					offset -= t->vec.type->size * init->stack[init->level].pos;
					ref = ir_LOAD(vt, ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)));
					ref = ir_REPLACE(vt, ref, ir_const_u8(rcc->active_ctx, init->stack[init->level].pos),
							c_value_ref(rcc, val));
					ir_STORE(
						ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
						ref);
				} else {
					ir_STORE(
						ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
						c_value_ref(rcc, val));
				}
			} else if (!C_IS_BIT_FIELD_PACKED(bit_field)) {
				c_do_store_bit_field(rcc,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
					C_BIT_FIELD_START(bit_field), C_BIT_FIELD_SIZE(bit_field), val);
			} else {
				c_do_store_bit_field_packed(rcc,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(rcc->active_ctx, offset)),
					C_BIT_FIELD_START(bit_field), C_BIT_FIELD_SIZE(bit_field), val);
			}
		}
	}
}

void c_do_init_nested(rcc_ctx *rcc, c_sym *obj, c_init *init, bool b)
{
	const c_type *type = init->stack[init->level].type;

	if (!b) {
		if (type->kind == C_TYPE_ARRAY) {
			if (type->array.length == 0 && !(type->attr & C_ATTR_FLEXIBLE)) {
				yy_error("excess elements in array initializer");
			}
			type = type->array.type;
		} else if (type->kind == C_TYPE_STRUCT) {
			c_field *field;

			if (type->record.num_fields == 0) {
				yy_error("excess elements in struct initializer");
			}
			field = &type->record.fields[init->stack[init->level].pos];
			type = field->type;
		} else if (type->kind == C_TYPE_UNION) {
			// TODO: select best type ???
			c_field *field;

			if (type->record.num_fields == 0) {
				yy_error("excess elements in union initializer");
			}
			field = &type->record.fields[0];
			type = field->type;
		}
	}

	if (type->kind != C_TYPE_ARRAY
	 && type->kind != C_TYPE_STRUCT
	 && type->kind != C_TYPE_UNION
	 && type->kind != C_TYPE_VECTOR) {
		yy_warning("braces around scalar initializer");
	} else if (!b) {
		init->level++;
		init->stack[init->level].type = type;
		init->stack[init->level].pos = 0;
		init->stack[init->level].last = 0;
	}
}

void c_do_init_end(rcc_ctx *rcc, c_sym *obj, c_init *init)
{
	if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
		if (obj->value.type->kind == C_TYPE_ARRAY) {
			/* Convert "flexible" array to regular */
			c_type *type = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));

			*type = *obj->value.type;
			if (rcc->active_scope) type->flags |= C_TYPE_GLOBAL;
			type->array.length = init->size / type->array.type->size;
			type->size = init->size;
			type->attr &= ~C_ATTR_FLEXIBLE;
			obj->value.type = type;
		}
		if (!c_value_is_const(&obj->value)
		 && (!c_value_is_ref(&obj->value) || !IR_IS_CONST_REF(obj->value.u.ref))) {
			c_do_init_patch_flexible_alloca(rcc, obj->value.u.ref, init->size);
		}
	}
	if (obj->tmp_data) {
		c_do_end_flexible(rcc, obj, init->size);
	}
}

void c_do_init_expr_start(rcc_ctx *rcc, c_sym *obj, const c_type *type)
{
	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(type->kind), yy_sym2str(rcc, type->tag));
	}
	memset(obj, 0, sizeof(c_sym));
	obj->kind = C_SYM_VAR;
	if (rcc->active_func && !rcc->c_static_data) {
		ir_ref size = ir_const_size_t(rcc->active_ctx, type->size);
		ir_ref addr = c_do_alloca(rcc, type->size, c_attr2align(type->attr), 0);
		if (type->kind != C_TYPE_VECTOR) {
			ir_memzero(rcc, addr, size, c_attr2align(type->attr));
		}
		c_value_set_rval(&obj->value, type, IR_ADDR, addr);
	} else {
		c_dcl d = {.type = type, .flags = C_DCL_DEFINITION, .attr = 0};
		c_name sym_name = c_create_static_var(rcc, YY_UNDEF, &d);
		ir_ref ref;
		void *addr;

		if (c_is_flexible(type)) {
			obj->tmp_data = 1;
			addr = ir_mem_calloc(1, type->size);
			if (!addr) yy_error("not enough memory to allocate data");
		} else {
			addr = c_linker_allocate_data(rcc, 0, type->size, c_attr2align(type->attr), type->kind == C_TYPE_ARRAY);
		}

		rcc->yy_hash.data[sym_name].sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
		rcc->yy_hash.data[sym_name].sym->value.u.val.ptr = addr;
		ref = ir_const_sym(rcc->active_ctx, IR_EXT_STR(sym_name));
		if (type->kind == C_TYPE_ARRAY) {
			c_value_set_rval(&obj->value, type, c_type2ir(rcc, type), ref);
		} else {
			c_value_set_lval(&obj->value, type, c_type2ir(rcc, type), ref);
		}
		obj->value.u.val.ptr = addr; /* keep address in addition to ref */
	}
}

void c_do_init_expr_end(rcc_ctx *rcc, c_value *v, c_sym *obj, size_t size)
{
	if (c_value_is_const(&obj->value)) {
		c_value_set_const(v, obj->value.type, c_type2ir(rcc, obj->value.type), obj->value.u.val);
#if 0
	} else if ((!rcc->active_scope || rcc->c_static_data)
	 && obj->value.type->kind != C_TYPE_ARRAY
	 && obj->value.type->kind != C_TYPE_STRUCT
	 && obj->value.type->kind != C_TYPE_UNION) {
		ir_type t = c_type2ir(rcc, obj->value.type);
		ir_val val;

		val.u64 = 0;
		switch (t) {
			case IR_BOOL:	val.b    = *(bool*)      obj->value.u.val.ptr; break;
			case IR_U8:     val.u8   = *(uint8_t*)   obj->value.u.val.ptr; break;
			case IR_U16:    val.u16  = *(uint16_t*)  obj->value.u.val.ptr; break;
			case IR_U32:    val.u32  = *(uint32_t*)  obj->value.u.val.ptr; break;
			case IR_U64:    val.u64  = *(uint64_t*)  obj->value.u.val.ptr; break;
			case IR_ADDR:   val.addr = *(uintptr_t*) obj->value.u.val.ptr; break;
			case IR_CHAR:   val.i64  = *(char*)      obj->value.u.val.ptr; break;
			case IR_I8:     val.i64  = *(int8_t*)    obj->value.u.val.ptr; break;
			case IR_I16:    val.i64  = *(int16_t*)   obj->value.u.val.ptr; break;
			case IR_I32:    val.i64  = *(int32_t*)   obj->value.u.val.ptr; break;
			case IR_I64:    val.i64  = *(int64_t*)   obj->value.u.val.ptr; break;
			case IR_DOUBLE: val.d    = *(double*)    obj->value.u.val.ptr; break;
			case IR_FLOAT:  val.f    = *(float*)     obj->value.u.val.ptr; break;
			default: IR_ASSERT(0);
		}
		c_value_set_const(v, obj->value.type, t, val);
#endif
	} else if (obj->value.type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, obj->value.type, c_type2ir(rcc, obj->value.type), obj->value.u.ref);
		if (IR_IS_CONST_REF(obj->value.u.ref)) v->u.val.ptr = obj->value.u.val.ptr;
	} else {
		c_value_set_rval(v, obj->value.type, c_type2ir(rcc, obj->value.type), obj->value.u.ref);
		if (IR_IS_CONST_REF(obj->value.u.ref)) v->u.val.ptr = obj->value.u.val.ptr;
	}
}

void c_do_generic_start(rcc_ctx *rcc, c_generic *g)
{
	memset(g, 0, sizeof(c_generic));
	g->old_control = c_do_nocode(rcc);
	g->last_control = rcc->active_ctx->control;
}

void c_do_generic_type(rcc_ctx *rcc, c_generic *g, const c_type *type, bool is_type)
{
	if (!is_type) {
		if (type->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) {
			/* remove top-level qualifiers */
			c_type *t = ir_arena_alloc(&rcc->c_arena, sizeof(c_type));
			*t = *type;
			if (rcc->active_scope) t->flags &= ~C_TYPE_GLOBAL;
			t->attr &= ~(C_ATTR_CONST|C_ATTR_VOLATILE);
			type = t;
		}
		if (type->kind == C_TYPE_FUNC) {
			type = c_create_pointer_type(rcc, type);
		}
	}
	g->type = type;
}

void c_do_generic_case(rcc_ctx *rcc, c_generic *g, const c_type *type, c_value *v)
{
	if (c_compatible_types(g->type, type, 0, 0)) {
		if (c_value_is_set(&g->matched_value)) yy_error("duplicate matched type case in \"_Generic\"");
		g->matched_value = *v;
		g->matched_control_start = g->last_control;
		g->matched_control_end = rcc->active_ctx->control;
	}
	g->last_control = rcc->active_ctx->control;
}

void c_do_generic_default(rcc_ctx *rcc, c_generic *g, c_value *v)
{
	if (c_value_is_set(&g->default_value)) yy_error("duplicate \"default\" case in \"_Generic\"");
	g->default_value = *v;
	g->default_control_start = g->last_control;
	g->default_control_end = rcc->active_ctx->control;
	g->last_control = rcc->active_ctx->control;
}

void c_do_generic_end(rcc_ctx *rcc, c_value *res, c_generic *g)
{
	if (!c_value_is_set(&g->matched_value)) {
		if (!c_value_is_set(&g->default_value)) yy_error("no matched type case in \"_Generic\"");
		g->matched_value = g->default_value;
		g->matched_control_start = g->default_control_start;
		g->matched_control_end = g->default_control_end;
	}
	*res = g->matched_value;
	if (g->matched_control_start != g->matched_control_end) {
		ir_ref ref = rcc->active_ctx->control;

		if (ref != g->matched_control_end) {
			while (rcc->active_ctx->ir_base[ref].op1 != g->matched_control_end) {
				ref = rcc->active_ctx->ir_base[ref].op1;
				IR_ASSERT(ref);
			}
			/* cut control from unreachable block */
			rcc->active_ctx->ir_base[ref].op1 = g->matched_control_start;
		} else {
			/* remove contol tail from unreachable block */
			rcc->active_ctx->control = g->matched_control_start;
		}
		ref = g->matched_control_end;
		while (rcc->active_ctx->ir_base[ref].op1 != g->matched_control_start) {
			ref = rcc->active_ctx->ir_base[ref].op1;
			IR_ASSERT(ref);
		}
		rcc->active_ctx->ir_base[ref].op1 = g->old_control;
		g->old_control = g->matched_control_end;
	}
	c_do_end_nocode(rcc, g->old_control);
}

static void c_check_incomplete_vmt(rcc_ctx *rcc, const c_type *t)
{
	while (1) {
		if (t->kind == C_TYPE_ARRAY || t->kind == C_TYPE_POINTER) {
			t = t->pointer.type;
		} else {
			IR_ASSERT(0);
			return;
		}
		if (!(t->attr & (C_ATTR_VLA|C_ATTR_VMT))) return;
		if (t->attr & C_ATTR_VLA) {
			c_value val;

			IR_ASSERT(t->kind == C_TYPE_ARRAY);
			if (!t->array.length) yy_error("[*] not allowed in other than function prototype scope");
			if (t->array.vla_tokens) {
				parse_vla_param_again(rcc, t->array.vla_tokens, &val);
				c_value_ref(rcc, &val);
				if (val.type->kind != c_type_size_t.kind) {
					c_do_cvt(rcc, &c_type_size_t, IR_SIZE_T, &val);
				}
				((c_type*)t)->array.length = val.u.ref;
			}
		}
	}
}

void c_do_func_start(rcc_ctx *rcc, c_name name, c_dcl *d, c_scope *scope)
{
	c_sym *func;
	const c_type *type = d->type;
	const c_type *proto_type = NULL;
	uint32_t flags;
	uint32_t i, j = 0;

	d->flags |= C_DCL_DEFINITION;
	func = c_declare(rcc, name, d);
	IR_ASSERT(func);
	rcc->active_func = func;
	rcc->active_func_name = name;
	rcc->c_prologue_end = IR_UNUSED;
	rcc->c_last_expect_ref = IR_UNUSED;
	rcc->c_computed_goto = IR_UNUSED;
	rcc->c_computed_goto_targets = IR_UNUSED;
	rcc->c_last_call_func_type = NULL;

	c_push_scope(rcc, scope);

	if ((type->attr & C_ATTR_OLD_FUNC) && !(func->value.type->attr & C_ATTR_OLD_FUNC)) {
		proto_type = func->value.type;
	}

	flags = 0;
	if (d->flags & C_DCL_STATIC) {
		flags |= IR_STATIC;
	}
	if (type->attr & C_ATTR_VARIADIC) {
		flags |= IR_VARARG_FUNC;
	}

	flags |= c_type_call_conv(rcc, type);
	rcc_ir_init(rcc, flags);

	if (type->func.ret_type->kind == C_TYPE_STRUCT || type->func.ret_type->kind == C_TYPE_UNION) {
		ir_type types[MAX_ABI_TYPES];
		int n = c_abi_lower_struct_ret(type->func.ret_type, types);

		if (n == 1) {
			rcc->active_ctx->ret_type = types[0];
		} else {
			IR_ASSERT(n == 0);
			rcc->active_ctx->ret_type = IR_ADDR;
			j = 1;
		}
	} else {
		rcc->active_ctx->ret_type = c_type2ir(rcc, type->func.ret_type);
	}

	rcc->active_func_scope = rcc->active_scope;

	ir_START();
	if ((type->func.ret_type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, type->func.ret_type)) {
		yy_error("return type is an incomplete type");
	}
	if (j) {
		ir_param(rcc->active_ctx, IR_ADDR, 1, "$ret", 1);
	}
	for (i = 0; i < type->func.num_params; i++) {
		c_param *p = &type->func.params[i];
		const c_type *t = p->type;

		if (!t) {
			yy_warning_fmt("type of \"%s\" defaults to \"int\"", yy_sym2str(rcc, p->name));
			t = p->type = &c_type_i32;
		}
		if ((t->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(rcc, t)) {
			yy_error_fmt("parameter %d has incomplete type", i + 1);
		}
		if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
			ir_type types[MAX_ABI_TYPES];
			int n = c_abi_lower_struct_arg(t, types);

			if (n == 1) {
				ir_param_ex(rcc->active_ctx, types[0], 1, IR_EXT_STR(p->name), i + j + 1);
			} else {
				IR_ASSERT(n == 0);
				ir_param_ex(rcc->active_ctx, IR_ADDR, 1, IR_EXT_STR(p->name), i + j + 1);
				if (!rcc->active_ctx->value_params) {
					rcc->active_ctx->value_params = ir_mem_calloc(type->func.num_params + j, sizeof(ir_value_param));
					if (!rcc->active_ctx->value_params) yy_error("out of memory");
				}
				rcc->active_ctx->value_params[i + j].size = t->size;
				rcc->active_ctx->value_params[i + j].align = c_attr2align(t->attr);
			}
		} else {
			ir_param_ex(rcc->active_ctx, c_type2ir(rcc, t), 1, IR_EXT_STR(p->name), i + j + 1);
		}
	}
	for (i = 0; i < type->func.num_params; i++) {
		c_param *p = &type->func.params[i];
		const c_type *t = p->type;
		ir_ref param_ref = i + j + 2;

		if (t->attr & C_ATTR_VLA) {
			c_value val;

			IR_ASSERT(t->kind == C_TYPE_POINTER);
			if (!t->array.length) yy_error("[*] not allowed in other than function prototype scope");
			((c_type*)t)->attr &= ~C_ATTR_VLA;
			if (t->array.vla_tokens) {
				parse_vla_param_again(rcc, t->array.vla_tokens, &val);
			}
		}

		if (t->attr & C_ATTR_VMT) {
			c_check_incomplete_vmt(rcc, t);
		}

		if (p->name) {
			c_name name = p->name;
			c_dcl dcl = {.flags = C_DCL_PARAM, .attr = 0, .type = t};
			c_sym *obj;

			obj = c_declare(rcc, name, &dcl);
			IR_ASSERT(obj &&  obj->kind == C_SYM_VAR && c_value_is_ref(&obj->value));

			if (proto_type && i < proto_type->func.num_params) {
				const c_type *proto_t = proto_type->func.params[i].type;

				if (!c_compatible_types(t, proto_t, 1, 0)) {
					c_value val;
					ir_type tt = c_type2ir(rcc, proto_t);

					rcc->active_ctx->ir_base[param_ref].type = tt;
					c_value_set_rval(&val, proto_t, tt, param_ref);
					c_do_check_cvt(rcc, t, &val, -1);
					param_ref = val.u.ref;
				}
			}

			if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
				ir_type types[MAX_ABI_TYPES];
				int n = c_abi_lower_struct_arg(t, types);

				if (n == 1) {
					if (t->size) {
						if (obj->value.type->attr & C_ATTR_VOLATILE) {
							ir_STORE_v(obj->value.u.ref, param_ref);
						} else {
							ir_STORE(obj->value.u.ref, param_ref);
						}
					}
				} else {
					IR_ASSERT(n == 0);
					c_value_set_lval(&obj->value, t, IR_ADDR, param_ref);
				}
			} else {
				if (obj->value.type->attr & C_ATTR_VOLATILE) {
					ir_VSTORE_v(obj->value.u.ref, param_ref);
				} else {
					ir_VSTORE(obj->value.u.ref, param_ref);
				}
			}
		} else {
			yy_warning("omitting the parameter name in a function definition");
		}
	}

	ir_BLOCK_BEGIN();
	rcc->c_prologue_end = rcc->active_ctx->control;
}

static bool c_is_dead_end(rcc_ctx *rcc, ir_insn *insn)
{
	while (insn->op == IR_BEGIN) {
		if (!insn->op1) return 1;
		insn = &rcc->active_ctx->ir_base[insn->op1];
		if (insn->op != IR_END) {
			return 0;
		}
		insn = &rcc->active_ctx->ir_base[insn->op1];
	}
	if (insn->op == IR_MERGE) {
		ir_ref input, *p, n;

		n = insn->inputs_count;
		for (p = insn->ops + 1; n > 0; p++, n--) {
			input = *p;
			insn = &rcc->active_ctx->ir_base[input];
			if (insn->op != IR_END) {
				return 0;
			}
			insn = &rcc->active_ctx->ir_base[insn->op1];
			if (!c_is_dead_end(rcc, insn)) {
				return 0;
			}
		}
	} else {
		return 0;
	}
	return 1;
}

void c_do_func_end(rcc_ctx *rcc, c_name name, c_dcl *d, c_scope *scope)
{
	ir_ctx *ctx;

	c_pop_scope(rcc, scope);

	ctx = rcc->active_ctx;
	if (ctx->control) {
		if (ctx->control == ctx->insns_count - 1
		 && ctx->ir_base[ctx->control].op == IR_BEGIN
		 && !ctx->ir_base[ctx->control].op1) {
			ctx->insns_count--;
			ctx->control = IR_UNUSED;
		} else if (ctx->ret_type) {
			ir_val val;

			if (!c_is_dead_end(rcc, &ctx->ir_base[ctx->control])) {
				yy_warning("control reaches end of non-void function");
			}
			val.u64 = 0;
			ir_RETURN(ir_const(ctx, val, ctx->ret_type));
		} else {
			ir_RETURN(IR_UNUSED);
		}
	}

	if (rcc->c_computed_goto) {
		ir_ref ref = ir_PHI_list(rcc->c_computed_goto);
		ir_ref goto_ref = ir_IGOTO(ref);

		if (rcc->c_computed_goto_targets) {
			ir_ref ref = rcc->c_computed_goto_targets;
			do {
				ir_insn *insn = &rcc->active_ctx->ir_base[ref];
				IR_ASSERT(insn->op == IR_BEGIN);
				ref = insn->op3;
				insn->op1 = goto_ref;
				insn->op3 = IR_UNUSED;
			} while (ref);
			rcc->c_computed_goto_targets = IR_UNUSED;
		}
	} else if (rcc->c_computed_goto_targets) {
		ir_ref ref = rcc->c_computed_goto_targets;
		do {
			ir_insn *insn = &rcc->active_ctx->ir_base[ref];
			ir_ref i, n;
			ir_insn *merge;

			IR_ASSERT(insn->op == IR_BEGIN);
			ref = insn->op3;
			insn->op3 = IR_UNUSED;
			IR_ASSERT(IR_IS_CONST_REF(insn->op2));

			/* re-link BEGIN node corresponding to label address */
			merge = &rcc->active_ctx->ir_base[insn->op1];
			IR_ASSERT(merge->op == IR_MERGE);
			insn->op1 = merge->op1;
			n = merge->inputs_count;
			for (i = 1; i < n; i++) {
				ir_ref input = ir_insn_op(merge, i + 1);
				ir_insn_set_op(merge, i, input);
			}
			ir_insn_set_op(merge, i, IR_UNUSED);
			merge->inputs_count = i - 1;
		} while (ref);
		rcc->c_computed_goto_targets = IR_UNUSED;
	}

	rcc_ir_compile(rcc, name, d, rcc->active_func);

	rcc->active_func = NULL; // TODO: nested functions ???
	rcc->active_func_name = 0; // TODO: nested functions ???
	rcc->active_func_scope = NULL;
}

yy_sym c_get_current_func_name(rcc_ctx *rcc)
{
	return rcc->active_func_name;
}

void c_do_asm_operand_constraint(rcc_ctx *rcc, c_asm *a, bool out, int n, c_name name, c_value *constraint)
{
	c_asm_operand *op = &a->ops[n];
	const char *p, *s, *reg_name;
	int num;
	int8_t reg;

	if (name && n) {
		int i;

		for (i = 0; i < n; i++) {
			if (a->ops[i].id == name && a->ops[i].flags != C_ASM_OP_LABEL) {
				yy_error_fmt("duplicate \"__asm__\" operand name \"%s\"", yy_sym2str(rcc, name));
			}
		}
	}

	op->flags = out ? C_ASM_OP_OUTPUT : C_ASM_OP_INPUT;
	op->id = name;
	op->constraint_str = p = s = constraint->u.val.ptr;
	op->constraint_len = constraint->u.ref - 1;

	while (*p == ' ') p++;
	if (out) {
		if (*p != '=' && *p != '+') {
			yy_error_fmt("invalid asm output constraint \"%s\" (lacks \"=\")", s);
		}
		if (*p == '+') op->flags |= C_ASM_OP_INOUT;
		p++;
	} else {
		if (*p == '=' || *p == '+') {
			yy_error_fmt("invalid asm input constraint \"%s\" (contains \"%c\")", s, *p);
		}
	}

next:
	while (*p == ' ') p++;
	while (*p) {
		switch (*p) {
			case '&':
				if (!out) goto error;
				op->flags |= C_ASM_OP_CLOBBERED;
				p++;
				goto next;
			case '%':
			case '-':
				// TODO : ???
				p++;
				goto next;
			case 'r':
				op->flags |= C_ASM_OP_REG_INT;
				break;
			case '{':
				p++;
				reg_name = p;
				while (*p) {
					if (!*p) goto error;
					if (*p == '}') break;
					p++;
				}
				reg = c_parse_reg_name(rcc, reg_name, p, 0);
				if (reg == IR_REG_NONE) goto error;
				if (reg >= IR_REG_GP_FIRST && reg <= IR_REG_GP_LAST) {
					op->flags |= C_ASM_OP_REG_INT;
					// TODO: remember register
				} else if (reg >= IR_REG_GP_FIRST && reg <= IR_REG_FP_LAST) {
					op->flags |= C_ASM_OP_REG_FP;
					// TODO: remember register
				} else {
					IR_ASSERT(0);
				}
				break;
			case 'm':
			case 'o':
			case 'V':
			case '<':
			case '>':
				op->flags |= C_ASM_OP_MEM;
				break;
			case 'i': /* immediate integer operand */
			case 'n': /* immediate integer operand (not a word wide) */
			case 's': /* immediate integer operand (not an integer) */
				op->flags |= C_ASM_OP_IMM_INT;
				break;
			case 'E': /* immediate floating operand (double) */
			case 'F': /* immediate floating operand (double or vector) */
				op->flags |= C_ASM_OP_IMM_FP;
				break;
			case 'g': /* any integer */
				if (out) {
					op->flags |= C_ASM_OP_REG_INT | C_ASM_OP_MEM;
				} else {
					op->flags |= C_ASM_OP_REG_INT | C_ASM_OP_IMM_INT | C_ASM_OP_MEM;
				}
				break;
			case 'X': /* any */
				if (out) {
					op->flags |= C_ASM_OP_REG_INT | C_ASM_OP_REG_FP | C_ASM_OP_MEM;
				} else {
					op->flags |= C_ASM_OP_ANY;
				}
				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				num = *p - '0';
				if (*(p+1) >= '0' && *(p+1) <= '9') {
					p++;
					num = num * 10 + *p - '0';
				}
				if (num >= n) goto error;
				// TODO: ???
				op->flags |= a->ops[num].flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP);
				break;
#if defined(IR_TARGET_AARCH64)
//			case 'k': /* stack pointer */
			case 'w': /* floating point register or SVE */
			case 'x': /* floating point register or SVE (restricted to 0..15) */
			case 'y': /* floating point register or SVE (restricted to 0..7) */
				op->flags |= C_ASM_OP_REG_FP;
				break;
			case 'I': /* integer constant that is valid as an immediate operand in an ADD instruction */
			case 'J': /* integer constant that is valid as an immediate operand in a SUB instruction (once negated) */
			case 'K': /* integer constant that can be used with a 32-bit logical instruction */
			case 'L': /* integer constant that can be used with a 64-bit logical instruction */
			case 'M': /* integer constant that is valid as an immediate operand in a 32-bit MOV pseudo instruction */
			case 'N': /* integer constant that is valid as an immediate operand in a 64-bit MOV pseudo instruction */
			case 'Z': /* integer constant zero */
				op->flags |= C_ASM_OP_IMM_INT;
				break;
//			case 'S' /* an absolute symbolic address or a label reference */
//			case 'Y' /* floating point constant zero */
//			case 'Q' /* a memory address which uses a single base register with no offset */
//			case 'U' /* Upl, Upa, Ush, Ump */
#elif defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
//			case 'A': /* eax:rdx */
			case 'a': /* rax */
			case 'b': /* ebx */
			case 'c': /* rcx */
			case 'd': /* rdx */
			case 'S': /* rsi */
			case 'D': /* rdi */
			case 'q': /* any register accessible as rl (al, bl, ...) */
//			case 'Q': /* any register accessible as rh (ah, bh, ...) */
			case 'R': /* legacy register */
			case 'U': /* call clobbered integer register */
				op->flags |= C_ASM_OP_REG_INT;
				break;
			case 'f': /* 80387 floating point stack register */
			case 't': /* 80387 floating point top stack register */
			case 'u': /* second from top of 80387 floating-point stack (%st(1)) */
			case 'y': /* MMX register */
			case 'x': /* SSE register */
			case 'v': /* EVEX encodable SSE register (%xmm0-%xmm31) */
				op->flags |= C_ASM_OP_REG_FP;
				break;
			case 'Y': /* "Yz" - first SSE register (%xmm0). */
				if (*(p+1) != 'z') goto error;
				p++;
				op->flags |= C_ASM_OP_REG_FP;
				break;
			case 'I': /* integer constant in the range 0..31, for 32-bit shifts */
			case 'J': /* integer constant in the range 0 … 63, for 64-bit shifts */
			case 'K': /* signed 8-bit integer constant */
			case 'L': /* 0xFF or 0xFFFF, for andsi as a zero-extending move */
			case 'M': /* 0, 1, 2, or 3 (shifts for the lea instruction) */
			case 'N': /* unsigned 8-bit integer constant (for in and out instructions) */
			case 'e': /* 32-bit signed integer constant, or a symbolic reference known to fit that range (for immediate operands in sign-extending x86-64 instructions). */
			case 'Z': /* 32-bit unsigned integer constant, or a symbolic reference known to fit that range (for immediate operands in zero-extending x86-64 instructions). */
//			case 'W': /* We, Wz, Wd, Ws */
//			case 'T': /* Tv, Tz */
				op->flags |= C_ASM_OP_IMM_INT;
				break;
			case 'G': /* 80387 floating point constant */
			case 'C': /* SSE constant zero operand */
				op->flags |= C_ASM_OP_IMM_FP;
				break;
#endif
			default:
				goto error;
		}
		p++;
		while (*p == ' ') p++;
		if (*p == ',') p++;
	}

	if (out) {
		if (op->flags & (C_ASM_OP_IMM_INT|C_ASM_OP_IMM_FP)) {
			goto error;
		} else if (op->flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP)) {
			a->flags |= C_ASM_HAS_OUT_REGS;
		}
	}
	return;

error:
	yy_error_fmt("invalid %s asm constraint \"%s\"", out ? "output" : "input", s);
}

void c_do_asm_operand_val(rcc_ctx *rcc, c_asm *a, bool out, int n, c_value *val)
{
	if (out && !c_value_is_lval(val)) {
		yy_error("lvalue required as asm output operand");
	} else if (val->type->kind == C_TYPE_VOID) {
		yy_error("invalid use of void expresion");
	}
	a->ops[n].val = *val;
}

void c_do_asm_operand_label(rcc_ctx *rcc, c_asm *a, int n, c_name label)
{
	a->flags |= C_ASM_HAS_LABELS;
	a->ops[n].flags = C_ASM_OP_LABEL;
	a->ops[n].id = label;
}

void c_do_asm_clobbers(rcc_ctx *rcc, c_asm *a, c_value *val)
{
	const char *str = val->u.val.ptr;

	if (strcmp(str, "cc") == 0) {
		a->flags |= C_ASM_CLOBBERS_CC;
	} else if (strcmp(str, "memory") == 0) {
		a->flags |= C_ASM_CLOBBERS_MEMORY;
	} else if (strcmp(str, "redzone") == 0) {
		a->flags |= C_ASM_CLOBBERS_REDZONE;
	} else {
		int8_t reg = c_parse_reg_name(rcc, val->u.val.ptr, NULL, 0);
		ir_regset regset= a->clobbers;

		IR_REGSET_INCL(regset, reg);
		a->clobbers = regset;
	}
}

static void c_do_asm_store_outs(rcc_ctx *rcc, c_asm *a, int n, int ref)
{
	c_asm_operand *op;
	c_value val;
	int i, n_out;

	n_out = 0;
	op = a->ops;
	for (i = 0; i < n; op++, i++) {
		if (op->flags & C_ASM_OP_OUTPUT) {
			if (op->flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP)) {
				c_value_set_rval(&val, op->val.type, op->val.u.type, ref + n_out);
				c_do_store(rcc, &op->val, &val);
				if (!n_out) ref += ir_insn_inputs_to_len(rcc->active_ctx->ir_base[ref].inputs_count) - 1;
				n_out++;
			}
		} else {
			break;
		}
	}
}

void c_do_asm(rcc_ctx *rcc, c_value *asm_str, c_asm *a, int n)
{
	c_asm_operand *op;
	ir_type type = IR_VOID;
	ir_ref ref;
	const char *str;
	size_t len;
	int i, n_in, n_out;
	ir_ref in[C_MAX_ASM_OPERANDS];

	if (a->flags & C_ASM_GOTO) {
		if (!(a->flags & C_ASM_HAS_LABELS)) yy_error("extended GCC asm with \"goto\", but without labels");
		a->flags |= C_ASM_VOLATILE;
	} else {
		if (a->flags & C_ASM_HAS_LABELS) yy_error("extended GCC asm with labels, but without \"goto\"");
	}

	n_in = n_out = 0;
	op = a->ops;
	for (i = 0; i < n; op++, i++) {
		if (op->flags & C_ASM_OP_OUTPUT) {
			if (op->flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP)) {
				if (!n_out) {
					type = op->val.u.type;
				}
				n_out++;
				if (op->flags & C_ASM_OP_INOUT) {
					c_value val = op->val;
					in[i] = c_value_ref(rcc, &val);
					n_in++;
				}
			} else {
				if (op->flags & C_ASM_OP_MEM) {
					if (c_value_is_lval(&op->val)) {
						in[i] = op->val.u.ref;
					} else {
						yy_error_fmt("\"__asm__\" memory input %d is not directly addressable", i);
					}
				} else {
					IR_ASSERT(0);
				}
				n_in++;
			}
		} else if (op->flags & C_ASM_OP_INPUT) {
			if (op->flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP|C_ASM_OP_IMM_INT|C_ASM_OP_IMM_FP)) {
				in[i] = c_value_ref(rcc, &op->val);
			} else if (op->flags & C_ASM_OP_MEM) {
				if (c_value_is_lval(&op->val)) {
					in[i] = op->val.u.ref;
				} else {
					yy_error_fmt("\"__asm__\" memory input %d is not directly addressable", i);
				}
			} else {
				IR_ASSERT(0);
			}
			n_in++;
		} else if (op->flags & C_ASM_OP_LABEL) {
			// TODO: n_in++;
		}
	}

	ref = ir_emit_N(rcc->active_ctx, IR_OPT(IR_ASM, type), 2 + n_in + ((n || a->clobbers) ? 1 : 0));
	ir_set_op(rcc->active_ctx, ref, 1, rcc->active_ctx->control);
	ir_set_op(rcc->active_ctx, ref, 2,
		ir_const_str(rcc->active_ctx, ir_stringl(rcc->active_ctx, asm_str->u.val.ptr, asm_str->u.ref - 1)));
	rcc->active_ctx->control = ref;

	if (n || a->clobbers) {
		yy_dyn_str args;
		int j = 4;

		yy_dyn_str_init(rcc, &args, "", 0);
		n_out = 0;
		op = a->ops;
		for (i = 0; i < n; op++, i++) {
			if (op->flags & (C_ASM_OP_INPUT|C_ASM_OP_OUTPUT)) {
				if (i != 0) yy_dyn_str_append(rcc, &args, ";", 1);
				if (op->id) {
					yy_dyn_str_append(rcc, &args, "[", 1);
					str = yy_sym2strl(rcc, op->id, &len);
					yy_dyn_str_append(rcc, &args, str, len);
					yy_dyn_str_append(rcc, &args, "]", 1);
				}
				yy_dyn_str_append(rcc, &args, op->constraint_str, op->constraint_len);

				if ((op->flags & (C_ASM_OP_INPUT|C_ASM_OP_INOUT))
				 || (op->flags & (C_ASM_OP_MEM|C_ASM_OP_REG_INT|C_ASM_OP_REG_FP)) == C_ASM_OP_MEM) {
					ir_set_op(rcc->active_ctx, ref, j, in[i]);
					j++;
				}
				if (op->flags & C_ASM_OP_OUTPUT) {
					if (n_out) {
						if (op->flags & (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP)) {
							rcc->active_ctx->control = ir_emit1(rcc->active_ctx, IR_OPT(IR_ASM_OUT, op->val.u.type), rcc->active_ctx->control);
						}
					}
					n_out++;
				}
			} else if (op->flags & C_ASM_OP_LABEL) {
				break;
			}
		}

		yy_dyn_str_append0(rcc, &args, "", 0);
		ir_set_op(rcc->active_ctx, ref, 3,
			ir_const_str(rcc->active_ctx, ir_stringl(rcc->active_ctx, args.str, args.len)));

		if (a->flags & C_ASM_GOTO) {
			ir_ref goto_ref = ir_emit1(rcc->active_ctx, IR_ASM_GOTO, rcc->active_ctx->control);
			rcc->active_ctx->control = IR_UNUSED;

			for (; i < n; op++, i++) {
				IR_ASSERT(op->flags & C_ASM_OP_LABEL);

				c_label *label = rcc->yy_hash.data[op->id].label;
				if (!label) {
					label = c_new_label(rcc, op->id, rcc->active_func_scope, NULL, rcc->active_scope == rcc->active_func_scope);
				}

				/* Rederect to actual target labels (breaking critical edges) */
				ir_BEGIN(goto_ref);
				if (a->flags & C_ASM_HAS_OUT_REGS) {
					c_do_asm_store_outs(rcc, a, n, ref);
				}
				ir_END_list(label->src_list);
			}

			ir_BEGIN(goto_ref);
		}

		if (a->flags & C_ASM_HAS_OUT_REGS) {
			c_do_asm_store_outs(rcc, a, n, ref);
		}
	}
}

void c_do_global_asm(rcc_ctx *rcc, c_value *str)
{
	yy_error("asm support not implemented yet");
}

void c_do_compile_start(rcc_ctx *rcc)
{
	rcc->c_dead_code = 0;
	rcc->c_static_data = 0;

	rcc->active_func = NULL;
	rcc->active_func_scope = NULL;
	rcc->active_scope = NULL;
	rcc->active_loop = NULL;
	rcc->active_func_name = 0;
	rcc->c_static_var_num = 0;
	rcc->c_static_str_num = 0;
	rcc->c_fixed_regset = 0;
	rcc->c_label_num = 0;

	ir_strtab_init(&rcc->c_strtab, 256, 0);
}

void c_do_compile_end(rcc_ctx *rcc)
{
	ir_strtab_free(&rcc->c_strtab);
}
