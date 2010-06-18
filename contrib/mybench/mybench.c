/*
 * Simple benchmark application that runs queries against a postgres
 * server and measures the respond times.
 *
 *
 *
 *
 * @ Boris Glavic
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "libpq-fe.h"

/* database connection and query execution */
static void connectToDB (void);
static void executeQuery (char *query);
static void executeExplain (char *query);
static void readAndExecuteQueries (void);
/* read input functions */
static char *readQuery (void);
static int isComment(char *line);
static int isSelect(char *query);
static int isWhiteSpace(char c);
static void createFile (char *name);
/* print functions */
static void printResult (PGresult *result, struct timeval start, struct timeval stop, struct timeval stop2, int numTuples);
static void printError (char *query, PGresult *result, int *error);
static void printExplain (PGresult *result);
static char *getTimeDiffString (struct timeval start, struct timeval stop);
/* utils */
static void printUsage(void);
static void getOpts (int argc, char **argv);
/* set Optimization options */
static void setOptions(int true);


/* connection parameters */
char *user;
char *password;
char *host;
char *db;
char *port;

/* optimization options */
char *trueOptions[20];
char *falseOptions[20];

/* connection */
PGconn *conn;
FILE *file;


/* counter for number of executed queries */
int qCounter;
int startQuery;
int maxNumQueries;
int useCursor;
int optionsFalseNum;
int optionsTrueNum;
int useExplain;
int outputExplainPlan;

/*
 * Main method.
 */

int
main (int argc, char** argv)
{
	maxNumQueries = 100000000;
	startQuery = 0;
	useCursor = 0;
	optionsFalseNum = 0;
	optionsTrueNum = 0;
	useExplain = 0;
	outputExplainPlan = 0;

	getOpts(argc, argv);

	connectToDB();

	setOptions(1);
	setOptions(0);

	readAndExecuteQueries();

	PQfinish(conn);

	return 0;
}

static void
readAndExecuteQueries ()
{
	char *query;
	int skip;

	qCounter = 1;
	skip = 0;

	while((query = readQuery()) != NULL && qCounter <= maxNumQueries)
	{
		if (strlen(query) > 0)
		{
			if (skip >= startQuery)
			{
				if (useExplain)
					executeExplain(query);
				else
					executeQuery(query);
			}
			else {
				skip++;
			}
		}
		free(query);
	}
}

static char *
readQuery ()
{
	char *result;
	char line[160];
	char *errorcode;

	result = (char *) malloc(10000);
	result[0] = '\0';
	while((errorcode = fgets(line, 160, stdin)) != NULL && !isComment(line))
	{
		result = strcat(result, line);
	}
	if (errorcode == NULL)
	{
		return NULL;
	}
	return result;
}

static int
isComment(char *line)
{
	return (strncmp(line, "--", 2) == 0);
}

static int
isSelect(char *query) {
	char *subString;
	int result;

	subString = query;
	while(isWhiteSpace(*subString)) {
		subString++;
	}

	result = (strncmp(subString, "SELECT", 6) == 0);
	result = result || (strncmp(subString, "select", 6) == 0);

	return result;
}

static int
isWhiteSpace(char c) {
	switch(c)
	{
	case ' ':
	case '\n':
	case '\t':
		return 1;
	default:
		return 0;
	}
}

static void
executeQuery (char *query)
{
	struct timeval startSecs;
	struct timeval stopSecs;
	struct timeval stopSecs2;
	struct timezone zone;
	int numResults;
	PGresult   *result;
	char *cursorQuery;
	int error;

	error = 0;
	/* use a cursor to fetch the results */
	if (useCursor) {
		numResults = 0;

		/* start timer */
		gettimeofday(&startSecs, &zone);

		if (isSelect(query)) {
			/* start transaction */
			result = PQexec(conn, "BEGIN TRANSACTION;");
			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				printError(query,result, &error);
			}
			PQclear(result);

			/*declare cursor */
			cursorQuery = malloc(80 * sizeof(char) + strlen(query));
			cursorQuery[0] = '\0';
			cursorQuery = strcat(cursorQuery, "DECLARE mycursor BINARY NO SCROLL CURSOR FOR ");
			cursorQuery = strcat(cursorQuery, query);

			result = PQexec(conn, cursorQuery);
			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				printError(query,result, &error);
			}
			PQclear(result);

			/* fetch results in steps of 1000 */
			result = PQexec(conn, "FETCH 1000 FROM mycursor");
			while(PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0)
			{
				numResults = numResults + PQntuples(result);
				/* free result */
				PQclear(result);
				result = PQexec(conn, "FETCH 1000 FROM mycursor");
			}

			if (PQresultStatus(result) == PGRES_TUPLES_OK)
			{
				/* stop timer */
				gettimeofday(&stopSecs, &zone);

				/* print results */
				printResult(result, startSecs, stopSecs, stopSecs, numResults);
			}
			else
			{
				printError(query,result, &error);
			}

			PQclear(result);

			/* release cursor and end transaction */
			result = PQexec(conn, "CLOSE mycursor");
			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				printError(query,result, &error);
			}
			PQclear(result);

			result = PQexec(conn, "END TRANSACTION");
			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				printError(query,result, &error);
			}
			PQclear(result);

			free(cursorQuery);
			qCounter++;
		}
		else {
			/* run query */
			result = PQexec(conn, query);

			if (PQresultStatus(result) != PGRES_COMMAND_OK)
			{
				printError(query,result, &error);
			}

			/* free result */
			PQclear(result);
		}
	}
	/* fetch all results at once */
	else {
		/* start timer */
		gettimeofday(&startSecs, &zone);

		/* run query */
		result = PQexec(conn, query);

		/* stop timer */
		gettimeofday(&stopSecs, &zone);

		/* if no error print query stats */
		if (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			printError(query,result, &error);
			qCounter++;
		}
		else {
			if (PQresultStatus(result) == PGRES_TUPLES_OK)
			{
				if (PQntuples(result) > 0)
				{
					PQgetvalue(result, PQntuples(result) - 1, 0);
				}
				gettimeofday(&stopSecs2, &zone);
				printResult(result, startSecs, stopSecs, stopSecs2, PQntuples(result));
				// increment only if select query
				qCounter++;
			}
		}

		/* free result */
		PQclear(result);
	}

	if (error)
	{
		fprintf(stdout, "ERROR\n");
	}

	//free(query);
}

static void executeExplain (char *query)
{
	PGresult   *result;
	char *explain = NULL;
	int error = 0;

	/* if statement is a select */
	if (isSelect(query)) {

		/* add EXPLAIN to query string */
		explain = (char *) malloc(10000);
		explain[0] = '\0';
		explain = strcat(explain, "EXPLAIN ");
		explain = strcat(explain, query);

		/* execute explain */
		result = PQexec(conn, explain);

		if (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			printError(explain,result, &error);
			qCounter++;
		}
		else {
			if (PQresultStatus(result) == PGRES_TUPLES_OK)
			{
				printExplain(result);
				qCounter++;
			}
		}
	}
	/* no select for example create view. do not use EXPLAIN */
	else
	{
		/* run statement */
		result = PQexec(conn, query);

		/* if no error print query stats */
		if (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			printError(query,result, &error);
			qCounter++;
		}
	}

	/* free result */
	PQclear(result);

	if (error)
		fprintf(stdout, "ERROR\n");

	if (explain)
		free(explain);
}

static void
printError (char *query, PGresult *result, int *error)
{
	*error = 1;
	fprintf(stderr, "$Q(%i): %s\n\nERROR:\n%s\n%s", qCounter, query, PQerrorMessage(conn), PQresultErrorMessage(result));
	fflush(stdout);
}

static void
printResult (PGresult *result, struct timeval start, struct timeval stop, struct timeval stop2, int numTuples)
{
	char *diff1;
	char *diff2;

	diff1 = getTimeDiffString(start, stop);
	diff2 = getTimeDiffString(start, stop2);

	printf("%i,%s,%s,%i,%i\n", qCounter + startQuery, diff1, diff2, PQnfields(result), numTuples);
	fflush(stdout);

	free(diff1);
	free(diff2);
	//sleep(1);
}

static void
printExplain (PGresult *result)
{
	int i;

	if (outputExplainPlan)
	{
		/* print each row of plan into a single line separated by a pipe symbol */
		for(i = 1; i < PQntuples(result); i++)
		{
			printf("%s|",PQgetvalue(result,i,0));
		}

		/* print line break */
		printf("\n");
	}
	else
	{
		/* print result line */
		printf("%s\n",PQgetvalue(result,0,0));
	}
}

static char *
getTimeDiffString (struct timeval start, struct timeval stop)
{
	long secDiff;
	long usecDiff;
	char *result;

	result = (char *) malloc(200);
	usecDiff = stop.tv_usec - start.tv_usec;
	secDiff = stop.tv_sec - start.tv_sec;
	if (usecDiff < 0)
	{
		usecDiff += 1000000;
		secDiff -= 1L;
	}

	sprintf(result,"%li.%06li", secDiff, usecDiff);

	return result;
}

static void
getOpts (int argc, char **argv)
{
	char c;

	/* set default values */

	/* parse options */
	while ((c = getopt(argc, argv, "h:u:p:d:n:s:ct:f:i:P:eo")) != -1)
	{
		switch (c)
		{
			case 'h':
				host = optarg;
				break;
			case 'u':
				user = optarg;
				break;
			case 'p':
				password = optarg;
				break;
			case 'd':
				db = optarg;
				break;
			case 'n':
				maxNumQueries = atoi(optarg);
				break;
			case 's':
				startQuery = atoi(optarg);
				break;
			case 'c':
				useCursor = 1;
				break;
			case 't':
				trueOptions[optionsTrueNum] = optarg;
				optionsTrueNum++;
				break;
			case 'f':
				falseOptions[optionsFalseNum] = optarg;
				optionsFalseNum++;
				break;
			case 'i':
				createFile(optarg);
				break;
			case 'P':
				port = optarg;
				break;
			case 'e':
				useExplain = 1;
				break;
			case 'o':
				outputExplainPlan = 1;
				break;
			default:
				printUsage();
				exit(1);
			break;
		}
	}

}

static void
createFile (char *name) {
	file = fopen(name, "r");
}

static void
connectToDB ()
{
	char *connStr;

	/* create connection string */
	connStr = (char *) malloc(sizeof(char) * 400);
	connStr[0] = ' ';

	if (host)
	{
		connStr = strcat(connStr, " host=");
		connStr = strcat(connStr, host);
	}
	if (db)
	{
		connStr = strcat(connStr, " dbname=");
		connStr = strcat(connStr, db);
	}
	if (user)
	{
		connStr = strcat(connStr, " user=");
		connStr = strcat(connStr, user);
	}
	if (password)
	{
		connStr = strcat(connStr, " password=");
		connStr = strcat(connStr, password);
	}
	if (port)
	{
		connStr = strcat(connStr, " port=");
		connStr = strcat(connStr, port);
	}

	/* try to connect to db */
	conn = PQconnectdb(connStr);

	/* check to see that the backend connection was successfully made */
	if (conn == NULL || PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "$Q(-2):  Connection to database \"%s\" failed:\n%s",
				connStr, PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}
}

static void setOptions (int true)
{
	int i;
	char *query;
	PGresult *result;
	int optionsNum;

	if (true)
		optionsNum = optionsTrueNum;
	else
		optionsNum = optionsFalseNum;

	for (i = 0; i < optionsNum; i++)
	{
		query = (char *) malloc(500 * sizeof(char));
		if (true)
			sprintf(query, "%s%s%s", "SET ", trueOptions[i], " TO TRUE;");
		else
			sprintf(query, "%s%s%s", "SET ", falseOptions[i], " TO FALSE;");

		/* run SET operation */
		result = PQexec(conn, query);

		/* if error print query stats */
		if (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_COMMAND_OK)
		{
			fprintf(stderr, "$Q(-2): SET option :%s\n\nERROR:\n%s\n%s", query, PQerrorMessage(conn), PQresultErrorMessage(result));
			exit(1);
		}

		PQclear(result);
		free(query);
	}
}

static void
printUsage()
{
	fprintf(stderr, "$Q(-2): Usage is: mybench [-c] [-h hostname] [-u username] [-p password] [-d database] "
			"[-n stopAfterNQueries] [-s skipFirstNQueries] [-t option to set true] [-f option to set false] [-e] [-o] \n");
}
