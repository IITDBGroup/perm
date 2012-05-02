
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
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
     GLOBAL = 394,
     GRANT = 395,
     GRANTED = 396,
     GRAPH = 397,
     GREATEST = 398,
     GROUP_P = 399,
     HANDLER = 400,
     HAVING = 401,
     HEADER_P = 402,
     HOLD = 403,
     HOUR_P = 404,
     HOW = 405,
     IF_P = 406,
     ILIKE = 407,
     IMMEDIATE = 408,
     IMMUTABLE = 409,
     IMPLICIT_P = 410,
     IN_P = 411,
     INCLUDING = 412,
     INCREMENT = 413,
     INDEX = 414,
     INDEXES = 415,
     INFLUENCE = 416,
     INHERIT = 417,
     INHERITS = 418,
     INITIALLY = 419,
     INNER_P = 420,
     INOUT = 421,
     INPUT_P = 422,
     INSENSITIVE = 423,
     INSERT = 424,
     INSTEAD = 425,
     INT_P = 426,
     INTEGER = 427,
     INTERSECT = 428,
     INTERVAL = 429,
     INTO = 430,
     INVOKER = 431,
     INWHERE = 432,
     IS = 433,
     ISNULL = 434,
     ISOLATION = 435,
     JOIN = 436,
     KEY = 437,
     LANCOMPILER = 438,
     LANGUAGE = 439,
     LARGE_P = 440,
     LAST_P = 441,
     LEADING = 442,
     LEAST = 443,
     LEFT = 444,
     LEVEL = 445,
     LIKE = 446,
     LIMIT = 447,
     LISTEN = 448,
     LOAD = 449,
     LOCAL = 450,
     LOCALTIME = 451,
     LOCALTIMESTAMP = 452,
     LOCATION = 453,
     LOCK_P = 454,
     LOGIN_P = 455,
     MAPPING = 456,
     MAPPROV = 457,
     MATCH = 458,
     MAXVALUE = 459,
     MINUTE_P = 460,
     MINVALUE = 461,
     MODE = 462,
     MONTH_P = 463,
     MOVE = 464,
     NAME_P = 465,
     NAMES = 466,
     NATIONAL = 467,
     NATURAL = 468,
     NCHAR = 469,
     NEW = 470,
     NEXT = 471,
     NO = 472,
     NOCREATEDB = 473,
     NOCREATEROLE = 474,
     NOCREATEUSER = 475,
     NOINHERIT = 476,
     NOLOGIN_P = 477,
     NONE = 478,
     NOSUPERUSER = 479,
     NOT = 480,
     NOTHING = 481,
     NOTIFY = 482,
     NOTNULL = 483,
     NOTTRANSITIVE = 484,
     NOWAIT = 485,
     NULL_P = 486,
     NULLIF = 487,
     NULLS_P = 488,
     NUMERIC = 489,
     OBJECT_P = 490,
     OF = 491,
     OFF = 492,
     OFFSET = 493,
     OIDS = 494,
     OLD = 495,
     ON = 496,
     ONLY = 497,
     OPERATOR = 498,
     OPTION = 499,
     OR = 500,
     ORDER = 501,
     OUT_P = 502,
     OUTER_P = 503,
     OVERLAPS = 504,
     OVERLAY = 505,
     OWNED = 506,
     OWNER = 507,
     PARSER = 508,
     PARTIAL = 509,
     PASSWORD = 510,
     PLACING = 511,
     PLANS = 512,
     POSITION = 513,
     PRECISION = 514,
     PRESERVE = 515,
     PREPARE = 516,
     PREPARED = 517,
     PRIMARY = 518,
     PRIOR = 519,
     PRIVILEGES = 520,
     PROCEDURAL = 521,
     PROCEDURE = 522,
     PROVENANCE = 523,
     QUOTE = 524,
     READ = 525,
     REAL = 526,
     REASSIGN = 527,
     RECHECK = 528,
     REFERENCES = 529,
     REINDEX = 530,
     RELATIVE_P = 531,
     RELEASE = 532,
     RENAME = 533,
     REPEATABLE = 534,
     REPLACE = 535,
     REPLICA = 536,
     RESET = 537,
     RESTART = 538,
     RESTRICT = 539,
     RETURNING = 540,
     RETURNS = 541,
     REVOKE = 542,
     RIGHT = 543,
     ROLE = 544,
     ROLLBACK = 545,
     ROW = 546,
     ROWS = 547,
     RULE = 548,
     SAVEPOINT = 549,
     SCHEMA = 550,
     SCROLL = 551,
     SEARCH = 552,
     SECOND_P = 553,
     SECURITY = 554,
     SELECT = 555,
     SEQUENCE = 556,
     SERIALIZABLE = 557,
     SESSION = 558,
     SESSION_USER = 559,
     SET = 560,
     SETOF = 561,
     SHARE = 562,
     SHOW = 563,
     SIMILAR = 564,
     SIMPLE = 565,
     SMALLINT = 566,
     SOME = 567,
     SQLTEXT = 568,
     SQLTEXTDB2 = 569,
     STABLE = 570,
     STANDALONE_P = 571,
     START = 572,
     STATEMENT = 573,
     STATISTICS = 574,
     STDIN = 575,
     STDOUT = 576,
     STORAGE = 577,
     STRICT_P = 578,
     STRIP_P = 579,
     SUBSTRING = 580,
     SUPERUSER_P = 581,
     SYMMETRIC = 582,
     SYSID = 583,
     SYSTEM_P = 584,
     TABLE = 585,
     TABLESPACE = 586,
     TEMP = 587,
     TEMPLATE = 588,
     TEMPORARY = 589,
     TEXT_P = 590,
     THEN = 591,
     THIS = 592,
     TIME = 593,
     TIMESTAMP = 594,
     TO = 595,
     TRAILING = 596,
     TRANSACTION = 597,
     TRANSITIVE = 598,
     TRANSPROV = 599,
     TRANSSQL = 600,
     TRANSXML = 601,
     TREAT = 602,
     TRIGGER = 603,
     TRIM = 604,
     TRUE_P = 605,
     TRUNCATE = 606,
     TRUSTED = 607,
     TYPE_P = 608,
     UNCOMMITTED = 609,
     UNENCRYPTED = 610,
     UNION = 611,
     UNIQUE = 612,
     UNKNOWN = 613,
     UNLISTEN = 614,
     UNTIL = 615,
     UPDATE = 616,
     USER = 617,
     USING = 618,
     VACUUM = 619,
     VALID = 620,
     VALIDATOR = 621,
     VALUE_P = 622,
     VALUES = 623,
     VARCHAR = 624,
     VARYING = 625,
     VERBOSE = 626,
     VERSION_P = 627,
     VIEW = 628,
     VOLATILE = 629,
     WHEN = 630,
     WHERE = 631,
     WHITESPACE_P = 632,
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
#define GLOBAL 394
#define GRANT 395
#define GRANTED 396
#define GRAPH 397
#define GREATEST 398
#define GROUP_P 399
#define HANDLER 400
#define HAVING 401
#define HEADER_P 402
#define HOLD 403
#define HOUR_P 404
#define HOW 405
#define IF_P 406
#define ILIKE 407
#define IMMEDIATE 408
#define IMMUTABLE 409
#define IMPLICIT_P 410
#define IN_P 411
#define INCLUDING 412
#define INCREMENT 413
#define INDEX 414
#define INDEXES 415
#define INFLUENCE 416
#define INHERIT 417
#define INHERITS 418
#define INITIALLY 419
#define INNER_P 420
#define INOUT 421
#define INPUT_P 422
#define INSENSITIVE 423
#define INSERT 424
#define INSTEAD 425
#define INT_P 426
#define INTEGER 427
#define INTERSECT 428
#define INTERVAL 429
#define INTO 430
#define INVOKER 431
#define INWHERE 432
#define IS 433
#define ISNULL 434
#define ISOLATION 435
#define JOIN 436
#define KEY 437
#define LANCOMPILER 438
#define LANGUAGE 439
#define LARGE_P 440
#define LAST_P 441
#define LEADING 442
#define LEAST 443
#define LEFT 444
#define LEVEL 445
#define LIKE 446
#define LIMIT 447
#define LISTEN 448
#define LOAD 449
#define LOCAL 450
#define LOCALTIME 451
#define LOCALTIMESTAMP 452
#define LOCATION 453
#define LOCK_P 454
#define LOGIN_P 455
#define MAPPING 456
#define MAPPROV 457
#define MATCH 458
#define MAXVALUE 459
#define MINUTE_P 460
#define MINVALUE 461
#define MODE 462
#define MONTH_P 463
#define MOVE 464
#define NAME_P 465
#define NAMES 466
#define NATIONAL 467
#define NATURAL 468
#define NCHAR 469
#define NEW 470
#define NEXT 471
#define NO 472
#define NOCREATEDB 473
#define NOCREATEROLE 474
#define NOCREATEUSER 475
#define NOINHERIT 476
#define NOLOGIN_P 477
#define NONE 478
#define NOSUPERUSER 479
#define NOT 480
#define NOTHING 481
#define NOTIFY 482
#define NOTNULL 483
#define NOTTRANSITIVE 484
#define NOWAIT 485
#define NULL_P 486
#define NULLIF 487
#define NULLS_P 488
#define NUMERIC 489
#define OBJECT_P 490
#define OF 491
#define OFF 492
#define OFFSET 493
#define OIDS 494
#define OLD 495
#define ON 496
#define ONLY 497
#define OPERATOR 498
#define OPTION 499
#define OR 500
#define ORDER 501
#define OUT_P 502
#define OUTER_P 503
#define OVERLAPS 504
#define OVERLAY 505
#define OWNED 506
#define OWNER 507
#define PARSER 508
#define PARTIAL 509
#define PASSWORD 510
#define PLACING 511
#define PLANS 512
#define POSITION 513
#define PRECISION 514
#define PRESERVE 515
#define PREPARE 516
#define PREPARED 517
#define PRIMARY 518
#define PRIOR 519
#define PRIVILEGES 520
#define PROCEDURAL 521
#define PROCEDURE 522
#define PROVENANCE 523
#define QUOTE 524
#define READ 525
#define REAL 526
#define REASSIGN 527
#define RECHECK 528
#define REFERENCES 529
#define REINDEX 530
#define RELATIVE_P 531
#define RELEASE 532
#define RENAME 533
#define REPEATABLE 534
#define REPLACE 535
#define REPLICA 536
#define RESET 537
#define RESTART 538
#define RESTRICT 539
#define RETURNING 540
#define RETURNS 541
#define REVOKE 542
#define RIGHT 543
#define ROLE 544
#define ROLLBACK 545
#define ROW 546
#define ROWS 547
#define RULE 548
#define SAVEPOINT 549
#define SCHEMA 550
#define SCROLL 551
#define SEARCH 552
#define SECOND_P 553
#define SECURITY 554
#define SELECT 555
#define SEQUENCE 556
#define SERIALIZABLE 557
#define SESSION 558
#define SESSION_USER 559
#define SET 560
#define SETOF 561
#define SHARE 562
#define SHOW 563
#define SIMILAR 564
#define SIMPLE 565
#define SMALLINT 566
#define SOME 567
#define SQLTEXT 568
#define SQLTEXTDB2 569
#define STABLE 570
#define STANDALONE_P 571
#define START 572
#define STATEMENT 573
#define STATISTICS 574
#define STDIN 575
#define STDOUT 576
#define STORAGE 577
#define STRICT_P 578
#define STRIP_P 579
#define SUBSTRING 580
#define SUPERUSER_P 581
#define SYMMETRIC 582
#define SYSID 583
#define SYSTEM_P 584
#define TABLE 585
#define TABLESPACE 586
#define TEMP 587
#define TEMPLATE 588
#define TEMPORARY 589
#define TEXT_P 590
#define THEN 591
#define THIS 592
#define TIME 593
#define TIMESTAMP 594
#define TO 595
#define TRAILING 596
#define TRANSACTION 597
#define TRANSITIVE 598
#define TRANSPROV 599
#define TRANSSQL 600
#define TRANSXML 601
#define TREAT 602
#define TRIGGER 603
#define TRIM 604
#define TRUE_P 605
#define TRUNCATE 606
#define TRUSTED 607
#define TYPE_P 608
#define UNCOMMITTED 609
#define UNENCRYPTED 610
#define UNION 611
#define UNIQUE 612
#define UNKNOWN 613
#define UNLISTEN 614
#define UNTIL 615
#define UPDATE 616
#define USER 617
#define USING 618
#define VACUUM 619
#define VALID 620
#define VALIDATOR 621
#define VALUE_P 622
#define VALUES 623
#define VARCHAR 624
#define VARYING 625
#define VERBOSE 626
#define VERSION_P 627
#define VIEW 628
#define VOLATILE 629
#define WHEN 630
#define WHERE 631
#define WHITESPACE_P 632
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
{

/* Line 1676 of yacc.c  */
#line 118 "gram.y"

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



/* Line 1676 of yacc.c  */
#line 907 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
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

