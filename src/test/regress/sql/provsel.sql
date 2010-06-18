
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
	WHERE topshots.id = wo.employee_id AND zushop.id = wo.shop_id;
		

-- explicit joins
SELECT PROVENANCE * FROM bagdiff3 a LEFT OUTER JOIN bagdiff1 b ON a.id = b.id;		

SELECT PROVENANCE * FROM bagdiff3 a RIGHT OUTER JOIN bagdiff1 b ON a.id = b.id;

SELECT PROVENANCE * FROM bagdiff2 a FULL OUTER JOIN bagdiff2 b ON a.id < b.id;

SELECT PROVENANCE a.one FROM muchcols a JOIN muchcols b ON a.one < b.four;

SELECT PROVENANCE a.id FROM bagdiff1 a NATURAL JOIN bagdiff2;

SELECT PROVENANCE * FROM bagdiff1 a JOIN bagdiff2 b USING(id);

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
SELECT PROVENANCE 1;

SELECT PROVENANCE 1, id FROM bagdiff1;

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
SELECT * FROM (SELECT PROVENANCE * FROM complexview) AS foo;	--error

/******************************************************************************
******* 	Restricted rewrites (BASERELATION, PROVENANCE (attr) **********
******************************************************************************/

-- views
SELECT PROVENANCE * FROM  

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
