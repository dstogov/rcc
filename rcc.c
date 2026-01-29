/*
 * RCC - Rational C Compiler
 * (CLI driver)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
# include <unistd.h>
# include <sys/time.h>
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#include <ir.h>
#include <ir_private.h>
#include <ir_builder.h>

#include "rcc.h"

#define RCC_DELAY_CODE_GEN 1

#undef _ir_CTX
#define _ir_CTX active_ctx

/* IR compiler */
#define C_RUN                         (1<<0)
#define C_DUMP_IR                     (1<<1)
#define C_DUMP_ASM                    (1<<2)
#define C_DUMP_LLVM                   (1<<3)
#define C_DUMP_SIZE                   (1<<4)
#define C_DUMP_TIME                   (1<<5)
#define C_GDB                         (1<<6)
#define C_PERF                        (1<<7)
#define C_SYNTAX_ONLY                 (1<<8)

#define C_DUMP_DOT                    (1<<9)

#define C_DUMP_IR_AFTER_LOAD          (1<<10)
#define C_DUMP_IR_AFTER_USE_LISTS     (1<<11)
#define C_DUMP_IR_AFTER_MEM2SSA       (1<<12)
#define C_DUMP_IR_AFTER_SCCP          (1<<13)
#define C_DUMP_IR_AFTER_CFG           (1<<14)
#define C_DUMP_IR_AFTER_DOM           (1<<15)
#define C_DUMP_IR_AFTER_LOOP          (1<<16)
#define C_DUMP_IR_AFTER_GCM           (1<<17)
#define C_DUMP_IR_AFTER_SCHEDULING    (1<<18)
#define C_DUMP_IR_AFTER_CODE_MATCHING (1<<19)
#define C_DUMP_IR_AFTER_LIVE_RANGES   (1<<20)
#define C_DUMP_IR_AFTER_COALESCING    (1<<21)
#define C_DUMP_IR_AFTER_REGALLOC      (1<<22)
#define C_DUMP_IR_FINAL               (1<<23)

#define C_DUMP_IR_AFTER_ALL           0x00fffc00

#define C_DUMP_IR_CODEGEN             (1<<24)

#define C_DUMP_LIVE_RANGES            (1<<25)

#define C_SINGLE_FILE                 (1<<29)
#define C_DO_LINK_INTERNAL            (1<<30)
#define C_DO_LINK_EXTERNAL            (1U<<31)

#define C_OPT_LEVEL              0x3
#define C_OPT_INLINE             (1<<2)
#define C_OPT_MEM2SSA            (1<<3)

#define IR_UNKNOWN_SIZE          1

static bool            c_native = 0;
static uint32_t        c_opt_flags = 2 | C_OPT_INLINE | C_OPT_MEM2SSA;
static uint32_t        c_flags = 0;
static uint32_t        c_save_flags = IR_SAVE_SAFE_NAMES;
static uint32_t        ir_flags = 0;
static uint32_t        ir_mflags = 0;
static uint64_t        ir_debug_regset = 0xffffffffffffffff;
static ir_arena       *c_linker_arena;
static ir_code_buffer  c_code_buffer;
static bool            protected = 1;
static ir_list         c_codegen_list;
static FILE           *c_out = NULL;

static void rcc_dump_func_proto(c_name name, bool prototype, FILE *f)
{
	c_sym *sym = yy_hash.data[name].sym;
	const c_type *t;
	uint32_t flags;
	ir_type ret_type;
	uint32_t params_count;
	uint8_t *param_types;

	IR_ASSERT(sym && sym->kind == C_SYM_FUNC);
	if (sym->linkage == C_LINK_INTERNAL) {
		fprintf(f, "static ");
	} else if (sym->is_external || (prototype && !sym->ctx)) {
		fprintf(f, "extern ");
	}
	fprintf(f, "func @%s", yy_sym2str(name));

	t = sym->value.type;
	IR_ASSERT(t->kind == C_TYPE_FUNC);
	param_types = alloca(t->func.num_params + 16);
	c_type2proto_ex(t, &flags, &ret_type, &params_count, param_types);
	if (sym->linkage == C_LINK_BUILTIN) {
		flags |= IR_CC_BUILTIN;
	}
	ir_print_proto_ex(flags, ret_type, params_count, param_types, f);
	if (prototype) {
		fprintf(f, ";\n");
	} else {
		fprintf(f, "\n");
	}

}

static bool rcc_may_inline(c_value *func, ir_ctx *ctx)
{
	if (func->type->attr & (C_ATTR_VARIADIC|C_ATTR_NOINLINE)) {
		return 0;
	}
	if ((func->type->attr & C_ATTR_ALWAYS_INLINE)
	 || ((func->type->attr & C_ATTR_INLINE) && ctx->insns_count <= 60)
	 || ctx->insns_count <= 30) {
		return 1;
	}
	return 0;
}

static void rcc_ir_codegen(c_name name, ir_ctx *ctx, c_sym *sym)
{
	c_value *func = &sym->value;

	IR_ASSERT(sym->has_code < C_CODE_STARTED);
	sym->has_code = C_CODE_STARTED;

	if (c_native) {
		ir_match(ctx);
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 0 || c_native || 0) {
		ir_assign_virtual_registers(ctx);
		if (c_flags & C_DUMP_IR_AFTER_CODE_MATCHING) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after code matching)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after code matching)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_RULES, stderr);
			}
		}
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		ir_compute_live_ranges(ctx);
		if (c_flags & C_DUMP_IR_AFTER_LIVE_RANGES) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after live ranges)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after live ranges)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_RULES | IR_SAVE_REGS, stderr);
				if (c_flags & C_DUMP_LIVE_RANGES) {
					ir_dump_live_ranges(ctx, stderr);
				}
			}
		}

		ir_coalesce(ctx);
		if (c_flags & C_DUMP_IR_AFTER_COALESCING) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after coalescing)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after coalescing)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_RULES | IR_SAVE_REGS, stderr);
				if (c_flags & C_DUMP_LIVE_RANGES) {
					ir_dump_live_ranges(ctx, stderr);
				}
			}
		}

		if (c_native) {
			ir_reg_alloc(ctx);
			if (c_flags & C_DUMP_IR_AFTER_REGALLOC) {
				if (c_flags & C_DUMP_DOT) {
					ir_dump_dot(ctx, yy_sym2str(name), "(after regalloc)", stderr);
				} else {
					rcc_dump_func_proto(name, 0, stderr);
					fprintf(stderr, "# (after regalloc)\n");
					ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_RULES | IR_SAVE_REGS, stderr);
					if (c_flags & C_DUMP_LIVE_RANGES) {
						ir_dump_live_ranges(ctx, stderr);
					}
				}
			}
		}

		ir_schedule_blocks(ctx);
	} else if (c_native || 0) {
		ir_compute_dessa_moves(ctx);
	}

	if (c_flags & C_DUMP_IR_CODEGEN) {
		if (!(c_flags & C_DUMP_DOT)) {
			rcc_dump_func_proto(name, 0, stderr);
			fprintf(stderr, "# (codegen)\n");
			ir_dump_codegen(ctx, stderr);
			if (c_flags & C_DUMP_LIVE_RANGES) {
				ir_dump_live_ranges(ctx, stderr);
			}
		}
	}

#ifdef IR_DEBUG
	ir_check(ctx);
#endif

	if (c_native) {
		size_t size;
		void *entry;

		ctx->code_buffer = &c_code_buffer;
		protected = 0;
		ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
		entry = ir_emit_code(ctx, &size);
		IR_ASSERT(entry);
		if (c_value_is_const(func)) {
			if (!sym->is_thunk) yy_error_fmt("external symbol \"%s\" used before the local one", yy_sym2str(name));
			ir_fix_thunk(func->u.val.ptr, entry);
			sym->is_thunk = 0;
		}
#ifndef _WIN32
		if (c_flags & C_GDB) {
			ir_gdb_register(yy_sym2str(name), entry, size, sizeof(void*), 0);
		}
#endif
		ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
		protected = 1;

		if (c_flags & C_DUMP_ASM) {
//			ir_ref i;
//			ir_insn *insn;
//
			ir_disasm_add_symbol(yy_sym2str(name), (uintptr_t)entry, size);
//
//			for (i = IR_UNUSED + 1, insn = ctx->ir_base - i; i < ctx->consts_count; i++, insn--) {
//				if (insn->op == IR_FUNC) {
//					const char *name = ir_get_str(ctx, insn->val.name);
//					void *addr = ir_loader_resolve_sym_name(loader, name, 0);
//
//					IR_ASSERT(addr);
//					ir_disasm_add_symbol(name, (uintptr_t)addr, IR_UNKNOWN_SIZE);
//TODO:			} else if (insn->op == IR_SYM) {
//				}
//			}
			ir_disasm(yy_sym2str(name), entry, size, 0, ctx, c_out);
		}

#ifndef _WIN32
		if (c_flags & C_PERF) {
			ir_perf_map_register(yy_sym2str(name), entry, size);
			ir_perf_jitdump_register(yy_sym2str(name), entry, size);
		}
#endif

		func->u.op |= C_VAL_CONST;
		func->u.type = IR_ADDR;
		func->u.val.ptr = entry;
	}

	sym->has_code = C_CODE_DONE;
}

void rcc_ir_compile(c_name name, ir_ctx *ctx, c_sym *sym)
{
	c_value *func = &sym->value;

	if (c_flags & C_DUMP_IR_AFTER_LOAD) {
		if (c_flags & C_DUMP_DOT) {
			ir_dump_dot(ctx, yy_sym2str(name), "(after load)", stderr);
		} else {
			rcc_dump_func_proto(name, 0, stderr);
			fprintf(stderr, "# (after load)\n");
			ir_save(ctx, c_save_flags, stderr);
		}
	}

	if (c_flags & C_SYNTAX_ONLY) {
		ir_free(ctx);
		return;
	}

	ir_build_def_use_lists(ctx);

	if (c_prologue_end) {
		ir_use_list *use_list = &ctx->use_lists[c_prologue_end];

		if (use_list->count == 1) {
			/* remove BLOCK_BEGIN mark */
			ir_insn *insn = &ctx->ir_base[c_prologue_end];

			IR_ASSERT(insn->op == IR_BLOCK_BEGIN);

			ir_ref prev = insn->op1;
			ir_ref next = ctx->use_edges[use_list->refs];
			ctx->ir_base[next].op1 = prev;
			ir_use_list_replace_one(ctx, prev, c_prologue_end, next);

			insn->optx = IR_NOP;
			insn->op1 = IR_UNUSED;
			use_list->count = 0;
			c_prologue_end = IR_UNUSED;
		}
	}

	if (c_flags & C_DUMP_IR_AFTER_USE_LISTS) {
		if (c_flags & C_DUMP_DOT) {
			ir_dump_dot(ctx, yy_sym2str(name), "(after use lists)", stderr);
		} else {
			rcc_dump_func_proto(name, 0, stderr);
			fprintf(stderr, "# (after use lists)\n");
			ir_save(ctx, c_save_flags | IR_SAVE_USE_LISTS, stderr);
		}
	}

#ifdef IR_DEBUG
	ir_check(ctx);
#endif

	if ((c_opt_flags & C_OPT_LEVEL) > 0 && (c_opt_flags & C_OPT_MEM2SSA)) {
		ir_build_cfg(ctx);
		ir_build_dominators_tree(ctx);
		ir_mem2ssa(ctx);
		if (c_flags & C_DUMP_IR_AFTER_MEM2SSA) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after mem2ssa)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after mem2ssa)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
			}
		}
		ir_reset_cfg(ctx);
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 1) {
		ir_sccp(ctx);
		if (c_flags & C_DUMP_IR_AFTER_SCCP) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after sccp)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after sccp)\n");
				ir_save(ctx, c_save_flags, stderr);
			}
		}
	}

	ir_build_cfg(ctx);
	if (c_flags & C_DUMP_IR_AFTER_CFG) {
		if (c_flags & C_DUMP_DOT) {
			ir_dump_dot(ctx, yy_sym2str(name), "(after cfg)", stderr);
		} else {
			rcc_dump_func_proto(name, 0, stderr);
			fprintf(stderr, "# (after cfg)\n");
			ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
		}
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		ir_build_dominators_tree(ctx);
		if (c_flags & C_DUMP_IR_AFTER_DOM) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after dom)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after dom)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
			}
		}

		ir_find_loops(ctx);
		if (c_flags & C_DUMP_IR_AFTER_LOOP) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after loop)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after loop)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
			}
		}

		ir_gcm(ctx);
		if (c_flags & C_DUMP_IR_AFTER_GCM) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after gcm)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after gcm)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_CFG_MAP, stderr);
			}
		}

		ir_schedule(ctx);
		if (c_flags & C_DUMP_IR_AFTER_SCHEDULING) {
			if (c_flags & C_DUMP_DOT) {
				ir_dump_dot(ctx, yy_sym2str(name), "(after scheduling)", stderr);
			} else {
				rcc_dump_func_proto(name, 0, stderr);
				fprintf(stderr, "# (after scheduling)\n");
				ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
			}
		}
	}

	if ((c_opt_flags & C_OPT_INLINE)
	 && name != YY_MAIN
	 && !c_value_is_const(func)
	 && rcc_may_inline(func, ctx)) {
		sym->value.u.op |= C_VAL_INLINE;
		if (!RCC_DELAY_CODE_GEN) {
			goto delay_codegen;
		}
	}

	if (!RCC_DELAY_CODE_GEN) {
		rcc_ir_codegen(name, ctx, sym);
		ir_free(ctx);
	} else {
		if (((c_flags & C_SINGLE_FILE) ? name == YY_MAIN : sym->linkage == C_LINK_EXTERNAL)
		 && !sym->has_code) {
			sym->has_code = C_CODE_SCHEDULED;
			if (!ir_list_capasity(&c_codegen_list)) ir_list_init(&c_codegen_list, 32);
			ir_list_push(&c_codegen_list, name);
		}
delay_codegen:
		ir_ctx *copy = ir_mem_malloc(sizeof(ir_ctx));
		memcpy(copy, ctx, sizeof(ir_ctx));
		sym->ctx = copy;
	}
}

static void* c_linker_resolve_sym_name(ir_loader *loader, const char *name, uint32_t flags)
{
	uint32_t len;
	c_name id;
	c_sym *sym;

	if (!c_native) return NULL;

	len = (uint32_t)strlen(name);
	id = yy_hash_lookup(name, len);
	sym = yy_hash.data[id].sym;
	if (sym && (sym->linkage != C_LINK_INTERNAL && sym->linkage != C_LINK_EXTERNAL)) {
		sym = c_global_sym(sym);
	}
	if (sym && (sym->linkage == C_LINK_INTERNAL || sym->linkage == C_LINK_EXTERNAL)) {
		if (c_value_is_const(&sym->value)) {
			IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.ptr);
			return sym->value.u.val.ptr;
		}

		if (!sym->ctx) {
			/* pass */
		} else if (protected && (sym->has_code < C_CODE_STARTED)) {
			/* Generate code early to avoid linking through thunk */
			rcc_ir_codegen(id, sym->ctx, sym);
			if (c_value_is_const(&sym->value)) {
				return sym->value.u.val.ptr;
			}
		} else if (flags & IR_RESOLVE_SYM_ADD_THUNK) {
			goto add_thunk;
		}

		if (sym->linkage == C_LINK_EXTERNAL) {
			void *addr;

			addr = ir_resolve_sym_name(name);
			if (addr) {
				sym->is_external = 1;
				sym->value.u.opt = IR_OPT(C_VAL_CONST, IR_ADDR);
				sym->value.u.val.ptr = addr;
				if (c_flags & C_DUMP_ASM) {
					ir_disasm_add_symbol(name, (uint64_t)(uintptr_t)addr, IR_UNKNOWN_SIZE);
				}
				return addr;
			}
		}

		if (flags & IR_RESOLVE_SYM_ADD_THUNK) {
			/* Undefined declaration */
			size_t size;
			void *addr;

add_thunk:
			if (protected) {
				ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
			}
			addr = ir_emit_thunk(&c_code_buffer, NULL, &size);
			if (protected) {
				ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
			}
			if (!addr) {
				yy_error_fmt("internal error");
			}
			sym->is_thunk = 1;
			sym->value.u.op |= C_VAL_CONST;
			sym->value.u.type = IR_ADDR;
			sym->value.u.val.ptr = addr;
			if (sym->linkage == C_LINK_INTERNAL) {
				c_flags |= C_DO_LINK_INTERNAL;
			} else {
				IR_ASSERT(sym->linkage == C_LINK_EXTERNAL);
				c_flags |= C_DO_LINK_EXTERNAL;
			}
			if (c_flags & C_DUMP_ASM) {
				/* thunk and real symbol use the same name */
				ir_disasm_add_symbol(name, (uint64_t)(uintptr_t)addr, size);
			}
			if ((RCC_DELAY_CODE_GEN || sym->ctx) && !sym->has_code) {
				sym->has_code = C_CODE_SCHEDULED;
				if (!ir_list_capasity(&c_codegen_list)) ir_list_init(&c_codegen_list, 32);
				ir_list_push(&c_codegen_list, id);
			}
			return addr;
		}
	} else if (yy_hash.data[id].link && yy_hash.data[id].link->is_asm_name) {
		void *addr = (void*)yy_hash.data[id].link->addr;

		if (addr) return addr;

		addr = ir_resolve_sym_name(name);
		if (addr) {
			if (c_flags & C_DUMP_ASM) {
				ir_disasm_add_symbol(name, (uint64_t)(uintptr_t)addr, IR_UNKNOWN_SIZE);
			}
			yy_hash.data[id].link->addr = addr;
			return addr;
		}
	}

	if (!(flags & IR_RESOLVE_SYM_SILENT)) {
		yy_error_fmt("undefined symbol \"%s\"", name);
	}
	return NULL;
}

void *c_linker_allocate_data(const char *name, size_t size)
{
	void *data = ir_arena_alloc(&c_linker_arena, size);
	if (UNEXPECTED(!data)) yy_error("not enough memory to allocate data");
	if ((c_flags & C_DUMP_ASM) && name) {
		ir_disasm_add_symbol(name, (uintptr_t)data, size);
	}
	memset(data, 0, size);
	return data;
}

static void c_linker_add_reloc(c_sym *obj, size_t obj_offset, c_name name, size_t name_offset)
{
	c_reloc *reloc = ir_arena_alloc(&yy_arena, sizeof(c_reloc));

	if (c_value_is_ref(&obj->value)) {
		size_t len;
		const char *str = ir_get_strl(active_ctx, active_ctx->ir_base[obj->value.u.ref].val.name, &len);
		c_name sym = yy_hash_find(str, len);
		obj = yy_hash.data[sym].sym;
	}

	reloc->obj_offset = obj_offset;
	reloc->name = name;
	reloc->name_offset = name_offset;
	reloc->next = obj->reloc;
	obj->reloc = reloc;
}

static bool c_linker_add_label(ir_loader *loader, const char *str, void *addr)
{
	c_name name;
	c_sym *sym;
	ir_val val;

	name = yy_hash_lookup(str, strlen(str));
	IR_ASSERT(!yy_hash.data[name].sym);

	/* Create a global symbol in yy_arena */
	sym = ir_arena_alloc(&yy_arena, sizeof(c_sym));
	memset(sym, 0, sizeof(c_sym));
	sym->kind = C_SYM_VAR;
	sym->linkage = C_LINK_INTERNAL;
	sym->is_thread_local = 0;
	sym->is_implemented = 1;
	val.ptr = addr;
	c_value_set_const(&sym->value, &c_type_const_ptr, IR_ADDR, val);
	yy_hash.data[name].sym = sym;
	return 1;
}

static ir_insn *c_linker_find_sym_offset(ir_insn *insn, size_t *offset)
{
	if (insn->op == IR_SYM) {
		return insn;
	} else if (insn->op == IR_BITCAST) {
		return c_linker_find_sym_offset(&active_ctx->ir_base[insn->op1], offset);
	} else if (insn->op == IR_ADD) {
		if (IR_IS_CONST_REF(insn->op2) && !IR_IS_SYM_CONST(active_ctx->ir_base[insn->op2].op)) {
			ir_insn *sym = c_linker_find_sym_offset(&active_ctx->ir_base[insn->op1], offset);
			if (sym) {
				*offset += active_ctx->ir_base[insn->op2].val.u64;
				return sym;
			}
		} else if (IR_IS_CONST_REF(insn->op1) && !IR_IS_SYM_CONST(active_ctx->ir_base[insn->op1].op)) {
			ir_insn *sym = c_linker_find_sym_offset(&active_ctx->ir_base[insn->op2], offset);
			if (sym) {
				*offset += active_ctx->ir_base[insn->op1].val.u64;
				return sym;
			}
		}
	} else if (insn->op == IR_SUB) {
		if (IR_IS_CONST_REF(insn->op2) && !IR_IS_SYM_CONST(active_ctx->ir_base[insn->op2].op)) {
			ir_insn *sym = c_linker_find_sym_offset(&active_ctx->ir_base[insn->op1], offset);
			if (sym) {
				*offset -= active_ctx->ir_base[insn->op2].val.u64;
				return sym;
			}
		}
	}
	return NULL;
}

bool c_linker_fix_reloc(c_sym *obj, size_t obj_offset, c_value *val)
{
	ir_insn *addr_insn;
	size_t offset = 0;

	if ((val->type->kind == C_TYPE_STRUCT || val->type->kind == C_TYPE_UNION || val->type->kind == C_TYPE_ARRAY)
	 && IR_IS_CONST_REF(val->u.ref)
	 && val->u.val.ptr) {
		/* Use a copy of struct/union value */
		val->u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
		val->u.ref = IR_UNUSED;
		return 1;
	}
	if (IR_IS_CONST_REF(val->u.ref) && !IR_IS_SYM_CONST(active_ctx->ir_base[val->u.ref].op)) {
		c_value_set_const(val, val->type, val->u.type, active_ctx->ir_base[val->u.ref].val);
		return 1;
	}
	if (val->type->kind != C_TYPE_POINTER
	 && (!C_IS_TYPE_INT(val->type) || c_value_is_lval(val) || val->type->size != sizeof(void*))) {
		return 0;
	}

	addr_insn = &active_ctx->ir_base[val->u.ref];
	if (!IR_IS_CONST_REF(val->u.ref)) {
		addr_insn = c_linker_find_sym_offset(addr_insn, &offset);
		if (!addr_insn) return 0;
	}

	if (addr_insn->op == IR_SYM || addr_insn->op == IR_FUNC) {
		size_t len;
		const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
		c_name n = yy_hash_lookup(name, len);
		c_sym *sym = yy_hash.data[n].sym;

		IR_ASSERT(sym && (sym->kind == C_SYM_VAR || (sym->kind == C_SYM_FUNC && offset == 0)));
		if (c_flags & C_DUMP_IR) {
			c_linker_add_reloc(obj, obj_offset, n, offset);
		}
		if (c_value_is_const(&sym->value)) {
			IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.addr);
			val->u.val.addr = sym->value.u.val.addr + offset;
		} else {
			val->u.val.addr = 0;
			if (sym->kind == C_SYM_FUNC && !sym->has_code) {
				sym->has_code = C_CODE_SCHEDULED;
				if (!ir_list_capasity(&c_codegen_list)) ir_list_init(&c_codegen_list, 32);
				ir_list_push(&c_codegen_list, n);
			}
			if (c_flags & C_RUN) {
				if (!(c_flags & C_DUMP_IR)) {
					/* reloc was already added before */
					c_linker_add_reloc(obj, obj_offset, n, offset);
				}
				if (sym->linkage == C_LINK_INTERNAL) {
					c_flags |= C_DO_LINK_INTERNAL;
				} else {
					IR_ASSERT(sym->linkage == C_LINK_EXTERNAL);
					c_flags |= C_DO_LINK_EXTERNAL;
				}
			}
		}
		return 1;
	} else if (addr_insn->op == IR_LABEL) {
		size_t len;
		const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
		c_name n = yy_hash_lookup(name, len);

		if (c_flags & (C_DUMP_IR|C_RUN)) {
			c_linker_add_reloc(obj, obj_offset, n, 0);
		}
		if (c_flags & C_RUN) {
			c_flags |= C_DO_LINK_INTERNAL;
		}

		/* Disable inlining */
		c_type *t = (c_type*)active_func->value.type;
		if (t->attr & (C_ATTR_ALWAYS_INLINE|C_ATTR_INLINE)) {
			yy_warning_fmt("function \"%s\" can never be inlined because it saves address of local label in a static variable",
				yy_sym2str(active_func_name));
		}
		t->attr |= C_ATTR_NOINLINE;
		t->attr &= ~(C_ATTR_ALWAYS_INLINE|C_ATTR_INLINE);
		return 1;
#if 0
	} else if (addr_insn->op == IR_FUNC) {
		size_t len;
		const char *name = ir_get_strl(active_ctx, addr_insn->val.name, &len);
		c_name n = yy_hash_lookup(name, len);
		c_sym *sym = yy_hash.data[n].sym;

		IR_ASSERT(sym && sym->kind == C_SYM_FUNC && offset == 0);
		if (c_flags & C_DUMP_IR) {
			c_linker_add_reloc(obj, obj_offset, n, 0);
		}
		if (c_value_is_const(&sym->value)) {
			IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.addr);
			val->u.val.addr = sym->value.u.val.addr;
		} else {
			if (!c_native) return 1;
			/* resolve name or add thunk */
			void *addr = c_linker_resolve_sym_name(NULL, name, IR_RESOLVE_SYM_ADD_THUNK);
			IR_ASSERT(addr);
			IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.addr == (uintptr_t)addr);
			val->u.val.addr = (uintptr_t)addr;
		}
		return 1;
#endif
	}
	return 0;
}

ir_loader c_linker = {
	.resolve_sym_name = c_linker_resolve_sym_name,
	.add_label        = c_linker_add_label,
};

void rcc_ir_init(ir_ctx *ctx, uint32_t flags)
{
	flags |= IR_FUNCTION;
	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		flags |= IR_OPT_FOLDING | IR_OPT_CFG | IR_OPT_CODEGEN;
	}
	flags |= ir_flags;
	ir_init(ctx, flags, 256, 1024);
	ctx->mflags = ir_mflags;
	ctx->fixed_regset = ~ir_debug_regset | c_fixed_regset;
	ctx->loader = &c_linker;
}

static const char *_sym_name = {
#define _YY_SYM(str, id) str "\0"
_YY_SYMBOLS(_YY_SYM)
_YY_KEYWORDS(_YY_SYM)
_YY_DIRECTIVES(_YY_SYM)
_YY_NAMES(_YY_SYM)
"\0"
#undef _YY_SYM
};

void rcc_init(void)
{
	yy_sym i;
	const char *s;
	size_t len;
	yy_hash_bucket *b;

	yy_arena = ir_arena_create(4096);
	yy_hash_init();

	memset(yy_hash.data, 0, YY_FIRST_KEYWORD * sizeof(yy_hash_bucket));
	i = 0;
	s = _sym_name;
	b = yy_hash.data;
	for (i = 0; i < YY_FIRST_KEYWORD; i++) {
		len = strlen(s);
		b->str = s;
		b->len = len;
		s += len + 1;
		b++;
	}
	yy_hash.count = i;
	while (*s) {
		len = strlen(s);
		yy_hash_lookup(s, len);
		s += len + 1;
	}

	pp_macro_define(YY___COUNTER__,         PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___DATE__,            PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FILE__,            PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FUNCTION__,        PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___PRETTY_FUNCTION__, PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FUNC__,            PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___LINE__,            PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___TIME__,            PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___INCLUDE_LEVEL__,   PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___BASE_FILE__,       PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___HAS_ATTRIBUTE,     PP_MACRO_BUILTIN, 1, NULL);
	pp_macro_define(YY___HAS_BUILTIN,       PP_MACRO_BUILTIN, 1, NULL);

	pp_start();
	c_stdinc_init();
	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		yy_pos = yy_text = yy_linepos = yy_buf = "#define __OPTIMIZE__ 1\n";
		yy_end = yy_buf + strlen(yy_buf);
		do {
			i = yy_next();
		} while (i != YY_EOF);
	}
	pp_dtor();

	c_arena = ir_arena_create(4096);
	c_linker_arena = ir_arena_create(4096);

	c_type *type;
	c_dcl dcl;
	memset(&dcl, 0, sizeof(dcl));
	dcl.flags = C_DCL_EXTERN | C_TYPE_SPEC_TYPE;

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	if (!type) yy_error("out of memory");
	memset(type, 0, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->func.ret_type = &c_type_ptr;
	type->func.num_params = 3;
	type->func.params = ir_arena_alloc(&c_arena, sizeof(c_param) * 3);
	type->func.params[0].name = 0;
	type->func.params[0].type = &c_type_ptr;
	type->func.params[1].name = 0;
	type->func.params[1].type = &c_type_const_ptr;
	type->func.params[2].name = 0;
	type->func.params[2].type = &c_type_size_t;
	dcl.type = type;

	c_declare(YY_MEMCPY, &dcl);

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	if (!type) yy_error("out of memory");
	memset(type, 0, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->func.ret_type = &c_type_ptr;
	type->func.num_params = 3;
	type->func.params = ir_arena_alloc(&c_arena, sizeof(c_param) * 3);
	type->func.params[0].name = 0;
	type->func.params[0].type = &c_type_ptr;
	type->func.params[1].name = 0;
	type->func.params[1].type = &c_type_i32;
	type->func.params[2].name = 0;
	type->func.params[2].type = &c_type_size_t;
	dcl.type = type;

	c_declare(YY_MEMSET, &dcl);
}

void rcc_free(void)
{
	while (pp_list_cache_idx > 0) {
		pp_list_cache_idx--;
		ir_mem_free(pp_list_cache[pp_list_cache_idx].syms);
	}
	yy_hash_free();
	if (c_linker_arena) ir_arena_free(c_linker_arena);
	if (c_arena) ir_arena_free(c_arena);
	if (yy_arena) ir_arena_free(yy_arena);
}

static int rcc_read(const char *file_name)
{
	int fd;
	size_t size, ret;
	char *buf;
	struct stat stat_buf;

	fd = open(file_name, O_RDONLY | O_BINARY);
	if (fd < 0) {
		fprintf(stderr, "ERROR: Cannot open input file \"%s\"\n", file_name);
		return 0;
	}

	if (fstat(fd, &stat_buf) != 0) {
		fprintf(stderr, "ERROR: Cannot get size of input file \"%s\"\n", file_name);
		return 0;
	}
	size = stat_buf.st_size;

	/* Allocate two additional bytes for "\n\0" */
	buf = ir_mem_malloc(size + 2);
	if (!buf) {
		fprintf(stderr, "ERROR: Cannot allocate buffer to read file \"%s\"\n", file_name);
		return 0;
	}

	ret = read(fd, buf, size);
	close(fd);

	if (ret != size) {
		ir_mem_free(buf);
		fprintf(stderr, "ERROR: Cannot read file \"%s\"\n", file_name);
		return 0;
	}

	if (size && buf[size - 1] != '\n') buf[size++] = '\n';
	buf[size] = '\0'; /* End marker */

	yy_pos = yy_text = yy_linepos = yy_buf = buf;
	yy_len = 0;
	yy_line = 1;
	yy_end = yy_buf + size;
	yy_file_name = yy_hash_lookup(file_name, strlen(file_name));

	/* Skip UTF-8 BOM */
	if (yy_pos[0] == '\xef' && yy_pos[1] == '\xbb' && yy_pos[2] == '\xbf') {
		yy_pos = yy_text = yy_linepos = yy_pos + 3;
	}

	pp_start();

	return 1;
}

static void rcc_dtor(void)
{
	uint32_t i;
	yy_hash_bucket *p;

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_FUNC && p->sym->ctx) {
			ir_free(p->sym->ctx);
			ir_mem_free(p->sym->ctx);
			p->sym->ctx = NULL;
		}
	}
	pp_dtor();
	ir_mem_free((void*)yy_buf);
}

static void rcc_fix_flexible_data(void)
{
	uint32_t i;
	yy_hash_bucket *p;

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym
		 && p->sym->kind == C_SYM_VAR
		 && !p->sym->is_string
		 && p->sym->value.type
		 && (p->sym->value.type->attr & C_ATTR_FLEXIBLE)) {
			c_type *type;

			/* Convert "flexible" array to regular */
			IR_ASSERT(p->sym->value.type->kind == C_TYPE_ARRAY);
			type = ir_arena_alloc(&c_arena, sizeof(c_type));
            *type = *p->sym->value.type;
			type->size = type->array.type->size;
			type->array.length = 1;
			type->attr &= ~C_ATTR_FLEXIBLE;
			p->sym->value.type = type;

			p->sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
			p->sym->value.u.val.ptr = c_linker_allocate_data(p->str, type->size);
			p->sym->is_implemented = 1;

			//yy_warning_fmt("array \"%s\" assumed to have one element", p->str); // error position ???
		}
	}
}

static bool c_is_type_const(const c_type *type)
{
	if (type->attr & C_ATTR_CONST) {
		return 1;
	} else if (type->kind == C_TYPE_ARRAY) {
		return c_is_type_const(type->array.type);
	}
	return 0;
}

static size_t rcc_emit_ir_data(FILE *f, const c_type *type, const void *addr, size_t base, c_reloc *rel)
{
	while (rel && rel->obj_offset < base) {
		rel = rel->next;
	}
	if (C_IS_TYPE_KIND_SCALAR(type->kind) || type->kind == C_TYPE_ENUM) {
		ir_type t = c_type2ir(type);
		size_t size = ir_type_size[t];

		if (rel && rel->obj_offset == base) {
			IR_ASSERT(size == sizeof(void*));
			if (rel->name_offset) {
				fprintf(f, "\tuintptr_t sym(@%s)+%lld,\n", yy_sym2str(rel->name), (long long)rel->name_offset);
			} else {
				fprintf(f, "\tuintptr_t sym(@%s),\n", yy_sym2str(rel->name));
			}
			return size;
		}

		switch (size) {
			case 1:
				if (!*(uint8_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%02x,\n", ir_type_cname[t], (uint32_t)*(uint8_t*)addr);
				}
				return 1;
			case 2:
				if (!*(uint16_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%04x,\n", ir_type_cname[t], (uint32_t)*(uint16_t*)addr);
				}
				return 2;
			case 4:
				if (!*(uint32_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%08x,\n", ir_type_cname[t], *(uint32_t*)addr);
				}
				return 4;
			case 8:
				if (!*(uint64_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%016" PRIx64 ",\n", ir_type_cname[t], *(uint64_t*)addr);
				}
				return 8;
			default:
				IR_ASSERT(0);
		}
	} else if (type->kind == C_TYPE_POINTER) {
		if (rel && rel->obj_offset == base) {
			if (rel->name_offset) {
				fprintf(f, "\tuintptr_t sym(@%s)+%lld,\n", yy_sym2str(rel->name), (long long)rel->name_offset);
			} else {
				fprintf(f, "\tuintptr_t sym(@%s),\n", yy_sym2str(rel->name));
			}
		} else if (!*(uintptr_t*)addr) {
			fprintf(f, "\tuintptr_t 0,\n");
		} else {
			fprintf(f, "\tuintptr_t 0x016%" PRIxPTR ",\n", *(uintptr_t*)addr);
		}
		return sizeof(void*);
	} else if (type->kind == C_TYPE_FUNC) {
		if (rel && rel->obj_offset == base) {
			fprintf(f, "\tuintptr_t func(@%s),\n", yy_sym2str(rel->name));
		} else if (!*(uintptr_t*)addr) {
			fprintf(f, "\tuintptr_t 0,\n");
		} else {
			fprintf(f, "\tuintptr_t 0x%" PRIxPTR ",\n", *(uintptr_t*)addr);
		}
		return sizeof(void*);
	} else if (type->kind == C_TYPE_ARRAY) {
		size_t offset = 0, el_offset = 0;
		int i;

		IR_ASSERT(!(type->attr & C_ATTR_VLA));
		for (i = 0; i < type->array.length; el_offset += type->array.type->size, i++) {
			while (offset < el_offset) {
				/* padding */
				fprintf(f, "\tuint8_t 0,\n");
				offset++;
			}
			offset += rcc_emit_ir_data(f, type->array.type, (const char*)addr + el_offset, base + el_offset, rel);
		}
		return offset;
	} else if (type->kind == C_TYPE_STRUCT) {
		size_t offset = 0;
		const c_field *field = type->record.fields;
		int i;

		for (i = 0; i < type->record.num_fields; field++, i++) {
			while (offset < field->offset) {
				fprintf(f, "\tuint8_t 0,\n");
				/* padding */
				offset++;
			}
			if (C_IS_BIT_FIELD(field->bit_field)) {
				while (i + 1 < type->record.num_fields
				 && C_IS_BIT_FIELD((field + 1)->bit_field)
				 && (field + 1)->offset == offset) {
					field++;
					i++;
				}
				size_t next = (i + 1 < type->record.num_fields) ? (field + 1)->offset : type->size;
				while (offset < next) {
					offset += rcc_emit_ir_data(f, &c_type_u8, (const char*)addr + offset, base + offset, rel);
				}
			} else {
				offset += rcc_emit_ir_data(f, field->type, (const char*)addr + field->offset, base + field->offset, rel);
			}
		}
		while (offset < type->size) {
			fprintf(f, "\tuint8_t 0,\n");
			/* padding */
			offset++;
		}
		return offset;
	} else if (type->kind == C_TYPE_UNION) {
		const c_field *field = type->record.fields;
		const c_field *best_field = NULL;
		size_t best_size = 0;
		size_t offset = 0;
		int i;

		for (i = 0; i < type->record.num_fields; field++, i++) {
			if (field->type->size > best_size) {
				best_field = field;
				best_size = field->type->size;
			}
		}
		if (best_field) {
			offset += rcc_emit_ir_data(f, best_field->type, addr, base, rel);
		}
		while (offset < type->size) {
			fprintf(f, "\tuint8_t 0,\n");
			/* padding */
			offset++;
		}
		return offset;
	} else {
		IR_ASSERT(0);
	}

	return 0;
}

static void rcc_emit_ir_mbstring(FILE *f, const c_type *type, const void *addr, int len)
{
	ir_type t = c_type2ir(type);
	size_t size = ir_type_size[t];

	switch (size) {
		case 2:
			while (len > 0) {
				if (!*(uint16_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%04x,\n", ir_type_cname[t], (uint32_t)*(uint16_t*)addr);
				}
				addr = (const char*)addr + 2;
				len -= 2;
			}
			break;
		case 4:
			while (len > 0) {
				if (!*(uint32_t*)addr) {
					fprintf(f, "\t%s 0,\n", ir_type_cname[t]);
				} else {
					fprintf(f, "\t%s 0x%08x,\n", ir_type_cname[t], *(uint32_t*)addr);
				}
				addr = (const char*)addr + 4;
				len -= 4;
			}
			break;
		default:
			IR_ASSERT(0);
	}
}

static void rcc_sort_relocs(c_sym *sym)
{
	c_reloc *first, *last, *next, *rel;

	first = last = sym->reloc;
	next = first->next;
	first->next = NULL;
	while (next) {
		rel = next;
		next = rel->next;
		if (rel->obj_offset < first->obj_offset) {
			rel->next = first;
			first = rel;
		} else if(rel->obj_offset > last->obj_offset) {
			last->next = rel;
			rel->next = NULL;
			last = rel;
		} else {
			c_reloc *q = NULL, *p = first;
			while (rel->obj_offset > p->obj_offset) {
				q = p;
				p = p->next;
			}
			q->next = rel;
			rel->next = p;
		}
	}
	sym->reloc = first;
}

static void rcc_emit_ir(FILE *f)
{
	uint32_t i;
	yy_hash_bucket *p;

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_FUNC) {
			if ((p->sym->is_external || !p->sym->ctx) && p->sym->alias) continue;
			rcc_dump_func_proto(i, 1, f);
		}
	}

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_VAR) {
			size_t size;

			if (p->sym->linkage == C_LINK_INTERNAL) {
				fprintf(f, "static ");
			} else if (p->sym->is_external || !c_value_is_set(&p->sym->value)) {
				if (p->sym->alias) continue;
				fprintf(f, "extern %s @%s;\n",
					(p->sym->is_string || c_is_type_const(p->sym->value.type)) ? "const" : "var",
					yy_sym2str(i));
				continue;
			}
			size = p->sym->is_string ? (size_t)p->sym->value.u.ref : p->sym->value.type->size;
			fprintf(f, "%s @%s[%" PRIuPTR "]%s",
				(p->sym->is_string || c_is_type_const(p->sym->value.type)) ? "const" : "var",
				yy_sym2str(p->sym->alias ? p->sym->alias : i),
				size,
				p->sym->is_implemented ?
					((p->sym->is_string && p->sym->value.type->array.type->size == 1) ? " = \"" : " = {\n") : ";\n");
			if (p->sym->is_implemented) {
				if (p->sym->reloc) {
					rcc_sort_relocs(p->sym);
				}
				if (!p->sym->is_string) {
					rcc_emit_ir_data(f, p->sym->value.type, p->sym->value.u.val.ptr, 0, p->sym->reloc);
					fprintf(f, "};\n");
				} else if (p->sym->value.type->array.type->size > 1) {
					rcc_emit_ir_mbstring(f, p->sym->value.type->array.type, p->sym->value.u.val.ptr, size);
					fprintf(f, "};\n");
				} else {
					ir_print_escaped_str(p->sym->value.u.val.ptr, size, f);
					fprintf(f, "\";\n");
				}
			}
		}
	}

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_FUNC && p->sym->ctx) {
			rcc_dump_func_proto(i, 0, f);
			ir_save(p->sym->ctx, c_save_flags, f);
		}
	}
}

static void rcc_emit_llvm_proto(const char *name, c_sym *func, FILE *f)
{
	const c_type *t = func->value.type;
	uint32_t flags;
	ir_type ret_type;
	uint32_t params_count;
	uint8_t *param_types;

	IR_ASSERT(t->kind == C_TYPE_FUNC);
	param_types = alloca(t->func.num_params + 16);
	c_type2proto_ex(t, &flags, &ret_type, &params_count, param_types);
	if (func->linkage == C_LINK_INTERNAL) {
		flags |= IR_STATIC;
	} else if (func->linkage == C_LINK_BUILTIN) {
		flags |= IR_CC_BUILTIN;
	}
	ir_emit_llvm_func_decl(name, flags, ret_type, params_count, param_types, f);
}

static void rcc_emit_llvm(FILE *f)
{
	uint32_t i;
	yy_hash_bucket *p;

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_FUNC && !p->sym->ctx) {
			if ((p->sym->is_external || !p->sym->ctx) && p->sym->alias) continue;
			rcc_emit_llvm_proto(p->str, p->sym, f);
		}
	}

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_VAR) {
			uint32_t flags = 0;
			const char *str;

			if (p->sym->linkage == C_LINK_INTERNAL) {
				flags |= IR_STATIC;
			} else if (p->sym->is_external || !c_value_is_set(&p->sym->value)) {
				flags |= IR_EXTERN;
			}
			if (c_is_type_const(p->sym->value.type)) {
				flags |= IR_CONST;
			}
			if (p->sym->alias) {
				str = yy_hash.data[p->sym->alias].str;
			} else {
				str = p->str;
			}
			//TODO: type ???
			ir_emit_llvm_sym_decl(str, flags, f);
			//TODO: initializer ???
		}
	}

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym && p->sym->kind == C_SYM_FUNC && p->sym->ctx) {
			ir_emit_llvm(p->sym->ctx, p->str, f);
		}
	}
}

static int rcc_preprocess(const char *file_name, FILE *f)
{
	if (!rcc_read(file_name)) {
		return 0;
	}
	pp_preprocess(f);
	rcc_dtor();
	return 1;
}

static int rcc_compile(const char *file_name)
{
	memset(&c_codegen_list, 0, sizeof(ir_list));
	c_do_compile_start();
	c_stdinc_builtin();

	if (!rcc_read(file_name)) {
		return 0;
	}
	rcc_parse();
	rcc_fix_flexible_data();
	if (c_flags & C_DUMP_IR) {
		rcc_emit_ir(c_out);
	}
	if (c_flags & C_DUMP_LLVM) {
		rcc_emit_llvm(c_out);
	}
	if (ir_list_capasity(&c_codegen_list)) {
		do {
			c_name name = ir_list_pop(&c_codegen_list);
			c_sym *sym = yy_hash.data[name].sym;
			if (sym && sym->ctx) {
				if (sym->value.u.val.ptr && !sym->is_thunk && !sym->is_external) continue; // already done ???
				rcc_ir_codegen(name, sym->ctx, sym);
			}
		} while (ir_list_len(&c_codegen_list));
		ir_list_free(&c_codegen_list);
	}
	c_do_compile_end();
	rcc_dtor();
	return 1;
}

static void rcc_link_internal(void)
{
	yy_sym i;
	yy_hash_bucket *p, *q;
	c_reloc *reloc;
	c_sym *sym;

	ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->sym) {
			if (p->sym->is_thunk) {
				if (p->sym->linkage == C_LINK_INTERNAL) {
					yy_error_fmt("Unresolved symbol \"%s\"", p->str);
				} else {
					IR_ASSERT(p->sym->linkage == C_LINK_EXTERNAL);
					if (c_flags & C_SINGLE_FILE) {
						void *addr = ir_resolve_sym_name(p->str);
						if (!addr) {
							yy_error_fmt("Unresolved symbol \"%s\"", p->str);
						} else {
							ir_fix_thunk((void*)p->sym->value.u.val.addr, addr);
							p->sym->value.u.val.addr = (uintptr_t)addr;
							p->sym->is_thunk = 0;
						}
					}
				}
			}
			reloc = p->sym->reloc;
			while (reloc) {
				q = &yy_hash.data[reloc->name];
				sym = q->sym;
				if (!sym) yy_error_fmt("Unresolved symbol \"%s\"", q->str);
				if (sym->linkage == C_LINK_INTERNAL) {
					if (!c_value_is_const(&sym->value) || sym->is_thunk) yy_error_fmt("Unresolved symbol \"%s\"", q->str);
					IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.addr);
					*(void**)((char*)p->sym->value.u.val.addr + reloc->obj_offset) =
						(char*)sym->value.u.val.addr + reloc->name_offset;
				} else {
					IR_ASSERT(sym->linkage == C_LINK_EXTERNAL);
					if (c_value_is_const(&sym->value)
					 && !sym->is_thunk) {
						IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.addr);
						*(void**)((char*)p->sym->value.u.val.addr + reloc->obj_offset) =
							(char*)sym->value.u.val.addr + reloc->name_offset;
					} else if (c_flags & C_SINGLE_FILE) {
						void *addr = ir_resolve_sym_name(q->str);
						if (!addr) {
							yy_error_fmt("Unresolved symbol \"%s\"", q->str);
						} else {
							if (sym->is_thunk) {
								ir_fix_thunk((void*)sym->value.u.val.addr, addr);
								sym->is_thunk = 0;
							}
							sym->value.u.optx = IR_OPT(C_VAL_CONST, IR_ADDR);
							sym->value.u.val.addr = (uintptr_t)addr;
							*(void**)((char*)p->sym->value.u.val.addr + reloc->obj_offset) =
								(char*)sym->value.u.val.addr + reloc->name_offset;
						}
					}
				}
				reloc = reloc->next;
			}
		}
	}
	ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
}

static void rcc_link(void)
{
	yy_sym i;
	yy_hash_bucket *p, *q;
	c_linker_sym *link;
	c_reloc *reloc;
	void *addr;

	ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		link = p->link;
		if (link) {
			if (!link->addr || link->is_thunk) {
				addr = ir_resolve_sym_name(p->str);
				if (!addr) yy_error_fmt("Unresolved symbol \"%s\"", p->str);
				if (link->is_thunk) ir_fix_thunk((void*)link->addr, addr);
				link->addr = addr;
				link->is_thunk = 0;
			}
			reloc = link->reloc;
			while (reloc) {
				q = &yy_hash.data[reloc->name];
				link = q->link;
				if (!link) {
					addr = ir_resolve_sym_name(q->str);
					if (!addr) yy_error_fmt("Unresolved symbol \"%s\"", q->str);
					link = ir_arena_alloc(&yy_arena, sizeof(c_linker_sym));
					link->addr = addr;
					link->reloc = NULL;
					link->is_thunk = 0;
					link->is_asm_name = 0;
					q->link = link;
				} else if (!link->addr || link->is_thunk) {
					addr = ir_resolve_sym_name(q->str);
					if (!addr) yy_error_fmt("Unresolved symbol \"%s\"", q->str);
					if (link->is_thunk) ir_fix_thunk((void*)link->addr, addr);
					link->addr = addr;
					link->is_thunk = 0;
				}
				*(void**)((char*)p->link->addr + reloc->obj_offset) = (char*)link->addr + reloc->name_offset;
				reloc = reloc->next;
			}
		}
	}
	ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
}

static struct {
	uint32_t    num_syms;
	void       *checkpoint;
} c_init_state;

static void rcc_remember_state(void)
{
	yy_sym i;
	yy_hash_bucket *p;

	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < yy_hash.count; p++, i++) {
		if (p->macro) p->macro->flags |= PP_MACRO_PREDEFINED;
		IR_ASSERT(!p->macro_stack);
		IR_ASSERT(!p->sym || i == YY_MEMCPY || i == YY_MEMSET);
		IR_ASSERT(!p->tag);
		IR_ASSERT(!p->label);
	}
	c_init_state.num_syms = yy_hash.count;
	c_init_state.checkpoint = ir_arena_checkpoint(c_arena);
}

static void rcc_update_link(yy_hash_bucket *p)
{
	c_sym *sym = p->sym;

	if (!sym->value.u.val.ptr) {
		if (!sym->alias) return;
		if (yy_hash.data[sym->alias].link) {
			p->link = yy_hash.data[sym->alias].link;
			return;
		}
		sym = yy_hash.data[sym->alias].sym;
		if (!sym || !sym->value.u.val.ptr) return;
	}

	if (p->link) {
		if (!p->link->addr) {
			p->link->addr = sym->value.u.val.ptr;
			p->link->is_thunk = sym->is_thunk;
		} else if (sym->is_thunk) {
			if (!p->link->is_thunk || sym->value.u.val.ptr != p->link->addr) {
				ir_fix_thunk((void*)sym->value.u.val.ptr, (void*)p->link->addr);
			}
		} else if (p->link->is_thunk) {
			ir_fix_thunk((void*)p->link->addr, (void*)sym->value.u.val.ptr);
			p->link->addr = sym->value.u.val.ptr;
			p->link->is_thunk = 0;
		} else if (sym->value.u.val.ptr != p->link->addr) {
			yy_error_fmt("redefined symbol \"%s\"", p->str);
		}
	} else {
		c_linker_sym *link = ir_arena_alloc(&yy_arena, sizeof(c_linker_sym));

		link->addr = sym->value.u.val.ptr;
		link->reloc = sym->reloc;
		link->is_thunk = sym->is_thunk;
		link->is_asm_name = 0;
		p->link = link;
	}
}

static void rcc_reset_state(void)
{
	yy_sym i;
	yy_hash_bucket *p;

	if (c_flags & C_RUN) {
		ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	}
	for (i = YY_LAST_KEYWORD + 1, p = yy_hash.data + i; i < c_init_state.num_syms; p++, i++) {
		if (p->macro && !(p->macro->flags & PP_MACRO_PREDEFINED)) {
			p->macro = NULL;
		}
		p->macro_stack = NULL;
		p->tag = NULL;
		p->label = NULL;
		if (i != YY_MEMCPY && i != YY_MEMSET) {
			if ((c_flags & C_RUN)
			 && p->sym
			 && !p->sym->has_asm_name
			 && (p->sym->linkage == C_LINK_EXTERNAL
			  || (p->sym->kind == C_SYM_VAR && p->sym->reloc))) {
				rcc_update_link(p);
			}
			p->sym = NULL;
		}
	}
	for (; i < yy_hash.count; p++, i++) {
		if ((c_flags & C_RUN)
		 && p->sym
		 && !p->sym->has_asm_name
		 && (p->sym->linkage == C_LINK_EXTERNAL
		  || (p->sym->kind == C_SYM_VAR && p->sym->reloc))) {
			rcc_update_link(p);
		}
		p->macro = NULL;
		p->macro_stack = NULL;
		p->sym = NULL;
		p->tag = NULL;
		p->label = NULL;
	}
	ir_arena_release(&c_arena, c_init_state.checkpoint);
	if (c_flags & C_RUN) {
		ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	}
}

#ifndef _WIN32
static double rcc_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}
#else
static double rcc_time(void)
{
	FILETIME filetime;

	GetSystemTimeAsFileTime(&filetime);
	return (double)((((uint64_t)filetime.dwHighDateTime << 32) | (uint64_t)filetime.dwLowDateTime)/10) /
		1000000.0;
}
#endif

static double rcc_atexit_start = 0.0;

static void rcc_atexit(void)
{
	if (rcc_atexit_start) {
		double t = rcc_time();
		fflush(stdout);
		fprintf(stderr, "\nexecution time = %0.6f\n", t - rcc_atexit_start);
		rcc_atexit_start = 0.0;
	}
}

static void rcc_help(const char *cmd)
{
	printf(
		"Usage: %s [options] input-file(s) [--run ...]\n"
		"General Options:\n"
		"  --run ...                  - run the main() function of generated code\n"
		"                               (the remaining arguments are passed to main)\n"
		"  --emit-ir                  - show generated and optimized IR code\n"
		"  -S                         - show generated assembler code\n"
		"  -o <file-name>             - put primary output into the specified file\n"
		"Preprocessor Options:\n"
		"  -E                         - preprocess only\n"
		"  -P                         - inhibit generation of linemarkers\n"
		"  -D name[=value]            - define macro\n"
		"  -U name                    - undefine macro\n"
		"  -I                         - add include directory\n"
		"  -dM                        - generate list of #define directives\n"
		"  -dN                        - generate list of #define directives (names only)\n"
		"  -dD                        - preprocess and generate list of #define directives\n"
		"  -dI                        - preprocess and generate list of #include directives\n"
		"Error Reporting Options:\n"
		"  -w                         - inhibit all warning messages\n"
		"  -fsyntax-only              - check the input files for syntax errors, but don't do anything beyond that\n"
		"Optimization Options:\n"
		"  -O[012]                    - optimization level (default: -O2)\n"
		"  -f[no-]inline              - enable/disable function inlining (default: enabled at -O1)\n"
		"  -fno-mem2ssa               - disable MEM2SSA pass (default: enabled at -O1)\n"
		"Code Generation Options:\n"
#if defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
		"  -mavx                      - use AVX instruction set\n"
		"  -m[no-]bmi1                - enable/disable BMI1 instruction set\n"
#endif
		"  -muse-fp                   - use base frame pointer register\n"
#if defined(IR_TARGET_X86)
		"  -mfastcall                 - use fastcall calling convention\n"
#endif
#ifndef _WIN32
		"Debugguing Options:\n"
		"  -g                         - produce debugging information (through JITGDB)\n"
		"  -p                         - provide information about JIT-ed code to Linux Perf\n"
		"                               the example usage:\n"
		"                                 $ perf record -k 1 rcc -p bench.c --run\n"
		"                                 $ perf inject -j -i perf.data -o perf.data.jitted\n"
		"                                 $ perf report -i perf.data.jitted\n"
#endif
		"IR Debugging Options:\n"
		"  --save                     - save IR\n"
		"  --save-cfg                 - save IR with information about CFG\n"
		"  --save-cfg-map             - save IR with information about assigned basic-locks\n"
		"  --save-rules               - save IR with information selectd code-generation \n"
		"  --save-regs                - save IR with information about assigned CPU register\n"
		"  --save-use-lists           - save IR with def->use chains\n"
		"  --save-ir-after-load       - save IR generated by C front-end\n"
		"  --save-ir-after-use-lists  - save IR after USE-LISTS construction\n"
		"  --save-ir-after-mem2ssa    - save IR after SSA construction pass\n"
		"  --save-ir-after-sccp       - save IR after SCCP optimization pass\n"
		"  --save-ir-after-cfg        - save IR after CFG construction\n"
		"  --save-ir-after-dom        - save IR after Dominators tree construction\n"
		"  --save-ir-after-loop       - save IR after Loop detection\n"
		"  --save-ir-after-gcm        - save IR after GCM optimization pass\n"
		"  --save-ir-after-scheduling - save IR after scheduling\n"
		"  --save-ir-after-matching   - save IR after code selection\n"
		"  --save-ir-after-live-ranges - save IR after live ranges identification\n"
		"  --save-ir-after-coalescing - save IR after live ranges coalescing\n"
		"  --save-ir-after-regalloc   - save IR after register allocation\n"
		"  --save-ir-codegen          - save IR with selcted code rules and registers\n"
		"  --save-ir-final            - save IR after all passes\n"
		"  --save-ir-after-each-pass  - save IR after each pass\n"
		"  --save-live-ranges         - save info about live ranges (use with --save-ir-after-live-ranges)\n"
		"  --save-dot                 - save IR in .DOT format (affects all --save-ir-...)\n"
		"                               the output may be converted into multi-page PDF using pipe: \n"
		"                                 $ rcc ... 2>&1 | dot -Tps:cairo:cairo | ps2pdf - > out.pdf\n"
#ifdef IR_DEBUG
		"  --debug-sccp               - debug SCCP optimization pass\n"
		"  --debug-gcm                - debug GCM optimization pass\n"
		"  --debug-gcm-split          - debug floating node splitting\n"
		"  --debug-scheduling         - debug SCHEDULE optimization pass\n"
		"  --debug-regalloc           - debug register allocator\n"
		"  --debug-regset <bit-mask>  - restrict available register set\n"
		"  --debug-bb-scheduling      - debug BB PLCEMENT optimization pass\n"
#endif
		"Utility Options\n"
		"  --emit-llvm                - convert final IR to LLVM code (implementation is incomplete)\n"
		"  --dump-size                - print size of generated code\n"
		"  --dump-time                - print compilation and execution time\n"
		"  --target                   - print JIT target\n"
		"  --version\n"
		"  --help\n",
		cmd);
}

static void rcc_process_defines(ir_list *def, const char **argv)
{
	char buf[256];

	pp_start();
	for (uint32_t j = 0; j < ir_list_len(def); j++) {
		int i = ir_list_at(def, j);
		const char *str = argv[i];
		const char *val;
		yy_sym sym;

		IR_ASSERT(str[0] == '-');
		if (str[1] == 'D') {
			str = (str[2] == 0) ? argv[i + 1] : str + 2;
			val = strchr(str, '=');
			if (val) {
			    snprintf(buf, sizeof(buf), "#define %.*s %s\n", (int)(val-str), str, val + 1);
			} else {
			    snprintf(buf, sizeof(buf), "#define %s\n", str);
			}
		} else {
			IR_ASSERT(str[1] == 'U');
			str = (str[2] == 0) ? argv[i + 1] : str + 2;
		    snprintf(buf, sizeof(buf), "#undef %s\n", str);
		}

		yy_pos = yy_text = yy_linepos = yy_buf = buf;
		yy_end = yy_buf + strlen(yy_buf);

		do {
			sym = yy_next();
		} while (sym != YY_EOF);
	}
	pp_dtor();
}

int main(int argc, const char **argv)
{
	bool preprocess_only = 0;
	uint32_t preprocess_flags = 0, compiler_flags = 0;
	int run_args = 0;
	const char *output = NULL;
	ir_list src, def;
	int i;
	ir_ctx ctx;
	double start_time = 0.0;
	int ret = 1;
#if defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
	uint32_t mflags_disabled = 0;
#endif

	ir_consistency_check();

	if (argc < 2) {
		fprintf(stderr, "ERROR: no input file(s)\n");
		return 1;
	}

	ir_list_init(&src, 16);
	ir_list_init(&def, 16);
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0
		 || strcmp(argv[i], "--help") == 0) {
			rcc_help(argv[0]);
			ret = 0;
			goto exit;
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("IR %s\n", IR_VERSION);
			ret = 0;
			goto exit;
		} else if (strcmp(argv[i], "--target") == 0) {
			printf("%s\n", IR_TARGET);
			ret = 0;
			goto exit;
		} else if (argv[i][0] == '-' && argv[i][1] == 'O' && strlen(argv[i]) == 3) {
			if (argv[i][2] == '0') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 0;
				c_opt_flags &= ~C_OPT_INLINE;
			} else if (argv[i][2] == '1') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 1;
			} else if (argv[i][2] == '2') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 2;
			} else {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				goto exit;
			}
		} else if (argv[i][0] == '-' && (argv[i][1] == 'D' || argv[i][1] == 'U')) {
			if (argv[i][2] == 0) {
				if (i + 1 == argc || argv[i+1][0] == '-') {
					fprintf(stderr, "ERROR: macro name missing after \"-%c\"\n", argv[i][1]);
					goto exit;
				}
				ir_list_push(&def, i);
				i++;
			} else {
				ir_list_push(&def, i);
			}
		} else if (argv[i][0] == '-' && argv[i][1] == 'I') {
			const char *path;

			if (argv[i][2] == 0) {
				if (i + 1 == argc || argv[i+1][0] == '-') {
					fprintf(stderr, "ERROR: include directory missing after \"-%c\"\n", argv[i][1]);
					goto exit;
				}
				path = argv[i+1];
				i++;
			} else {
				path = argv[i] + 2;
			}
			if (!pp_add_include_dir(path)) {
				fprintf(stderr, "ERROR: too many -I options");
				goto exit;
			}
		} else if (argv[i][0] == '-' && argv[i][1] == 'o') {
			if (argv[i][2] == 0) {
				if (i + 1 == argc || (argv[i+1][0] == '-' && argv[i+1][1] != 0)) {
					fprintf(stderr, "ERROR: missing filename after \"-%c\"\n", argv[i][1]);
					goto exit;
				}
				output = argv[i+1];
				i++;
			} else {
				output = argv[i] + 2;
			}
		} else if (strcmp(argv[i], "-fno-inline") == 0) {
			c_opt_flags &= ~C_OPT_INLINE;
		} else if (strcmp(argv[i], "-finline") == 0) {
			c_opt_flags |= C_OPT_INLINE;
		} else if (strcmp(argv[i], "-fno-mem2ssa") == 0) {
			c_opt_flags &= ~C_OPT_MEM2SSA;
		} else if (strcmp(argv[i], "-E") == 0) {
			preprocess_only = 1;
		} else if (strcmp(argv[i], "-P") == 0) {
			preprocess_flags |= PP_NO_LINEMARKERS;
		} else if (strcmp(argv[i], "-dM") == 0) {
			preprocess_flags |= PP_NO_OUTPUT |PP_DUMP_MACROS;
		} else if (strcmp(argv[i], "-dD") == 0) {
			preprocess_flags |= PP_DUMP_MACROS;
		} else if (strcmp(argv[i], "-dN") == 0) {
			preprocess_flags |= PP_DUMP_MACROS | PP_DUMP_MACRO_NAMES;
		} else if (strcmp(argv[i], "-dI") == 0) {
			preprocess_flags |= PP_DUMP_INCLUDES;
		} else if (strcmp(argv[i], "-w") == 0) {
			compiler_flags |= YY_NO_WARNINGS;
#if defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
		} else if (strcmp(argv[i], "-mavx") == 0) {
			ir_mflags |= IR_X86_AVX;
		} else if (strcmp(argv[i], "-mbmi1") == 0) {
			ir_mflags |= IR_X86_BMI1;
		} else if (strcmp(argv[i], "-mno-bmi1") == 0) {
			mflags_disabled |= IR_X86_BMI1;
#endif
		} else if (strcmp(argv[i], "-muse-fp") == 0) {
			ir_flags |= IR_USE_FRAME_POINTER;
#if defined(IR_TARGET_X86)
		} else if (strcmp(argv[i], "-mfastcall") == 0) {
			ir_flags |= IR_CC_FASTCALL;
#endif
		} else if (strcmp(argv[i], "--save-cfg") == 0) {
			c_save_flags |= IR_SAVE_CFG;
		} else if (strcmp(argv[i], "--save-cfg-map") == 0) {
			c_save_flags |= IR_SAVE_CFG | IR_SAVE_CFG_MAP;
		} else if (strcmp(argv[i], "--save-rules") == 0) {
			c_save_flags |= IR_SAVE_RULES;
		} else if (strcmp(argv[i], "--save-regs") == 0) {
			c_save_flags |= IR_SAVE_REGS;
		} else if (strcmp(argv[i], "--save-use-lists") == 0) {
			c_save_flags |= IR_SAVE_USE_LISTS;
		} else if (strcmp(argv[i], "--save-ir-after-load") == 0) {
			c_flags |= C_DUMP_IR_AFTER_LOAD;
		} else if (strcmp(argv[i], "--save-ir-after-use-lists") == 0) {
			c_flags |= C_DUMP_IR_AFTER_USE_LISTS;
		} else if (strcmp(argv[i], "--save-ir-after-mem2ssa") == 0) {
			c_flags |= C_DUMP_IR_AFTER_MEM2SSA;
		} else if (strcmp(argv[i], "--save-ir-after-sccp") == 0) {
			c_flags |= C_DUMP_IR_AFTER_SCCP;
		} else if (strcmp(argv[i], "--save-ir-after-cfg") == 0) {
			c_flags |= C_DUMP_IR_AFTER_CFG;
		} else if (strcmp(argv[i], "--save-ir-after-dom") == 0) {
			c_flags |= C_DUMP_IR_AFTER_DOM;
		} else if (strcmp(argv[i], "--save-ir-after-loop") == 0) {
			c_flags |= C_DUMP_IR_AFTER_LOOP;
		} else if (strcmp(argv[i], "--save-ir-after-gcm") == 0) {
			c_flags |= C_DUMP_IR_AFTER_GCM;
		} else if (strcmp(argv[i], "--save-ir-after-scheduling") == 0) {
			c_flags |= C_DUMP_IR_AFTER_SCHEDULING;
		} else if (strcmp(argv[i], "--save-ir-after-matching") == 0) {
			c_flags |= C_DUMP_IR_AFTER_CODE_MATCHING;
		} else if (strcmp(argv[i], "--save-ir-after-live-ranges") == 0) {
			c_flags |= C_DUMP_IR_AFTER_LIVE_RANGES;
		} else if (strcmp(argv[i], "--save-ir-after-coalescing") == 0) {
			c_flags |= C_DUMP_IR_AFTER_COALESCING;
		} else if (strcmp(argv[i], "--save-ir-after-regalloc") == 0) {
			c_flags |= C_DUMP_IR_AFTER_REGALLOC;
		} else if (strcmp(argv[i], "--save-ir-codegen") == 0) {
			c_flags |= C_DUMP_IR_CODEGEN;
		} else if (strcmp(argv[i], "--save-ir-final") == 0) {
			c_flags |= C_DUMP_IR_FINAL;
		} else if (strcmp(argv[i], "--save-ir-after-each-pass") == 0) {
			c_flags |= C_DUMP_IR_AFTER_ALL;
		} else if (strcmp(argv[i], "--save-live-ranges") == 0) {
			c_flags |= C_DUMP_LIVE_RANGES;
		} else if (strcmp(argv[i], "--save-dot") == 0) {
			c_flags |= C_DUMP_DOT;
#ifdef IR_DEBUG
		} else if (strcmp(argv[i], "--debug-sccp") == 0) {
			ir_flags |= IR_DEBUG_SCCP;
		} else if (strcmp(argv[i], "--debug-gcm") == 0) {
			ir_flags |= IR_DEBUG_GCM;
		} else if (strcmp(argv[i], "--debug-gcm-split") == 0) {
			ir_flags |= IR_DEBUG_GCM_SPLIT;
		} else if (strcmp(argv[i], "--debug-scheduling") == 0) {
			ir_flags |= IR_DEBUG_SCHEDULE;
		} else if (strcmp(argv[i], "--debug-regalloc") == 0) {
			ir_flags |= IR_DEBUG_RA;
		} else if (strcmp(argv[i], "--debug-bb-scheduling") == 0) {
			ir_flags |= IR_DEBUG_BB_SCHEDULE;
#endif
		} else if (strcmp(argv[i], "--debug-regset") == 0) {
			if (i + 1 == argc || argv[i + 1][0] == '-') {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
			ir_debug_regset = strtoull(argv[i + 1], NULL, 0);
			i++;
		} else if (strcmp(argv[i], "--emit-ir") == 0) {
			c_flags |= C_DUMP_IR;
			c_save_flags |= IR_SAVE_CFG;
		} else if (strcmp(argv[i], "--save") == 0) {
			c_flags |= C_DUMP_IR;
		} else if (strcmp(argv[i], "--emit-llvm") == 0) {
			c_flags |= C_DUMP_LLVM;
		} else if (strcmp(argv[i], "-S") == 0) {
			c_flags |= C_DUMP_ASM;
		} else if (strcmp(argv[i], "--dump-size") == 0) {
			c_flags |= C_DUMP_SIZE;
		} else if (strcmp(argv[i], "--dump-time") == 0) {
			c_flags |= C_DUMP_TIME;
		} else if (strcmp(argv[i], "-g") == 0) {
			c_flags |= C_GDB;
		} else if (strcmp(argv[i], "-p") == 0) {
			c_flags |= C_PERF;
		} else if (strcmp(argv[i], "-fsyntax-only") == 0) {
			c_flags |= C_SYNTAX_ONLY;
		} else if (strcmp(argv[i], "--run") == 0) {
			c_flags |= C_RUN;
			if (i + 1 < argc) {
				run_args = i + 1;
				break;
			}
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "ERROR: Unknown option '%s' (use --help)\n", argv[i]);
			goto exit;
		} else {
			ir_list_push(&src, i);
		}
	}

	pp_add_sys_include_dirs();

	if (c_flags & C_SYNTAX_ONLY) {
		if (c_flags & (C_DUMP_SIZE|C_DUMP_ASM|C_RUN)) {
			fprintf(stderr, "ERROR: -fsyntax-only is not compatible with native compilation flags (-S, --run, --dump-size)\n");
			goto exit;
		}
		c_opt_flags = 0;
	}

	if (ir_list_len(&src) == 0) {
		fprintf(stderr, "ERROR: no input file(s)\n");
		goto exit;
	}

	if (output && output[0] != '-' && output[1] != 0) {
		c_out = fopen(output, "w");
		if (!c_out) {
			fprintf(stderr, "ERROR: cannot open output file \"%s\"\n", output);
			goto exit;
		}
	} else {
		c_out = stdout;
	}

#ifndef _WIN32
	if ((c_flags & C_PERF) && (c_flags & C_RUN) && !preprocess_only) {
		ir_perf_jitdump_open();
	}
#endif

	if (c_flags & C_DUMP_TIME) {
		start_time = rcc_time();
	}

	ir_init(&ctx, IR_OPT_FOLDING, 64, 64);
	global_ctx = active_ctx = &ctx;
	ir_START();

	if (preprocess_only) {
		yy_flags = YY_FLAGS_PP_DEFAULT | preprocess_flags;
		rcc_init();
		if (ir_list_len(&def)) rcc_process_defines(&def, argv);

		uint32_t n = ir_list_len(&src);
		if (n > 1) rcc_remember_state();
		for (uint32_t j = 0; j < n; j++) {
			const char *input = argv[ir_list_at(&src, j)];

			if (j) rcc_reset_state();
			if (!rcc_preprocess(input, stdout)) {
				rcc_free();
				ir_free(&ctx);
				goto exit;
			}
		}

		rcc_free();

		if (c_flags & C_DUMP_TIME) {
			double t = rcc_time();
			fprintf(stderr, "\npreprocessing time = %0.6f\n", t - start_time);
		}

		ret = 0;
	} else {
		c_native = (c_flags & (C_DUMP_SIZE|C_DUMP_ASM|C_RUN)) != 0;

		if (c_native) {
			size_t size = 4 * 1024 * 1024;
			c_code_buffer.start = ir_mem_mmap(size);
			if (!c_code_buffer.start) {
				fprintf(stderr, "ERROR: Cannot allocate JIT code buffer\n");
				goto exit;
			}
			c_code_buffer.pos = c_code_buffer.start;
			c_code_buffer.end = (char*)c_code_buffer.start + size;

#if defined(IR_TARGET_X86) || defined(IR_TARGET_X64)
			if (c_flags & C_RUN) {
				uint32_t cpuinfo = ir_cpuinfo();

				if (!(cpuinfo & IR_X86_SSE2)) {
					fprintf(stderr, "ERROR: incompatible CPU (SSE2 is not supported)\n");
					return 1;
				}

				if ((ir_mflags & IR_X86_AVX) && !(cpuinfo & IR_X86_AVX)) {
					fprintf(stderr, "ERROR: -mavx is not compatible with CPU (AVX is not supported)\n");
					return 1;
				}
				if ((cpuinfo & IR_X86_BMI1) && !(mflags_disabled & IR_X86_BMI1)) {
					ir_mflags |= IR_X86_BMI1;
				}
			} else {
				if (!(mflags_disabled & IR_X86_BMI1)) {
					ir_mflags |= IR_X86_BMI1;
				}
			}
#endif
		}

		yy_flags = YY_FLAGS_DEFAULT | compiler_flags;
		rcc_init();
		if (ir_list_len(&def)) rcc_process_defines(&def, argv);

		uint32_t n = ir_list_len(&src);
		if (n > 1) {
			rcc_remember_state();
		} else {
			c_flags |= C_SINGLE_FILE;
		}
		for (uint32_t j = 0; j < n; j++) {
			const char *input = argv[ir_list_at(&src, j)];

			if (j) rcc_reset_state();
			if (!rcc_compile(input)) {
				rcc_free();
				ir_free(&ctx);
				goto exit;
			}
			if ((c_flags & C_DO_LINK_INTERNAL)
			 || (n == 1 && (c_flags & C_DO_LINK_EXTERNAL))) {
				rcc_link_internal();
				c_flags &= ~(C_DO_LINK_INTERNAL|C_DO_LINK_EXTERNAL);
			}
		}

		if (c_flags & (C_DUMP_SIZE - 1)) {
			fflush(stdout);
		}

		if (c_flags & C_DUMP_SIZE) {
			fprintf(stderr, "\ncode size = %lld\n",
				(long long int)((char*)c_code_buffer.pos - (char*)c_code_buffer.start));
		}

		if (c_flags & C_DUMP_TIME) {
			double t = rcc_time();
			fprintf(stderr, "\ncompilation time = %0.6f\n", t - start_time);
			start_time = t;
		}

		if (c_flags & C_RUN) {
			int jit_argc = 1;
			const char **jit_argv;
			int (*func)(int, const char**) = NULL;

			if (ir_list_len(&src) > 1) {
				c_linker_sym *link;

				rcc_reset_state();
				rcc_link();
				link = yy_hash.data[YY_MAIN].link;

				if (!link || !link->addr) {
					rcc_free();
					ir_free(&ctx);
					fprintf(stderr, "undefined reference to function \"main\"\n");
					goto exit;
				}
				func = link->addr;
			} else {
				c_sym *sym = yy_hash.data[YY_MAIN].sym;

				if (!sym || sym->kind != C_SYM_FUNC || !c_value_is_const(&sym->value)) {
					rcc_free();
					ir_free(&ctx);
					fprintf(stderr, "undefined reference to function \"main\"\n");
					goto exit;
				}
				IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.ptr);
				func = sym->value.u.val.ptr;
			}

			ir_free(&ctx);
			ir_list_free(&def);
			ir_list_free(&src);

			if (run_args && argc > run_args) {
				jit_argc = argc - run_args + 1;
			}
			jit_argv = alloca(sizeof(char*) * jit_argc);
			jit_argv[0] = "jit code";
			for (i = 1; i < jit_argc; i++) {
				jit_argv[i] = argv[run_args + i - 1];
			}

			if (c_flags & C_DUMP_TIME) {
				rcc_atexit_start = start_time;
				atexit(rcc_atexit);
			}

			ret = func(jit_argc, jit_argv);

			if ((c_flags & C_DUMP_TIME) && rcc_atexit_start) {
				double t = rcc_time();
				fflush(stdout);
				fprintf(stderr, "\nexecution time = %0.6f\n", t - rcc_atexit_start);
				rcc_atexit_start = 0.0;
			}

#ifndef _WIN32
			if (c_flags & C_PERF) {
				ir_perf_jitdump_close();
			}
#endif

			rcc_free();
			return ret;
		} else {
			ret = 0;
		}

		rcc_free();
	}

	ir_free(&ctx);

exit:
	ir_list_free(&def);
	ir_list_free(&src);
	return ret;
}
