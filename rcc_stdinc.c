/*
 * RCC - Rational C Compiler
 * (Standard include files)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */

#include <ir.h>
#include <ir_private.h>

#include "rcc.h"

static const char c_boot[] =
"#define __RCC__                  1\n"
"#define __STDC__                 1\n"
"#define __STDC_HOSTED__          1\n"
"#define __STDC_VERSION__         201112L\n"
//"#define __STDC_ISO_10646__       201103L\n"
//"#define __STDC_MB_MIGHT_NEQ_WC__ 1\n"
//"#define __STDC_UTF_16__          1\n"
//"#define __STDC_UTF_32__          1\n"
//"#define __STDC_ANALYZABLE__      1\n"
//"#define __STDC_IEC_559__         1\n"
//"#define __STDC_IEC_559_COMPLEX__ 1\n"
//"#define __STDC_LIB_EXT1__        201ymmL\n"
"#define __STDC_NO_ATOMICS__      1\n"
"#define __STDC_NO_COMPLEX__      1\n"
"#define __STDC_NO_THREADS__      1\n"
"#define __STDC_NO_VLA__          1\n"
/* allow GCC extensions: __attribute__ */
//"#define __GNUC__                 3\n"
//"#define __GNUC_MINOR__           2\n"
//"#define __GNUC_PATCHLEVEL__      0\n"
#if defined(IR_TARGET_X64)
"#define __LP64__                 1\n"
"#define __x86_64__               1\n"
"#define __SIZEOF_SHORT__         2\n"
"#define __SIZEOF_INT__           4\n"
"#define __SIZEOF_LONG__          8\n"
"#define __SIZEOF_LONG_LONG__     8\n"
"#define __SIZEOF_POINTER__       8\n"
"#define __SIZEOF_PTRDIFF_T__     8\n"
"#define __SIZEOF_SIZE_T__        8\n"
"#define __SIZEOF_FLOAT__         4\n"
"#define __SIZEOF_DOUBLE__        8\n"
//"#define __SIZEOF_LONG_DOUBLE__   16\n"
"#define __CHAR_BIT__             8\n"
"#define __BYTE_ORDER__           1234\n"
"#define __ORDER_LITTLE_ENDIAN__  1234\n"
"#define __ORDER_BIG_ENDIAN__     4321\n"

"#define __SCHAR_MAX__            0x7f\n"
"#define __SHRT_MAX__             0x7fff\n"
"#define __INT_MAX__              0x7fffffff\n"
"#define __LONG_MAX__             0x7fffffffffffffffL\n"
"#define __LONG_LONG_MAX__        0x7fffffffffffffffLL\n"

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

"#define __SIZE_TYPE__            unsigned long\n"
"#define __PTRDIFF_TYPE__         long\n"
"#define __UINTPTR_TYPE__         unsigned long\n"
"#define __INTPTR_TYPE__          long\n"
#elif defined(IR_TARGET_X86)
"#define __ILP32__                1\n"
"#define __i386__                 1\n"
"#define __SIZEOF_SHORT__         2\n"
"#define __SIZEOF_INT__           4\n"
"#define __SIZEOF_LONG__          4\n"
"#define __SIZEOF_LONG_LONG__     8\n"
"#define __SIZEOF_POINTER__       4\n"
"#define __SIZEOF_PTRDIFF_T__     4\n"
"#define __SIZEOF_SIZE_T__        4\n"
"#define __SIZEOF_FLOAT__         4\n"
"#define __SIZEOF_DOUBLE__        8\n"
//"#define __SIZEOF_LONG_DOUBLE__   16\n"
"#define __CHAR_BIT__             8\n"
"#define __BYTE_ORDER__           1234\n"
"#define __ORDER_LITTLE_ENDIAN__  1234\n"
"#define __ORDER_BIG_ENDIAN__     4321\n"

"#define __SCHAR_MAX__            0x7f\n"
"#define __SHRT_MAX__             0x7fff\n"
"#define __INT_MAX__              0x7fffffff\n"
"#define __LONG_MAX__             0x7fffffffL\n"
"#define __LONG_LONG_MAX__        0x7fffffffffffffffLL\n"

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

"#define __SIZE_TYPE__            unsigned long\n"
"#define __PTRDIFF_TYPE__         long\n"
"#define __UINTPTR_TYPE__         unsigned long\n"
"#define __INTPTR_TYPE__          long\n"
#elif defined(IR_TARGET_AARCH64)
"#define __LP64__                 1\n"
"#define __aarch64__              1\n"
"#define __SIZEOF_SHORT__         2\n"
"#define __SIZEOF_INT__           4\n"
"#define __SIZEOF_LONG__          8\n"
"#define __SIZEOF_LONG_LONG__     8\n"
"#define __SIZEOF_POINTER__       8\n"
"#define __SIZEOF_PTRDIFF_T__     8\n"
"#define __SIZEOF_SIZE_T__        8\n"
"#define __SIZEOF_FLOAT__         4\n"
"#define __SIZEOF_DOUBLE__        8\n"
//"#define __SIZEOF_LONG_DOUBLE__   16\n"
"#define __CHAR_BIT__             8\n"
"#define __BYTE_ORDER__           1234\n"
"#define __ORDER_LITTLE_ENDIAN__  1234\n"
"#define __ORDER_BIG_ENDIAN__     4321\n"

"#define __SCHAR_MAX__            0x7f\n"
"#define __SHRT_MAX__             0x7fff\n"
"#define __INT_MAX__              0x7fffffff\n"
"#define __LONG_MAX__             0x7fffffffffffffffL\n"
"#define __LONG_LONG_MAX__        0x7fffffffffffffffLL\n"

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

"#define __SIZE_TYPE__            unsigned long\n"
"#define __PTRDIFF_TYPE__         long\n"
"#define __UINTPTR_TYPE__         unsigned long\n"
"#define __INTPTR_TYPE__          long\n"
#endif
"\n";

static const char c_stdarg_h[] =
"#ifndef __STDARG_H\n"
"#define __STDARG_H\n"
"\n"
"#if defined(__i386__) || defined(__WIN32) || defined(__APPLE__)\n"
"typedef char *va_list;\n"
"#elif defined(__x86_64__)\n"
"typedef struct {\n"
"  unsigned int gp_offset;\n"
"  unsigned int fp_offset;\n"
"  void *overflow_arg_area;\n"
"  void *reg_save_area;\n"
"} va_list[1];\n"
"#elif defined(__aarch64__)\n"
"typedef struct _ir_va_list {\n"
"  void    *stack;\n"
"  void    *gr_top;\n"
"  void    *vr_top;\n"
"  int      gr_offset;\n"
"  int      vr_offset;\n"
"} va_list;\n"
"#endif\n"
"\n"
"#define va_start(ap, param) __builtin_va_start (ap)\n"
"#define va_arg(ap, type) __builtin_va_arg(ap, (type *) 0)\n"
"#define va_end(ap) 0\n"
"#define va_copy(dest, src) ((dest)[0] = (src)[0])\n"
"\n"
"#ifndef __GNUC_VA_LIST\n"
"#define __GNUC_VA_LIST 1\n"
"#endif\n"
"typedef va_list __gnuc_va_list;\n"
"\n"
"#endif\n";

static const char c_stddef_h[] =
"#ifndef __STDDEF_H\n"
"#define __STDDEF_H\n"
"\n"
"typedef __PTRDIFF_TYPE__ ptrdiff_t;\n"
"typedef __SIZE_TYPE__ size_t;\n"
"typedef long double max_align_t;\n"
"typedef int wchar_t;\n"
"\n"
"#define NULL ((void *) 0)\n"
"\n"
"#define offsetof(type, member_designator) ((size_t) & ((type *) 0)->member_designator)\n"
"\n"
"#endif\n";

static const char c_stdbool_h[] =
"#ifndef _STDBOOL_H\n"
"#define _STDBOOL_H\n"
"\n"
"#define bool	_Bool\n"
"#define true	1\n"
"#define false	0\n"
"#define __bool_true_false_are_defined 1\n"
"\n"
"#endif\n";

static const char c_float_h[] =
"#ifndef __FLOAT_H\n"
"#define __FLOAT_H\n"
"\n"
"#define FLT_RADIX 2\n"
"\n"
"#define FLT_MANT_DIG 24\n"
"#define FLT_DIG 6\n"
"#define FLT_DECIMAL_DIG 9\n"
"#define FLT_EPSILON 0x1p-23\n"
"#define FLT_MIN_EXP (-125)\n"
"#define FLT_MAX_EXP 128\n"
"#define FLT_MIN_10_EXP (-37)\n"
"#define FLT_MAX_10_EXP 38\n"
"#define FLT_MIN 0x1p-126\n"
"#define FLT_MAX 0x1.fffffep+127\n"
"#define FLT_TRUE_MIN 0x1p-149\n"
"\n"
"#define DBL_MANT_DIG 53\n"
"#define DBL_DIG 15\n"
"#define DBL_DECIMAL_DIG 17\n"
"#define DBL_EPSILON 0x1p-52\n"
"#define DBL_MIN_EXP (-1021)\n"
"#define DBL_MAX_EXP 1024\n"
"#define DBL_MIN_10_EXP (-307)\n"
"#define DBL_MAX_10_EXP 308\n"
"#define DBL_MIN 0x1p-1022\n"
"#define DBL_MAX 0x1.fffffffffffffp+1023\n"
"#define DBL_TRUE_MIN 0x0.0000000000001p-1022\n"
"\n"
"#define LDBL_MANT_DIG DBL_MANT_DIG\n"
"#define LDBL_DIG DBL_DIG\n"
"#define LDBL_DECIMAL_DIG DBL_DECIMAL_DIG\n"
"#define LDBL_EPSILON DBL_EPSILON\n"
"#define LDBL_MIN_EXP DBL_MIN_EXP\n"
"#define LDBL_MAX_EXP DBL_MAX_EXP\n"
"#define LDBL_MIN_10_EXP DBL_MIN_10_EXP\n"
"#define LDBL_MAX_10_EXP DBL_MAX_10_EXP\n"
"#define LDBL_MIN DBL_MIN\n"
"#define LDBL_MAX DBL_MAX\n"
"#define LDBL_TRUE_MIN DBL_TRUE_MIN\n"
"\n"
"#define FLT_EVAL_METHOD 0\n"
"#define FLT_ROUNDS 1 /* round to the nearest */\n"
"\n"
"#endif /* #ifndef __FLOAT_H */\n";

static const char c_alloca_h[] =
"#define alloca(size) __builtin_alloca(size)\n";

#define STDINC_COUNT 5

static struct {
	yy_sym      name;
	uint32_t    content_len;
	const char *content;
} c_stdinc[STDINC_COUNT];

void c_stdinc_init(void)
{
	yy_sym sym;

	c_stdinc[0].name = yy_hash_lookup("stdarg.h", sizeof("stdarg.h") - 1);
	c_stdinc[0].content = c_stdarg_h;
	c_stdinc[0].content_len = sizeof(c_stdarg_h) - 1;

	c_stdinc[1].name = yy_hash_lookup("stddef.h", sizeof("stddef.h") - 1);
	c_stdinc[1].content = c_stddef_h;
	c_stdinc[1].content_len = sizeof(c_stddef_h) - 1;

	c_stdinc[2].name = yy_hash_lookup("stdbool.h", sizeof("stdbool.h") - 1);
	c_stdinc[2].content = c_stdbool_h;
	c_stdinc[2].content_len = sizeof(c_stdbool_h) - 1;

	c_stdinc[3].name = yy_hash_lookup("float.h", sizeof("float.h") - 1);
	c_stdinc[3].content = c_float_h;
	c_stdinc[3].content_len = sizeof(c_float_h) - 1;

	c_stdinc[4].name = yy_hash_lookup("alloca.h", sizeof("alloca.h") - 1);
	c_stdinc[4].content = c_alloca_h;
	c_stdinc[4].content_len = sizeof(c_alloca_h) - 1;

	yy_file_name = yy_hash_lookup("builtin", sizeof("builtin") - 1);
	yy_pos = yy_text = yy_linepos = yy_buf = c_boot;
	yy_len = 0;
	yy_line = 1;
	yy_end = yy_buf + sizeof(c_boot) - 1;

	do {
		sym = yy_next();
	} while (sym != YY_EOF);
}

const char *c_stdinc_find(yy_sym name, size_t *len)
{
	int i;

	for (i = 0; i < STDINC_COUNT; i++) {
		if (c_stdinc[i].name == name) {
			*len = c_stdinc[i].content_len;
			return c_stdinc[i].content;
		}
	}
	return NULL;
}
