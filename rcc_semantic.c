/*
 * RCC - Rational C Compiler
 * (Semantic analysis and intermediate code (IR) generation)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <ir.h>
#include <ir_private.h>
#include <ir_builder.h>

#include "rcc.h"

#undef _ir_CTX
#define _ir_CTX active_ctx

const c_type c_type_void                = {.kind = C_TYPE_VOID,                .size = 0,  .attr = 1};
const c_type c_type_bool                = {.kind = C_TYPE_BOOL,                .size = 1,  .attr = 1};
const c_type c_type_char                = {.kind = C_TYPE_CHAR,                .size = 1,  .attr = 1};
const c_type c_type_u8                  = {.kind = C_TYPE_U8,                  .size = 1,  .attr = 1};
const c_type c_type_i8                  = {.kind = C_TYPE_I8,                  .size = 1,  .attr = 1};
const c_type c_type_u16                 = {.kind = C_TYPE_U16,                 .size = 2,  .attr = 2};
const c_type c_type_i16                 = {.kind = C_TYPE_I16,                 .size = 2,  .attr = 2};
const c_type c_type_u32                 = {.kind = C_TYPE_U32,                 .size = 4,  .attr = 3};
const c_type c_type_i32                 = {.kind = C_TYPE_I32,                 .size = 4,  .attr = 3};
const c_type c_type_ul                  = {.kind = C_TYPE_UL,                  .size = C_LONG_SIZE,  .attr = C_LONG_ALIGN};
const c_type c_type_ull                 = {.kind = C_TYPE_ULL,                 .size = 8,  .attr = 4};
const c_type c_type_il                  = {.kind = C_TYPE_IL,                  .size = C_LONG_SIZE,  .attr = C_LONG_ALIGN};
const c_type c_type_ill                 = {.kind = C_TYPE_ILL,                 .size = 8,  .attr = 4};
const c_type c_type_float               = {.kind = C_TYPE_FLOAT,               .size = 4,  .attr = 3};
const c_type c_type_double              = {.kind = C_TYPE_DOUBLE,              .size = 8,  .attr = 4};
//??? TODO: long double support ???
//???const c_type c_type_long_double         = {.kind = C_TYPE_LONG_DOUBLE,         .size = 16, .attr = 4};
const c_type c_type_long_double         = {.kind = C_TYPE_DOUBLE,              .size = 8,  .attr = 4};
const c_type c_type_float_complex       = {.kind = C_TYPE_FLOAT_COMPLEX,       .size = 8,  .attr = 3};
const c_type c_type_double_complex      = {.kind = C_TYPE_DOUBLE_COMPLEX,      .size = 16, .attr = 5};
const c_type c_type_long_double_complex = {.kind = C_TYPE_LONG_DOUBLE_COMPLEX, .size = 32, .attr = 5};

const c_type c_type_string = {
	.kind = C_TYPE_ARRAY,
	.size = sizeof(void*),
	.attr = 1 | /*C_ATTR_CONST | ???*/ C_ATTR_FLEXIBLE,
	.array.type = &c_type_char,
	.array.length = 0
};

const c_type c_type_lstring = {
	.kind = C_TYPE_ARRAY,
	.size = sizeof(void*),
	.attr = 3 | /*C_ATTR_CONST | ???*/ C_ATTR_FLEXIBLE,
	.array.type = &c_type_i32,
	.array.length = 0
};

const c_type c_type_string_u16 = {
	.kind = C_TYPE_ARRAY,
	.size = sizeof(void*),
	.attr = 2 | /*C_ATTR_CONST | ???*/ C_ATTR_FLEXIBLE,
	.array.type = &c_type_u16,
	.array.length = 0
};

const c_type c_type_string_u32 = {
	.kind = C_TYPE_ARRAY,
	.size = sizeof(void*),
	.attr = 3 | /*C_ATTR_CONST | ???*/ C_ATTR_FLEXIBLE,
	.array.type = &c_type_u32,
	.array.length = 0
};

       ir_arena   *c_arena;
       bool        c_dead_code = 0;

       ir_ctx     *active_ctx = NULL;
       ir_ctx     *global_ctx = NULL;

static c_sym      *active_func = NULL;
static c_scope    *active_func_scope = NULL;
static c_scope    *active_scope = NULL;
static c_loop     *active_loop = NULL;
static c_name      active_func_name = 0;
static uint32_t    c_static_sym_num = 0;

static bool c_valid_alignment(c_value *val)
{
	return ((C_IS_TYPE_SIGNED(val->type) && val->u.val.i64 >= 0)
	 && val->u.val.u64 != 0
	 && (val->u.val.u64 & (val->u.val.u64 - 1)) == 0);
}

static uint32_t c_align2attr(size_t align)
{
	if (align == 0) return 0;
	return ir_ntzl(align) + 1;
}

static size_t c_attr2align(uint32_t attr)
{
	if ((attr & C_ATTR_ALIGN_MASK) == 0) return 0;
	return 1ULL << ((attr & C_ATTR_ALIGN_MASK) - 1);
}

static size_t c_aligned_type_size(const c_type *t)
{
	if (t->attr & C_ATTR_ALIGN_MASK) {
		return IR_ALIGNED_SIZE(t->size, c_attr2align(t->attr));
	} else {
		return t->size;
	}
}

ir_type c_type2ir(const c_type *t)
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
#if 1
//???
		case C_TYPE_FUNC:    return IR_ADDR;
		case C_TYPE_STRUCT:  return IR_ADDR;
		case C_TYPE_UNION:   return IR_ADDR;
#endif
		default:
			IR_ASSERT(0);
			return IR_VOID;
	}
}

static ir_ref c_type2proto(const c_type *t, uint32_t linkage)
{
	uint32_t flags = 0;
	uint32_t params_count;
	uint8_t *param_types;
	int i;
	const c_type *ret_type;

	IR_ASSERT(t->kind == C_TYPE_FUNC);
	ret_type = t->func.ret_type;
	if (ret_type->kind == C_TYPE_STRUCT || ret_type->kind == C_TYPE_UNION) {
		if (ret_type->size <= sizeof(void*)) {
			ret_type = (ret_type->size <= 4) ? &c_type_u32 : &c_type_u64;
		} else {
			yy_error("long struct return not implemented yet"); //???
		}
	}
	if (t->func.num_params > 0) {
		params_count = t->func.num_params;
		param_types = alloca(params_count);
		for (i = 0; i < t->func.num_params; i++) {
			const c_type *param_type = t->func.params[i].type;

			if (param_type->kind == C_TYPE_STRUCT || param_type->kind == C_TYPE_UNION) {
				if (param_type->size <= sizeof(void*)) {
					param_type = (param_type->size <= 4) ? &c_type_u32 : &c_type_u64;
				} else {
					yy_error("long struct arguments not implemented yet"); //???
				}
			}
			param_types[i] = c_type2ir(param_type);
		}
	} else {
		params_count = 0;
		param_types = NULL;
	}
	if (t->attr & C_ATTR_VARIADIC) {
		flags |= IR_VARARG_FUNC;
	}
	if (linkage == C_LINK_BUILTIN) {
		flags |= IR_BUILTIN_FUNC;
	}
	return ir_proto(active_ctx, flags, c_type2ir(ret_type), params_count, param_types);
}

static bool c_fix_incomplete_type(const c_type *type)
{
	IR_ASSERT((type->flags & C_TYPE_INCOMPLETE) && type->tag);
	if (yy_hash.data[type->tag].tag
	 && yy_hash.data[type->tag].tag->type != type
	 && yy_hash.data[type->tag].tag->type->kind == type->kind
	 && !(yy_hash.data[type->tag].tag->type->flags & C_TYPE_INCOMPLETE)) {
		uint32_t attr = type->attr;
		c_type *t = (c_type*)type;
		*t = *yy_hash.data[type->tag].tag->type;
		t->attr |= (attr & C_TYPE_ATTRS); //???
		return 1;
	}
	return 0;
}

void c_push_scope(c_scope *scope)
{
	scope->list.syms = NULL;
	scope->list.size = 0;
	scope->list.len = 0;
	scope->checkpoint = ir_arena_checkpoint(c_arena);
	scope->prev = active_scope;
	active_scope = scope;
}

void c_pop_scope(c_scope *scope)
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
					yy_hash.data[id].sym = ptr;
					break;
				case C_POP_TAG:
					yy_hash.data[id].tag = ptr;
					break;
				case C_POP_LABEL:
					c_do_finish_label(id, yy_hash.data[id].label);
					if (!yy_hash.data[id].label->is_local) {
						ir_mem_free(yy_hash.data[id].label);
					}
					yy_hash.data[id].label = ptr;
					break;
				default:
					IR_ASSERT(0);
			}
		}
		pp_list_release(scope->list.syms, scope->list.size);
	}
	if (scope->checkpoint) {
		ir_arena_release(&c_arena, scope->checkpoint);
	}
	active_scope = scope->prev;
	if (scope == active_func_scope) {
		active_func_scope = active_scope;
	}
}

const c_type *c_resolve_type_name(c_name name)
{
	c_sym *s = yy_hash.data[name].sym;
	if (!s || s->kind != C_SYM_TYPE) yy_error_fmt("\"%s\" is not a type name", yy_hash.data[name].str);
	return s->value.type;
}

void c_resolve_sym_name(c_value *res, c_name name, yy_sym sym)
{
	c_sym *s = yy_hash.data[name].sym;
	if (!s) {
		if (sym == YY__LPAREN) {
			c_dcl dcl;

			if (yy_flags & PP_EVAL_EXPRESSION) {
				ir_val val;
				if (!c_dead_code) yy_error_fmt("undefined function macro \"%s\"", yy_hash.data[name].str);
				val.u64 = 0;
				c_value_set_const(res, &c_type_void, IR_VOID, val);
				return;
			}
			yy_warning_fmt("implicit declaration of function \"%s\"", yy_hash.data[name].str);

			/* Function in going to be declared in the global scope */
			memset(&dcl, 0, sizeof(dcl));
			dcl.flags = C_DCL_EXTERN | C_TYPE_SPEC_TYPE;
			c_type *type = type = ir_arena_alloc(&c_arena, sizeof(c_type));
			type->kind = C_TYPE_FUNC;
			type->flags = 0;
			type->attr = 0;
			type->size = sizeof(void*);
			type->func.ret_type = &c_type_i32;
			type->func.num_params = -1;
			type->func.params = NULL;
			dcl.type = type;
			s = c_declare(name, &dcl);
		} else {
			yy_error_fmt("undefined identifier \"%s\"", yy_hash.data[name].str);
		}
	}
	if (s->kind == C_SYM_TYPE) yy_error_fmt("\"%s\" is a type name", yy_hash.data[name].str);
	if (s->kind == C_SYM_CONST) {
		c_value_set_const(res, s->value.type, c_type2ir(s->value.type), s->value.u.val);
	} else if (s->kind == C_SYM_VAR || s->kind == C_SYM_FUNC) {
		if (c_value_is_ref(&s->value)) {
			IR_ASSERT(s->kind != C_SYM_FUNC);
			*res = s->value;
		} else if (s->linkage == C_LINK_EXTERNAL || s->linkage == C_LINK_INTERNAL || s->linkage == C_LINK_BUILTIN) {
			const char *name_str;
			size_t name_len;
			ir_ref ref;

			name_str = yy_sym2strl(name, &name_len);
			if (s->kind == C_SYM_FUNC) {
				ref = ir_const_func(active_ctx, ir_strl(active_ctx, name_str, name_len),
					c_type2proto(s->value.type, s->linkage));
			} else {
				ref = ir_const_sym(active_ctx, ir_strl(active_ctx, name_str, name_len));
			}
			if (s->kind == C_SYM_FUNC) {
				c_value_set_rval(res, s->value.type, IR_ADDR, ref);
				if (s->linkage == C_LINK_BUILTIN) {
					res->u.op |= C_VAL_BUILTIN;
				}
				if (s == active_func) {
					/* recursive function - disable inlining */
					c_type *type = (c_type*)s->value.type;
					type->attr |= C_ATTR_NOINLINE;
				} else if (s->value.u.op & C_VAL_INLINE) {
					res->u.op |= C_VAL_INLINE;
					res->u.val.ptr = s->ctx;
				}
			} else if (s->value.type->kind != C_TYPE_ARRAY) {
				c_value_set_lval(res, s->value.type, c_type2ir(s->value.type), ref);
			} else {
				c_value_set_rval(res, s->value.type, c_type2ir(s->value.type), ref);
			}
		} else {
			IR_ASSERT(0);
		}
	} else {
		IR_ASSERT(0);
	}
}

static void c_resolve_type_spec(c_dcl *d)
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
			break;
		default:
			if ((d->flags & C_TYPE_SPEC_ANY) == 0) {
				yy_warning("type defaults to \"int\"");
				d->type = &c_type_i32;
				break;
			}
			yy_error("unsupported type specifier combination");
			break;
	}
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
}

static void c_validate_dcl(c_dcl *d)
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
			if (d->type->kind == C_TYPE_FUNC) yy_error("invalid use of \"_Alignas\" for a function");
			if (d->flags & C_DCL_TYPEDEF) yy_error("invalid use of \"_Alignas\" with \"typedef\"");
			if (d->flags & C_DCL_REGISTER) yy_error("invalid use of \"_Alignas\" with \"register\"");
		}
	}
}

static void c_merge_type_attr(c_dcl *d)
{
	c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));
	*type = *d->type;
	type->attr |= (d->attr & C_TYPE_ATTRS);
	if ((d->attr & C_ATTR_ALIGN_MASK)
	 && (d->attr & C_ATTR_ALIGN_MASK) != (d->type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr = (type->attr & ~C_ATTR_ALIGN_MASK) | (d->attr & C_ATTR_ALIGN_MASK);
	}
	d->type = type;
}

static void c_finalize_type(c_dcl *d)
{
	if (!d->type) c_resolve_type_spec(d);
	if (d->flags & C_TYPE_SPEC_ATOMIC) {
		d->attr |= C_ATTR_ATOMIC;
		d->flags &= ~C_TYPE_SPEC_ANY;
		d->flags |= C_TYPE_SPEC_TYPE;
	}

	c_validate_dcl(d);

	if ((d->flags & C_TYPE_SPEC_NAME)
	 && (d->attr & C_TYPE_ATTRS)
	 && (d->type->kind == C_TYPE_ARRAY)) {
		const c_type *t = d->type;

		do {
			t = t->array.type;
		} while (t->kind == C_TYPE_ARRAY);
		if ((t->attr & C_TYPE_ATTRS) != (d->attr & C_TYPE_ATTRS)) {
			c_type *tmp = ir_arena_alloc(&c_arena, sizeof(c_type));

			*tmp = *d->type;
			d->type = tmp;
			t = d->type;
			do {
				tmp = ir_arena_alloc(&c_arena, sizeof(c_type));
				*tmp = *t->array.type;
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
		c_merge_type_attr(d);
	}
	d->attr &= ~C_TYPE_ATTRS;
}

const c_type *c_resolve_type(c_dcl *d)
{
	c_finalize_type(d);
	return d->type;
}

static bool c_compatible_types(const c_type *t1, const c_type *t2, bool unqualified, bool func)
{
	uint32_t attr1, attr2;
	c_type_kind t1_kind = t1->kind;
	c_type_kind t2_kind = t2->kind;

	if (t1 == t2) return 1;

	attr1 = t1->attr & ~(C_ATTR_ALIGN_MASK|C_ATTR_FLEXIBLE|0xfffe0000);
	attr2 = t2->attr & ~(C_ATTR_ALIGN_MASK|C_ATTR_FLEXIBLE|0xfffe0000);
	if (unqualified) {
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
//		return 0;
	} else if (t1->kind == C_TYPE_ARRAY) {
		if (!(t1->attr & C_ATTR_FLEXIBLE) && !(t2->attr & C_ATTR_FLEXIBLE) && t1->array.length != t2->array.length) return 0;
		if (!c_compatible_types(t1->array.type, t2->array.type, 0, 0)) return 0;
	} else if (t1->kind == C_TYPE_POINTER) {
		if (!c_compatible_types(t1->pointer.type, t2->pointer.type, 0, 0)) return 0;
	} else if (t1->kind == C_TYPE_STRUCT || t1->kind == C_TYPE_UNION) {
		if (t1->record.tag != t2->record.tag) return 0;
		if (t1->record.tag && t2->record.tag) return 1;
		if (t1->record.fields != t2->record.fields) return 0;
	} else if (t1->kind == C_TYPE_FUNC) {
		c_param *p1, *p2;
		uint32_t n;

		if (!c_compatible_types(t1->func.ret_type, t2->func.ret_type, 0, 0)) return 0;
		if (func && (t1->func.num_params < 0 || t2->func.num_params < 0)) return 1;
		if (t1->func.num_params != t2->func.num_params) return 0;
		if (t1->func.num_params <= 0) return 1;
		p1 = t1->func.params;
		p2 = t2->func.params;
		for (n = t1->func.num_params; n > 0; p1++, p2++, n--) {
			if (!c_compatible_types(p1->type, p2->type, 1, 0)) return 0;
		}
	};
	return 1;
}

static void c_validate_redeclaration(c_name name, c_dcl *d, c_sym *sym)
{
	if (d->flags & C_DCL_TYPEDEF) {
		if (sym->kind != C_SYM_TYPE) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(name));
		} else if (!c_compatible_types(d->type, sym->value.type, 0, 0)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(name));
		}
	} else if (d->flags & C_DCL_ENUM_CONST) {
		yy_error_fmt("redeclaration of \"%s\"", yy_sym2str(name));
	} else if (d->type->kind == C_TYPE_FUNC) {
		if (sym->kind != C_SYM_FUNC) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(name));
		} else if (!c_compatible_types(d->type, sym->value.type, 0, 1)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(name));
		} else if ((d->flags & C_DCL_STATIC) && sym->linkage != C_LINK_INTERNAL) {
			yy_error_fmt("static declaration of \"%s\" follows non-static declaration", yy_sym2str(name));
		} else {
			if (sym->value.type->func.num_params < 0 && d->type->func.num_params >= 0) {
				c_type *t = (c_type*)sym->value.type;
				t->func.num_params = d->type->func.num_params;
				t->func.params = d->type->func.params;
			}
		}
		if ((d->flags & C_DCL_DEFINITION) && sym->is_implemented) {
			yy_error_fmt("redefinition of \"%s\"", yy_sym2str(name));
		}
	} else {
		if (sym->kind != C_SYM_VAR || !c_compatible_types(d->type, sym->value.type, 0, 0)) {
			yy_error_fmt("incompatible redeclaration of \"%s\"", yy_sym2str(name));
		} else if ((d->flags & C_DCL_DEFINITION) && sym->is_implemented) {
			yy_error_fmt("redefinition of \"%s\"", yy_sym2str(name));
		} else if ((d->flags & C_DCL_STATIC) && sym->linkage != C_LINK_INTERNAL) {
			yy_error_fmt("static declaration of \"%s\" follows non-static declaration", yy_sym2str(name));
		} else if (!(d->flags & (C_DCL_STATIC|C_DCL_EXTERN)) && sym->linkage == C_LINK_INTERNAL) {
			yy_error_fmt("non-static declaration of \"%s\" follows static declaration", yy_sym2str(name));
		} else if ((d->flags & C_DCL_THREAD_LOCAL) && !sym->is_thread_local) {
			yy_error_fmt("thread-local declaration of \"%s\" follows non-thread-local declaration", yy_sym2str(name));
		} else if (!(d->flags & C_DCL_THREAD_LOCAL) && sym->is_thread_local) {
			yy_error_fmt("non-thread-local declaration of \"%s\" follows thread-local declaration", yy_sym2str(name));
		}
		if (sym->value.type->kind == C_TYPE_ARRAY && (sym->value.type->attr & C_ATTR_FLEXIBLE)
		 && d->type->kind == C_TYPE_ARRAY && !(d->type->attr & C_ATTR_FLEXIBLE)) {
			c_type *t = (c_type*)sym->value.type;
			t->attr &= ~C_ATTR_FLEXIBLE;
			t->array.length = d->type->array.length;
			t->size = d->type->size;
		}
		if (sym->linkage == C_LINK_EXTERNAL
		 && !c_value_is_const(&sym->value)
		 && !(d->flags & C_DCL_EXTERN)
		 && ((d->flags & C_DCL_DEFINITION) || !(d->type->attr & C_ATTR_FLEXIBLE))) {
			void *addr;
			size_t size = d->type->size;

			if (d->type->kind == C_TYPE_ARRAY && (d->type->attr & C_ATTR_FLEXIBLE)
			 && sym->value.type->kind == C_TYPE_ARRAY && !(sym->value.type->attr & C_ATTR_FLEXIBLE)) {
				size = sym->value.type->size;
			}
			sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
			sym->value.u.val.ptr = addr = c_linker_allocate_data(size);
			ir_disasm_add_symbol(yy_sym2str(name), (uintptr_t)addr, size); //???
			sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
		}
	}
}

c_sym *c_global_sym(c_sym *sym)
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

static c_name c_create_static_var(c_name name, void *addr)
{
	yy_dyn_str  dyn_str;
	const char *name_str;
	size_t name_len;
	uint32_t i, n;
	char buf[16];
	c_sym *sym;

	name_str = yy_sym2strl(name, &name_len);
	yy_dyn_str_init(&dyn_str, name_str, name_len);
	yy_dyn_str_append(&dyn_str, ".", 1);

	i = sizeof(buf);
	n = ++c_static_sym_num;
	buf[--i] = 0;
	do {
		buf[--i] = '0' + n % 10;
		n = n / 10;
	} while (n != 0);
	yy_dyn_str_append0(&dyn_str, buf + i, sizeof(buf) - i - 1);
	name = yy_hash_lookup(dyn_str.str, dyn_str.len);

	/* Create a global symbol in yy_arena */
	sym = ir_arena_alloc(&yy_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	sym->kind = C_SYM_VAR;
	sym->linkage = C_LINK_INTERNAL;
	sym->is_thread_local = 0;
	sym->is_implemented = 1;
	sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
	sym->value.u.val.ptr = addr;
	yy_hash.data[name].sym = sym;

	return name;
}

static bool c_is_builtin_func_name(c_name name)
{
	return name >= YY_BUILTIN_FIRST && name <= YY_BUILTIN_LAST;
}

c_sym *c_declare(c_name name, c_dcl *d)
{
	c_sym *sym;
	c_scope *scope = active_scope;

	c_finalize_type(d);
	if (d->attr) {
		if (d->attr & C_ATTR_INLINE) {
			if (d->type->kind != C_TYPE_FUNC) yy_error("invalid use of \"inline\"");
		}
		if (d->attr & C_ATTR_NORETURN) {
			if (d->type->kind != C_TYPE_FUNC) yy_error("invalid use of \"_Noreturn\"");
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

	sym = yy_hash.data[name].sym;
	if (sym) {
		if (d->flags & C_DCL_EXTERN) { // && sym is extern
			if (!sym->scope) {
				c_validate_redeclaration(name, d, sym);
				return sym;
			} else {
				c_sym *gsym = c_global_sym(sym);
				if (gsym) {
					c_validate_redeclaration(name, d, gsym);
				}
				if (active_scope) {
					if (!active_scope->list.syms) pp_list_init(&active_scope->list);
					pp_list_push(&active_scope->list, name);
					pp_list_push_ptr(&active_scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));
				}
				if (gsym) {
					yy_hash.data[name].sym = gsym;
					return gsym;
				}
			}
		}
		if (sym->scope == scope) {
			if (!scope) {
				c_validate_redeclaration(name, d, sym);
				return sym;
			} else {
				yy_error_fmt("redeclaration of \"%s\"", yy_sym2str(name));
				return NULL;
			}
		}
	}

	if (scope) {
		if (!scope->list.syms) pp_list_init(&scope->list);
		pp_list_push(&scope->list, name);
		pp_list_push_ptr(&scope->list, (void*)(((uintptr_t)sym) | C_POP_SYM));
	}

	if (((d->type->flags & C_TYPE_INCOMPLETE)
	  && !(d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN))
	  && !c_fix_incomplete_type(d->type))
	 || ((d->type->attr & C_ATTR_FLEXIBLE)
	  && !(d->flags & (C_DCL_TYPEDEF|C_DCL_EXTERN|C_DCL_DEFINITION))
	  && scope)
	 || ((d->type->kind == C_TYPE_VOID)
	  && !(d->flags & C_DCL_TYPEDEF))) {
		yy_error_fmt("storage size of \"%s\" isn't known", yy_sym2str(name));
		return NULL;
	}

	sym = ir_arena_alloc(&c_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	if (d->flags & C_DCL_TYPEDEF) {
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-C_DCL_TYPEDEF)) == 0);
		sym->kind = C_SYM_TYPE;
	} else if (d->flags & C_DCL_ENUM_CONST) {
		IR_ASSERT((d->flags & C_DCL_STORAGE_CLASS) == 0);
		sym->kind = C_SYM_CONST;
		/* the value will be set in c_declare_enum_val() */
	} else if (d->type->kind == C_TYPE_FUNC) {
		if ((d->flags & (C_DCL_THREAD_LOCAL|C_DCL_AUTO|C_DCL_REGISTER))
		 || ((d->flags & C_DCL_STATIC) && active_scope)) {
			yy_error_fmt("invalid storage class for function \"%s\"", yy_sym2str(name));
		}
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC))) == 0);
		sym->kind = C_SYM_FUNC;
		if (!(d->flags & (C_DCL_STATIC|C_DCL_DEFINITION))
		 && c_is_builtin_func_name(name)) {
			/* TODO: verify prototype ??? */
			sym->linkage = C_LINK_BUILTIN;
		} else {
			sym->linkage = (d->flags & C_DCL_STATIC) ? C_LINK_INTERNAL : C_LINK_EXTERNAL;
		}
		sym->is_thread_local = 0;
		sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
	} else {
		if (!scope) {
			if (d->flags & C_DCL_AUTO) yy_error_fmt("file-scope declaration of \"%s\" specifies \"auto\"", yy_sym2str(name));
			if (d->flags & C_DCL_REGISTER) yy_error("global register variables are not implemented yet"); //???
		} else {
			if ((d->flags & (C_DCL_THREAD_LOCAL|C_DCL_STATIC|C_DCL_EXTERN)) == C_DCL_THREAD_LOCAL) {
				yy_error_fmt("function-scope \"%s\" declared \"_Thread_local\"", yy_sym2str(name));
			}
		}
		IR_ASSERT((d->flags & (C_DCL_STORAGE_CLASS-(C_DCL_EXTERN|C_DCL_STATIC|C_DCL_THREAD_LOCAL|C_DCL_AUTO|C_DCL_REGISTER))) == 0);
		sym->kind = C_SYM_VAR;
		if ((d->flags & (C_DCL_STATIC|C_DCL_EXTERN)) || !scope) {
			sym->linkage = (d->flags & C_DCL_STATIC) ? C_LINK_INTERNAL : C_LINK_EXTERNAL;
			sym->is_thread_local = (d->flags & C_DCL_THREAD_LOCAL) != 0;

			if ((d->flags & C_DCL_EXTERN) && (d->flags & C_DCL_DEFINITION)) {
				yy_warning_fmt("\%s\" initialized and declared \"extern\"", yy_sym2str(name));
				d->flags &= ~C_DCL_EXTERN;
			}
			if (!(d->flags & C_DCL_EXTERN)
			 && ((d->flags & C_DCL_DEFINITION) || !(d->type->attr & C_ATTR_FLEXIBLE))) {
				void *addr = c_linker_allocate_data(d->type->size);

				sym->is_implemented = (d->flags & C_DCL_DEFINITION) != 0;
				if (!scope || !(d->flags & C_DCL_STATIC)) {
					ir_disasm_add_symbol(yy_sym2str(name), (uintptr_t)addr, d->type->size); //???
					sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
					sym->value.u.val.ptr = addr;
				} else {
					ir_ref sym_name = c_create_static_var(name, addr);
					size_t len;
					const char *str = yy_sym2strl(sym_name, &len);
					ir_ref ref;

					ir_disasm_add_symbol(str, (uintptr_t)addr, d->type->size); //???
					ref = ir_const_sym(active_ctx, ir_strl(active_ctx, str, len));
					if (d->type->kind == C_TYPE_ARRAY) {
						c_value_set_rval(&sym->value, d->type, c_type2ir(d->type), ref);
					} else {
						c_value_set_lval(&sym->value, d->type, c_type2ir(d->type), ref);
					}
					sym->value.u.val.ptr = addr;
				}
			}
		} else {
			ir_ref ref;

			sym->linkage = C_LINK_NONE;
			sym->is_thread_local = 0;
			if (d->type->kind == C_TYPE_ARRAY) {
				size_t size = (d->type->attr & C_ATTR_FLEXIBLE) ? (size_t)-1 : d->type->size;
				ref = c_do_alloca(size, (d->flags & C_DCL_DEFINITION) != 0);
				c_value_set_rval(&sym->value, d->type, c_type2ir(d->type), ref);
			} else if (d->type->kind == C_TYPE_STRUCT || d->type->kind == C_TYPE_UNION) {
				ref = c_do_alloca(d->type->size, (d->flags & C_DCL_DEFINITION) != 0);
				c_value_set_lval(&sym->value, d->type, c_type2ir(d->type), ref);
			} else {
				ref = ir_var(active_ctx, c_type2ir(d->type), 1, yy_sym2str(name));
				c_value_set_var(&sym->value, d->type, c_type2ir(d->type), ref);
			}
		}
	}
	sym->value.type = d->type;
	sym->scope = scope;

	yy_hash.data[name].sym = sym;

	return sym;
}

void c_empty_declaration(c_dcl *d)
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

c_type *c_resolve_tag(c_name name, c_dcl *d, bool define)
{
	c_type *type;
	c_tag *tag;

	IR_ASSERT(name);
	tag = yy_hash.data[name].tag;
	if (tag) {
		if (((d->flags & C_TYPE_SPEC_ENUM) && (tag->type->kind != C_TYPE_ENUM))
		 || ((d->flags & C_TYPE_SPEC_STRUCT) && (tag->type->kind != C_TYPE_STRUCT))
		 || ((d->flags & C_TYPE_SPEC_UNION) && (tag->type->kind != C_TYPE_UNION))) {
			yy_error_fmt("\"%s\" defined as wrong kind of tag", yy_sym2str(name));
		}
		if (define) {
			if (tag->type->flags & C_TYPE_INCOMPLETE) {
				if (tag->type->flags & C_TYPE_INPROGRESS) yy_error_fmt("nested redefinition of \"%s %s\"", c_tag2str(tag), yy_sym2str(name));
				if (tag->scope == active_scope) {
					d->type = tag->type;
					return (c_type*)tag->type;
				}
			} else {
				if (tag->scope == active_scope) yy_error_fmt("redefinition of \"%s %s\"", c_tag2str(tag), yy_sym2str(name));
			}
		} else {
			d->type = tag->type;
			d->flags &= ~C_TYPE_SPEC_ANY;
			d->flags |= C_TYPE_SPEC_TYPE;
			return (c_type*)tag->type;
		}
	}

	if (active_scope) {
		if (!active_scope->list.syms) pp_list_init(&active_scope->list);
		pp_list_push(&active_scope->list, name);
		pp_list_push_ptr(&active_scope->list, (void*)(((uintptr_t)tag) | C_POP_TAG));
	}

	if (d->flags & C_TYPE_SPEC_ENUM) {
		type = c_make_enum_type(d, name);
	} else {
		type = c_make_struct_type(d, name);
	}

	tag = ir_arena_alloc(&c_arena, sizeof(c_tag));
	tag->scope = active_scope;
	tag->type = d->type;

	yy_hash.data[name].tag = tag;
	return type;
}

static void c_validate_pointer_type(const c_type *t)
{
	//???
}

static c_type *c_create_pointer_type(const c_type *element_type)
{
	c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));
	type->kind = C_TYPE_POINTER;
	type->flags = 0;
	type->attr = c_align2attr(_Alignof(void*));
	type->size = sizeof(void*);
	type->pointer.type = element_type;
	return type;
}

void c_make_pointer_type(c_dcl *d)
{
	c_type *type;

	c_finalize_type(d);
	c_validate_pointer_type(d->type);

	type = c_create_pointer_type(d->type);
	type->attr |= d->attr & C_POINTER_ATTRS;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_POINTER_ATTRS;
}

static void c_validate_array_element_type(const c_type *t)
{
	if (t->kind == C_TYPE_VOID) yy_error("array of voids");
	if (t->kind == C_TYPE_FUNC) yy_error("array of functions");
	if ((t->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(t)) {
		yy_error("array type has incomplete element type");
	}
	if (t->kind == C_TYPE_ARRAY && (t->attr & C_ATTR_FLEXIBLE)) {
		yy_error("array type has element type with undefined size");
	}
}

void c_make_array_type(c_dcl *d, c_dcl *dim, c_value *len, uint64_t attr)
{
	c_type *type;
	size_t length;

	c_finalize_type(d);
	c_validate_array_element_type(d->type);

	length = 0;
	if (!(attr & (C_ATTR_FLEXIBLE|C_ATTR_VLA)) && len && c_value_is_set(len)) {
		if (!C_IS_TYPE_INT(len->type) && len->type->kind != C_TYPE_ENUM) {
			yy_error("size of array has non-integer type");
		} else if (!c_value_is_const(len)) {
			if (!active_scope) {
				yy_error("array size must be a constant expression");
			} else {
				yy_error("variable length arrays are not supported yet"); //???
			}
		} else {
			if (IR_IS_TYPE_SIGNED(len->u.type) && len->u.val.i64 < 0) yy_error("array size is negative");
			length = len->u.val.u64;
		}
	}

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	type->kind = C_TYPE_ARRAY;
	type->flags = 0;
	type->size = c_aligned_type_size(d->type) * length;
	type->attr = attr | (d->attr & C_ARRAY_ATTRS);
	if ((d->type->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= d->type->attr & C_ATTR_ALIGN_MASK;
	}
	type->array.type = d->type;
	type->array.length = length;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_ARRAY_ATTRS;
}

c_type *c_make_enum_type(c_dcl *d, c_name tag)
{
	c_type *type;
	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	type->kind = C_TYPE_ENUM;
	type->flags = C_TYPE_INCOMPLETE;
	type->attr = (d->attr & C_ENUM_ATTRS);
	type->size = 0;
	type->enumeration.tag = tag;
	type->enumeration.kind = C_TYPE_I64; /* this is going to be fixed in c_finish_enum_type(); */
	type->enumeration.values = (c_name*)type; /* fake pointer for comparison only */

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_ENUM_ATTRS;

	return type;
}

void c_declare_enum_val(const c_type *type, c_name name, c_dcl *attr, c_value *val, int64_t *min, uint64_t *max, c_value *last)
{
	c_sym *obj;
	const c_type *const_type;

	if (val && c_value_is_set(val)) {
		if (!c_value_is_const(val) || (!C_IS_TYPE_INT(val->type) && val->type->kind != C_TYPE_ENUM)) {
			yy_error_fmt("enumerator value for \"%s\" is not an integer constant", yy_sym2str(name));
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
	obj = c_declare(name, attr);
	IR_ASSERT(obj && obj->kind == C_SYM_CONST);
	c_value_set_const(&obj->value, const_type, last->u.type, last->u.val);
}

void c_finish_enum_type(c_type *type, c_dcl *d, int64_t min, uint64_t max)
{
	IR_ASSERT(type && type->kind == C_TYPE_ENUM);
	type->attr |= d->attr & C_ENUM_ATTRS;
	if ((type->attr & C_ATTR_PACKED) && min >= -0x7FLL-1 && max <= 0x7FULL) {
		type->enumeration.kind = C_TYPE_I8;
		type->size = sizeof(int8_t);
		type->attr |= c_align2attr(_Alignof(int8_t));
	} else if ((type->attr & C_ATTR_PACKED) && min >= 0 && max <= 0xFFULL) {
		type->enumeration.kind = C_TYPE_U8;
		type->size = sizeof(uint8_t);
		type->attr |= c_align2attr(_Alignof(uint8_t));
	} else if ((type->attr & C_ATTR_PACKED) && min >= -0x7FFFLL-1 && max <= 0x7FFFULL) {
		type->enumeration.kind = C_TYPE_I16;
		type->size = sizeof(int16_t);
		type->attr |= c_align2attr(_Alignof(int16_t));
	} else if ((type->attr & C_ATTR_PACKED) && min >= 0 && max <= 0xFFFFULL) {
		type->enumeration.kind = C_TYPE_U16;
		type->size = sizeof(uint16_t);
		type->attr |= c_align2attr(_Alignof(uint16_t));
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

	if ((d->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= (d->attr & C_ATTR_ALIGN_MASK);
	}
	type->flags &= ~C_TYPE_INCOMPLETE;

	d->type = type;
}

c_type *c_make_struct_type(c_dcl *d, c_name tag)
{
	c_type *type;
	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	type->kind = (d->flags & C_TYPE_SPEC_UNION) ? C_TYPE_UNION : C_TYPE_STRUCT;
	type->flags = C_TYPE_INCOMPLETE;
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

static void c_grow_struct_fields(c_type *type)
{
	if (type->record.num_fields == C_ALLOCA_FIELDS) {
		c_field *ptr = ir_mem_malloc(type->record.num_fields * 2 * sizeof(c_field));
		memcpy(ptr, type->record.fields, type->record.num_fields * sizeof(c_field));
		type->record.fields = ptr;
	} else if (type->record.num_fields % C_ALLOCA_FIELDS == 0) {
		type->record.fields = ir_mem_realloc(type->record.fields, IR_ALIGNED_SIZE(type->record.num_fields + 1, C_ALLOCA_FIELDS) * sizeof(c_field));
	}
}

static c_field *c_find_struct_field(const c_type *type, c_name name, size_t *offset)
{
	int32_t i;
	c_field *f;

	for (i = 0, f = type->record.fields; i < type->record.num_fields; f++, i++) {
		if (f->name) {
			if (f->name == name) {
				*offset = f->offset;
				return f;
			}
		} else if (f->type->kind == C_TYPE_STRUCT || f->type->kind == C_TYPE_UNION) {
			c_field *f2 = c_find_struct_field(f->type, name, offset);
			if (f2) {
				*offset += f->offset;
				return f2;
			}
		}
	}
	return NULL;
}

void c_static_assert(c_value *expr, c_value *msg)
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

void c_declare_struct_field(c_type *type, c_name name, c_dcl *field, c_value *bits)
{
	uint32_t i;
	size_t offset;

	IR_ASSERT(type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION);

	c_finalize_type(field);
	if (field->type->kind == C_TYPE_VOID) yy_error_fmt("field \"%s\" declared void", yy_sym2str(name));
	if (field->type->kind == C_TYPE_FUNC) yy_error_fmt("field \"%s\" declared as a function", yy_sym2str(name));
	if ((field->type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(field->type)) {
		yy_error_fmt("field \"%s\" has incomplete type", yy_sym2str(name));
	}
	if (name && c_find_struct_field(type, name, &offset)) yy_error_fmt("duplicate member \"%s\"", yy_sym2str(name));

	if (type->record.num_fields >= C_ALLOCA_FIELDS) c_grow_struct_fields(type);

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
		if (!c_value_is_const(bits)) yy_error_fmt("bit-field \"%s\" width not an integer constant", yy_sym2str(name));
		if (!IR_IS_TYPE_INT(bits->u.type)) yy_error_fmt("bit-field \"%s\" width not an integer constant", yy_sym2str(name));
		if (IR_IS_TYPE_SIGNED(bits->u.type) && bits->u.val.i64 < 0) yy_error_fmt("negative width in bit-field \"%s\"", yy_sym2str(name));
		if (bits->u.val.i64 == 0 && name) yy_error_fmt("zero width for bit-field \"%s\"", yy_sym2str(name));
		if (!C_IS_TYPE_INT(field->type) && field->type->kind != C_TYPE_ENUM) {
			yy_error_fmt("bit-field \"%s\" has invalid type", yy_sym2str(name));
		}
		if (bits->u.val.u64 > field->type->size * 8) yy_error_fmt("width of \"%s\" exceeds its type", yy_sym2str(name));
		if (bits->u.val.u64 < field->type->size * 8) {
			IR_ASSERT(bits->u.val.u64 < 64);
			type->record.fields[i].bit_field = C_BIT_FIELD(0, bits->u.val.u64);
		} else {
			type->record.fields[i].bit_field = 0;
		}
	}
}

static void c_do_check_nested_redeclarations(const c_type *type, const c_type *nested_type)
{
	int32_t i;

	for (i = 0; i < nested_type->record.num_fields; i++) {
		c_field *field = &nested_type->record.fields[i];

		if (field->name) {
			size_t offset;
			c_field *field2 = c_find_struct_field(type, field->name, &offset);

			if (field2 && field2 != field) {
				yy_error_fmt("duplicate member \"%s\"", yy_sym2str(field->name));
			}
		} else if (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT) {
			c_do_check_nested_redeclarations(type, field->type);
		}
	}
}

#ifdef _WIN32
# define IS_GCC_STRUCT(attr) (((attr) & C_ATTR_GCC_STRUCT) == 1)
#else
# define IS_GCC_STRUCT(attr) (((attr) & C_ATTR_MS_STRUCT) == 0)
#endif

static size_t c_gcc_field_alignment(c_type *type, c_field *field, bool *packed)
{
	size_t align = c_attr2align(field->type->attr);
	size_t a = field->offset;

	if (!align) align = 1;
	if (!C_IS_BIT_FIELD(field->bit_field)
	 || C_BIT_FIELD_SIZE(field->bit_field) != 0) {
		if ((type->attr & C_ATTR_PACKED) || (field->type->attr & C_ATTR_PACKED)) {
			align = 1;
			*packed = 1;
		}
		if (pp_pack) {
			*packed = 1;
			if (pp_pack < align) align = pp_pack;
			if (pp_pack < a) a = 0;
		}
	}
	if (a) align = a;
	return align;
}

static size_t c_ms_field_alignment(c_type *type, c_field *field)
{
	size_t align = c_attr2align(field->type->attr);
	size_t a = field->offset;

	if (!align) align = 1;
	if ((type->attr & C_ATTR_PACKED) || (field->type->attr & C_ATTR_PACKED)) {
		align = 1;
	}
	if (pp_pack) {
		if (pp_pack < align) align = pp_pack;
		if (pp_pack < a) a = 0;
	}
	if (a) align = a;
	return align;
}

void c_finish_struct_type(c_type *type, c_dcl *d)
{
	IR_ASSERT(type && (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION));
	type->attr |= d->attr & C_STRUCT_ATTRS;
	if ((d->attr & C_ATTR_ALIGN_MASK) > (type->attr & C_ATTR_ALIGN_MASK)) {
		type->attr &= ~C_ATTR_ALIGN_MASK;
		type->attr |= (d->attr & C_ATTR_ALIGN_MASK);
	}

	if (type->record.num_fields) {
		c_field *fields = ir_arena_alloc(&c_arena, sizeof(c_field) * type->record.num_fields);
		int32_t i;
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
						c_do_check_nested_redeclarations(type, field->type);
					}

					field_align = c_gcc_field_alignment(type, field, &packed);
					field_size = C_IS_BIT_FIELD(field->bit_field) ?
						(size_t)(C_BIT_FIELD_SIZE(field->bit_field) + 7) / 8 : field->type->size;
					field->offset = 0;
					if (field_align > struct_align) struct_align = field_align;
					if (field_size > size) size = field_size;
				}
			} else {
				for (i = 0; i < type->record.num_fields; i++) {
					c_field *field = &type->record.fields[i];

					if (!field->name
					 && (field->type->kind == C_TYPE_UNION || field->type->kind == C_TYPE_STRUCT)) {
						c_do_check_nested_redeclarations(type, field->type);
					}

					field_align = c_ms_field_alignment(type, field);
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
						c_do_check_nested_redeclarations(type, field->type);
					}
					if (/*field->type->kind == C_TYPE_ARRAY
					 && */(field->type->attr & C_ATTR_FLEXIBLE)) {
						if (type->kind == C_TYPE_UNION) yy_error("flexible array member in union");
						if (i != type->record.num_fields - 1) yy_error("flexible array member not at the end of struct");
//						if (type->record.num_fields == 1) yy_error("flexible array member in a struct with no named members");
					}

					field_align = c_gcc_field_alignment(type, field, &packed);
					if (!C_IS_BIT_FIELD(field->bit_field)) {
						field->offset = IR_ALIGNED_SIZE(size, field_align);
						last_offset = size = field->offset + field->type->size;
						last_bit = 0;
					} else {
						uint32_t bits = C_BIT_FIELD_SIZE(field->bit_field);
						uint32_t first_bit = 0;

						if (field->type->size == 8 && bits <= 32) {
							field->type = (C_IS_TYPE_SIGNED(field->type)) ? &c_type_i32 : &c_type_u32;
						}
						if (bits == 0 || field->offset) {
							last_offset = IR_ALIGNED_SIZE(size, field_align);
							last_bit = bits;
							if (bits == 0
							 && ((type->attr & C_ATTR_PACKED) || pp_pack)) {
								/* prevent modification of the struct alignment */
								field_align = 1;
							}
						} else {
							if (!packed
							 && (last_offset * 8 + last_bit) / (field_align * 8)
							  != (last_offset * 8 + last_bit + bits - 1) / (field_align * 8)) {
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
						c_do_check_nested_redeclarations(type, field->type);
					}
					if (/*field->type->kind == C_TYPE_ARRAY
					 && */(field->type->attr & C_ATTR_FLEXIBLE)) {
						if (type->kind == C_TYPE_UNION) yy_error("flexible array member in union");
						if (i != type->record.num_fields - 1) yy_error("flexible array member not at the end of struct");
//						if (type->record.num_fields == 1) yy_error("flexible array member in a struct with no named members");
					}

					field_align = c_ms_field_alignment(type, field);
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

void c_validate_func_params(c_name name, c_dcl *d)
{
	int32_t i;
	const c_type *type = d->type;

	if (type->func.num_params <= 0) return;
	for (i = 0; i < type->func.num_params; i++) {
		if (!type->func.params[i].type) {
			yy_warning_fmt("type of \"%s\" defaults to \"int\"", yy_sym2str(type->func.params[i].name));
			type->func.params[i].type = &c_type_i32;
		}
	}
}

static void c_grow_func_params(c_param **params, int32_t *num_params)
{
	if (*num_params == C_ALLOCA_PARAMS) {
		c_param *ptr = ir_mem_malloc(*num_params * 2 * sizeof(c_param));
		memcpy(ptr, *params, *num_params * sizeof(c_param));
		*params = ptr;
	} else if (*num_params % C_ALLOCA_FIELDS == 0) {
		*params = ir_mem_realloc(*params, IR_ALIGNED_SIZE(*num_params + 1, C_ALLOCA_PARAMS) * sizeof(c_param));
	}
}

void c_declare_func_param(c_param **params, int32_t *num_params, c_name name, c_dcl *param)
{
	c_finalize_type(param);
	if (param->flags & (C_DCL_STORAGE_CLASS-C_DCL_REGISTER)) {
		if (name) {
			yy_error_fmt("storage class specified for parameter \"%s\"", yy_sym2str(name));
		} else {
			yy_error("storage class specified for parameter");
		}
	} else if (param->type->kind == C_TYPE_VOID) {
		if (name) {
			yy_error_fmt("parameter \"%s\" has void type", yy_sym2str(name));
		} else if (*num_params) {
			yy_error("\"void\" must be the only parameter");
		}
	} else if (param->attr & C_ATTR_ALIGN_MASK) {
		yy_error("invalid use of \"_Alignas\" for a function parameter");
	} else if (param->type->kind == C_TYPE_FUNC) {
		c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));
		type->kind = C_TYPE_POINTER;
		type->flags = 0;
		type->attr = c_align2attr(_Alignof(void*));
		type->size = sizeof(void*);
		type->pointer.type = param->type;
		param->type = type;
	} else if (param->type->kind == C_TYPE_ARRAY) {
		param->type = c_create_pointer_type(param->type->array.type);
	}

	if (name) {
		int32_t i;

		for (i = 0; i < *num_params; i++) {
			if ((*params)[i].name == name) {
				yy_error_fmt("redefinition of parameter \"%s\"", yy_sym2str(name));
			}
		}
	}

	if (*num_params >= C_ALLOCA_PARAMS) c_grow_func_params(params, num_params);

	(*params)[*num_params].name = name;
	(*params)[*num_params].type = param->type;
	(*num_params)++;
}

void c_declare_func_param_name(c_param **params, int32_t *num_params, c_name name)
{
	int32_t i;

	IR_ASSERT(name);
	for (i = 0; i < *num_params; i++) {
		if ((*params)[i].name == name) {
			yy_error_fmt("multiple parameters named \"%s\"", yy_sym2str(name));
		}
	}

	if (*num_params >= C_ALLOCA_PARAMS) c_grow_func_params(params, num_params);

	(*params)[*num_params].name = name;
	(*params)[*num_params].type = NULL;
	(*num_params)++;
}

static void c_validate_func_ret_type(const c_type *t)
{
	if (t->kind == C_TYPE_FUNC) yy_error("function returning a function");
	if (t->kind == C_TYPE_ARRAY) yy_error("function returning an array");
}

void c_make_func_type(c_dcl *d, c_param *params, int32_t num_params, bool is_variadic)
{
	c_type *type;

	c_finalize_type(d);
	c_validate_func_ret_type(d->type);

	if (num_params) {
		if (params[0].type && params[0].type->kind == C_TYPE_VOID) {
			if (num_params != 1) yy_error("\"void\" must be the only parameter");
			num_params = 0;
		}
	} else {
		num_params = -1;
	}
	if (num_params > 0) {
		c_param *ptr = ir_arena_alloc(&c_arena, sizeof(c_param) * num_params);
		memcpy(ptr, params, sizeof(c_param) * num_params);
		if (num_params > C_ALLOCA_PARAMS) ir_mem_free(params);
		params = ptr;
	} else {
		params = NULL;
	}

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->flags = 0;
	type->size = sizeof(void*);
	type->attr = d->attr & C_FUNC_TYPE_ATTRS;
	if (is_variadic) type->attr |= C_ATTR_VARIADIC;
	type->func.ret_type = d->type;
	type->func.num_params = num_params;
	type->func.params = params;

	d->type = type;
	d->flags &= ~C_TYPE_SPEC_ANY;
	d->flags |= C_TYPE_SPEC_TYPE;
	d->attr &= ~C_FUNC_TYPE_ATTRS;
}

void c_declare_func_param_type(const c_type *type, c_name name, c_dcl *param)
{
	int32_t i;

	IR_ASSERT(type->kind == C_TYPE_FUNC);
	IR_ASSERT(name);
	c_finalize_type(param);
	if (param->flags & (C_DCL_STORAGE_CLASS-C_DCL_REGISTER)) {
		yy_error_fmt("storage class specified for parameter \"%s\"", yy_sym2str(name));
	} else if (param->type->kind == C_TYPE_VOID) {
		yy_error_fmt("parameter \"%s\" has void type", yy_sym2str(name));
	} else if (param->attr & C_ATTR_ALIGN_MASK) {
		yy_error("invalid use of \"_Alignas\" for a function parameter");
	} else if (param->type->kind == C_TYPE_FUNC) {
		c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));
		type->kind = C_TYPE_POINTER;
		type->flags = 0;
		type->attr = c_align2attr(_Alignof(void*));
		type->size = sizeof(void*);
		type->pointer.type = param->type;
		param->type = type;
	} else if (param->type->kind == C_TYPE_ARRAY) {
		param->type = c_create_pointer_type(param->type->array.type);
	}
    if (type->func.num_params > 0) {
		for (i = 0; i < type->func.num_params; i++) {
			if (type->func.params[i].name == name) {
				if (type->func.params[i].type) {
					yy_error_fmt("redefinition of parameter \"%s\"", yy_sym2str(name));
				}
				type->func.params[i].type = param->type;
				return;
			}
		}
	}
	yy_error_fmt("declaration for parameter \"%s\" but no such parameter", yy_sym2str(name));
}

static void c_fix_nested_type(const c_type *t, c_type *nested)
{
	switch (nested->kind) {
		case C_TYPE_POINTER:
			/* "char" is used as a terminator of nested declaration */
			if (nested->pointer.type == &c_type_char) {
				nested->pointer.type = t;
				c_validate_pointer_type(t);
			} else {
				c_fix_nested_type(t, (c_type*)nested->pointer.type);
			}
			break;
		case C_TYPE_ARRAY:
			/* "char" is used as a terminator of nested declaration */
			if (nested->array.type == &c_type_char) {
				nested->array.type = t;
				c_validate_array_element_type(t);
			} else {
				c_fix_nested_type(t, (c_type*)nested->array.type);
			}
			nested->size = nested->array.length * nested->array.type->size;
			nested->attr &= ~C_ATTR_ALIGN_MASK;
			nested->attr |= nested->array.type->attr & C_ATTR_ALIGN_MASK;
			break;
		case C_TYPE_FUNC:
			/* "char" is used as a terminator of nested declaration */
			if (nested->func.ret_type == &c_type_char) {
				nested->func.ret_type = t;
			} else {
				c_fix_nested_type(t, (c_type*)nested->func.ret_type);
			}
			break;
		default:
			IR_ASSERT(0);
	}
}

void c_make_nested_type(c_dcl *d, c_dcl *nested)
{
	c_finalize_type(d);
	if (nested->type && nested->type != &c_type_char) {
		c_fix_nested_type(d->type, (c_type*)nested->type);
		d->type = nested->type;
	}
}

void c_gcc_attribute(c_dcl *d, c_name attr, c_value *val)
{
	if (!c_value_is_set(val)) val = NULL;
	switch (attr) {
		case YY_ALIGNED:
		case YY___ALIGNED__:
			if (!val) {
				// TODO: ???
			} else if (!c_value_is_const(val) || !C_IS_TYPE_INT(val->type)) {
				yy_warning("attribute \"aligned\" value must be an integer constant");
			} else {
				if (!c_valid_alignment(val)) {
					yy_warning("attribute \"aligned\" value must be a power of two");
				} else {
					if ((d->attr & C_ATTR_ALIGN_MASK) != 0) yy_warning("multiple alignments");
					d->attr |= c_align2attr(val->u.val.u64);
				}
			}
			break;
		case YY_PACKED:
		case YY___PACKED__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			if ((d->flags & (C_TYPE_SPEC_ENUM|C_TYPE_SPEC_STRUCT|C_TYPE_SPEC_UNION))
			 || ((d->flags & C_TYPE_SPEC_ANY) == C_TYPE_SPEC_TYPE
			  && (d->type->kind == C_TYPE_ENUM || d->type->kind == C_TYPE_STRUCT || d->type->kind == C_TYPE_UNION)
			  && (d->type->flags & C_TYPE_INCOMPLETE))) {
				d->attr |= C_ATTR_PACKED;
			} else {
				yy_warning_fmt("\"%s\" attribure ignored", yy_sym2str(attr));
			}
			break;
		case YY_GCC_STRUCT:
		case YY___GCC_STRUCT__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_GCC_STRUCT;
			break;
		case YY_MS_STRUCT:
		case YY___MS_STRUCT__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_MS_STRUCT;
			break;
		case YY_CONST:
		case YY___CONST__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_CONST;
			break;
		case YY_ALWAYS_INLINE:
		case YY___ALWAYS_INLINE__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_ALWAYS_INLINE;
			break;
		case YY_NOINLINE:
		case YY___NOINLINE__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_NOINLINE;
			break;
		case YY_NORETURN:
		case YY___NORETURN__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_NORETURN;
			break;
		case YY_NOTHROW:
		case YY___NOTHROW__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_NOTHROW;
			break;
		case YY_LEAF:
		case YY___LEAF__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_LEAF;
			break;
		case YY_PURE:
		case YY___PURE__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_PURE;
			break;
		case YY_HOT:
		case YY___HOT__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_HOT;
			break;
		case YY_COLD:
		case YY___COLD__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_COLD;
			break;
		case YY_DEPRECATED:
		case YY___DEPRECATED__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_DEPRECATED;
			break;
		/* Statement Attributes */
		case YY_FALLTHROUGH:
		case YY___FALLTHROUGH__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_FALLTHROUGH;
			break;
		case YY_MUSTTAIL:
		case YY___MUSTTAIL__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_MUSTTAIL;
			break;
		case YY_CDECL:
		case YY___CDECL__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_CDECL;
			break;
		case YY_FASTCALL:
		case YY___FASTCALL__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_FASTCALL;
			break;
		case YY_UNUSED:
		case YY___UNUSED__:
			if (val) yy_warning_fmt("attribute \"%s\" with unused value", yy_sym2str(attr));
			d->attr |= C_ATTR_UNUSED;
			break;
		default:
			yy_warning_fmt("unsupported attribure \"%s\"", yy_sym2str(attr));
	}
}

void c_sizeof_type(c_value *res, const c_type *type)
{
	ir_val val;

	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type)) {
		yy_error_fmt("invalid application of \"%s\" to incomplete type", "sizeof");
	}
	val.u64 = type->size;
	c_value_set_const(res, &c_type_size_t, IR_SIZE_T, val);
}

void c_sizeof_expr(c_value *res, yy_sym op, c_value *expr, ir_ref old_control)
{
	ir_val val;

	if (op == YY_SIZEOF) {
		if (C_IS_BIT_FIELD(expr->u.proto)) {
			yy_error("\"sizeof\" applied to a bit-field");
		} else if (c_value_is_const(expr)
		 && expr->type->kind == C_TYPE_ARRAY
		 && (expr->type == &c_type_string
		  || expr->type == &c_type_lstring
		  || expr->type == &c_type_string_u16
		  || expr->type == &c_type_string_u32)) {
			val.u64 = expr->u.ref + expr->type->array.type->size; /* ref keeps string lenght */
		} else if (expr->type->attr & C_ATTR_FLEXIBLE) {
			yy_error_fmt("invalid application of \"%s\" to incomplete type", "sizeof");
		} else {
			if (expr->type->kind == C_TYPE_BOOL
			 && c_value_is_ref(expr)
			 && active_ctx->ir_base[expr->u.ref].op != IR_LOAD
			 && active_ctx->ir_base[expr->u.ref].op != IR_VLOAD) {
				/* IR uses 1-byte "bool" for computation, but C assumes 4-byte "int" */
				val.u64 = 4;
			} else {
				val.u64 = expr->type->size;
			}
		}
	} else {
		IR_ASSERT(op == YY___ALIGNOF || op == YY___ALIGNOF__);
		if (expr->type->kind == C_TYPE_BOOL
		 && c_value_is_ref(expr)
		 && active_ctx->ir_base[expr->u.ref].op != IR_LOAD
		 && active_ctx->ir_base[expr->u.ref].op != IR_VLOAD) {
			/* IR uses 1-byte "bool" for computation, but C assumes 4-byte "int" */
			val.u64 = 4;
		} else {
			val.u64 = c_attr2align(expr->type->attr);
		}
	}
	c_value_set_const(res, &c_type_size_t, IR_SIZE_T, val);
	ir_UNREACHABLE();
	// TODO: cleanup dead code ???
	active_ctx->control = old_control;
}

void c_alignof_type(c_value *res, const c_type *type)
{
	ir_val val;

	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type)) {
		yy_error_fmt("invalid application of \"%s\" to incomplete type", "_Alignof");
	}
	val.u64 = c_attr2align(type->attr);
	c_value_set_const(res, &c_type_size_t, IR_SIZE_T, val);
}

void c_alignas_expr(c_dcl *dcl, c_value *expr)
{
	if (!c_value_is_const(expr) || !C_IS_TYPE_INT(expr->type)) {
		yy_error("_Alignas width not an integer constant");
	} else if (!c_valid_alignment(expr)) {
		yy_error("_Alignas value must be a power of two");
	}
	dcl->attr |= c_align2attr(expr->u.val.u64);
}

const c_type *c_typeof_expr(c_value *expr, ir_ref old_control)
{
	ir_UNREACHABLE();
	// TODO: cleanup dead code ???
	active_ctx->control = old_control;
	return expr->type;
}

static c_label *c_new_label(c_name name, c_scope *scope, c_label *label, bool local)
{
	IR_ASSERT(scope);
	if (!scope->list.syms) pp_list_init(&scope->list);
	pp_list_push(&scope->list, name);
	pp_list_push_ptr(&scope->list, (void*)(((uintptr_t)label) | C_POP_LABEL));

	if (local) {
		label = ir_arena_alloc(&c_arena, sizeof(c_label));
		label->is_local = 1;
	} else {
		label = ir_mem_malloc(sizeof(c_label)); // TODO: cache allocatons
		label->is_local = 0;
	}
	label->is_unused = 0;
	label->dst = IR_UNUSED;
	label->src_list = IR_UNUSED;
	label->scope = scope;
	yy_hash.data[name].label = label;
	return label;
}

void c_declare_local_label(c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = yy_hash.data[name].label;
	if (label && label->scope == active_scope) {
		yy_error_fmt("duplicate label declaration \"%s\"", yy_sym2str(name));
		return;
	}

	c_new_label(name, active_scope, label, 1);
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

static void ir_memcpy(ir_ctx *ctx, ir_ref dst, ir_ref src, ir_ref size)
{
	ir_CALL_3(IR_VOID,
		ir_const_func(active_ctx,
			ir_strl(active_ctx, "memcpy", sizeof("memcpy")-1),
			ir_proto_3(active_ctx, 0, IR_ADDR, IR_ADDR, IR_ADDR, IR_SIZE_T)),
		dst, src, size);
}

static void ir_memzero(ir_ctx *ctx, ir_ref dst, ir_ref size)
{
	ir_CALL_3(IR_VOID,
		ir_const_func(active_ctx,
			ir_strl(active_ctx, "memset", sizeof("memset")-1),
			ir_proto_3(active_ctx, 0, IR_ADDR, IR_ADDR, IR_I32, IR_SIZE_T)),
		dst, ir_const_i32(active_ctx, 0), size);
}

ir_ref c_do_nocode(void)
{
	ir_ref old_control = active_ctx->control;
	active_ctx->control = IR_UNUSED;
	ir_BEGIN(IR_UNUSED);
	return old_control;
}

ir_ref c_do_alloca(size_t size, bool zero)
{
	ir_ref size_ref = (size == (size_t)-1) ? IR_UNUSED : ir_const_size_t(active_ctx, size);
	ir_ref ref = ir_ALLOCA(size_ref);
	if (zero) {
		ir_memzero(active_ctx, ref, size_ref);
	}
	return ref;
}

static void c_do_load_bit_field(c_value *val, uint32_t first_bit, uint32_t bits)
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
			ref = ir_SHL(type, ref, ir_const(active_ctx, v, type));
		}
		if (ir_type_size[type] * 8 != bits) {
			v.u64 = ir_type_size[type] * 8 - bits;
			ref = ir_SAR(type, ref, ir_const(active_ctx, v, type));
		}
	} else {
		if (ir_type_size[val->u.type] > ir_type_size[type]) {
			ref = ir_ZEXT(val->u.type, ref);
			type = val->u.type;
		}
		if (ir_type_size[type] * 8 == (first_bit + bits)) {
			if (ir_type_size[type] * 8 != bits) {
				v.u64 = ir_type_size[type] * 8 - bits;
				ref = ir_SHR(type, ref, ir_const(active_ctx, v, type));
			}
		} else if (first_bit == 0 && bits <= 32) { // use AND instead of SHL+SHR if small immediate (AArch64) ???
			v.u64 = (uint64_t)((1UL<<bits)-1);
			ref = ir_AND(type, ref, ir_const(active_ctx, v, type));
		} else {
			v.u64 = ir_type_size[type] * 8 - (first_bit + bits);
			ref = ir_SHL(type, ref, ir_const(active_ctx, v, type));
			if (ir_type_size[type] * 8 != bits) {
				v.u64 = ir_type_size[type] * 8 - bits;
				ref = ir_SHR(type, ref, ir_const(active_ctx, v, type));
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

static void c_do_load_bit_field_packed(c_value *val, uint32_t first_bit, uint32_t bits)
{
	ir_type type = val->u.type;
	ir_ref addr = val->u.ref;
	ir_val shift;
	ir_ref ret, ref;
	uint8_t mask;
	uint32_t orig_bits = bits;

	if (ir_type_size[type] > 1) {
		ret = ir_ZEXT(type, ir_LOAD_U8(addr));
	} else {
		ret = ir_LOAD(type, addr);
	}
	shift.u64 = first_bit;
	if (first_bit) {
		ret = ir_SHR(type, ret, ir_const(active_ctx, shift, type));
		bits -= 8 - first_bit;
		shift.u64 = -(int)first_bit;
	} else {
		bits -= 8;
	}

	while (bits >= 8) {
		addr = ir_ADD_A(addr, ir_const_size_t(active_ctx, 1));
		if (ir_type_size[type] > 1) {
			ref = ir_ZEXT(type, ir_LOAD_U8(addr));
		} else {
			ref = ir_LOAD(type, addr);
		}
		shift.u64 += 8;
		ret = ir_OR(type, ret, ir_SHL(type, ref, ir_const(active_ctx, shift, type)));
		bits -= 8;
	}

	if (bits) {
		addr = ir_ADD_A(addr, ir_const_size_t(active_ctx, 1));
		mask = ((1UL<<bits)-1);
		if (ir_type_size[type] > 1) {
			ref = ir_ZEXT(type, ir_AND_U8(ir_LOAD_U8(addr), ir_const_u8(active_ctx, mask)));
		} else {
			ir_val v;
			v.u64 = mask;
			ref = ir_AND(type, ir_LOAD(type, addr), ir_const(active_ctx, v, type));
		}
		shift.u64 += 8;
		ret = ir_OR(type, ret, ir_SHL(type, ref, ir_const(active_ctx, shift, type)));
	}

	if (IR_IS_TYPE_SIGNED(val->u.type) && val->type->kind != C_TYPE_ENUM) {
		/* sign extend */
		shift.u64 = ir_type_size[val->u.type] * 8 - orig_bits;
		if (shift.u64) {
			ir_ref c = ir_const(active_ctx, shift, val->u.type);
			ret = ir_SHL(val->u.type, ret, c);
			ret = ir_SAR(val->u.type, ret, c);
		}
	}

	val->u.ref = ret;
}

void c_value_rval(c_value *val)
{
	if (c_value_is_lval(val)) {
		IR_ASSERT(val->type->kind != C_TYPE_ARRAY && val->type->kind != C_TYPE_FUNC);
		if (c_value_is_var(val)) {
			val->u.ref = ir_VLOAD(val->u.type, val->u.ref);
		} else if (val->type->kind != C_TYPE_STRUCT && val->type->kind != C_TYPE_UNION) {
			if (!C_IS_BIT_FIELD(val->u.proto)) {
				val->u.ref = ir_LOAD(val->u.type, val->u.ref);
			} else if (!C_IS_BIT_FIELD_PACKED(val->u.proto)) {
				c_do_load_bit_field(val, C_BIT_FIELD_START(val->u.proto), C_BIT_FIELD_SIZE(val->u.proto));
			} else {
				c_do_load_bit_field_packed(val, C_BIT_FIELD_START(val->u.proto), C_BIT_FIELD_SIZE(val->u.proto));
			}
		}
		val->u.op &= ~(C_VAL_LVAL|C_VAL_VAR);
	}
}

static ir_ref c_value_ref(c_value *val)
{
	if (c_value_is_const(val)) {
		ir_type t = (val->type->kind == C_TYPE_ENUM) ? c_type2ir(val->type) : val->u.type;
		return ir_const(active_ctx, val->u.val, t);
	} else {
		if (c_value_is_lval(val)) {
			c_value_rval(val);
		}
		return val->u.ref;
	}
}

static void c_do_trunc(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

// enum support ???
	IR_ASSERT((C_IS_TYPE_INT_OR_PTR(t) || t->kind == C_TYPE_ARRAY)
		&& (C_IS_TYPE_INT_OR_PTR(v->type) || v->type->kind == C_TYPE_ARRAY)
		&& t->size < v->type->size);
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] < ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_TRUNC(type, c_value_ref(v)));
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

static void c_do_bitcast(const c_type *t, ir_type type, c_value *v)
{
	IR_ASSERT(t->size == v->type->size);
	IR_ASSERT(ir_type_size[type] == ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_BITCAST(type, c_value_ref(v)));
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

static void c_do_sext(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT_OR_PTR(t) && C_IS_TYPE_INT_OR_PTR(v->type) && t->size > v->type->size);
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] > ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_SEXT(type, c_value_ref(v)));
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

static void c_do_zext(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT_OR_PTR(t) && C_IS_TYPE_INT_OR_PTR(v->type) && t->size > v->type->size);
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_INT(v->u.type) && ir_type_size[type] > ir_type_size[v->u.type]);
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_ZEXT(type, c_value_ref(v)));
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

static void c_do_fp2int(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_INT(t) && C_IS_TYPE_FP(v->type));
	IR_ASSERT(IR_IS_TYPE_INT(type) && IR_IS_TYPE_FP(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_FP2INT(type, c_value_ref(v)));
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

static void c_do_int2fp(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_FP(t) && C_IS_TYPE_INT(v->type));
	IR_ASSERT(IR_IS_TYPE_FP(type) && IR_IS_TYPE_INT(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_INT2FP(type, c_value_ref(v)));
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

static void c_do_fp2fp(const c_type *t, ir_type type, c_value *v)
{
	ir_val val;

	IR_ASSERT(C_IS_TYPE_FP(t) && C_IS_TYPE_FP(v->type));
	IR_ASSERT(IR_IS_TYPE_FP(type) && IR_IS_TYPE_FP(v->u.type));
	if (c_value_is_ref(v)) {
		c_value_set_rval(v, t, type, ir_FP2FP(type, c_value_ref(v)));
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

static void c_do_cvt(const c_type *t, ir_type type, c_value *v)
{
	if (type != v->u.type) {
		if (IR_IS_TYPE_INT(type)) {
			if (IR_IS_TYPE_INT(v->u.type)) {
				if (ir_type_size[type] < ir_type_size[v->u.type]) {
					c_do_trunc(t, type, v);
				} else if (ir_type_size[type] == ir_type_size[v->u.type]) {
					c_do_bitcast(t, type, v);
				} else if (IR_IS_TYPE_SIGNED(v->u.type)) {
					c_do_sext(t, type, v);
				} else {
					c_do_zext(t, type, v);
				}
			} else if (IR_IS_TYPE_FP(v->u.type)) {
				c_do_fp2int(t, type, v);
			} else {
				IR_ASSERT(0);
			}
		} else if (IR_IS_TYPE_FP(type)) {
			if (IR_IS_TYPE_INT(v->u.type)) {
				c_do_int2fp(t, type, v);
			} else if (IR_IS_TYPE_FP(v->u.type)) {
				c_do_fp2fp(t, type, v);
			} else {
				IR_ASSERT(0);
			}
		} else {
			IR_ASSERT(0);
		}
	} else if (t != v->type) {
		v->type = t;
	}
}

void c_do_addr(c_value *v)
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
	}

	type = c_create_pointer_type(v->type);
	if (c_value_is_ref(v)) {
		if (c_value_is_var(v)) {
			ref = ir_VADDR(v->u.ref);
		} else {
			ref = v->u.ref;
		}
		if (!IR_IS_CONST_REF(ref) || IR_IS_SYM_CONST(active_ctx->ir_base[ref].op)) {
			c_value_set_rval(v, type, IR_ADDR, ref);
		} else {
			c_value_set_const(v, type, IR_ADDR, active_ctx->ir_base[ref].val);
		}
	} else {
		v->type = type; // check type ???
	}
}

static ir_ref c_do_store_bit_field(ir_ref addr, uint32_t first_bit, uint32_t bits, c_value *val)
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

	ret = ref = c_value_ref(val);
	if (val->u.type != type) {
		if (ir_type_size[type] < ir_type_size[val->u.type]) {
			ref = ir_TRUNC(type, ref);
		} else if (ir_type_size[type] == ir_type_size[val->u.type]) {
			ref = ir_BITCAST(type, ref);
		} else {
			ref = ir_ZEXT(type, ref);
		}
	}

	v.u64 = (1UL<<bits)-1;
	ref = ir_AND(type, ref, ir_const(active_ctx, v, type));
	if (val->u.type == type) ret = ref;

	if (first_bit) {
		v.u64 = first_bit;
		ref = ir_SHL(type, ref, ir_const(active_ctx, v, type));
	}

	v.u64 = ~(((1UL<<bits)-1)<<first_bit);
	ir_STORE(
		addr,
		ir_OR(type,
			ir_AND(type,
				ir_LOAD(type, addr),
				ir_const(active_ctx, v, type)),
			ref));

	if (IR_IS_TYPE_SIGNED(val->u.type) && val->type->kind != C_TYPE_ENUM) {
		v.u64 = ir_type_size[val->u.type] * 8 - bits;
		if (v.u64) {
			ir_ref c = ir_const(active_ctx, v, val->u.type);
			ret = ir_SHL(val->u.type, ret, c);
			ret = ir_SAR(val->u.type, ret, c);
		}
	} else if (val->u.type != type) {
		v.u64 = (1UL<<bits)-1;
		ret = ir_AND(val->u.type, ret, ir_const(active_ctx, v, val->u.type));
	}
	return ret;
}

static ir_ref c_do_store_bit_field_packed(ir_ref addr, uint32_t first_bit, uint32_t bits, c_value *val)
{
	ir_type type = (ir_type_size[val->u.type] != 1) ? IR_U8 : val->u.type;
	ir_val shift, mask;
	ir_ref ret, ref;

	ret = ref = c_value_ref(val);
	shift.u64 = first_bit;
	if (first_bit) {
		mask.u64 = ~(((1UL<<(8-first_bit))-1)<<first_bit);
		ref = ir_SHL(val->u.type, ret, ir_const(active_ctx, shift, val->u.type));
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}
		ir_STORE(
			addr,
			ir_OR(type,
				ir_AND(type, ir_LOAD(type, addr), ir_const(active_ctx, mask, type)),
				ref));
		shift.i64 = -(int)first_bit;
		bits -= 8 - first_bit;
	} else {
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ret);
		}
		ir_STORE(addr, ref);
		bits -= 8;
	}

	while (bits >= 8) {
		addr = ir_ADD_A(addr, ir_const_size_t(active_ctx, 1));
		shift.i64 += 8;
		ref = ir_SHR(val->u.type, ret, ir_const(active_ctx, shift, val->u.type));
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}
		ir_STORE(addr, ref);
		bits -= 8;
	}

	if (bits) {
		addr = ir_ADD_A(addr, ir_const_size_t(active_ctx, 1));
		shift.i64 += 8;
		ref = ir_SHR(val->u.type, ret, ir_const(active_ctx, shift, val->u.type));
		if (ir_type_size[val->u.type] != 1) {
			ref = ir_TRUNC_U8(ref);
		}

		mask.u64 = (1UL<<bits)-1;
		ref = ir_AND(type, ref, ir_const(active_ctx, mask, type));

		mask.u64 = ~((1UL<<bits)-1);
		ir_STORE(
			addr,
				ir_OR(type,
				ir_AND(type,
					ir_LOAD(type, addr),
					ir_const(active_ctx, mask, type)),
				ref));
	}

	return ret;
}

static ir_ref c_do_store(c_value *addr, c_value *val)
{
	ir_ref ref;

	if (!C_IS_BIT_FIELD(addr->u.proto)) {
		ref = c_value_ref(val);
		if (c_value_is_var(addr)) {
			ir_VSTORE(addr->u.ref, ref);
		} else {
			ir_STORE(addr->u.ref, ref);
		}
		return ref;
	} else if (!C_IS_BIT_FIELD_PACKED(addr->u.proto)) {
		return c_do_store_bit_field(addr->u.ref,
			C_BIT_FIELD_START(addr->u.proto), C_BIT_FIELD_SIZE(addr->u.proto), val);
	} else {
		return c_do_store_bit_field_packed(addr->u.ref,
			C_BIT_FIELD_START(addr->u.proto), C_BIT_FIELD_SIZE(addr->u.proto), val);
	}
}

/* arg >   0 - means real argument number
 * arg ==  0 - return value
 * arg == -1 - assign
 * arg == -2 - init
 */
static void c_do_check_cvt(const c_type *type, c_value *val, int32_t arg)
{
	const c_type *val_type = val->type;

	if (C_IS_TYPE_NUM(type) || type->kind == C_TYPE_ENUM) {
		if (!C_IS_TYPE_NUM(val_type) && val_type->kind != C_TYPE_ENUM) {
			if (val_type->kind == C_TYPE_POINTER || val_type->kind == C_TYPE_ARRAY || val_type->kind == C_TYPE_FUNC) {
				if (arg < 0) {
					yy_warning("assignment makes integer from pointer without a cast");
					if (arg == -2 && !active_scope) {
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
				uint32_t attr = (val_type->pointer.type->attr & ~type->pointer.type->attr)
					& (C_ATTR_CONST|C_ATTR_VOLATILE|C_ATTR_ATOMIC);
				if (attr) {
					if (attr & C_ATTR_CONST) {
						if (arg < 0) {
							yy_warning_fmt("assignment discards \"%s\" qualifier from pointer target type", "const");
						} else if (arg > 0) {
							yy_warning_fmt("passing argument %d discards \"%s\" qualifier from pointer target type", arg, "const");
						} else {
							yy_warning_fmt("return discards \"%s\" qualifier from pointer target type", "const");
						}
					} else if (attr & C_ATTR_VOLATILE) {
						if (arg < 0) {
							yy_warning_fmt("assignment discards \"%s\" qualifier from pointer target type", "volatile");
						} else if (arg > 0) {
							yy_warning_fmt("passing argument %d discards \"%s\" qualifier from pointer target type", arg, "volatile");
						} else {
							yy_warning_fmt("return discards \"%s\" qualifier from pointer target type", "volatile");
						}
					} else if (attr & C_ATTR_ATOMIC) {
						if (arg < 0) {
							yy_warning_fmt("assignment discards \"%s\" qualifier from pointer target type", "atomic");
						} else if (arg > 0) {
							yy_warning_fmt("passing argument %d discards \"%s\" qualifier from pointer target type", arg, "atomic");
						} else {
							yy_warning_fmt("return discards \"%s\" qualifier from pointer target type", "atomic");
						}
					}
				}
			}
		} else if (val_type->kind == C_TYPE_FUNC
		 && (type->pointer.type->kind == C_TYPE_VOID
		  || c_compatible_types(type->pointer.type, val_type, 0, 1))) {
			/* pass */
		} else if (C_IS_TYPE_NUM(val_type) || val_type->kind == C_TYPE_ENUM) {
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
	} else if ((type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION)
	 && c_compatible_types(type, val_type, 1, 0)) {
		/* pass */
	} else {
		goto incompatible;
	}
	c_do_cvt(type, c_type2ir(type), val);
}

void c_do_cast(const c_type *t, c_value *v)
{
	if (t == v->type) {
	} else if (t->kind == C_TYPE_VOID) {
		c_value_set_rval(v, &c_type_void, IR_VOID, IR_NULL);
	} else if (!C_IS_TYPE_SCALAR_OR_PTR(t)) {
		yy_error("conversion to non-scalar type requested");
	} else if (t->flags & C_TYPE_INCOMPLETE) {
		yy_error("conversion to incomplete type");
	} else if (v->type->kind == C_TYPE_VOID || v->type->kind == C_TYPE_STRUCT || v->type->kind == C_TYPE_UNION) {
		yy_error("conversion of non-scalar type requested");
	} else if (t->kind == C_TYPE_POINTER) {
		if (C_IS_TYPE_FP(v->type)) {
			yy_error("cannot convert floating point value to a pointer");
		} else if (t->size != v->type->size
		 && (C_IS_TYPE_INT(v->type) || v->type->kind == C_TYPE_ENUM)
		 && !c_value_is_const(v)) {
			yy_warning("cast to pointer from integer of different size");
		}
	} else if (v->type->kind == C_TYPE_POINTER || v->type->kind == C_TYPE_ARRAY) {
		if (C_IS_TYPE_FP(t)) {
			yy_error("cannot convert pointer to a floating point");
		} else if (t->size != v->type->size
		 && (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM)
		 && !c_value_is_const(v)) {
			yy_warning("cast from pointer to integer of different size");
		}
	}
	c_value_rval(v);
	if (t->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) {
		/* remove top-level qualifiers */
		c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));
		*type = *t;
		type->attr &= ~(C_ATTR_CONST|C_ATTR_VOLATILE);
		t = type;
	}
	c_do_cvt(t, c_type2ir(t), v);
	v->u.proto = 0; /*reset bit-field */
}

void c_do_post_op(yy_sym sym, c_value *v)
{
	c_value val, tmp;
	ir_type res_type, type;
	ir_val one;
	ir_ref ref;

	if (!c_value_is_lval(v) || !C_IS_TYPE_SCALAR_OR_PTR(v->type)) {
		yy_error_fmt("lvalue required as \"%s\" operand", yy_sym2str(sym));
	} else if (v->type->attr & C_ATTR_CONST) {
		yy_error_fmt("% of read-only location",
			(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
	}
	val = *v;
	c_value_rval(&val);
	res_type = type = val.u.type;
	if (v->type->kind == C_TYPE_POINTER) {
		if (v->type->pointer.type->kind == C_TYPE_VOID) {
			one.u64 = 1;
		} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(v->type->pointer.type)) {
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
		ref = ir_ADD(res_type, val.u.ref, ir_const(active_ctx, one, type));
	} else {
		IR_ASSERT(sym == YY__MINUS_MINUS);
		ref = ir_SUB(res_type, val.u.ref, ir_const(active_ctx, one, type));
	}
	c_value_set_rval(&tmp, val.type, type, ref);
	c_do_store(v, &tmp);
	*v = val;
}

void c_do_pre_op(yy_sym sym, c_value *v)
{
	c_value val;
	ir_type res_type, type;
	ir_val one;

	if (!c_value_is_lval(v) || !C_IS_TYPE_SCALAR_OR_PTR(v->type)) {
		yy_error_fmt("lvalue required as \"%s\" operand", yy_sym2str(sym));
	} else if (v->type->attr & C_ATTR_CONST) {
		yy_error_fmt("% of read-only location",
			(sym == YY__PLUS_PLUS) ? "increment" : "decrement");
	}
	val = *v;
	c_value_rval(&val);
	res_type = type = val.u.type;
	if (v->type->kind == C_TYPE_POINTER) {
		if (v->type->pointer.type->kind == C_TYPE_VOID) {
			one.u64 = 1;
		} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(v->type->pointer.type)) {
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
		val.u.ref = ir_ADD(res_type, val.u.ref, ir_const(active_ctx, one, type));
	} else {
		IR_ASSERT(sym == YY__MINUS_MINUS);
		val.u.ref = ir_SUB(res_type, val.u.ref, ir_const(active_ctx, one, type));
	}
	c_do_store(v, &val);
	*v = val;
}

void c_do_deref(c_value *v)
{
	c_value_rval(v);
	if (v->type->kind != C_TYPE_POINTER && v->type->kind != C_TYPE_ARRAY) {
		if (v->type->kind == C_TYPE_FUNC) return;
		yy_error("invalid type argument of unary \"*\"");
	} else if (v->type->pointer.type->kind == C_TYPE_VOID) {
		yy_error("dereferencing \"void *\" pointer");
	} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(v->type->pointer.type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(v->type->pointer.type->kind), yy_sym2str(v->type->pointer.type->tag));
	}
	if (v->type->pointer.type->kind != C_TYPE_FUNC
	 && v->type->pointer.type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, v->type->pointer.type, c_type2ir(v->type->pointer.type), c_value_ref(v));
	} else {
		c_value_set_rval(v, v->type->pointer.type, c_type2ir(v->type->pointer.type), c_value_ref(v));
	}
}

void c_do_unary_plus(c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(&c_type_i32, IR_I32, v);
		}
	} else if (!C_IS_TYPE_FP(t)) {
		yy_error("invalid type argument of unary \"+\"");
	}
}

void c_do_neg(c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(&c_type_i32, IR_I32, v);
		}
	} else if (!C_IS_TYPE_FP(t)) {
		yy_error("invalid type argument of unary \"-\"");
	}

	if (c_value_is_ref(v)) {
		v->u.ref = ir_NEG(v->u.type, c_value_ref(v));
	} else {
		switch (v->u.type) {
			case IR_I32:
			case IR_U32:    v->u.val.i64 = -v->u.val.i32; break;
			case IR_I64:
			case IR_U64:    v->u.val.i64 = -v->u.val.i64; break;
			case IR_FLOAT:  v->u.val.f = -v->u.val.f; break;
			case IR_DOUBLE: v->u.val.d = -v->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
	}
}

void c_do_not(c_value *v)
{
	const c_type *t = v->type;

	c_value_rval(v);
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(&c_type_i32, IR_I32, v);
		}
	} else {
		yy_error("invalid type argument of unary \"~\"");
	}
	if (c_value_is_ref(v)) {
		v->u.ref = ir_NOT(v->u.type, c_value_ref(v));
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

void c_do_bool_not(c_value *v)
{
	ir_val val;

	c_value_rval(v);
	if (v->type->kind == C_TYPE_VOID || v->type->kind == C_TYPE_STRUCT || v->type->kind == C_TYPE_UNION) {
		yy_error("invalid type argument of unary \"!\"");
	}
	if (c_value_is_ref(v)) {
		if (v->u.type == IR_BOOL) {
			c_value_set_rval(v, &c_type_bool, IR_BOOL, ir_NOT_B(c_value_ref(v)));
		} else {
			val.u64 = 0;
			c_value_set_rval(v, &c_type_bool, IR_BOOL,
				ir_EQ(c_value_ref(v), ir_const(active_ctx, val, v->u.type)));
		}
	} else {
		val.u64 = (v->u.val.u64 == 0);
		c_value_set_const(v, &c_type_bool, IR_BOOL, val);
	}
}

void c_do_array_dim(c_value *v, c_value *dim)
{
	const c_type *type;
	ir_ref ref;

	type = v->type;
	if (type->kind == C_TYPE_ARRAY) {
		IR_ASSERT(type->array.type->size);
		type = type->array.type;
		ref = c_value_ref(v);
	} else if (type->kind == C_TYPE_POINTER) {
		if (type->pointer.type->kind == C_TYPE_VOID) yy_error("dereferencing \"void *\" pointer");
		if ((type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(type->pointer.type->kind), yy_sym2str(type->pointer.type->tag));
		}
		type = type->pointer.type;
		ref = c_value_ref(v);
	} else {
		type = dim->type;
		if (type->kind == C_TYPE_ARRAY) {
			IR_ASSERT(type->array.type->size);
			type = type->array.type;
		} else if (type->kind == C_TYPE_POINTER) {
			if (type->pointer.type->kind == C_TYPE_VOID) yy_error("dereferencing \"void *\" pointer");
			if ((type->pointer.type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type->pointer.type)) {
				yy_error_fmt("invalid use of undefined \"%s %s\"",
					c_type_kind2str(type->pointer.type->kind), yy_sym2str(type->pointer.type->tag));
			}
			type = type->pointer.type;
		} else {
			yy_error("subscripted value is neither array nor pointer");
		}
		/* turn X[Y] into Y[X] */
		ref = c_value_ref(dim);
		dim = v;
	}
	if (!C_IS_TYPE_INT(dim->type) && dim->type->kind != C_TYPE_ENUM) yy_error("array subscript is not an integer");
	c_value_rval(dim);
	if (C_IS_TYPE_SIGNED(dim->type)) {
		if (dim->type->kind != c_type_ssize_t.kind) {
			c_do_cvt(&c_type_ssize_t, IR_SSIZE_T, dim);
		}
		ref = ir_ADD_A(ref, ir_MUL(IR_SSIZE_T, c_value_ref(dim),
				ir_const_ssize_t(active_ctx, type->size)));
		if (type->kind != C_TYPE_ARRAY) {
			c_value_set_lval(v, type, c_type2ir(type), ref);
		} else {
			c_value_set_rval(v, type, c_type2ir(type), ref);
		}
	} else {
		if (dim->type->kind != c_type_size_t.kind) {
			c_do_cvt(&c_type_size_t, IR_SIZE_T, dim);
		}
		ref = ir_ADD_A(ref, ir_MUL(IR_SIZE_T, c_value_ref(dim),
				ir_const_size_t(active_ctx, type->size)));
		if (type->kind != C_TYPE_ARRAY) {
			c_value_set_lval(v, type, c_type2ir(type), ref);
		} else {
			c_value_set_rval(v, type, c_type2ir(type), ref);
		}
	}
}

void c_do_struct_field(c_value *v, c_name field_name)
{
	c_field *field;
	size_t offset;

	if (v->type->kind != C_TYPE_STRUCT && v->type->kind != C_TYPE_UNION) {
		yy_error_fmt("request for member \"%s\" in something not a structure or union", yy_sym2str(field_name));
	} else if ((v->type->flags & C_TYPE_INCOMPLETE)) {
		IR_ASSERT(v->type->tag);
		if (!c_fix_incomplete_type(v->type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(v->type->pointer.type->record.tag));
		}
	}
	field = c_find_struct_field(v->type, field_name, &offset);
	if (!field) {
		if (v->type->record.tag) {
			yy_error_fmt("\"%s %s\" has no member named \"%s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(v->type->record.tag),
				yy_sym2str(field_name));
		} else {
			yy_error_fmt("%s has no member named \"%s\"",
				(v->type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(field_name));
		}
	}
	ir_ref ref = ir_ADD_A(v->u.ref, ir_const_size_t(active_ctx, offset));
	if (field->type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, field->type, c_type2ir(field->type), ref);
		v->u.proto = field->bit_field;
	} else {
		c_value_set_rval(v, field->type, c_type2ir(field->type), ref);
	}
}

void c_do_struct_field_deref(c_value *v, c_name field_name)
{
	c_field *field;
	size_t offset;

	if (v->type->kind != C_TYPE_POINTER && v->type->kind != C_TYPE_ARRAY) {
		yy_error("invalid type argument of \"->\"");
	} else if (v->type->pointer.type->kind != C_TYPE_STRUCT && v->type->pointer.type->kind != C_TYPE_UNION) {
		yy_error_fmt("request for member \"%s\" in something not a structure or union", yy_sym2str(field_name));
	} else if ((v->type->pointer.type->flags & C_TYPE_INCOMPLETE)) {
		IR_ASSERT(v->type->pointer.type->tag);
		if (!c_fix_incomplete_type(v->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(v->type->pointer.type->tag));
		}
	}
	field = c_find_struct_field(v->type->pointer.type, field_name, &offset);
	if (!field) {
		if (v->type->pointer.type->record.tag) {
			yy_error_fmt("\"%s %s\" has no member named \"%s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(v->type->pointer.type->record.tag),
				yy_sym2str(field_name));
		} else {
			yy_error_fmt("%s has no member named \"%s\"",
				(v->type->pointer.type->kind == C_TYPE_STRUCT) ? "struct" : "union",
				yy_sym2str(field_name));
		}
	}
	ir_ref ref = ir_ADD_A(c_value_ref(v), ir_const_size_t(active_ctx, offset));
	if (field->type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, field->type, c_type2ir(field->type), ref);
		v->u.proto = field->bit_field;
	} else {
		c_value_set_rval(v, field->type, c_type2ir(field->type), ref);
	}
}

void c_do_builtin(c_value *val, c_name name, int32_t num_args, c_value *args)
{
	if (name == YY___BUILTIN_VA_START) {
		if (num_args != 1) yy_error("wrong number of arguments in __builtin_va_start() call");
		// TODO: arg type check ???
		ir_VA_START(c_value_ref(&args[0]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_VA_ARG) {
		const c_type *type;
		ir_type t;
		ir_ref ref;

		if (num_args != 2) yy_error("wrong number of arguments in __builtin_va_arg() call");
		type = args[1].type;
		if (type->kind != C_TYPE_POINTER) yy_error("wrong type of 2nd argument of __builtin_va_arg() call");
		type = type->pointer.type;
		if (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION) {
			ir_ref alloca;

			if (type->size > sizeof(void*)) yy_error("long struct arguments not implemented yet"); //???
			t = (type->size <= 4) ? IR_U32 : IR_U64;
			ref = ir_VA_ARG(c_value_ref(&args[0]), t);
			alloca = ir_ALLOCA(ir_const_size_t(active_ctx, type->size));
			ir_STORE(alloca, ref);
			c_value_set_lval(val, type, IR_ADDR, alloca);
		} else {
			t = c_type2ir(type);
			ref = ir_VA_ARG(c_value_ref(&args[0]), t);
			c_value_set_rval(val, type, t, ref);
		}
	} else if (name == YY___BUILTIN_VA_END) {
		if (num_args != 1) yy_error("wrong number of arguments in __builtin_va_end() call");
		ir_VA_END(c_value_ref(&args[0]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_VA_COPY) {
		if (num_args != 2) yy_error("wrong number of arguments in __builtin_va_copy() call");
		ir_VA_COPY(c_value_ref(&args[0]), c_value_ref(&args[0]));
		c_value_set_rval(val, &c_type_void, IR_VOID, IR_UNUSED);
	} else if (name == YY___BUILTIN_EXPECT) {
		if (num_args != 2) yy_error("wrong number of arguments in __builtin_expect() call");
		// TODO: set IF propability ???
		c_value_set_rval(val, args[0].type, args[0].u.type, c_value_ref(&args[0]));
	} else {
		IR_ASSERT(0);
	}
	return;
}

static ir_ref c_do_convert_builtin(c_value *func, int32_t num_args, ir_ref *arg_refs)
{
	if (c_value_is_ref(func)) {
		const ir_insn *func_insn = &active_ctx->ir_base[func->u.ref];
		size_t name_len;
		const char *name = ir_get_strl(active_ctx, func_insn->val.name, &name_len);
		c_name sym_name = yy_hash_find(name, name_len);
		if (sym_name == YY_ABS) {
			return ir_ABS_I32(arg_refs[0]);
		} else if (sym_name == YY_LABS) {
			return ir_ABS_I64(arg_refs[0]); //???
		} else if (sym_name == YY_FABS) {
			return ir_ABS_D(arg_refs[0]);
		} else if (sym_name == YY_FABSF) {
			return ir_ABS_F(arg_refs[0]);
		}
	}

	return 0;
}

static ir_ref ir_inline_call(ir_ctx *ctx, ir_ctx *func_ctx, uint32_t num_args, ir_ref *args)
{
	ir_ref *buf = alloca(sizeof(ir_ref) * (func_ctx->consts_count + func_ctx->insns_count * 2 - 1));
	ir_ref *xlat = buf + func_ctx->consts_count - 1;
	ir_ref *xlat2 = xlat + func_ctx->insns_count;
	ir_ref ret = IR_UNUSED;
	ir_ref i, j, op1, op2, op3;
	ir_ref start = IR_UNUSED, block_begin = IR_UNUSED;
	bool has_alloca;
	ir_insn *insn;
	bool add_phi = 0;
	ir_list bp_list;

	/* Copy costants */
	for (i = 1 - func_ctx->consts_count, insn = func_ctx->ir_base + i; i < IR_TRUE; i++, insn++) {
		ir_val val = insn->val;
		uint32_t optx = insn->optx;
		ir_op op = optx & IR_OPT_OP_MASK;

		if (op == IR_FUNC || op == IR_SYM || op == IR_STR) {
			size_t len;
			const char *str = ir_get_strl(func_ctx, val.str, &len);

			val.str = ir_strl(ctx, str, len);
		}
		if (op == IR_FUNC || op == IR_FUNC_ADDR) {
			ir_ref proto = insn->proto;

			if (proto) {
				size_t len;
				const char *str = ir_get_strl(func_ctx, proto, &len);

				proto = ir_strl(ctx, str, len);
				optx = IR_OPTX(op, IR_OPT_TYPE(optx), proto);
			}
		}
		xlat[i] = ir_const_ex(ctx, val, IR_OPT_TYPE(optx), optx);
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
		xlat2[i] = xlat[i] = args[i - 2];
		insn++;
		i++;
	}
	j = i;

	/* Check if the inlined function expands stack through VAR or ALLOCA */
	while (i < func_ctx->insns_count) {
		ir_ref n = insn->inputs_count;
		if (n <= 3) {
			if (insn->op == IR_VAR || insn->op == IR_ALLOCA) {
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
	if (has_alloca) {
		ir_ref end = ir_emit1(ctx, IR_END, ctx->control);
		start = ir_emit1(ctx, IR_BEGIN, end);
		block_begin = xlat2[1] = xlat[1] = ctx->control = ir_emit1(ctx, IR_OPT(IR_BLOCK_BEGIN, IR_ADDR), start);
	} else {
		start = xlat2[1] = xlat[1] = ctx->control;
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
				IR_ASSERT(op != IR_PROTO);
				xlat2[i] = xlat[i] = ir_fold(ctx, insn->opt, op1, op2, op3);
			} else if (op == IR_RETURN) {
				ctx->control = op1;
				if (insn->op2) {
					op2 = insn->op2;
					IR_ASSERT(op2 < i);
					op2 = xlat[op2];
					ir_END_PHI_list(ret, op2);
					add_phi = 1;
				} else {
					ir_END_list(ret);
				}
				ctx->control = IR_UNUSED;
				xlat2[i] = xlat[i] = IR_UNUSED;
			} else if (op == IR_UNREACHABLE) {
				ctx->control = IR_UNUSED;
				_ir_UNREACHABLE(ctx);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_IJMP) {
				ctx->control = IR_UNUSED;
				_ir_IJMP(ctx, op2);
				xlat2[i] = xlat[i] = ctx->control;
				ctx->control = IR_UNUSED;
			} else if (op == IR_BEGIN) {
				ctx->control = IR_UNUSED;
				_ir_BEGIN(ctx, op1);
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
				size_t len;
				const char *str = ir_get_strl(func_ctx, op2, &len);

				op2 = ir_strl(ctx, str, len);
				if (insn->op1 == 1) op1 = start;
				xlat2[i] = xlat[i] = ir_emit(ctx, insn->opt, op1, op2, op3);
			} else {
				IR_ASSERT(op != IR_PROTO);
				IR_ASSERT(op != IR_VA_START && op != IR_VA_ARG && op != IR_VA_COPY && op != IR_VA_END);
				xlat2[i] = xlat[i] = ir_emit(ctx, insn->opt, op1, op2, op3);
			}
			insn++;
			i++;
		} else if (op == IR_TAILCALL) {
			IR_ASSERT(op != IR_TAILCALL);
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
	}

	/* Add BLOCK_END if necessary */
	if (has_alloca) {
		ctx->control = ir_emit2(ctx, IR_BLOCK_END, ctx->control, block_begin);
	}

	return ret;
}

void c_do_call(c_value *func, int32_t num_args, c_value *args)
{
	const c_type *func_type, *ret_type;
	ir_type t;
	ir_ref ref, ret_struct = IR_UNUSED;
	ir_ref *arg_refs = NULL;

	c_value_rval(func);
	if (func->type->kind == C_TYPE_FUNC) {
		func_type = func->type;
	} else if (func->type->kind == C_TYPE_POINTER
	 && func->type->pointer.type->kind == C_TYPE_FUNC) {
		func_type = func->type->pointer.type;
	} else {
		if (yy_flags & PP_EVAL_EXPRESSION) {
			ir_val val;
			val.u64 = 0;
			c_value_set_const(func, &c_type_i32, IR_I32, val);
			return;
		}
		yy_error("called object is not a function or function pointer");
	}
	IR_ASSERT(func->u.type == IR_ADDR);
	ret_type = func_type->func.ret_type;
	if (ret_type->kind == C_TYPE_STRUCT || ret_type->kind == C_TYPE_UNION) {
		if (ret_type->size <= sizeof(void*)) {
			ret_struct = ir_ALLOCA(ir_const_size_t(active_ctx, ret_type->size));
			ret_type = (ret_type->size <= 4) ? &c_type_u32 : &c_type_u64;
		} else {
			yy_error("long struct return not implemented yet"); //???
		}
	}
	if (num_args != func_type->func.num_params) {
		if (func_type->func.num_params < 0) {
			/* pass */
		} else if (num_args < func_type->func.num_params) {
			yy_error("too few arguments"); // TODO: to function "%s" ???
		} else if (!(func_type->attr & C_ATTR_VARIADIC)) {
			yy_error("too many arguments"); // TODO: to function "%s" ???
		}
	}
	if ((func_type->func.ret_type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(func_type->func.ret_type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(func_type->func.ret_type->kind), yy_sym2str(func_type->func.ret_type->tag));
	}
	t = c_type2ir(ret_type);
	if (num_args > 0) {
		int i;

		arg_refs = alloca(sizeof(ir_ref) * num_args);
		for (i = 0; i < num_args; i++) {
			c_value_rval(&args[i]);
			if (i < func_type->func.num_params) {
				if (func_type->func.params[i].type != args[i].type) {
					c_do_check_cvt(func_type->func.params[i].type, &args[i], i + 1);
				}
			} else {
				if (args[i].type->kind == C_TYPE_FLOAT) {
					c_do_fp2fp(&c_type_double, IR_DOUBLE, &args[i]);
				} else if ((C_IS_TYPE_INT(args[i].type) || args[i].type->kind == C_TYPE_ENUM)
				 && args[i].type->size < 4) {
					if (C_IS_TYPE_SIGNED(args[i].type)) {
						c_do_sext(&c_type_i32, IR_I32, &args[i]);
					} else {
						c_do_zext(&c_type_u32, IR_U32, &args[i]);
					}
				}
			}
			if (args[i].type->kind == C_TYPE_STRUCT || args[i].type->kind == C_TYPE_UNION) {
				if (args[i].type->size <= sizeof(void*)) {
					if (args[i].type->size <= 4) {
						arg_refs[i] = ir_LOAD_U32(args[i].u.ref);
					} else {
						arg_refs[i] = ir_LOAD_U64(args[i].u.ref);
					}
				} else {
					yy_error("long struct arguments not implemented yet"); //???
				}
			} else {
				arg_refs[i] = c_value_ref(&args[i]);
			}
		}
	}
	if (func->u.op & C_VAL_INLINE) {
		ref = ir_inline_call(active_ctx, (ir_ctx*)func->u.val.ptr, num_args, arg_refs);
	} else if (!(func->u.op & C_VAL_BUILTIN)) {
		ref = ir_CALL_N(t, c_value_ref(func), num_args, arg_refs);
		if (func->type->attr & C_ATTR_NORETURN) {
			ir_val val;

			ir_UNREACHABLE();
			ir_BEGIN(IR_UNUSED);
			ret_type = func_type->func.ret_type;
			t = c_type2ir(ret_type);
			val.u64 = 0;
			c_value_set_const(func, ret_type, t, val);
			return;
		}
	} else {
		ref = c_do_convert_builtin(func, num_args, arg_refs);
		if (!ref) {
			ref = ir_CALL_N(t, c_value_ref(func), num_args, arg_refs);
		}
	}
	ret_type = func_type->func.ret_type;
	t = c_type2ir(ret_type);
	if (ret_type->kind == C_TYPE_STRUCT || ret_type->kind == C_TYPE_UNION) {
		if (ret_type->size <= sizeof(void*)) {
			ir_STORE(ret_struct, ref);
			c_value_set_lval(func, ret_type, t, ret_struct);
		} else {
			yy_error("long struct return not implemented yet"); //???
		}
	} else {
		c_value_set_rval(func, ret_type, t, ref);
	}
}

static const c_type *c_common_type(yy_sym sym, c_value *op1, c_value *op2)
{
	const c_type *op1_type = op1->type;
	const c_type *op2_type = op2->type;
	c_type_kind t1 = op1_type->kind;
	c_type_kind t2 = op2_type->kind;

	if (t1 == C_TYPE_POINTER || t1 == C_TYPE_ARRAY) {
		if (sym == YY__PLUS) {
			if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
				if (op2_type->size != op1_type->size) {
					c_do_cvt(&c_type_size_t, IR_SIZE_T, op2);
				}
				// array -> pointer ???
				return op1_type;
			}
		} else if (sym == YY__MINUS) {
			if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
				if (op2_type->size != op1_type->size) {
					c_do_cvt(&c_type_size_t, IR_SIZE_T, op2);
				}
				// array -> pointer ???
				return op1_type;
			} else if ((t2 == C_TYPE_POINTER || t2 == C_TYPE_ARRAY)
			 && c_compatible_types(op1_type->pointer.type, op2_type->pointer.type, 1, 0)) {
				// array -> pointer ???
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
				// TODO: select best type ???
				return op1_type;
			} else if (C_IS_TYPE_INT(op2_type)) {
				if (c_value_is_const(op2) && op2->u.val.u64 == 0) return op1_type;
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
					c_do_cvt(&c_type_size_t, IR_SIZE_T, op1);
				}
				return op2_type;
			}
		} else if (sym == YY__LESS || sym == YY__LESS_EQUAL || sym == YY__GREATER || sym == YY__GREATER_EQUAL
			|| sym == YY__EQUAL_EQUAL || sym == YY__BANG_EQUAL || YY__COLON) {
			if (C_IS_TYPE_INT(op1_type)) {
				if (c_value_is_const(op1) && op1->u.val.u64 == 0) return op2_type;
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
			if (sym == YY__COLON) return c_create_pointer_type(op1_type);
			return op1_type;
		}
		return NULL;
	} else if (C_IS_TYPE_KIND_FP(t1)) {
		if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR) {
			return NULL;
		} else if (t1 == t2) {
			return op1_type;
		} else if (C_IS_TYPE_KIND_FP(t2)) {
			if (op1_type->size >= op2_type->size) {
				c_do_fp2fp(op1_type, op1->u.type, op2);
				return op1_type;
			} else {
				c_do_fp2fp(op2_type, op2->u.type, op1);
				return op2_type;
			}
		} else if (C_IS_TYPE_KIND_INT(t2) || t2 == C_TYPE_ENUM) {
			c_do_int2fp(op1_type, op1->u.type, op2);
			return op1_type;
		}
	} else if (C_IS_TYPE_KIND_FP(t2)) {
		if (sym == YY__PERCENT || sym == YY__AND || sym == YY__UPARROW || sym == YY__BAR) {
			return NULL;
		} else if (C_IS_TYPE_KIND_INT(t1) || t1 == C_TYPE_ENUM) {
			c_do_int2fp(op2_type, op2->u.type, op1);
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
				if (op1_type->size != 8 || C_IS_TYPE_KIND_SIGNED(t1)) c_do_cvt(&c_type_u64, IR_U64, op1);
				if (op2_type->size != 8 || C_IS_TYPE_KIND_SIGNED(t2)) c_do_cvt(&c_type_u64, IR_U64, op2);
				return &c_type_u64;
			} else {
				if (op1_type->size != 8 || C_IS_TYPE_KIND_UNSIGNED(t1)) c_do_cvt(&c_type_i64, IR_I64, op1);
				if (op2_type->size != 8 || C_IS_TYPE_KIND_UNSIGNED(t2)) c_do_cvt(&c_type_i64, IR_I64, op2);
				return &c_type_i64;
			}
		} else {
			if ((op1_type->size == 4 && C_IS_TYPE_KIND_UNSIGNED(t1)
			  && (!C_IS_BIT_FIELD(op1->u.proto) || C_BIT_FIELD_SIZE(op1->u.proto) >= 32))
			 || (sym != YY__LESS_LESS && sym != YY__GREATER_GREATER
			  && op2_type->size == 4 && C_IS_TYPE_KIND_UNSIGNED(t2)
			  && (!C_IS_BIT_FIELD(op2->u.proto) || C_BIT_FIELD_SIZE(op2->u.proto) >= 32))) {
				if (op1_type->size != 4 || C_IS_TYPE_KIND_SIGNED(t1)) c_do_cvt(&c_type_u32, IR_U32, op1);
				if (op2_type->size != 4 || C_IS_TYPE_KIND_SIGNED(t2)) c_do_cvt(&c_type_u32, IR_U32, op2);
				return &c_type_u32;
			} else {
				if (op1_type->size != 4 || C_IS_TYPE_KIND_UNSIGNED(t1)) c_do_cvt(&c_type_i32, IR_I32, op1);
				if (op2_type->size != 4 || C_IS_TYPE_KIND_UNSIGNED(t2)) c_do_cvt(&c_type_i32, IR_I32, op2);
				return &c_type_i32;
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

static void c_do_add(const c_type *type, c_value *op1, c_value *op2)
{
	ir_val val;
	ir_ref ref;
	size_t element_size;

	if (op1->type->kind == C_TYPE_POINTER || op1->type->kind == C_TYPE_ARRAY) {
		if (op1->type->pointer.type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op1->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(op1->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op1->type->pointer.type->kind), yy_sym2str(op1->type->pointer.type->tag));
		} else {
			element_size = op1->type->pointer.type->size;
		}
		IR_ASSERT(C_IS_TYPE_INT(op2->type) || op2->type->kind == C_TYPE_ENUM);
		if (c_value_is_const(op1) && c_value_is_const(op2)) {
			val.addr = op1->u.val.addr + op2->u.val.u64 * element_size;
			c_value_set_const(op1, op1->type, IR_ADDR, val);
		} else {
			if (C_IS_TYPE_SIGNED(op2->type)) {
				if (op2->type->kind != c_type_ssize_t.kind) {
					c_do_cvt(&c_type_ssize_t, IR_SSIZE_T, op2);
				}
				if (element_size == 1) {
					ref = c_value_ref(op2);
				} else {
					ref = ir_MUL(IR_SSIZE_T, c_value_ref(op2),
						ir_const_ssize_t(active_ctx, element_size));
				}
			} else {
				if (op2->type->kind != c_type_size_t.kind) {
					c_do_cvt(&c_type_size_t, IR_SIZE_T, op2);
				}
				if (element_size == 1) {
					ref = c_value_ref(op2);
				} else {
					ref = ir_MUL(IR_SIZE_T, c_value_ref(op2),
						ir_const_size_t(active_ctx, element_size));
				}
			}
			ref = ir_ADD_A(c_value_ref(op1), ref);
			c_value_set_rval(op1, type, IR_ADDR, ref);
		}
	} else if (op2->type->kind == C_TYPE_POINTER || op2->type->kind == C_TYPE_ARRAY) {
		if (op2->type->pointer.type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op2->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(op2->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op2->type->pointer.type->kind), yy_sym2str(op2->type->pointer.type->tag));
		} else {
			element_size = op2->type->pointer.type->size;
		}
		IR_ASSERT(C_IS_TYPE_INT(op1->type) || op1->type->kind == C_TYPE_ENUM);
		if (c_value_is_const(op1) && c_value_is_const(op2)) {
			val.addr = op2->u.val.addr + op1->u.val.u64 * element_size;
			c_value_set_const(op1, op2->type, IR_ADDR, val);
		} else {
			if (C_IS_TYPE_SIGNED(op1->type)) {
				if (op1->type->kind != c_type_ssize_t.kind) {
					c_do_cvt(&c_type_ssize_t, IR_SSIZE_T, op1);
				}
				if (element_size == 1) {
					ref = c_value_ref(op1);
				} else {
					ref = ir_MUL(IR_SSIZE_T, c_value_ref(op1),
						ir_const_ssize_t(active_ctx, element_size));
				}
			} else {
				if (op1->type->kind != c_type_size_t.kind) {
					c_do_cvt(&c_type_size_t, IR_SIZE_T, op1);
				}
				if (element_size == 1) {
					ref = c_value_ref(op1);
				} else {
					ref = ir_MUL(IR_SIZE_T, c_value_ref(op1),
						ir_const_size_t(active_ctx, element_size));
				}
			}
			ref = ir_ADD_A(c_value_ref(op2), ref);
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
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);
		ref = ir_ADD(t, c_value_ref(op1), c_value_ref(op2));
		c_value_set_rval(op1, type, t, ref);
	}
}

static void c_do_sub(const c_type *type, c_value *op1, c_value *op2)
{
	ir_val val;
	ir_ref ref;
	size_t element_size;

	if (op1->type->kind == C_TYPE_POINTER || op1->type->kind == C_TYPE_ARRAY) {
		if (op1->type->pointer.type->kind == C_TYPE_VOID) {
			element_size = 1;
		} else if ((op1->type->pointer.type->flags & C_TYPE_INCOMPLETE)
		 && !c_fix_incomplete_type(op1->type->pointer.type)) {
			yy_error_fmt("invalid use of undefined \"%s %s\"",
				c_type_kind2str(op1->type->pointer.type->kind), yy_sym2str(op1->type->pointer.type->tag));
		} else {
			element_size = op1->type->pointer.type->size;
		}
		if (op2->type->kind == C_TYPE_POINTER || op2->type->kind == C_TYPE_ARRAY) {
			IR_ASSERT(op1->type->pointer.type->size == op2->type->pointer.type->size);
			if (c_value_is_const(op1) && c_value_is_const(op2)) {
				val.i64 = (op1->u.val.addr - op2->u.val.addr) / element_size;
				c_value_set_const(op1, &c_type_ssize_t, IR_SSIZE_T, val);
			 } else {
				ref = ir_SUB_A(c_value_ref(op1), c_value_ref(op2));
				if (element_size != 1) {
					ref = ir_DIV(IR_SSIZE_T, ref,
						ir_const_ssize_t(active_ctx, element_size));
				}
				type = &c_type_ssize_t;
				c_value_set_rval(op1, type, IR_SSIZE_T, ref);
			 }
		} else {
			IR_ASSERT(C_IS_TYPE_INT(op2->type) || op2->type->kind == C_TYPE_ENUM);
			if (c_value_is_const(op1) && c_value_is_const(op2)) {
				val.addr = op1->u.val.addr - op2->u.val.u64 * element_size;
				c_value_set_const(op1, op1->type, IR_ADDR, val);
			} else {
				if (C_IS_TYPE_SIGNED(op2->type)) {
					if (op2->type->kind != c_type_ssize_t.kind) {
						c_do_cvt(&c_type_ssize_t, IR_SSIZE_T, op2);
					}
					if (element_size == 1) {
						ref = c_value_ref(op2);
					} else {
						ref = ir_MUL(IR_SSIZE_T, c_value_ref(op2),
							ir_const_ssize_t(active_ctx, element_size));
					}
				} else {
					if (op2->type->kind != c_type_size_t.kind) {
						c_do_cvt(&c_type_size_t, IR_SIZE_T, op2);
					}
					if (element_size == 1) {
						ref = c_value_ref(op2);
					} else {
						ref = ir_MUL(IR_SIZE_T, c_value_ref(op2),
							ir_const_size_t(active_ctx, element_size));
					}
				}
				ref = ir_SUB_A(c_value_ref(op1), ref);
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
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);
		ref = ir_SUB(t, c_value_ref(op1), c_value_ref(op2));
		c_value_set_rval(op1, type, t, ref);
	}
}

static void c_do_mul(const c_type *type, c_value *op1, c_value *op2)
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
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		c_value_set_rval(op1, type, t, ir_MUL(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_div(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2) && (op2->u.val.u64 != 0 || !active_scope)) {
		ir_val val;

		if (IR_IS_TYPE_INT(op1->u.type) && c_value_is_const(op2) && op2->u.val.u64 == 0) {
			yy_error("division by zero");
		}
		switch (op1->u.type) {
			case IR_I32:    val.i64 = op1->u.val.i32 / op2->u.val.i32; break;
			case IR_U32:    val.u64 = op1->u.val.u32 / op2->u.val.u32; break;
			case IR_I64:    val.i64 = op1->u.val.i64 / op2->u.val.i64; break;
			case IR_U64:    val.u64 = op1->u.val.u64 / op2->u.val.u64; break;
			case IR_FLOAT:  val.f = op1->u.val.f / op2->u.val.f; val.u32_hi = 0; break;
			case IR_DOUBLE: val.d = op1->u.val.d / op2->u.val.d; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		if (c_value_is_const(op2) && op2->u.val.u64 == 0) {
			yy_warning("division by zero");
		}
		c_value_set_rval(op1, type, t, ir_DIV(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_mod(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2) && (op2->u.val.u64 != 0 || !active_scope)) {
		ir_val val;

		if (c_value_is_const(op2) && op2->u.val.u64 == 0) {
			yy_error("division by zero");
		}
		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 % op2->u.val.i32; break;
			case IR_U32: val.u64 = op1->u.val.u32 % op2->u.val.u32; break;
			case IR_I64: val.i64 = op1->u.val.i64 % op2->u.val.i64; break;
			case IR_U64: val.u64 = op1->u.val.u64 % op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		if (c_value_is_const(op2) && op2->u.val.u64 == 0) {
			yy_warning("division by zero");
		}
		c_value_set_rval(op1, type, t, ir_MOD(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_shl(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32: val.i64 = (int32_t)(op1->u.val.u32 << op2->u.val.u32); break;
			case IR_U32: val.u64 = op1->u.val.u32 << op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 << op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);
		c_value_set_rval(op1, type, t, ir_SHL(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_shr(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32: val.i64 = op1->u.val.i32 >> op2->u.val.i32; break;
			case IR_U32: val.u64 = op1->u.val.u32 >> op2->u.val.u32; break;
			case IR_I64: val.i64 = op1->u.val.i64 >> op2->u.val.i64; break;
			case IR_U64: val.u64 = op1->u.val.u64 >> op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		if (C_IS_TYPE_SIGNED(type)) {
			c_value_set_rval(op1, type, t, ir_SAR(t, c_value_ref(op1), c_value_ref(op2)));
		} else {
			c_value_set_rval(op1, type, t, ir_SHR(t, c_value_ref(op1), c_value_ref(op2)));
		}
	}
}

static void c_do_and(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32:
			case IR_U32: val.u64 = op1->u.val.u32 & op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 & op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		c_value_set_rval(op1, type, t, ir_AND(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_xor(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32:
			case IR_U32: val.u64 = op1->u.val.u32 ^ op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 ^ op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		c_value_set_rval(op1, type, t, ir_XOR(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_or(const c_type *type, c_value *op1, c_value *op2)
{
	if (c_value_is_const(op1) && c_value_is_const(op2)) {
		ir_val val;

		switch (op1->u.type) {
			case IR_I32:
			case IR_U32: val.u64 = op1->u.val.u32 | op2->u.val.u32; break;
			case IR_I64:
			case IR_U64: val.u64 = op1->u.val.u64 | op2->u.val.u64; break;
			default: IR_ASSERT(0); return;
		}
		c_value_set_const(op1, type, c_type2ir(type), val);
	} else {
		ir_type t = c_type2ir(type);

		c_value_set_rval(op1, type, t, ir_OR(t, c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_lt(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_LT(c_value_ref(op1), c_value_ref(op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_ULT(c_value_ref(op1), c_value_ref(op2)));
		}
	}
}

static void c_do_gt(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_GT(c_value_ref(op1), c_value_ref(op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_UGT(c_value_ref(op1), c_value_ref(op2)));
		}
	}
}

static void c_do_le(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_LE(c_value_ref(op1), c_value_ref(op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_ULE(c_value_ref(op1), c_value_ref(op2)));
		}
	}
}

static void c_do_ge(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		if (C_IS_TYPE_SIGNED(type) || C_IS_TYPE_FP(type)) {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_GE(c_value_ref(op1), c_value_ref(op2)));
		} else {
			c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_UGE(c_value_ref(op1), c_value_ref(op2)));
		}
	}
}

static void c_do_eq(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_EQ(c_value_ref(op1), c_value_ref(op2)));
	}
}

static void c_do_ne(const c_type *type, c_value *op1, c_value *op2)
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
	} else {
		c_value_set_rval(op1, &c_type_bool, IR_BOOL, ir_NE(c_value_ref(op1), c_value_ref(op2)));
	}
}

void c_do_binary_op(yy_sym sym, c_value *op1, c_value *op2)
{
	const c_type *type;

	c_value_rval(op1);
	c_value_rval(op2);
	type = c_common_type(sym, op1, op2);
	if (!type) yy_error_fmt("invalid operands to binary \"%s\"", yy_sym2str(sym));
	switch (sym) {
		case YY__PLUS:            c_do_add(type, op1, op2); break;
		case YY__MINUS:           c_do_sub(type, op1, op2); break;
		case YY__STAR:            c_do_mul(type, op1, op2); break;
		case YY__SLASH:           c_do_div(type, op1, op2); break;
		case YY__PERCENT:         c_do_mod(type, op1, op2); break;
		case YY__LESS_LESS:       c_do_shl(type, op1, op2); break;
		case YY__GREATER_GREATER: c_do_shr(type, op1, op2); break;
		case YY__AND:             c_do_and(type, op1, op2); break;
		case YY__UPARROW:         c_do_xor(type, op1, op2); break;
		case YY__BAR:             c_do_or(type, op1, op2); break;
		case YY__LESS:            c_do_lt(type, op1, op2); break;
		case YY__GREATER:         c_do_gt(type, op1, op2); break;
		case YY__LESS_EQUAL:      c_do_le(type, op1, op2); break;
		case YY__GREATER_EQUAL:   c_do_ge(type, op1, op2); break;
		case YY__EQUAL_EQUAL:     c_do_eq(type, op1, op2); break;
		case YY__BANG_EQUAL:      c_do_ne(type, op1, op2); break;
		default: IR_ASSERT(0);
	}
}

void c_do_assign_op(yy_sym sym, c_value *op1, c_value *op2)
{
	if (!c_value_is_lval(op1) || op1->type->kind == C_TYPE_FUNC) {
		yy_error("lvalue required as left operand of assignment");
	}
	switch (sym) {
		case YY__EQUAL:                 break;
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
	if (sym != YY__EQUAL) {
		c_value tmp = *op1;
		c_value_rval(&tmp);
		c_do_binary_op(sym, &tmp, op2);
		*op2 = tmp;
	} else {
		c_value_rval(op2);
	}
	if (op1->type != op2->type) c_do_check_cvt(op1->type, op2, -1);
	if (op1->type->attr & C_ATTR_CONST) yy_error_fmt("%s of read-only location", "assignment");
	if (C_IS_TYPE_SCALAR_OR_PTR(op1->type)) {
		ir_ref ref = c_do_store(op1, op2);

		if (!IR_IS_CONST_REF(ref) || IR_IS_SYM_CONST(active_ctx->ir_base[ref].op)) {
			c_value_set_rval(op1, op1->type, op1->u.type, ref);
		} else {
			c_value_set_const(op1, op1->type, op1->u.type, active_ctx->ir_base[ref].val);
		}
	} else {
		IR_ASSERT(op1->type->size == op2->type->size);
		if (op1->type->size) {
			ir_memcpy(active_ctx, op1->u.ref, c_value_ref(op2), ir_const_size_t(active_ctx, op2->type->size));
		}
	}
}


static void c_do_bool(c_value *res, c_value *op1)
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
			ir_NE(c_value_ref(op1), ir_const(active_ctx, val, op1->u.type)));
	}
}

ir_ref c_do_bool_and_start(c_value *op1)
{
	if (op1->type->kind == C_TYPE_VOID || op1->type->kind == C_TYPE_STRUCT || op1->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	}
	if (c_value_is_const(op1)) {
		if (c_value_is_true(op1)) return IR_UNUSED;
		c_dead_code = 1;
	}
	c_do_bool(op1, op1);
	ir_ref ref = ir_IF(c_value_ref(op1));
	ir_IF_TRUE(ref);
	return ref;
}

void c_do_bool_and_end(c_value *op1, c_value *op2, ir_ref if_ref)
{
	ir_val val;

	if (op2->type->kind == C_TYPE_VOID || op2->type->kind == C_TYPE_STRUCT || op2->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	}
	if (if_ref) {
		ir_ref ref;

		c_do_bool(op2, op2);
		ref = c_value_ref(op2);
		ir_MERGE_WITH_EMPTY_FALSE(if_ref);
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
		c_do_bool(op1, op2);
	}
}

ir_ref c_do_bool_or_start(c_value *op1)
{
	if (op1->type->kind == C_TYPE_VOID || op1->type->kind == C_TYPE_STRUCT || op1->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	}
	if (c_value_is_const(op1)) {
		if(!c_value_is_true(op1)) return IR_UNUSED;
		c_dead_code = 1;
	}
	c_do_bool(op1, op1);
	ir_ref ref = ir_IF(c_value_ref(op1));
	ir_IF_FALSE(ref);
	return ref;
}

void c_do_bool_or_end(c_value *op1, c_value *op2, ir_ref if_ref)
{
	ir_val val;

	if (op2->type->kind == C_TYPE_VOID || op2->type->kind == C_TYPE_STRUCT || op2->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	}
	if (if_ref) {
		ir_ref ref;

		c_do_bool(op2, op2);
		ref = c_value_ref(op2);
		ir_MERGE_WITH_EMPTY_TRUE(if_ref);
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
		c_do_bool(op1, op2);
	}
}

void c_do_cond_op(c_value *cond, c_value *op1, c_value *op2)
{
	const c_type *type;

	if (op1->type->kind == op2->type->kind && op1->type->kind == C_TYPE_VOID) return;
	type = c_common_type(YY__COLON, op1, op2);
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
				c_type *t = ir_arena_alloc(&c_arena, sizeof(c_type));
				*t = *type;
				t->pointer.type = ir_arena_alloc(&c_arena, sizeof(c_type));
				*((c_type*)(t->pointer.type)) = *type->pointer.type;
				((c_type*)(t->pointer.type))->attr |= (t1->attr | t2->attr) & (C_ATTR_CONST|C_ATTR_VOLATILE);
				type = t;
		    }
		}
		if (op1->type != type) c_do_cvt(type, c_type2ir(type), op1);
		if (op2->type != type) c_do_cvt(type, c_type2ir(type), op2);
	}
	if (c_value_is_const(cond)) {
		if (c_value_is_true(cond)) {
			if (c_value_is_set(op1)) {
				*cond = *op1;
			}
		} else {
			*cond = *op2;
		}
	} else if (type != &c_type_void) {
		ir_type t = c_type2ir(type);
#if 1
		c_value_set_rval(cond, type, t, ir_PHI_2(t, c_value_ref(op1), c_value_ref(op2)));
#else
		c_value_set_rval(cond, type, t, ir_COND(t, c_value_ref(cond), c_value_ref(op1), c_value_ref(op2)));
#endif
	} else {
		c_value_set_rval(cond, type, IR_VOID, IR_UNUSED);
	}
}

void c_do_statement_expression(c_scope *scope)
{
	if (!active_func_scope) yy_error("statement expression allowed only inside a function");
	c_push_scope(scope);
}

ir_ref c_do_if(c_value *cond)
{
	ir_ref ref;

	if (cond->type->kind == C_TYPE_VOID || cond->type->kind == C_TYPE_STRUCT || cond->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	} else if (C_IS_TYPE_FP(cond->type)) {
		ir_val val;
		val.u64 = 0;
		ref = ir_NE(c_value_ref(cond), ir_const(active_ctx, val, cond->u.type));
	} else {
		ref = c_value_ref(cond);
	}
	if (IR_IS_CONST_REF(ref) && !ir_const_is_true(&active_ctx->ir_base[ref])) c_dead_code = 1;
	ref = ir_IF(ref);
	active_ctx->ir_base[ref].op3 = IR_UNUSED;
	ir_IF_TRUE(ref);
	return ref;
}

void c_do_if_else(ir_ref if_ref, bool orig_dead_code)
{
	ir_ref end_true_ref = ir_END();
	active_ctx->ir_base[if_ref].op3 = end_true_ref;
	ir_IF_FALSE(if_ref);
	if (!orig_dead_code) {
		ir_ref cond = active_ctx->ir_base[if_ref].op2;
		c_dead_code = (IR_IS_CONST_REF(cond) && ir_const_is_true(&active_ctx->ir_base[cond]));
	}
}

void c_do_if_end(ir_ref if_ref, bool orig_dead_code)
{
	ir_ref end_true_ref = active_ctx->ir_base[if_ref].op3;

	if (end_true_ref) {
		active_ctx->ir_base[if_ref].op3 = IR_UNUSED;
		ir_MERGE_2(end_true_ref, ir_END());
	} else {
		ir_MERGE_WITH_EMPTY_FALSE(if_ref);
	}
	c_dead_code = orig_dead_code;
}

void c_do_switch(c_loop *loop, c_value *cond)
{
	const c_type *t;

	c_value_rval(cond);
	t = cond->type;
	if (C_IS_TYPE_INT(t) || t->kind == C_TYPE_ENUM) {
		if (t->size < 4) {
			c_do_cvt(&c_type_i32, IR_I32, cond);
		}
	} else {
		yy_error("switch quantity not an integer");
	}
	loop->is_switch = 1;
	loop->switch_type = cond->type;
	loop->start = IR_UNUSED;
	loop->check = ir_SWITCH(c_value_ref(cond));
	loop->next = IR_UNUSED;
	loop->break_list = IR_UNUSED;
	loop->continue_list = IR_UNUSED;
	loop->prev = active_loop;
	loop->case_labels = NULL;
	active_loop = loop;
	ir_BEGIN(IR_UNUSED);
}

static c_loop *c_find_switch(void)
{
	c_loop *loop = active_loop;

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

static void c_case_labels_add_i(c_loop *loop, ir_val min, ir_val max)
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
					q = ir_mem_malloc(sizeof(c_case_labels));
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
					q = ir_mem_malloc(sizeof(c_case_labels));
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
		loop->case_labels = p = ir_mem_malloc(sizeof(c_case_labels));
		p->min = min;
		p->max = max;
		p->parent = NULL;
		p->left = NULL;
		p->right = NULL;
		p->color = 0;
	}
}

static void c_case_labels_add_u(c_loop *loop, ir_val min, ir_val max)
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
					q = ir_mem_malloc(sizeof(c_case_labels));
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
					q = ir_mem_malloc(sizeof(c_case_labels));
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
		loop->case_labels = p = ir_mem_malloc(sizeof(c_case_labels));
		p->min = min;
		p->max = max;
		p->parent = NULL;
		p->left = NULL;
		p->right = NULL;
		p->color = 0;
	}
}

static void c_case_labels_free(c_case_labels *p)
{
	if (p->left) c_case_labels_free(p->left);
	if (p->right) c_case_labels_free(p->right);
	ir_mem_free(p);
}

void c_do_case(c_value *v)
{
	c_loop *loop = c_find_switch();
	ir_ref prev = IR_UNUSED;

	if (!loop) yy_error("case label not within a switch statement");
	if (!c_value_is_const(v) || (!C_IS_TYPE_INT(v->type) && v->type->kind != C_TYPE_ENUM)) {
		yy_error("case label does not reduce to an integer constant");
	}
	if (active_ctx->control) {
		prev = ir_END();
	}
	if (loop->switch_type != v->type) {
		c_do_cvt(loop->switch_type, c_type2ir(loop->switch_type), v);
	}
	if (C_IS_TYPE_SIGNED(loop->switch_type)) {
		c_case_labels_add_i(loop, v->u.val, v->u.val);
	} else {
		c_case_labels_add_u(loop, v->u.val, v->u.val);
	}
	ir_CASE_VAL(loop->check, c_value_ref(v));
	if (prev) {
		ir_MERGE_2(prev, ir_END());
	}
}

void c_do_case_range(c_value *v1, c_value *v2)
{
	c_loop *loop = c_find_switch();
	ir_ref list = IR_UNUSED;

	if (!loop) yy_error("case label not within a switch statement");
	if (!c_value_is_const(v1) || (!C_IS_TYPE_INT(v1->type) && v1->type->kind != C_TYPE_ENUM)
	 || !c_value_is_const(v2) || (!C_IS_TYPE_INT(v2->type) && v2->type->kind != C_TYPE_ENUM)) {
		yy_error("case labels do not reduce to integer constants");
	}
	if (active_ctx->control) {
		ir_END_list(list);
	}
	if (loop->switch_type != v1->type) {
		c_do_cvt(loop->switch_type, c_type2ir(loop->switch_type), v1);
	}
	if (loop->switch_type != v2->type) {
		c_do_cvt(loop->switch_type, c_type2ir(loop->switch_type), v2);
	}
	if (C_IS_TYPE_SIGNED(loop->switch_type)) {
		if (v1->u.val.i64 <= v2->u.val.i64) {
			c_case_labels_add_i(loop, v1->u.val, v2->u.val);
			if (v2->u.val.i64 - v1->u.val.i64 < 64) {
				int64_t i;

				for (i = v1->u.val.i64; i <= v2->u.val.i64; i++) {
					v1->u.val.i64 = i;
					ir_CASE_VAL(loop->check, c_value_ref(v1));
					ir_END_list(list);
				}
			} else {
				ir_CASE_RANGE(loop->check, c_value_ref(v1), c_value_ref(v2));
				ir_END_list(list);
			}
		} else {
			yy_warning("empty range specified");
		}
	} else {
		if (v1->u.val.u64 <= v2->u.val.u64) {
			c_case_labels_add_u(loop, v1->u.val, v2->u.val);
			if (v2->u.val.u64 - v1->u.val.u64 < 64) {
				uint64_t i;

				for (i = v1->u.val.u64; i <= v2->u.val.u64; i++) {
					v1->u.val.u64 = i;
					ir_CASE_VAL(loop->check, c_value_ref(v1));
					ir_END_list(list);
				}
			} else {
				ir_CASE_RANGE(loop->check, c_value_ref(v1), c_value_ref(v2));
				ir_END_list(list);
			}
		} else {
			yy_warning("empty range specified");
		}
	}
	ir_MERGE_list(list);
}

void c_do_case_default(void)
{
	c_loop *loop = c_find_switch();
	ir_ref prev = IR_UNUSED;

	if (!loop) yy_error("\"default\" label not within a switch statement");
	if (loop->next) yy_error("multiple default labels in one switch");
	if (active_ctx->control) {
		prev = ir_END();
	}
	ir_CASE_DEFAULT(loop->check);
	if (prev) {
		ir_MERGE_2(prev, ir_END());
	}
	loop->next = active_ctx->control;
}

void c_do_switch_end(c_loop *loop)
{
	if (loop->case_labels) {
		c_case_labels_free(loop->case_labels);
	}
	if (!loop->next) {
		if (active_ctx->control) {
			ir_END_list(loop->break_list);
		}
		ir_CASE_DEFAULT(loop->check);
		ir_END_list(loop->break_list);
	}
	if (loop->break_list) {
		if (active_ctx->control) {
			ir_END_list(loop->break_list);
		}
		ir_MERGE_list(loop->break_list);
	}
	active_loop = loop->prev;
}

void c_do_loop_start(c_loop *loop)
{
	loop->is_switch = 0;
	loop->start = ir_LOOP_BEGIN(ir_END());
	loop->check = IR_UNUSED;
	loop->next = IR_UNUSED;
	loop->break_list = IR_UNUSED;
	loop->continue_list = IR_UNUSED;
	loop->prev = active_loop;
	active_loop = loop;
}

void c_do_loop_check(c_loop *loop, c_value *cond)
{
	ir_ref ref;

	if (cond->type->kind == C_TYPE_VOID || cond->type->kind == C_TYPE_STRUCT || cond->type->kind == C_TYPE_UNION) {
		yy_error("scalar is required");
	} else if (C_IS_TYPE_FP(cond->type)) {
		ir_val val;
		val.u64 = 0;
		ref = ir_NE(c_value_ref(cond), ir_const(active_ctx, val, cond->u.type));
	} else {
		ref = c_value_ref(cond);
	}
	loop->check = ir_IF(ref);
	ir_IF_TRUE(loop->check);
}

void c_do_loop_continue_label(c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
		loop->continue_list = IR_UNUSED;
	}
}

void c_do_loop_end(c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
	}
	ir_ref end = ir_LOOP_END();
	active_ctx->ir_base[loop->start].op2 = end;
	ir_IF_FALSE(loop->check);
	if (loop->break_list) {
		ir_END_list(loop->break_list);
		ir_MERGE_list(loop->break_list);
	}
	active_loop = loop->prev;
}

void c_do_for_next_start(c_loop *loop)
{
	/* store "control" link in BEGIN.op2 */
	loop->next = active_ctx->control =
		ir_emit3(active_ctx, IR_BEGIN, IR_UNUSED, active_ctx->control, active_ctx->flags);
	/* disable FOLDING */
	active_ctx->flags &= ~IR_OPT_FOLDING;
}

void c_do_for_next_end(c_loop *loop)
{
	ir_ref end = ir_END();
	/* restore "control" link from BEGIN.op2 */
	active_ctx->control = active_ctx->ir_base[loop->next].op2;
	/* restore FOLDING */
	active_ctx->flags = active_ctx->ir_base[loop->next].op3;
	/* store END of "next" block in BEGIN.op2 */
	active_ctx->ir_base[loop->next].op2 = end;
			active_ctx->ir_base[loop->next].op3 = IR_UNUSED;
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

void c_do_for_end(c_loop *loop)
{
	if (loop->continue_list) {
		ir_END_list(loop->continue_list);
		ir_MERGE_list(loop->continue_list);
	}

	if (loop->next) {
		ir_ref start = loop->next;
		ir_ref end = active_ctx->ir_base[loop->next].op2;
		active_ctx->control = ir_repeat_code_block(active_ctx, start, end, active_ctx->control);
		memset(&active_ctx->ir_base[start], 0, sizeof(ir_insn) * ((end - start) + 1));
	}

	ir_ref end = ir_LOOP_END();
	active_ctx->ir_base[loop->start].op2 = end;

	if (loop->check) {
		ir_IF_FALSE(loop->check);
		if (loop->break_list) {
			ir_END_list(loop->break_list);
			ir_MERGE_list(loop->break_list);
		}
	} else if (loop->break_list) {
		ir_MERGE_list(loop->break_list);
	} else {
		ir_BEGIN(IR_UNUSED);
	}
	active_loop = loop->prev;
}

static c_loop *c_find_loop(void)
{
	c_loop *loop = active_loop;

	while (loop) {
		if (!loop->is_switch) return loop;
		loop = loop->prev;
	}
	return NULL;
}

void c_do_continue(void)
{
	c_loop *loop = c_find_loop();

	if (!loop) yy_error("continue statement not within a loop");
	ir_END_list(loop->continue_list);
	ir_BEGIN(IR_UNUSED);
}

void c_do_break(void)
{
	if (!active_loop) yy_error("break statement not within loop or switch");
	ir_END_list(active_loop->break_list);
	ir_BEGIN(IR_UNUSED);
}

void c_do_return(c_value *val)
{
	IR_ASSERT(active_func);
	if (val && c_value_is_set(val) && val->type->kind != C_TYPE_VOID) {
		if (!active_ctx->ret_type) {
			yy_error("\"return\" with a value, in function returning void");
		} else if (active_func->value.type->func.ret_type != val->type) {
			c_do_check_cvt(active_func->value.type->func.ret_type, val, 0);
		}
		if (val->type->kind == C_TYPE_STRUCT || val->type->kind == C_TYPE_UNION) {
			if (val->type->size <= sizeof(void*)) {
				if (val->type->size <= 4) {
					ir_RETURN(ir_LOAD_U32(val->u.ref));
				} else {
					ir_RETURN(ir_LOAD_U64(val->u.ref));
				}
			} else {
				yy_error("long struct return not implemented yet"); //???
			}
		} else {
			ir_RETURN(c_value_ref(val));
		}
	} else {
		if (active_ctx->ret_type) yy_error("\"return\" with no value, in function returning non-void");
		ir_RETURN(IR_UNUSED);
	}
	/* start an unreachable block  (it's going to be optimized out) */
	ir_BEGIN(IR_UNUSED);
}

void c_do_goto(c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = yy_hash.data[name].label;
	if (!label) {
		label = c_new_label(name, active_func_scope, NULL, active_scope == active_func_scope);
	}
	ir_END_list(label->src_list);
	ir_BEGIN(IR_UNUSED);
}

c_label *c_do_set_label(c_name name)
{
	c_label *label;

	IR_ASSERT(name);
	label = yy_hash.data[name].label;
	if (!label) {
		label = c_new_label(name, active_func_scope, NULL, active_scope == active_func_scope);
	} else if (label->dst) {
		yy_error_fmt("duplicate label \"%s\"", yy_sym2str(name));
		return NULL;
	}

	if (label->src_list) {
		ir_ref ref = label->src_list;
		uint32_t n = 0;
		ir_ref *srcs;

		/* count inputs count */
		do {
			ir_insn *insn = &active_ctx->ir_base[ref];

			IR_ASSERT(insn->op == IR_END);
			ref = insn->op2;
			n++;
		} while (ref != IR_UNUSED);

		srcs = alloca(sizeof(ir_ref) * (n + 2));

		ref = label->src_list;
		n = 0;
		do {
			ir_insn *insn = &active_ctx->ir_base[ref];

			srcs[n] = ref;
			IR_ASSERT(insn->op == IR_END);
			ref = insn->op2;
			n++;
		} while (ref != IR_UNUSED);
		srcs[n++] = ir_END();
		srcs[n++] = IR_UNUSED;

		ir_MERGE_N(n, srcs);

		label->src_list = IR_UNUSED;
	} else {
		ir_MERGE_2(ir_END(), IR_UNUSED);
	}
	label->dst = active_ctx->control;

	return label;
}

void c_do_set_label_attrs(c_label *label, c_dcl *attrs)
{
	if (attrs->attr & C_ATTR_UNUSED) {
		label->is_unused = 1;
	}
}

 void c_do_finish_label(c_name name, c_label *label)
{
	if (label->dst) {
		ir_ref end, *ops;
		ir_insn *insn = &active_ctx->ir_base[label->dst];

		IR_ASSERT(insn->op == IR_MERGE);
		if (!label->src_list) {
			if (insn->inputs_count == 2) {
				if (!label->is_unused) {
					yy_warning_fmt("label \"%s\" defined but not used", yy_sym2str(name));
				}
				insn->op = IR_BEGIN;
				insn->inputs_count = 1;
			} else {
				insn->inputs_count--;
			}
		} else {
			if (!active_ctx->ir_base[label->src_list].op2) {
				/* one element list */
				end = label->src_list;
			} else {
				ir_ref prev = IR_UNUSED;

				if (active_ctx->control) {
					prev = ir_END(); // TODO: try to avoid this contol split ???
				}
				ir_MERGE_list(label->src_list);
				end = ir_END();
				if (prev) {
					ir_BEGIN(prev);
				}
			}
			ops = active_ctx->ir_base[label->dst].ops;
			ops[insn->inputs_count] = end;
		}
	} else if (label->src_list) {
		yy_error_fmt("label \"l1\" used but not defined", yy_sym2str(name));
	}
}

void c_do_label_value(c_value *res, c_name label)
{
	memset(res, 0, sizeof(*res)); //???
	yy_error("computed goto not implemented yet"); //???
}

void c_do_computed_goto(c_value *v)
{
	yy_error("computed goto not implemented yet"); //???
}

static bool c_do_init_fix_reloc(c_value *val)
{
	ir_insn *addr_insn;

	if (val->type->kind != C_TYPE_POINTER) {
		return 0;
	}

	addr_insn = &active_ctx->ir_base[val->u.ref];
	if (!IR_IS_CONST_REF(val->u.ref)) {
		if (addr_insn->opt == IR_OPT(IR_ADD, IR_ADDR)
		 && IR_IS_CONST_REF(addr_insn->op1)
		 && IR_IS_CONST_REF(addr_insn->op2)) {
			// address resolution (add reloc) ???
			size_t offset;

			if (active_ctx->ir_base[addr_insn->op1].op == IR_SYM
			 && !IR_IS_SYM_CONST(active_ctx->ir_base[addr_insn->op2].op)) {
				offset = active_ctx->ir_base[addr_insn->op2].val.u64;
				addr_insn = &active_ctx->ir_base[addr_insn->op1];
			} else if (active_ctx->ir_base[addr_insn->op2].op == IR_SYM
			 && !IR_IS_SYM_CONST(active_ctx->ir_base[addr_insn->op1].op)) {
				offset = active_ctx->ir_base[addr_insn->op1].val.u64;
				addr_insn = &active_ctx->ir_base[addr_insn->op2];
		    } else {
				return 0;
			}
			size_t len;
			const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
			c_name n = yy_hash_lookup(name, len);
			IR_ASSERT(yy_hash.data[n].sym
				&& yy_hash.data[n].sym->kind == C_SYM_VAR
				&& c_value_is_const(&yy_hash.data[n].sym->value)
				&& yy_hash.data[n].sym->value.u.type == IR_ADDR
				&& yy_hash.data[n].sym->value.u.val.addr);
			val->u.val.addr = yy_hash.data[n].sym->value.u.val.addr + offset;
			return 1;
		}
	} else if (addr_insn->op == IR_SYM) {
		// address resolution (add reloc) ???
		size_t len;
		const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
		c_name n = yy_hash_lookup(name, len);
		IR_ASSERT(yy_hash.data[n].sym
			&& yy_hash.data[n].sym->kind == C_SYM_VAR
			&& c_value_is_const(&yy_hash.data[n].sym->value)
			&& yy_hash.data[n].sym->value.u.type == IR_ADDR
			&& yy_hash.data[n].sym->value.u.val.addr);
		val->u.val.addr = yy_hash.data[n].sym->value.u.val.addr;
		return 1;
	} else if (addr_insn->op == IR_FUNC) {
		size_t len;
		const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
		c_name n = yy_hash_lookup(name, len);
		IR_ASSERT(yy_hash.data[n].sym && yy_hash.data[n].sym->kind == C_SYM_FUNC);
		if (!c_value_is_const(&yy_hash.data[n].sym->value)) {
			/* resolve name or add thunk */
			void *addr = c_linker_resolve_sym_name(NULL, name, IR_RESOLVE_SYM_ADD_THUNK);
			IR_ASSERT(addr);
			(void)addr;
		}
		IR_ASSERT(yy_hash.data[n].sym->value.u.type == IR_ADDR
			&& yy_hash.data[n].sym->value.u.val.addr);
		val->u.val.addr = yy_hash.data[n].sym->value.u.val.addr;
		return 1;
	}
	return 0;
}

static void c_do_init(void *addr, c_value *val)
{
	const c_type *type = val->type;

	if (!c_value_is_const(val) && !c_do_init_fix_reloc(val)) {
		yy_error("initializer element is not constant");
	}

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
		case C_TYPE_STRUCT:
		case C_TYPE_UNION:    memcpy(addr, val->u.val.ptr, type->size); break;
		case C_TYPE_ENUM:     kind = type->enumeration.kind; goto repeat;
		default: IR_ASSERT(0);
	}
}

static void c_do_init_patch_flexible_alloca(ir_ref ref, size_t len)
{
	IR_ASSERT(ref > 0);
	IR_ASSERT(active_ctx->ir_base[ref].op == IR_ALLOCA);
	active_ctx->ir_base[ref].op2 = ir_const_size_t(active_ctx, len);
	if (active_ctx->ir_base[ref + 1].op == IR_CALL
	 && active_ctx->ir_base[ref + 1].inputs_count == 5
	 && active_ctx->ir_base[ref + 1].op1 == ref
//???	 && active_ctx->ir_base[ref + 1].op2 == memset
	 && 	active_ctx->ir_base[ref + 1].op3 == ref) {
		ir_ref *ops = active_ctx->ir_base[ref + 1].ops;
		ops[5] = active_ctx->ir_base[ref].op2;
	}
}

static void c_do_grow_flexible(c_sym *obj, size_t size)
{
	IR_ASSERT(obj->value.u.type == IR_ADDR);
	obj->value.u.val.ptr = c_linker_grow_data(obj->value.u.val.ptr, size);
	if (c_value_is_ref(&obj->value)) {
		size_t len;
		const char *str = ir_get_strl(active_ctx, active_ctx->ir_base[obj->value.u.ref].val.name, &len);
		c_name sym = yy_hash_find(str, len);
		yy_hash.data[sym].sym->value.u.val.ptr = obj->value.u.val.ptr;
	}
}

void c_do_init_obj(c_sym *obj, c_value *val)
{
	if (obj->kind != C_SYM_VAR) {
		if (obj->kind == C_SYM_FUNC) yy_error("function is initialized like a variable");
		if (obj->kind == C_SYM_TYPE) yy_error("typedef is initialized");
		IR_ASSERT(0);
	}
	c_value_rval(val);
	if (obj->value.type != val->type) {
		if (obj->value.type->kind == C_TYPE_ARRAY
		 && c_value_is_const(val)
		 && val->type->kind == C_TYPE_ARRAY
		 && ((val->type == &c_type_string
		   && (obj->value.type->array.type->kind == C_TYPE_CHAR
		    || obj->value.type->array.type->kind == C_TYPE_U8
		    || obj->value.type->array.type->kind == C_TYPE_I8))
		  || (val->type == &c_type_lstring
		   && obj->value.type->array.type == val->type->array.type)
		  || (val->type == &c_type_string_u16
		   && obj->value.type->array.type == val->type->array.type)
		  || (val->type == &c_type_string_u32
		   && obj->value.type->array.type == val->type->array.type))) {
			const char *str = val->u.val.ptr;
			size_t len = val->u.ref; /* ref keeps string lenght */
			if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
				/* Convert "flexible" array to regular */
				c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));

				*type = *obj->value.type;
				len += val->type->array.type->size;
				type->array.length = type->size = len;
				type->attr &= ~C_ATTR_FLEXIBLE;
				obj->value.type = type;
				if (c_value_is_const(&obj->value)
				 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
					c_do_grow_flexible(obj, obj->value.type->size);
				} else {
					c_do_init_patch_flexible_alloca(obj->value.u.ref, len);
				}
			} else if (len > (size_t)obj->value.type->array.length) {
				if (val->type->array.type->size == 1) {
					yy_error("initializer-string for array of \"char\" is too long");
				} else {
					yy_error("initializer-string for array is too long");
				}
			} else if (len + val->type->array.type->size < (size_t)obj->value.type->array.length) {
				len += val->type->array.type->size;
			}
			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				if (!c_value_is_const(val) && !c_do_init_fix_reloc(val)) yy_error("initializer element is not constant");
				IR_ASSERT(obj->value.u.type == IR_ADDR);
				memcpy((char*)obj->value.u.val.ptr, str, len);
			} else {
				IR_ASSERT(obj->value.u.ref > 0);
				IR_ASSERT(active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
				ir_memcpy(active_ctx,
					obj->value.u.ref,
					ir_const_addr(active_ctx, (uintptr_t)str),
					ir_const_size_t(active_ctx, len));
			}
			return;
		}
		c_do_check_cvt(obj->value.type, val, -2);
	}
	if (obj->linkage == C_LINK_EXTERNAL || obj->linkage == C_LINK_INTERNAL) {
		IR_ASSERT((c_value_is_const(&obj->value)
				&& obj->value.u.type == IR_ADDR
				&& obj->value.u.val.ptr)
			|| (c_value_is_ref(&obj->value)
				&& active_scope
				&& IR_IS_CONST_REF(obj->value.u.ref)
				&& active_ctx->ir_base[obj->value.u.ref].op == IR_SYM
				&& obj->value.u.val.ptr));
		c_do_init(obj->value.u.val.ptr, val);
	} else if (C_IS_TYPE_SCALAR_OR_PTR(obj->value.type)) {
		IR_ASSERT(c_value_is_ref(&obj->value));
		if (active_ctx->ir_base[obj->value.u.ref].op == IR_VAR) {
			ir_VSTORE(obj->value.u.ref, c_value_ref(val));
		} else {
			ir_STORE(obj->value.u.ref, c_value_ref(val));
		}
	} else {
		IR_ASSERT(obj->value.type->size == val->type->size);
		if (c_value_is_const(&obj->value)
		 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
			if (!c_value_is_const(val) && !c_do_init_fix_reloc(val)) yy_error("initializer element is not constant");
			IR_ASSERT(obj->value.u.type == IR_ADDR);
			IR_ASSERT(val->u.type == IR_ADDR);
			memcpy((char*)obj->value.u.val.ptr, val->u.val.ptr, val->type->size);
		} else {
			IR_ASSERT(obj->value.u.ref > 0);
			IR_ASSERT(active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			ir_memcpy(active_ctx,
				obj->value.u.ref,
				c_value_ref(val),
				ir_const_size_t(active_ctx, val->type->size));
		}
	}
}

void c_do_init_first(c_sym *obj, c_init *init, const c_type *type, size_t offset)
{
	init->offset = offset;
	init->level = 0;
	init->stack[0].type = type;
	init->stack[0].pos = 0;
}

void c_do_init_dim(c_sym *obj, c_init *init, c_value *dim)
{
	const c_type *type = init->stack[init->level].type;

	if (type->kind != C_TYPE_ARRAY) yy_error("array index in non-array initializer");
	if (!c_value_is_const(dim) || !C_IS_TYPE_INT(dim->type)) yy_error("array index in initializer not an integer constant");
	if (C_IS_TYPE_SIGNED(dim->type) && dim->u.val.i64 < 0) yy_error("array index in initializer exceeds array bounds");
	if (dim->u.val.i64 >= type->array.length && !(type->attr & C_ATTR_FLEXIBLE)) yy_error("array index in initializer exceeds array bounds");

	if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
	init->stack[init->level].pos = dim->u.val.i64;
	init->level++;
	init->stack[init->level].type = type->array.type;
	init->stack[init->level].pos = 0;
}

static bool c_find_struct_field_ex(const c_type *type, c_name name, c_init *init)
{
	int32_t i;
	c_field *f;

	for (i = 0, f = type->record.fields; i < type->record.num_fields; f++, i++) {
		if (f->name) {
			if (f->name == name) {
				IR_ASSERT(!C_IS_BIT_FIELD(f->bit_field)); //???
				if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
				init->stack[init->level].pos = i;
				init->level++;
				init->stack[init->level].type = f->type;
				init->stack[init->level].pos = 0;
				return 1;
			}
		} else if (f->type->kind == C_TYPE_STRUCT || f->type->kind == C_TYPE_UNION) {
			if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
			init->stack[init->level].pos = i;
			init->level++;
			init->stack[init->level].type = f->type;
			if (c_find_struct_field_ex(f->type, name, init)) return 1;
			init->level--;
		}
	}
	return NULL;
}

void c_do_init_field(c_sym *obj, c_init *init, c_name field_name)
{
	const c_type *type = init->stack[init->level].type;

	if (type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) {
		yy_error("field name not in struct or union initializer");
	} else if (!c_find_struct_field_ex(type, field_name, init)) {
		yy_error_fmt("struct or union has no member named \"%s\"", yy_sym2str(field_name));
	}
}

void c_do_init_next(c_sym *obj, c_init *init)
{
	const c_type *type;
	int64_t pos;

	while (1) {
		type = init->stack[init->level].type;
		pos =  init->stack[init->level].pos;
		if (type->kind == C_TYPE_ARRAY) {
			if (++pos < type->array.length || (type->attr & C_ATTR_FLEXIBLE)) {
				init->stack[init->level].pos = pos;
				return;
			}
			if (init->level == 0) yy_error("excess elements in array initializer");
			init->level--;
		} else if (type->kind == C_TYPE_STRUCT) {
			if (++pos < type->record.num_fields) {
				init->stack[init->level].pos = pos;
				return;
			}
			if (init->level == 0) yy_error("excess elements in struct initializer");
			init->level--;
		} else if (type->kind == C_TYPE_UNION) {
			if (init->level == 0) {
				yy_warning("excess elements in union initializer");
				return;
			}
			init->level--;
		} else {
			if (init->level == 0) yy_error("excess elements in scalar initializer");
			init->level--;
		}
	}
}

void c_do_init_set(c_sym *obj, c_init *init, c_value *val, size_t *size)
{
	const c_type *type = init->stack[init->level].type;
	const c_type *last_array_type = NULL;
	size_t last_array_offset = 0;
	size_t offset;
	uint32_t i;
	uint16_t bit_field = 0;

	while (1) {
		if (type == val->type) {
			break;
		} else if (type->kind == C_TYPE_ARRAY) {
			if (val->type == &c_type_string
			 && c_value_is_const(val)
			 && type->kind == C_TYPE_ARRAY
			 && (type->array.type->kind == C_TYPE_CHAR
			  || type->array.type->kind == C_TYPE_I8
			  || type->array.type->kind == C_TYPE_U8)) {
				break;
			}
			type = type->array.type;
			if (type->kind != C_TYPE_ARRAY && type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) break;
		} else if (type->kind == C_TYPE_STRUCT) {
			c_field *f;

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
			// TODO: select best type
			type = type->record.fields[0].type;
			if (type->kind != C_TYPE_ARRAY && type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) break;
		} else {
			break;
		}

		if (type == val->type || c_compatible_types(type, val->type, 1, 0)) {
			break;
		}

		if (val->type == &c_type_string
		 && c_value_is_const(val)
		 && type->kind == C_TYPE_ARRAY
		 && (type->array.type->kind == C_TYPE_CHAR
		  || type->array.type->kind == C_TYPE_I8
		  || type->array.type->kind == C_TYPE_U8)) {
			break;
		}

		if (init->level >= C_INIT_STACK_SIZE) yy_error("too deep initialization level");
		init->level++;
		init->stack[init->level].type = type;
		init->stack[init->level].pos = 0;
	}

	/* recalculate offset */
	offset = init->offset;
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
//				offset += t->record.fields[init->stack[i].pos].offset;
				c_field *f = &t->record.fields[init->stack[i].pos];
				offset += f->offset;
				bit_field = f->bit_field;
			} else {
				IR_ASSERT(i == init->level);
			}
		} else {
			IR_ASSERT(i == init->level);
		}
	}

	if (val->type != type) {
		if (type->kind == C_TYPE_ARRAY
		 && c_value_is_const(val)
		 && val->type->kind == C_TYPE_ARRAY
		 && ((val->type == &c_type_string
		   && (type->array.type->kind == C_TYPE_CHAR
		    || type->array.type->kind == C_TYPE_U8
		    || type->array.type->kind == C_TYPE_I8))
		  || (val->type == &c_type_lstring
		   && type->array.type == val->type->array.type)
		  || (val->type == &c_type_string_u16
		   && type->array.type == val->type->array.type)
		  || (val->type == &c_type_string_u32
		   && type->array.type == val->type->array.type))) {
			const char *str = val->u.val.ptr;
			size_t len = val->u.ref; /* ref keeps string lenght */

			if (len > (size_t)type->array.length && !(type->attr & C_ATTR_FLEXIBLE)) {
				if (val->type->array.type->size == 1) {
					yy_error("initializer-string for array of \"char\" is too long");
				} else {
					yy_error("initializer-string for array is too long");
				}
			}

			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				if (!c_value_is_const(val) && !c_do_init_fix_reloc(val)) yy_error("initializer element is not constant");
				IR_ASSERT(obj->value.u.type == IR_ADDR);
				if (type->attr & C_ATTR_FLEXIBLE) {
					len += type->array.type->size; /* for terminating zero */
					if (obj->value.type == type) {
						*size = len;
					} else if (obj->value.type->kind == C_TYPE_STRUCT
					 && obj->value.type->record.fields[obj->value.type->record.num_fields-1].type == type) {
						/* last element of struct */
						*size = offset + len;
					} else {
						yy_error("initialization of flexible array member in a nested context");
					}
					c_do_grow_flexible(obj, *size);
				} else if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
					/* element of flexible array */
					*size = offset + obj->value.type->array.type->size;
					c_do_grow_flexible(obj, *size);
				}
				memcpy((char*)obj->value.u.val.ptr + offset, str, len);
			} else {
				IR_ASSERT(obj->value.u.ref > 0);
				IR_ASSERT(active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
				if (type->attr & C_ATTR_FLEXIBLE) {
					len += type->array.type->size;
					if (obj->value.type == type) {
						*size = len;
					} else if (obj->value.type->kind == C_TYPE_STRUCT
					 && obj->value.type->record.fields[obj->value.type->record.num_fields-1].type == type) {
						/* last element of struct */
						*size = offset + len;
					} else {
						yy_error("initialization of flexible array member in a nested context");
					}
				}
				ir_memcpy(active_ctx,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(active_ctx, offset)),
					ir_const_addr(active_ctx, (uintptr_t)str),
					ir_const_size_t(active_ctx, len));
			}
			return;
		}
		c_do_check_cvt(type, val, -1);
	}

	if (obj->value.type->kind == C_TYPE_ARRAY && (obj->value.type->attr & C_ATTR_FLEXIBLE)) {
		if (offset + type->size > obj->value.type->size) {
			// TODO: alignment support ???
			size_t len =
				(offset + type->size + obj->value.type->array.type->size - 1) / obj->value.type->array.type->size;
			if (obj->value.type->array.type->size * len > *size) {
				*size = obj->value.type->array.type->size * len;
				if (c_value_is_const(&obj->value)
				 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
					c_do_grow_flexible(obj, *size);
				}
			}
		}
	} else if (last_array_type) {
		if (obj->value.type->kind != C_TYPE_STRUCT
		 || obj->value.type->record.fields[obj->value.type->record.num_fields-1].type != last_array_type) {
			yy_error("initialization of flexible array member in a nested context");
		}
		/* last element of struct */
		if (last_array_offset + last_array_type->array.type->size > *size) {
			*size = last_array_offset + last_array_type->array.type->size; 
			if (c_value_is_const(&obj->value)
			 || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
				c_do_grow_flexible(obj, *size);
			}
		}
	} else {
		IR_ASSERT(offset + type->size <= obj->value.type->size || C_IS_BIT_FIELD(bit_field));
	}
	if (c_value_is_const(&obj->value) || (c_value_is_ref(&obj->value) && IR_IS_CONST_REF(obj->value.u.ref))) {
		if (!c_value_is_const(val) && !c_do_init_fix_reloc(val)) yy_error("initializer element is not constant");
		IR_ASSERT(obj->value.u.type == IR_ADDR && obj->value.u.val.ptr);
		if (!C_IS_BIT_FIELD(bit_field)) {
			c_do_init((char*)obj->value.u.val.ptr + offset, val);
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
				mask = (((1UL<<bits)-1)<<first_bit);
				data[0] &= ~mask;
				data[0] |= bits_val & mask;
			} else {
				mask = (((1UL<<(64-first_bit))-1)<<first_bit);
				data[0] &= ~mask;
				data[0] |= (bits_val << first_bit) & mask;
				mask = (1UL<<(bits-64+first_bit))-1;
				data[1] &= ~mask;
				data[1] |= (bits_val >> (64 - first_bit)) & mask;
			}
			memcpy((char*)obj->value.u.val.ptr + offset, data, (first_bit + bits + 7) / 8);
		}
	} else {
		IR_ASSERT(obj->value.u.ref > 0);
		if (type->kind == C_TYPE_STRUCT || type->kind == C_TYPE_UNION) {
			IR_ASSERT(!C_IS_BIT_FIELD(bit_field));
			IR_ASSERT(active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			if (type->size) {
				ir_memcpy(active_ctx,
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(active_ctx, offset)),
					c_value_ref(val),
					ir_const_size_t(active_ctx, type->size));
			}
		} else if (active_ctx->ir_base[obj->value.u.ref].op == IR_VAR) {
			IR_ASSERT(!C_IS_BIT_FIELD(bit_field));
			ir_VSTORE(obj->value.u.ref, c_value_ref(val));
		} else {
			IR_ASSERT(active_ctx->ir_base[obj->value.u.ref].op == IR_ALLOCA);
			if (!C_IS_BIT_FIELD(bit_field)) {
				ir_STORE(
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(active_ctx, offset)),
					c_value_ref(val));
			} else if (!C_IS_BIT_FIELD_PACKED(bit_field)) {
				c_do_store_bit_field(
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(active_ctx, offset)),
					C_BIT_FIELD_START(bit_field), C_BIT_FIELD_SIZE(bit_field), val);
			} else {
				c_do_store_bit_field_packed(
					ir_ADD_A(obj->value.u.ref, ir_const_size_t(active_ctx, offset)),
					C_BIT_FIELD_START(bit_field), C_BIT_FIELD_SIZE(bit_field), val);
			}
		}
	}
}

const c_type *c_do_init_nested(c_sym *obj, c_init *init, bool b, size_t *offset_ptr)
{
	const c_type *type = init->stack[init->level].type;
	size_t offset;
	uint32_t i;

	if (!b) {
		if (type->kind == C_TYPE_ARRAY) {
			type = type->array.type;
		} else if (type->kind == C_TYPE_STRUCT) {
			c_field *field = &type->record.fields[init->stack[init->level].pos];
			type = field->type;
		} else if (type->kind == C_TYPE_UNION) {
			// TODO: select best type
			c_field *field = &type->record.fields[0];
			type = field->type;
		}
	}

	if (type->kind != C_TYPE_ARRAY && type->kind != C_TYPE_STRUCT && type->kind != C_TYPE_UNION) {
		yy_warning("braces around scalar initializer");
	}

	/* recalculate offset */
	offset = init->offset;
	for (i = 0; i <= init->level; i++) {
		const c_type *t = init->stack[i].type;
		if (t->kind == C_TYPE_ARRAY) {
			offset += t->array.type->size * init->stack[i].pos;
		} else if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
			if (init->stack[i].pos < t->record.num_fields) {
				c_field *f = &t->record.fields[init->stack[i].pos];
				offset += f->offset;
				IR_ASSERT(!C_IS_BIT_FIELD(f->bit_field) || i == init->level);
			} else {
				IR_ASSERT(i == init->level);
			}
		} else {
			IR_ASSERT(i == init->level);
		}
	}

	*offset_ptr = offset;
	return type;
}

void c_do_init_end(c_sym *obj, size_t size)
{
	if (obj->value.type->attr & C_ATTR_FLEXIBLE) {
		if (obj->value.type->kind == C_TYPE_ARRAY) {
			/* Convert "flexible" array to regular */
			c_type *type = ir_arena_alloc(&c_arena, sizeof(c_type));

			*type = *obj->value.type;
			type->array.length = size / type->array.type->size;
			type->size = size;
			type->attr &= ~C_ATTR_FLEXIBLE;
			obj->value.type = type;
		}
		if (!c_value_is_const(&obj->value)
		 && (!c_value_is_ref(&obj->value) || !IR_IS_CONST_REF(obj->value.u.ref))) {
			c_do_init_patch_flexible_alloca(obj->value.u.ref, size);
		}
	}
}

void c_do_init_expr_start(c_sym *obj, const c_type *type)
{
	if ((type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type)) {
		yy_error_fmt("invalid use of undefined \"%s %s\"",
			c_type_kind2str(type->kind), yy_sym2str(type->tag));
	}
	memset(obj, 0, sizeof(c_sym));
	obj->kind = C_SYM_VAR;
	if (active_func) {
		ir_ref size = ir_const_size_t(active_ctx, type->size);
		ir_ref addr = ir_ALLOCA(size);
		ir_memzero(active_ctx, addr, size);
		c_value_set_rval(&obj->value, type, IR_ADDR, addr);
	} else {
		ir_val val;

		val.ptr = c_linker_allocate_data(type->size); // TODO: use temporary area ???
		c_value_set_const(&obj->value, type, IR_ADDR, val);
	}
}

void c_do_init_expr_end(c_value *v, c_sym *obj, size_t size)
{
	c_do_init_end(obj, size);
	if (c_value_is_const(&obj->value)) {
		c_value_set_const(v, obj->value.type, c_type2ir(obj->value.type), obj->value.u.val);
	} else if (obj->value.type->kind != C_TYPE_ARRAY) {
		c_value_set_lval(v, obj->value.type, c_type2ir(obj->value.type), obj->value.u.ref);
	} else {
		c_value_set_rval(v, obj->value.type, c_type2ir(obj->value.type), obj->value.u.ref);
	}
}

void c_do_generic_start(c_generic *g)
{
	memset(g, 0, sizeof(c_generic));
	g->old_control = c_do_nocode();
	g->last_control = active_ctx->control;
}

void c_do_generic_type(c_generic *g, const c_type *type)
{
	if (type->attr & (C_ATTR_CONST|C_ATTR_VOLATILE)) {
		/* remove top-level qualifiers */
		c_type *t = ir_arena_alloc(&c_arena, sizeof(c_type));
		*t = *type;
		t->attr &= ~(C_ATTR_CONST|C_ATTR_VOLATILE);
		type = t;
	}
	if (type->kind == C_TYPE_FUNC) {
		type = c_create_pointer_type(type);
	}
	g->type = type;
}

void c_do_generic_case(c_generic *g, const c_type *type, c_value *v)
{
	if (c_compatible_types(g->type, type, 0, 0)) {
		if (c_value_is_set(&g->matched_value)) yy_error("duplicate matched type case in \"_Generic\"");
		g->matched_value = *v;
		g->matched_control_start = g->last_control;
		g->matched_control_end = active_ctx->control;
	}
	g->last_control = active_ctx->control;
}

void c_do_generic_default(c_generic *g, c_value *v)
{
	if (c_value_is_set(&g->default_value)) yy_error("duplicate \"default\" case in \"_Generic\"");
	g->default_value = *v;
	g->default_control_start = g->last_control;
	g->default_control_end = active_ctx->control;
	g->last_control = active_ctx->control;
}

void c_do_generic_end(c_value *res, c_generic *g)
{
	if (!c_value_is_set(&g->matched_value)) {
		if (!c_value_is_set(&g->default_value)) yy_error("no matched type case in \"_Generic\"");
		g->matched_value = g->default_value;
		g->matched_control_start = g->default_control_start;
		g->matched_control_end = g->default_control_end;
	}
	*res = g->matched_value;
	if (g->matched_control_start != g->matched_control_end) {
		ir_ref ref = active_ctx->control;

		if (ref != g->matched_control_end) {
			while (active_ctx->ir_base[ref].op1 != g->matched_control_end) {
				ref = active_ctx->ir_base[ref].op1;
				IR_ASSERT(ref);
			}
			/* cut control from unreachable block */
			active_ctx->ir_base[ref].op1 = g->matched_control_start;
		} else {
			/* remove contol tail from unreachable block */
			active_ctx->control = g->matched_control_start;
		}
		ref = g->matched_control_end;
		while (active_ctx->ir_base[ref].op1 != g->matched_control_start) {
			ref = active_ctx->ir_base[ref].op1;
			IR_ASSERT(ref);
		}
		active_ctx->ir_base[ref].op1 = g->old_control;
		g->old_control = g->matched_control_end;
	}
	ir_UNREACHABLE();
	// TODO: cleanup dead code ???
	active_ctx->control = g->old_control;
}

void c_do_func_start(c_name name, c_dcl *d, c_scope *scope, ir_ctx *ctx)
{
	c_sym *func;
	const c_type *type = d->type;
	uint32_t flags;
	int32_t i;

	d->flags |= C_DCL_DEFINITION;
	func = c_declare(name, d);
	IR_ASSERT(func);
	active_func = func;
	active_func_name = name;

	c_push_scope(scope);

	flags = 0;
	if (type->attr & C_ATTR_VARIADIC) {
		flags |= IR_VARARG_FUNC;
	}

	rcc_ir_init(ctx, flags);
	if (type->func.ret_type->kind == C_TYPE_STRUCT || type->func.ret_type->kind == C_TYPE_UNION) {
		if (type->func.ret_type->size <= sizeof(void*)) {
			ctx->ret_type = (type->func.ret_type->size <= 4) ? IR_U32 : IR_U64;
		} else {
			yy_error("long struct return not implemented yet"); //???
		}
	} else {
		ctx->ret_type = c_type2ir(type->func.ret_type);
	}

	active_ctx = ctx;
	active_func_scope = active_scope;

	ir_START();
	if ((type->func.ret_type->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(type->func.ret_type)) {
		yy_error("return type is an incomplete type");
	}
	if (type->func.num_params <= 0) return;
	for (i = 0; i < type->func.num_params; i++) {
		c_param *p = &type->func.params[i];
		const c_type *t = p->type;

		if (!t) {
			yy_warning_fmt("type of \"%s\" defaults to \"int\"", yy_sym2str(p->name));
			t = p->type = &c_type_i32;
		}
		if ((t->flags & C_TYPE_INCOMPLETE) && !c_fix_incomplete_type(t)) {
			yy_error_fmt("parameter %d has incomplete type", i + 1);
		}
		if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
			if (t->size <= sizeof(void*)) {
				t = (t->size <= 4) ? &c_type_u32 : &c_type_u64;
			} else {
				yy_error("long struct arguments not implemented yet"); //???
			}
		}
		ir_param(active_ctx, c_type2ir(t), 1, yy_sym2str(p->name), i + 1);
	}
	for (i = 0; i < type->func.num_params; i++) {
		c_param *p = &type->func.params[i];
		const c_type *t = p->type;

		if (p->name) {
			c_name name = p->name;
			c_dcl dcl;
			c_sym *obj;

			dcl.flags = C_DCL_PARAM;
			dcl.attr = 0;
			dcl.type = t;
			obj = c_declare(name, &dcl);
			IR_ASSERT(obj &&  obj->kind == C_SYM_VAR && c_value_is_ref(&obj->value));
			if (t->kind == C_TYPE_STRUCT || t->kind == C_TYPE_UNION) {
				if (t->size <= sizeof(void*)) {
					if (t->size) {
						ir_STORE(obj->value.u.ref, i + 2);
					}
				} else {
					yy_error("long struct arguments not implemented yet"); //???
				}
			} else {
				ir_VSTORE(obj->value.u.ref, i + 2);
			}
		} else {
			yy_warning("omitting the parameter name in a function definition");
		}
	}
}

static bool c_is_dead_end(ir_insn *insn)
{
	while (insn->op == IR_BEGIN) {
		if (!insn->op1) return 1;
		insn = &active_ctx->ir_base[insn->op1];
		if (insn->op != IR_END) {
			return 0;
		}
		insn = &active_ctx->ir_base[insn->op1];
	}
	if (insn->op == IR_MERGE) {
		ir_ref input, *p, n;

		n = insn->inputs_count;
		for (p = insn->ops + 1; n > 0; p++, n--) {
			input = *p;
			insn = &active_ctx->ir_base[input];
			if (insn->op != IR_END) {
				return 0;
			}
			insn = &active_ctx->ir_base[insn->op1];
			if (!c_is_dead_end(insn)) {
				return 0;
			}
		}
	} else {
		return 0;
	}
	return 1;
}

void c_do_func_end(c_name name, c_dcl *d, c_scope *scope, ir_ctx *ctx)
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
			if (kind == C_POP_LABEL) {
				c_do_finish_label(id, yy_hash.data[id].label);
			}
		}
	}

	if (ctx->control) {
		if (ctx->control == active_ctx->insns_count - 1
		 && active_ctx->ir_base[ctx->control].op == IR_BEGIN
		 && !active_ctx->ir_base[ctx->control].op1) {
			active_ctx->insns_count--;
		} else if (ctx->ret_type) {
			ir_val val;

			if (!c_is_dead_end(&active_ctx->ir_base[ctx->control])) {
				yy_warning("control reaches end of non-void function");
			}
			val.u64 = 0;
			ir_RETURN(ir_const(ctx, val, ctx->ret_type));
		} else {
			ir_RETURN(IR_UNUSED);
		}
	}

	rcc_ir_compile(name, ctx, active_func);

	active_func = NULL; // TODO: nested functions ???
	active_func_name = 0; // TODO: nested functions ???
	active_func_scope = NULL;
	active_ctx = global_ctx;

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
					yy_hash.data[id].sym = ptr;
					break;
				case C_POP_TAG:
					yy_hash.data[id].tag = ptr;
					break;
				case C_POP_LABEL:
//					c_do_finish_label(id, yy_hash.data[id].label);
					if (!yy_hash.data[id].label->is_local) {
						ir_mem_free(yy_hash.data[id].label);
					}
					yy_hash.data[id].label = ptr;
					break;
				default:
					IR_ASSERT(0);
			}
		}
		pp_list_release(scope->list.syms, scope->list.size);
		scope->list.syms = NULL;
	}

	c_pop_scope(scope);
}

yy_sym c_get_current_func_name(void)
{
	return active_func_name;
}
