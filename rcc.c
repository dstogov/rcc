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

#undef _ir_CTX
#define _ir_CTX active_ctx

/* IR compiler */
#define C_DUMP_IR_AFTER_LOAD     (1<<0)
#define C_DUMP_IR_AFTER_MEM2SSA  (1<<1)
#define C_DUMP_IR_AFTER_SCCP     (1<<2)
#define C_DUMP_IR_AFTER_SCHEDULE (1<<3)
#define C_DUMP_IR                (1<<4)
#define C_DUMP_ASM               (1<<5)
#define C_DUMP_SIZE              (1<<6)
#define C_DUMP_TIME              (1<<7)
#define C_GDB                    (1<<8)

#define C_OPT_LEVEL              0x3
#define C_OPT_INLINE             (1<<2)
#define C_OPT_MEM2SSA            (1<<3)

static bool            c_native = 0;
static uint32_t        c_opt_flags = 2 | C_OPT_INLINE | C_OPT_MEM2SSA;
static uint32_t        c_dump_flags = 0;
static uint32_t        c_save_flags = 0;
static ir_arena       *c_linker_arena;
static ir_code_buffer  c_code_buffer;
static bool            protected = 1;

void* c_linker_resolve_sym_name(ir_loader *loader, const char *name, uint32_t flags)
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
		if (sym->linkage == C_LINK_EXTERNAL) {
			void *addr = ir_resolve_sym_name(name);
			if (addr) {
				sym->value.u.opt = IR_OPT(C_VAL_CONST, IR_ADDR);
				sym->value.u.val.ptr = addr;
				return addr;
			}
		}
		if (flags & IR_RESOLVE_SYM_ADD_THUNK) {
			/* Undefined declaration */
			// TODO: Add thunk or relocation ???
			size_t size;
			void *addr;
			
			if (protected) {
				ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
			}
			addr = ir_emit_thunk(&c_code_buffer, NULL, &size);
			if (protected) {
				ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
			}
			if (addr) {
				sym->value.u.opt = IR_OPT(C_VAL_CONST, IR_ADDR);
				sym->value.u.val.ptr = addr;
				ir_disasm_add_symbol(name, (uint64_t)(uintptr_t)addr, size); //???
				return addr;
			}
		}
	}
	if (!(flags & IR_RESOLVE_SYM_SILENT)) {
		yy_error_fmt("undefined symbol \"%s\"", name);
	}
	return NULL;
}

void *c_linker_allocate_data(size_t size)
{
	void *data = ir_arena_alloc(&c_linker_arena, size);
	memset(data, 0, size);
	return data;
}

void *c_linker_grow_data(void *addr, size_t size)
{
	size_t old_size = (char*)c_linker_arena->ptr - (char*)addr;

	IR_ASSERT(size > old_size);
	if (size - old_size >= (size_t)(c_linker_arena->end - c_linker_arena->ptr)) {
		void *new_addr = ir_arena_alloc(&c_linker_arena, size);
		memcpy(new_addr, addr, old_size);
		memset((char*)new_addr + old_size, 0, size - old_size);
		return new_addr;
	}
	memset((char*)addr + old_size, 0, size - old_size);
	c_linker_arena->ptr += size - old_size;
	return addr;
}

ir_loader c_linker = {
	.resolve_sym_name = c_linker_resolve_sym_name,
};

static void rcc_dump_func_proto(c_name name, FILE *f)
{
	c_sym *sym = yy_hash.data[name].sym;
	const c_type *t;

	IR_ASSERT(sym && sym->kind == C_SYM_FUNC);
	if (sym->linkage == C_LINK_INTERNAL) {
		fprintf(f, "static ");
	} else if (sym->linkage == C_LINK_EXTERNAL && 0/*???*/) {
		fprintf(f, "extern ");
	}
	fprintf(f, "func %s(", yy_sym2str(name));

	t = sym->value.type;
	if (t->func.num_params > 0) {
		int n = t->func.num_params;
		const c_param *p = t->func.params;

		fprintf(f, "%s", ir_type_cname[c_type2ir(p->type)]);
		p++;
		while (--n) {
			fprintf(f, ", %s", ir_type_cname[c_type2ir(p->type)]);
			p++;
		}
		if (t->attr & C_ATTR_VARIADIC) {
			fprintf(f, ", ...");
		}
	} else if (t->attr & C_ATTR_VARIADIC) {
		fprintf(f, "...");
	}
	fprintf(f, "): %s", ir_type_cname[c_type2ir(t->func.ret_type)]);

//???
//	if (flags & IR_FASTCALL_FUNC) {
//		fprintf(f, " __fastcall");
//	} else if (flags & IR_BUILTIN_FUNC) {
//		fprintf(f, " __builtin");
//	}
	fprintf(f, ";\n");
}

void rcc_ir_init(ir_ctx *ctx, uint32_t flags)
{
	flags |= IR_FUNCTION;
	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		flags |= IR_OPT_FOLDING | IR_OPT_CFG | IR_OPT_CODEGEN;
	}
	ir_init(ctx, flags, 256, 1024);
	ctx->loader = &c_linker;
}

void rcc_ir_codegen(c_name name, ir_ctx *ctx, c_value *func)
{
	size_t size;
	void *entry;

	ctx->code_buffer = &c_code_buffer;
	protected = 0;
	ir_mem_unprotect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	entry = ir_emit_code(ctx, &size);
	IR_ASSERT(entry);
	if (c_value_is_const(func)) {
		ir_fix_thunk(func->u.val.ptr, entry);
	}
#ifndef _WIN32
	if (c_dump_flags & C_GDB) {
		ir_gdb_register(yy_sym2str(name), entry, size, sizeof(void*), 0);
	}
#endif
	ir_mem_protect(c_code_buffer.start, (char*)c_code_buffer.end - (char*)c_code_buffer.start);
	protected = 1;

	if (c_dump_flags & C_DUMP_ASM) {
//		ir_ref i;
//		ir_insn *insn;
//
		ir_disasm_add_symbol(yy_sym2str(name), (uintptr_t)entry, size);
//
//		for (i = IR_UNUSED + 1, insn = ctx->ir_base - i; i < ctx->consts_count; i++, insn--) {
//			if (insn->op == IR_FUNC) {
//				const char *name = ir_get_str(ctx, insn->val.name);
//				void *addr = ir_loader_resolve_sym_name(loader, name, 0);
//
//				IR_ASSERT(addr);
//				ir_disasm_add_symbol(name, (uintptr_t)addr, IR_UNKNOWN_SIZE);
//TODO:			} else if (insn->op == IR_SYM) {
//			}
//		}
		ir_disasm(yy_sym2str(name), entry, size, 0, ctx, stderr);
	}

	func->u.opt = IR_OPT(C_VAL_CONST, IR_ADDR);
	func->u.val.ptr = entry;
}

void rcc_ir_compile(c_name name, ir_ctx *ctx, c_value *func)
{
	if (c_dump_flags & C_DUMP_IR_AFTER_LOAD) {
		rcc_dump_func_proto(name, stderr);
		ir_save(ctx, c_save_flags, stderr);
	}

	ir_build_def_use_lists(ctx);

#ifdef IR_DEBUG
	ir_check(ctx);
#endif

	if ((c_opt_flags & C_OPT_LEVEL) > 0 && (c_opt_flags & C_OPT_MEM2SSA)) {
		ir_build_cfg(ctx);
		ir_build_dominators_tree(ctx);
		ir_mem2ssa(ctx);
		if (c_dump_flags & C_DUMP_IR_AFTER_MEM2SSA) {
			rcc_dump_func_proto(name, stderr);
			ir_save(ctx, c_save_flags, stderr);
		}
		ir_reset_cfg(ctx);
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 1) {
		ir_sccp(ctx);
		if (c_dump_flags & C_DUMP_IR_AFTER_SCCP) {
			rcc_dump_func_proto(name, stderr);
			ir_save(ctx, c_save_flags, stderr);
		}
	}

	ir_build_cfg(ctx);

	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		ir_build_dominators_tree(ctx);
		ir_find_loops(ctx);
		ir_gcm(ctx);
		ir_schedule(ctx);
		if (c_dump_flags & C_DUMP_IR_AFTER_SCHEDULE) {
			rcc_dump_func_proto(name, stderr);
			ir_save(ctx, c_save_flags | IR_SAVE_CFG, stderr);
		}
	}

	if (c_native) {
		ir_match(ctx);
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 0 || c_native || 0) {
		ir_assign_virtual_registers(ctx);
	}

	if ((c_opt_flags & C_OPT_LEVEL) > 0) {
		ir_compute_live_ranges(ctx);
		ir_coalesce(ctx);
		if (c_native) {
			ir_reg_alloc(ctx);
		}
		ir_schedule_blocks(ctx);
	} else if (c_native || 0) {
		ir_compute_dessa_moves(ctx);
	}

	if (c_dump_flags & C_DUMP_IR) {
		rcc_dump_func_proto(name, stderr);
		ir_save(ctx, c_save_flags | IR_SAVE_CFG | IR_SAVE_RULES, stderr);
	}

#ifdef IR_DEBUG
	ir_check(ctx);
#endif

	if (c_native) {
		rcc_ir_codegen(name, ctx, func);
	}

	ir_free(ctx);
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

	pp_macro_define(YY___COUNTER__,  PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___DATE__,     PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FILE__,     PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FUNCTION__, PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___FUNC__,     PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___LINE__,     PP_MACRO_BUILTIN, 0, NULL);
	pp_macro_define(YY___TIME__,     PP_MACRO_BUILTIN, 0, NULL);

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

	c_declare_builtin(YY___BUILTIN_VA_START, NULL);
	c_declare_builtin(YY___BUILTIN_VA_ARG, NULL);
	c_declare_builtin(YY___BUILTIN_VA_END, NULL);
	c_declare_builtin(YY___BUILTIN_VA_COPY, NULL);

	c_type *type;
	c_type *c_type_ptr_void = ir_arena_alloc(&c_arena, sizeof(c_type));
	memset(c_type_ptr_void, 0, sizeof(c_type));
	c_type_ptr_void->kind = C_TYPE_POINTER;
	//c_type_ptr_void->attr = C_ATTR_RESTRICT;
	c_type_ptr_void->pointer.type = &c_type_void;

	c_type *c_type_ptr_const_void = ir_arena_alloc(&c_arena, sizeof(c_type));
	memset(c_type_ptr_const_void, 0, sizeof(c_type));
	c_type_ptr_const_void->kind = C_TYPE_POINTER;
	//c_type_ptr_const_void->attr = C_ATTR_RESTRICT;
	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	memset(type, 0, sizeof(c_type));
	type->kind = C_TYPE_VOID;
	type->attr = C_ATTR_CONST;
	c_type_ptr_const_void->pointer.type = type;

	c_dcl dcl;
	memset(&dcl, 0, sizeof(dcl));
	dcl.flags = C_DCL_EXTERN | C_TYPE_SPEC_TYPE;

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	memset(type, 0, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->func.ret_type = c_type_ptr_void;
	type->func.num_params = 3;
	type->func.params = ir_arena_alloc(&c_arena, sizeof(c_param) * 3);
	type->func.params[0].name = 0;
	type->func.params[0].type = c_type_ptr_void;
	type->func.params[1].name = 0;
	type->func.params[1].type = c_type_ptr_const_void;
	type->func.params[2].name = 0;
	type->func.params[2].type = &c_type_size_t;
	dcl.type = type;

	c_declare(YY_MEMCPY, &dcl);

	type = ir_arena_alloc(&c_arena, sizeof(c_type));
	memset(type, 0, sizeof(c_type));
	type->kind = C_TYPE_FUNC;
	type->func.ret_type = c_type_ptr_void;
	type->func.num_params = 3;
	type->func.params = ir_arena_alloc(&c_arena, sizeof(c_param) * 3);
	type->func.params[0].name = 0;
	type->func.params[0].type = c_type_ptr_void;
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

	buf = ir_mem_malloc(size + 1);
	if (!buf) {
		fprintf(stderr, "ERROR: Cannot allocate buffer to read file \"%s\"\n", file_name);
		return 0;
	}

	ret = read(fd, buf, size);
	close(fd);

	if (ret != size) {
		fprintf(stderr, "ERROR: Cannot read file \"%s\"\n", file_name);
		return 0;
	}

	buf[size] = '\0'; /* End marker */

	yy_pos = yy_text = yy_linepos = yy_buf = buf;
	yy_len = 0;
	yy_line = 1;
	yy_end = yy_buf + size;
	yy_file_name = yy_hash_lookup(file_name, strlen(file_name));

	pp_start();

	return 1;
}

static void rcc_dtor(void)
{
	pp_dtor();
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
	if (!rcc_read(file_name)) {
		return 0;
	}
	rcc_parse();
	rcc_dtor();
	return 1;
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
		fprintf(stderr, "\nexecution time = %0.6f\n", t - rcc_atexit_start);
		rcc_atexit_start = 0.0;
	}
}

static void rcc_help(const char *cmd)
{
	printf(
		"Usage: %s [options] input-file [--run ...]\n"
		"General Options:\n"
		"  --run ...                  - run the main() function of generated code\n"
		"                               (the remaining arguments are passed to main)\n"
		"  -g                         - produce debugging information (through JITGDB)\n"
		"  -S                         - show generated assembler code\n"
		"Optimization Options:\n"
		"  -O[012]                    - optimization level (default: 2)\n"
		"  -fno-inline                - disable function inlining\n"
		"  -fno-mem2ssa               - disable MEM2SSA pass\n"
		"Preprocessor Options:\n"
		"  -E                         - preprocess only\n"
		"  -P                         - inhibit generation of linemarkers\n"
		"  -dM                        - generate list of #define directives\n"
		"  -dN                        - generate list of #define directives (names only)\n"
		"  -dD                        - preprocess and generate list of #define directives\n"
		"  -dI                        - preprocess and generate list of #include directives\n"
		"Error Reporting Options:\n"
		"  -w                         - inhibit all warning messages\n"
		"IR Options:\n"
		"  --save-ir-after-load       - print IR generated by C front-end\n"
		"  --save-ir-after-mem2ssa    - print IR after SSA construction pass\n"
		"  --save-ir-after-sccp       - print IR after SCCP optimization pass\n"
		"  --save-ir-after-schedule   - print IR after scheduling\n"
		"  --emit-ir                  - print final IR\n"
		"Utility Options\n"
		"  --dump-size                - print size of generated code\n"
		"  --dump-time                - print compilation and execution time\n"
		"  --target                   - print JIT target\n"
		"  --version\n"
		"  --help\n",
		cmd);
}

int main(int argc, char **argv)
{
	bool preprocess_only = 0;
	uint32_t preprocess_flags = 0, compiler_flags = 0;
	bool run = 0;
	int run_args = 0;
	const char *input = NULL;
	int i;
	ir_ctx ctx;
	double start_time = 0.0;
	int ret = 0;

	ir_consistency_check();

	if (argc < 2) {
		fprintf(stderr, "ERROR: no input file\n");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0
		 || strcmp(argv[i], "--help") == 0) {
			rcc_help(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("IR %s\n", IR_VERSION);
			return 0;
		} else if (strcmp(argv[i], "--target") == 0) {
			printf("%s\n", IR_TARGET);
			return 0;
		} else if (argv[i][0] == '-' && argv[i][1] == 'O' && strlen(argv[i]) == 3) {
			if (argv[i][2] == '0') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 0;
			} else if (argv[i][2] == '1') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 1;
			} else if (argv[i][2] == '2') {
				c_opt_flags = (c_opt_flags & ~C_OPT_LEVEL) | 2;
			} else {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
		} else if (strcmp(argv[i], "-fno-inline") == 0) {
			c_opt_flags &= ~C_OPT_INLINE;
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
//TODO: -D ???
//TODO: -U ???
//TODO: -I ???
//TODO: -include file
//TODO: -isystem dir
//TODO: -o ???
		} else if (strcmp(argv[i], "--save-ir-after-load") == 0) {
			c_dump_flags |= C_DUMP_IR_AFTER_LOAD;
		} else if (strcmp(argv[i], "--save-ir-after-mem2ssa") == 0) {
			c_dump_flags |= C_DUMP_IR_AFTER_MEM2SSA;
		} else if (strcmp(argv[i], "--save-ir-after-sccp") == 0) {
			c_dump_flags |= C_DUMP_IR_AFTER_SCCP;
		} else if (strcmp(argv[i], "--save-ir-after-schedule") == 0) {
			c_dump_flags |= C_DUMP_IR_AFTER_SCHEDULE;
		} else if (strcmp(argv[i], "--emit-ir") == 0) {
			c_dump_flags |= C_DUMP_IR;
		} else if (strcmp(argv[i], "-S") == 0) {
			c_dump_flags |= C_DUMP_ASM;
		} else if (strcmp(argv[i], "--dump-size") == 0) {
			c_dump_flags |= C_DUMP_SIZE;
		} else if (strcmp(argv[i], "--dump-time") == 0) {
			c_dump_flags |= C_DUMP_TIME;
		} else if (strcmp(argv[i], "-g") == 0) {
			c_dump_flags |= C_GDB;
		} else if (strcmp(argv[i], "--run") == 0) {
			run = 1;
			if (i + 1 < argc) {
				run_args = i + 1;
				break;
			}
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "ERROR: Unknown option '%s' (use --help)\n", argv[i]);
			return 1;
		} else {
			if (input) {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
			input = argv[i];
		}
	}

	if (c_dump_flags & C_DUMP_TIME) {
		start_time = rcc_time();
	}

	ir_init(&ctx, IR_OPT_FOLDING, 64, 64);
	global_ctx = active_ctx = &ctx;
	ir_START();

	if (preprocess_only) {
		yy_flags = YY_FLAGS_PP_DEFAULT | preprocess_flags;
		rcc_init();
		rcc_preprocess(input, stdout);
		rcc_free();

		if (c_dump_flags & C_DUMP_TIME) {
			double t = rcc_time();
			fprintf(stderr, "\npreprocessing time = %0.6f\n", t - start_time);
		}
	} else {
		c_native = run || (c_dump_flags & (C_DUMP_SIZE|C_DUMP_ASM));

		if (c_native) {
			size_t size = 2 * 1024 * 1024;
			c_code_buffer.start = ir_mem_mmap(size);
			if (!c_code_buffer.start) {
				fprintf(stderr, "ERROR: Cannot allocate JIT code buffer\n");
				return 1;
			}
			c_code_buffer.pos = c_code_buffer.start;
			c_code_buffer.end = (char*)c_code_buffer.start + size;
		}

		yy_flags = YY_FLAGS_DEFAULT | compiler_flags;
		rcc_init();
		if (rcc_compile(input)) {
			if (c_dump_flags & C_DUMP_SIZE) {
				fprintf(stderr, "\ncode size = %lld\n",
					(long long int)((char*)c_code_buffer.pos - (char*)c_code_buffer.start));
			}

			if (c_dump_flags & C_DUMP_TIME) {
				double t = rcc_time();
				fprintf(stderr, "\ncompilation time = %0.6f\n", t - start_time);
				start_time = t;
			}

			if (run) {
				int jit_argc = 1;
				char **jit_argv;
				c_sym *sym = yy_hash.data[YY_MAIN].sym;
				int (*func)(int, char**) = NULL;

				if (!sym || sym->kind != C_SYM_FUNC || !c_value_is_const(&sym->value)) {
					rcc_free();
					ir_free(&ctx);
					fprintf(stderr, "undefined reference to function \"main\"\n");
					return 1;
				}
				IR_ASSERT(sym->value.u.type == IR_ADDR && sym->value.u.val.ptr);
				func = sym->value.u.val.ptr;

				if (run_args && argc > run_args) {
					jit_argc = argc - run_args + 1;
				}
				jit_argv = alloca(sizeof(char*) * jit_argc);
				jit_argv[0] = "jit code";
				for (i = 1; i < jit_argc; i++) {
					jit_argv[i] = argv[run_args + i - 1];
				}

				if (c_dump_flags & C_DUMP_TIME) {
					rcc_atexit_start = start_time;
					atexit(rcc_atexit);
				}

				ret = func(jit_argc, jit_argv);
			}
		} else {
			ret = 1;
		}
		rcc_free();
	}

	ir_free(&ctx);

	return ret;
}
