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
     TOK_STRING = 258,
     TOK_NUMBER = 259,
     TOK_SIZE = 260,
     TOK_TYPE = 261,
     TOK_EXT = 262,
     TOK_SOURCES = 263,
     TOK_COMPLETE = 264,
     TOK_BITRATE = 265,
     TOK_LENGTH = 266,
     TOK_CODEC = 267,
     TOK_RATING = 268,
     TOK_TITLE = 269,
     TOK_ALBUM = 270,
     TOK_ARTIST = 271,
     TOK_TYPEVAL = 272,
     TOK_AND = 273,
     TOK_OR = 274,
     TOK_NOT = 275,
     TOK_OPR_EQ = 276,
     TOK_OPR_LT = 277,
     TOK_OPR_LE = 278,
     TOK_OPR_GT = 279,
     TOK_OPR_GE = 280,
     TOK_OPR_NE = 281,
     TOK_ED2K_LINK = 282,
     TOK_EOF = 283
   };
#endif
/* Tokens.  */
#define TOK_STRING 258
#define TOK_NUMBER 259
#define TOK_SIZE 260
#define TOK_TYPE 261
#define TOK_EXT 262
#define TOK_SOURCES 263
#define TOK_COMPLETE 264
#define TOK_BITRATE 265
#define TOK_LENGTH 266
#define TOK_CODEC 267
#define TOK_RATING 268
#define TOK_TITLE 269
#define TOK_ALBUM 270
#define TOK_ARTIST 271
#define TOK_TYPEVAL 272
#define TOK_AND 273
#define TOK_OR 274
#define TOK_NOT 275
#define TOK_OPR_EQ 276
#define TOK_OPR_LT 277
#define TOK_OPR_LE 278
#define TOK_OPR_GT 279
#define TOK_OPR_GE 280
#define TOK_OPR_NE 281
#define TOK_ED2K_LINK 282
#define TOK_EOF 283




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE

{
	uint64			num;
	int				iopr;
	CStringA		*pstr;
	CSearchExpr		*pexpr;
	CSearchAttr		*pattr;
}
/* Line 1489 of yacc.c.  */

	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

