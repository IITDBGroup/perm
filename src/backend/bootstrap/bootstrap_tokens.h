/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CONST_P = 258,
     ID = 259,
     OPEN = 260,
     XCLOSE = 261,
     XCREATE = 262,
     INSERT_TUPLE = 263,
     XDECLARE = 264,
     INDEX = 265,
     ON = 266,
     USING = 267,
     XBUILD = 268,
     INDICES = 269,
     UNIQUE = 270,
     XTOAST = 271,
     COMMA = 272,
     EQUALS = 273,
     LPAREN = 274,
     RPAREN = 275,
     OBJ_ID = 276,
     XBOOTSTRAP = 277,
     XSHARED_RELATION = 278,
     XWITHOUT_OIDS = 279,
     NULLVAL = 280,
     low = 281,
     high = 282
   };
#endif
/* Tokens.  */
#define CONST_P 258
#define ID 259
#define OPEN 260
#define XCLOSE 261
#define XCREATE 262
#define INSERT_TUPLE 263
#define XDECLARE 264
#define INDEX 265
#define ON 266
#define USING 267
#define XBUILD 268
#define INDICES 269
#define UNIQUE 270
#define XTOAST 271
#define COMMA 272
#define EQUALS 273
#define LPAREN 274
#define RPAREN 275
#define OBJ_ID 276
#define XBOOTSTRAP 277
#define XSHARED_RELATION 278
#define XWITHOUT_OIDS 279
#define NULLVAL 280
#define low 281
#define high 282




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 86 "bootparse.y"
{
	List		*list;
	IndexElem	*ielem;
	char		*str;
	int			ival;
	Oid			oidval;
}
/* Line 1529 of yacc.c.  */
#line 111 "y.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE boot_yylval;

