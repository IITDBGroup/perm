/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         plpgsql_yyparse
#define yylex           plpgsql_yylex
#define yyerror         plpgsql_yyerror
#define yylval          plpgsql_yylval
#define yychar          plpgsql_yychar
#define yydebug         plpgsql_yydebug
#define yynerrs         plpgsql_yynerrs


/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "gram.y"

/*-------------------------------------------------------------------------
 *
 * gram.y				- Parser for the PL/pgSQL
 *						  procedural language
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/pl/plpgsql/src/gram.y,v 1.108 2008/01/01 19:46:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "plpgsql.h"

#include "parser/parser.h"


static PLpgSQL_expr		*read_sql_construct(int until,
											int until2,
											const char *expected,
											const char *sqlstart,
											bool isexpression,
											bool valid_sql,
											int *endtoken);
static	PLpgSQL_expr	*read_sql_stmt(const char *sqlstart);
static	PLpgSQL_type	*read_datatype(int tok);
static	PLpgSQL_stmt	*make_execsql_stmt(const char *sqlstart, int lineno);
static	PLpgSQL_stmt_fetch *read_fetch_direction(void);
static	PLpgSQL_stmt	*make_return_stmt(int lineno);
static	PLpgSQL_stmt	*make_return_next_stmt(int lineno);
static	PLpgSQL_stmt	*make_return_query_stmt(int lineno);
static	void			 check_assignable(PLpgSQL_datum *datum);
static	void			 read_into_target(PLpgSQL_rec **rec, PLpgSQL_row **row,
										  bool *strict);
static	PLpgSQL_row		*read_into_scalar_list(const char *initial_name,
											   PLpgSQL_datum *initial_datum);
static PLpgSQL_row		*make_scalar_list1(const char *initial_name,
										   PLpgSQL_datum *initial_datum,
										   int lineno);
static	void			 check_sql_expr(const char *stmt);
static	void			 plpgsql_sql_error_callback(void *arg);
static	char			*check_label(const char *yytxt);
static	void			 check_labels(const char *start_label,
									  const char *end_label);



/* Line 268 of yacc.c  */
#line 131 "y.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     K_ALIAS = 258,
     K_ASSIGN = 259,
     K_BEGIN = 260,
     K_BY = 261,
     K_CLOSE = 262,
     K_CONSTANT = 263,
     K_CONTINUE = 264,
     K_CURSOR = 265,
     K_DEBUG = 266,
     K_DECLARE = 267,
     K_DEFAULT = 268,
     K_DIAGNOSTICS = 269,
     K_DOTDOT = 270,
     K_ELSE = 271,
     K_ELSIF = 272,
     K_END = 273,
     K_EXCEPTION = 274,
     K_EXECUTE = 275,
     K_EXIT = 276,
     K_FOR = 277,
     K_FETCH = 278,
     K_FROM = 279,
     K_GET = 280,
     K_IF = 281,
     K_IN = 282,
     K_INFO = 283,
     K_INSERT = 284,
     K_INTO = 285,
     K_IS = 286,
     K_LOG = 287,
     K_LOOP = 288,
     K_MOVE = 289,
     K_NOSCROLL = 290,
     K_NOT = 291,
     K_NOTICE = 292,
     K_NULL = 293,
     K_OPEN = 294,
     K_OR = 295,
     K_PERFORM = 296,
     K_ROW_COUNT = 297,
     K_RAISE = 298,
     K_RENAME = 299,
     K_RESULT_OID = 300,
     K_RETURN = 301,
     K_REVERSE = 302,
     K_SCROLL = 303,
     K_STRICT = 304,
     K_THEN = 305,
     K_TO = 306,
     K_TYPE = 307,
     K_WARNING = 308,
     K_WHEN = 309,
     K_WHILE = 310,
     T_FUNCTION = 311,
     T_TRIGGER = 312,
     T_STRING = 313,
     T_NUMBER = 314,
     T_SCALAR = 315,
     T_ROW = 316,
     T_RECORD = 317,
     T_DTYPE = 318,
     T_WORD = 319,
     T_ERROR = 320,
     O_OPTION = 321,
     O_DUMP = 322
   };
#endif
/* Tokens.  */
#define K_ALIAS 258
#define K_ASSIGN 259
#define K_BEGIN 260
#define K_BY 261
#define K_CLOSE 262
#define K_CONSTANT 263
#define K_CONTINUE 264
#define K_CURSOR 265
#define K_DEBUG 266
#define K_DECLARE 267
#define K_DEFAULT 268
#define K_DIAGNOSTICS 269
#define K_DOTDOT 270
#define K_ELSE 271
#define K_ELSIF 272
#define K_END 273
#define K_EXCEPTION 274
#define K_EXECUTE 275
#define K_EXIT 276
#define K_FOR 277
#define K_FETCH 278
#define K_FROM 279
#define K_GET 280
#define K_IF 281
#define K_IN 282
#define K_INFO 283
#define K_INSERT 284
#define K_INTO 285
#define K_IS 286
#define K_LOG 287
#define K_LOOP 288
#define K_MOVE 289
#define K_NOSCROLL 290
#define K_NOT 291
#define K_NOTICE 292
#define K_NULL 293
#define K_OPEN 294
#define K_OR 295
#define K_PERFORM 296
#define K_ROW_COUNT 297
#define K_RAISE 298
#define K_RENAME 299
#define K_RESULT_OID 300
#define K_RETURN 301
#define K_REVERSE 302
#define K_SCROLL 303
#define K_STRICT 304
#define K_THEN 305
#define K_TO 306
#define K_TYPE 307
#define K_WARNING 308
#define K_WHEN 309
#define K_WHILE 310
#define T_FUNCTION 311
#define T_TRIGGER 312
#define T_STRING 313
#define T_NUMBER 314
#define T_SCALAR 315
#define T_ROW 316
#define T_RECORD 317
#define T_DTYPE 318
#define T_WORD 319
#define T_ERROR 320
#define O_OPTION 321
#define O_DUMP 322




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 54 "gram.y"

		int32					ival;
		bool					boolean;
		char					*str;
		struct
		{
			char *name;
			int  lineno;
		}						varname;
		struct
		{
			char *name;
			int  lineno;
			PLpgSQL_datum   *scalar;
			PLpgSQL_rec     *rec;
			PLpgSQL_row     *row;
		}						forvariable;
		struct
		{
			char *label;
			int  n_initvars;
			int  *initvarnos;
		}						declhdr;
		struct
		{
			char *end_label;
			List *stmts;
		}						loop_body;
		List					*list;
		PLpgSQL_type			*dtype;
		PLpgSQL_datum			*scalar;	/* a VAR, RECFIELD, or TRIGARG */
		PLpgSQL_variable		*variable;	/* a VAR, REC, or ROW */
		PLpgSQL_var				*var;
		PLpgSQL_row				*row;
		PLpgSQL_rec				*rec;
		PLpgSQL_expr			*expr;
		PLpgSQL_stmt			*stmt;
		PLpgSQL_stmt_block		*program;
		PLpgSQL_condition		*condition;
		PLpgSQL_exception		*exception;
		PLpgSQL_exception_block	*exception_block;
		PLpgSQL_nsitem			*nsitem;
		PLpgSQL_diag_item		*diagitem;
		PLpgSQL_stmt_fetch		*fetch;



/* Line 293 of yacc.c  */
#line 349 "y.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 361 "y.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  9
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   335

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  77
/* YYNRULES -- Number of rules.  */
#define YYNRULES  144
/* YYNRULES -- Number of states.  */
#define YYNSTATES  241

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   322

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      71,    72,     2,     2,    73,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    68,
      69,     2,    70,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    74,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     8,    13,    14,    16,    19,    21,    24,
      25,    27,    35,    37,    40,    44,    46,    49,    51,    57,
      59,    61,    67,    73,    79,    80,    88,    89,    91,    93,
      94,    95,    99,   101,   105,   108,   110,   112,   114,   116,
     118,   119,   121,   122,   123,   126,   128,   130,   132,   134,
     135,   137,   140,   142,   145,   147,   149,   151,   153,   155,
     157,   159,   161,   163,   165,   167,   169,   171,   173,   175,
     177,   179,   181,   185,   190,   196,   200,   202,   206,   208,
     210,   212,   214,   216,   218,   222,   231,   232,   238,   241,
     246,   252,   257,   261,   263,   265,   267,   269,   274,   276,
     278,   281,   286,   288,   290,   292,   294,   296,   298,   300,
     306,   309,   311,   313,   317,   320,   324,   330,   336,   337,
     342,   345,   347,   348,   349,   354,   357,   359,   365,   369,
     371,   372,   373,   374,   375,   376,   382,   383,   385,   387,
     389,   391,   393,   396,   398
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      76,     0,    -1,    56,    77,    81,    80,    -1,    57,    77,
      81,    80,    -1,    -1,    78,    -1,    78,    79,    -1,    79,
      -1,    66,    67,    -1,    -1,    68,    -1,    82,     5,   151,
     102,   138,    18,   148,    -1,   147,    -1,   147,    83,    -1,
     147,    83,    84,    -1,    12,    -1,    84,    85,    -1,    85,
      -1,    69,    69,   150,    70,    70,    -1,    12,    -1,    86,
      -1,    95,    97,    98,    99,   100,    -1,    95,     3,    22,
      94,    68,    -1,    44,    96,    51,    96,    68,    -1,    -1,
      95,    88,    10,    87,    90,    93,    89,    -1,    -1,    35,
      -1,    48,    -1,    -1,    -1,    71,    91,    72,    -1,    92,
      -1,    91,    73,    92,    -1,    95,    98,    -1,    31,    -1,
      22,    -1,    64,    -1,    64,    -1,    64,    -1,    -1,     8,
      -1,    -1,    -1,    36,    38,    -1,    68,    -1,   101,    -1,
       4,    -1,    13,    -1,    -1,   103,    -1,   103,   104,    -1,
     104,    -1,    81,    68,    -1,   106,    -1,   113,    -1,   115,
      -1,   116,    -1,   117,    -1,   120,    -1,   122,    -1,   123,
      -1,   127,    -1,   129,    -1,   130,    -1,   105,    -1,   107,
      -1,   131,    -1,   132,    -1,   133,    -1,   135,    -1,   136,
      -1,    41,   151,   143,    -1,   112,   151,     4,   143,    -1,
      25,    14,   151,   108,    68,    -1,   108,    73,   109,    -1,
     109,    -1,   111,     4,   110,    -1,    42,    -1,    45,    -1,
      60,    -1,    60,    -1,    61,    -1,    62,    -1,   112,    74,
     144,    -1,    26,   151,   145,   102,   114,    18,    26,    68,
      -1,    -1,    17,   151,   145,   102,   114,    -1,    16,   102,
      -1,   147,    33,   151,   126,    -1,   147,    55,   151,   146,
     126,    -1,   147,    22,   118,   126,    -1,   151,   119,    27,
      -1,    60,    -1,    64,    -1,    62,    -1,    61,    -1,   121,
     151,   148,   149,    -1,    21,    -1,     9,    -1,    46,   151,
      -1,    43,   151,   125,   124,    -1,    58,    -1,    19,    -1,
      53,    -1,    37,    -1,    28,    -1,    32,    -1,    11,    -1,
     102,    18,    33,   148,    68,    -1,   128,   151,    -1,    64,
      -1,    65,    -1,    29,   151,    30,    -1,    20,   151,    -1,
      39,   151,   137,    -1,    23,   151,   134,   137,    30,    -1,
      34,   151,   134,   137,    68,    -1,    -1,     7,   151,   137,
      68,    -1,    38,    68,    -1,    60,    -1,    -1,    -1,    19,
     151,   139,   140,    -1,   140,   141,    -1,   141,    -1,    54,
     151,   142,    50,   102,    -1,   142,    40,   150,    -1,   150,
      -1,    -1,    -1,    -1,    -1,    -1,    69,    69,   150,    70,
      70,    -1,    -1,    64,    -1,    60,    -1,    62,    -1,    61,
      -1,    68,    -1,    54,   143,    -1,    64,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   226,   226,   230,   236,   237,   240,   241,   244,   250,
     251,   254,   276,   283,   290,   302,   311,   313,   317,   319,
     321,   325,   359,   364,   369,   368,   420,   423,   427,   434,
     446,   449,   478,   482,   488,   495,   496,   498,   527,   537,
     548,   549,   554,   565,   566,   570,   572,   580,   581,   585,
     588,   592,   599,   608,   610,   612,   614,   616,   618,   620,
     622,   624,   626,   628,   630,   632,   634,   636,   638,   640,
     642,   644,   648,   661,   675,   688,   692,   698,   710,   714,
     720,   728,   733,   738,   743,   758,   774,   777,   806,   812,
     829,   847,   886,  1087,  1102,  1123,  1131,  1141,  1156,  1160,
    1166,  1195,  1238,  1244,  1248,  1252,  1256,  1260,  1264,  1270,
    1277,  1284,  1286,  1290,  1304,  1337,  1467,  1489,  1502,  1507,
    1520,  1527,  1545,  1547,  1546,  1579,  1583,  1589,  1602,  1612,
    1619,  1623,  1627,  1631,  1635,  1639,  1650,  1653,  1657,  1661,
    1665,  1671,  1673,  1677,  1687
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "K_ALIAS", "K_ASSIGN", "K_BEGIN", "K_BY",
  "K_CLOSE", "K_CONSTANT", "K_CONTINUE", "K_CURSOR", "K_DEBUG",
  "K_DECLARE", "K_DEFAULT", "K_DIAGNOSTICS", "K_DOTDOT", "K_ELSE",
  "K_ELSIF", "K_END", "K_EXCEPTION", "K_EXECUTE", "K_EXIT", "K_FOR",
  "K_FETCH", "K_FROM", "K_GET", "K_IF", "K_IN", "K_INFO", "K_INSERT",
  "K_INTO", "K_IS", "K_LOG", "K_LOOP", "K_MOVE", "K_NOSCROLL", "K_NOT",
  "K_NOTICE", "K_NULL", "K_OPEN", "K_OR", "K_PERFORM", "K_ROW_COUNT",
  "K_RAISE", "K_RENAME", "K_RESULT_OID", "K_RETURN", "K_REVERSE",
  "K_SCROLL", "K_STRICT", "K_THEN", "K_TO", "K_TYPE", "K_WARNING",
  "K_WHEN", "K_WHILE", "T_FUNCTION", "T_TRIGGER", "T_STRING", "T_NUMBER",
  "T_SCALAR", "T_ROW", "T_RECORD", "T_DTYPE", "T_WORD", "T_ERROR",
  "O_OPTION", "O_DUMP", "';'", "'<'", "'>'", "'('", "')'", "','", "'['",
  "$accept", "pl_function", "comp_optsect", "comp_options", "comp_option",
  "opt_semi", "pl_block", "decl_sect", "decl_start", "decl_stmts",
  "decl_stmt", "decl_statement", "$@1", "opt_scrollable",
  "decl_cursor_query", "decl_cursor_args", "decl_cursor_arglist",
  "decl_cursor_arg", "decl_is_for", "decl_aliasitem", "decl_varname",
  "decl_renname", "decl_const", "decl_datatype", "decl_notnull",
  "decl_defval", "decl_defkey", "proc_sect", "proc_stmts", "proc_stmt",
  "stmt_perform", "stmt_assign", "stmt_getdiag", "getdiag_list",
  "getdiag_list_item", "getdiag_kind", "getdiag_target", "assign_var",
  "stmt_if", "stmt_else", "stmt_loop", "stmt_while", "stmt_for",
  "for_control", "for_variable", "stmt_exit", "exit_type", "stmt_return",
  "stmt_raise", "raise_msg", "raise_level", "loop_body", "stmt_execsql",
  "execsql_start", "stmt_execsql_insert", "stmt_dynexecute", "stmt_open",
  "stmt_fetch", "stmt_move", "opt_fetch_direction", "stmt_close",
  "stmt_null", "cursor_variable", "exception_sect", "@2",
  "proc_exceptions", "proc_exception", "proc_conditions",
  "expr_until_semi", "expr_until_rightbracket", "expr_until_then",
  "expr_until_loop", "opt_block_label", "opt_label", "opt_exitcond",
  "opt_lblname", "lno", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,    59,    60,
      62,    40,    41,    44,    91
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    76,    77,    77,    78,    78,    79,    80,
      80,    81,    82,    82,    82,    83,    84,    84,    85,    85,
      85,    86,    86,    86,    87,    86,    88,    88,    88,    89,
      90,    90,    91,    91,    92,    93,    93,    94,    95,    96,
      97,    97,    98,    99,    99,   100,   100,   101,   101,   102,
     102,   103,   103,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   105,   106,   107,   108,   108,   109,   110,   110,
     111,   112,   112,   112,   112,   113,   114,   114,   114,   115,
     116,   117,   118,   119,   119,   119,   119,   120,   121,   121,
     122,   123,   124,   125,   125,   125,   125,   125,   125,   126,
     127,   128,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   138,   140,   140,   141,   142,   142,
     143,   144,   145,   146,   147,   147,   148,   148,   148,   148,
     148,   149,   149,   150,   151
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     4,     4,     0,     1,     2,     1,     2,     0,
       1,     7,     1,     2,     3,     1,     2,     1,     5,     1,
       1,     5,     5,     5,     0,     7,     0,     1,     1,     0,
       0,     3,     1,     3,     2,     1,     1,     1,     1,     1,
       0,     1,     0,     0,     2,     1,     1,     1,     1,     0,
       1,     2,     1,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     3,     4,     5,     3,     1,     3,     1,     1,
       1,     1,     1,     1,     3,     8,     0,     5,     2,     4,
       5,     4,     3,     1,     1,     1,     1,     4,     1,     1,
       2,     4,     1,     1,     1,     1,     1,     1,     1,     5,
       2,     1,     1,     3,     2,     3,     5,     5,     0,     4,
       2,     1,     0,     0,     4,     2,     1,     5,     3,     1,
       0,     0,     0,     0,     0,     5,     0,     1,     1,     1,
       1,     1,     2,     1,     0
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     4,     4,     0,     0,   134,     5,     7,   134,     1,
       8,     0,     9,     0,    12,     6,     9,     0,    10,     2,
     144,    15,    13,     3,   143,     0,   134,    19,     0,    38,
       0,    14,    17,    20,    40,     0,   144,    99,   144,    98,
     144,     0,   144,   144,   144,     0,   144,   144,   144,   144,
      81,    82,    83,   111,   112,     0,   122,    50,    52,    65,
      54,    66,   144,    55,    56,    57,    58,    59,   144,    60,
      61,    62,   144,    63,    64,    67,    68,    69,    70,    71,
      12,    39,     0,     0,    16,     0,    41,    27,    28,     0,
      42,   135,     0,   114,   118,   144,   132,     0,   118,   120,
       0,   130,     0,   100,    53,   144,     0,    51,   131,     0,
     136,   110,   144,   144,   144,     0,     0,     0,    24,    43,
     121,     0,     0,     0,   134,   113,     0,   115,    72,   108,
     103,   106,   107,   105,   104,     0,   123,   136,    84,   130,
     138,   140,   139,   137,     0,   134,     0,   134,   133,     0,
       0,    37,     0,    30,     0,     0,   119,     0,    80,     0,
      76,     0,    86,     0,   102,   101,     0,    11,    73,   130,
     141,    97,     0,    91,    93,    96,    95,    94,     0,    89,
     134,    23,    18,    22,     0,     0,    44,    47,    48,    45,
      21,    46,   116,    74,     0,     0,   134,   144,     0,   117,
     144,   124,   126,   142,     0,    92,    90,     0,    32,    42,
      36,    35,    29,    75,    78,    79,    77,    88,   132,     0,
       0,   125,   136,    31,     0,    34,    25,   134,     0,     0,
     129,     0,    33,    86,    85,     0,   134,   109,    87,   128,
     127
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     3,     5,     6,     7,    19,    55,    13,    22,    31,
      32,    33,   153,    89,   226,   185,   207,   208,   212,   152,
      34,    82,    90,   119,   155,   190,   191,   172,    57,    58,
      59,    60,    61,   159,   160,   216,   161,    62,    63,   198,
      64,    65,    66,   145,   178,    67,    68,    69,    70,   165,
     135,   173,    71,    72,    73,    74,    75,    76,    77,   122,
      78,    79,   121,   106,   166,   201,   202,   229,   128,   138,
     124,   180,    80,   144,   171,    25,    26
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -179
static const yytype_int16 yypact[] =
{
      37,   -27,   -27,    53,    23,    39,   -27,  -179,    39,  -179,
    -179,    40,     5,   108,   100,  -179,     5,    51,  -179,  -179,
    -179,  -179,    22,  -179,  -179,    47,   166,  -179,    54,  -179,
      50,    22,  -179,  -179,    13,    52,  -179,  -179,  -179,  -179,
    -179,   106,  -179,  -179,  -179,    61,  -179,  -179,  -179,  -179,
    -179,  -179,  -179,  -179,  -179,    63,   102,    42,  -179,  -179,
    -179,  -179,    60,  -179,  -179,  -179,  -179,  -179,  -179,  -179,
    -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,
       3,  -179,    84,    51,  -179,   115,  -179,  -179,  -179,   128,
    -179,  -179,    80,  -179,  -179,  -179,  -179,   112,  -179,  -179,
      80,  -179,    73,  -179,  -179,  -179,   125,  -179,  -179,   140,
     -19,  -179,  -179,  -179,  -179,    54,    77,    85,  -179,   116,
    -179,    83,    80,    95,   107,  -179,    80,  -179,  -179,  -179,
    -179,  -179,  -179,  -179,  -179,    98,  -179,   -19,  -179,  -179,
    -179,  -179,  -179,  -179,   -30,   266,    -5,   266,  -179,    89,
      88,  -179,    91,    90,   122,     6,  -179,   133,  -179,    14,
    -179,   160,    79,   109,  -179,  -179,   120,  -179,  -179,  -179,
    -179,  -179,   148,  -179,  -179,  -179,  -179,  -179,   151,  -179,
     266,  -179,  -179,  -179,   117,     4,  -179,  -179,  -179,  -179,
    -179,  -179,  -179,  -179,    95,   -14,   266,  -179,   161,  -179,
    -179,   120,  -179,  -179,   147,  -179,  -179,    27,  -179,  -179,
    -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,   156,
      51,  -179,   -19,  -179,   117,  -179,  -179,   107,   126,   -20,
    -179,   129,  -179,    79,  -179,    51,   216,  -179,  -179,  -179,
    -179
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -179,  -179,   181,  -179,   182,   174,    24,  -179,  -179,  -179,
     162,  -179,  -179,  -179,  -179,  -179,  -179,   -28,  -179,  -179,
    -178,    87,  -179,   -11,  -179,  -179,  -179,   -26,  -179,   142,
    -179,  -179,  -179,  -179,     9,  -179,  -179,  -179,  -179,   -25,
    -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,  -179,
    -179,  -130,  -179,  -179,  -179,  -179,  -179,  -179,  -179,   113,
    -179,  -179,   -82,  -179,  -179,  -179,    12,  -179,  -117,  -179,
     -12,  -179,    64,  -133,  -179,   -81,   -35
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -135
static const yytype_int16 yytable[] =
{
      56,    92,   116,    93,   167,    94,   209,    96,    97,    98,
     187,   100,   101,   102,   103,    21,    85,   179,   127,   188,
     235,    86,   168,   -26,   169,   112,   210,   109,   214,    12,
     236,   215,    16,   110,    27,   211,   113,   111,   170,     4,
     157,   140,   141,   142,   163,   143,   209,  -134,    87,    36,
     206,    37,   203,     9,  -134,   174,   175,   176,   114,   177,
     123,    88,    38,    39,  -134,    40,    28,    41,    42,    14,
     136,    43,    14,    18,   189,  -134,    44,   146,   147,   148,
      45,    46,   193,    47,   129,    48,    29,   194,    49,   231,
      10,    30,   130,     1,     2,   196,   197,  -134,   162,   223,
     224,   131,    50,    51,    52,   132,    53,    54,    11,    17,
     133,    11,    21,    20,    36,    24,    37,    35,    81,    83,
      95,   105,    91,   -49,   -49,   -49,   134,    38,    39,    99,
      40,   104,    41,    42,   108,   115,    43,   117,   118,   230,
     120,    44,   125,   137,   139,    45,    46,   150,    47,   151,
      48,   156,   154,    49,   239,   158,   164,   181,   182,   183,
     186,   184,   218,   192,   195,   220,   204,    50,    51,    52,
     217,    53,    54,    36,   200,    37,    11,   199,   205,   219,
     222,    29,   228,     8,   -49,   -49,    38,    39,    15,    40,
      23,    41,    42,    84,   234,    43,   232,   237,   225,   107,
      44,   233,   149,   213,    45,    46,   227,    47,   238,    48,
     240,   126,    49,   221,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    36,     0,    37,    50,    51,    52,     0,
      53,    54,     0,     0,   -49,    11,    38,    39,     0,    40,
       0,    41,    42,     0,     0,    43,     0,     0,     0,     0,
      44,     0,     0,     0,    45,    46,     0,    47,     0,    48,
       0,     0,    49,     0,     0,     0,     0,     0,     0,     0,
     -49,     0,     0,    36,     0,    37,    50,    51,    52,     0,
      53,    54,     0,     0,   -49,    11,    38,    39,     0,    40,
       0,    41,    42,     0,     0,    43,     0,     0,     0,     0,
      44,     0,     0,     0,    45,    46,     0,    47,     0,    48,
       0,     0,    49,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    50,    51,    52,     0,
      53,    54,     0,     0,     0,    11
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-179))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      26,    36,    83,    38,   137,    40,   184,    42,    43,    44,
       4,    46,    47,    48,    49,    12,     3,   147,   100,    13,
      40,     8,   139,    10,    54,    22,    22,    62,    42,     5,
      50,    45,     8,    68,    12,    31,    33,    72,    68,    66,
     122,    60,    61,    62,   126,    64,   224,     5,    35,     7,
     180,     9,   169,     0,    12,    60,    61,    62,    55,    64,
      95,    48,    20,    21,    22,    23,    44,    25,    26,     5,
     105,    29,     8,    68,    68,    33,    34,   112,   113,   114,
      38,    39,    68,    41,    11,    43,    64,    73,    46,   222,
      67,    69,    19,    56,    57,    16,    17,    55,   124,    72,
      73,    28,    60,    61,    62,    32,    64,    65,    69,    69,
      37,    69,    12,     5,     7,    64,     9,    70,    64,    69,
      14,    19,    70,    16,    17,    18,    53,    20,    21,    68,
      23,    68,    25,    26,    74,    51,    29,    22,    10,   220,
      60,    34,    30,    18,     4,    38,    39,    70,    41,    64,
      43,    68,    36,    46,   235,    60,    58,    68,    70,    68,
      38,    71,   197,    30,     4,   200,    18,    60,    61,    62,
     196,    64,    65,     7,    54,     9,    69,    68,    27,    18,
      33,    64,    26,     2,    18,    19,    20,    21,     6,    23,
      16,    25,    26,    31,    68,    29,   224,    68,   209,    57,
      34,   227,   115,   194,    38,    39,   218,    41,   233,    43,
     236,    98,    46,   201,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     7,    -1,     9,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    18,    69,    20,    21,    -1,    23,
      -1,    25,    26,    -1,    -1,    29,    -1,    -1,    -1,    -1,
      34,    -1,    -1,    -1,    38,    39,    -1,    41,    -1,    43,
      -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      54,    -1,    -1,     7,    -1,     9,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    18,    69,    20,    21,    -1,    23,
      -1,    25,    26,    -1,    -1,    29,    -1,    -1,    -1,    -1,
      34,    -1,    -1,    -1,    38,    39,    -1,    41,    -1,    43,
      -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    60,    61,    62,    -1,
      64,    65,    -1,    -1,    -1,    69
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    56,    57,    76,    66,    77,    78,    79,    77,     0,
      67,    69,    81,    82,   147,    79,    81,    69,    68,    80,
       5,    12,    83,    80,    64,   150,   151,    12,    44,    64,
      69,    84,    85,    86,    95,    70,     7,     9,    20,    21,
      23,    25,    26,    29,    34,    38,    39,    41,    43,    46,
      60,    61,    62,    64,    65,    81,   102,   103,   104,   105,
     106,   107,   112,   113,   115,   116,   117,   120,   121,   122,
     123,   127,   128,   129,   130,   131,   132,   133,   135,   136,
     147,    64,    96,    69,    85,     3,     8,    35,    48,    88,
      97,    70,   151,   151,   151,    14,   151,   151,   151,    68,
     151,   151,   151,   151,    68,    19,   138,   104,    74,   151,
     151,   151,    22,    33,    55,    51,   150,    22,    10,    98,
      60,   137,   134,   151,   145,    30,   134,   137,   143,    11,
      19,    28,    32,    37,    53,   125,   151,    18,   144,     4,
      60,    61,    62,    64,   148,   118,   151,   151,   151,    96,
      70,    64,    94,    87,    36,    99,    68,   137,    60,   108,
     109,   111,   102,   137,    58,   124,   139,   148,   143,    54,
      68,   149,   102,   126,    60,    61,    62,    64,   119,   126,
     146,    68,    70,    68,    71,    90,    38,     4,    13,    68,
     100,   101,    30,    68,    73,     4,    16,    17,   114,    68,
      54,   140,   141,   143,    18,    27,   126,    91,    92,    95,
      22,    31,    93,   109,    42,    45,   110,   102,   151,    18,
     151,   141,    33,    72,    73,    98,    89,   145,    26,   142,
     150,   148,    92,   102,    68,    40,    50,    68,   114,   150,
     102
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1806 of yacc.c  */
#line 227 "gram.y"
    {
						yylval.program = (PLpgSQL_stmt_block *)(yyvsp[(3) - (4)].stmt);
					}
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 231 "gram.y"
    {
						yylval.program = (PLpgSQL_stmt_block *)(yyvsp[(3) - (4)].stmt);
					}
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 245 "gram.y"
    {
						plpgsql_DumpExecTree = true;
					}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 255 "gram.y"
    {
						PLpgSQL_stmt_block *new;

						new = palloc0(sizeof(PLpgSQL_stmt_block));

						new->cmd_type	= PLPGSQL_STMT_BLOCK;
						new->lineno		= (yyvsp[(3) - (7)].ival);
						new->label		= (yyvsp[(1) - (7)].declhdr).label;
						new->n_initvars = (yyvsp[(1) - (7)].declhdr).n_initvars;
						new->initvarnos = (yyvsp[(1) - (7)].declhdr).initvarnos;
						new->body		= (yyvsp[(4) - (7)].list);
						new->exceptions	= (yyvsp[(5) - (7)].exception_block);

						check_labels((yyvsp[(1) - (7)].declhdr).label, (yyvsp[(7) - (7)].str));
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 277 "gram.y"
    {
						plpgsql_ns_setlocal(false);
						(yyval.declhdr).label	  = (yyvsp[(1) - (1)].str);
						(yyval.declhdr).n_initvars = 0;
						(yyval.declhdr).initvarnos = NULL;
					}
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 284 "gram.y"
    {
						plpgsql_ns_setlocal(false);
						(yyval.declhdr).label	  = (yyvsp[(1) - (2)].str);
						(yyval.declhdr).n_initvars = 0;
						(yyval.declhdr).initvarnos = NULL;
					}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 291 "gram.y"
    {
						plpgsql_ns_setlocal(false);
						if ((yyvsp[(3) - (3)].str) != NULL)
							(yyval.declhdr).label = (yyvsp[(3) - (3)].str);
						else
							(yyval.declhdr).label = (yyvsp[(1) - (3)].str);
						/* Remember variables declared in decl_stmts */
						(yyval.declhdr).n_initvars = plpgsql_add_initdatums(&((yyval.declhdr).initvarnos));
					}
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 303 "gram.y"
    {
						/* Forget any variables created before block */
						plpgsql_add_initdatums(NULL);
						/* Make variable names be local to block */
						plpgsql_ns_setlocal(true);
					}
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 312 "gram.y"
    {	(yyval.str) = (yyvsp[(2) - (2)].str);	}
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 314 "gram.y"
    {	(yyval.str) = (yyvsp[(1) - (1)].str);	}
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 318 "gram.y"
    {	(yyval.str) = (yyvsp[(3) - (5)].str);	}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 320 "gram.y"
    {	(yyval.str) = NULL;	}
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 322 "gram.y"
    {	(yyval.str) = NULL;	}
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 326 "gram.y"
    {
						PLpgSQL_variable	*var;

						var = plpgsql_build_variable((yyvsp[(1) - (5)].varname).name, (yyvsp[(1) - (5)].varname).lineno,
													 (yyvsp[(3) - (5)].dtype), true);
						if ((yyvsp[(2) - (5)].boolean))
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->isconst = (yyvsp[(2) - (5)].boolean);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("row or record variable cannot be CONSTANT")));
						}
						if ((yyvsp[(4) - (5)].boolean))
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->notnull = (yyvsp[(4) - (5)].boolean);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("row or record variable cannot be NOT NULL")));
						}
						if ((yyvsp[(5) - (5)].expr) != NULL)
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->default_val = (yyvsp[(5) - (5)].expr);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("default value for row or record variable is not supported")));
						}
					}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 360 "gram.y"
    {
						plpgsql_ns_additem((yyvsp[(4) - (5)].nsitem)->itemtype,
										   (yyvsp[(4) - (5)].nsitem)->itemno, (yyvsp[(1) - (5)].varname).name);
					}
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 365 "gram.y"
    {
						plpgsql_ns_rename((yyvsp[(2) - (5)].str), (yyvsp[(4) - (5)].str));
					}
    break;

  case 24:

/* Line 1806 of yacc.c  */
#line 369 "gram.y"
    { plpgsql_ns_push((yyvsp[(1) - (3)].varname).name); }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 371 "gram.y"
    {
						PLpgSQL_var *new;
						PLpgSQL_expr *curname_def;
						char		buf[1024];
						char		*cp1;
						char		*cp2;

						/* pop local namespace for cursor args */
						plpgsql_ns_pop();

						new = (PLpgSQL_var *)
							plpgsql_build_variable((yyvsp[(1) - (7)].varname).name, (yyvsp[(1) - (7)].varname).lineno,
												   plpgsql_build_datatype(REFCURSOROID,
																		  -1),
												   true);

						curname_def = palloc0(sizeof(PLpgSQL_expr));

						curname_def->dtype = PLPGSQL_DTYPE_EXPR;
						strcpy(buf, "SELECT ");
						cp1 = new->refname;
						cp2 = buf + strlen(buf);
						/*
						 * Don't trust standard_conforming_strings here;
						 * it might change before we use the string.
						 */
						if (strchr(cp1, '\\') != NULL)
							*cp2++ = ESCAPE_STRING_SYNTAX;
						*cp2++ = '\'';
						while (*cp1)
						{
							if (SQL_STR_DOUBLE(*cp1, true))
								*cp2++ = *cp1;
							*cp2++ = *cp1++;
						}
						strcpy(cp2, "'::refcursor");
						curname_def->query = pstrdup(buf);
						new->default_val = curname_def;

						new->cursor_explicit_expr = (yyvsp[(7) - (7)].expr);
						if ((yyvsp[(5) - (7)].row) == NULL)
							new->cursor_explicit_argrow = -1;
						else
							new->cursor_explicit_argrow = (yyvsp[(5) - (7)].row)->rowno;
						new->cursor_options = CURSOR_OPT_FAST_PLAN | (yyvsp[(2) - (7)].ival);
					}
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 420 "gram.y"
    {
						(yyval.ival) = 0;
					}
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 424 "gram.y"
    {
						(yyval.ival) = CURSOR_OPT_NO_SCROLL;
					}
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 428 "gram.y"
    {
						(yyval.ival) = CURSOR_OPT_SCROLL;
					}
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 434 "gram.y"
    {
						PLpgSQL_expr *query;

						plpgsql_ns_setlocal(false);
						query = read_sql_stmt("");
						plpgsql_ns_setlocal(true);

						(yyval.expr) = query;
					}
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 446 "gram.y"
    {
						(yyval.row) = NULL;
					}
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 450 "gram.y"
    {
						PLpgSQL_row *new;
						int i;
						ListCell *l;

						new = palloc0(sizeof(PLpgSQL_row));
						new->dtype = PLPGSQL_DTYPE_ROW;
						new->lineno = plpgsql_scanner_lineno();
						new->rowtupdesc = NULL;
						new->nfields = list_length((yyvsp[(2) - (3)].list));
						new->fieldnames = palloc(new->nfields * sizeof(char *));
						new->varnos = palloc(new->nfields * sizeof(int));

						i = 0;
						foreach (l, (yyvsp[(2) - (3)].list))
						{
							PLpgSQL_variable *arg = (PLpgSQL_variable *) lfirst(l);
							new->fieldnames[i] = arg->refname;
							new->varnos[i] = arg->dno;
							i++;
						}
						list_free((yyvsp[(2) - (3)].list));

						plpgsql_adddatum((PLpgSQL_datum *) new);
						(yyval.row) = new;
					}
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 479 "gram.y"
    {
						(yyval.list) = list_make1((yyvsp[(1) - (1)].variable));
					}
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 483 "gram.y"
    {
						(yyval.list) = lappend((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].variable));
					}
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 489 "gram.y"
    {
						(yyval.variable) = plpgsql_build_variable((yyvsp[(1) - (2)].varname).name, (yyvsp[(1) - (2)].varname).lineno,
													(yyvsp[(2) - (2)].dtype), true);
					}
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 499 "gram.y"
    {
						char	*name;
						PLpgSQL_nsitem *nsi;

						plpgsql_convert_ident(yytext, &name, 1);
						if (name[0] != '$')
							yyerror("only positional parameters can be aliased");

						plpgsql_ns_setlocal(false);

						nsi = plpgsql_ns_lookup(name, NULL, NULL, NULL);
						if (nsi == NULL)
						{
							plpgsql_error_lineno = plpgsql_scanner_lineno();
							ereport(ERROR,
									(errcode(ERRCODE_UNDEFINED_PARAMETER),
									 errmsg("function has no parameter \"%s\"",
											name)));
						}

						plpgsql_ns_setlocal(true);

						pfree(name);

						(yyval.nsitem) = nsi;
					}
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 528 "gram.y"
    {
						char	*name;

						plpgsql_convert_ident(yytext, &name, 1);
						(yyval.varname).name = name;
						(yyval.varname).lineno  = plpgsql_scanner_lineno();
					}
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 538 "gram.y"
    {
						char	*name;

						plpgsql_convert_ident(yytext, &name, 1);
						/* the result must be palloc'd, see plpgsql_ns_rename */
						(yyval.str) = name;
					}
    break;

  case 40:

/* Line 1806 of yacc.c  */
#line 548 "gram.y"
    { (yyval.boolean) = false; }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 550 "gram.y"
    { (yyval.boolean) = true; }
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 554 "gram.y"
    {
						/*
						 * If there's a lookahead token, read_datatype
						 * should consume it.
						 */
						(yyval.dtype) = read_datatype(yychar);
						yyclearin;
					}
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 565 "gram.y"
    { (yyval.boolean) = false; }
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 567 "gram.y"
    { (yyval.boolean) = true; }
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 571 "gram.y"
    { (yyval.expr) = NULL; }
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 573 "gram.y"
    {
						plpgsql_ns_setlocal(false);
						(yyval.expr) = plpgsql_read_expression(';', ";");
						plpgsql_ns_setlocal(true);
					}
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 585 "gram.y"
    {
						(yyval.list) = NIL;
					}
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 589 "gram.y"
    { (yyval.list) = (yyvsp[(1) - (1)].list); }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 593 "gram.y"
    {
							if ((yyvsp[(2) - (2)].stmt) == NULL)
								(yyval.list) = (yyvsp[(1) - (2)].list);
							else
								(yyval.list) = lappend((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].stmt));
						}
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 600 "gram.y"
    {
							if ((yyvsp[(1) - (1)].stmt) == NULL)
								(yyval.list) = NULL;
							else
								(yyval.list) = list_make1((yyvsp[(1) - (1)].stmt));
						}
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 609 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (2)].stmt); }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 611 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 613 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 615 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 617 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 619 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 621 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 623 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 625 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 627 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 629 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 631 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 633 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 635 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 637 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 639 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 641 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 643 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 645 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); }
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 649 "gram.y"
    {
						PLpgSQL_stmt_perform *new;

						new = palloc0(sizeof(PLpgSQL_stmt_perform));
						new->cmd_type = PLPGSQL_STMT_PERFORM;
						new->lineno   = (yyvsp[(2) - (3)].ival);
						new->expr  = (yyvsp[(3) - (3)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 73:

/* Line 1806 of yacc.c  */
#line 662 "gram.y"
    {
						PLpgSQL_stmt_assign *new;

						new = palloc0(sizeof(PLpgSQL_stmt_assign));
						new->cmd_type = PLPGSQL_STMT_ASSIGN;
						new->lineno   = (yyvsp[(2) - (4)].ival);
						new->varno = (yyvsp[(1) - (4)].ival);
						new->expr  = (yyvsp[(4) - (4)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 676 "gram.y"
    {
						PLpgSQL_stmt_getdiag	 *new;

						new = palloc0(sizeof(PLpgSQL_stmt_getdiag));
						new->cmd_type = PLPGSQL_STMT_GETDIAG;
						new->lineno   = (yyvsp[(3) - (5)].ival);
						new->diag_items  = (yyvsp[(4) - (5)].list);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 75:

/* Line 1806 of yacc.c  */
#line 689 "gram.y"
    {
						(yyval.list) = lappend((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].diagitem));
					}
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 693 "gram.y"
    {
						(yyval.list) = list_make1((yyvsp[(1) - (1)].diagitem));
					}
    break;

  case 77:

/* Line 1806 of yacc.c  */
#line 699 "gram.y"
    {
						PLpgSQL_diag_item *new;

						new = palloc(sizeof(PLpgSQL_diag_item));
						new->target = (yyvsp[(1) - (3)].ival);
						new->kind = (yyvsp[(3) - (3)].ival);

						(yyval.diagitem) = new;
					}
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 711 "gram.y"
    {
						(yyval.ival) = PLPGSQL_GETDIAG_ROW_COUNT;
					}
    break;

  case 79:

/* Line 1806 of yacc.c  */
#line 715 "gram.y"
    {
						(yyval.ival) = PLPGSQL_GETDIAG_RESULT_OID;
					}
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 721 "gram.y"
    {
						check_assignable(yylval.scalar);
						(yyval.ival) = yylval.scalar->dno;
					}
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 729 "gram.y"
    {
						check_assignable(yylval.scalar);
						(yyval.ival) = yylval.scalar->dno;
					}
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 734 "gram.y"
    {
						check_assignable((PLpgSQL_datum *) yylval.row);
						(yyval.ival) = yylval.row->rowno;
					}
    break;

  case 83:

/* Line 1806 of yacc.c  */
#line 739 "gram.y"
    {
						check_assignable((PLpgSQL_datum *) yylval.rec);
						(yyval.ival) = yylval.rec->recno;
					}
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 744 "gram.y"
    {
						PLpgSQL_arrayelem	*new;

						new = palloc0(sizeof(PLpgSQL_arrayelem));
						new->dtype		= PLPGSQL_DTYPE_ARRAYELEM;
						new->subscript	= (yyvsp[(3) - (3)].expr);
						new->arrayparentno = (yyvsp[(1) - (3)].ival);

						plpgsql_adddatum((PLpgSQL_datum *)new);

						(yyval.ival) = new->dno;
					}
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 759 "gram.y"
    {
						PLpgSQL_stmt_if *new;

						new = palloc0(sizeof(PLpgSQL_stmt_if));
						new->cmd_type	= PLPGSQL_STMT_IF;
						new->lineno		= (yyvsp[(2) - (8)].ival);
						new->cond		= (yyvsp[(3) - (8)].expr);
						new->true_body	= (yyvsp[(4) - (8)].list);
						new->false_body = (yyvsp[(5) - (8)].list);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 774 "gram.y"
    {
						(yyval.list) = NIL;
					}
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 778 "gram.y"
    {
						/*
						 * Translate the structure:	   into:
						 *
						 * IF c1 THEN				   IF c1 THEN
						 *	 ...						   ...
						 * ELSIF c2 THEN			   ELSE
						 *								   IF c2 THEN
						 *	 ...							   ...
						 * ELSE							   ELSE
						 *	 ...							   ...
						 * END IF						   END IF
						 *							   END IF
						 */
						PLpgSQL_stmt_if *new_if;

						/* first create a new if-statement */
						new_if = palloc0(sizeof(PLpgSQL_stmt_if));
						new_if->cmd_type	= PLPGSQL_STMT_IF;
						new_if->lineno		= (yyvsp[(2) - (5)].ival);
						new_if->cond		= (yyvsp[(3) - (5)].expr);
						new_if->true_body	= (yyvsp[(4) - (5)].list);
						new_if->false_body	= (yyvsp[(5) - (5)].list);

						/* wrap the if-statement in a "container" list */
						(yyval.list) = list_make1(new_if);
					}
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 807 "gram.y"
    {
						(yyval.list) = (yyvsp[(2) - (2)].list);
					}
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 813 "gram.y"
    {
						PLpgSQL_stmt_loop *new;

						new = palloc0(sizeof(PLpgSQL_stmt_loop));
						new->cmd_type = PLPGSQL_STMT_LOOP;
						new->lineno   = (yyvsp[(3) - (4)].ival);
						new->label	  = (yyvsp[(1) - (4)].str);
						new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;

						check_labels((yyvsp[(1) - (4)].str), (yyvsp[(4) - (4)].loop_body).end_label);
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 830 "gram.y"
    {
						PLpgSQL_stmt_while *new;

						new = palloc0(sizeof(PLpgSQL_stmt_while));
						new->cmd_type = PLPGSQL_STMT_WHILE;
						new->lineno   = (yyvsp[(3) - (5)].ival);
						new->label	  = (yyvsp[(1) - (5)].str);
						new->cond	  = (yyvsp[(4) - (5)].expr);
						new->body	  = (yyvsp[(5) - (5)].loop_body).stmts;

						check_labels((yyvsp[(1) - (5)].str), (yyvsp[(5) - (5)].loop_body).end_label);
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 848 "gram.y"
    {
						/* This runs after we've scanned the loop body */
						if ((yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_FORI)
						{
							PLpgSQL_stmt_fori		*new;

							new = (PLpgSQL_stmt_fori *) (yyvsp[(3) - (4)].stmt);
							new->label	  = (yyvsp[(1) - (4)].str);
							new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;
							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else if ((yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_FORS)
						{
							PLpgSQL_stmt_fors		*new;

							new = (PLpgSQL_stmt_fors *) (yyvsp[(3) - (4)].stmt);
							new->label	  = (yyvsp[(1) - (4)].str);
							new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;
							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else
						{
							PLpgSQL_stmt_dynfors	*new;

							Assert((yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_DYNFORS);
							new = (PLpgSQL_stmt_dynfors *) (yyvsp[(3) - (4)].stmt);
							new->label	  = (yyvsp[(1) - (4)].str);
							new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;
							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}

						check_labels((yyvsp[(1) - (4)].str), (yyvsp[(4) - (4)].loop_body).end_label);
						/* close namespace started in opt_block_label */
						plpgsql_ns_pop();
					}
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 887 "gram.y"
    {
						int			tok = yylex();

						/* Simple case: EXECUTE is a dynamic FOR loop */
						if (tok == K_EXECUTE)
						{
							PLpgSQL_stmt_dynfors	*new;
							PLpgSQL_expr			*expr;

							expr = plpgsql_read_expression(K_LOOP, "LOOP");

							new = palloc0(sizeof(PLpgSQL_stmt_dynfors));
							new->cmd_type = PLPGSQL_STMT_DYNFORS;
							new->lineno   = (yyvsp[(1) - (3)].ival);
							if ((yyvsp[(2) - (3)].forvariable).rec)
							{
								new->rec = (yyvsp[(2) - (3)].forvariable).rec;
								check_assignable((PLpgSQL_datum *) new->rec);
							}
							else if ((yyvsp[(2) - (3)].forvariable).row)
							{
								new->row = (yyvsp[(2) - (3)].forvariable).row;
								check_assignable((PLpgSQL_datum *) new->row);
							}
							else if ((yyvsp[(2) - (3)].forvariable).scalar)
							{
								/* convert single scalar to list */
								new->row = make_scalar_list1((yyvsp[(2) - (3)].forvariable).name, (yyvsp[(2) - (3)].forvariable).scalar, (yyvsp[(2) - (3)].forvariable).lineno);
								/* no need for check_assignable */
							}
							else
							{
								plpgsql_error_lineno = (yyvsp[(2) - (3)].forvariable).lineno;
								yyerror("loop variable of loop over rows must be a record or row variable or list of scalar variables");
							}
							new->query = expr;

							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else
						{
							PLpgSQL_expr	*expr1;
							bool			 reverse = false;

							/*
							 * We have to distinguish between two
							 * alternatives: FOR var IN a .. b and FOR
							 * var IN query. Unfortunately this is
							 * tricky, since the query in the second
							 * form needn't start with a SELECT
							 * keyword.  We use the ugly hack of
							 * looking for two periods after the first
							 * token. We also check for the REVERSE
							 * keyword, which means it must be an
							 * integer loop.
							 */
							if (tok == K_REVERSE)
								reverse = true;
							else
								plpgsql_push_back_token(tok);

							/*
							 * Read tokens until we see either a ".."
							 * or a LOOP. The text we read may not
							 * necessarily be a well-formed SQL
							 * statement, so we need to invoke
							 * read_sql_construct directly.
							 */
							expr1 = read_sql_construct(K_DOTDOT,
													   K_LOOP,
													   "LOOP",
													   "SELECT ",
													   true,
													   false,
													   &tok);

							if (tok == K_DOTDOT)
							{
								/* Saw "..", so it must be an integer loop */
								PLpgSQL_expr		*expr2;
								PLpgSQL_expr		*expr_by;
								PLpgSQL_var			*fvar;
								PLpgSQL_stmt_fori	*new;
								char				*varname;

								/* Check first expression is well-formed */
								check_sql_expr(expr1->query);

								/* Read and check the second one */
								expr2 = read_sql_construct(K_LOOP,
														   K_BY,
														   "LOOP",
														   "SELECT ",
														   true,
														   true,
														   &tok);

								/* Get the BY clause if any */
								if (tok == K_BY)
									expr_by = plpgsql_read_expression(K_LOOP, "LOOP");
								else
									expr_by = NULL;

								/* Should have had a single variable name */
								plpgsql_error_lineno = (yyvsp[(2) - (3)].forvariable).lineno;
								if ((yyvsp[(2) - (3)].forvariable).scalar && (yyvsp[(2) - (3)].forvariable).row)
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("integer FOR loop must have just one target variable")));

								/* create loop's private variable */
								plpgsql_convert_ident((yyvsp[(2) - (3)].forvariable).name, &varname, 1);
								fvar = (PLpgSQL_var *)
									plpgsql_build_variable(varname,
														   (yyvsp[(2) - (3)].forvariable).lineno,
														   plpgsql_build_datatype(INT4OID,
																				  -1),
														   true);

								new = palloc0(sizeof(PLpgSQL_stmt_fori));
								new->cmd_type = PLPGSQL_STMT_FORI;
								new->lineno   = (yyvsp[(1) - (3)].ival);
								new->var	  = fvar;
								new->reverse  = reverse;
								new->lower	  = expr1;
								new->upper	  = expr2;
								new->step	  = expr_by;

								(yyval.stmt) = (PLpgSQL_stmt *) new;
							}
							else
							{
								/*
								 * No "..", so it must be a query loop. We've prefixed an
								 * extra SELECT to the query text, so we need to remove that
								 * before performing syntax checking.
								 */
								char				*tmp_query;
								PLpgSQL_stmt_fors	*new;

								if (reverse)
									yyerror("cannot specify REVERSE in query FOR loop");

								Assert(strncmp(expr1->query, "SELECT ", 7) == 0);
								tmp_query = pstrdup(expr1->query + 7);
								pfree(expr1->query);
								expr1->query = tmp_query;

								check_sql_expr(expr1->query);

								new = palloc0(sizeof(PLpgSQL_stmt_fors));
								new->cmd_type = PLPGSQL_STMT_FORS;
								new->lineno   = (yyvsp[(1) - (3)].ival);
								if ((yyvsp[(2) - (3)].forvariable).rec)
								{
									new->rec = (yyvsp[(2) - (3)].forvariable).rec;
									check_assignable((PLpgSQL_datum *) new->rec);
								}
								else if ((yyvsp[(2) - (3)].forvariable).row)
								{
									new->row = (yyvsp[(2) - (3)].forvariable).row;
									check_assignable((PLpgSQL_datum *) new->row);
								}
								else if ((yyvsp[(2) - (3)].forvariable).scalar)
								{
									/* convert single scalar to list */
									new->row = make_scalar_list1((yyvsp[(2) - (3)].forvariable).name, (yyvsp[(2) - (3)].forvariable).scalar, (yyvsp[(2) - (3)].forvariable).lineno);
									/* no need for check_assignable */
								}
								else
								{
									plpgsql_error_lineno = (yyvsp[(2) - (3)].forvariable).lineno;
									yyerror("loop variable of loop over rows must be a record or row variable or list of scalar variables");
								}

								new->query = expr1;
								(yyval.stmt) = (PLpgSQL_stmt *) new;
							}
						}
					}
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 1088 "gram.y"
    {
						int			tok;

						(yyval.forvariable).name = pstrdup(yytext);
						(yyval.forvariable).lineno  = plpgsql_scanner_lineno();
						(yyval.forvariable).scalar = yylval.scalar;
						(yyval.forvariable).rec = NULL;
						(yyval.forvariable).row = NULL;
						/* check for comma-separated list */
						tok = yylex();
						plpgsql_push_back_token(tok);
						if (tok == ',')
							(yyval.forvariable).row = read_into_scalar_list((yyval.forvariable).name, (yyval.forvariable).scalar);
					}
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 1103 "gram.y"
    {
						int			tok;

						(yyval.forvariable).name = pstrdup(yytext);
						(yyval.forvariable).lineno  = plpgsql_scanner_lineno();
						(yyval.forvariable).scalar = NULL;
						(yyval.forvariable).rec = NULL;
						(yyval.forvariable).row = NULL;
						/* check for comma-separated list */
						tok = yylex();
						plpgsql_push_back_token(tok);
						if (tok == ',')
						{
							plpgsql_error_lineno = (yyval.forvariable).lineno;
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("\"%s\" is not a scalar variable",
											(yyval.forvariable).name)));
						}
					}
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 1124 "gram.y"
    {
						(yyval.forvariable).name = pstrdup(yytext);
						(yyval.forvariable).lineno  = plpgsql_scanner_lineno();
						(yyval.forvariable).scalar = NULL;
						(yyval.forvariable).rec = yylval.rec;
						(yyval.forvariable).row = NULL;
					}
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 1132 "gram.y"
    {
						(yyval.forvariable).name = pstrdup(yytext);
						(yyval.forvariable).lineno  = plpgsql_scanner_lineno();
						(yyval.forvariable).scalar = NULL;
						(yyval.forvariable).row = yylval.row;
						(yyval.forvariable).rec = NULL;
					}
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 1142 "gram.y"
    {
						PLpgSQL_stmt_exit *new;

						new = palloc0(sizeof(PLpgSQL_stmt_exit));
						new->cmd_type = PLPGSQL_STMT_EXIT;
						new->is_exit  = (yyvsp[(1) - (4)].boolean);
						new->lineno	  = (yyvsp[(2) - (4)].ival);
						new->label	  = (yyvsp[(3) - (4)].str);
						new->cond	  = (yyvsp[(4) - (4)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 1157 "gram.y"
    {
						(yyval.boolean) = true;
					}
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 1161 "gram.y"
    {
						(yyval.boolean) = false;
					}
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 1167 "gram.y"
    {
						int	tok;

						tok = yylex();
						if (tok == 0)
							yyerror("unexpected end of function definition");

						/*
						 * To avoid making NEXT and QUERY effectively be
						 * reserved words within plpgsql, recognize them
						 * via yytext.
						 */
						if (pg_strcasecmp(yytext, "next") == 0)
						{
							(yyval.stmt) = make_return_next_stmt((yyvsp[(2) - (2)].ival));
						}
						else if (pg_strcasecmp(yytext, "query") == 0)
						{
							(yyval.stmt) = make_return_query_stmt((yyvsp[(2) - (2)].ival));
						}
						else
						{
							plpgsql_push_back_token(tok);
							(yyval.stmt) = make_return_stmt((yyvsp[(2) - (2)].ival));
						}
					}
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 1196 "gram.y"
    {
						PLpgSQL_stmt_raise		*new;
						int	tok;

						new = palloc(sizeof(PLpgSQL_stmt_raise));

						new->cmd_type	= PLPGSQL_STMT_RAISE;
						new->lineno		= (yyvsp[(2) - (4)].ival);
						new->elog_level = (yyvsp[(3) - (4)].ival);
						new->message	= (yyvsp[(4) - (4)].str);
						new->params		= NIL;

						tok = yylex();

						/*
						 * We expect either a semi-colon, which
						 * indicates no parameters, or a comma that
						 * begins the list of parameter expressions
						 */
						if (tok != ',' && tok != ';')
							yyerror("syntax error");

						if (tok == ',')
						{
							PLpgSQL_expr *expr;
							int term;

							for (;;)
							{
								expr = read_sql_construct(',', ';', ", or ;",
														  "SELECT ",
														  true, true, &term);
								new->params = lappend(new->params, expr);
								if (term == ';')
									break;
							}
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 1239 "gram.y"
    {
						(yyval.str) = plpgsql_get_string_value();
					}
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 1245 "gram.y"
    {
						(yyval.ival) = ERROR;
					}
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 1249 "gram.y"
    {
						(yyval.ival) = WARNING;
					}
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 1253 "gram.y"
    {
						(yyval.ival) = NOTICE;
					}
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 1257 "gram.y"
    {
						(yyval.ival) = INFO;
					}
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 1261 "gram.y"
    {
						(yyval.ival) = LOG;
					}
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 1265 "gram.y"
    {
						(yyval.ival) = DEBUG1;
					}
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 1271 "gram.y"
    {
						(yyval.loop_body).stmts = (yyvsp[(1) - (5)].list);
						(yyval.loop_body).end_label = (yyvsp[(4) - (5)].str);
					}
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 1278 "gram.y"
    {
						(yyval.stmt) = make_execsql_stmt((yyvsp[(1) - (2)].str), (yyvsp[(2) - (2)].ival));
					}
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 1285 "gram.y"
    { (yyval.str) = pstrdup(yytext); }
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 1287 "gram.y"
    { (yyval.str) = pstrdup(yytext); }
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 1291 "gram.y"
    {
						/*
						 * We have to special-case INSERT so that its INTO
						 * won't be treated as an INTO-variables clause.
						 *
						 * Fortunately, this is the only valid use of INTO
						 * in a pl/pgsql SQL command, and INTO is already
						 * a fully reserved word in the main grammar.
						 */
						(yyval.stmt) = make_execsql_stmt("INSERT INTO", (yyvsp[(2) - (3)].ival));
					}
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 1305 "gram.y"
    {
						PLpgSQL_stmt_dynexecute *new;
						PLpgSQL_expr *expr;
						int endtoken;

						expr = read_sql_construct(K_INTO, ';', "INTO|;",
												  "SELECT ",
												  true, true, &endtoken);

						new = palloc(sizeof(PLpgSQL_stmt_dynexecute));
						new->cmd_type = PLPGSQL_STMT_DYNEXECUTE;
						new->lineno = (yyvsp[(2) - (2)].ival);
						new->query = expr;
						new->into = false;
						new->strict = false;
						new->rec = NULL;
						new->row = NULL;

						/* If we found "INTO", collect the argument */
						if (endtoken == K_INTO)
						{
							new->into = true;
							read_into_target(&new->rec, &new->row, &new->strict);
							if (yylex() != ';')
								yyerror("syntax error");
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 1338 "gram.y"
    {
						PLpgSQL_stmt_open *new;
						int				  tok;

						new = palloc0(sizeof(PLpgSQL_stmt_open));
						new->cmd_type = PLPGSQL_STMT_OPEN;
						new->lineno = (yyvsp[(2) - (3)].ival);
						new->curvar = (yyvsp[(3) - (3)].var)->varno;
						new->cursor_options = CURSOR_OPT_FAST_PLAN;

						if ((yyvsp[(3) - (3)].var)->cursor_explicit_expr == NULL)
						{
							/* be nice if we could use opt_scrollable here */
						    tok = yylex();
							if (tok == K_NOSCROLL)
							{
								new->cursor_options |= CURSOR_OPT_NO_SCROLL;
								tok = yylex();
							}
							else if (tok == K_SCROLL)
							{
								new->cursor_options |= CURSOR_OPT_SCROLL;
								tok = yylex();
							}

							if (tok != K_FOR)
							{
								plpgsql_error_lineno = (yyvsp[(2) - (3)].ival);
								ereport(ERROR,
										(errcode(ERRCODE_SYNTAX_ERROR),
										 errmsg("syntax error at \"%s\"",
												yytext),
										 errdetail("Expected FOR to open a reference cursor.")));
							}

							tok = yylex();
							if (tok == K_EXECUTE)
							{
								new->dynquery = read_sql_stmt("SELECT ");
							}
							else
							{
								plpgsql_push_back_token(tok);
								new->query = read_sql_stmt("");
							}
						}
						else
						{
							if ((yyvsp[(3) - (3)].var)->cursor_explicit_argrow >= 0)
							{
								char   *cp;

								tok = yylex();
								if (tok != '(')
								{
									plpgsql_error_lineno = plpgsql_scanner_lineno();
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("cursor \"%s\" has arguments",
													(yyvsp[(3) - (3)].var)->refname)));
								}

								/*
								 * Push back the '(', else read_sql_stmt
								 * will complain about unbalanced parens.
								 */
								plpgsql_push_back_token(tok);

								new->argquery = read_sql_stmt("SELECT ");

								/*
								 * Now remove the leading and trailing parens,
								 * because we want "select 1, 2", not
								 * "select (1, 2)".
								 */
								cp = new->argquery->query;

								if (strncmp(cp, "SELECT", 6) != 0)
								{
									plpgsql_error_lineno = plpgsql_scanner_lineno();
									/* internal error */
									elog(ERROR, "expected \"SELECT (\", got \"%s\"",
										 new->argquery->query);
								}
								cp += 6;
								while (*cp == ' ') /* could be more than 1 space here */
									cp++;
								if (*cp != '(')
								{
									plpgsql_error_lineno = plpgsql_scanner_lineno();
									/* internal error */
									elog(ERROR, "expected \"SELECT (\", got \"%s\"",
										 new->argquery->query);
								}
								*cp = ' ';

								cp += strlen(cp) - 1;

								if (*cp != ')')
									yyerror("expected \")\"");
								*cp = '\0';
							}
							else
							{
								tok = yylex();
								if (tok == '(')
								{
									plpgsql_error_lineno = plpgsql_scanner_lineno();
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("cursor \"%s\" has no arguments",
													(yyvsp[(3) - (3)].var)->refname)));
								}

								if (tok != ';')
								{
									plpgsql_error_lineno = plpgsql_scanner_lineno();
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("syntax error at \"%s\"",
													yytext)));
								}
							}
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 1468 "gram.y"
    {
						PLpgSQL_stmt_fetch *fetch = (yyvsp[(3) - (5)].fetch);
						PLpgSQL_rec	   *rec;
						PLpgSQL_row	   *row;

						/* We have already parsed everything through the INTO keyword */
						read_into_target(&rec, &row, NULL);

						if (yylex() != ';')
							yyerror("syntax error");

						fetch->lineno = (yyvsp[(2) - (5)].ival);
						fetch->rec		= rec;
						fetch->row		= row;
						fetch->curvar	= (yyvsp[(4) - (5)].var)->varno;
						fetch->is_move	= false;

						(yyval.stmt) = (PLpgSQL_stmt *)fetch;
					}
    break;

  case 117:

/* Line 1806 of yacc.c  */
#line 1490 "gram.y"
    {
						PLpgSQL_stmt_fetch *fetch = (yyvsp[(3) - (5)].fetch);

						fetch->lineno = (yyvsp[(2) - (5)].ival);
						fetch->curvar	= (yyvsp[(4) - (5)].var)->varno;
						fetch->is_move	= true;

						(yyval.stmt) = (PLpgSQL_stmt *)fetch;
					}
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 1502 "gram.y"
    {
						(yyval.fetch) = read_fetch_direction();
					}
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 1508 "gram.y"
    {
						PLpgSQL_stmt_close *new;

						new = palloc(sizeof(PLpgSQL_stmt_close));
						new->cmd_type = PLPGSQL_STMT_CLOSE;
						new->lineno = (yyvsp[(2) - (4)].ival);
						new->curvar = (yyvsp[(3) - (4)].var)->varno;

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					}
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 1521 "gram.y"
    {
						/* We do not bother building a node for NULL */
						(yyval.stmt) = NULL;
					}
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 1528 "gram.y"
    {
						if (yylval.scalar->dtype != PLPGSQL_DTYPE_VAR)
							yyerror("cursor variable must be a simple variable");

						if (((PLpgSQL_var *) yylval.scalar)->datatype->typoid != REFCURSOROID)
						{
							plpgsql_error_lineno = plpgsql_scanner_lineno();
							ereport(ERROR,
									(errcode(ERRCODE_DATATYPE_MISMATCH),
									 errmsg("\"%s\" must be of type cursor or refcursor",
											((PLpgSQL_var *) yylval.scalar)->refname)));
						}
						(yyval.var) = (PLpgSQL_var *) yylval.scalar;
					}
    break;

  case 122:

/* Line 1806 of yacc.c  */
#line 1545 "gram.y"
    { (yyval.exception_block) = NULL; }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 1547 "gram.y"
    {
						/*
						 * We use a mid-rule action to add these
						 * special variables to the namespace before
						 * parsing the WHEN clauses themselves.
						 */
						PLpgSQL_exception_block *new = palloc(sizeof(PLpgSQL_exception_block));
						PLpgSQL_variable *var;

						var = plpgsql_build_variable("sqlstate", (yyvsp[(2) - (2)].ival),
													 plpgsql_build_datatype(TEXTOID, -1),
													 true);
						((PLpgSQL_var *) var)->isconst = true;
						new->sqlstate_varno = var->dno;

						var = plpgsql_build_variable("sqlerrm", (yyvsp[(2) - (2)].ival),
													 plpgsql_build_datatype(TEXTOID, -1),
													 true);
						((PLpgSQL_var *) var)->isconst = true;
						new->sqlerrm_varno = var->dno;

						(yyval.exception_block) = new;
					}
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 1571 "gram.y"
    {
						PLpgSQL_exception_block *new = (yyvsp[(3) - (4)].exception_block);
						new->exc_list = (yyvsp[(4) - (4)].list);

						(yyval.exception_block) = new;
					}
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 1580 "gram.y"
    {
							(yyval.list) = lappend((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].exception));
						}
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 1584 "gram.y"
    {
							(yyval.list) = list_make1((yyvsp[(1) - (1)].exception));
						}
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 1590 "gram.y"
    {
						PLpgSQL_exception *new;

						new = palloc0(sizeof(PLpgSQL_exception));
						new->lineno     = (yyvsp[(2) - (5)].ival);
						new->conditions = (yyvsp[(3) - (5)].condition);
						new->action	    = (yyvsp[(5) - (5)].list);

						(yyval.exception) = new;
					}
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 1603 "gram.y"
    {
							PLpgSQL_condition	*old;

							for (old = (yyvsp[(1) - (3)].condition); old->next != NULL; old = old->next)
								/* skip */ ;
							old->next = plpgsql_parse_err_condition((yyvsp[(3) - (3)].str));

							(yyval.condition) = (yyvsp[(1) - (3)].condition);
						}
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 1613 "gram.y"
    {
							(yyval.condition) = plpgsql_parse_err_condition((yyvsp[(1) - (1)].str));
						}
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 1619 "gram.y"
    { (yyval.expr) = plpgsql_read_expression(';', ";"); }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 1623 "gram.y"
    { (yyval.expr) = plpgsql_read_expression(']', "]"); }
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1627 "gram.y"
    { (yyval.expr) = plpgsql_read_expression(K_THEN, "THEN"); }
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1631 "gram.y"
    { (yyval.expr) = plpgsql_read_expression(K_LOOP, "LOOP"); }
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1635 "gram.y"
    {
						plpgsql_ns_push(NULL);
						(yyval.str) = NULL;
					}
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 1640 "gram.y"
    {
						plpgsql_ns_push((yyvsp[(3) - (5)].str));
						(yyval.str) = (yyvsp[(3) - (5)].str);
					}
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 1650 "gram.y"
    {
						(yyval.str) = NULL;
					}
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1654 "gram.y"
    {
						(yyval.str) = check_label(yytext);
					}
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1658 "gram.y"
    {
						(yyval.str) = check_label(yytext);
					}
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1662 "gram.y"
    {
						(yyval.str) = check_label(yytext);
					}
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1666 "gram.y"
    {
						(yyval.str) = check_label(yytext);
					}
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1672 "gram.y"
    { (yyval.expr) = NULL; }
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1674 "gram.y"
    { (yyval.expr) = (yyvsp[(2) - (2)].expr); }
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1678 "gram.y"
    {
						char	*name;

						plpgsql_convert_ident(yytext, &name, 1);
						(yyval.str) = name;
					}
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1687 "gram.y"
    {
						(yyval.ival) = plpgsql_error_lineno = plpgsql_scanner_lineno();
					}
    break;



/* Line 1806 of yacc.c  */
#line 3807 "y.tab.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 1692 "gram.y"



#define MAX_EXPR_PARAMS  1024

/*
 * determine the expression parameter position to use for a plpgsql datum
 *
 * It is important that any given plpgsql datum map to just one parameter.
 * We used to be sloppy and assign a separate parameter for each occurrence
 * of a datum reference, but that fails for situations such as "select DATUM
 * from ... group by DATUM".
 *
 * The params[] array must be of size MAX_EXPR_PARAMS.
 */
static int
assign_expr_param(int dno, int *params, int *nparams)
{
	int		i;

	/* already have an instance of this dno? */
	for (i = 0; i < *nparams; i++)
	{
		if (params[i] == dno)
			return i+1;
	}
	/* check for array overflow */
	if (*nparams >= MAX_EXPR_PARAMS)
	{
		plpgsql_error_lineno = plpgsql_scanner_lineno();
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("too many variables specified in SQL statement")));
	}
	/* add new parameter dno to array */
	params[*nparams] = dno;
	(*nparams)++;
	return *nparams;
}


PLpgSQL_expr *
plpgsql_read_expression(int until, const char *expected)
{
	return read_sql_construct(until, 0, expected, "SELECT ", true, true, NULL);
}

static PLpgSQL_expr *
read_sql_stmt(const char *sqlstart)
{
	return read_sql_construct(';', 0, ";", sqlstart, false, true, NULL);
}

/*
 * Read a SQL construct and build a PLpgSQL_expr for it.
 *
 * until:		token code for expected terminator
 * until2:		token code for alternate terminator (pass 0 if none)
 * expected:	text to use in complaining that terminator was not found
 * sqlstart:	text to prefix to the accumulated SQL text
 * isexpression: whether to say we're reading an "expression" or a "statement"
 * valid_sql:   whether to check the syntax of the expr (prefixed with sqlstart)
 * endtoken:	if not NULL, ending token is stored at *endtoken
 *				(this is only interesting if until2 isn't zero)
 */
static PLpgSQL_expr *
read_sql_construct(int until,
				   int until2,
				   const char *expected,
				   const char *sqlstart,
				   bool isexpression,
				   bool valid_sql,
				   int *endtoken)
{
	int					tok;
	int					lno;
	PLpgSQL_dstring		ds;
	int					parenlevel = 0;
	int					nparams = 0;
	int					params[MAX_EXPR_PARAMS];
	char				buf[32];
	PLpgSQL_expr		*expr;

	lno = plpgsql_scanner_lineno();
	plpgsql_dstring_init(&ds);
	plpgsql_dstring_append(&ds, sqlstart);

	for (;;)
	{
		tok = yylex();
		if (tok == until && parenlevel == 0)
			break;
		if (tok == until2 && parenlevel == 0)
			break;
		if (tok == '(' || tok == '[')
			parenlevel++;
		else if (tok == ')' || tok == ']')
		{
			parenlevel--;
			if (parenlevel < 0)
				yyerror("mismatched parentheses");
		}
		/*
		 * End of function definition is an error, and we don't expect to
		 * hit a semicolon either (unless it's the until symbol, in which
		 * case we should have fallen out above).
		 */
		if (tok == 0 || tok == ';')
		{
			if (parenlevel != 0)
				yyerror("mismatched parentheses");
			plpgsql_error_lineno = lno;
			if (isexpression)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing \"%s\" at end of SQL expression",
								expected)));
			else
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing \"%s\" at end of SQL statement",
								expected)));
		}

		if (plpgsql_SpaceScanned)
			plpgsql_dstring_append(&ds, " ");

		switch (tok)
		{
			case T_SCALAR:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.scalar->dno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			case T_ROW:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.row->rowno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			case T_RECORD:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.rec->recno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			default:
				plpgsql_dstring_append(&ds, yytext);
				break;
		}
	}

	if (endtoken)
		*endtoken = tok;

	expr = palloc(sizeof(PLpgSQL_expr) + sizeof(int) * nparams - sizeof(int));
	expr->dtype			= PLPGSQL_DTYPE_EXPR;
	expr->query			= pstrdup(plpgsql_dstring_get(&ds));
	expr->plan			= NULL;
	expr->nparams		= nparams;
	while(nparams-- > 0)
		expr->params[nparams] = params[nparams];
	plpgsql_dstring_free(&ds);

	if (valid_sql)
		check_sql_expr(expr->query);

	return expr;
}

static PLpgSQL_type *
read_datatype(int tok)
{
	int					lno;
	PLpgSQL_dstring		ds;
	char			   *type_name;
	PLpgSQL_type		*result;
	bool				needspace = false;
	int					parenlevel = 0;

	lno = plpgsql_scanner_lineno();

	/* Often there will be a lookahead token, but if not, get one */
	if (tok == YYEMPTY)
		tok = yylex();

	if (tok == T_DTYPE)
	{
		/* lexer found word%TYPE and did its thing already */
		return yylval.dtype;
	}

	plpgsql_dstring_init(&ds);

	while (tok != ';')
	{
		if (tok == 0)
		{
			if (parenlevel != 0)
				yyerror("mismatched parentheses");
			else
				yyerror("incomplete datatype declaration");
		}
		/* Possible followers for datatype in a declaration */
		if (tok == K_NOT || tok == K_ASSIGN || tok == K_DEFAULT)
			break;
		/* Possible followers for datatype in a cursor_arg list */
		if ((tok == ',' || tok == ')') && parenlevel == 0)
			break;
		if (tok == '(')
			parenlevel++;
		else if (tok == ')')
			parenlevel--;
		if (needspace)
			plpgsql_dstring_append(&ds, " ");
		needspace = true;
		plpgsql_dstring_append(&ds, yytext);

		tok = yylex();
	}

	plpgsql_push_back_token(tok);

	type_name = plpgsql_dstring_get(&ds);

	if (type_name[0] == '\0')
		yyerror("missing datatype declaration");

	plpgsql_error_lineno = lno;	/* in case of error in parse_datatype */

	result = plpgsql_parse_datatype(type_name);

	plpgsql_dstring_free(&ds);

	return result;
}

static PLpgSQL_stmt *
make_execsql_stmt(const char *sqlstart, int lineno)
{
	PLpgSQL_dstring		ds;
	int					nparams = 0;
	int					params[MAX_EXPR_PARAMS];
	char				buf[32];
	PLpgSQL_stmt_execsql *execsql;
	PLpgSQL_expr		*expr;
	PLpgSQL_row			*row = NULL;
	PLpgSQL_rec			*rec = NULL;
	int					tok;
	bool				have_into = false;
	bool				have_strict = false;

	plpgsql_dstring_init(&ds);
	plpgsql_dstring_append(&ds, sqlstart);

	for (;;)
	{
		tok = yylex();
		if (tok == ';')
			break;
		if (tok == 0)
			yyerror("unexpected end of function definition");
		if (tok == K_INTO)
		{
			if (have_into)
				yyerror("INTO specified more than once");
			have_into = true;
			read_into_target(&rec, &row, &have_strict);
			continue;
		}

		if (plpgsql_SpaceScanned)
			plpgsql_dstring_append(&ds, " ");

		switch (tok)
		{
			case T_SCALAR:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.scalar->dno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			case T_ROW:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.row->rowno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			case T_RECORD:
				snprintf(buf, sizeof(buf), " $%d ",
						 assign_expr_param(yylval.rec->recno,
										   params, &nparams));
				plpgsql_dstring_append(&ds, buf);
				break;

			default:
				plpgsql_dstring_append(&ds, yytext);
				break;
		}
	}

	expr = palloc(sizeof(PLpgSQL_expr) + sizeof(int) * nparams - sizeof(int));
	expr->dtype			= PLPGSQL_DTYPE_EXPR;
	expr->query			= pstrdup(plpgsql_dstring_get(&ds));
	expr->plan			= NULL;
	expr->nparams		= nparams;
	while(nparams-- > 0)
		expr->params[nparams] = params[nparams];
	plpgsql_dstring_free(&ds);

	check_sql_expr(expr->query);

	execsql = palloc(sizeof(PLpgSQL_stmt_execsql));
	execsql->cmd_type = PLPGSQL_STMT_EXECSQL;
	execsql->lineno  = lineno;
	execsql->sqlstmt = expr;
	execsql->into	 = have_into;
	execsql->strict	 = have_strict;
	execsql->rec	 = rec;
	execsql->row	 = row;

	return (PLpgSQL_stmt *) execsql;
}


static PLpgSQL_stmt_fetch *
read_fetch_direction(void)
{
	PLpgSQL_stmt_fetch *fetch;
	int			tok;
	bool		check_FROM = true;

	/*
	 * We create the PLpgSQL_stmt_fetch struct here, but only fill in
	 * the fields arising from the optional direction clause
	 */
	fetch = (PLpgSQL_stmt_fetch *) palloc0(sizeof(PLpgSQL_stmt_fetch));
	fetch->cmd_type = PLPGSQL_STMT_FETCH;
	/* set direction defaults: */
	fetch->direction = FETCH_FORWARD;
	fetch->how_many  = 1;
	fetch->expr      = NULL;

	/*
	 * Most of the direction keywords are not plpgsql keywords, so we
	 * rely on examining yytext ...
	 */
	tok = yylex();
	if (tok == 0)
		yyerror("unexpected end of function definition");

	if (pg_strcasecmp(yytext, "next") == 0)
	{
		/* use defaults */
	}
	else if (pg_strcasecmp(yytext, "prior") == 0)
	{
		fetch->direction = FETCH_BACKWARD;
	}
	else if (pg_strcasecmp(yytext, "first") == 0)
	{
		fetch->direction = FETCH_ABSOLUTE;
	}
	else if (pg_strcasecmp(yytext, "last") == 0)
	{
		fetch->direction = FETCH_ABSOLUTE;
		fetch->how_many  = -1;
	}
	else if (pg_strcasecmp(yytext, "absolute") == 0)
	{
		fetch->direction = FETCH_ABSOLUTE;
		fetch->expr = read_sql_construct(K_FROM, K_IN, "FROM or IN",
										 "SELECT ", true, true, NULL);
		check_FROM = false;
	}
	else if (pg_strcasecmp(yytext, "relative") == 0)
	{
		fetch->direction = FETCH_RELATIVE;
		fetch->expr = read_sql_construct(K_FROM, K_IN, "FROM or IN",
										 "SELECT ", true, true, NULL);
		check_FROM = false;
	}
	else if (pg_strcasecmp(yytext, "forward") == 0)
	{
		/* use defaults */
	}
	else if (pg_strcasecmp(yytext, "backward") == 0)
	{
		fetch->direction = FETCH_BACKWARD;
	}
	else if (tok != T_SCALAR)
	{
		plpgsql_push_back_token(tok);
		fetch->expr = read_sql_construct(K_FROM, K_IN, "FROM or IN",
										 "SELECT ", true, true, NULL);
		check_FROM = false;
	}
	else
	{
		/* Assume there's no direction clause */
		plpgsql_push_back_token(tok);
		check_FROM = false;
	}

	/* check FROM or IN keyword after direction's specification */
	if (check_FROM)
	{
		tok = yylex();
		if (tok != K_FROM && tok != K_IN)
			yyerror("expected FROM or IN");
	}

	return fetch;
}


static PLpgSQL_stmt *
make_return_stmt(int lineno)
{
	PLpgSQL_stmt_return *new;

	new = palloc0(sizeof(PLpgSQL_stmt_return));
	new->cmd_type = PLPGSQL_STMT_RETURN;
	new->lineno   = lineno;
	new->expr	  = NULL;
	new->retvarno = -1;

	if (plpgsql_curr_compile->fn_retset)
	{
		if (yylex() != ';')
			yyerror("RETURN cannot have a parameter in function "
					"returning set; use RETURN NEXT or RETURN QUERY");
	}
	else if (plpgsql_curr_compile->out_param_varno >= 0)
	{
		if (yylex() != ';')
			yyerror("RETURN cannot have a parameter in function with OUT parameters");
		new->retvarno = plpgsql_curr_compile->out_param_varno;
	}
	else if (plpgsql_curr_compile->fn_rettype == VOIDOID)
	{
		if (yylex() != ';')
			yyerror("RETURN cannot have a parameter in function returning void");
	}
	else if (plpgsql_curr_compile->fn_retistuple)
	{
		switch (yylex())
		{
			case K_NULL:
				/* we allow this to support RETURN NULL in triggers */
				break;

			case T_ROW:
				new->retvarno = yylval.row->rowno;
				break;

			case T_RECORD:
				new->retvarno = yylval.rec->recno;
				break;

			default:
				yyerror("RETURN must specify a record or row variable in function returning tuple");
				break;
		}
		if (yylex() != ';')
			yyerror("RETURN must specify a record or row variable in function returning tuple");
	}
	else
	{
		/*
		 * Note that a well-formed expression is
		 * _required_ here; anything else is a
		 * compile-time error.
		 */
		new->expr = plpgsql_read_expression(';', ";");
	}

	return (PLpgSQL_stmt *) new;
}


static PLpgSQL_stmt *
make_return_next_stmt(int lineno)
{
	PLpgSQL_stmt_return_next *new;

	if (!plpgsql_curr_compile->fn_retset)
		yyerror("cannot use RETURN NEXT in a non-SETOF function");

	new = palloc0(sizeof(PLpgSQL_stmt_return_next));
	new->cmd_type	= PLPGSQL_STMT_RETURN_NEXT;
	new->lineno		= lineno;
	new->expr		= NULL;
	new->retvarno	= -1;

	if (plpgsql_curr_compile->out_param_varno >= 0)
	{
		if (yylex() != ';')
			yyerror("RETURN NEXT cannot have a parameter in function with OUT parameters");
		new->retvarno = plpgsql_curr_compile->out_param_varno;
	}
	else if (plpgsql_curr_compile->fn_retistuple)
	{
		switch (yylex())
		{
			case T_ROW:
				new->retvarno = yylval.row->rowno;
				break;

			case T_RECORD:
				new->retvarno = yylval.rec->recno;
				break;

			default:
				yyerror("RETURN NEXT must specify a record or row variable in function returning tuple");
				break;
		}
		if (yylex() != ';')
			yyerror("RETURN NEXT must specify a record or row variable in function returning tuple");
	}
	else
		new->expr = plpgsql_read_expression(';', ";");

	return (PLpgSQL_stmt *) new;
}


static PLpgSQL_stmt *
make_return_query_stmt(int lineno)
{
	PLpgSQL_stmt_return_query *new;

	if (!plpgsql_curr_compile->fn_retset)
		yyerror("cannot use RETURN QUERY in a non-SETOF function");

	new = palloc0(sizeof(PLpgSQL_stmt_return_query));
	new->cmd_type = PLPGSQL_STMT_RETURN_QUERY;
	new->lineno = lineno;
	new->query = read_sql_construct(';', 0, ")", "", false, true, NULL);

	return (PLpgSQL_stmt *) new;
}


static void
check_assignable(PLpgSQL_datum *datum)
{
	switch (datum->dtype)
	{
		case PLPGSQL_DTYPE_VAR:
			if (((PLpgSQL_var *) datum)->isconst)
			{
				plpgsql_error_lineno = plpgsql_scanner_lineno();
				ereport(ERROR,
						(errcode(ERRCODE_ERROR_IN_ASSIGNMENT),
						 errmsg("\"%s\" is declared CONSTANT",
								((PLpgSQL_var *) datum)->refname)));
			}
			break;
		case PLPGSQL_DTYPE_ROW:
			/* always assignable? */
			break;
		case PLPGSQL_DTYPE_REC:
			/* always assignable?  What about NEW/OLD? */
			break;
		case PLPGSQL_DTYPE_RECFIELD:
			/* always assignable? */
			break;
		case PLPGSQL_DTYPE_ARRAYELEM:
			/* always assignable? */
			break;
		case PLPGSQL_DTYPE_TRIGARG:
			yyerror("cannot assign to tg_argv");
			break;
		default:
			elog(ERROR, "unrecognized dtype: %d", datum->dtype);
			break;
	}
}

/*
 * Read the argument of an INTO clause.  On entry, we have just read the
 * INTO keyword.
 */
static void
read_into_target(PLpgSQL_rec **rec, PLpgSQL_row **row, bool *strict)
{
	int			tok;

	/* Set default results */
	*rec = NULL;
	*row = NULL;
	if (strict)
		*strict = false;

	tok = yylex();
	if (strict && tok == K_STRICT)
	{
		*strict = true;
		tok = yylex();
	}

	switch (tok)
	{
		case T_ROW:
			*row = yylval.row;
			check_assignable((PLpgSQL_datum *) *row);
			break;

		case T_RECORD:
			*rec = yylval.rec;
			check_assignable((PLpgSQL_datum *) *rec);
			break;

		case T_SCALAR:
			*row = read_into_scalar_list(yytext, yylval.scalar);
			break;

		default:
			plpgsql_error_lineno = plpgsql_scanner_lineno();
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("syntax error at \"%s\"", yytext),
					 errdetail("Expected record variable, row variable, "
							   "or list of scalar variables following INTO.")));
	}
}

/*
 * Given the first datum and name in the INTO list, continue to read
 * comma-separated scalar variables until we run out. Then construct
 * and return a fake "row" variable that represents the list of
 * scalars.
 */
static PLpgSQL_row *
read_into_scalar_list(const char *initial_name,
					  PLpgSQL_datum *initial_datum)
{
	int				 nfields;
	char			*fieldnames[1024];
	int				 varnos[1024];
	PLpgSQL_row		*row;
	int				 tok;

	check_assignable(initial_datum);
	fieldnames[0] = pstrdup(initial_name);
	varnos[0]	  = initial_datum->dno;
	nfields		  = 1;

	while ((tok = yylex()) == ',')
	{
		/* Check for array overflow */
		if (nfields >= 1024)
		{
			plpgsql_error_lineno = plpgsql_scanner_lineno();
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("too many INTO variables specified")));
		}

		tok = yylex();
		switch(tok)
		{
			case T_SCALAR:
				check_assignable(yylval.scalar);
				fieldnames[nfields] = pstrdup(yytext);
				varnos[nfields++]	= yylval.scalar->dno;
				break;

			default:
				plpgsql_error_lineno = plpgsql_scanner_lineno();
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("\"%s\" is not a scalar variable",
								yytext)));
		}
	}

	/*
	 * We read an extra, non-comma token from yylex(), so push it
	 * back onto the input stream
	 */
	plpgsql_push_back_token(tok);

	row = palloc(sizeof(PLpgSQL_row));
	row->dtype = PLPGSQL_DTYPE_ROW;
	row->refname = pstrdup("*internal*");
	row->lineno = plpgsql_scanner_lineno();
	row->rowtupdesc = NULL;
	row->nfields = nfields;
	row->fieldnames = palloc(sizeof(char *) * nfields);
	row->varnos = palloc(sizeof(int) * nfields);
	while (--nfields >= 0)
	{
		row->fieldnames[nfields] = fieldnames[nfields];
		row->varnos[nfields] = varnos[nfields];
	}

	plpgsql_adddatum((PLpgSQL_datum *)row);

	return row;
}

/*
 * Convert a single scalar into a "row" list.  This is exactly
 * like read_into_scalar_list except we never consume any input.
 * In fact, since this can be invoked long after the source
 * input was actually read, the lineno has to be passed in.
 */
static PLpgSQL_row *
make_scalar_list1(const char *initial_name,
				  PLpgSQL_datum *initial_datum,
				  int lineno)
{
	PLpgSQL_row		*row;

	check_assignable(initial_datum);

	row = palloc(sizeof(PLpgSQL_row));
	row->dtype = PLPGSQL_DTYPE_ROW;
	row->refname = pstrdup("*internal*");
	row->lineno = lineno;
	row->rowtupdesc = NULL;
	row->nfields = 1;
	row->fieldnames = palloc(sizeof(char *));
	row->varnos = palloc(sizeof(int));
	row->fieldnames[0] = pstrdup(initial_name);
	row->varnos[0] = initial_datum->dno;

	plpgsql_adddatum((PLpgSQL_datum *)row);

	return row;
}

/*
 * When the PL/PgSQL parser expects to see a SQL statement, it is very
 * liberal in what it accepts; for example, we often assume an
 * unrecognized keyword is the beginning of a SQL statement. This
 * avoids the need to duplicate parts of the SQL grammar in the
 * PL/PgSQL grammar, but it means we can accept wildly malformed
 * input. To try and catch some of the more obviously invalid input,
 * we run the strings we expect to be SQL statements through the main
 * SQL parser.
 *
 * We only invoke the raw parser (not the analyzer); this doesn't do
 * any database access and does not check any semantic rules, it just
 * checks for basic syntactic correctness. We do this here, rather
 * than after parsing has finished, because a malformed SQL statement
 * may cause the PL/PgSQL parser to become confused about statement
 * borders. So it is best to bail out as early as we can.
 */
static void
check_sql_expr(const char *stmt)
{
	ErrorContextCallback  syntax_errcontext;
	ErrorContextCallback *previous_errcontext;
	MemoryContext oldCxt;

	if (!plpgsql_check_syntax)
		return;

	/*
	 * Setup error traceback support for ereport(). The previous
	 * ereport callback is installed by pl_comp.c, but we don't want
	 * that to be invoked (since it will try to transpose the syntax
	 * error to be relative to the CREATE FUNCTION), so temporarily
	 * remove it from the list of callbacks.
	 */
	Assert(error_context_stack->callback == plpgsql_compile_error_callback);

	previous_errcontext = error_context_stack;
	syntax_errcontext.callback = plpgsql_sql_error_callback;
	syntax_errcontext.arg = (char *) stmt;
	syntax_errcontext.previous = error_context_stack->previous;
	error_context_stack = &syntax_errcontext;

	oldCxt = MemoryContextSwitchTo(compile_tmp_cxt);
	(void) raw_parser(stmt);
	MemoryContextSwitchTo(oldCxt);

	/* Restore former ereport callback */
	error_context_stack = previous_errcontext;
}

static void
plpgsql_sql_error_callback(void *arg)
{
	char *sql_stmt = (char *) arg;

	Assert(plpgsql_error_funcname);

	errcontext("SQL statement in PL/PgSQL function \"%s\" near line %d",
			   plpgsql_error_funcname, plpgsql_error_lineno);
	internalerrquery(sql_stmt);
	internalerrposition(geterrposition());
	errposition(0);
}

static char *
check_label(const char *yytxt)
{
	char	   *label_name;

	plpgsql_convert_ident(yytxt, &label_name, 1);
	if (plpgsql_ns_lookup_label(label_name) == NULL)
		yyerror("no such label");
	return label_name;
}

static void
check_labels(const char *start_label, const char *end_label)
{
	if (end_label)
	{
		if (!start_label)
		{
			plpgsql_error_lineno = plpgsql_scanner_lineno();
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("end label \"%s\" specified for unlabelled block",
							end_label)));
		}

		if (strcmp(start_label, end_label) != 0)
		{
			plpgsql_error_lineno = plpgsql_scanner_lineno();
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("end label \"%s\" differs from block's label \"%s\"",
							end_label, start_label)));
		}
	}
}

/* Needed to avoid conflict between different prefix settings: */
#undef yylex

#include "pl_scan.c"

