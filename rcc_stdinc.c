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
//"#define __STDC_NO_VLA__          1\n"
/* allow GCC extensions: __attribute__ */
"#define __GNUC__                 4\n"
"#define __GNUC_MINOR__           2\n"
"#define __GNUC_PATCHLEVEL__      0\n"
"#define __USER_LABEL_PREFIX__\n"
#if defined(IR_TARGET_X64)
# if defined(_WIN32)
"#define _LLP64                   1\n"
"#define __LLP64__                1\n"
# else
"#define _LP64                    1\n"
"#define __LP64__                 1\n"
# endif
"#define __amd64                  1\n"
"#define __amd64__                1\n"
"#define __x86_64                 1\n"
"#define __x86_64__               1\n"
"#define __SIZEOF_SHORT__         2\n"
"#define __SIZEOF_INT__           4\n"
# if defined(_WIN32)
"#define __SIZEOF_LONG__          4\n"
# else
"#define __SIZEOF_LONG__          8\n"
#endif
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
# if defined(_WIN32)
"#define __LONG_MAX__             0x7fffffffL\n"
# else
"#define __LONG_MAX__             0x7fffffffffffffffL\n"
# endif
"#define __LONG_LONG_MAX__        0x7fffffffffffffffLL\n"
# if defined(_WIN32)
"#define __PTRDIFF_MAX__          0x7fffffffffffffffLL\n"
# else
"#define __PTRDIFF_MAX__          0x7fffffffffffffffL\n"
# endif
"#define __INT8_MAX__             0x7f\n"
"#define __UINT8_MAX__            0xff\n"
"#define __INT16_MAX__            0x7fff\n"
"#define __UINT16_MAX__           0xffff\n"
"#define __INT32_MAX__            0x7fffffff\n"
"#define __UINT32_MAX__           0xffffffffU\n"
# if defined(_WIN32)
"#define __INT64_MAX__            0x7fffffffffffffffLL\n"
"#define __UINT64_MAX__           0xffffffffffffffffULL\n"
"#define __SIZE_MAX__             0xffffffffffffffffULL\n"
"#define __WCHAR_MAX__            0xffff\n"
# else
"#define __INT64_MAX__            0x7fffffffffffffffL\n"
"#define __UINT64_MAX__           0xffffffffffffffffUL\n"
"#define __SIZE_MAX__             0xffffffffffffffffUL\n"
"#define __WCHAR_MAX__            0x7fffffff\n"
# endif

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MIN_EXP__          (-125)\n"
"#define __FLT_MAX_EXP__          128\n"
"#define __FLT_MIN_10_EXP__       (-37)\n"
"#define __FLT_MAX_10_EXP__       38\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MIN_EXP__          (-1021)\n"
"#define __DBL_MAX_EXP__          1024\n"
"#define __DBL_MIN_10_EXP__       (-307)\n"
"#define __DBL_MAX_10_EXP__       308\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

# if defined(_WIN32)
"#define __SIZE_TYPE__            unsigned long long\n"
"#define __PTRDIFF_TYPE__         long long\n"
"#define __UINTPTR_TYPE__         unsigned long long\n"
"#define __INTPTR_TYPE__          long long\n"
# else
"#define __SIZE_TYPE__            unsigned long\n"
"#define __PTRDIFF_TYPE__         long\n"
"#define __UINTPTR_TYPE__         unsigned long\n"
"#define __INTPTR_TYPE__          long\n"
# endif
"#define __INT8_TYPE__            signed char\n"
"#define __UINT8_TYPE__           unsigned char\n"
"#define __INT16_TYPE__           short int\n"
"#define __UINT16_TYPE__          unsigned short int\n"
"#define __INT32_TYPE__           int\n"
"#define __UINT32_TYPE__          unsigned int\n"
# if defined(_WIN32)
"#define __INT64_TYPE__           long long int\n"
"#define __UINT64_TYPE__          unsigned long long int\n"
"#define __INTMAX_TYPE__          long long int\n"
"#define __UINTMAX_TYPE__         unsigned long long int\n"
# else
"#define __INT64_TYPE__           long int\n"
"#define __UINT64_TYPE__          unsigned long int\n"
"#define __INTMAX_TYPE__          long int\n"
"#define __UINTMAX_TYPE__         unsigned long int\n"
# endif

#ifdef _WIN32
"#define _WIN32                   1\n"
"#define _WIN64                   1\n"
"#define _M_X64                   100\n"
"#define _M_AMD64                 100\n"
"#define _INTEGRAL_MAX_BITS       64\n"
"#define __int8                   char\n"
"#define __int16                  __INT16_TYPE__\n"
"#define __int32                  __INT32_TYPE__\n"
"#define __int64                  __INT64_TYPE__ \n"
#endif

#elif defined(IR_TARGET_X86)
"#define _ILP32                   1\n"
"#define __ILP32__                1\n"
"#define i386                     1\n"
"#define __i386                   1\n"
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
"#define __PTRDIFF_MAX__          0x7ffffffff\n"
"#define __INT8_MAX__             0x7f\n"
"#define __UINT8_MAX__            0xff\n"
"#define __INT16_MAX__            0x7fff\n"
"#define __UINT16_MAX__           0xffff\n"
"#define __INT32_MAX__            0x7fffffff\n"
"#define __UINT32_MAX__           0xffffffffU\n"
"#define __INT64_MAX__            0x7fffffffffffffffLL\n"
"#define __UINT64_MAX__           0xffffffffffffffffULL\n"
"#define __SIZE_MAX__             0xffffffffU\n"
# if defined(_WIN32)
"#define __WCHAR_MAX__            0xffff\n"
# else
"#define __WCHAR_MAX__            0x7fffffff\n"
# endif

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MIN_EXP__          (-125)\n"
"#define __FLT_MAX_EXP__          128\n"
"#define __FLT_MIN_10_EXP__       (-37)\n"
"#define __FLT_MAX_10_EXP__       38\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MIN_EXP__          (-1021)\n"
"#define __DBL_MAX_EXP__          1024\n"
"#define __DBL_MIN_10_EXP__       (-307)\n"
"#define __DBL_MAX_10_EXP__       308\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

"#define __SIZE_TYPE__            unsigned int\n"
"#define __PTRDIFF_TYPE__         int\n"
"#define __UINTPTR_TYPE__         unsigned int\n"
"#define __INTPTR_TYPE__          int\n"
"#define __INT8_TYPE__            signed char\n"
"#define __UINT8_TYPE__           unsigned char\n"
"#define __INT16_TYPE__           short int\n"
"#define __UINT16_TYPE__          unsigned short int\n"
"#define __INT32_TYPE__           int\n"
"#define __UINT32_TYPE__          unsigned int\n"
"#define __INT64_TYPE__           long long int\n"
"#define __UINT64_TYPE__          unsigned long long int\n"
"#define __INTMAX_TYPE__          long long int\n"
"#define __UINTMAX_TYPE__         unsigned long long int\n"

#ifdef _WIN32
"#define _WIN32                   1\n"
"#define _M_IX86                  600\n"
"#define _INTEGRAL_MAX_BITS       64\n"
"#define __int8                   char\n"
"#define __int16                  __INT16_TYPE__\n"
"#define __int32                  __INT32_TYPE__\n"
"#define __int64                  __INT64_TYPE__ \n"
#endif

#elif defined(IR_TARGET_AARCH64)
"#define _LP64                    1\n"
"#define __LP64__                 1\n"
"#define __aarch64__              1\n"
"#define __arm64__                1\n"
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
"#define __PTRDIFF_MAX__          0x7fffffffffffffffL\n"
"#define __INT8_MAX__             0x7f\n"
"#define __UINT8_MAX__            0xff\n"
"#define __INT16_MAX__            0x7fff\n"
"#define __UINT16_MAX__           0xffff\n"
"#define __INT32_MAX__            0x7fffffff\n"
"#define __UINT32_MAX__           0xffffffffU\n"
"#define __INT64_MAX__            0x7fffffffffffffffL\n"
"#define __UINT64_MAX__           0xffffffffffffffffUL\n"
"#define __SIZE_MAX__             0xffffffffffffffffUL\n"
# if defined(_WIN32)
"#define __WCHAR_MAX__            0xffff\n"
# else
"#define __WCHAR_MAX__            0x7fffffff\n"
# endif

"#define __FLT_MANT_DIG__         24\n"
"#define __FLT_DIG__              6\n"
"#define __FLT_DECIMAL_DIG__      9\n"
"#define __FLT_MIN_EXP__          (-125)\n"
"#define __FLT_MAX_EXP__          128\n"
"#define __FLT_MIN_10_EXP__       (-37)\n"
"#define __FLT_MAX_10_EXP__       38\n"
"#define __FLT_MAX__              3.40282346638528859811704183484516925e+38F\n"
"#define __FLT_MIN__              1.17549435082228750796873653722224568e-38F\n"
"#define __FLT_EPSILON__          1.19209289550781250000000000000000000e-7F\n"
"#define __DBL_MANT_DIG__         53\n"
"#define __DBL_DIG__              15\n"
"#define __DBL_DECIMAL_DIG__      17\n"
"#define __DBL_MIN_EXP__          (-1021)\n"
"#define __DBL_MAX_EXP__          1024\n"
"#define __DBL_MIN_10_EXP__       (-307)\n"
"#define __DBL_MAX_10_EXP__       308\n"
"#define __DBL_MAX__              1.79769313486231570814527423731704357e+308\n"
"#define __DBL_MIN__              2.22507385850720138309023271733240406e-308\n"
"#define __DBL_EPSILON__          2.22044604925031308084726333618164062e-16\n"

"#define __SIZE_TYPE__            unsigned long\n"
"#define __PTRDIFF_TYPE__         long\n"
"#define __UINTPTR_TYPE__         unsigned long\n"
"#define __INTPTR_TYPE__          long\n"
"#define __INT8_TYPE__            signed char\n"
"#define __UINT8_TYPE__           unsigned char\n"
"#define __INT16_TYPE__           short int\n"
"#define __UINT16_TYPE__          unsigned short int\n"
"#define __INT32_TYPE__           int\n"
"#define __UINT32_TYPE__          unsigned int\n"
"#define __INT64_TYPE__           long int\n"
"#define __UINT64_TYPE__          unsigned long int\n"
"#define __INTMAX_TYPE__          long int\n"
"#define __UINTMAX_TYPE__         unsigned long int\n"
#endif

#ifdef _WIN32
"#define __WCHAR_TYPE__           unsigned short\n"
#else
"#define __WCHAR_TYPE__           int\n"
#endif
"#define __CHAR16_TYPE__          short unsigned int\n"
"#define __CHAR32_TYPE__          unsigned int\n"

#if defined(__linux__)
"#define linux                    1\n"
"#define __linux                  1\n"
"#define __linux__                1\n"
"#define unix                     1\n"
"#define __unix                   1\n"
"#define __unix__                 1\n"
#elif defined(__APPLE__)
"#define __APPLE__                1\n"
#elif defined(__FreeBSD__)
"#define __FreeBSD__              1\n"
"#define unix                     1\n"
"#define __unix                   1\n"
"#define __unix__                 1\n"
#elif defined(__NetBSD__)
"#define __NetBSD__               1\n"
"#define unix                     1\n"
"#define __unix                   1\n"
"#define __unix__                 1\n"
#elif defined(__unix__)
"#define unix                     1\n"
"#define __unix                   1\n"
"#define __unix__                 1\n"
#endif

"#define __thread                 _Thread_local\n"
"#define static_assert            _Static_assert\n"

"#define __builtin_offsetof(t, f) ((__SIZE_TYPE__)&(((t*)0)->f))\n"

"#define __builtin_isless(x, y)         ((x) < (y))\n"
"#define __builtin_islessequal(x, y)    ((x) <= (y))\n"
"#define __builtin_isgreater(x, y)      ((x) > (y))\n"
"#define __builtin_isgreaterequal(x, y) ((x) >= (y))\n"
"#define __builtin_islessgreater(x, y)  (((x) != (y)) && !__builtin_isunordered(x, y))\n"
"\n";

static const char c_builtin[] =
#if defined(__i386__) || defined(_WIN32) || defined(__APPLE__)
"typedef char *__builtin_va_list;\n"
#elif defined(__x86_64__)
"typedef struct {\n"
"  unsigned int gp_offset;\n"
"  unsigned int fp_offset;\n"
"  void *overflow_arg_area;\n"
"  void *reg_save_area;\n"
"} __builtin_va_list[1];\n"
#elif defined(__aarch64__)
"typedef struct {\n"
"  void    *stack;\n"
"  void    *gr_top;\n"
"  void    *vr_top;\n"
"  int      gr_offset;\n"
"  int      vr_offset;\n"
"} __builtin_va_list[1];\n"
#endif

"int __builtin_memcmp(const void *, const void *, __SIZE_TYPE__) __asm__(\"memcmp\");"
"void *__builtin_memmove(void *, const void *, __SIZE_TYPE__) __asm__(\"memmove\");"

"void __builtin_exit(int) __asm__(\"exit\") __attribute__((noinline))\n;"

"void *__builtin_malloc(__SIZE_TYPE__) __asm__(\"malloc\")\n;"
"void *__builtin_free(void *) __asm__(\"free\")\n;"
"void *__builtin_calloc(__SIZE_TYPE__, __SIZE_TYPE__) __asm__(\"calloc\")\n;"
"void *__builtin_realloc(void *, __SIZE_TYPE__) __asm__(\"realloc\")\n;"

"char *__builtin_strcat(char *, const char *) __asm__(\"strcat\");\n"
"char *__builtin_strchr(const char *, int) __asm__(\"strchr\");\n"
"int __builtin_strcmp(const char *, const char *) __asm__(\"strcmp\");\n"
"char *__builtin_strcpy(char *, const char *) __asm__(\"strcpy\");\n"
#ifdef _WIN32
"char *__builtin_strdup(const char *) __asm__(\"_strdup\");\n"
#else
"char *__builtin_strdup(const char *) __asm__(\"strdup\");\n"
#endif
"__SIZE_TYPE__ __builtin_strlen(const char *) __asm__(\"strlen\");\n"
"char *__builtin_strncat(char *, const char *, __SIZE_TYPE__) __asm__(\"strncat\");\n"
"int __builtin_strncmp(const char *, const char *, __SIZE_TYPE__) __asm__(\"strncmp\");\n"
"char *__builtin_strncpy(char *, const char *, __SIZE_TYPE__) __asm__(\"strncpy\");\n"
"char *__builtin_strndup(const char *, __SIZE_TYPE__) __asm__(\"strndup\");\n"
"char *__builtin_strrchr(const char *, int) __asm__(\"strchr\");\n"

"int __builtin_printf(const char *, ...) __asm__(\"printf\");\n"
"int __builtin_snprintf(char *, __SIZE_TYPE__, const char *, ...) __asm__(\"snprintf\");\n"
"int __builtin_sprintf(char *, const char *, ...) __asm__(\"sprintf\");\n"

"int __builtin_puts(const char *) __asm__(\"puts\");\n"
"\n";

static const char c_stdarg_h[] =
"#ifndef __STDARG_H\n"
"#define __STDARG_H\n"
"\n"
"#ifndef __GNUC_VA_LIST\n"
"#define __GNUC_VA_LIST 1\n"
"typedef __builtin_va_list __gnuc_va_list;\n"
"#endif\n"
"\n"
"#ifndef _VA_LIST_DEFINED\n"
"#define _VA_LIST_DEFINED\n"
"typedef __gnuc_va_list va_list;\n"
"#endif\n"
"\n"
"#define va_start __builtin_va_start\n"
"#define va_arg __builtin_va_arg\n"
"#define va_end(ap) (void)(ap)\n"
#if defined(__i386__) || defined(_WIN32) || defined(__APPLE__)
"#define va_copy(dest, src) ((dest) = (src))\n"
#else
"#define va_copy(dest, src) ((dest)[0] = (src)[0])\n"
#endif
"\n"
"#endif\n";

static const char c_stddef_h[] =
"#ifndef __STDDEF_H\n"
"#define __STDDEF_H\n"
"\n"
"typedef __PTRDIFF_TYPE__ ptrdiff_t;\n"
"typedef __SIZE_TYPE__ size_t;\n"
"typedef long double max_align_t;\n"
"typedef __WCHAR_TYPE__ wchar_t;\n"
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

static const char c_limits_h[] =
"#ifndef _RCC_LIMITS_H\n"
"#define _RCC_LINITS_H\n"
"\n"
"#define CHAR_BIT   8\n"
"#define SCHAR_MIN  (-128)\n"
"#define SCHAR_MAX  127\n"
"#define UCHAR_MAX  255\n"
#if defined(IR_TARGET_X64) || defined(IR_TARGET_X86)
"#define CHAR_MIN   (-128)\n"
"#define CHAR_MAX   127\n"
#else
"#define CHAR_MIN   0\n"
"#define CHAR_MAX   255\n"
#endif
"#define MB_LEN_MAX 16\n"
"#define SHRT_MIN   (-32768)\n"
"#define SHRT_MAX   32767\n"
"#define USHRT_MAX  65535\n"
"#define INT_MIN    (-INT_MAX-1)\n"
"#define INT_MAX    2147483647\n"
"#define UINT_MAX   4294967295U\n"
#if defined(IR_TARGET_X64) || defined(IR_TARGET_AARCH64)
"#define LONG_MIN   (-LONG_MAX-1L)\n"
"#define LONG_MAX   9223372036854775807L\n"
"#define ULONG_MAX  18446744073709551615UL\n"
#else
"#define LONG_MIN   (-LONG_MAX-1)\n"
"#define LONG_MAX   2147483647\n"
"#define ULONG_MAX  4294967295U\n"
#endif
"#define LLONG_MIN  (-LLONG_MAX-1LL)\n"
"#define LLONG_MAX  9223372036854775807LL\n"
"#define ULLONG_MAX 18446744073709551615ULL\n"
"\n"
"#endif /* #ifndef _RCC_LIMITS_H */\n";

#define STDINC_COUNT 6

void c_stdinc_init(rcc_ctx *rcc)
{
	yy_sym sym;

	IR_ASSERT(STDINC_COUNT < STDINC_SIZE);

	rcc->c_stdinc[0].name = yy_hash_lookup(rcc, "stdarg.h", sizeof("stdarg.h") - 1);
	rcc->c_stdinc[0].content = c_stdarg_h;
	rcc->c_stdinc[0].content_len = sizeof(c_stdarg_h) - 1;

	rcc->c_stdinc[1].name = yy_hash_lookup(rcc, "stddef.h", sizeof("stddef.h") - 1);
	rcc->c_stdinc[1].content = c_stddef_h;
	rcc->c_stdinc[1].content_len = sizeof(c_stddef_h) - 1;

	rcc->c_stdinc[2].name = yy_hash_lookup(rcc, "stdbool.h", sizeof("stdbool.h") - 1);
	rcc->c_stdinc[2].content = c_stdbool_h;
	rcc->c_stdinc[2].content_len = sizeof(c_stdbool_h) - 1;

	rcc->c_stdinc[3].name = yy_hash_lookup(rcc, "float.h", sizeof("float.h") - 1);
	rcc->c_stdinc[3].content = c_float_h;
	rcc->c_stdinc[3].content_len = sizeof(c_float_h) - 1;

	rcc->c_stdinc[4].name = yy_hash_lookup(rcc, "alloca.h", sizeof("alloca.h") - 1);
	rcc->c_stdinc[4].content = c_alloca_h;
	rcc->c_stdinc[4].content_len = sizeof(c_alloca_h) - 1;

	rcc->c_stdinc[5].name = yy_hash_lookup(rcc, "limits.h", sizeof("limits.h") - 1);
	rcc->c_stdinc[5].content = c_limits_h;
	rcc->c_stdinc[5].content_len = sizeof(c_limits_h) - 1;

	rcc->yy_file_name = yy_hash_lookup(rcc, "builtin", sizeof("builtin") - 1);
	rcc->yy_pos = rcc->yy_text = rcc->yy_linepos = rcc->yy_buf = c_boot;
	rcc->yy_len = 0;
	rcc->yy_line = 1;
	rcc->yy_end = rcc->yy_buf + sizeof(c_boot) - 1;

	do {
		sym = yy_next(rcc);
	} while (sym != YY_EOF);
}

void c_stdinc_builtin(rcc_ctx *rcc)
{
	rcc->yy_file_name = yy_hash_lookup(rcc, "builtin", sizeof("builtin") - 1);
	rcc->yy_pos = rcc->yy_text = rcc->yy_linepos = rcc->yy_buf = c_builtin;
	rcc->yy_len = 0;
	rcc->yy_line = 1;
	rcc->yy_end = rcc->yy_buf + sizeof(c_builtin) - 1;

	rcc_parse(rcc);
}

const char *c_stdinc_find(rcc_ctx *rcc, yy_sym name, size_t *len)
{
	int i;

	for (i = 0; i < STDINC_COUNT; i++) {
		if (rcc->c_stdinc[i].name == name) {
			*len = rcc->c_stdinc[i].content_len;
			return rcc->c_stdinc[i].content;
		}
	}
	return NULL;
}
