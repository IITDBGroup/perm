/******************************************************************************
*******************************************************************************
*******************************************************************************
******* 	Table Defs     ************************************************
*******************************************************************************
*******************************************************************************
******************************************************************************/

/******************************************************************************
******* 	real world example		   ****************************
******************************************************************************/

DROP TABLE IF EXISTS address CASCADE;
DROP TABLE IF EXISTS employee CASCADE;
DROP TABLE IF EXISTS shop CASCADE;
DROP TABLE IF EXISTS item CASCADE;
DROP TABLE IF EXISTS sales CASCADE;
DROP TABLE IF EXISTS employee_works_at_shop CASCADE;
DROP TABLE IF EXISTS customer CASCADE;

CREATE TABLE address (
	id SERIAL,
	city text,
	street text,
	number int,
	PRIMARY KEY (id)
	);
	
	
CREATE TABLE employee (
	id SERIAL,
	first_name TEXT,
	last_name TEXT,
	salary int,
	PRIMARY KEY (id)
	);
	
CREATE TABLE shop (
	id serial,
	name text,
	location_id int8,
	manager_id int8,
	PRIMARY KEY (id),
	FOREIGN KEY (location_id) REFERENCES address (id),
	FOREIGN KEY (manager_id) REFERENCES employee(id)
	);

	
CREATE TABLE item (
	id SERIAL,
	name text,
	price int,
	cost int,
	PRIMARY KEY (id)
	);
	
CREATE TABLE sales (
	item_id int8,
	shop_id int8,
	number_items int,
	sales_date DATE,
	PRIMARY KEY (shop_id, item_id, sales_date),
	FOREIGN KEY (shop_id) REFERENCES shop (id),
	FOREIGN KEY (item_id) REFERENCES item (id)
	);
	
	
CREATE TABLE employee_works_at_shop (
	employee_id int8,
	shop_id int8,
	PRIMARY KEY (employee_id, shop_id),
	FOREIGN KEY (employee_id) REFERENCES employee (id),
	FOREIGN KEY (shop_id) REFERENCES shop (id)
	);
	
CREATE TABLE customer (
	customer_id SERIAL,
	name text,
	address_id int8,
	PRIMARY KEY (customer_id),
	FOREIGN KEY (address_id) REFERENCES address (id)
	);
	
	
/******************************************************************************
******* 	real world example data		   ****************************
******************************************************************************/


/**** address *****/
INSERT INTO address VALUES (DEFAULT, 'Zürich','Sihlquai',155);
INSERT INTO address VALUES (DEFAULT, 'Zürich','Altstettener Strasse',25);
INSERT INTO address VALUES (DEFAULT, 'Basel','Birmannsgasse',5);
INSERT INTO address VALUES (DEFAULT, 'Bern','Bernerstrasse',109);

INSERT INTO address VALUES (DEFAULT, 'Zürich','Kundenstrasse',23);
INSERT INTO address VALUES (DEFAULT, 'Luzern','Hallostrasse',23);
INSERT INTO address VALUES (DEFAULT, 'Basel','Baumweg',23);
INSERT INTO address VALUES (DEFAULT, 'Basel','Missionsstrasse',24);

/****** employee **/
INSERT INTO employee VALUES (DEFAULT, 'Peter', 'Meter', 10000);
INSERT INTO employee VALUES (DEFAULT, 'Getrud', 'Müller', 8000);
INSERT INTO employee VALUES (DEFAULT, 'Heinz', 'Heller', 15000);
INSERT INTO employee VALUES (DEFAULT, 'Fritz', 'Knoten', 9000);

INSERT INTO employee VALUES (DEFAULT, 'Boris', 'Heinzer', 5000);
INSERT INTO employee VALUES (DEFAULT, 'Gert', 'Gertsen', 12000);
INSERT INTO employee VALUES (DEFAULT, 'Doris', 'Dorissen', 3000);
INSERT INTO employee VALUES (DEFAULT, 'Walter', 'Ott', 2000);

/***** shop *******/
INSERT INTO shop VALUES (DEFAULT, 'Zürich city', 1,1);
INSERT INTO shop VALUES (DEFAULT, 'Zürich Altstetten', 2,2);
INSERT INTO shop VALUES (DEFAULT, 'Basel', 3,3);
INSERT INTO shop VALUES (DEFAULT, 'Bern', 4,4);



/******* employee working at shop ******/
INSERT INTO employee_works_at_shop VALUES (1,1);
INSERT INTO employee_works_at_shop VALUES (2,2);
INSERT INTO employee_works_at_shop VALUES (3,3);
INSERT INTO employee_works_at_shop VALUES (4,4);

INSERT INTO employee_works_at_shop VALUES (5,1);
INSERT INTO employee_works_at_shop VALUES (6,2);
INSERT INTO employee_works_at_shop VALUES (7,2);
INSERT INTO employee_works_at_shop VALUES (8,3);

/******** customer *********************/
INSERT INTO customer VALUES (DEFAULT, 'P. Kunde', 5);
INSERT INTO customer VALUES (DEFAULT, 'J. Kunde', 6);
INSERT INTO customer VALUES (DEFAULT, 'B. Kunde', 7);
INSERT INTO customer VALUES (DEFAULT, 'Q. Kunde', 8);



/**** item *******/
INSERT INTO item VALUES (DEFAULT, 'heckenschere', 25, 13);
INSERT INTO item VALUES (DEFAULT, 'Rasenmäher', 599, 125);
INSERT INTO item VALUES (DEFAULT, 'Gartenschlauch', 19, 5);

/***** sales *****/
INSERT INTO sales VALUES (1,1,25,'2007-01-01');
INSERT INTO sales VALUES (1,1,11,'2007-05-01');
INSERT INTO sales VALUES (1,2,46,'2008-01-01');
INSERT INTO sales VALUES (1,2,2,'2008-10-01');
INSERT INTO sales VALUES (1,3,33,'2007-01-01');
INSERT INTO sales VALUES (1,4,20,'2007-01-01');
INSERT INTO sales VALUES (3,1,6,'2009-01-01');
INSERT INTO sales VALUES (3,4,100,'2009-01-01');
INSERT INTO sales VALUES (3,4,1,'2009-01-25');
INSERT INTO sales VALUES (3,2,13,'2007-01-01');
INSERT INTO sales VALUES (3,2,13,'2010-01-01');
INSERT INTO sales VALUES (3,2,3,'2003-01-15');
INSERT INTO sales VALUES (2,1,20,'2003-01-01');
INSERT INTO sales VALUES (2,3,1,'2002-01-01');
INSERT INTO sales VALUES (2,4,1,'2007-01-01');

/******************************************************************************
******* 	set operations data and table def  ****************************
******************************************************************************/

DROP TABLE IF EXISTS bagdiff1 CASCADE;
DROP TABLE IF EXISTS bagdiff2 CASCADE;
DROP TABLE IF EXISTS bagdiff3 CASCADE;
DROP TABLE IF EXISTS bagdiff4 CASCADE;
DROP TABLE IF EXISTS bagdiff5 CASCADE;

CREATE TABLE bagdiff1 (
	id int
	);

	
INSERT INTO bagdiff1 VALUES (1);
INSERT INTO bagdiff1 VALUES (1);

CREATE TABLE bagdiff2 (
	id int
	);

INSERT INTO bagdiff2 VALUES (1);
	
CREATE TABLE bagdiff3 (
	id int
	);

INSERT INTO bagdiff3 VALUES (1);
INSERT INTO bagdiff3 VALUES (1);
INSERT INTO bagdiff3 VALUES (2);
INSERT INTO bagdiff3 VALUES (3);

CREATE TABLE bagdiff4 (
	id int,
	nonnu int
	);

INSERT INTO bagdiff4 VALUES (1,100);
INSERT INTO bagdiff4 VALUES (1,100);
INSERT INTO bagdiff4 VALUES (null,100);
INSERT INTO bagdiff4 VALUES (null,100);

CREATE TABLE bagdiff5 (
	id int,
	nonnu int
	);


/******************************************************************************
******* 	SPJ and A data and table def	   ****************************
******************************************************************************/

DROP TABLE IF EXISTS muchcols CASCADE;
DROP TABLE IF EXISTS aggr CASCADE;

CREATE TABLE muchcols (
	one int,
	two text,
	three date,
	four int8,
	five text,
	PRIMARY KEY (one, two)
	);
	
INSERT INTO muchcols VALUES (1,'first','2007-01-01'::date, 4321212123545,'hello world');
INSERT INTO muchcols VALUES (2,'second','2007-01-01'::date, 9898989,'hello');
INSERT INTO muchcols VALUES (2,'third','2007-01-01'::date, 212123545,'world');
INSERT INTO muchcols VALUES (3,'fourth','2007-01-01'::date, 12,'hhhhhhhh');

CREATE TABLE aggr (
	id int
	);
	
INSERT INTO aggr VALUES (1);
INSERT INTO aggr VALUES (2);
INSERT INTO aggr VALUES (3);
INSERT INTO aggr VALUES (4);
INSERT INTO aggr VALUES (5);
INSERT INTO aggr VALUES (6);
INSERT INTO aggr VALUES (7);
INSERT INTO aggr VALUES (8);
INSERT INTO aggr VALUES (9);
INSERT INTO aggr VALUES (10);
INSERT INTO aggr VALUES (11);
INSERT INTO aggr VALUES (12);
INSERT INTO aggr VALUES (13);
INSERT INTO aggr VALUES (14);
INSERT INTO aggr VALUES (15);
INSERT INTO aggr VALUES (16);
INSERT INTO aggr VALUES (17);
INSERT INTO aggr VALUES (18);
INSERT INTO aggr VALUES (19);
INSERT INTO aggr VALUES (20);
INSERT INTO aggr VALUES (21);
INSERT INTO aggr VALUES (22);
INSERT INTO aggr VALUES (23);
INSERT INTO aggr VALUES (24);
INSERT INTO aggr VALUES (25);
INSERT INTO aggr VALUES (26);
INSERT INTO aggr VALUES (27);
INSERT INTO aggr VALUES (28);
INSERT INTO aggr VALUES (29);
INSERT INTO aggr VALUES (30);
INSERT INTO aggr VALUES (31);
INSERT INTO aggr VALUES (32);

/******************************************************************************
******* 	unusual SQL features		   ****************************
******************************************************************************/

DROP TABLE IF EXISTS emtpy;
DROP TABLE IF EXISTS r;
DROP TABLE IF EXISTS s;
DROP TABLE IF EXISTS t;

CREATE TABLE empty (
	id int);

CREATE TABLE r (
	i int);

INSERT INTO r VALUES (1);
INSERT INTO r VALUES (2);
INSERT INTO r VALUES (3);

CREATE TABLE s (
	i int);

INSERT INTO s VALUES (1);
INSERT INTO s VALUES (2);
INSERT INTO s VALUES (4);
	
CREATE TABLE t (
	i int);
	
INSERT INTO t VALUES (2);
INSERT INTO t VALUES (4);
INSERT INTO t VALUES (6);
	
/******************************************************************************
******* 	unusual SQL features		   ****************************
******************************************************************************/

DROP TABLE IF EXISTS arraytest;
DROP TABLE IF EXISTS structtest;
DROP TYPE IF EXISTS testtype;

-- struct
CREATE TYPE testtype AS (
	one int,
	two text,
	three int);
	
CREATE TABLE structtest (
	one int,
	two testtype);

INSERT INTO structtest VALUES (1, '(1,"hello",1)');
INSERT INTO structtest VALUES (2, '(2,"hello",1)');
INSERT INTO structtest VALUES (3, '(3,"hello",2)');
	
-- array
CREATE TABLE arraytest (
	id int,
	arr int[]
	);
	
INSERT INTO arraytest VALUES (1, '{1,2,3}');
INSERT INTO arraytest VALUES (2, '{2,4,5,6}');

-- function calls

/******************************************************************************
******* 	views				   ****************************
******************************************************************************/

DROP VIEW IF EXISTS bagdiffivew;
DROP VIEW IF EXISTS complexview;
DROP VIEW IF EXISTS provview1;


CREATE VIEW bagdiffview AS SELECT * FROM bagdiff1;

CREATE VIEW complexview AS SELECT m.one FROM muchcols m, (SELECT  * FROM bagdiff3) AS bag WHERE m.one >= bag.id;

CREATE VIEW provview1 AS SELECT PROVENANCE * FROM bagdiff1;

CREATE VIEW provview2 AS 
	SELECT PROVENANCE zushop.name, topshots.last_name 
	FROM (SELECT * FROM employee WHERE salary > 10000) AS topshots,
		(SELECT * FROM shop WHERE shop.name LIKE '%Z%rich%') AS zushop, 
		employee_works_at_shop wo
	WHERE topshots.id = wo.employee_id
		AND zushop.id = wo.shop_id;
		
-- view with provenance subselect
CREATE VIEW subprovview AS SELECT * FROM (SELECT PROVENANCE * FROM bagdiff1) AS prov WHERE prov_public_bagdiff1_id > 1; --check

/******************************************************************************
*******************************************************************************
*******************************************************************************
******* 	Queries		***********************************************
*******************************************************************************
*******************************************************************************
******************************************************************************/

/******************************************************************************
******* 	SP Queries			   ****************************
******************************************************************************/

SELECT PROVENANCE * FROM bagdiff1;

SELECT PROVENANCE one, four FROM muchcols WHERE one > 1;

SELECT PROVENANCE (one * 100) AS res, two || four, (one * four) AS times FROM muchcols WHERE NOT (one > 1 AND five LIKE '%hhhh%');

/******************************************************************************
******* 	SPJ Queries			   ****************************
******************************************************************************/

-- unrestricted join with projection
SELECT PROVENANCE a.one, b.two FROM muchcols a, muchcols b;

-- equality join
SELECT PROVENANCE a.one, b.two FROM muchcols a, muchcols b WHERE a.one = b.one;

SELECT PROVENANCE a.one, b.* FROM muchcols a, muchcols b WHERE a.one = b.one AND a.two = b.two;

-- theta join
SELECT PROVENANCE a.one, b.two FROM muchcols a, muchcols b WHERE a.one = b.one AND b.four < a.four AND a.five LIKE '%w%';

-- sub queries
SELECT PROVENANCE zushop.name, topshots.last_name 
	FROM (SELECT * FROM employee WHERE salary > 10000) AS topshots,
		(SELECT * FROM shop WHERE shop.name LIKE '%Z%rich%') AS zushop, 
		employee_works_at_shop wo
	WHERE topshots.id = wo.employee_id
		AND zushop.id = wo.shop_id;

-- explicit joins
SELECT PROVENANCE * FROM bagdiff3 a LEFT OUTER JOIN bagdiff1 b ON a.id = b.id;		

SELECT PROVENANCE * FROM bagdiff3 a RIGHT OUTER JOIN bagdiff1 b ON a.id = b.id;

SELECT PROVENANCE * FROM bagdiff2 a FULL OUTER JOIN bagdiff2 b ON a.id = b.id;

SELECT PROVENANCE a.one FROM muchcols a JOIN muchcols b ON a.one < b.four;

SELECT PROVENANCE a.id FROM bagdiff1 a NATURAL JOIN bagdiff2;

SELECT PROVENANCE * FROM bagdiff1 a JOIN bagdiff2 b USING(id);

SELECT PROVENANCE * FROM bagdiff1 NATURAL JOIN bagdiff2 NATURAL JOIN bagdiff3;

SELECT PROVENANCE * FROM bagdiff1 JOIN bagdiff2 USING (id) JOIN bagdiff3 USING (id); 

SELECT PROVENANCE * FROM bagdiff1 a, bagdiff1 NATURAL JOIN (bagdiff2 NATURAL JOIN bagdiff3);		--error

-- explicit joins mixed with subqueries
SELECT PROVENANCE * FROM bagdiff1 a LEFT JOIN (SELECT * FROM bagdiff3) b ON (a.id < b.id);

SELECT PROVENANCE * FROM (SELECT sum(one) AS sum FROM muchcols GROUP BY four) AS suba JOIN (SELECT * FROM muchcols) AS subb ON (sum > subb.one);

SELECT PROVENANCE * FROM r NATURAL JOIN s , t;

SELECT PROVENANCE * FROM r, s NATURAL JOIN t, bagdiff1, s s2 NATURAL JOIN t t2;

/******************************************************************************
******* 	SP Setsem queries			   ****************************
******************************************************************************/		

SELECT PROVENANCE DISTINCT * FROM muchcols;

SELECT PROVENANCE DISTINCT * FROM bagdiff3;

SELECT PROVENANCE DISTINCT one, four FROM muchcols WHERE one > 1;

SELECT PROVENANCE DISTINCT (one * 100) AS res, two || four, (one * four) AS times FROM muchcols WHERE NOT (one > 1 AND five LIKE '%hhhh%');

/******************************************************************************
******* 	SPJ Setsem queries			   ****************************
******************************************************************************/		

-- unrestricted join with projection
SELECT PROVENANCE DISTINCT a.one, b.two FROM muchcols a, muchcols b;

-- equality join
SELECT PROVENANCE DISTINCT a.one, b.two FROM muchcols a, muchcols b WHERE a.one = b.one;

SELECT PROVENANCE DISTINCT  a.one, b.* FROM muchcols a, muchcols b WHERE a.one = b.one AND a.two = b.two;

-- theta join
SELECT PROVENANCE DISTINCT a.one, b.two FROM muchcols a, muchcols b WHERE a.one = b.one AND b.four < a.four AND a.five LIKE '%w%';

-- sub queries
SELECT PROVENANCE DISTINCT zushop.name, topshots.last_name 
	FROM (SELECT * FROM employee WHERE salary > 10000) AS topshots,
		(SELECT * FROM shop WHERE shop.name LIKE '%Z%rich%') AS zushop, 
		employee_works_at_shop wo
	WHERE topshots.id = wo.employee_id
		AND zushop.id = wo.shop_id;
		

/******************************************************************************
******* 	aggregation queries *******************************************
******************************************************************************/

SELECT PROVENANCE sum(one) FROM muchcols;

SELECT PROVENANCE sum(four) FROM muchcols GROUP BY one;

SELECT PROVENANCE sum(four) AS sum FROM muchcols GROUP BY one HAVING sum(four) > 1000;

SELECT PROVENANCE sum(sump) 
	FROM (SELECT sum(i.price) AS sump
		FROM item i,
			sales s
		WHERE i.id = s.item_id
		GROUP BY s.shop_id) AS sub;

-- aggstar
SELECT PROVENANCE count(*) FROM bagdiff3;

--aggdistinct
SELECT PROVENANCE count(DISTINCT id) FROM bagdiff4;

-- group by an many attrs
SELECT PROVENANCE count(*) FROM muchcols GROUP BY one,two, three;

--aggregations in expressions and group by on expressions
SELECT PROVENANCE sum(id) FROM bagdiff3 GROUP BY (id < 3);

SELECT PROVENANCE sum(id) FROM bagdiff3 GROUP BY (id / 5);

SELECT PROVENANCE sum(sub.sum) FROM (SELECT sum(id) AS sum FROM aggr GROUP BY (id / 2)) AS sub;

SELECT PROVENANCE sum(sub.sum) FROM 									
	(SELECT sum(sub.sum),(id / 2) AS id FROM 
		(SELECT sum(id) AS sum,(id / 2) AS id FROM aggr GROUP BY (id / 2)) AS sub 
	GROUP BY (id / 2)) AS sub;

SELECT PROVENANCE sum(sub.sum),(id / 2) AS id FROM							 
		(SELECT sum(id) AS sum,(id / 2) AS id FROM aggr GROUP BY (id / 2)) AS sub 
	GROUP BY sum, (id / 2);
	
	
SELECT PROVENANCE sum(sub.sum) FROM 									
	(SELECT sum(sub.sum),(id / 2) AS id FROM 
		(SELECT sum(sub.sum),(id / 2) AS id FROM 
			(SELECT sum(id) AS sum,(id / 2) AS id FROM aggr GROUP BY (id / 2)) AS sub
		GROUP BY (id / 2)) AS sub
	GROUP BY (id / 2)) AS sub;
;
	
SELECT PROVENANCE sum(sumid) FROM (SELECT sum(sumid) AS sumid FROM (SELECT sum(sumid) AS sumid FROM (SELECT sum(id) AS sumid FROM aggr) AS sub) AS sub) as sub;

SELECT PROVENANCE sum(id * 10) FROM bagdiff1;

SELECT PROVENANCE sum(id) / 30 FROM bagdiff1;

SELECT PROVENANCE sum(id) / avg(id) FROM bagdiff1;	

/************* real world example aggregation queries*************************/

-- total sale per shop
SELECT PROVENANCE sum(sa.price) AS total_sales 
	FROM shop sh,
		(SELECT i.price * s.number_items AS price, shop_id FROM item i, sales s WHERE i.id = s.item_id) AS sa
	WHERE sh.id = sa.shop_id 
	GROUP BY sh.id;
	
-- total profit
SELECT PROVENANCE avg(profit) AS avg_prof 
	FROM 
	(SELECT (sales - costs) AS profit 
		FROM
		(SELECT sum(sa.price) AS sales, sh.id AS shop_id
			FROM shop sh,
				(SELECT i.price * s.number_items AS price, 
					shop_id FROM item i, sales s 
					WHERE i.id = s.item_id) AS sa
			WHERE sh.id = sa.shop_id 
			GROUP BY sh.id) AS sales_per_shop,
		(SELECT sum(em.salary) AS costs, sh.id AS shop_id
			FROM shop sh,
				employee em,
				employee_works_at_shop wo
			WHERE wo.employee_id = em.id AND wo.shop_id = sh.id 
			GROUP BY sh.id) AS costs_per_shop
		WHERE sales_per_shop.shop_id = costs_per_shop.shop_id) as prof;

-- compare with original result
SELECT DISTINCT (avg_prof = orig_avg_prof) FROM 
(SELECT PROVENANCE avg(profit) AS avg_prof 
	FROM 
	(SELECT (sales - costs) AS profit 
		FROM
		(SELECT sum(sa.price) AS sales, sh.id AS shop_id
			FROM shop sh,
				(SELECT i.price * s.number_items AS price, 
					shop_id FROM item i, sales s 
					WHERE i.id = s.item_id) AS sa
			WHERE sh.id = sa.shop_id 
			GROUP BY sh.id) AS sales_per_shop,
		(SELECT sum(em.salary) AS costs, sh.id AS shop_id
			FROM shop sh,
				employee em,
				employee_works_at_shop wo
			WHERE wo.employee_id = em.id AND wo.shop_id = sh.id 
			GROUP BY sh.id) AS costs_per_shop
		WHERE sales_per_shop.shop_id = costs_per_shop.shop_id) as prof
	) AS prov,
(SELECT avg(profit) AS orig_avg_prof 
	FROM 
	(SELECT (sales - costs) AS profit 
		FROM
		(SELECT sum(sa.price) AS sales, sh.id AS shop_id
			FROM shop sh,
				(SELECT i.price * s.number_items AS price,
					shop_id FROM item i, sales s 
					WHERE i.id = s.item_id) AS sa
			WHERE sh.id = sa.shop_id 
			GROUP BY sh.id) AS sales_per_shop,
		(SELECT sum(em.salary) AS costs, sh.id AS shop_id
			FROM shop sh,
				employee em,
				employee_works_at_shop wo
			WHERE wo.employee_id = em.id AND wo.shop_id = sh.id 
			GROUP BY sh.id) AS costs_per_shop
		WHERE sales_per_shop.shop_id = costs_per_shop.shop_id) as prof
	) AS orig;

/************* setsem aggregation queries*************************/
SELECT PROVENANCE DISTINCT sum(id) FROM employee;		

SELECT PROVENANCE DISTINCT * FROM (SELECT sum(id) FROM employee) AS sub;

/******************************************************************************
******* 	set operation Queries		   ****************************
******************************************************************************/

/************* set diff queries				**********************/
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 EXCEPT ALL SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 EXCEPT SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 EXCEPT ALL SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 EXCEPT SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 EXCEPT SELECT * FROM bagdiff1) AS sub;

/************* union					**********************/
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION ALL SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION SELECT * FROM bagdiff3) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff2 UNION ALL SELECT * FROM bagdiff3) AS sub;

/************* intersection				**********************/
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 INTERSECT SELECT * FROM bagdiff1) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 INTERSECT ALL SELECT * FROM bagdiff1) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 INTERSECT SELECT * FROM bagdiff2 INTERSECT SELECT * FROM bagdiff1) AS sub;

-- except with empty set
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 EXCEPT SELECT id FROM bagdiff5) AS sub;

/******** bigger set trees and association of set ops using brackets *********/

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION SELECT * FROM bagdiff2 UNION SELECT * FROM bagdiff3) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION (SELECT * FROM bagdiff2 UNION SELECT * FROM bagdiff3)) AS sub;

-- except except test
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 EXCEPT ALL (SELECT * FROM bagdiff3 EXCEPT ALL SELECT * FROM bagdiff2)) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff2 EXCEPT ALL (SELECT * FROM bagdiff3 EXCEPT SELECT * FROM bagdiff2)) AS sub;

--mixed operators
SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 EXCEPT ALL (SELECT * FROM bagdiff1 UNION ALL SELECT * FROM bagdiff2)) AS sub; 

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 EXCEPT ALL (SELECT * FROM bagdiff1 UNION SELECT * FROM bagdiff2)) AS sub;

SELECT PROVENANCE * FROM ((SELECT * FROM bagdiff1 UNION ALL SELECT * FROM bagdiff2) EXCEPT ALL (SELECT * FROM bagdiff3 UNION ALL SELECT * FROM bagdiff2)) AS sub; 

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff3 UNION (SELECT * FROM bagdiff3 INTERSECT SELECT * FROM bagdiff2)) AS sub;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 INTERSECT (SELECT * FROM bagdiff2 UNION SELECT * FROM bagdiff3)) AS sub;

SELECT PROVENANCE * FROM ((SELECT * FROM bagdiff1 INTERSECT SELECT * FROM bagdiff2) UNION (SELECT * FROM bagdiff2 UNION SELECT * FROM bagdiff3)) AS sub;

/************* real world example set queries ********************************/
SELECT PROVENANCE name 
	FROM (SELECT name, address_id FROM customer 
			UNION
		SELECT name, location_id FROM shop) AS en,
		address a
	WHERE en.address_id = a.id AND a.city = 'Zürich';

/************* set op setsem queries		******************************/

SELECT PROVENANCE DISTINCT * FROM (SELECT * FROM bagdiff2 UNION ALL SELECT * FROM bagdiff3) AS sub;

SELECT PROVENANCE DISTINCT * FROM (SELECT * FROM bagdiff1 EXCEPT ALL SELECT * FROM bagdiff2) AS sub;

SELECT PROVENANCE DISTINCT * FROM ((SELECT * FROM bagdiff1 UNION ALL SELECT * FROM bagdiff2) EXCEPT ALL (SELECT * FROM bagdiff3 UNION ALL SELECT * FROM bagdiff2)) AS sub;

/******************************************************************************
******* complex queries combing subqueries, aggregation and set ops ***********
******************************************************************************/
 
/******************************************************************************
******* sublinks: IN / ANY / ...  without references to super query (uncorrelated subqueries) 
***********************************************************************************/
 
----------------- in where clause ----------------------------------
SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT s.i FROM s);

SELECT PROVENANCE * FROM r WHERE r.i <= ANY (SELECT s.i FROM s);

SELECT PROVENANCE * FROM r WHERE r.i NOT IN (SELECT s.i FROM s);

SELECT PROVENANCE * FROM r WHERE r.i = ALL (SELECT * FROM bagdiff1);

SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id IN (SELECT id FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id NOT IN (SELECT id FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id > ANY (SELECT id + 1 FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id > ALL (SELECT id FROM bagdiff3 WHERE id != 3);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = (SELECT id FROM bagdiff1 LIMIT 1);	
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE 1 IN (SELECT id FROM bagdiff3);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE EXISTS(SELECT id FROM bagdiff3 WHERE id = 3);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = (SELECT DISTINCT id FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = (SELECT max(id) FROM bagdiff3);
 
SELECT PROVENANCE * FROM r a, s b WHERE a.i = b.i AND  a.i IN (SELECT * FROM t);
 
SELECT PROVENANCE a.id FROM bagdiff3 a, bagdiff1 b WHERE a.id = b.id AND a.id = (SELECT max(id) FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = 3 OR a.id IN (SELECT id FROM bagdiff1);
 
SELECT PROVENANCE * FROM bagdiff1 WHERE id < 3 + (SELECT id FROM bagdiff3 LIMIT 1);
 
SELECT PROVENANCE * FROM bagdiff1 WHERE id < 3 + mod((SELECT id FROM bagdiff3 WHERE id = 3 LIMIT 1), 2);
 
SELECT PROVENANCE * FROM r,s,t WHERE r.i IN (SELECT * FROM t);
 
SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT s.i FROM s,t WHERE s.i = t.i);

SELECT PROVENANCE * FROM bagdiff1 WHERE id < ANY (SELECT r.i FROM r,s,t WHERE r.i = s.i AND s.i = t.i);

SELECT PROVENANCE * FROM r,s WHERE r.i = s.i AND r.i IN (SELECT s.i FROM r,s,t WHERE r.i = s.i AND s.i = t.i);			

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT s.i FROM r,s,t,bagdiff3 WHERE r.i = s.i AND s.i = t.i AND bagdiff3.id = s.i);

-- more complex queries in sublink
SELECT PROVENANCE * FROM r WHERE r.i < ANY (SELECT sum(i) FROM s);										

SELECT PROVENANCE * FROM r WHERE r.i < ALL (SELECT count(*) FROM (SELECT * FROM s WHERE s.i = 2) ssub, (SELECT * FROM t) tsub WHERE ssub.i <= tsub.i);

SELECT PROVENANCE * FROM r WHERE EXISTS (SELECT s.i FROM s,t WHERE s.i > 2 AND s.i < t.i);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT s.i FROM s UNION SELECT * FROM t);

--with view
SELECT PROVENANCE * FROM bagdiffview a WHERE a.id < 5 AND a.id IN (SELECT * FROM bagdiff1);

SELECT PROVENANCE * FROM bagdiffview a WHERE a.id < 5 AND a.id IN (SELECT * FROM bagdiffview);

SELECT PROVENANCE * FROM bagdiffview a WHERE a.id IN (SELECT * FROM bagdiffview);
 
-- with empty relation in sublink
SELECT PROVENANCE * FROM r WHERE r.i NOT IN (SELECT * FROM empty);

SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = 3 OR EXISTS (SELECT id FROM bagdiff1 WHERE id = 2);	
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id = 3 OR a.id = (SELECT id FROM bagdiff1 WHERE id = 2);	
 
SELECT PROVENANCE * FROM bagdiff3 a WHERE NOT EXISTS(SELECT id FROM bagdiff3 WHERE id = 4);		

-- sublinks in a subquery
SELECT PROVENANCE * FROM (SELECT * FROM r WHERE r.i IN (SELECT s.i FROM s)) sub;

-- sublinks in a query with provenance subquery
SELECT i, prov_public_r_i, prov_public_s_i FROM (SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s)) sub; --error adapt analyse.c to recognise sublink provenance attrs


-- more than one sublink
SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s) AND r.i IN (SELECT * FROM t);				

SELECT PROVENANCE * FROM bagdiff3 a WHERE (a.id IN (SELECT * FROM bagdiff3)) AND (a.id NOT IN (SELECT * FROM bagdiff1));

SELECT PROVENANCE * FROM r WHERE (SELECT * FROM s WHERE i = 2) IN (SELECT * FROM t);

SELECT PROVENANCE * FROM r, s WHERE r.i = s.i AND r.i IN (SELECT * FROM t) AND s.i IN (SELECT * FROM t);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s) OR r.i IN (SELECT * FROM t);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s) OR r.i + 1 IN (SELECT * FROM t);

SELECT PROVENANCE * FROM r WHERE r.i > ALL (SELECT * FROM s) OR r.i > ALL (SELECT * FROM t);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s) AND r.i + 1 IN (SELECT * FROM t) AND r.i IN (SELECT * FROM bagdiff1);

----------------------- target list -----------------------
SELECT PROVENANCE *, (SELECT DISTINCT id FROM bagdiff1) FROM bagdiff3;

SELECT PROVENANCE *, (SELECT * FROM s WHERE s.i = 4) FROM r;

SELECT PROVENANCE id, (SELECT sum(id) FROM bagdiff3) AS g FROM bagdiff2;
 
SELECT PROVENANCE id, (SELECT id FROM bagdiff3 LIMIT 1) AS g FROM bagdiff2;

--more than one sublink
SELECT PROVENANCE *, (SELECT * FROM s WHERE s.i = 2), r.i IN (SELECT * FROM t) FROM r;

SELECT PROVENANCE *, (SELECT * FROM t WHERE t.i = 4) FROM r,s;

--empty sublink query
SELECT PROVENANCE *, (SELECT * FROM t WHERE t.i > 1000) FROM r;

----------------------- in aggregation -----------------------				--error
SELECT PROVENANCE count(*) FROM bagdiff1 a WHERE a.id < 5 OR a.id IN (SELECT * FROM bagdiff3);
 
SELECT PROVENANCE sum(i + (SELECT * FROM s LIMIT 1)) FROM r;	--error
 
----------------------- having sublink -----------------------				--error
SELECT PROVENANCE sum(id) FROM bagdiff1 HAVING sum(id) > ANY (SELECT * FROM bagdiff3);
 
SELECT PROVENANCE sum(id) FROM bagdiff1 GROUP BY id HAVING id IN (SELECT * FROM bagdiff3);	
 
SELECT PROVENANCE sum(id) FROM bagdiff1 GROUP BY id HAVING id/2 IN (SELECT * FROM bagdiff3);	

SELECT PROVENANCE sum(id) FROM bagdiff1 GROUP BY id HAVING sum(id) IN (SELECT * FROM bagdiff3);	

SELECT PROVENANCE sum(id) * 10 FROM bagdiff1 HAVING avg(id)::int IN (SELECT *  FROM bagdiff3);

SELECT PROVENANCE sum(id * 10), avg(id) FROM bagdiff3 HAVING sum(id) < 20 AND (avg(id) * 10)::int > ANY (SELECT * FROM bagdiff1); 
 
------------------------- group by -----------------------
SELECT PROVENANCE sum(id) FROM bagdiff1 GROUP BY id IN (SELECT * FROM bagdiff3);			--error?

SELECT PROVENANCE sum(id) FROM bagdiff1 GROUP BY id, (SELECT * FROM bagdiff3 LIMIT 1);			--error

--combination of different cases

/******************************************************************************
******* sublinks: IN / ANY / ... nested, uncorrelated subqueries 
***********************************************************************************/

-- nested sublinks
SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT * FROM s WHERE s.i IN (SELECT * FROM t WHERE t.i = t.i));	--error at least provenance attrs off sublink are not correct

 
/******************************************************************************
******* sublinks: IN / ANY / ... with references to super query (correlated subqueries) 
***********************************************************************************/
 
----------------- in where clause ------------------------------------
SELECT PROVENANCE * FROM bagdiff3 a WHERE a.id > ALL (SELECT id FROM bagdiff3 WHERE id != a.id);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT s.i FROM s WHERE s.i = r.i);

SELECT PROVENANCE * FROM r WHERE r.i IN (SELECT r.i FROM s);

SELECT PROVENANCE * FROM r WHERE r.i < ALL (SELECT * FROM s WHERE s.i > r.i);

--more than one sublink
SELECT PROVENANCE * FROM r WHERE r.i <= ALL (SELECT * FROM s WHERE s.i >= r.i) AND r.i IN (SELECT * FROM t);

----------------- in target list -------------------------------------
SELECT PROVENANCE *, (SELECT * FROM s WHERE s.i = r.i) FROM r;							

SELECT PROVENANCE *, r.i IN (SELECT * FROM s WHERE s.i >= r.i) FROM r;

--more than one sublink
SELECT PROVENANCE *, r.i IN (SELECT * FROM s WHERE s.i = r.i), (SELECT * FROM t WHERE r.i = t.i) FROM r;

SELECT PROVENANCE *, r.i IN (SELECT * FROM s WHERE s.i >= r.i), (SELECT * FROM r a WHERE a.i = r.i) FROM r;

/******************************************************************************
******* sublinks: IN / ANY / ... nested, correlated subqueries 
***********************************************************************************/

----------------- in where clause ------------------------------------


/******************************************************************************
******* queries with unusual or postgres specific SQL features		*******
******************************************************************************/

-- array
SELECT PROVENANCE * FROM arraytest;

SELECT PROVENANCE id, arr[1] FROM arraytest WHERE arr[2] < 5;

SELECT PROVENANCE sum(id) FROM arraytest GROUP BY arr[2];

SELECT PROVENANCE sum(arr[3]) FROM arraytest;				

SELECT PROVENANCE a.id FROM arraytest a, arraytest b WHERE a.arr[1] = b.arr[2];

SELECT PROVENANCE * FROM (SELECT arr[1] FROM arraytest UNION SELECT arr[2] FROM arraytest) AS sub;

-- struct
SELECT PROVENANCE one, (two).one FROM structtest WHERE (two).one > 0;

SELECT PROVENANCE sum((two).one) FROM structtest;

SELECT PROVENANCE sum((two).one) FROM structtest GROUP BY (two).three;

SELECT PROVENANCE a.one FROM structtest a, structtest b WHERE (a.two).one = (b.two).three;

SELECT PROVENANCE * FROM (SELECT (two).one FROM structtest UNION SELECT (two).three FROM structtest) AS sub;

--ORDER BY
SELECT PROVENANCE * FROM muchcols ORDER BY one;

SELECT PROVENANCE * FROM (SELECT sum(four) AS sumf FROM  muchcols GROUP BY one) AS t ORDER BY sumf;

SELECT PROVENANCE * FROM (SELECT * FROM bagdiff1 UNION SELECT * FROM bagdiff3) AS sub ORDER BY id;

SELECT PROVENANCE sum(four) AS sumf FROM muchcols GROUP BY one ORDER BY sumf;

-- function calls
SELECT PROVENANCE char_length(two) FROM muchcols;

SELECT PROVENANCE one FROM muchcols WHERE char_length(two) > 5;

SELECT PROVENANCE sum(char_length(two)) FROM muchcols;		

SELECT PROVENANCE sum(one) FROM muchcols GROUP BY char_length(two);

SELECT PROVENANCE a.one FROM muchcols a, muchcols b WHERE char_length(a.two) = char_length(b.two);

SELECT PROVENANCE * FROM (SELECT char_length(two) FROM muchcols UNION SELECT char_length(two) FROM muchcols) AS sub;		

-- constants as TLEs 
SELECT PROVENANCE 1;			--error

SELECT PROVENANCE 1, id FROM bagdiff1;

-- case
SELECT PROVENANCE 100 * (case when id > 2 then 5 else 1 end) FROM bagdiff3;

SELECT PROVENANCE id FROM bagdiff3 WHERE id > (case when id > 2 then 5 else 1 end);

-- constant IN
SELECT PROVENANCE id FROM bagdiff3 WHERE id IN (1,2,10,15);

-- case construct
SELECT PROVENANCE (CASE WHEN (id = 1) THEN 10 ELSE 0 END) FROM bagdiff3;

SELECT PROVENANCE sum(CASE WHEN (id = 1) THEN 10 ELSE 0 END) FROM bagdiff3;

SELECT PROVENANCE sum(id) FROM bagdiff3 WHERE (CASE WHEN (id = 1) THEN true ELSE false END);

/******************************************************************************
******* 	check order of provenance attrs    ****************************
******************************************************************************/

SELECT PROVENANCE i.id, j.id * 10, k.id * 100 FROM bagdiff3 i, bagdiff3 j, bagdiff3 k WHERE i.id = 1 AND j.id = 2 AND k.id = 3;

SELECT PROVENANCE * FROM (SELECT id FROM bagdiff1 UNION SELECT (id * 10) FROM bagdiff1 UNION SELECT (id * 100) FROM bagdiff1) AS sub;

--mixed base relation and subqueries
SELECT PROVENANCE * FROM (SELECT id * 100 FROM bagdiff3 WHERE id = 2) AS i, bagdiff1 AS j;

SELECT PROVENANCE * FROM bagdiff1 AS j, (SELECT id * 100 FROM bagdiff3 WHERE id = 2) AS i;

/******************************************************************************
******* 	check queries on views	and provenance views 	***************
******************************************************************************/
SELECT PROVENANCE * FROM bagdiffview;

SELECT PROVENANCE id FROM bagdiffview WHERE id > 1;

SELECT PROVENANCE * FROM complexview;

SELECT PROVENANCE (one * 100) FROM complexview WHERE one != 0;

SELECT * FROM provview1;

SELECT * FROM provview2;

SELECT id FROM provview1 WHERE prov_public_bagdiff1_id = 1;

SELECT * FROM subprovview;

/******************************************************************************
******* 	Provenance queries as Subqueries	 	***************
******************************************************************************/

SELECT foo.id, foo.prov_public_bagdiff1_id FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;

-- unqualified attr names

SELECT id, prov_public_bagdiff1_id FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;

SELECT id, one, prov_public_bagdiff1_id, prov_public_muchcols_one, prov_public_muchcols_two 
	FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo, (SELECT PROVENANCE * FROM muchcols) AS boo;

-- * expressions

SELECT * FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;

SELECT foo.* FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;

	
-- nested queries
SELECT foo.prov_public_bagdiff1_id FROM (SELECT boo.id, boo.prov_public_bagdiff1_id FROM (SELECT PROVENANCE * FROM bagdiff1) AS boo) AS foo;

-- functions on attributes
SELECT sum(prov_public_bagdiff1_id) FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;	

SELECT foo.prov_public_bagdiff1_id * 10 FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo;	

-- prov attr in WHERE condition
SELECT id, prov_public_bagdiff1_id FROM (SELECT PROVENANCE * FROM bagdiff1) AS foo WHERE foo.prov_public_bagdiff1_id = 3; 

--set ops
SELECT PROVENANCE * FROM bagdiff1 UNION SELECT PROVENANCE * FROM bagdiff2;

--with view unfolding
SELECT * FROM (SELECT PROVENANCE * FROM complexview) AS foo;	

/******************************************************************************
******* 	Restricted rewrites (BASERELATION, PROVENANCE (attr) **********
******************************************************************************/

SELECT PROVENANCE * FROM complexview BASERELATION;

SELECT PROVENANCE * FROM (SELECT (id * 100) AS idcen FROM bagdiff1) BASERELATION AS test;

SELECT PROVENANCE sum(id) FROM provview1 PROVENANCE (prov_public_bagdiff1_id); 

SELECT PROVENANCE * FROM (SELECT PROVENANCE * FROM bagdiff1) PROVENANCE (prov_public_bagdiff1_id) AS bagprov;

SELECT PROVENANCE * FROM (SELECT PROVENANCE sum(id) FROM bagdiff1) PROVENANCE (prov_public_bagdiff1_id) AS bagprov;

SELECT PROVENANCE name || last_name 
	FROM provview2 PROVENANCE (prov_public_employee_id , prov_public_employee_first_name , prov_public_employee_last_name , prov_public_employee_salary ,
				prov_public_shop_id , prov_public_shop_name, prov_public_shop_location_id , prov_public_shop_manager_id ,
				prov_public_employee_works_at_shop_employee_id , prov_public_employee_works_at_shop_shop_id); 

/******************************************************************************
*******************************************************************************
*******************************************************************************
******* 	clean up	***********************************************
*******************************************************************************
*******************************************************************************
******************************************************************************/

/*DROP TABLE IF EXISTS address CASCADE;
DROP TABLE IF EXISTS employee CASCADE;
DROP TABLE IF EXISTS shop CASCADE;
DROP TABLE IF EXISTS item CASCADE;
DROP TABLE IF EXISTS sales CASCADE;
DROP TABLE IF EXISTS employee_works_at_shop CASCADE;
DROP TABLE IF EXISTS customer CASCADE;
DROP TABLE IF EXISTS bagdiff1 CASCADE;
DROP TABLE IF EXISTS bagdiff2 CASCADE;
DROP TABLE IF EXISTS bagdiff3 CASCADE;
DROP TABLE IF EXISTS muchcols CASCADE;
DROP TABLE IF EXISTS arraytest;
DROP TABLE IF EXISTS structtest;

DROP TYPE IF EXISTS testtype;*/