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
     ABORT_P = 258,
     ABSOLUTE_P = 259,
     ACCESS = 260,
     ACTION = 261,
     ADD_P = 262,
     ADMIN = 263,
     AFTER = 264,
     AGGREGATE = 265,
     ALL = 266,
     ALSO = 267,
     ALTER = 268,
     ALWAYS = 269,
     ANALYSE = 270,
     ANALYZE = 271,
     AND = 272,
     ANNOT = 273,
     ANY = 274,
     ARRAY = 275,
     AS = 276,
     ASC = 277,
     ASSERTION = 278,
     ASSIGNMENT = 279,
     ASYMMETRIC = 280,
     AT = 281,
     AUTHORIZATION = 282,
     BACKWARD = 283,
     BASERELATION = 284,
     BEFORE = 285,
     BEGIN_P = 286,
     BETWEEN = 287,
     BIGINT = 288,
     BINARY = 289,
     BIT = 290,
     BOOLEAN_P = 291,
     BOTH = 292,
     BY = 293,
     CACHE = 294,
     CALLED = 295,
     CASCADE = 296,
     CASCADED = 297,
     CASE = 298,
     CAST = 299,
     CHAIN = 300,
     CHAR_P = 301,
     CHARACTER = 302,
     CHARACTERISTICS = 303,
     CHECK = 304,
     CHECKPOINT = 305,
     CLASS = 306,
     CLOSE = 307,
     CLUSTER = 308,
     COALESCE = 309,
     COLLATE = 310,
     COLUMN = 311,
     COMMENT = 312,
     COMMIT = 313,
     COMMITTED = 314,
     COMPLETE = 315,
     CONCURRENTLY = 316,
     CONFIGURATION = 317,
     CONNECTION = 318,
     CONSTRAINT = 319,
     CONSTRAINTS = 320,
     CONTENT_P = 321,
     CONTRIBUTION = 322,
     CONVERSION_P = 323,
     COPY = 324,
     COST = 325,
     CREATE = 326,
     CREATEDB = 327,
     CREATEROLE = 328,
     CREATEUSER = 329,
     CROSS = 330,
     CSV = 331,
     CURRENT_P = 332,
     CURRENT_DATE = 333,
     CURRENT_ROLE = 334,
     CURRENT_TIME = 335,
     CURRENT_TIMESTAMP = 336,
     CURRENT_USER = 337,
     CURSOR = 338,
     CYCLE = 339,
     DATABASE = 340,
     DAY_P = 341,
     DEALLOCATE = 342,
     DEC = 343,
     DECIMAL_P = 344,
     DECLARE = 345,
     DEFAULT = 346,
     DEFAULTS = 347,
     DEFERRABLE = 348,
     DEFERRED = 349,
     DEFINER = 350,
     DELETE_P = 351,
     DELIMITER = 352,
     DELIMITERS = 353,
     DESC = 354,
     DICTIONARY = 355,
     DISABLE_P = 356,
     DISCARD = 357,
     DISTINCT = 358,
     DO = 359,
     DOCUMENT_P = 360,
     DOMAIN_P = 361,
     DOUBLE_P = 362,
     DROP = 363,
     EACH = 364,
     ELSE = 365,
     ENABLE_P = 366,
     ENCODING = 367,
     ENCRYPTED = 368,
     END_P = 369,
     ENUM_P = 370,
     ESCAPE = 371,
     EXCEPT = 372,
     EXCLUDING = 373,
     EXCLUSIVE = 374,
     EXECUTE = 375,
     EXISTS = 376,
     EXPLAIN = 377,
     EXTERNAL = 378,
     EXTRACT = 379,
     FALSE_P = 380,
     FAMILY = 381,
     FETCH = 382,
     FIRST_P = 383,
     FLOAT_P = 384,
     FOR = 385,
     FORCE = 386,
     FOREIGN = 387,
     FORWARD = 388,
     FREEZE = 389,
     FROM = 390,
     FULL = 391,
     FUNCTION = 392,
     GLOBAL = 393,
     GRANT = 394,
     GRANTED = 395,
     GRAPH = 396,
     GREATEST = 397,
     GROUP_P = 398,
     HANDLER = 399,
     HAVING = 400,
     HEADER_P = 401,
     HOLD = 402,
     HOUR_P = 403,
     IF_P = 404,
     ILIKE = 405,
     IMMEDIATE = 406,
     IMMUTABLE = 407,
     IMPLICIT_P = 408,
     IN_P = 409,
     INCLUDING = 410,
     INCREMENT = 411,
     INDEX = 412,
     INDEXES = 413,
     INFLUENCE = 414,
     INHERIT = 415,
     INHERITS = 416,
     INITIALLY = 417,
     INNER_P = 418,
     INOUT = 419,
     INPUT_P = 420,
     INSENSITIVE = 421,
     INSERT = 422,
     INSTEAD = 423,
     INT_P = 424,
     INTEGER = 425,
     INTERSECT = 426,
     INTERVAL = 427,
     INTO = 428,
     INVOKER = 429,
     IS = 430,
     ISNULL = 431,
     ISOLATION = 432,
     JOIN = 433,
     KEY = 434,
     LANCOMPILER = 435,
     LANGUAGE = 436,
     LARGE_P = 437,
     LAST_P = 438,
     LEADING = 439,
     LEAST = 440,
     LEFT = 441,
     LEVEL = 442,
     LIKE = 443,
     LIMIT = 444,
     LISTEN = 445,
     LOAD = 446,
     LOCAL = 447,
     LOCALTIME = 448,
     LOCALTIMESTAMP = 449,
     LOCATION = 450,
     LOCK_P = 451,
     LOGIN_P = 452,
     MAPPING = 453,
     MAPPROV = 454,
     MATCH = 455,
     MAXVALUE = 456,
     MINUTE_P = 457,
     MINVALUE = 458,
     MODE = 459,
     MONTH_P = 460,
     MOVE = 461,
     NAME_P = 462,
     NAMES = 463,
     NATIONAL = 464,
     NATURAL = 465,
     NCHAR = 466,
     NEW = 467,
     NEXT = 468,
     NO = 469,
     NOCREATEDB = 470,
     NOCREATEROLE = 471,
     NOCREATEUSER = 472,
     NOINHERIT = 473,
     NOLOGIN_P = 474,
     NONE = 475,
     NOSUPERUSER = 476,
     NOT = 477,
     NOTHING = 478,
     NOTIFY = 479,
     NOTNULL = 480,
     NOTTRANSITIVE = 481,
     NOWAIT = 482,
     NULL_P = 483,
     NULLIF = 484,
     NULLS_P = 485,
     NUMERIC = 486,
     OBJECT_P = 487,
     OF = 488,
     OFF = 489,
     OFFSET = 490,
     OIDS = 491,
     OLD = 492,
     ON = 493,
     ONLY = 494,
     OPERATOR = 495,
     OPTION = 496,
     OR = 497,
     ORDER = 498,
     OUT_P = 499,
     OUTER_P = 500,
     OVERLAPS = 501,
     OVERLAY = 502,
     OWNED = 503,
     OWNER = 504,
     PARSER = 505,
     PARTIAL = 506,
     PASSWORD = 507,
     PLACING = 508,
     PLANS = 509,
     POSITION = 510,
     PRECISION = 511,
     PRESERVE = 512,
     PREPARE = 513,
     PREPARED = 514,
     PRIMARY = 515,
     PRIOR = 516,
     PRIVILEGES = 517,
     PROCEDURAL = 518,
     PROCEDURE = 519,
     PROVENANCE = 520,
     QUOTE = 521,
     READ = 522,
     REAL = 523,
     REASSIGN = 524,
     RECHECK = 525,
     REFERENCES = 526,
     REINDEX = 527,
     RELATIVE_P = 528,
     RELEASE = 529,
     RENAME = 530,
     REPEATABLE = 531,
     REPLACE = 532,
     REPLICA = 533,
     RESET = 534,
     RESTART = 535,
     RESTRICT = 536,
     RETURNING = 537,
     RETURNS = 538,
     REVOKE = 539,
     RIGHT = 540,
     ROLE = 541,
     ROLLBACK = 542,
     ROW = 543,
     ROWS = 544,
     RULE = 545,
     SAVEPOINT = 546,
     SCHEMA = 547,
     SCROLL = 548,
     SEARCH = 549,
     SECOND_P = 550,
     SECURITY = 551,
     SELECT = 552,
     SEQUENCE = 553,
     SERIALIZABLE = 554,
     SESSION = 555,
     SESSION_USER = 556,
     SET = 557,
     SETOF = 558,
     SHARE = 559,
     SHOW = 560,
     SIMILAR = 561,
     SIMPLE = 562,
     SMALLINT = 563,
     SOME = 564,
     SQLTEXT = 565,
     SQLTEXTDB2 = 566,
     STABLE = 567,
     STANDALONE_P = 568,
     START = 569,
     STATEMENT = 570,
     STATISTICS = 571,
     STDIN = 572,
     STDOUT = 573,
     STORAGE = 574,
     STRICT_P = 575,
     STRIP_P = 576,
     SUBSTRING = 577,
     SUPERUSER_P = 578,
     SYMMETRIC = 579,
     SYSID = 580,
     SYSTEM_P = 581,
     TABLE = 582,
     TABLESPACE = 583,
     TEMP = 584,
     TEMPLATE = 585,
     TEMPORARY = 586,
     TEXT_P = 587,
     THEN = 588,
     THIS = 589,
     TIME = 590,
     TIMESTAMP = 591,
     TO = 592,
     TRAILING = 593,
     TRANSACTION = 594,
     TRANSITIVE = 595,
     TRANSPROV = 596,
     TRANSSQL = 597,
     TRANSXML = 598,
     TREAT = 599,
     TRIGGER = 600,
     TRIM = 601,
     TRUE_P = 602,
     TRUNCATE = 603,
     TRUSTED = 604,
     TYPE_P = 605,
     UNCOMMITTED = 606,
     UNENCRYPTED = 607,
     UNION = 608,
     UNIQUE = 609,
     UNKNOWN = 610,
     UNLISTEN = 611,
     UNTIL = 612,
     UPDATE = 613,
     USER = 614,
     USING = 615,
     VACUUM = 616,
     VALID = 617,
     VALIDATOR = 618,
     VALUE_P = 619,
     VALUES = 620,
     VARCHAR = 621,
     VARYING = 622,
     VERBOSE = 623,
     VERSION_P = 624,
     VIEW = 625,
     VOLATILE = 626,
     WHEN = 627,
     WHERE = 628,
     WHITESPACE_P = 629,
     WITH = 630,
     WITHOUT = 631,
     WORK = 632,
     WRITE = 633,
     XML_P = 634,
     XMLATTRIBUTES = 635,
     XMLCONCAT = 636,
     XMLELEMENT = 637,
     XMLFOREST = 638,
     XMLPARSE = 639,
     XMLPI = 640,
     XMLROOT = 641,
     XMLSERIALIZE = 642,
     XSLT = 643,
     YEAR_P = 644,
     YES_P = 645,
     ZONE = 646,
     NULLS_FIRST = 647,
     NULLS_LAST = 648,
     WITH_CASCADED = 649,
     WITH_LOCAL = 650,
     WITH_CHECK = 651,
     IDENT = 652,
     FCONST = 653,
     SCONST = 654,
     BCONST = 655,
     XCONST = 656,
     Op = 657,
     ICONST = 658,
     PARAM = 659,
     POSTFIXOP = 660,
     UMINUS = 661,
     TYPECAST = 662
   };
#endif
/* Tokens.  */
#define ABORT_P 258
#define ABSOLUTE_P 259
#define ACCESS 260
#define ACTION 261
#define ADD_P 262
#define ADMIN 263
#define AFTER 264
#define AGGREGATE 265
#define ALL 266
#define ALSO 267
#define ALTER 268
#define ALWAYS 269
#define ANALYSE 270
#define ANALYZE 271
#define AND 272
#define ANNOT 273
#define ANY 274
#define ARRAY 275
#define AS 276
#define ASC 277
#define ASSERTION 278
#define ASSIGNMENT 279
#define ASYMMETRIC 280
#define AT 281
#define AUTHORIZATION 282
#define BACKWARD 283
#define BASERELATION 284
#define BEFORE 285
#define BEGIN_P 286
#define BETWEEN 287
#define BIGINT 288
#define BINARY 289
#define BIT 290
#define BOOLEAN_P 291
#define BOTH 292
#define BY 293
#define CACHE 294
#define CALLED 295
#define CASCADE 296
#define CASCADED 297
#define CASE 298
#define CAST 299
#define CHAIN 300
#define CHAR_P 301
#define CHARACTER 302
#define CHARACTERISTICS 303
#define CHECK 304
#define CHECKPOINT 305
#define CLASS 306
#define CLOSE 307
#define CLUSTER 308
#define COALESCE 309
#define COLLATE 310
#define COLUMN 311
#define COMMENT 312
#define COMMIT 313
#define COMMITTED 314
#define COMPLETE 315
#define CONCURRENTLY 316
#define CONFIGURATION 317
#define CONNECTION 318
#define CONSTRAINT 319
#define CONSTRAINTS 320
#define CONTENT_P 321
#define CONTRIBUTION 322
#define CONVERSION_P 323
#define COPY 324
#define COST 325
#define CREATE 326
#define CREATEDB 327
#define CREATEROLE 328
#define CREATEUSER 329
#define CROSS 330
#define CSV 331
#define CURRENT_P 332
#define CURRENT_DATE 333
#define CURRENT_ROLE 334
#define CURRENT_TIME 335
#define CURRENT_TIMESTAMP 336
#define CURRENT_USER 337
#define CURSOR 338
#define CYCLE 339
#define DATABASE 340
#define DAY_P 341
#define DEALLOCATE 342
#define DEC 343
#define DECIMAL_P 344
#define DECLARE 345
#define DEFAULT 346
#define DEFAULTS 347
#define DEFERRABLE 348
#define DEFERRED 349
#define DEFINER 350
#define DELETE_P 351
#define DELIMITER 352
#define DELIMITERS 353
#define DESC 354
#define DICTIONARY 355
#define DISABLE_P 356
#define DISCARD 357
#define DISTINCT 358
#define DO 359
#define DOCUMENT_P 360
#define DOMAIN_P 361
#define DOUBLE_P 362
#define DROP 363
#define EACH 364
#define ELSE 365
#define ENABLE_P 366
#define ENCODING 367
#define ENCRYPTED 368
#define END_P 369
#define ENUM_P 370
#define ESCAPE 371
#define EXCEPT 372
#define EXCLUDING 373
#define EXCLUSIVE 374
#define EXECUTE 375
#define EXISTS 376
#define EXPLAIN 377
#define EXTERNAL 378
#define EXTRACT 379
#define FALSE_P 380
#define FAMILY 381
#define FETCH 382
#define FIRST_P 383
#define FLOAT_P 384
#define FOR 385
#define FORCE 386
#define FOREIGN 387
#define FORWARD 388
#define FREEZE 389
#define FROM 390
#define FULL 391
#define FUNCTION 392
#define GLOBAL 393
#define GRANT 394
#define GRANTED 395
#define GRAPH 396
#define GREATEST 397
#define GROUP_P 398
#define HANDLER 399
#define HAVING 400
#define HEADER_P 401
#define HOLD 402
#define HOUR_P 403
#define IF_P 404
#define ILIKE 405
#define IMMEDIATE 406
#define IMMUTABLE 407
#define IMPLICIT_P 408
#define IN_P 409
#define INCLUDING 410
#define INCREMENT 411
#define INDEX 412
#define INDEXES 413
#define INFLUENCE 414
#define INHERIT 415
#define INHERITS 416
#define INITIALLY 417
#define INNER_P 418
#define INOUT 419
#define INPUT_P 420
#define INSENSITIVE 421
#define INSERT 422
#define INSTEAD 423
#define INT_P 424
#define INTEGER 425
#define INTERSECT 426
#define INTERVAL 427
#define INTO 428
#define INVOKER 429
#define IS 430
#define ISNULL 431
#define ISOLATION 432
#define JOIN 433
#define KEY 434
#define LANCOMPILER 435
#define LANGUAGE 436
#define LARGE_P 437
#define LAST_P 438
#define LEADING 439
#define LEAST 440
#define LEFT 441
#define LEVEL 442
#define LIKE 443
#define LIMIT 444
#define LISTEN 445
#define LOAD 446
#define LOCAL 447
#define LOCALTIME 448
#define LOCALTIMESTAMP 449
#define LOCATION 450
#define LOCK_P 451
#define LOGIN_P 452
#define MAPPING 453
#define MAPPROV 454
#define MATCH 455
#define MAXVALUE 456
#define MINUTE_P 457
#define MINVALUE 458
#define MODE 459
#define MONTH_P 460
#define MOVE 461
#define NAME_P 462
#define NAMES 463
#define NATIONAL 464
#define NATURAL 465
#define NCHAR 466
#define NEW 467
#define NEXT 468
#define NO 469
#define NOCREATEDB 470
#define NOCREATEROLE 471
#define NOCREATEUSER 472
#define NOINHERIT 473
#define NOLOGIN_P 474
#define NONE 475
#define NOSUPERUSER 476
#define NOT 477
#define NOTHING 478
#define NOTIFY 479
#define NOTNULL 480
#define NOTTRANSITIVE 481
#define NOWAIT 482
#define NULL_P 483
#define NULLIF 484
#define NULLS_P 485
#define NUMERIC 486
#define OBJECT_P 487
#define OF 488
#define OFF 489
#define OFFSET 490
#define OIDS 491
#define OLD 492
#define ON 493
#define ONLY 494
#define OPERATOR 495
#define OPTION 496
#define OR 497
#define ORDER 498
#define OUT_P 499
#define OUTER_P 500
#define OVERLAPS 501
#define OVERLAY 502
#define OWNED 503
#define OWNER 504
#define PARSER 505
#define PARTIAL 506
#define PASSWORD 507
#define PLACING 508
#define PLANS 509
#define POSITION 510
#define PRECISION 511
#define PRESERVE 512
#define PREPARE 513
#define PREPARED 514
#define PRIMARY 515
#define PRIOR 516
#define PRIVILEGES 517
#define PROCEDURAL 518
#define PROCEDURE 519
#define PROVENANCE 520
#define QUOTE 521
#define READ 522
#define REAL 523
#define REASSIGN 524
#define RECHECK 525
#define REFERENCES 526
#define REINDEX 527
#define RELATIVE_P 528
#define RELEASE 529
#define RENAME 530
#define REPEATABLE 531
#define REPLACE 532
#define REPLICA 533
#define RESET 534
#define RESTART 535
#define RESTRICT 536
#define RETURNING 537
#define RETURNS 538
#define REVOKE 539
#define RIGHT 540
#define ROLE 541
#define ROLLBACK 542
#define ROW 543
#define ROWS 544
#define RULE 545
#define SAVEPOINT 546
#define SCHEMA 547
#define SCROLL 548
#define SEARCH 549
#define SECOND_P 550
#define SECURITY 551
#define SELECT 552
#define SEQUENCE 553
#define SERIALIZABLE 554
#define SESSION 555
#define SESSION_USER 556
#define SET 557
#define SETOF 558
#define SHARE 559
#define SHOW 560
#define SIMILAR 561
#define SIMPLE 562
#define SMALLINT 563
#define SOME 564
#define SQLTEXT 565
#define SQLTEXTDB2 566
#define STABLE 567
#define STANDALONE_P 568
#define START 569
#define STATEMENT 570
#define STATISTICS 571
#define STDIN 572
#define STDOUT 573
#define STORAGE 574
#define STRICT_P 575
#define STRIP_P 576
#define SUBSTRING 577
#define SUPERUSER_P 578
#define SYMMETRIC 579
#define SYSID 580
#define SYSTEM_P 581
#define TABLE 582
#define TABLESPACE 583
#define TEMP 584
#define TEMPLATE 585
#define TEMPORARY 586
#define TEXT_P 587
#define THEN 588
#define THIS 589
#define TIME 590
#define TIMESTAMP 591
#define TO 592
#define TRAILING 593
#define TRANSACTION 594
#define TRANSITIVE 595
#define TRANSPROV 596
#define TRANSSQL 597
#define TRANSXML 598
#define TREAT 599
#define TRIGGER 600
#define TRIM 601
#define TRUE_P 602
#define TRUNCATE 603
#define TRUSTED 604
#define TYPE_P 605
#define UNCOMMITTED 606
#define UNENCRYPTED 607
#define UNION 608
#define UNIQUE 609
#define UNKNOWN 610
#define UNLISTEN 611
#define UNTIL 612
#define UPDATE 613
#define USER 614
#define USING 615
#define VACUUM 616
#define VALID 617
#define VALIDATOR 618
#define VALUE_P 619
#define VALUES 620
#define VARCHAR 621
#define VARYING 622
#define VERBOSE 623
#define VERSION_P 624
#define VIEW 625
#define VOLATILE 626
#define WHEN 627
#define WHERE 628
#define WHITESPACE_P 629
#define WITH 630
#define WITHOUT 631
#define WORK 632
#define WRITE 633
#define XML_P 634
#define XMLATTRIBUTES 635
#define XMLCONCAT 636
#define XMLELEMENT 637
#define XMLFOREST 638
#define XMLPARSE 639
#define XMLPI 640
#define XMLROOT 641
#define XMLSERIALIZE 642
#define XSLT 643
#define YEAR_P 644
#define YES_P 645
#define ZONE 646
#define NULLS_FIRST 647
#define NULLS_LAST 648
#define WITH_CASCADED 649
#define WITH_LOCAL 650
#define WITH_CHECK 651
#define IDENT 652
#define FCONST 653
#define SCONST 654
#define BCONST 655
#define XCONST 656
#define Op 657
#define ICONST 658
#define PARAM 659
#define POSTFIXOP 660
#define UMINUS 661
#define TYPECAST 662




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 118 "gram.y"
{
	int					ival;
	char				chr;
	char				*str;
	const char			*keyword;
	bool				boolean;
	JoinType			jtype;
	DropBehavior		dbehavior;
	OnCommitAction		oncommit;
	List				*list;
	Node				*node;
	Value				*value;
	ObjectType			objtype;

	TypeName			*typnam;
	FunctionParameter   *fun_param;
	FunctionParameterMode fun_param_mode;
	FuncWithArgs		*funwithargs;
	DefElem				*defelt;
	SortBy				*sortby;
	JoinExpr			*jexpr;
	IndexElem			*ielem;
	Alias				*alias;
	RangeVar			*range;
	IntoClause			*into;
	A_Indices			*aind;
	ResTarget			*target;
	PrivTarget			*privtarget;

	InsertStmt			*istmt;
	VariableSetStmt		*vsetstmt;
}
/* Line 1489 of yacc.c.  */
#line 896 "y.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE base_yylval;

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

extern YYLTYPE base_yylloc;
