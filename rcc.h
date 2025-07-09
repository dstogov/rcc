/*
 * RCC - Rational C Compiler
 * (Common data structures)
 * Copyright (C) 2025 Dmitry Stogov <dmitrystogov@gmail.com>
 */
#ifndef RCC_H
#define RCC_H

#ifndef __has_attribute
# define __has_attribute(x) 0
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
	_("&",                             YY__AND)                        \
	_("*",                             YY__STAR)                       \
	_("+",                             YY__PLUS)                       \
	_("-",                             YY__MINUS)                      \
	_("~",                             YY__TILDE)                      \
	_("!",                             YY__BANG)                       \
	_("/",                             YY__SLASH)                      \
	_("%",                             YY__PERCENT)                    \
	_("<<",                            YY__LESS_LESS)                  \
	_(">>",                            YY__GREATER_GREATER)            \
	_("<",                             YY__LESS)                       \
	_(">",                             YY__GREATER)                    \
	_("<=",                            YY__LESS_EQUAL)                 \
	_(">=",                            YY__GREATER_EQUAL)              \
	_("==",                            YY__EQUAL_EQUAL)                \
	_("!=",                            YY__BANG_EQUAL)                 \
	_("^",                             YY__UPARROW)                    \
	_("|",                             YY__BAR)                        \
	_("&&",                            YY__AND_AND)                    \
	_("||",                            YY__BAR_BAR)                    \
	_("?",                             YY__QUERY)                      \
	_(":",                             YY__COLON)                      \
	_(";",                             YY__SEMICOLON)                  \
	_("...",                           YY__POINT_POINT_POINT)          \
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
	_("#",                             YY__HASH)                       \
	_("##",                            YY__HASH_HASH)                  \
	_("##",                            YY_PP_JOIN)                     \
	_("<PLACE>",                       YY_PP_PLACE_MARKER)             \

// TODO: Trigraphs are not supported yet ???
#define _YY_TRIGRAPHS(_) \
	_("<:",                            YY__LBRACK_SPEC)                \
	_(":>",                            YY__RBRACK_SPEC)                \
	_("<%:",                           YY__LBRACE_SPEC)                \
	_("%>",                            YY__RBRACE_SPEC)                \
	_("%:",                            YY__HASH_SPEC)                  \
	_("%:%:",                          YY__HASH_HASH_SPEC)             \

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
	_("__alignof",                     YY___ALIGNOF)                   \
	_("__alignof__",                   YY___ALIGNOF__)                 \
	_("__asm",                         YY___ASM)                       \
	_("__asm__",                       YY___ASM__)                     \
	_("__attribute",                   YY___ATTRIBUTE)                 \
	_("__attribute__",                 YY___ATTRIBUTE__)               \
	_("__const",                       YY___CONST)                     \
	_("__const__",                     YY___CONST__)                   \
	_("__complex",                     YY___COMPLEX)                   \
	_("__complex__",                   YY___COMPLEX__)                 \
	_("__extension__",                 YY___EXTENSION__)               \
	_("__inline",                      YY___INLINE)                    \
	_("__inline__",                    YY___INLINE__)                  \
	_("__label__",                     YY___LABEL__)                   \
	_("__restrict",                    YY___RESTRICT)                  \
	_("__restrict__",                  YY___RESTRICT__)                \
	_("__volatile",                    YY___VOLATILE)                  \
	_("__volatile__",                  YY___VOLATILE__)                \
	_("__builtin_va_start",            YY___BUILTIN_VA_START)          \
	_("__builtin_va_arg",              YY___BUILTIN_VA_ARG)            \
	_("__builtin_va_end",              YY___BUILTIN_VA_END)            \
	_("__builtin_va_copy",             YY___BUILTIN_VA_COPY)           \

#define YY_LAST_KEYWORD                YY___BUILTIN_VA_COPY

#define _YY_DIRECTIVES(_) \
	_("define",                        YY_DEFINE)                      \
	_("include",                       YY_INCLUDE)                     \
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

#define _YY_NAMES(_) \
	_("__COUNTER__",                   YY___COUNTER__)                 \
	_("__DATE__",                      YY___DATE__)                    \
	_("__FILE__",                      YY___FILE__)                    \
	_("__FUNCTION__",                  YY___FUNCTION__)                \
	_("__func__",                      YY___FUNC__)                    \
	_("__LINE__",                      YY___LINE__)                    \
	_("__TIME__",                      YY___TIME__)                    \
	_("__VA_ARGS__",                   YY___VA_ARGS__)                 \
	_("E",                             YY_E)                           \
	_("memcpy",                        YY_MEMCPY)                      \
	_("memset",                        YY_MEMSET)                      \
	_("main",                          YY_MAIN)                        \
	/* GCC attributes */                                               \
	_("alias",                         YY_ALIAS)                /*f  */\
	_("__alias__",                     YY___ALIAS__)            /*f  */\
	_("aligned",                       YY_ALIGNED)              /* vt*/\
	_("__aligned__",                   YY___ALIGNED__)          /* vt*/\
	_("always_inline",                 YY_ALWAYS_INLINE)        /*f  */\
	_("__always_inline__",             YY___ALWAYS_INLINE__)    /*f  */\
	_("cdecl",                         YY_CDECL)                /*f  */\
	_("__cdecl__",                     YY___CDECL__)            /*f  */\
	_("cleanup",                       YY_CLEANUP)              /* v */\
	_("cold",                          YY_COLD)                 /*f  */\
	_("__cold__",                      YY___COLD__)             /*f  */\
	_("__cleanup__",                   YY___CLEANUP__)          /* v */\
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
	_("gcc_struct",                    YY_GCC_STRUCT)           /*  t*/\
	_("__gcc_struct__",                YY___GCC_STRUCT__)       /*  t*/\
	_("hot",                           YY_HOT)                  /*f  */\
	_("__hot__",                       YY___HOT__)              /*f  */\
	_("leaf",                          YY_LEAF)                 /*f  */\
	_("__leaf__",                      YY___LEAF__)             /*f  */\
	_("may_alias",                     YY_MAY_ALIAS)            /*  t*/\
	_("__may_alias__",                 YY___MAY_ALIAS__)        /*  t*/\
	_("mode",                          YY_MODE)                 /* v */\
	_("__mode__",                      YY___MODE__)             /* v */\
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
	_("pure",                          YY_PURE)                 /*f  */\
	_("__pure__",                      YY___PURE__)             /*f  */\
	_("regparam",                      YY_REGPARAM)             /*f  */\
	_("__regparam__",                  YY___REGPARAM__)         /*f  */\
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
	_("weak",                          YY_WEAK)                 /*fv */\
	_("__weak__",                      YY___WEAK__)             /*fv */\
	_("weak_ref",                      YY_WEAK_REF)             /*f  */\
	_("__weak_ref__",                  YY___WEAK_REF__)         /*f  */\
	/* builtin functions */                                            \
	_("abs",                           YY_ABS)                         \
	_("labs",                          YY_LABS)                        \
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
YY_BUILTIN_FIRST = YY_ABS,
YY_BUILTIN_LAST = YY_TRUNCF,
YY_LAST = 0x7fffffff,
#undef _YY_SYM
} yy_sym;

/* yy_flags bits */
#define YY_PREPROCESS        (1<<0)
#define YY_SKIP_WS           (1<<1)
#define YY_SKIP_EOL          (1<<2)
#define YY_SKIP_COMMENTS     (1<<3)
#define YY_ACCEPT_PP_NUMBER  (1<<4)
#define YY_ACCEPT_PUNCTUATOR (1<<5)
#define YY_NO_MACRO          (1<<6)
#define YY_ACCEPT_NOSUBST    (1<<7)

#define PP_NO_LINEMARKERS    (1<<8)
#define PP_NO_OUTPUT         (1<<9)
#define PP_DUMP_MACROS       (1<<10)
#define PP_DUMP_MACRO_NAMES  (1<<11)
#define PP_DUMP_INCLUDES     (1<<12)
#define PP_EVAL_EXPRESSION   (1<<13)

#define YY_NO_WARNINGS       (1<<16)
#define YY_LANG_GNU          (1<<17)

#define YY_FLAGS_DEFAULT     (YY_PREPROCESS|YY_SKIP_WS|YY_SKIP_EOL|YY_SKIP_COMMENTS)
#define YY_FLAGS_PP_DEFAULT  (YY_PREPROCESS|YY_SKIP_COMMENTS|YY_ACCEPT_PP_NUMBER|YY_ACCEPT_PUNCTUATOR)

/* C scanner */
yy_sym yy_next(void);

/* C symbol table */
typedef struct _pp_macro pp_macro;
typedef struct _c_sym c_sym;
typedef struct _c_tag c_tag;
typedef struct _c_label c_label;

typedef struct _yy_hash_bucket {
	uint32_t                 h;      /* hash value */
	uint32_t                 next;   /* index of next bucket for hash conflict resolution */
	size_t                   len;    /* string length */
	const char              *str;
	pp_macro                *macro;
	c_sym                   *sym;
	c_tag                   *tag;
	c_label                 *label;
} yy_hash_bucket;

typedef struct {
	yy_hash_bucket          *data;
	uint32_t                 count;
	uint32_t                 size;
	uint32_t                 mask;
} yy_hashtab;

void yy_hash_init(void);
void yy_hash_free(void);
yy_sym yy_hash_find(const char *str, size_t len);
yy_sym yy_hash_lookup(const char *str, size_t len);

/* Dynamic Strings */
typedef struct {
	char   *str;
	size_t  len;
} yy_dyn_str;

void yy_dyn_str_init(yy_dyn_str *dyn_str, const char *str, size_t len);
void yy_dyn_str_init0(yy_dyn_str *dyn_str, const char *str, size_t len);
void yy_dyn_str_append(yy_dyn_str *dyn_str, const char *str, size_t len);
void yy_dyn_str_append0(yy_dyn_str *dyn_str, const char *str, size_t len);

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

struct _pp_macro {
	uint32_t                 flags;
	int32_t                  num_args;
	uint32_t                 size;
	yy_sym                  *tokens;
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

void pp_macro_define(yy_sym name, uint32_t flags, uint32_t num_args, yy_sym *tokens);
bool pp_macro_expand(pp_macro *macro, yy_sym sym);
void pp_parse_directive(void);
void pp_pop_include(void);
void pp_start(void);
void pp_dtor(void);
void pp_preprocess(FILE *f);
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
#define C_ATTR_VLA               (1<<13)

/* struct type attributes */
#define C_ATTR_MS_STRUCT         (1<<14)
#define C_ATTR_GCC_STRUCT        (1<<15)

/* function attributes */
#define C_ATTR_VARIADIC          (1<<16)
#define C_ATTR_INLINE            (1<<17)
#define C_ATTR_NORETURN          (1<<18)
#define C_ATTR_ALWAYS_INLINE     (1<<19)
#define C_ATTR_NOINLINE          (1<<20)
#define C_ATTR_NOTHROW           (1<<21)
#define C_ATTR_LEAF              (1<<22)
#define C_ATTR_PURE              (1<<23)
#define C_ATTR_HOT               (1<<24)
#define C_ATTR_COLD              (1<<25)
#define C_ATTR_DEPRECATED        (1<<26)
#define C_ATTR_CDECL             (1<<27)
#define C_ATTR_FASTCALL          (1<<28)

/* statement attributes */
#define C_ATTR_FALLTHROUGH       (1<<29)
#define C_ATTR_MUSTTAIL          (1<<30)
//#define C_ATTR_ASSUME            (1<<31)

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
		|C_ATTR_LEAF|C_ATTR_PURE|C_ATTR_HOT|C_ATTR_COLD|C_ATTR_DEPRECATED|C_ATTR_CDECL|C_ATTR_FASTCALL)

#define C_POINTER_ATTRS \
	(C_ATTR_CONST|C_ATTR_RESTRICT)

typedef enum {
	C_TYPE_VOID,
	C_TYPE_BOOL,
	C_TYPE_U8,
	C_TYPE_U16,
	C_TYPE_U32,
	C_TYPE_U64,
//	C_TYPE_ADDR,
	C_TYPE_CHAR,
	C_TYPE_I8,
	C_TYPE_I16,
	C_TYPE_I32,
	C_TYPE_I64,
	C_TYPE_FLOAT,
	C_TYPE_DOUBLE,
	C_TYPE_LONG_DOUBLE,
	C_TYPE_ENUM,
	C_TYPE_POINTER,
	C_TYPE_FUNC,
	C_TYPE_ARRAY,
	C_TYPE_STRUCT,
	C_TYPE_UNION,
	C_TYPE_FLOAT_COMPLEX,
	C_TYPE_DOUBLE_COMPLEX,
	C_TYPE_LONG_DOUBLE_COMPLEX,
} c_type_kind;

#define C_IS_TYPE_KIND_SCALAR(t)        ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_LONG_DOUBLE)
#define C_IS_TYPE_KIND_INT(t)           ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_I64)
#define C_IS_TYPE_KIND_UNSIGNED(t)      ((t) >= C_TYPE_BOOL && (t) <= C_TYPE_U64)
#define C_IS_TYPE_KIND_SIGNED(t)        ((t) >= C_TYPE_CHAR && (t) <= C_TYPE_I64)
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
			int32_t        length;
		} array;
		struct {
			const c_type  *type;
		} pointer;
		struct {
			c_name         tag;
			int32_t        num_fields;
			c_field       *fields;
		} record;
		struct {
			int32_t        num_params;
//			ffi_abi        abi;
			const c_type  *ret_type;
			c_param       *params;
		} func;
		c_name             tag;
	};
};

#define C_VAL_REF      (1<<0)
#define C_VAL_CONST    (1<<1)
#define C_VAL_LVAL     (1<<2)
#define C_VAL_VAR      (1<<3)
#define C_VAL_BUILTIN  (1<<4)
#define C_VAL_INLINE   (1<<5)

typedef struct {
	const c_type  *type;
	ir_insn        u;    /* u.op keeps C_VAL_* flags, u.op1 - ref, u.proto - bits */
} c_value;

struct _c_dcl {
	uint32_t               flags;
	uint32_t               attr;
	const c_type          *type;
};

typedef enum {
	C_SYM_TYPE,
	C_SYM_FUNC,
	C_SYM_CONST,
	C_SYM_VAR,
} c_sym_kind;

typedef enum {
	C_LINK_NONE,
	C_LINK_EXTERNAL,
	C_LINK_INTERNAL,
	C_LINK_BUILTIN,
} c_sym_linkage;

struct _c_sym {
	c_sym_kind             kind: 2;
	c_sym_linkage          linkage: 2;         /* only for C_SYM_VAR and C_SYM_FUNC */
	bool                   is_thread_local: 1; /* only for C_SYM_VAR */
	bool                   is_implemented: 1;  /* only for C_SYM_VAR and C_SYM_FUNC */
	c_scope               *scope;
	c_value                value;              /* type is part of the value */
	ir_ctx                *ctx;                /* function IR (used for function inlining) */
};

struct _c_tag {
	const c_type          *type;
	c_scope               *scope;
};

#define C_IS_BIT_FIELD(bit_field)     ((bit_field) != 0)
#define C_BIT_FIELD(start, lenght)    ((1 << 12) | ((start) << 6) | (lenght))
#define C_BIT_FIELD_START(bit_field)  (((bit_field) >> 6) & 0x3f)
#define C_BIT_FIELD_SIZE(bit_field)   ((bit_field) & 0x3f)

struct _c_field {
	c_name                 name;
	uint16_t               bit_field; /* 1-bit - is bit-field, 6-bits - first bit, 6-bits - bit lenght */
	size_t                 offset;
	const c_type          *type;
};

#define C_ALLOCA_PARAMS 16
#define C_ALLOCA_FIELDS 16

struct _c_param {
	c_name                 name;
	const c_type          *type;
};

struct _c_scope {
	pp_list   list;
	void     *checkpoint;
	c_scope  *prev;
};

typedef struct _c_case_labels c_case_labels;

struct _c_loop {
	bool      is_switch;
	uint8_t   switch_type; /* ir_type */
	ir_ref    start;
	ir_ref    check;
	ir_ref    next;
	ir_ref    break_list;
	ir_ref    continue_list;
	c_loop   *prev;
	c_case_labels *case_labels;
};

struct _c_label {
	bool      is_local;
	ir_ref    dst;
	ir_ref    src_list;
	c_scope  *scope;
};

#define C_INIT_STACK_SIZE 32

typedef struct {
	size_t             offset;
	uint32_t           level;
	struct {
		const c_type  *type;
		int64_t        pos;
	} stack[C_INIT_STACK_SIZE];
} c_init;

extern const c_type c_type_void;
extern const c_type c_type_bool;
extern const c_type c_type_char;
extern const c_type c_type_u8;
extern const c_type c_type_i8;
extern const c_type c_type_u16;
extern const c_type c_type_i16;
extern const c_type c_type_u32;
extern const c_type c_type_i32;
extern const c_type c_type_u64;
extern const c_type c_type_i64;
extern const c_type c_type_float;
extern const c_type c_type_double;
extern const c_type c_type_long_double;
extern const c_type c_type_float_complex;
extern const c_type c_type_double_complex;
extern const c_type c_type_long_double_complex;
extern const c_type c_type_string;

#if __SIZEOF_SIZE_T__ == 8
# define c_type_size_t  c_type_u64
# define c_type_ssize_t c_type_i64
#else
# define c_type_size_t  c_type_u32
# define c_type_ssize_t c_type_i32
#endif

#define C_POP_MASK   0x3
#define C_POP_SYM    0x0
#define C_POP_TAG    0x1
#define C_POP_LABEL  0x2

void c_push_scope(c_scope *scope);
void c_pop_scope(c_scope *scope);

const c_type *c_resolve_type(c_dcl *dcl);
const c_type *c_resolve_type_name(c_name name);
void c_resolve_sym_name(c_value *res, c_name name, yy_sym sym);
c_type *c_resolve_tag(c_name name, c_dcl *dcl, bool define);
c_type *c_make_struct_type(c_dcl *dcl, c_name tag);
c_type *c_make_enum_type(c_dcl *dcl, c_name tag);
void c_make_pointer_type(c_dcl *dcl);
void c_make_array_type(c_dcl *dcl, c_dcl *dim, c_value *len, uint64_t attr);
void c_make_func_type(c_dcl *dcl, c_param *params, int32_t num_params, bool is_variadic);
void c_make_nested_type(c_dcl *dcl, c_dcl *nested);
void c_finish_struct_type(c_type *type, c_dcl *d);
void c_finish_enum_type(c_type *dcl, c_dcl *d, int64_t min, uint64_t max);
void c_validate_func_params(c_name name, c_dcl *dcl);

c_sym *c_declare(c_name name, c_dcl *dcl);
void c_declare_struct_field(c_type *type, c_name name, c_dcl *field, c_value *bits);
void c_declare_enum_val(const c_type *type, c_name name, c_dcl *attr, c_value *val, int64_t *min, uint64_t *max, c_value *last);
void c_declare_func_param(c_param **params, int32_t *num_params, c_name name, c_dcl *param);
void c_declare_func_param_name(c_param **params, int32_t *num_params, c_name name);
void c_declare_func_param_type(const c_type *type, c_name name, c_dcl *param);
void c_declare_local_label(c_name name);
void c_empty_declaration(c_dcl *d);

void c_gcc_attribute(c_dcl *dcl, c_name name, c_value *v);

void c_sizeof_type(c_value *res, const c_type *type);
void c_sizeof_expr(c_value *res, c_value *expr, ir_ref old_control);
void c_alignof_type(c_value *res, const c_type *type);
void c_alignof_expr(c_value *res, c_value *expr, ir_ref old_control);
void c_alignas_expr(c_dcl *dcl, c_value *expr);
const c_type *c_typeof_expr(c_value *expr, ir_ref old_control);

void c_static_assert(c_value *expr, c_value *msg);

ir_type c_type2ir(const c_type *t);
c_sym *c_global_sym(c_sym *sym);
yy_sym c_get_current_func_name(void);

/* IR Code Generation */
void c_value_rval(c_value *val);

ir_ref c_do_nocode(void);
ir_ref c_do_alloca(size_t size, bool zero);
void c_do_cast(const c_type *t, c_value *v);
void c_do_post_op(yy_sym sym, c_value *v);
void c_do_pre_op(yy_sym sym, c_value *v);
void c_do_addr(c_value *v);
void c_do_deref(c_value *v);
void c_do_unary_plus(c_value *v);
void c_do_neg(c_value *v);
void c_do_not(c_value *v);
void c_do_bool_not(c_value *v);
void c_do_array_dim(c_value *v, c_value *dim);
void c_do_struct_field(c_value *v, c_name field);
void c_do_struct_field_deref(c_value *v, c_name field);
void c_do_builtin(c_value *val, c_name name, int32_t num_args, c_value *args);
void c_do_call(c_value *func, int32_t num_args, c_value *args);
void c_do_binary_op(yy_sym sym, c_value *v, c_value *op2);
void c_do_assign_op(yy_sym sym, c_value *v, c_value *op2);
ir_ref c_do_bool_and_start(c_value *v);
void c_do_bool_and_end(c_value *v, c_value *op2, ir_ref if_ref);
ir_ref c_do_bool_or_start(c_value *v);
void c_do_bool_or_end(c_value *v, c_value *op2, ir_ref if_ref);
void c_do_cond_op(c_value *v, c_value *op2, c_value *op3);

ir_ref c_do_if(c_value *cond);
void c_do_if_else(ir_ref _if, bool orig_dead_code);
void c_do_if_end(ir_ref _if, bool orig_dead_code);
void c_do_switch(c_loop *loop, c_value *cond);
void c_do_case(c_value *v);
void c_do_case_range(c_value *v1, c_value *v2);
void c_do_case_default(void);
void c_do_switch_end(c_loop *loop);
void c_do_loop_start(c_loop *loop);
void c_do_loop_check(c_loop *loop, c_value *cond);
void c_do_loop_continue_label(c_loop *loop);
void c_do_loop_end(c_loop *loop);
void c_do_for_next_start(c_loop *loop);
void c_do_for_next_end(c_loop *loop);
void c_do_for_end(c_loop *loop);
void c_do_continue(void);
void c_do_break(void);
void c_do_return(c_value *v);
void c_do_goto(c_name name);
c_label *c_do_set_label(c_name name);
void c_do_set_label_attrs(c_label *label, c_dcl *attrs);
void c_do_finish_label(c_name name, c_label *label);
void c_do_label_value(c_value *res, c_name label);
void c_do_computed_goto(c_value *v);

void c_do_init_obj(c_sym *obj, c_value *v);
void c_do_init_dim(c_sym *obj, c_init *init, c_value *dim);
void c_do_init_field(c_sym *obj, c_init *init, c_name field);
void c_do_init_first(c_sym *obj, c_init *init, const c_type *t, size_t offset);
void c_do_init_next(c_sym *obj, c_init *init);
void c_do_init_set(c_sym *obj, c_init *init, c_value *val, size_t *size);
const c_type *c_do_init_nested(c_sym *obj, c_init *init, bool b, size_t *offset_ptr);
void c_do_init_end(c_sym *obj, size_t size);

void c_do_init_expr_start(c_sym *obj, const c_type *t);
void c_do_init_expr_end(c_value *v, c_sym *obj, size_t size);

void c_do_func_start(c_name name, c_dcl *d, c_scope *scope, ir_ctx *ctx);
void c_do_func_end(c_name name, c_dcl *d, c_scope *scope, ir_ctx *ctx);

/* C Parser */
bool parse_pp_expr(void);
void rcc_parse(void);

/* Error Reporting */
void yy_error(const char *msg) yy_noreturn;
void yy_error_fmt(const char *fmt, ...) yy_noreturn;
void yy_warning(const char *msg);
void yy_warning_fmt(const char *fmt, ...);

/* Linker */
void *c_linker_allocate_data(size_t size);
void *c_linker_grow_data(void *addr, size_t size);
void* c_linker_resolve_sym_name(ir_loader *loader, const char *name, uint32_t flags);

/* IR compiler */
void rcc_ir_init(ir_ctx *ctx, uint32_t flags);
void rcc_ir_compile(c_name name, ir_ctx *ctx, c_sym *sym);

/* C compiler state */
extern const char           *yy_pos;            /* pointer to current scanned character          */
extern const char           *yy_text;           /* pointer to start of the current scanned token */
extern const char           *yy_linepos;        /* pointer to start of the current scanned line  */
extern size_t                yy_len;            /* length of the value of terminal token */
extern int32_t               yy_line;           /* line number */
extern yy_sym                yy_file_name;      /* interned file name */
extern const char           *yy_buf;
extern const char           *yy_end;

extern yy_hashtab            yy_hash;
extern ir_arena             *yy_arena;
extern uint32_t              yy_flags;

extern pp_list               pp_list_cache[PP_LIST_CACHE_SIZE];
extern uint32_t              pp_list_cache_idx;

extern pp_subst_stream       pp_subst_stack[PP_SUBST_STACK_SIZE];
extern uint32_t              pp_subst_level;

extern uint32_t              pp_include_level;        /* include nesting level */
extern uint32_t              pp_include_ifndef_state; /* state to catch includes protected by #ifndef macro */

extern ir_arena             *c_arena;
extern bool                  c_dead_code;
extern ir_ctx               *active_ctx;
extern ir_ctx               *global_ctx;

/* Standard include files */
void c_stdinc_init(void);
const char *c_stdinc_find(yy_sym name, size_t *len);

/* inline helpers */
IR_ALWAYS_INLINE const char *yy_sym2str(yy_sym sym)
{
	return yy_hash.data[sym].str;
}

IR_ALWAYS_INLINE const char *yy_sym2strl(yy_sym sym, size_t *len)
{
	*len = yy_hash.data[sym].len;
	return yy_hash.data[sym].str;
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

IR_ALWAYS_INLINE yy_sym *pp_save_val(yy_sym *tokens)
{
	IR_ASSERT(yy_len < 0x7fffffff);
	*tokens++ = (int32_t)yy_len;
	return pp_save_ptr(tokens, yy_text);
}

IR_ALWAYS_INLINE yy_sym *pp_load_ptr(yy_sym *tokens, void **ptr)
{
#if __SIZEOF_POINTER__ == 4
	*ptr = (void*)(uintptr_t)*tokens++;
#else
	uintptr_t val = (uintptr_t)*tokens++ << 32;
	val |= (uintptr_t)*tokens++;
	*ptr = (void*)val;
#endif
	return tokens;
}

IR_ALWAYS_INLINE yy_sym *pp_load_val(yy_sym *tokens)
{
	yy_len = *tokens++;
	return pp_load_ptr(tokens, (void**)&yy_text);
}

IR_ALWAYS_INLINE void pp_list_init(pp_list *l)
{
	if (pp_list_cache_idx != 0) {
		pp_list_cache_idx--;
		l->syms = pp_list_cache[pp_list_cache_idx].syms;
		l->size = pp_list_cache[pp_list_cache_idx].size;
		l->len = 0;
	} else {
		l->size = 64; /* default initial size */
		l->len = 0;
		l->syms = ir_mem_malloc(l->size * sizeof(yy_sym));
	}
}

IR_ALWAYS_INLINE void pp_list_release(yy_sym *syms, uint32_t size)
{
	if (pp_list_cache_idx < PP_LIST_CACHE_SIZE) {
		pp_list_cache[pp_list_cache_idx].syms = syms;
		pp_list_cache[pp_list_cache_idx].size = size;
		pp_list_cache_idx++;
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

IR_ALWAYS_INLINE void pp_list_push_val(pp_list *l)
{
	IR_ASSERT(yy_len < 0x7fffffff);
	uint32_t len = l->len + sizeof(void*)/sizeof(int32_t) + 1;

	if (len > l->size) {
		pp_list_grow(l, len);
	}
	pp_save_val(l->syms + l->len);
	l->len += sizeof(void*)/sizeof(int32_t) + 1;
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

IR_ALWAYS_INLINE void c_value_set_const(c_value *res, const c_type *type, ir_type t, ir_val val)
{
	res->type = type;
	res->u.optx = IR_OPT(C_VAL_CONST, t);
	res->u.val = val;
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
	res->type = &c_type_void;
	res->u.optx = 0;
}

#endif /* RCC_H */
