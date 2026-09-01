/*
 * RCC - Rational C Compiler
 * (Common data structures)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */
#ifndef RCC_H
#define RCC_H

#ifdef _WIN32
# if defined(_M_X64) || defined(_M_ARM64)
#  define __SIZEOF_POINTER__ 8
# elif defined(_M_IX86)
#  define __SIZEOF_POINTER__ 4
# endif
#endif

#if __has_attribute(noreturn)
# define yy_noreturn __attribute__((noreturn))
#else
# define yy_noreturn
#endif

/* C tokens */
#define _YY_SYMBOLS(_) \
	_("<EOF>",                         YY_EOF)                         \
	/* terminals */                                                    \
	_("<EOL>",                         YY_EOL)                         \
	_("<WS>",                          YY_WS)                          \
	_("<ONE_LINE_COMMENT>",            YY_ONE_LINE_COMMENT)            \
	_("<COMMENT>",                     YY_COMMENT)                     \
	_("<DECIMAL_NUMBER>",              YY_DECIMAL_NUMBER)              \
	_("<OCTAL_NUMBER>",                YY_OCTAL_NUMBER)                \
	_("<HEXADECIMAL_NUMBER>",          YY_BINARY_NUMBER)               \
	_("<HEXADECIMAL_NUMBER>",          YY_HEXADECIMAL_NUMBER)          \
	_("<FLOATING_NUMBER>",             YY_FLOATING_NUMBER)             \
	_("<HEXADECIMAL_FLOATING_NUMBER>", YY_HEXADECIMAL_FLOATING_NUMBER) \
	_("<PP_NUMBER>",                   YY_PP_NUMBER)                   \
	_("<CHARACTER>",                   YY_CHARACTER)                   \
	_("<STRING>",                      YY_STRING)                      \
	_("<PP_PUNCTUATOR>",               YY_PP_PUNCTUATOR)               \
	_("<ID>",                          YY_ID)                          \
	/* punctuators */                                                  \
	_("[",                             YY__LBRACK)                     \
	_("]",                             YY__RBRACK)                     \
	_("(",                             YY__LPAREN)                     \
	_(")",                             YY__RPAREN)                     \
	_("{",                             YY__LBRACE)                     \
	_("}",                             YY__RBRACE)                     \
	_(".",                             YY__POINT)                      \
	_("->",                            YY__MINUS_GREATER)              \
	_("++",                            YY__PLUS_PLUS)                  \
	_("--",                            YY__MINUS_MINUS)                \
	_("~",                             YY__TILDE)                      \
	_("!",                             YY__BANG)                       \
	_(";",                             YY__SEMICOLON)                  \
	_("...",                           YY__POINT_POINT_POINT)          \
	/* infix operators ordered by their precendency */                 \
	_("*",                             YY__STAR)                       \
	_("/",                             YY__SLASH)                      \
	_("%",                             YY__PERCENT)                    \
	_("+",                             YY__PLUS)                       \
	_("-",                             YY__MINUS)                      \
	_("<<",                            YY__LESS_LESS)                  \
	_(">>",                            YY__GREATER_GREATER)            \
	_("<",                             YY__LESS)                       \
	_(">",                             YY__GREATER)                    \
	_("<=",                            YY__LESS_EQUAL)                 \
	_(">=",                            YY__GREATER_EQUAL)              \
	_("==",                            YY__EQUAL_EQUAL)                \
	_("!=",                            YY__BANG_EQUAL)                 \
	_("&",                             YY__AND)                        \
	_("^",                             YY__UPARROW)                    \
	_("|",                             YY__BAR)                        \
	_("&&",                            YY__AND_AND)                    \
	_("||",                            YY__BAR_BAR)                    \
	_("?",                             YY__QUERY)                      \
	_(":",                             YY__COLON)                      \
	_("=",                             YY__EQUAL)                      \
	_("*=",                            YY__STAR_EQUAL)                 \
	_("/=",                            YY__SLASH_EQUAL)                \
	_("%=",                            YY__PERCENT_EQUAL)              \
	_("+=",                            YY__PLUS_EQUAL)                 \
	_("-=",                            YY__MINUS_EQUAL)                \
	_("<<=",                           YY__LESS_LESS_EQUAL)            \
	_(">>=",                           YY__GREATER_GREATER_EQUAL)      \
	_("&=",                            YY__AND_EQUAL)                  \
	_("^=",                            YY__UPARROW_EQUAL)              \
	_("|=",                            YY__BAR_EQUAL)                  \
	_(",",                             YY__COMMA)                      \
	/* preprocessor symbols */                                         \
	_("#",                             YY__HASH)                       \
	_("##",                            YY__HASH_HASH)                  \
	_("##",                            YY_PP_JOIN)                     \
	_("<PLACE>",                       YY_PP_PLACE_MARKER)             \

#define YY_FIRST_KEYWORD               YY_AUTO

#define _YY_KEYWORDS(_) \
	_("auto",                          YY_AUTO)                        \
	_("break",                         YY_BREAK)                       \
	_("case",                          YY_CASE)                        \
	_("char",                          YY_CHAR)                        \
	_("const",                         YY_CONST)                       \
	_("continue",                      YY_CONTINUE)                    \
	_("default",                       YY_DEFAULT)                     \
	_("do",                            YY_DO)                          \
	_("double",                        YY_DOUBLE)                      \
	_("else",                          YY_ELSE)                        \
	_("enum",                          YY_ENUM)                        \
	_("extern",                        YY_EXTERN)                      \
	_("float",                         YY_FLOAT)                       \
	_("for",                           YY_FOR)                         \
	_("goto",                          YY_GOTO)                        \
	_("if",                            YY_IF)                          \
	_("inline",                        YY_INLINE)                      \
	_("int",                           YY_INT)                         \
	_("long",                          YY_LONG)                        \
	_("register",                      YY_REGISTER)                    \
	_("restrict",                      YY_RESTRICT)                    \
	_("return",                        YY_RETURN)                      \
	_("short",                         YY_SHORT)                       \
	_("signed",                        YY_SIGNED)                      \
	_("sizeof",                        YY_SIZEOF)                      \
	_("static",                        YY_STATIC)                      \
	_("struct",                        YY_STRUCT)                      \
	_("switch",                        YY_SWITCH)                      \
	_("typedef",                       YY_TYPEDEF)                     \
	_("union",                         YY_UNION)                       \
	_("unsigned",                      YY_UNSIGNED)                    \
	_("void",                          YY_VOID)                        \
	_("volatile",                      YY_VOLATILE)                    \
	_("while",                         YY_WHILE)                       \
	_("_Alignas",                      YY__ALIGNAS)                    \
	_("_Alignof",                      YY__ALIGNOF)                    \
	_("_Atomic",                       YY__ATOMIC)                     \
	_("_Bool",                         YY__BOOL)                       \
	_("_Complex",                      YY__COMPLEX)                    \
	_("_Generic",                      YY__GENERIC)                    \
	_("_Imaginary",                    YY__IMAGINARY)                  \
	_("_Noreturn",                     YY__NORETURN)                   \
	_("_Static_assert",                YY__STATIC_ASSERT)              \
	_("_Thread_local",                 YY__THREAD_LOCAL)               \
	/* extensions */                                                   \
	_("asm",                           YY_ASM)                         \
	_("typeof",                        YY_TYPEOF)                      \
	_("__typeof",                      YY___TYPEOF)                    \
	_("__typeof__",                    YY___TYPEOF__)                  \
	_("__alignof",                     YY___ALIGNOF)                   \
	_("__alignof__",                   YY___ALIGNOF__)                 \
	_("__asm",                         YY___ASM)                       \
	_("__asm__",                       YY___ASM__)                     \
	_("__attribute",                   YY___ATTRIBUTE)                 \
	_("__attribute__",                 YY___ATTRIBUTE__)               \
	_("__cdecl",                       YY___CDECL)                     \
	_("__const",                       YY___CONST)                     \
	_("__const__",                     YY___CONST__)                   \
	_("__complex",                     YY___COMPLEX)                   \
	_("__complex__",                   YY___COMPLEX__)                 \
	_("__declspec",                    YY___DECLSPEC)                  \
	_("__extension__",                 YY___EXTENSION__)               \
	_("__fastcall",                    YY___FASTCALL)                  \
	_("__forceinline",                 YY___FORCEINLINE)               \
	_("__inline",                      YY___INLINE)                    \
	_("__inline__",                    YY___INLINE__)                  \
	_("__label__",                     YY___LABEL__)                   \
	_("__restrict",                    YY___RESTRICT)                  \
	_("__restrict__",                  YY___RESTRICT__)                \
	_("__signed",                      YY___SIGNED)                    \
	_("__signed__",                    YY___SIGNED__)                  \
	_("__unaligned",                   YY___UNALIGNED)                 \
	_("__volatile",                    YY___VOLATILE)                  \
	_("__volatile__",                  YY___VOLATILE__)                \
	_("__int128",                      YY___INT128)                    \
	_("__int128_t",                    YY___INT128_T)                  \
	_("__uint128_t",                   YY___UINT128_T)                 \
	_("__builtin_va_start",            YY___BUILTIN_VA_START)          \
	_("__builtin_va_arg",              YY___BUILTIN_VA_ARG)            \
	_("__builtin_va_end",              YY___BUILTIN_VA_END)            \
	_("__builtin_va_copy",             YY___BUILTIN_VA_COPY)           \
	_("__builtin_alloca",              YY___BUILTIN_ALLOCA)            \
	_("__builtin_abort",               YY___BUILTIN_ABORT)             \
	_("__builtin_trap" ,               YY___BUILTIN_TRAP)              \
	_("__builtin_debugtrap",           YY___BUILTIN_DEBUGTRAP)         \
	_("__builtin_frame_address",       YY___BUILTIN_FRAME_ADDRESS)     \
	_("__builtin_choose_expr",         YY___BUILTIN_CHOOSE_EXPR)       \
	_("__builtin_classify_type",       YY___BUILTIN_CLASSIFY_TYPE)     \
	_("__builtin_constant_p",          YY___BUILTIN_CONSTANT_P)        \
	_("__builtin_types_compatible_p",  YY___BUILTIN_TYPES_COMPATIBLE_P)\
	_("__builtin_abs",                 YY___BUILTIN_ABS)               \
	_("__builtin_labs",                YY___BUILTIN_LABS)              \
	_("__builtin_llabs",               YY___BUILTIN_LLABS)             \
	_("__builtin_fabs",                YY___BUILTIN_FABS)              \
	_("__builtin_fabsf",               YY___BUILTIN_FABSF)             \
	_("__builtin_bswap16",             YY___BUILTIN_BSWAP16)           \
	_("__builtin_bswap32",             YY___BUILTIN_BSWAP32)           \
	_("__builtin_bswap64",             YY___BUILTIN_BSWAP64)           \
	_("__builtin_popcount",            YY___BUILTIN_POPCOUNT)          \
	_("__builtin_popcountl",           YY___BUILTIN_POPCOUNTL)         \
	_("__builtin_popcountll",          YY___BUILTIN_POPCOUNTLL)        \
	_("__builtin_clz",                 YY___BUILTIN_CLZ)               \
	_("__builtin_clzl",                YY___BUILTIN_CLZL)              \
	_("__builtin_clzll",               YY___BUILTIN_CLZLL)             \
	_("__builtin_ctz",                 YY___BUILTIN_CTZ)               \
	_("__builtin_ctzl",                YY___BUILTIN_CTZL)              \
	_("__builtin_ctzll",               YY___BUILTIN_CTZLL)             \
	_("__builtin_ffs",                 YY___BUILTIN_FFS)               \
	_("__builtin_ffsl",                YY___BUILTIN_FFSL)              \
	_("__builtin_ffsll",               YY___BUILTIN_FFSLL)             \
	_("__builtin_huge_val",            YY___BUILTIN_HUGE_VAL)          \
	_("__builtin_huge_valf",           YY___BUILTIN_HUGE_VALF)         \
	_("__builtin_inf",                 YY___BUILTIN_INF)               \
	_("__builtin_inff",                YY___BUILTIN_INFF)              \
	_("__builtin_isunordered",         YY___BUILTIN_ISUNORDERED)       \
	_("__builtin_memcpy",              YY___BUILTIN_MEMCPY)            \
	_("__builtin_memset",              YY___BUILTIN_MEMSET)            \
	_("__builtin_nan",                 YY___BUILTIN_NAN)               \
	_("__builtin_nanf",                YY___BUILTIN_NANF)              \
	_("__builtin_add_overflow",        YY___BUILTIN_ADD_OVERFLOW)      \
	_("__builtin_add_overflow_p",      YY___BUILTIN_ADD_OVERFLOW_P)    \
	_("__builtin_sadd_overflow",       YY___BUILTIN_SADD_OVERFLOW)     \
	_("__builtin_saddl_overflow",      YY___BUILTIN_SADDL_OVERFLOW)    \
	_("__builtin_saddll_overflow",     YY___BUILTIN_SADDLL_OVERFLOW)   \
	_("__builtin_uadd_overflow",       YY___BUILTIN_UADD_OVERFLOW)     \
	_("__builtin_uaddl_overflow",      YY___BUILTIN_UADDL_OVERFLOW)    \
	_("__builtin_uaddll_overflow",     YY___BUILTIN_UADDLL_OVERFLOW)   \
	_("__builtin_sub_overflow",        YY___BUILTIN_SUB_OVERFLOW)      \
	_("__builtin_sub_overflow_p",      YY___BUILTIN_SUB_OVERFLOW_P)    \
	_("__builtin_ssub_overflow",       YY___BUILTIN_SSUB_OVERFLOW)     \
	_("__builtin_ssubl_overflow",      YY___BUILTIN_SSUBL_OVERFLOW)    \
	_("__builtin_ssubll_overflow",     YY___BUILTIN_SSUBLL_OVERFLOW)   \
	_("__builtin_usub_overflow",       YY___BUILTIN_USUB_OVERFLOW)     \
	_("__builtin_usubl_overflow",      YY___BUILTIN_USUBL_OVERFLOW)    \
	_("__builtin_usubll_overflow",     YY___BUILTIN_USUBLL_OVERFLOW)   \
	_("__builtin_mul_overflow",        YY___BUILTIN_MUL_OVERFLOW)      \
	_("__builtin_mul_overflow_p",      YY___BUILTIN_MUL_OVERFLOW_P)    \
	_("__builtin_smul_overflow",       YY___BUILTIN_SMUL_OVERFLOW)     \
	_("__builtin_smull_overflow",      YY___BUILTIN_SMULL_OVERFLOW)    \
	_("__builtin_smulll_overflow",     YY___BUILTIN_SMULLL_OVERFLOW)   \
	_("__builtin_umul_overflow",       YY___BUILTIN_UMUL_OVERFLOW)     \
	_("__builtin_umull_overflow",      YY___BUILTIN_UMULL_OVERFLOW)    \
	_("__builtin_umulll_overflow",     YY___BUILTIN_UMULLL_OVERFLOW)   \
	_("__builtin_convertvector",       YY___BUILTIN_CONVERTVECTOR)     \
	_("__builtin_shuffle",             YY___BUILTIN_SHUFFLE)           \
	_("__builtin_shufflevector",       YY___BUILTIN_SHUFFLEVECTOR)     \
	_("__builtin_expect",              YY___BUILTIN_EXPECT)            \
	_("__builtin_prefetch",            YY___BUILTIN_PREFETCH)          \
	_("__builtin_unreachable",         YY___BUILTIN_UNREACHABLE)       \

#define YY_LAST_KEYWORD                YY___BUILTIN_UNREACHABLE

#define _YY_DIRECTIVES(_) \
	_("define",                        YY_DEFINE)                      \
	_("include",                       YY_INCLUDE)                     \
	_("include_next",                  YY_INCLUDE_NEXT)                \
	_("ifdef",                         YY_IFDEF)                       \
	_("ifndef",                        YY_IFNDEF)                      \
	_("elif",                          YY_ELIF)                        \
	_("endif",                         YY_ENDIF)                       \
	_("defined",                       YY_DEFINED)                     \
	_("undef",                         YY_UNDEF)                       \
	_("error",                         YY_ERROR)                       \
	_("line",                          YY_LINE)                        \
	_("pragma",                        YY_PRAGMA)                      \
	_("warning",                       YY_WARNING)                     \
	_("__has_include",                 YY___HAS_INCLUDE)               \
	_("__has_include_next",            YY___HAS_INCLUDE_NEXT)          \
	_("__has_attribute",               YY___HAS_ATTRIBUTE)             \
	_("__has_builtin",                 YY___HAS_BUILTIN)               \

#define _YY_NAMES(_) \
	_("__COUNTER__",                   YY___COUNTER__)                 \
	_("__DATE__",                      YY___DATE__)                    \
	_("__FILE__",                      YY___FILE__)                    \
	_("__FUNCTION__",                  YY___FUNCTION__)                \
	_("__PRETTY_FUNCTION__",           YY___PRETTY_FUNCTION__)         \
	_("__func__",                      YY___FUNC__)                    \
	_("__LINE__",                      YY___LINE__)                    \
	_("__TIME__",                      YY___TIME__)                    \
	_("__INCLUDE_LEVEL__",             YY___INCLUDE_LEVEL__)           \
	_("__BASE_FILE__",                 YY___BASE_FILE__)               \
	_("__VA_ARGS__",                   YY___VA_ARGS__)                 \
	_("_Pragma",                       YY__PRAGMA)                     \
	_("__pragma",                      YY___PRAGMA)                    \
	_("region",                        YY_REGION)                      \
	_("endregion",                     YY_ENDREGION)                   \
	_("E",                             YY_E)                           \
	_("comment",                       YY__COMMENT)                    \
	_("once",                          YY_ONCE)                        \
	_("option",                        YY_OPTION)                      \
	_("push_macro",                    YY_PUSH_MACRO)                  \
	_("pop_macro",                     YY_POP_MACRO)                   \
	_("pack",                          YY_PACK)                        \
	_("push",                          YY_PUSH)                        \
	_("pop",                           YY_POP)                         \
	_("memcpy",                        YY_MEMCPY)                      \
	_("memset",                        YY_MEMSET)                      \
	_("strndup",                       YY_STRNDUP)                     \
	_("abort",                         YY_ABORT)                       \
	_("_setjmp",                       YY__SETJMP)                     \
	_("main",                          YY_MAIN)                        \
	/* GCC attributes */                                               \
	_("alias",                         YY_ALIAS)                /*f  */\
	_("__alias__",                     YY___ALIAS__)            /*f  */\
	_("align",                         YY_ALIGN)                /* vt*/\
	_("aligned",                       YY_ALIGNED)              /* vt*/\
	_("__aligned__",                   YY___ALIGNED__)          /* vt*/\
	_("always_inline",                 YY_ALWAYS_INLINE)        /*f  */\
	_("__always_inline__",             YY___ALWAYS_INLINE__)    /*f  */\
	_("cdecl",                         YY_CDECL)                /*f  */\
	_("__cdecl__",                     YY___CDECL__)            /*f  */\
	_("cleanup",                       YY_CLEANUP)              /* v */\
	_("__cleanup__",                   YY___CLEANUP__)          /* v */\
	_("cold",                          YY_COLD)                 /*f  */\
	_("__cold__",                      YY___COLD__)             /*f  */\
	/*_("const",                       YY_CONST)*/              /*f  */\
	/*_("__const__",                   YY___CONST__)*/          /*f  */\
	_("constructor",                   YY_CONSTRUCTOR)          /*f  */\
	_("__constructor__",               YY___CONSTRUCTOR__)      /*f  */\
	_("destructor",                    YY_DESTRUCTOR)           /*f  */\
	_("__destructor__",                YY___DESTRUCTOR__)       /*f  */\
	_("deprecated",                    YY_DEPRECATED)           /*f t*/\
	_("__deprecated__",                YY___DEPRECATED__)       /*f t*/\
	_("dllexport",                     YY_DLLEXPORT)            /*fv */\
	_("__dllexport__",                 YY___DLLEXPORT__)        /*fv */\
	_("dllimport",                     YY_DLLIMPORT)            /*fv */\
	_("__dllimport__",                 YY___DLLIMPORT__)        /*fv */\
	_("fallthrough",                   YY_FALLTHROUGH)          /*  s*/\
	_("__fallthrough__",               YY___FALLTHROUGH__)      /*  s*/\
	_("fastcall",                      YY_FASTCALL)             /*f  */\
	_("__fastcall__",                  YY___FASTCALL__)         /*f  */\
	_("format",                        YY_FORMAT)               /*f  */\
	_("__format__",                    YY___FORMAT__)           /*f  */\
	_("format_arg",                    YY_FORMAT_ARG)           /*f  */\
	_("__format_arg__",                YY___FORMAT_ARG__)       /*f  */\
	_("gcc_struct",                    YY_GCC_STRUCT)           /*  t*/\
	_("__gcc_struct__",                YY___GCC_STRUCT__)       /*  t*/\
	_("hot",                           YY_HOT)                  /*f  */\
	_("__hot__",                       YY___HOT__)              /*f  */\
	_("leaf",                          YY_LEAF)                 /*f  */\
	_("__leaf__",                      YY___LEAF__)             /*f  */\
	_("malloc",                        YY_MALLOC)               /*f  */\
	_("__malloc__",                    YY___MALLOC__)           /*f  */\
	_("may_alias",                     YY_MAY_ALIAS)            /*  t*/\
	_("__may_alias__",                 YY___MAY_ALIAS__)        /*  t*/\
	_("mode",                          YY_MODE)                 /* v */\
	_("__mode__",                      YY___MODE__)             /* v */\
	_("ms_abi",                        YY_MS_ABI)               /*f  */\
	_("__ms_abi__",                    YY___MS_ABI__)           /*f  */\
	_("ms_struct",                     YY_MS_STRUCT)            /*  t*/\
	_("__ms_struct__",                 YY___MS_STRUCT__)        /*  t*/\
	_("musttail",                      YY_MUSTTAIL)             /*  s*/\
	_("__musttail__",                  YY___MUSTTAIL__)         /*  s*/\
	_("noinline",                      YY_NOINLINE)             /*f  */\
	_("__noinline__",                  YY___NOINLINE__)         /*f  */\
	_("nonnull",                       YY_NONNULL)              /*f  */\
	_("__nonnull__",                   YY___NONNULL__)          /*f  */\
	_("noreturn",                      YY_NORETURN)             /*f  */\
	_("__noreturn__",                  YY___NORETURN__)         /*f  */\
	_("nothrow",                       YY_NOTHROW)              /*f  */\
	_("__nothrow__",                   YY___NOTHROW__)          /*f  */\
	_("packed",                        YY_PACKED)               /* vt*/\
	_("__packed__",                    YY___PACKED__)           /* vt*/\
	_("preserve_none",                 YY_PRESERVE_NONE)        /*f  */\
	_("__preserve_none__",             YY___PRESERVE_NONE__)    /*f  */\
	_("pure",                          YY_PURE)                 /*f  */\
	_("__pure__",                      YY___PURE__)             /*f  */\
	_("regparm",                       YY_REGPARM)              /*f  */\
	_("__regparm__",                   YY___REGPARM__)          /*f  */\
	_("saveall",                       YY_SAVEALL)              /*f  */\
	_("__saveall__",                   YY___SAVEALL__)          /*f  */\
	_("section",                       YY_SECTION)              /*fv */\
	_("__section__",                   YY___SECTION__)          /*fv */\
	_("selectany",                     YY_SELECTANY)            /* v */\
	_("__selectany__",                 YY___SELECTANY__)        /* v */\
	_("sentinel",                      YY_SENTINEL)             /*f  */\
	_("__sentinel__",                  YY___SENTINEL__)         /*f  */\
	_("shared",                        YY_SHARED)               /* v */\
	_("__shared__",                    YY___SHARED__)           /* v */\
	_("sseregparam",                   YY_SSEREGPARAM)          /*f  */\
	_("__sseregparam__",               YY___SSEREGPARAM__)      /*f  */\
	_("stdcall",                       YY_STDCALL)              /*f  */\
	_("__stdcall__",                   YY___STDCALL__)          /*f  */\
	_("sysv_abi",                      YY_SYSV_ABI)             /*f  */\
	_("__sysv_abi__",                  YY___SYSV_ABI__)         /*f  */\
	_("tls_model",                     YY_TLS_MODEL)            /* v */\
	_("__tls_model__",                 YY___TLS_MODEL__)        /* v */\
	_("transparent_union",             YY_TRANSPARENT_UNION)    /*  t*/\
	_("__transparent_union__",         YY___TRANSPARENT_UNION__)/*  t*/\
	_("unused",                        YY_UNUSED)               /*fvt*/\
	_("__unused__",                    YY___UNUSED__)           /*fvt*/\
	_("used",                          YY_USED)                 /*f  */\
	_("__used__",                      YY___USED__)             /*f  */\
	_("vector_size",                   YY_VECTOR_SIZE)          /*fv */\
	_("__vector_size__",               YY___VECTOR_SIZE__)      /*fv */\
	_("visibility",                    YY_VISIBILITY)           /*f  */\
	_("__visibility__",                YY___VISIBILITY__)       /*f  */\
	_("warn_unused_result",            YY_WARN_UNUSED_RESULT)   /*f  */\
	_("__warn_unused_result__",        YY___WARN_UNUSED_RESULT__) /*f  */\
	_("weak",                          YY_WEAK)                 /*fv */\
	_("__weak__",                      YY___WEAK__)             /*fv */\
	_("weakref",                       YY_WEAKREF)              /*f  */\
	_("__weakref__",                   YY___WEAKREF__ )         /*f  */\
	_("byte",                          YY_BYTE)                        \
	_("__byte__",                      YY___BYTE__)                    \
	_("word",                          YY_WORD)                        \
	_("__word__",                      YY___WORD__)                    \
	_("QI",                            YY_QI)                          \
	_("__QI__",                        YY___QI__)                      \
	_("HI",                            YY_HI)                          \
	_("__HI__",                        YY___HI__)                      \
	_("SI",                            YY_SI)                          \
	_("__SI__",                        YY___SI__)                      \
	_("DI",                            YY_DI)                          \
	_("__DI__",                        YY___DI__)                      \
	_("SF",                            YY_SF)                          \
	_("__SF__",                        YY___SF__)                      \
	_("DF",                            YY_DF)                          \
	_("__DF__",                        YY___DF__)                      \
	/* builtin functions */                                            \
	_("alloca",                        YY_ALLOCA)                      \
	_("__va_start",                    YY___VA_START)                  \
	_("abs",                           YY_ABS)                         \
	_("labs",                          YY_LABS)                        \
	_("llabs",                         YY_LLABS)                       \
	_("fabs",                          YY_FABS)                        \
	_("fabsf",                         YY_FABSF)                       \
	_("ceil",                          YY_CEIL)                        \
	_("ceilf",                         YY_CEILF)                       \
	_("floor",                         YY_FLOOR)                       \
	_("floorf",                        YY_FLOORF)                      \
	_("nearbyint",                     YY_NEARBYINT)                   \
	_("nearbyintf",                    YY_NEARBYINTF)                  \
	_("rint",                          YY_RINT)                        \
	_("rintf",                         YY_RINTF)                       \
	_("sqrt",                          YY_SQRT)                        \
	_("sqrtf",                         YY_SQRTF)                       \
	_("trunc",                         YY_TRUNC)                       \
	_("truncf",                        YY_TRUNCF)                      \

typedef enum {
#define _YY_SYM(str, id) id,
_YY_SYMBOLS(_YY_SYM)
_YY_KEYWORDS(_YY_SYM)
_YY_DIRECTIVES(_YY_SYM)
_YY_NAMES(_YY_SYM)
YY_LAST_NAME,
YY_BUILTIN_FIRST = YY_ALLOCA,
#if defined(IR_TARGET_X64) || defined(IR_TARGET_X86)
YY_BUILTIN_LAST = YY_TRUNCF,
#else
YY_BUILTIN_LAST = YY_FABSF,
#endif
YY_LAST = 0x7fffffff,
#undef _YY_SYM
} yy_sym;

#define YY_HAS_ATTRIBUTE(id) (((id) >= YY_ALIAS && (id) <= YY___WEAKREF__) || (id) == YY_CONST || (id) == YY___CONST__)
#define YY_HAS_BUILTIN(id)   ((id) >= YY___BUILTIN_VA_START && (id) <= YY___BUILTIN_UNREACHABLE)

/* yy_flags bits */
#define YY_SKIP_WS           (1<<0)
#define YY_SKIP_EOL          (1<<1)
#define YY_SKIP_COMMENTS     (1<<2)
#define YY_ACCEPT_PP_NUMBER  (1<<3)
#define YY_ACCEPT_PUNCTUATOR (1<<4)
#define YY_NO_MACRO          (1<<5)
#define YY_NO_DIRECTIVE      (1<<6)
#define YY_ACCEPT_NOSUBST    (1<<7)

#define PP_NO_LINEMARKERS    (1<<8)
#define PP_NO_OUTPUT         (1<<9)
#define PP_DUMP_MACROS       (1<<10)
#define PP_DUMP_MACRO_NAMES  (1<<11)
#define PP_DUMP_INCLUDES     (1<<12)
#define PP_EVAL_EXPRESSION   (1<<13)
#define PP_PREPROCESS        (1<<14)
#define PP_ASM_COMMENTS      (1<<15)

#define YY_FLAGS_DEFAULT     (YY_SKIP_WS|YY_SKIP_EOL|YY_SKIP_COMMENTS)
#define YY_FLAGS_PP_DEFAULT  (PP_PREPROCESS|YY_SKIP_COMMENTS|YY_ACCEPT_PP_NUMBER|YY_ACCEPT_PUNCTUATOR)
#define YY_FLAGS_PP          (PP_NO_LINEMARKERS|PP_NO_OUTPUT|PP_DUMP_MACROS|PP_DUMP_MACRO_NAMES|PP_DUMP_INCLUDES|PP_EVAL_EXPRESSION|PP_PREPROCESS)
#define YY_FLAGS_C           0

/* C compiler context/state */
typedef struct _rcc_ctx rcc_ctx;

/* C scanner */
yy_sym yy_next(rcc_ctx *rcc);

/* C symbol table */
typedef struct _pp_macro pp_macro;
typedef struct _pp_macro_list pp_macro_list;
typedef struct _c_sym c_sym;
typedef struct _c_tag c_tag;
typedef struct _c_label c_label;
typedef struct _c_linker_sym c_linker_sym;

typedef struct _yy_hash_bucket {
	uint32_t                 h;      /* hash value */
	uint32_t                 next;   /* index of next bucket for hash conflict resolution */
	size_t                   len;    /* string length */
	const char              *str;
	pp_macro                *macro;
	pp_macro_list           *macro_stack;
	c_sym                   *sym;
	c_tag                   *tag;
	c_label                 *label;
	c_linker_sym            *link;
} yy_hash_bucket;

typedef struct {
	yy_hash_bucket          *data;
	uint32_t                 count;
	uint32_t                 size;
	uint32_t                 mask;
} yy_hashtab;

void yy_hash_init(rcc_ctx *rcc);
void yy_hash_free(rcc_ctx *rcc);
yy_sym yy_hash_find(rcc_ctx *rcc, const char *str, size_t len);
yy_sym yy_hash_lookup(rcc_ctx *rcc, const char *str, size_t len);

/* Dynamic Strings */
typedef struct {
	char   *str;
	size_t  len;
} yy_dyn_str;

void yy_dyn_str_init(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len);
void yy_dyn_str_init0(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len);
char *yy_dyn_str_grow(rcc_ctx *rcc, yy_dyn_str *dyn_str, size_t len);
void yy_dyn_str_append(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len);
void yy_dyn_str_append0(rcc_ctx *rcc, yy_dyn_str *dyn_str, const char *str, size_t len);

/* C preprocessor */
#define PP_SUBST_STACK_SIZE  32
#define PP_LIST_CACHE_SIZE   32

/* pp_macro.flags bits */
#define PP_MACRO_FUNCTION    (1<<0)
#define PP_MACRO_VAR_ARG     (1<<1)
#define PP_MACRO_EMPTY       (1<<2)
#define PP_MACRO_HAS_JOIN    (1<<3)
#define PP_MACRO_BUILTIN     (1<<4)
#define PP_MACRO_PREDEFINED  (1<<5)
#define PP_MACRO_EXPANDED    (1<<6)
#define PP_MACRO_DISABLED    (1<<7)

#define PP_IS_ID(sym)        ((sym) >= YY_FIRST_KEYWORD)
#define PP_HAS_VAL(sym)      ((sym) > YY_WS && (sym) <= YY_ID)

#define PP_NOSUBST           0x40000000

#define PP_MACRO_ARG         0x40000000
#define PP_STRINGIZE         0x20000000

struct _pp_macro {
	uint32_t                 flags;
	int32_t                  num_args;
	yy_sym                   tokens[];
};

struct _pp_macro_list {
	pp_macro                *macro;
	pp_macro_list           *next;
};

typedef struct {
	yy_sym                  *tokens;
	yy_sym                  *start;
	pp_macro                *macro;
	uint32_t                 size;
	bool                     skip_eof;
} pp_subst_stream;

typedef struct {
	yy_sym                  *syms;
	uint32_t                 size;
	uint32_t                 len;
} pp_list;

bool pp_add_include_dir(rcc_ctx *rcc, const char *path);
void pp_add_sys_include_dirs(rcc_ctx *rcc);

void pp_start(rcc_ctx *rcc);
void pp_dtor(rcc_ctx *rcc);
pp_subst_stream *pp_push_stream(rcc_ctx *rcc);
pp_subst_stream *pp_pop_stream(rcc_ctx *rcc);
void pp_macro_define(rcc_ctx *rcc, yy_sym name, uint32_t flags, uint32_t num_args, yy_sym *tokens, uint32_t len);
bool pp_macro_expand(rcc_ctx *rcc, pp_macro *macro, yy_sym sym);
void pp_parse_directive(rcc_ctx *rcc);
void pp_pop_include(rcc_ctx *rcc);
void pp_preprocess(rcc_ctx *rcc, FILE *f);
void pp_list_grow(pp_list *l, uint32_t size);

/* C Semantic & IR Code Generation */
#define C_TYPE_SPEC_VOID         (1<<0)
#define C_TYPE_SPEC_CHAR         (1<<1)
#define C_TYPE_SPEC_BOOL         (1<<2)
#define C_TYPE_SPEC_INT          (1<<3)
#define C_TYPE_SPEC_FLOAT        (1<<4)
#define C_TYPE_SPEC_DOUBLE       (1<<5)
#define C_TYPE_SPEC_COMPLEX      (1<<6)
#define C_TYPE_SPEC_SHORT        (1<<7)
#define C_TYPE_SPEC_LONG         (1<<8)
#define C_TYPE_SPEC_LONG_LONG    (1<<9)
#define C_TYPE_SPEC_SIGNED       (1<<10)
#define C_TYPE_SPEC_UNSIGNED     (1<<11)
#define C_TYPE_SPEC_ATOMIC       (1<<12)
#define C_TYPE_SPEC_STRUCT       (1<<13)
#define C_TYPE_SPEC_UNION        (1<<14)
#define C_TYPE_SPEC_ENUM         (1<<15)
#define C_TYPE_SPEC_NAME         (1<<16)
#define C_TYPE_SPEC_TYPE         (1<<17)

#if defined(IR_64) && !defined(_WIN32)
# define C_TYPE_SPEC_INT64       C_TYPE_SPEC_LONG
#else
# define C_TYPE_SPEC_INT64       (C_TYPE_SPEC_LONG_LONG|C_TYPE_SPEC_LONG)
#endif

#define C_TYPE_SPEC_ANY_MODE     (C_TYPE_SPEC_SIGNED-2)
#define C_TYPE_SPEC_ANY          ((1<<18)-1)

#define C_DCL_TYPEDEF            (1<<18)
#define C_DCL_EXTERN             (1<<19)
#define C_DCL_STATIC             (1<<20)
#define C_DCL_AUTO               (1<<21)
#define C_DCL_REGISTER           (1<<22)
#define C_DCL_THREAD_LOCAL       (1<<23)
#define C_DCL_ENUM_CONST         (1<<24) /* used internally */
#define C_DCL_DEFINITION         (1<<25) /* used internally */
#define C_DCL_PARAM              (1<<26) /* used internally */
#define C_DCL_FOR                (1<<27) /* used internally */
#define C_DCL_REG_VAR            (1<<28) /* used internally */
#define C_DCL_HAS_ASM_NAME       (1<<29) /* used internally */
#define C_DCL_STATEMENT          (1<<30) /* used internally */

#define C_DCL_STORAGE_CLASS      (C_DCL_TYPEDEF|C_DCL_EXTERN|C_DCL_STATIC|C_DCL_AUTO|C_DCL_REGISTER|C_DCL_THREAD_LOCAL)

#define C_ATTR_ALIGN_MASK        ((1<<7)-1) /* 6 low bits, log2 of alignment-1 */

/* type attributes */
#define C_ATTR_CONST             (1<<7)
#define C_ATTR_RESTRICT          (1<<8)
#define C_ATTR_VOLATILE          (1<<9)
#define C_ATTR_ATOMIC            (1<<10)

/* enum or struct type attributes */
#define C_ATTR_PACKED            (1<<11)

/* array type attributes */
#define C_ATTR_FLEXIBLE          (1<<12) /* flexible (without defined length) array */
#define C_ATTR_VLA               (1<<13) /* variable length array */

/* varable modified type (e.g. pointer to VLA) */
#define C_ATTR_VMT               (1<<14)

/* struct type attributes */
#define C_ATTR_MS_STRUCT         (1<<15)
#define C_ATTR_GCC_STRUCT        (1<<16)

/* function attributes */
#define C_ATTR_VARIADIC          (1<<17)
#define C_ATTR_INLINE            (1<<18)
#define C_ATTR_NORETURN          (1<<19)
#define C_ATTR_ALWAYS_INLINE     (1<<20)
#define C_ATTR_NOINLINE          (1<<21)
#define C_ATTR_NOTHROW           (1<<22)
#define C_ATTR_CONST_FUNC        (1<<23)
#define C_ATTR_PURE              (1<<24)
#define C_ATTR_LEAF              (0)        /* TODO: find a free bit ??? */
#define C_ATTR_HOT               (0)        /* TODO: find a free bit ??? */
#define C_ATTR_COLD              (0)        /* TODO: find a free bit ??? */
#define C_ATTR_DEPRECATED        (0)        /* TODO: find a free bit ??? */
#define C_ATTR_UNUSED            (1<<25)
#define C_ATTR_OLD_FUNC          (1<<26)

/* symbol attributes */
#define C_ATTR_WEAK              (1<<27)

/* calling convention */
#define C_ATTR_CALL_CONV         ((1<<28) | (1<<29) | (1<<30))

#define C_ATTR_CC_DEFAULT        0
#define C_ATTR_CC_CDECL          (1U<<28)
#define C_ATTR_CC_FASTCALL       (2U<<28)
#define C_ATTR_CC_PRESERVE_NONE  (3U<<28)

#if defined(IR_TARGET_X64)
# define C_ATTR_CC_X86_64_SYSV   (4U<<28)
# define C_ATTR_CC_X86_64_MS     (5U<<28)
#elif defined(IR_TARGET_X86)
# define C_ATTR_CC_REGPARM_1     (4U<<28)
# define C_ATTR_CC_REGPARM_2     (5U<<28)
# define C_ATTR_CC_REGPARM_3     (6U<<28)
#endif

#define C_TYPE_ATTRS \
	(C_ATTR_CONST|C_ATTR_RESTRICT|C_ATTR_VOLATILE|C_ATTR_ATOMIC)

#define C_STRUCT_ATTRS \
	(C_ATTR_PACKED|C_ATTR_MS_STRUCT|C_ATTR_GCC_STRUCT)

#define C_ENUM_ATTRS \
	(C_ATTR_PACKED)

#define C_ARRAY_ATTRS \
	(C_ATTR_CONST|C_ATTR_VLA|C_ATTR_FLEXIBLE)

#define C_FUNC_TYPE_ATTRS \
	(C_ATTR_VARIADIC|C_ATTR_INLINE|C_ATTR_NORETURN|C_ATTR_ALWAYS_INLINE|C_ATTR_NOINLINE|C_ATTR_NOTHROW \
		|C_ATTR_LEAF|C_ATTR_PURE|C_ATTR_HOT|C_ATTR_COLD|C_ATTR_DEPRECATED|C_ATTR_CALL_CONV \
		|C_ATTR_UNUSED|C_ATTR_OLD_FUNC|C_ATTR_CONST_FUNC)

#define C_POINTER_ATTRS \
	(C_ATTR_CONST|C_ATTR_RESTRICT)

#define C_ATTR2_CONSTRUCTOR      (1<<0)
#define C_ATTR2_DESTRUCTOR       (1<<1)

/* statement attributes */
#define C_ATTR2_FALLTHROUGH      (1<<30)
#define C_ATTR2_MUSTTAIL         (1U<<31)

typedef enum {
	C_TYPE_VOID,
	C_TYPE_BOOL,
	C_TYPE_U8,
	C_TYPE_U16,
	C_TYPE_U32,
	C_TYPE_UL,
	C_TYPE_ULL,
	C_TYPE_CHAR,
	C_TYPE_I8,
	C_TYPE_I16,
	C_TYPE_I32,
	C_TYPE_IL,
	C_TYPE_ILL,
	C_TYPE_FLOAT,
	C_TYPE_DOUBLE,
	C_TYPE_LONG_DOUBLE,
	C_TYPE_ENUM,
	C_TYPE_POINTER,
	C_TYPE_FUNC,
	C_TYPE_ARRAY,
	C_TYPE_STRUCT,
	C_TYPE_UNION,
	C_TYPE_VECTOR,
	C_TYPE_FLOAT_COMPLEX,
	C_TYPE_DOUBLE_COMPLEX,
	C_TYPE_LONG_DOUBLE_COMPLEX,
} c_type_kind;

#define C_IS_TYPE_KIND_SCALAR(t)        ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_LONG_DOUBLE)
#define C_IS_TYPE_KIND_INT(t)           ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_ILL)
#define C_IS_TYPE_KIND_UNSIGNED(t)      ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_ULL)
#define C_IS_TYPE_KIND_SIGNED(t)        ((t) >= C_TYPE_CHAR && (t) <= C_TYPE_ILL)
#define C_IS_TYPE_KIND_FP(t)            ((t) >= C_TYPE_FLOAT && (t) <= C_TYPE_LONG_DOUBLE)

#define C_IS_TYPE_SCALAR(t)        C_IS_TYPE_KIND_SCALAR((t)->kind)
#define C_IS_TYPE_INT(t)           C_IS_TYPE_KIND_INT((t)->kind)
#define C_IS_TYPE_UNSIGNED(t)      C_IS_TYPE_KIND_UNSIGNED((t)->kind)
#define C_IS_TYPE_SIGNED(t)        C_IS_TYPE_KIND_SIGNED((t)->kind)
#define C_IS_TYPE_FP(t)            C_IS_TYPE_KIND_FP((t)->kind)

#define C_IS_TYPE_NUM(t)           (C_IS_TYPE_INT(t) || C_IS_TYPE_FP(t))
#define C_IS_TYPE_SCALAR_OR_PTR(t) (C_IS_TYPE_SCALAR(t) || (t)->kind == C_TYPE_ENUM || (t)->kind == C_TYPE_POINTER)
#define C_IS_TYPE_INT_OR_PTR(t)    (C_IS_TYPE_INT(t) || (t)->kind == C_TYPE_ENUM || (t)->kind == C_TYPE_POINTER)

typedef enum {
	C_TYPE_INCOMPLETE = (1<<0), /* incomplete (not defined) enum, struct, union */
	C_TYPE_INPROGRESS = (1<<1), /* incomplete (not completely defined) struct, union */
	C_TYPE_GLOBAL     = (1<<2),
	C_TYPE_IN_FUNC    = (1<<3),
	C_TYPE_OPAQUE     = (1<<4), /* opaque vector */
} c_type_flag;

typedef yy_sym c_name;

typedef struct _c_scope c_scope;
typedef struct _c_loop  c_loop;
typedef struct _c_type  c_type;
typedef struct _c_dcl   c_dcl;
typedef struct _c_field c_field;
typedef struct _c_param c_param;

struct _c_type {
	c_type_kind            kind : 8;
	ir_type                ir_type : 8;
	uint8_t                flags : 8;
	uint32_t               attr;
	size_t                 size;
	union {
		struct {
			c_name         tag;
			c_type_kind    kind;
			c_name        *values; /* fake pointer for comparison only */
		} enumeration;
		struct {
			const c_type  *type;
			uintptr_t      length;
			yy_sym        *vla_tokens;
		} array;
		struct {
			const c_type  *type;
		} pointer;
		struct {
			c_name         tag;
			uint32_t       num_fields;
			c_field       *fields;
		} record;
		struct {
			uint32_t       num_params;
//			ffi_abi        abi;
			const c_type  *ret_type;
			c_param       *params;
		} func;
		struct {
			const c_type  *type;
			uintptr_t      length;
		} vec;
		c_name             tag;
	};
};

#define C_VAL_REF      (1<<0)
#define C_VAL_CONST    (1<<1)
#define C_VAL_LVAL     (1<<2)
#define C_VAL_VAR      (1<<3)
#define C_VAL_REG      (1<<4)
#define C_VAL_VOLATILE (1<<5)
#define C_VAL_BUILTIN  (1<<5)
#define C_VAL_INLINE   (1<<6)
#define C_VAL_STR      (1<<7)

typedef struct {
	const c_type  *type;
	ir_insn        u;    /* u.op keeps C_VAL_* flags, u.op1 - ref, u.proto - bits */
} c_value;

struct _c_dcl {
	uint32_t               flags;
	uint32_t               attr;
	const c_type          *type;
	c_name                 alias;
	c_name                 cleanup_func; /* may be set for local variables */
	int8_t                 reg;
	uint32_t               attr2;
	uint32_t               vector_size;
};

typedef enum {
	C_SYM_TYPE,
	C_SYM_FUNC,
	C_SYM_CONST,
	C_SYM_VAR,
	C_SYM_PARAM = C_SYM_VAR,
} c_sym_kind;

typedef enum {
	C_LINK_NONE,
	C_LINK_EXTERNAL,
	C_LINK_INTERNAL,
	C_LINK_BUILTIN,
} c_sym_linkage;

typedef struct _c_reloc c_reloc;

struct _c_reloc {
	size_t   obj_offset;
	c_name   name;
	size_t   name_offset;
	c_reloc *next;
};

struct _c_linker_sym {
	const void *addr;
	c_reloc    *reloc;
	bool        is_thunk;
	bool        is_asm_name;
};

struct _c_sym {
	c_sym_kind             kind: 3;            /* enum bit-filed requires an extra bit for MSVC ??? */
	c_sym_linkage          linkage: 3;         /* only for C_SYM_VAR and C_SYM_FUNC */
	bool                   is_external: 1;     /* only for C_SYM_VAR and C_SYM_FUNC */
	bool                   is_thread_local: 1; /* only for C_SYM_VAR */
	bool                   is_implemented: 1;  /* only for C_SYM_VAR and C_SYM_FUNC */
	bool                   is_string: 1;
	bool                   is_thunk: 1;        /* TODO: replace thunks with relocs ??? */
	uint8_t                has_code: 2;        /* Code generation state (see C_CODE_...) */
	bool                   tmp_data: 1;        /* temporary growable data area */
	bool                   has_asm_name: 1;
	union {
		c_name             alias;
		c_name             cleanup_func;
	};
	c_scope               *scope;
	c_value                value;              /* type is part of the value */
	union {
		c_reloc           *reloc;              /* list of cross-references to other symbols */
		ir_ctx            *ctx;                /* function IR (used for delayed code-gen or function inlining) */
		c_sym             *cleanup_next;
	};
};

struct _c_tag {
	const c_type          *type;
	c_scope               *scope;
};

/* c_sym.has_code values */
#define C_CODE_NONE        0
#define C_CODE_SCHEDULED   1
#define C_CODE_STARTED     2
#define C_CODE_DONE        3

#define C_IS_SIMPLE_VAL(bit_field)    ((bit_field) == 0)
#define C_IS_BIT_FIELD(bit_field)     ((bit_field) & (1 << 12))
#define C_BIT_FIELD(start, lenght)    ((1 << 12) | ((start) << 6) | (lenght))
#define C_BIT_FIELD_START(bit_field)  (((bit_field) >> 6) & 0x3f)
#define C_BIT_FIELD_SIZE(bit_field)   ((bit_field) & 0x3f)

#define C_IS_BIT_FIELD_PACKED(bit_field)  ((bit_field) & (1 << 13))
#define C_SET_BIT_FIELD_PACKED(bit_field) do {(bit_field) |= (1 << 13);} while (0)

#define C_IS_VECTOR_DIM(proto)            ((proto) & (1 << 14))
#define C_VECTOR_DIM(type)                ((1 << 14) | (type))
#define C_VECTOR_DIM_TYPE(proto)          (proto & 0xff)

struct _c_field {
	c_name                 name;
	uint16_t               bit_field; /* 1-bit - is bit-field, 6-bits - first bit, 6-bits - bit lenght */
	size_t                 offset;
	const c_type          *type;
};

#define C_ALLOCA_PARAMS  16
#define C_ALLOCA_FIELDS  16
#define C_ALLOCA_STRINGS 16

struct _c_param {
	c_name                 name;
	const c_type          *type;
};

struct _c_scope {
	pp_list   list;
	void     *checkpoint;
	ir_ref    vla_block;
	ir_ref    last_vla_block;
	c_sym    *cleanup_sym;
	c_scope  *prev;
};

typedef struct _c_case_labels c_case_labels;

struct _c_loop {
	bool           is_switch;
	const c_type  *switch_type;
	ir_ref         start;
	ir_ref         check;
	ir_ref         next;
	ir_ref         break_list;
	ir_ref         continue_list;
	c_scope       *scope;
	c_loop        *prev;
	c_case_labels *case_labels;
};

struct _c_label {
	bool      is_local;
	bool      is_unused;
	ir_ref    dst;
	ir_ref    src_list;
	ir_ref    vla_block;
	ir_ref    value_sym;      /* used for labels as value and computed goto */
	ir_ref    value_block;    /* used for labels as value and computed goto */
	c_sym    *cleanup_sym;
	c_scope  *scope;
};

#define C_INIT_STACK_SIZE 32

typedef struct {
	size_t             size;
	uint32_t           level;
	uint32_t           ranges;
	c_type             holder;
	struct {
		const c_type  *type;
		uintptr_t      pos;
		uintptr_t      last;
	} stack[C_INIT_STACK_SIZE];
} c_init;

typedef struct {
	const c_type      *type;
	ir_ref             old_control;
	ir_ref             last_control;
	c_value            matched_value;
	ir_ref             matched_control_start;
	ir_ref             matched_control_end;
	c_value            default_value;
	ir_ref             default_control_start;
	ir_ref             default_control_end;
} c_generic;

extern const c_type c_type_void;
extern const c_type c_type_bool;
extern const c_type c_type_char;
extern const c_type c_type_u8;
extern const c_type c_type_i8;
extern const c_type c_type_u16;
extern const c_type c_type_i16;
extern const c_type c_type_u32;
extern const c_type c_type_i32;
extern const c_type c_type_ul;
extern const c_type c_type_ull;
extern const c_type c_type_il;
extern const c_type c_type_ill;
extern const c_type c_type_float;
extern const c_type c_type_double;
extern const c_type c_type_long_double;
extern const c_type c_type_float_complex;
extern const c_type c_type_double_complex;
extern const c_type c_type_long_double_complex;
extern const c_type c_type_string;
extern const c_type c_type_lstring;
extern const c_type c_type_string_u16;
extern const c_type c_type_string_u32;
extern const c_type c_type_const_string;
extern const c_type c_type_const_lstring;
extern const c_type c_type_const_string_u16;
extern const c_type c_type_const_string_u32;
extern const c_type c_type_ptr;
extern const c_type c_type_const_ptr;

#if __SIZEOF_SIZE_T__ == 8 && !defined(_WIN32)
# define C_LONG_SIZE    8
# define C_LONG_ALIGN   4
# define c_type_size_t  c_type_ul
# define c_type_ssize_t c_type_il
# define c_type_i64     c_type_il
# define c_type_u64     c_type_ul
# define C_TYPE_I64     C_TYPE_IL
# define C_TYPE_U64     C_TYPE_UL
# define C_TYPE_SIZE_T  C_TYPE_UL
# define IR_LONG        IR_I64
# define IR_ULONG       IR_U64
#elif __SIZEOF_SIZE_T__ == 8 && defined(_WIN32)
# define C_LONG_SIZE    4
# define C_LONG_ALIGN   3
# define c_type_size_t  c_type_ull
# define c_type_ssize_t c_type_ill
# define c_type_i64     c_type_ill
# define c_type_u64     c_type_ull
# define C_TYPE_I64     C_TYPE_ILL
# define C_TYPE_U64     C_TYPE_ULL
# define C_TYPE_SIZE_T  C_TYPE_U64
# define IR_LONG        IR_I32
# define IR_ULONG       IR_U32
#else
# define C_LONG_SIZE    4
# define C_LONG_ALIGN   3
# define c_type_size_t  c_type_u32
# define c_type_ssize_t c_type_i32
# define c_type_i64     c_type_ill
# define c_type_u64     c_type_ull
# define C_TYPE_I64     C_TYPE_ILL
# define C_TYPE_U64     C_TYPE_ULL
# define C_TYPE_SIZE_T  C_TYPE_U32
# define IR_LONG        IR_I32
# define IR_ULONG       IR_U32
#endif

#ifdef _WIN32
# define C_WCHAR_SIZE   2
# define C_WCHAR_ALIGN  2
# define C_WCHAR_SIGNED 1
# define c_type_wchar_t c_type_u16
# define C_TYPE_WCHAR_T C_TYPE_U16
# define IR_WCHAR       IR_U16
#else
# define C_WCHAR_SIZE   4
# define C_WCHAR_ALIGN  3
# define C_WCHAR_SIGNED 0
# define c_type_wchar_t c_type_i32
# define C_TYPE_WCHAR_T C_TYPE_I32
# define IR_WCHAR       IR_I32
#endif

#define C_POP_MASK   0x3
#define C_POP_SYM    0x0
#define C_POP_TAG    0x1
#define C_POP_LABEL  0x2

void c_push_scope(rcc_ctx *rcc, c_scope *scope);
void c_pop_scope(rcc_ctx *rcc, c_scope *scope);
void c_pop_scope_light(rcc_ctx *rcc, c_scope *scope);

void c_wrong_type_specifiers(rcc_ctx *rcc, uint32_t flags, yy_sym sym);
const c_type *c_resolve_type(rcc_ctx *rcc, c_dcl *dcl);
const c_type *c_resolve_type_name(rcc_ctx *rcc, c_name name);
void c_resolve_sym_name(rcc_ctx *rcc, c_value *res, c_name name, yy_sym sym);
c_type *c_resolve_tag(rcc_ctx *rcc, c_name name, c_dcl *dcl, bool define, const c_type *underlying_type);
c_type *c_make_struct_type(rcc_ctx *rcc, c_dcl *dcl, c_name tag);
c_type *c_make_enum_type(rcc_ctx *rcc, c_dcl *dcl, c_name tag, const c_type *underlying_type);
const c_type *c_underlying_enum_type(rcc_ctx *rcc, c_dcl *dcl);
void c_make_pointer_type(rcc_ctx *rcc, c_dcl *dcl);
void c_make_array_type(rcc_ctx *rcc, c_dcl *dcl, c_dcl *dim, c_value *len, uint64_t attr, bool is_param);
void c_make_func_type(rcc_ctx *rcc, c_dcl *dcl, c_param *params, uint32_t num_params, uint32_t attr);
void c_make_nested_type(rcc_ctx *rcc, c_dcl *dcl, c_dcl *nested);
void c_finish_struct_type(rcc_ctx *rcc, c_type *type, c_dcl *d);
void c_finish_enum_type(rcc_ctx *rcc, c_type *dcl, c_dcl *d, int64_t min, uint64_t max);
void c_validate_func_params(rcc_ctx *rcc, c_name name, c_dcl *dcl);

c_sym *c_declare(rcc_ctx *rcc, c_name name, c_dcl *dcl);
void c_declare_struct_field(rcc_ctx *rcc, c_type *type, c_name name, c_dcl *field, c_value *bits);
void c_declare_enum_val(rcc_ctx *rcc, const c_type *type, c_name name, c_dcl *attr, c_value *val, int64_t *min, uint64_t *max, c_value *last);
void c_declare_func_param(rcc_ctx *rcc, c_param **params, uint32_t *num_params, c_name name, c_dcl *param);
void c_declare_func_param_name(rcc_ctx *rcc, c_param **params, uint32_t *num_params, c_name name);
void c_declare_func_param_type(rcc_ctx *rcc, const c_type *type, c_name name, c_dcl *param);
void c_declare_local_label(rcc_ctx *rcc, c_name name);
void c_empty_declaration(rcc_ctx *rcc, c_dcl *d);

void c_gcc_attribute_aligned(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *v);
void c_gcc_attribute_packed(rcc_ctx *rcc, c_dcl *d, c_name attr);
void c_gcc_attribute_cleanup(rcc_ctx *rcc, c_dcl *d, c_name attr, c_name func);
void c_gcc_attribute_regparm(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *v);
void c_gcc_attribute_vector_size(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *v);
yy_sym c_gcc_attribute(rcc_ctx *rcc, c_dcl *dcl, c_name attr, yy_sym sym);
void c_gcc_attribute_alias(rcc_ctx *rcc, c_dcl *d, c_name attr, c_value *v);
void c_asm_alias(rcc_ctx *rcc, c_dcl *d, c_value *v);
void c_declspec_align(rcc_ctx *rcc, c_dcl *dcl, c_value *v);
yy_sym c_declspec(rcc_ctx *rcc, c_dcl *dcl, c_name attr, yy_sym sym);

void c_sizeof_type(rcc_ctx *rcc, c_value *res, const c_type *type);
void c_sizeof_expr(rcc_ctx *rcc, yy_sym op, c_value *expr, ir_ref old_control);
void c_alignof_type(rcc_ctx *rcc, c_value *res, const c_type *type);
void c_alignas_expr(rcc_ctx *rcc, c_dcl *dcl, c_value *expr);
const c_type *c_typeof_expr(rcc_ctx *rcc, c_value *expr, ir_ref old_control);

void c_static_assert(rcc_ctx *rcc, c_value *expr, c_value *msg);

c_sym *c_global_sym(rcc_ctx *rcc, c_sym *sym);
yy_sym c_get_current_func_name(rcc_ctx *rcc);

void c_type2proto_ex(rcc_ctx *rcc, const c_type *t,
                     uint32_t *flags_ptr, ir_type *ret_type_ptr,
                     uint32_t *params_count_ptr, uint8_t *param_types);

/* IR Code Generation */
void c_value_rval(rcc_ctx *rcc, c_value *val);

ir_ref c_do_nocode(rcc_ctx *rcc);
void c_do_end_nocode(rcc_ctx *rcc, ir_ref old_control);
ir_ref c_do_alloca(rcc_ctx *rcc, size_t size, uint32_t align, bool zero);
void c_do_cast(rcc_ctx *rcc, const c_type *t, c_value *v);
void c_do_post_op(rcc_ctx *rcc, yy_sym sym, c_value *v);
void c_do_pre_op(rcc_ctx *rcc, yy_sym sym, c_value *v);
void c_do_addr(rcc_ctx *rcc, c_value *v);
void c_do_deref(rcc_ctx *rcc, c_value *v);
void c_do_unary_plus(rcc_ctx *rcc, c_value *v);
void c_do_neg(rcc_ctx *rcc, c_value *v);
void c_do_not(rcc_ctx *rcc, c_value *v);
void c_do_bool_not(rcc_ctx *rcc, c_value *v);
void c_do_array_dim(rcc_ctx *rcc, c_value *v, c_value *dim);
void c_do_struct_field(rcc_ctx *rcc, c_value *v, c_name field);
void c_do_struct_field_deref(rcc_ctx *rcc, c_value *v, c_name field);
c_value *c_do_grow_actual_parameters(rcc_ctx *rcc, c_value *args, uint32_t num_args);
void c_do_builtin(rcc_ctx *rcc, c_value *val, c_name name, uint32_t num_args, c_value *args);
void c_do_builtin_constant_p(rcc_ctx *rcc, c_value *val);
void c_do_builtin_classify_type(rcc_ctx *rcc, c_value *val, const c_type *type);
void c_do_builtin_types_compatible_p(rcc_ctx *rcc, c_value *val, const c_type *type);
void c_do_builtin_va_arg(rcc_ctx *rcc, c_value *val, const c_type *type);
void c_do_builtin_convertvector(rcc_ctx *rcc, c_value *val, const c_type *type);
void c_do_call(rcc_ctx *rcc, c_value *func, uint32_t num_args, c_value *args, c_value *res);
void c_do_binary_op(rcc_ctx *rcc, yy_sym sym, c_value *v, c_value *op2);
void c_do_assign_op(rcc_ctx *rcc, yy_sym sym, c_value *v, c_value *op2);
ir_ref c_do_bool_and_start(rcc_ctx *rcc, c_value *v);
void c_do_bool_and_end(rcc_ctx *rcc, c_value *v, c_value *op2, ir_ref if_ref);
ir_ref c_do_bool_or_start(rcc_ctx *rcc, c_value *v);
void c_do_bool_or_end(rcc_ctx *rcc, c_value *v, c_value *op2, ir_ref if_ref);
void c_do_cond_op(rcc_ctx *rcc, c_value *v, c_value *op2, c_value *op3);
void c_do_statement_expression(rcc_ctx *rcc, c_scope *scope, c_value *v);

ir_ref c_do_if(rcc_ctx *rcc, c_value *cond);
void c_do_if_else(rcc_ctx *rcc, ir_ref _if, bool orig_dead_code);
void c_do_if_end(rcc_ctx *rcc, ir_ref _if, bool orig_dead_code);
void c_do_switch(rcc_ctx *rcc, c_loop *loop, c_value *cond);
void c_do_case(rcc_ctx *rcc, c_value *v);
void c_do_case_range(rcc_ctx *rcc, c_value *v1, c_value *v2);
void c_do_case_default(rcc_ctx *rcc);
void c_do_switch_end(rcc_ctx *rcc, c_loop *loop);
void c_do_loop_start(rcc_ctx *rcc, c_loop *loop);
void c_do_loop_check(rcc_ctx *rcc, c_loop *loop, c_value *cond);
void c_do_loop_continue_label(rcc_ctx *rcc, c_loop *loop);
void c_do_loop_end(rcc_ctx *rcc, c_loop *loop);
void c_do_for_next_start(rcc_ctx *rcc, c_loop *loop);
void c_do_for_next_end(rcc_ctx *rcc, c_loop *loop);
void c_do_for_end(rcc_ctx *rcc, c_loop *loop);
void c_do_continue(rcc_ctx *rcc);
void c_do_break(rcc_ctx *rcc);
void c_do_return(rcc_ctx *rcc, c_value *v);
void c_do_tailcall(rcc_ctx *rcc, c_value *v);
void c_do_goto(rcc_ctx *rcc, c_name name);
c_label *c_do_set_label(rcc_ctx *rcc, c_name name);
void c_do_set_label_attrs(rcc_ctx *rcc, c_label *label, c_dcl *attrs);
void c_do_finish_label(rcc_ctx *rcc, c_name name, c_label *label);
void c_do_label_value(rcc_ctx *rcc, c_value *res, c_name label);
void c_do_computed_goto(rcc_ctx *rcc, c_value *v);

void c_do_init_obj(rcc_ctx *rcc, c_sym *obj, c_value *v);
void c_do_init_start(rcc_ctx *rcc, c_sym *obj, c_init *init);
void c_do_init_dim(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *dim);
void c_do_init_range(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *last);
void c_do_init_field(rcc_ctx *rcc, c_sym *obj, c_init *init, c_name field);
void c_do_init_next(rcc_ctx *rcc, c_sym *obj, c_init *init);
void c_do_init_rollback(rcc_ctx *rcc, c_sym *obj, c_init *init, uint32_t orig_level, uint32_t level);
void c_do_init_set(rcc_ctx *rcc, c_sym *obj, c_init *init, c_value *val);
void c_do_init_nested(rcc_ctx *rcc, c_sym *obj, c_init *init, bool b);
void c_do_init_end(rcc_ctx *rcc, c_sym *obj, c_init *init);

void c_do_init_expr_start(rcc_ctx *rcc, c_sym *obj, const c_type *t);
void c_do_init_expr_end(rcc_ctx *rcc, c_value *v, c_sym *obj, size_t size);

void c_do_generic_start(rcc_ctx *rcc, c_generic *g);
void c_do_generic_type(rcc_ctx *rcc, c_generic *g, const c_type *t, bool is_type);
void c_do_generic_case(rcc_ctx *rcc, c_generic *g, const c_type *t, c_value *v);
void c_do_generic_default(rcc_ctx *rcc, c_generic *g, c_value *v);
void c_do_generic_end(rcc_ctx *rcc, c_value *res, c_generic *g);

void c_do_func_start(rcc_ctx *rcc, c_name name, c_dcl *d, c_scope *scope);
void c_do_func_end(rcc_ctx *rcc, c_name name, c_dcl *d, c_scope *scope);

void c_do_compile_start(rcc_ctx *rcc);
void c_do_compile_end(rcc_ctx *rcc);

/* Inline assembler */
#define C_MAX_ASM_OPERANDS 30 /* limit 30 is define in GCC info */

#define C_ASM_VOLATILE          (1<<0)
#define C_ASM_INLINE            (1<<1)
#define C_ASM_GOTO              (1<<2)
#define C_ASM_CLOBBERS_CC       (1<<3)
#define C_ASM_CLOBBERS_MEMORY   (1<<4)
#define C_ASM_CLOBBERS_REDZONE  (1<<5)
#define C_ASM_HAS_LABELS        (1<<6)
#define C_ASM_HAS_OUT_REGS      (1<<7)

#define C_ASM_OP_OUTPUT         (1<<0)
#define C_ASM_OP_INPUT          (1<<1)
#define C_ASM_OP_LABEL          (1<<2)
#define C_ASM_OP_INOUT          (1<<3)

#define C_ASM_OP_CLOBBERED      (1<<4)

#define C_ASM_OP_REG_INT        (1<<8)
#define C_ASM_OP_REG_FP         (1<<9)
#define C_ASM_OP_IMM_INT        (1<<10)
#define C_ASM_OP_IMM_FP         (1<<11)
#define C_ASM_OP_MEM            (1<<12)

#define C_ASM_OP_ANY            (C_ASM_OP_REG_INT|C_ASM_OP_REG_FP|C_ASM_OP_IMM_INT|C_ASM_OP_IMM_FP|C_ASM_OP_MEM)

typedef struct _c_asm_operand {
	uint32_t    flags;
	c_name      id;
	const char *constraint_str;
	size_t      constraint_len;
	c_value     val;
} c_asm_operand;

typedef struct _c_asm {
	uint32_t flags;
	uint64_t clobbers;
	c_asm_operand ops[C_MAX_ASM_OPERANDS];
} c_asm;

void c_do_asm_operand_constraint(rcc_ctx *rcc, c_asm *a, bool out, int n, c_name name, c_value *constraint);
void c_do_asm_operand_val(rcc_ctx *rcc, c_asm *a, bool out, int n, c_value *val);
void c_do_asm_clobbers(rcc_ctx *rcc, c_asm *a, c_value *val);
void c_do_asm_operand_label(rcc_ctx *rcc, c_asm *a, int n, c_name label);
void c_do_asm(rcc_ctx *rcc, c_value *asm_str, c_asm *a, int n);
void c_do_global_asm(rcc_ctx *rcc, c_value *asm_str);

/* C Parser */
bool parse_pp_expr(rcc_ctx *rcc);
const char* parse_pp_string(rcc_ctx *rcc, size_t *len);
void parse_vla_param_again(rcc_ctx *rcc, yy_sym *vla_tokens, c_value *val);
void rcc_parse(rcc_ctx *rcc);

/* Error Reporting */
#define E_ERROR                        (1<<0)
#define E_WARNING                      (1<<1)

#define E_WRITE_STRINGS                (1<<2)
#define E_UNSUPPORTED                  (1<<3)
#define E_IMPLICIT_FUNC_DCL            (1<<4)
#define E_DISCARDED_QUALIFIERS         (1<<5)

#define E_ERRORS_DEFAULT               E_ERROR
#define E_WARNINGS_DEFAULT             (E_WARNING|E_UNSUPPORTED|E_IMPLICIT_FUNC_DCL|E_DISCARDED_QUALIFIERS)

#define E_WARNINGS_NONE                0x00000000U
#define E_WARNINGS_ALL                 (E_WARNING|E_UNSUPPORTED|E_IMPLICIT_FUNC_DCL|E_DISCARDED_QUALIFIERS)

#define yy_error(_msg)                 yy_error_(rcc, _msg)
#define yy_error_fmt(_msg, ...)        yy_error_fmt_(rcc, _msg, __VA_ARGS__)
#define yy_warning(_msg)               yy_warning_(rcc, E_WARNING, _msg)
#define yy_warning_fmt(_msg, ...)      yy_warning_fmt_(rcc, E_WARNING, _msg, __VA_ARGS__)

#define yy_warning_ex(_kind, _msg)            yy_warning_(rcc, _kind, _msg)
#define yy_warning_ex_fmt(_kind, _msg, ...)   yy_warning_fmt_(rcc, _kind, _msg, __VA_ARGS__)

void yy_error_(rcc_ctx *rcc, const char *msg) yy_noreturn;
void yy_error_fmt_(rcc_ctx *rcc, const char *fmt, ...) yy_noreturn;
void yy_warning_(rcc_ctx *rcc, uint32_t kind, const char *msg);
void yy_warning_fmt_(rcc_ctx *rcc, uint32_t kind, const char *fmt, ...);

/* Linker */
void *c_linker_allocate_data(rcc_ctx *rcc, c_name name, size_t size, size_t align, bool is_array);
bool  c_linker_fix_reloc(rcc_ctx *rcc, c_sym *obj, size_t obj_offset, c_value *val);
void  c_linker_del_reloc(rcc_ctx *rcc, c_sym *obj, size_t obj_offset);
void  c_linker_del_relocs(rcc_ctx *rcc, c_sym *obj, size_t obj_offset, size_t size);

/* IR compiler */
void rcc_ir_init(rcc_ctx *rcc, uint32_t flags);
void rcc_ir_compile(rcc_ctx *rcc, c_name name, c_dcl *d, c_sym *sym);

/* C compiler context/state */
#define INCLUDE_STACK_SIZE   32
#define IFDEF_STACK_SIZE     256
#define PACK_STACK_SIZE      16
#define PP_MAX_INCLUDE_PATHS 31

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
	int                      next_dir;
} pp_include_state;

#define STDINC_SIZE          16

typedef struct _c_stdinc_t {
	yy_sym      name;
	uint32_t    content_len;
	const char *content;
} c_stdinc_t;

typedef struct _rcc_loader {
	ir_loader          l;
	rcc_ctx           *rcc;
} rcc_loader;

#define C_OPT_LEVEL              0x3
#define C_OPT_INLINE             (1<<2)
#define C_OPT_MEM2SSA            (1<<3)
#define C_OPT_TAILCALL           (1<<4)

struct _rcc_ctx {
	/* C Scanner */
	const char           *yy_pos;            /* pointer to current scanned character          */
	const char           *yy_text;           /* pointer to start of the current scanned token */
	const char           *yy_linepos;        /* pointer to start of the current scanned line  */
	size_t                yy_len;            /* length of the value of terminal token */
	int32_t               yy_line;           /* line number */
	yy_sym                yy_file_name;      /* interned file name */
	const char           *yy_buf;
	const char           *yy_end;

	yy_hashtab            yy_hash;
	ir_arena             *yy_arena;
	uint32_t              yy_flags;

	/* C Preprocessor */
	pp_list               pp_list_cache[PP_LIST_CACHE_SIZE];
	uint32_t              pp_list_cache_idx;

	pp_subst_stream       pp_subst_stack[PP_SUBST_STACK_SIZE];
	pp_include_state      pp_include_stack[INCLUDE_STACK_SIZE];
	uint8_t               pp_ifdef_stack[IFDEF_STACK_SIZE];

	pp_subst_stream      *pp_stream;
	uint32_t              pp_ifdef_level;          /* ifdef nesting level */
	uint32_t              pp_include_level;        /* include nesting level */
	uint32_t              pp_include_ifdef_level;  /* ifdef nesting level for the current include */
	uint32_t              pp_include_ifndef_state; /* state to catch includes protected by #ifndef macro */

	uint32_t              pp_counter;              /* __COUNTER__ value */

	uint8_t               pp_pack;
	uint8_t               pp_pack_stack_pos;
	uint8_t               pp_pack_stack[PACK_STACK_SIZE];

	yy_sym                pp_include_ifndef_macro; /* macro that protects the current include */
	ir_hashtab           *pp_include_hash;         /* map include file-name -> protection macro */

	int                   pp_last_search_dir;
	int                   pp_next_search_dir;

	uint32_t              pp_recursion_level;      /* used for debug only */

	int                   pp_include_paths_count;
	const char           *pp_include_paths[PP_MAX_INCLUDE_PATHS + 1];

	yy_sym                pp_out_file_name;
	uint32_t              pp_out_level;
	int32_t               pp_out_line;
	FILE                 *pp_out_file;

	/* Stndard headers */
	c_stdinc_t            c_stdinc[STDINC_SIZE];

	/* C -> IR Translator */
	ir_arena             *c_arena;                 /* Arena to keeps C types, symbols, tags, etc */
	ir_arena             *c_func_arena;            /* Arena to keeps C labels */
	c_scope              *active_scope;            /* The current inner-most C declaration scope */
	c_loop               *active_loop;             /* The current inner-most C loop or switch statement */
	c_scope              *active_func_scope;       /* The scope of the current function */
	c_sym                *active_func;             /* C symbol of the current function */
	c_name                active_func_name;        /* The current function name (id) */

	uint32_t              c_static_var_num;        /* Number of static variables in the current compiled file */
	uint32_t              c_static_str_num;        /* Number of unique C strings in the current compiled file */

	bool                  c_dead_code;
	bool                  c_static_data;
	uint64_t              c_fixed_regset;          /* Set of CPU registers excluded from regular register-allocation */
	const c_type         *c_last_call_func_type;   /* Type of the last called funtion (used for tail-call) */

	ir_ctx               *active_ctx;
	ir_ctx               *global_ctx;
	ir_ref                c_prologue_end;
	ir_ref                c_computed_goto;
	ir_ref                c_computed_goto_targets;
	uint32_t              c_label_num;             /* Number of generated IR_LABEL instructions (label as value) */
	ir_ref                c_last_expect_ref;
	bool                  c_last_expect_val;

	ir_strtab             c_strtab;                /* Hash to keep a single data object for the identical C strings */

	/* Error Reporting */
	uint32_t              e_errors;                /* bitset of error kinds that are treated as fatal */
	uint32_t              e_warnings;              /* bitset of error kinds that are treated as warnings */

	/* Linker */
	rcc_loader            c_linker;
	ir_arena             *c_linker_arena;
	uint32_t              constructors_count;
	uint32_t              destructors_count;
	c_name               *constructors;
	c_name               *destructors;

	/* Main Compiler Driver */
	uint32_t              c_flags;                 /* compiler actions (see C_RUN, C_DUMP_* in rcc.c) */
	uint32_t              c_opt_flags;             /* optimization level and flags (see C_OPT_* above) */

	uint32_t              ir_flags;                /* IR context flags (see IR_* defines in ir.h) */
	uint32_t              ir_mflags;               /* CPU specific flags (see IR_X86_... in ir.h) */
	uint32_t              ir_mflags_disabled;      /* CPU specific flags (see IR_X86_... in ir.h) */
	uint64_t              ir_debug_regset;
	uint32_t              ir_save_flags;           /* modificators for IR dumps (see IR_SAVE_* in ir.h) */
	ir_code_buffer        code_buffer;             /* pre-allocated JIT code area */
	bool                  protected;               /* code_buffer is write-proteted */
	ir_list               codegen_queue;           /* delayed code-generation queue */
	FILE                 *output;                  /* the main output file */

	struct {
		uint32_t          yy_num_syms;             /* number of symbols in "yy_hash" */
		void             *c_arena_checkpoint;      /* checkpoint to restore "c_arena" state */
	} reset_state;                                 /* used to restore RCC state between compilation of few files */
};

void rcc_parse_options(rcc_ctx *ctx, const char *str, size_t len);

/* Standard include files */
void c_stdinc_init(rcc_ctx *rcc);
void c_stdinc_builtin(rcc_ctx *rcc);
const char *c_stdinc_find(rcc_ctx *rcc, yy_sym name, size_t *len);

/* inline helpers */
IR_ALWAYS_INLINE const char *yy_sym2str(rcc_ctx *rcc, yy_sym sym)
{
	return rcc->yy_hash.data[sym].str;
}

IR_ALWAYS_INLINE const char *yy_sym2strl(rcc_ctx *rcc, yy_sym sym, size_t *len)
{
	*len = rcc->yy_hash.data[sym].len;
	return rcc->yy_hash.data[sym].str;
}

IR_ALWAYS_INLINE yy_sym *pp_save_ptr(yy_sym *tokens, const void *ptr)
{
#if __SIZEOF_POINTER__ == 4
	*tokens++ = (intptr_t)ptr;
#else
	*tokens++ = (int32_t)(((uintptr_t)ptr >> 32) & 0xffffffff);
	*tokens++ = (int32_t)((uintptr_t)ptr & 0xffffffff);
#endif
	return tokens;
}

IR_ALWAYS_INLINE yy_sym *pp_save_str(yy_sym *tokens, const char *text, size_t len)
{
	IR_ASSERT(len < 0x7fffffff);
	*tokens++ = (int32_t)len;
	return pp_save_ptr(tokens, text);
}

IR_ALWAYS_INLINE yy_sym *pp_save_val(rcc_ctx *rcc, yy_sym *tokens)
{
	IR_ASSERT(rcc->yy_len < 0x7fffffff);
	*tokens++ = (int32_t)rcc->yy_len;
	return pp_save_ptr(tokens, rcc->yy_text);
}

IR_ALWAYS_INLINE yy_sym *pp_load_ptr(yy_sym *tokens, void **ptr)
{
#if __SIZEOF_POINTER__ == 4
	*ptr = (void*)(uintptr_t)*tokens++;
#else
	uintptr_t val = (uintptr_t)(uint32_t)*tokens++ << 32;
	val |= (uintptr_t)(uint32_t)*tokens++;
	*ptr = (void*)val;
#endif
	return tokens;
}

IR_ALWAYS_INLINE yy_sym *pp_load_str(yy_sym *tokens, const char **text, size_t *len)
{
	*len = *tokens++;
	return pp_load_ptr(tokens, (void**)text);
}

IR_ALWAYS_INLINE yy_sym *pp_load_val(rcc_ctx *rcc, yy_sym *tokens)
{
	rcc->yy_len = *tokens++;
	return pp_load_ptr(tokens, (void**)&rcc->yy_text);
}

IR_ALWAYS_INLINE void pp_list_init(rcc_ctx *rcc, pp_list *l)
{
	if (rcc->pp_list_cache_idx != 0) {
		rcc->pp_list_cache_idx--;
		l->syms = rcc->pp_list_cache[rcc->pp_list_cache_idx].syms;
		l->size = rcc->pp_list_cache[rcc->pp_list_cache_idx].size;
		l->len = 0;
	} else {
		l->size = 64; /* default initial size */
		l->len = 0;
		l->syms = ir_mem_malloc(l->size * sizeof(yy_sym));
	}
}

IR_ALWAYS_INLINE void pp_list_release(rcc_ctx *rcc, yy_sym *syms, uint32_t size)
{
	if (rcc->pp_list_cache_idx < PP_LIST_CACHE_SIZE) {
		rcc->pp_list_cache[rcc->pp_list_cache_idx].syms = syms;
		rcc->pp_list_cache[rcc->pp_list_cache_idx].size = size;
		rcc->pp_list_cache_idx++;
	} else {
		ir_mem_free(syms);
	}
}

IR_ALWAYS_INLINE void pp_list_push(pp_list *l, yy_sym sym)
{
	uint32_t len = l->len++;

	if (len >= l->size) {
		pp_list_grow(l, len + 1);
	}
	l->syms[len] = sym;
}

IR_ALWAYS_INLINE void pp_list_push_ptr(pp_list *l, void *ptr)
{
	uint32_t len = l->len + sizeof(void*)/sizeof(int32_t);

	if (len > l->size) {
		pp_list_grow(l, len);
	}
	pp_save_ptr(l->syms + l->len, ptr);
	l->len += sizeof(void*)/sizeof(int32_t);
}

IR_ALWAYS_INLINE void pp_list_push_val(rcc_ctx *rcc, pp_list *l)
{
	IR_ASSERT(rcc->yy_len < 0x7fffffff);
	uint32_t len = l->len + sizeof(void*)/sizeof(int32_t) + 1;

	if (len > l->size) {
		pp_list_grow(l, len);
	}
	pp_save_val(rcc, l->syms + l->len);
	l->len += sizeof(void*)/sizeof(int32_t) + 1;
}

IR_ALWAYS_INLINE yy_sym *pp_list_push_val_from(pp_list *l, yy_sym *tokens)
{
	const char *_text;
	size_t _len;
	uint32_t len = l->len + sizeof(void*)/sizeof(int32_t) + 1;

	if (len > l->size) {
		pp_list_grow(l, len);
	}
	tokens = pp_load_str(tokens, &_text, &_len);
	pp_save_str(l->syms + l->len, _text, _len);
	l->len += sizeof(void*)/sizeof(int32_t) + 1;
	return tokens;
}

IR_ALWAYS_INLINE void c_value_set_rval(c_value *res, const c_type *type, ir_type t, ir_ref ref)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_REF, t);
	res->u.ref = ref;
}

IR_ALWAYS_INLINE void c_value_set_lval(c_value *res, const c_type *type, ir_type t, ir_ref ref)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_REF | C_VAL_LVAL, t);
	res->u.ref = ref;
}

IR_ALWAYS_INLINE void c_value_set_var(c_value *res, const c_type *type, ir_type t, ir_ref ref)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_REF | C_VAL_LVAL | C_VAL_VAR, t);
	res->u.ref = ref;
}

IR_ALWAYS_INLINE void c_value_set_reg(c_value *res, const c_type *type, ir_type t, int8_t reg)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_REF | C_VAL_LVAL | C_VAL_REG, t);
	res->u.ref = reg;
}

IR_ALWAYS_INLINE void c_value_set_const(c_value *res, const c_type *type, ir_type t, ir_val val)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_CONST, t);
	res->u.val = val;
}

IR_ALWAYS_INLINE void c_value_set_const_str(c_value *res, const c_type *type, ir_type t, const void *str, size_t size)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_CONST | C_VAL_STR, t);
	res->u.val.ptr = (void*)str;
	IR_ASSERT(size <= 0x7fffffff);
	res->u.ref = (ir_ref)size; /* string size (including terminating zero) */
}

IR_ALWAYS_INLINE bool c_value_is_const(c_value *v)
{
	return (v->u.op & C_VAL_CONST);
}

IR_ALWAYS_INLINE bool c_value_is_ref(c_value *v)
{
	return (v->u.op & C_VAL_REF) != 0;
}

IR_ALWAYS_INLINE bool c_value_is_lval(c_value *v)
{
	return (v->u.op & C_VAL_LVAL) != 0;
}

IR_ALWAYS_INLINE bool c_value_is_var(c_value *v)
{
	return (v->u.op & C_VAL_VAR) != 0;
}

IR_ALWAYS_INLINE bool c_value_is_reg(c_value *v)
{
	return (v->u.op & C_VAL_REG) != 0;
}

IR_ALWAYS_INLINE bool c_value_is_set(c_value *v)
{
	return (v->u.op & (C_VAL_REF | C_VAL_CONST)) != 0;
}

IR_ALWAYS_INLINE bool c_value_is_true(c_value *v)
{
	IR_ASSERT(c_value_is_const(v));
	return v->u.val.u64 != 0;
}

IR_ALWAYS_INLINE void c_value_clear(c_value *res)
{
	res->u.optx = 0;
}

IR_ALWAYS_INLINE bool c_value_is_const_str(c_value *v)
{
	return (v->u.op & C_VAL_STR) != 0;
}

IR_ALWAYS_INLINE const void *c_value_str_addr(c_value *v)
{
	return v->u.val.ptr;
}

IR_ALWAYS_INLINE size_t c_value_str_size(c_value *v)
{
	return v->u.ref; /* string size (including terminating zero) */
}

IR_ALWAYS_INLINE uint32_t c_align2attr(size_t align)
{
	if (align == 0) return 0;
	return ir_ntzl(align) + 1;
}

IR_ALWAYS_INLINE size_t c_attr2align(uint32_t attr)
{
	if ((attr & C_ATTR_ALIGN_MASK) == 0) return 0;
	return 1ULL << ((attr & C_ATTR_ALIGN_MASK) - 1);
}
#endif /* RCC_H */
