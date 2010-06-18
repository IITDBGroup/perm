-- Test basic TRUNCATE functionality.
CREATE TABLE truncate_a (col1 integer primary key);
INSERT INTO truncate_a VALUES (1);
INSERT INTO truncate_a VALUES (2);
SELECT * FROM truncate_a;
-- Roll truncate back
BEGIN;
TRUNCATE truncate_a;
ROLLBACK;
SELECT * FROM truncate_a;
-- Commit the truncate this time
BEGIN;
TRUNCATE truncate_a;
COMMIT;
SELECT * FROM truncate_a;

-- Test foreign-key checks
CREATE TABLE trunc_b (a int REFERENCES truncate_a);
CREATE TABLE trunc_c (a serial PRIMARY KEY);
CREATE TABLE trunc_d (a int REFERENCES trunc_c);
CREATE TABLE trunc_e (a int REFERENCES truncate_a, b int REFERENCES trunc_c);

TRUNCATE TABLE truncate_a;		-- fail
TRUNCATE TABLE truncate_a,trunc_b;		-- fail
TRUNCATE TABLE truncate_a,trunc_b,trunc_e;	-- ok
TRUNCATE TABLE truncate_a,trunc_e;		-- fail
TRUNCATE TABLE trunc_c;		-- fail
TRUNCATE TABLE trunc_c,trunc_d;		-- fail
TRUNCATE TABLE trunc_c,trunc_d,trunc_e;	-- ok
TRUNCATE TABLE trunc_c,trunc_d,trunc_e,truncate_a;	-- fail
TRUNCATE TABLE trunc_c,trunc_d,trunc_e,truncate_a,trunc_b;	-- ok

TRUNCATE TABLE truncate_a RESTRICT; -- fail
TRUNCATE TABLE truncate_a CASCADE;  -- ok

-- circular references
ALTER TABLE truncate_a ADD FOREIGN KEY (col1) REFERENCES trunc_c;

-- Add some data to verify that truncating actually works ...
INSERT INTO trunc_c VALUES (1);
INSERT INTO truncate_a VALUES (1);
INSERT INTO trunc_b VALUES (1);
INSERT INTO trunc_d VALUES (1);
INSERT INTO trunc_e VALUES (1,1);
TRUNCATE TABLE trunc_c;
TRUNCATE TABLE trunc_c,truncate_a;
TRUNCATE TABLE trunc_c,truncate_a,trunc_d;
TRUNCATE TABLE trunc_c,truncate_a,trunc_d,trunc_e;
TRUNCATE TABLE trunc_c,truncate_a,trunc_d,trunc_e,trunc_b;

-- Verify that truncating did actually work
SELECT * FROM truncate_a
   UNION ALL
 SELECT * FROM trunc_c
   UNION ALL
 SELECT * FROM trunc_b
   UNION ALL
 SELECT * FROM trunc_d;
SELECT * FROM trunc_e;

-- Add data again to test TRUNCATE ... CASCADE
INSERT INTO trunc_c VALUES (1);
INSERT INTO truncate_a VALUES (1);
INSERT INTO trunc_b VALUES (1);
INSERT INTO trunc_d VALUES (1);
INSERT INTO trunc_e VALUES (1,1);

TRUNCATE TABLE trunc_c CASCADE;  -- ok

SELECT * FROM truncate_a
   UNION ALL
 SELECT * FROM trunc_c
   UNION ALL
 SELECT * FROM trunc_b
   UNION ALL
 SELECT * FROM trunc_d;
SELECT * FROM trunc_e;

DROP TABLE truncate_a,trunc_c,trunc_b,trunc_d,trunc_e CASCADE;
