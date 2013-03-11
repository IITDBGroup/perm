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
     AGGPROJECT = 265,
     AGGREGATE = 266,
     ALL = 267,
     ALSO = 268,
     ALTER = 269,
     ALWAYS = 270,
     ANALYSE = 271,
     ANALYZE = 272,
     AND = 273,
     ANNOT = 274,
     ANY = 275,
     ARRAY = 276,
     AS = 277,
     ASC = 278,
     ASSERTION = 279,
     ASSIGNMENT = 280,
     ASYMMETRIC = 281,
     AT = 282,
     AUTHORIZATION = 283,
     BACKWARD = 284,
     BASERELATION = 285,
     BEFORE = 286,
     BEGIN_P = 287,
     BETWEEN = 288,
     BIGINT = 289,
     BINARY = 290,
     BIT = 291,
     BOOLEAN_P = 292,
     BOTH = 293,
     BY = 294,
     CACHE = 295,
     CALLED = 296,
     CASCADE = 297,
     CASCADED = 298,
     CASE = 299,
     CAST = 300,
     CHAIN = 301,
     CHAR_P = 302,
     CHARACTER = 303,
     CHARACTERISTICS = 304,
     CHECK = 305,
     CHECKPOINT = 306,
     CLASS = 307,
     CLOSE = 308,
     CLUSTER = 309,
     COALESCE = 310,
     COLLATE = 311,
     COLUMN = 312,
     COMMENT = 313,
     COMMIT = 314,
     COMMITTED = 315,
     COMPLETE = 316,
     CONCURRENTLY = 317,
     CONFIGURATION = 318,
     CONNECTION = 319,
     CONSTRAINT = 320,
     CONSTRAINTS = 321,
     CONTENT_P = 322,
     CONTRIBUTION = 323,
     CONVERSION_P = 324,
     COPY = 325,
     COST = 326,
     CREATE = 327,
     CREATEDB = 328,
     CREATEROLE = 329,
     CREATEUSER = 330,
     CROSS = 331,
     CSV = 332,
     CURRENT_P = 333,
     CURRENT_DATE = 334,
     CURRENT_ROLE = 335,
     CURRENT_TIME = 336,
     CURRENT_TIMESTAMP = 337,
     CURRENT_USER = 338,
     CURSOR = 339,
     CYCLE = 340,
     DATABASE = 341,
     DAY_P = 342,
     DEALLOCATE = 343,
     DEC = 344,
     DECIMAL_P = 345,
     DECLARE = 346,
     DEFAULT = 347,
     DEFAULTS = 348,
     DEFERRABLE = 349,
     DEFERRED = 350,
     DEFINER = 351,
     DELETE_P = 352,
     DELIMITER = 353,
     DELIMITERS = 354,
     DESC = 355,
     DICTIONARY = 356,
     DISABLE_P = 357,
     DISCARD = 358,
     DISTINCT = 359,
     DO = 360,
     DOCUMENT_P = 361,
     DOMAIN_P = 362,
     DOUBLE_P = 363,
     DROP = 364,
     EACH = 365,
     ELSE = 366,
     ENABLE_P = 367,
     ENCODING = 368,
     ENCRYPTED = 369,
     END_P = 370,
     ENUM_P = 371,
     ESCAPE = 372,
     EXCEPT = 373,
     EXCLUDING = 374,
     EXCLUSIVE = 375,
     EXECUTE = 376,
     EXISTS = 377,
     EXPLAIN = 378,
     EXTERNAL = 379,
     EXTRACT = 380,
     FALSE_P = 381,
     FAMILY = 382,
     FETCH = 383,
     FIRST_P = 384,
     FLOAT_P = 385,
     FOR = 386,
     FORCE = 387,
     FOREIGN = 388,
     FORWARD = 389,
     FREEZE = 390,
     FROM = 391,
     FULL = 392,
     FUNCTION = 393,
     GENISPROVROW = 394,
     GLOBAL = 395,
     GRANT = 396,
     GRANTED = 397,
     GRAPH = 398,
     GREATEST = 399,
     GROUP_P = 400,
     HANDLER = 401,
     HAVING = 402,
     HEADER_P = 403,
     HOLD = 404,
     HOUR_P = 405,
     HOW = 406,
     IF_P = 407,
     ILIKE = 408,
     IMMEDIATE = 409,
     IMMUTABLE = 410,
     IMPLICIT_P = 411,
     IN_P = 412,
     INCLUDING = 413,
     INCREMENT = 414,
     INDEX = 415,
     INDEXES = 416,
     INFLUENCE = 417,
     INHERIT = 418,
     INHERITS = 419,
     INITIALLY = 420,
     INNER_P = 421,
     INOUT = 422,
     INPUT_P = 423,
     INSENSITIVE = 424,
     INSERT = 425,
     INSTEAD = 426,
     INT_P = 427,
     INTEGER = 428,
     INTERSECT = 429,
     INTERVAL = 430,
     INTO = 431,
     INVOKER = 432,
     INWHERE = 433,
     IS = 434,
     ISNULL = 435,
     ISPROVROWATTRS = 436,
     ISOLATION = 437,
     JOIN = 438,
     KEY = 439,
     LANCOMPILER = 440,
     LANGUAGE = 441,
     LARGE_P = 442,
     LAST_P = 443,
     LEADING = 444,
     LEAST = 445,
     LEFT = 446,
     LEVEL = 447,
     LIKE = 448,
     LIMIT = 449,
     LISTEN = 450,
     LOAD = 451,
     LOCAL = 452,
     LOCALTIME = 453,
     LOCALTIMESTAMP = 454,
     LOCATION = 455,
     LOCK_P = 456,
     LOGIN_P = 457,
     MAPPING = 458,
     MAPPROV = 459,
     MATCH = 460,
     MAXVALUE = 461,
     MINUTE_P = 462,
     MINVALUE = 463,
     MODE = 464,
     MONTH_P = 465,
     MOVE = 466,
     NAME_P = 467,
     NAMES = 468,
     NATIONAL = 469,
     NATURAL = 470,
     NCHAR = 471,
     NEW = 472,
     NEXT = 473,
     NO = 474,
     NOCREATEDB = 475,
     NOCREATEROLE = 476,
     NOCREATEUSER = 477,
     NOINHERIT = 478,
     NOLOGIN_P = 479,
     NONE = 480,
     NOSUPERUSER = 481,
     NOT = 482,
     NOTHING = 483,
     NOTIFY = 484,
     NOTNULL = 485,
     NOTTRANSITIVE = 486,
     NOWAIT = 487,
     NULL_P = 488,
     NULLIF = 489,
     NULLS_P = 490,
     NUMERIC = 491,
     OBJECT_P = 492,
     OF = 493,
     OFF = 494,
     OFFSET = 495,
     OIDS = 496,
     OLD = 497,
     ON = 498,
     ONLY = 499,
     OPERATOR = 500,
     OPTION = 501,
     OR = 502,
     ORDER = 503,
     OUT_P = 504,
     OUTER_P = 505,
     OVERLAPS = 506,
     OVERLAY = 507,
     OWNED = 508,
     OWNER = 509,
     PARSER = 510,
     PARTIAL = 511,
     PASSWORD = 512,
     PLACING = 513,
     PLANS = 514,
     POSITION = 515,
     PRECISION = 516,
     PRESERVE = 517,
     PREPARE = 518,
     PREPARED = 519,
     PRIMARY = 520,
     PRIOR = 521,
     PRIVILEGES = 522,
     PROCEDURAL = 523,
     PROCEDURE = 524,
     PROVENANCE = 525,
     QUOTE = 526,
     READ = 527,
     REAL = 528,
     REASSIGN = 529,
     RECHECK = 530,
     REFERENCES = 531,
     REINDEX = 532,
     RELATIVE_P = 533,
     RELEASE = 534,
     RENAME = 535,
     REPEATABLE = 536,
     REPLACE = 537,
     REPLICA = 538,
     RESET = 539,
     RESTART = 540,
     RESTRICT = 541,
     RETURNING = 542,
     RETURNS = 543,
     REVOKE = 544,
     RIGHT = 545,
     ROLE = 546,
     ROLLBACK = 547,
     ROW = 548,
     ROWS = 549,
     RULE = 550,
     SAVEPOINT = 551,
     SCHEMA = 552,
     SCROLL = 553,
     SEARCH = 554,
     SECOND_P = 555,
     SECURITY = 556,
     SELECT = 557,
     SEQUENCE = 558,
     SERIALIZABLE = 559,
     SESSION = 560,
     SESSION_USER = 561,
     SET = 562,
     SETOF = 563,
     SHARE = 564,
     SHOW = 565,
     SIMILAR = 566,
     SIMPLE = 567,
     SMALLINT = 568,
     SOME = 569,
     SQLTEXT = 570,
     SQLTEXTDB2 = 571,
     STABLE = 572,
     STANDALONE_P = 573,
     START = 574,
     STATEMENT = 575,
     STATISTICS = 576,
     STDIN = 577,
     STDOUT = 578,
     STORAGE = 579,
     STRICT_P = 580,
     STRIP_P = 581,
     SUBSTRING = 582,
     SUPERUSER_P = 583,
     SYMMETRIC = 584,
     SYSID = 585,
     SYSTEM_P = 586,
     TABLE = 587,
     TABLESPACE = 588,
     TEMP = 589,
     TEMPLATE = 590,
     TEMPORARY = 591,
     TEXT_P = 592,
     THEN = 593,
     THIS = 594,
     TIME = 595,
     TIMESTAMP = 596,
     TO = 597,
     TRAILING = 598,
     TRANSACTION = 599,
     TRANSITIVE = 600,
     TRANSPROV = 601,
     TRANSSQL = 602,
     TRANSXML = 603,
     TREAT = 604,
     TRIGGER = 605,
     TRIM = 606,
     TRUE_P = 607,
     TRUNCATE = 608,
     TRUSTED = 609,
     TYPE_P = 610,
     UNCOMMITTED = 611,
     UNENCRYPTED = 612,
     UNION = 613,
     UNIQUE = 614,
     UNKNOWN = 615,
     UNLISTEN = 616,
     UNTIL = 617,
     UPDATE = 618,
     USER = 619,
     USING = 620,
     VACUUM = 621,
     VALID = 622,
     VALIDATOR = 623,
     VALUE_P = 624,
     VALUES = 625,
     VARCHAR = 626,
     VARYING = 627,
     VERBOSE = 628,
     VERSION_P = 629,
     VIEW = 630,
     VOLATILE = 631,
     WHEN = 632,
     WHERE = 633,
     WHITESPACE_P = 634,
     WITH = 635,
     WITHOUT = 636,
     WORK = 637,
     WRITE = 638,
     XML_P = 639,
     XMLATTRIBUTES = 640,
     XMLCONCAT = 641,
     XMLELEMENT = 642,
     XMLFOREST = 643,
     XMLPARSE = 644,
     XMLPI = 645,
     XMLROOT = 646,
     XMLSERIALIZE = 647,
     XSLT = 648,
     YEAR_P = 649,
     YES_P = 650,
     ZONE = 651,
     NULLS_FIRST = 652,
     NULLS_LAST = 653,
     WITH_CASCADED = 654,
     WITH_LOCAL = 655,
     WITH_CHECK = 656,
     IDENT = 657,
     FCONST = 658,
     SCONST = 659,
     BCONST = 660,
     XCONST = 661,
     Op = 662,
     ICONST = 663,
     PARAM = 664,
     POSTFIXOP = 665,
     UMINUS = 666,
     TYPECAST = 667
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
#define AGGPROJECT 265
#define AGGREGATE 266
#define ALL 267
#define ALSO 268
#define ALTER 269
#define ALWAYS 270
#define ANALYSE 271
#define ANALYZE 272
#define AND 273
#define ANNOT 274
#define ANY 275
#define ARRAY 276
#define AS 277
#define ASC 278
#define ASSERTION 279
#define ASSIGNMENT 280
#define ASYMMETRIC 281
#define AT 282
#define AUTHORIZATION 283
#define BACKWARD 284
#define BASERELATION 285
#define BEFORE 286
#define BEGIN_P 287
#define BETWEEN 288
#define BIGINT 289
#define BINARY 290
#define BIT 291
#define BOOLEAN_P 292
#define BOTH 293
#define BY 294
#define CACHE 295
#define CALLED 296
#define CASCADE 297
#define CASCADED 298
#define CASE 299
#define CAST 300
#define CHAIN 301
#define CHAR_P 302
#define CHARACTER 303
#define CHARACTERISTICS 304
#define CHECK 305
#define CHECKPOINT 306
#define CLASS 307
#define CLOSE 308
#define CLUSTER 309
#define COALESCE 310
#define COLLATE 311
#define COLUMN 312
#define COMMENT 313
#define COMMIT 314
#define COMMITTED 315
#define COMPLETE 316
#define CONCURRENTLY 317
#define CONFIGURATION 318
#define CONNECTION 319
#define CONSTRAINT 320
#define CONSTRAINTS 321
#define CONTENT_P 322
#define CONTRIBUTION 323
#define CONVERSION_P 324
#define COPY 325
#define COST 326
#define CREATE 327
#define CREATEDB 328
#define CREATEROLE 329
#define CREATEUSER 330
#define CROSS 331
#define CSV 332
#define CURRENT_P 333
#define CURRENT_DATE 334
#define CURRENT_ROLE 335
#define CURRENT_TIME 336
#define CURRENT_TIMESTAMP 337
#define CURRENT_USER 338
#define CURSOR 339
#define CYCLE 340
#define DATABASE 341
#define DAY_P 342
#define DEALLOCATE 343
#define DEC 344
#define DECIMAL_P 345
#define DECLARE 346
#define DEFAULT 347
#define DEFAULTS 348
#define DEFERRABLE 349
#define DEFERRED 350
#define DEFINER 351
#define DELETE_P 352
#define DELIMITER 353
#define DELIMITERS 354
#define DESC 355
#define DICTIONARY 356
#define DISABLE_P 357
#define DISCARD 358
#define DISTINCT 359
#define DO 360
#define DOCUMENT_P 361
#define DOMAIN_P 362
#define DOUBLE_P 363
#define DROP 364
#define EACH 365
#define ELSE 366
#define ENABLE_P 367
#define ENCODING 368
#define ENCRYPTED 369
#define END_P 370
#define ENUM_P 371
#define ESCAPE 372
#define EXCEPT 373
#define EXCLUDING 374
#define EXCLUSIVE 375
#define EXECUTE 376
#define EXISTS 377
#define EXPLAIN 378
#define EXTERNAL 379
#define EXTRACT 380
#define FALSE_P 381
#define FAMILY 382
#define FETCH 383
#define FIRST_P 384
#define FLOAT_P 385
#define FOR 386
#define FORCE 387
#define FOREIGN 388
#define FORWARD 389
#define FREEZE 390
#define FROM 391
#define FULL 392
#define FUNCTION 393
#define GENISPROVROW 394
#define GLOBAL 395
#define GRANT 396
#define GRANTED 397
#define GRAPH 398
#define GREATEST 399
#define GROUP_P 400
#define HANDLER 401
#define HAVING 402
#define HEADER_P 403
#define HOLD 404
#define HOUR_P 405
#define HOW 406
#define IF_P 407
#define ILIKE 408
#define IMMEDIATE 409
#define IMMUTABLE 410
#define IMPLICIT_P 411
#define IN_P 412
#define INCLUDING 413
#define INCREMENT 414
#define INDEX 415
#define INDEXES 416
#define INFLUENCE 417
#define INHERIT 418
#define INHERITS 419
#define INITIALLY 420
#define INNER_P 421
#define INOUT 422
#define INPUT_P 423
#define INSENSITIVE 424
#define INSERT 425
#define INSTEAD 426
#define INT_P 427
#define INTEGER 428
#define INTERSECT 429
#define INTERVAL 430
#define INTO 431
#define INVOKER 432
#define INWHERE 433
#define IS 434
#define ISNULL 435
#define ISPROVROWATTRS 436
#define ISOLATION 437
#define JOIN 438
#define KEY 439
#define LANCOMPILER 440
#define LANGUAGE 441
#define LARGE_P 442
#define LAST_P 443
#define LEADING 444
#define LEAST 445
#define LEFT 446
#define LEVEL 447
#define LIKE 448
#define LIMIT 449
#define LISTEN 450
#define LOAD 451
#define LOCAL 452
#define LOCALTIME 453
#define LOCALTIMESTAMP 454
#define LOCATION 455
#define LOCK_P 456
#define LOGIN_P 457
#define MAPPING 458
#define MAPPROV 459
#define MATCH 460
#define MAXVALUE 461
#define MINUTE_P 462
#define MINVALUE 463
#define MODE 464
#define MONTH_P 465
#define MOVE 466
#define NAME_P 467
#define NAMES 468
#define NATIONAL 469
#define NATURAL 470
#define NCHAR 471
#define NEW 472
#define NEXT 473
#define NO 474
#define NOCREATEDB 475
#define NOCREATEROLE 476
#define NOCREATEUSER 477
#define NOINHERIT 478
#define NOLOGIN_P 479
#define NONE 480
#define NOSUPERUSER 481
#define NOT 482
#define NOTHING 483
#define NOTIFY 484
#define NOTNULL 485
#define NOTTRANSITIVE 486
#define NOWAIT 487
#define NULL_P 488
#define NULLIF 489
#define NULLS_P 490
#define NUMERIC 491
#define OBJECT_P 492
#define OF 493
#define OFF 494
#define OFFSET 495
#define OIDS 496
#define OLD 497
#define ON 498
#define ONLY 499
#define OPERATOR 500
#define OPTION 501
#define OR 502
#define ORDER 503
#define OUT_P 504
#define OUTER_P 505
#define OVERLAPS 506
#define OVERLAY 507
#define OWNED 508
#define OWNER 509
#define PARSER 510
#define PARTIAL 511
#define PASSWORD 512
#define PLACING 513
#define PLANS 514
#define POSITION 515
#define PRECISION 516
#define PRESERVE 517
#define PREPARE 518
#define PREPARED 519
#define PRIMARY 520
#define PRIOR 521
#define PRIVILEGES 522
#define PROCEDURAL 523
#define PROCEDURE 524
#define PROVENANCE 525
#define QUOTE 526
#define READ 527
#define REAL 528
#define REASSIGN 529
#define RECHECK 530
#define REFERENCES 531
#define REINDEX 532
#define RELATIVE_P 533
#define RELEASE 534
#define RENAME 535
#define REPEATABLE 536
#define REPLACE 537
#define REPLICA 538
#define RESET 539
#define RESTART 540
#define RESTRICT 541
#define RETURNING 542
#define RETURNS 543
#define REVOKE 544
#define RIGHT 545
#define ROLE 546
#define ROLLBACK 547
#define ROW 548
#define ROWS 549
#define RULE 550
#define SAVEPOINT 551
#define SCHEMA 552
#define SCROLL 553
#define SEARCH 554
#define SECOND_P 555
#define SECURITY 556
#define SELECT 557
#define SEQUENCE 558
#define SERIALIZABLE 559
#define SESSION 560
#define SESSION_USER 561
#define SET 562
#define SETOF 563
#define SHARE 564
#define SHOW 565
#define SIMILAR 566
#define SIMPLE 567
#define SMALLINT 568
#define SOME 569
#define SQLTEXT 570
#define SQLTEXTDB2 571
#define STABLE 572
#define STANDALONE_P 573
#define START 574
#define STATEMENT 575
#define STATISTICS 576
#define STDIN 577
#define STDOUT 578
#define STORAGE 579
#define STRICT_P 580
#define STRIP_P 581
#define SUBSTRING 582
#define SUPERUSER_P 583
#define SYMMETRIC 584
#define SYSID 585
#define SYSTEM_P 586
#define TABLE 587
#define TABLESPACE 588
#define TEMP 589
#define TEMPLATE 590
#define TEMPORARY 591
#define TEXT_P 592
#define THEN 593
#define THIS 594
#define TIME 595
#define TIMESTAMP 596
#define TO 597
#define TRAILING 598
#define TRANSACTION 599
#define TRANSITIVE 600
#define TRANSPROV 601
#define TRANSSQL 602
#define TRANSXML 603
#define TREAT 604
#define TRIGGER 605
#define TRIM 606
#define TRUE_P 607
#define TRUNCATE 608
#define TRUSTED 609
#define TYPE_P 610
#define UNCOMMITTED 611
#define UNENCRYPTED 612
#define UNION 613
#define UNIQUE 614
#define UNKNOWN 615
#define UNLISTEN 616
#define UNTIL 617
#define UPDATE 618
#define USER 619
#define USING 620
#define VACUUM 621
#define VALID 622
#define VALIDATOR 623
#define VALUE_P 624
#define VALUES 625
#define VARCHAR 626
#define VARYING 627
#define VERBOSE 628
#define VERSION_P 629
#define VIEW 630
#define VOLATILE 631
#define WHEN 632
#define WHERE 633
#define WHITESPACE_P 634
#define WITH 635
#define WITHOUT 636
#define WORK 637
#define WRITE 638
#define XML_P 639
#define XMLATTRIBUTES 640
#define XMLCONCAT 641
#define XMLELEMENT 642
#define XMLFOREST 643
#define XMLPARSE 644
#define XMLPI 645
#define XMLROOT 646
#define XMLSERIALIZE 647
#define XSLT 648
#define YEAR_P 649
#define YES_P 650
#define ZONE 651
#define NULLS_FIRST 652
#define NULLS_LAST 653
#define WITH_CASCADED 654
#define WITH_LOCAL 655
#define WITH_CHECK 656
#define IDENT 657
#define FCONST 658
#define SCONST 659
#define BCONST 660
#define XCONST 661
#define Op 662
#define ICONST 663
#define PARAM 664
#define POSTFIXOP 665
#define UMINUS 666
#define TYPECAST 667




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
#line 906 "y.tab.h"
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
