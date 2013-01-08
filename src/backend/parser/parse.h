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
     HOW = 404,
     IF_P = 405,
     ILIKE = 406,
     IMMEDIATE = 407,
     IMMUTABLE = 408,
     IMPLICIT_P = 409,
     IN_P = 410,
     INCLUDING = 411,
     INCREMENT = 412,
     INDEX = 413,
     INDEXES = 414,
     INFLUENCE = 415,
     INHERIT = 416,
     INHERITS = 417,
     INITIALLY = 418,
     INNER_P = 419,
     INOUT = 420,
     INPUT_P = 421,
     INSENSITIVE = 422,
     INSERT = 423,
     INSTEAD = 424,
     INT_P = 425,
     INTEGER = 426,
     INTERSECT = 427,
     INTERVAL = 428,
     INTO = 429,
     INVOKER = 430,
     INWHERE = 431,
     IS = 432,
     ISNULL = 433,
     ISOLATION = 434,
     JOIN = 435,
     KEY = 436,
     LANCOMPILER = 437,
     LANGUAGE = 438,
     LARGE_P = 439,
     LAST_P = 440,
     LEADING = 441,
     LEAST = 442,
     LEFT = 443,
     LEVEL = 444,
     LIKE = 445,
     LIMIT = 446,
     LISTEN = 447,
     LOAD = 448,
     LOCAL = 449,
     LOCALTIME = 450,
     LOCALTIMESTAMP = 451,
     LOCATION = 452,
     LOCK_P = 453,
     LOGIN_P = 454,
     MAPPING = 455,
     MAPPROV = 456,
     MATCH = 457,
     MAXVALUE = 458,
     MINUTE_P = 459,
     MINVALUE = 460,
     MODE = 461,
     MONTH_P = 462,
     MOVE = 463,
     NAME_P = 464,
     NAMES = 465,
     NATIONAL = 466,
     NATURAL = 467,
     NCHAR = 468,
     NEW = 469,
     NEXT = 470,
     NO = 471,
     NOCREATEDB = 472,
     NOCREATEROLE = 473,
     NOCREATEUSER = 474,
     NOINHERIT = 475,
     NOLOGIN_P = 476,
     NONE = 477,
     NOSUPERUSER = 478,
     NOT = 479,
     NOTHING = 480,
     NOTIFY = 481,
     NOTNULL = 482,
     NOTTRANSITIVE = 483,
     NOWAIT = 484,
     NULL_P = 485,
     NULLIF = 486,
     NULLS_P = 487,
     NUMERIC = 488,
     OBJECT_P = 489,
     OF = 490,
     OFF = 491,
     OFFSET = 492,
     OIDS = 493,
     OLD = 494,
     ON = 495,
     ONLY = 496,
     OPERATOR = 497,
     OPTION = 498,
     OR = 499,
     ORDER = 500,
     OUT_P = 501,
     OUTER_P = 502,
     OVERLAPS = 503,
     OVERLAY = 504,
     OWNED = 505,
     OWNER = 506,
     PARSER = 507,
     PARTIAL = 508,
     PASSWORD = 509,
     PLACING = 510,
     PLANS = 511,
     POSITION = 512,
     PRECISION = 513,
     PRESERVE = 514,
     PREPARE = 515,
     PREPARED = 516,
     PRIMARY = 517,
     PRIOR = 518,
     PRIVILEGES = 519,
     PROCEDURAL = 520,
     PROCEDURE = 521,
     PROVENANCE = 522,
     QUOTE = 523,
     READ = 524,
     REAL = 525,
     REASSIGN = 526,
     RECHECK = 527,
     REFERENCES = 528,
     REINDEX = 529,
     RELATIVE_P = 530,
     RELEASE = 531,
     RENAME = 532,
     REPEATABLE = 533,
     REPLACE = 534,
     REPLICA = 535,
     RESET = 536,
     RESTART = 537,
     RESTRICT = 538,
     RETURNING = 539,
     RETURNS = 540,
     REVOKE = 541,
     RIGHT = 542,
     ROLE = 543,
     ROLLBACK = 544,
     ROW = 545,
     ROWS = 546,
     RULE = 547,
     SAVEPOINT = 548,
     SCHEMA = 549,
     SCROLL = 550,
     SEARCH = 551,
     SECOND_P = 552,
     SECURITY = 553,
     SELECT = 554,
     SEQUENCE = 555,
     SERIALIZABLE = 556,
     SESSION = 557,
     SESSION_USER = 558,
     SET = 559,
     SETOF = 560,
     SHARE = 561,
     SHOW = 562,
     SIMILAR = 563,
     SIMPLE = 564,
     SMALLINT = 565,
     SOME = 566,
     SQLTEXT = 567,
     SQLTEXTDB2 = 568,
     STABLE = 569,
     STANDALONE_P = 570,
     START = 571,
     STATEMENT = 572,
     STATISTICS = 573,
     STDIN = 574,
     STDOUT = 575,
     STORAGE = 576,
     STRICT_P = 577,
     STRIP_P = 578,
     SUBSTRING = 579,
     SUPERUSER_P = 580,
     SYMMETRIC = 581,
     SYSID = 582,
     SYSTEM_P = 583,
     TABLE = 584,
     TABLESPACE = 585,
     TEMP = 586,
     TEMPLATE = 587,
     TEMPORARY = 588,
     TEXT_P = 589,
     THEN = 590,
     THIS = 591,
     TIME = 592,
     TIMESTAMP = 593,
     TO = 594,
     TRAILING = 595,
     TRANSACTION = 596,
     TRANSITIVE = 597,
     TRANSPROV = 598,
     TRANSSQL = 599,
     TRANSXML = 600,
     TREAT = 601,
     TRIGGER = 602,
     TRIM = 603,
     TRUE_P = 604,
     TRUNCATE = 605,
     TRUSTED = 606,
     TYPE_P = 607,
     UNCOMMITTED = 608,
     UNENCRYPTED = 609,
     UNION = 610,
     UNIQUE = 611,
     UNKNOWN = 612,
     UNLISTEN = 613,
     UNTIL = 614,
     UPDATE = 615,
     USER = 616,
     USING = 617,
     VACUUM = 618,
     VALID = 619,
     VALIDATOR = 620,
     VALUE_P = 621,
     VALUES = 622,
     VARCHAR = 623,
     VARYING = 624,
     VERBOSE = 625,
     VERSION_P = 626,
     VIEW = 627,
     VOLATILE = 628,
     WHEN = 629,
     WHERE = 630,
     WHITESPACE_P = 631,
     WHY = 632,
     WITH = 633,
     WITHOUT = 634,
     WORK = 635,
     WRITE = 636,
     XML_P = 637,
     XMLATTRIBUTES = 638,
     XMLCONCAT = 639,
     XMLELEMENT = 640,
     XMLFOREST = 641,
     XMLPARSE = 642,
     XMLPI = 643,
     XMLROOT = 644,
     XMLSERIALIZE = 645,
     XSLT = 646,
     YEAR_P = 647,
     YES_P = 648,
     ZONE = 649,
     NULLS_FIRST = 650,
     NULLS_LAST = 651,
     WITH_CASCADED = 652,
     WITH_LOCAL = 653,
     WITH_CHECK = 654,
     IDENT = 655,
     FCONST = 656,
     SCONST = 657,
     BCONST = 658,
     XCONST = 659,
     Op = 660,
     ICONST = 661,
     PARAM = 662,
     POSTFIXOP = 663,
     UMINUS = 664,
     TYPECAST = 665
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
#define HOW 404
#define IF_P 405
#define ILIKE 406
#define IMMEDIATE 407
#define IMMUTABLE 408
#define IMPLICIT_P 409
#define IN_P 410
#define INCLUDING 411
#define INCREMENT 412
#define INDEX 413
#define INDEXES 414
#define INFLUENCE 415
#define INHERIT 416
#define INHERITS 417
#define INITIALLY 418
#define INNER_P 419
#define INOUT 420
#define INPUT_P 421
#define INSENSITIVE 422
#define INSERT 423
#define INSTEAD 424
#define INT_P 425
#define INTEGER 426
#define INTERSECT 427
#define INTERVAL 428
#define INTO 429
#define INVOKER 430
#define INWHERE 431
#define IS 432
#define ISNULL 433
#define ISOLATION 434
#define JOIN 435
#define KEY 436
#define LANCOMPILER 437
#define LANGUAGE 438
#define LARGE_P 439
#define LAST_P 440
#define LEADING 441
#define LEAST 442
#define LEFT 443
#define LEVEL 444
#define LIKE 445
#define LIMIT 446
#define LISTEN 447
#define LOAD 448
#define LOCAL 449
#define LOCALTIME 450
#define LOCALTIMESTAMP 451
#define LOCATION 452
#define LOCK_P 453
#define LOGIN_P 454
#define MAPPING 455
#define MAPPROV 456
#define MATCH 457
#define MAXVALUE 458
#define MINUTE_P 459
#define MINVALUE 460
#define MODE 461
#define MONTH_P 462
#define MOVE 463
#define NAME_P 464
#define NAMES 465
#define NATIONAL 466
#define NATURAL 467
#define NCHAR 468
#define NEW 469
#define NEXT 470
#define NO 471
#define NOCREATEDB 472
#define NOCREATEROLE 473
#define NOCREATEUSER 474
#define NOINHERIT 475
#define NOLOGIN_P 476
#define NONE 477
#define NOSUPERUSER 478
#define NOT 479
#define NOTHING 480
#define NOTIFY 481
#define NOTNULL 482
#define NOTTRANSITIVE 483
#define NOWAIT 484
#define NULL_P 485
#define NULLIF 486
#define NULLS_P 487
#define NUMERIC 488
#define OBJECT_P 489
#define OF 490
#define OFF 491
#define OFFSET 492
#define OIDS 493
#define OLD 494
#define ON 495
#define ONLY 496
#define OPERATOR 497
#define OPTION 498
#define OR 499
#define ORDER 500
#define OUT_P 501
#define OUTER_P 502
#define OVERLAPS 503
#define OVERLAY 504
#define OWNED 505
#define OWNER 506
#define PARSER 507
#define PARTIAL 508
#define PASSWORD 509
#define PLACING 510
#define PLANS 511
#define POSITION 512
#define PRECISION 513
#define PRESERVE 514
#define PREPARE 515
#define PREPARED 516
#define PRIMARY 517
#define PRIOR 518
#define PRIVILEGES 519
#define PROCEDURAL 520
#define PROCEDURE 521
#define PROVENANCE 522
#define QUOTE 523
#define READ 524
#define REAL 525
#define REASSIGN 526
#define RECHECK 527
#define REFERENCES 528
#define REINDEX 529
#define RELATIVE_P 530
#define RELEASE 531
#define RENAME 532
#define REPEATABLE 533
#define REPLACE 534
#define REPLICA 535
#define RESET 536
#define RESTART 537
#define RESTRICT 538
#define RETURNING 539
#define RETURNS 540
#define REVOKE 541
#define RIGHT 542
#define ROLE 543
#define ROLLBACK 544
#define ROW 545
#define ROWS 546
#define RULE 547
#define SAVEPOINT 548
#define SCHEMA 549
#define SCROLL 550
#define SEARCH 551
#define SECOND_P 552
#define SECURITY 553
#define SELECT 554
#define SEQUENCE 555
#define SERIALIZABLE 556
#define SESSION 557
#define SESSION_USER 558
#define SET 559
#define SETOF 560
#define SHARE 561
#define SHOW 562
#define SIMILAR 563
#define SIMPLE 564
#define SMALLINT 565
#define SOME 566
#define SQLTEXT 567
#define SQLTEXTDB2 568
#define STABLE 569
#define STANDALONE_P 570
#define START 571
#define STATEMENT 572
#define STATISTICS 573
#define STDIN 574
#define STDOUT 575
#define STORAGE 576
#define STRICT_P 577
#define STRIP_P 578
#define SUBSTRING 579
#define SUPERUSER_P 580
#define SYMMETRIC 581
#define SYSID 582
#define SYSTEM_P 583
#define TABLE 584
#define TABLESPACE 585
#define TEMP 586
#define TEMPLATE 587
#define TEMPORARY 588
#define TEXT_P 589
#define THEN 590
#define THIS 591
#define TIME 592
#define TIMESTAMP 593
#define TO 594
#define TRAILING 595
#define TRANSACTION 596
#define TRANSITIVE 597
#define TRANSPROV 598
#define TRANSSQL 599
#define TRANSXML 600
#define TREAT 601
#define TRIGGER 602
#define TRIM 603
#define TRUE_P 604
#define TRUNCATE 605
#define TRUSTED 606
#define TYPE_P 607
#define UNCOMMITTED 608
#define UNENCRYPTED 609
#define UNION 610
#define UNIQUE 611
#define UNKNOWN 612
#define UNLISTEN 613
#define UNTIL 614
#define UPDATE 615
#define USER 616
#define USING 617
#define VACUUM 618
#define VALID 619
#define VALIDATOR 620
#define VALUE_P 621
#define VALUES 622
#define VARCHAR 623
#define VARYING 624
#define VERBOSE 625
#define VERSION_P 626
#define VIEW 627
#define VOLATILE 628
#define WHEN 629
#define WHERE 630
#define WHITESPACE_P 631
#define WHY 632
#define WITH 633
#define WITHOUT 634
#define WORK 635
#define WRITE 636
#define XML_P 637
#define XMLATTRIBUTES 638
#define XMLCONCAT 639
#define XMLELEMENT 640
#define XMLFOREST 641
#define XMLPARSE 642
#define XMLPI 643
#define XMLROOT 644
#define XMLSERIALIZE 645
#define XSLT 646
#define YEAR_P 647
#define YES_P 648
#define ZONE 649
#define NULLS_FIRST 650
#define NULLS_LAST 651
#define WITH_CASCADED 652
#define WITH_LOCAL 653
#define WITH_CHECK 654
#define IDENT 655
#define FCONST 656
#define SCONST 657
#define BCONST 658
#define XCONST 659
#define Op 660
#define ICONST 661
#define PARAM 662
#define POSTFIXOP 663
#define UMINUS 664
#define TYPECAST 665




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
/* Line 1529 of yacc.c.  */
#line 902 "y.tab.h"
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
