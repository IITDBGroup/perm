/*-------------------------------------------------------------------------
 *
 * provstack.c
 *	  POSTGRES C provenance stack and list helper module
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provstack.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"					// needed for all stuff

#include "nodes/nodes.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "utils/lsyscache.h"
#include "nodes/value.h"

/* global vars */
List *baseRelStack;
List *pStack;
bool baseRelStackActive = false;
int curUniqueAttrNum;
int curUniqueRelNum;
List *rewriteMethodStack;

/* prototypes */
static ListCell *popNthCell (List ** list, int n);


/*
 * Reset the unique id generators
 */
void
resetUniqueNameGens (void)
{
	curUniqueAttrNum = 1;
	curUniqueRelNum = 1;
}

/*
 *
 */
char *
appendIdToString (char *string, int *id)
{
	char *result;
	char *numString;
	int newLen;

	numString = palloc((sizeof(int)) * 8 + 1);
	sprintf(numString, "%i",*id);
	newLen = strlen(string) + strlen(numString) + 1;

	result = palloc(newLen);
	result = strcpy(result, string);
	result = strcat(result, numString);

	(*id)++;
	pfree(numString);

	return result;
}

/*
 * Deactives the baseRelStack by removing all elements from the stack and set baseRelStackActive to false.
 * This method should be called after a top level sublink was processed.
 */

void
deactiveBaseRelStack (void)
{
	baseRelStack = NIL;
	baseRelStackActive = false;
}

/*
 * Return the position of a Target entry from list that has a expr field that equals node.
 */

int
listPositionExprTargetList (List *list, Node *node)
{
	ListCell *lc;
	TargetEntry *te;
	Index count;

	count = 0;
	foreach(lc, list)
	{
		te = (TargetEntry *) lfirst(lc);
		if (equal(te->expr,node))
			return count;
		count++;
	}

	return -1;
}

/*
 * searches for an int value in an int list. If the int value is
 * found it's position in the list (starting at 0) is returned. Otherwise -1 is returned.
 */

int
listPositionInt(List *list, int datum)
{
	ListCell   *cell;
	int count;

	count = 0;
	foreach(cell, list)
	{
		if (lfirst_int(cell) == datum)
			return count;
		count++;
	}

	return -1;
}

/*
 * Searches for an Node in a list. Equality is checked using the equal macro.
 * If the node is found its position in the list is returned, else -1 is returned.
 */

int
nodePositionInList (List *list, Node *node)
{
	ListCell *lc;
	Node *check;
	Index position;


	for(lc = list->head, position = 0; lc != NULL; lc = lc->next, position++)
	{
		check = (Node *) lfirst(lc);
		if (equal(node, check))
			return position;
	}

	return -1;
}

/*
 * Searches for an Node in a list. Equality is checked using the equal macro.
 */

bool
nodeInList (List *list, Node *node)
{
	return nodePositionInList (list, node) != -1;
}

/*
 * Returns two if lists left and right have at least on common element (determined using equal).
 */

bool
list_overlap (List *left, List *right)
{
	ListCell *lc;
	ListCell *innerLc;
	void *leftElem;
	void *rightElem;

	foreach(lc, left)
	{
		leftElem = lfirst(lc);

		foreach(innerLc, right)
		{
			rightElem = lfirst(innerLc);

			if(equal(leftElem, rightElem))
				return true;
		}
	}

	return false;
}

/*
 * Search for an node in a list that fulfills an given predicate. The first node that fulfills the predicate is returned.
 * (NULL else).
 */

Node *
getFirstNodeForPred (List *list, bool (*predicate) (), void *context)
{
	ListCell *lc;
	Node *elem;

	foreach(lc, list)
	{
		elem = (Node *) lfirst(lc);

		if (predicate(elem, context))
			return elem;
	}

	return NULL;
}

/*
 * Gets elements from a stack until the nth element is reached (counting from the bottom of the stack) without changing the stack.
 * The poped elements are returned as a list in reversed order.
 */

List *
getAllUntil (List *stack, int stopElem)
{
	int numElem;

	numElem = list_length(stack) - stopElem;
	return getListAndReverse (stack, numElem);
}


/*
 * Gets n elements from a stack and returns them as a list without changing the stack. The elements in the list appear
 *  in the order they have been pushed on the stack.
 */

List *
getListAndReverse (List *stack, int numElem)
{
	List* result;
	int i;

	Assert(list_length(stack) >= numElem);

	LOGDEBUG("gLaR    -- START");

	result = NIL;
	for (i = 0; i < numElem; i++)
	{
		result = lcons(list_nth(stack,i),result);
	}

	logPList(result);

	logDebug("gLaR    -- FINISHED");

	return result;
}

/*
 * Pops elements from a stack until the nth element is reached (counting from the bottom of the stack). The poped elements
 * are returned as a list in reversed order.
 */


List *
popAllUntil (List **stack, int stopElem)
{
	int numElem;

	numElem = list_length(*stack) - stopElem;
	return popListAndReverse (stack, numElem);
}

/*
 * Pops n elements from a stack and returns them as a list. The elements in the list appear
 *  in the order they have been pushed on the stack.
 */

List *
popListAndReverse (List **stack, int numElem)
{
	List* result;
	int i;

	Assert(list_length(*stack) >= numElem);

	result = NIL;
	for (i = 0; i < numElem; i++)
	{
		result = lcons(pop(stack),result);
	}

	logPList(result);

	return result;
}

/*
 * Pop all elements at the positions stored in the elements list. Return these as a list but in reversed order.
 */

List *
popAllInListAndReverse (List **stack, List *elements)
{
	ListCell *lc;
	List *result;
	List *reversed;
	void *curElem;

	result = popAllInList (stack, elements);
	reversed = NIL;

	/* reverse list */
	foreach(lc, result)
	{
		curElem = lfirst(lc);
		reversed = lcons(curElem, reversed);
	}

	return reversed;
}

/*
 * Pop all elements at the positions stored in the elements list. Return these as a list
 */

List *
popAllInList (List **stack, List *elements)
{
	ListCell *lc;
	List *result;
	List *remove;
	int curElem;

	result = NIL;

	/* first build up result and then remove elements */
	foreach(lc, elements)
	{
		curElem = lfirst_int(lc);
		result = lappend(result, list_nth(*stack, curElem));
	}

	remove = copyObject(elements);
	sortIntList(&remove, false);

	foreach(lc, remove)
	{
		curElem = lfirst_int(lc);
		popNth(stack, curElem);
	}

	return result;
}

/*
 *
 */
List *
getAllInList (List *list, List *elements)
{
	ListCell *lc;
	List *result;
	int element;

	result = NIL;

	foreach(lc, elements)
	{
		element = lfirst_int(lc);
		result = lappend(result, list_nth(list, element));
	}

	return result;
}

/*
 * returns and removes the first element of a List
 */

void *
pop (List **list)
{
	void *firstElem;

	Assert(*list != NIL);

	firstElem = linitial (*list);
	*list = list_delete_first((*list));
	return firstElem;
}

/*
 * returns and removes the last element of a List
 */

void *
deQueue (List **list)
{
	void *lastElem;
	ListCell *lc;

	Assert(*list != NIL);

	lastElem = lfirst((*list)->tail);

	/* find element before last element */
	for(lc = (*list)->head; lc->next != NULL; lc = lc->next);

	/* remove last element */
	(*list)->tail = lc;
	lc->next = NULL;

	return lastElem;
}

/*
 *	Removes the n-th element from list and returns it. Elements are counted starting with 0.
 */

int
popNthInt (List **list, int n)
{
	ListCell *lc;

	lc = popNthCell (list, n);
	return lc->data.int_value;
}

/*
 * Removes the n-th element from list and returns it. Elements are counted starting with 0.
 */

void *
popNth (List **list, int n)
{
	ListCell *lc;

	lc = popNthCell (list, n);

	return lc->data.ptr_value;
}

/*
 * Removes and returns the nth cell of a list. Cells are counted starting with 0.
 */

static ListCell *
popNthCell (List ** list, int n)
{
	ListCell *lc;
	ListCell *nth;

	Assert(n >= 0 && list_length(*list) > n);

	(*list)->length = (*list)->length - 1;
	lc = (*list)->head;
	/* remove head */
	if (n == 0)
	{
		(*list)->head = lc->next;
		/* only case where list can become empty */
		if ((*list)->length == 0)
		{
			*list = NIL;
		}
		return lc->data.ptr_value;
	}
	/* not head find nth element */
	while (n > 1)
	{
		lc = lc->next;
		n--;
	}
	nth = lc->next;

	/* remove nth element */
	lc->next = nth->next;

	/* is last then adapt tail */
	if (nth == (*list)->tail)
	{
		(*list)->tail = lc;
	}

	return nth;
}

/*
 * Pushes an pointer on a stack (a list used as a stack)
 */

List *
push (List **list, void *newElem)
{
	*list = lcons(newElem, *list);
	return *list;
}

/*
 * Inserts a new element into list after position "afterPos".
 */

List *
insert (List **list, void *newElem, int afterPos)
{
	ListCell *lc;
	ListCell *newCell;
	int i;

	Assert(afterPos < list_length(*list));

	for(i = 0, lc = (*list)->head; i < afterPos; i++, lc = lc->next);

	newCell = (ListCell *) palloc(sizeof(ListCell));
	newCell->data.ptr_value = newElem;

	newCell->next = lc->next;
	lc->next = newCell;

	return *list;
}

/*
 *
 */

void
replaceNth (List *list, void *newElem, int pos)
{
	ListCell *lc;
	int i;

	Assert(pos < list_length(list) && pos >= 0);

	for(i = 0, lc = list->head; i < pos; i++, lc = lc->next);

	lc->data.ptr_value = newElem;
}

/*
 * Sorts a list of integers. In increasing or decreasing order.
 */

List *
sortIntList (List **list, bool increasing)	//OPTIMIZE use more efficient sort method instead of bubble sort
{
	ListCell *lc;
	int i, j, sortTemp;
	bool switchElems;

	for (i = 0; i < list_length(*list); i++)
	{
		lc = (*list)->head;
		for (j = 0; j < list_length(*list) - 1; j++)
		{
			if (increasing)
			{
				switchElems = lfirst_int(lc) > lfirst_int(lc->next);
			}
			else
			{
				switchElems = lfirst_int(lc) < lfirst_int(lc->next);
			}

			if (switchElems)
			{
				sortTemp = lfirst_int(lc);
				lfirst_int(lc) = lfirst_int(lc->next);
				lfirst_int(lc->next) = sortTemp;
			}
			lc = lc->next;
		}
	}

	return *list;
}

/*
 * sorts a pointer list using the provided comparison function to compare the list elements.
 */

List *
sortList (List **list, int (*compare) (void *left, void *right), bool increasing)
{
	ListCell *lc;
	int i,j;
	void *sortTemp;
	int comparison, factor;

	factor = increasing ? 1 : -1;

	for (i = 0; i < list_length(*list); i++)
	{
		lc = (*list)->head;

		for(j = 0; j < list_length(*list) - 1; j++)
		{
			comparison = compare (lfirst(lc), lfirst(lc->next)) * factor;

			if (comparison == 1)
			{
				sortTemp = lfirst(lc);
				lfirst(lc) = lfirst(lc->next);
				lfirst(lc->next) = sortTemp;
			}

			lc = lc->next;
		}
	}

	return *list;
}

/*
 * Compare the attribute numbers of Var nodes.
 */

int
compareVars (void *left, void *right)
{
	Var *l;
	Var *r;

	l = (Var *) left;
	r = (Var *) right;

	if (l->varattno < r->varattno)
		return -1;
	else if (l->varattno > r->varattno)
		return 1;
	return 0;
}

/*
 * Compares target entries on their ressortgroupref value
 */

int
compareTeOnRessortgroupref (void *left, void *right)
{
	TargetEntry *l;
	TargetEntry *r;

	l = (TargetEntry *) left;
	r = (TargetEntry *) right;

	if (l->ressortgroupref < r->ressortgroupref)
		return -1;
	else if (l->ressortgroupref > r->ressortgroupref)
		return 1;
	return 0;
}

/*
 *
 */

int
compareCopyMapRelEntryOnRtindex (void *left, void *right)
{
	CopyMapRelEntry *l;
	CopyMapRelEntry *r;

	l = (CopyMapRelEntry *) left;
	r = (CopyMapRelEntry *) right;

	if (l->rtindex < r->rtindex)
		return -1;
	else if (l->rtindex > r->rtindex)
		return 1;
	return 0;
}

/*
 * Returns a new list that is a copy of parameter list, but with reversed element order.
 */

List *
reverseList (List *list)
{
	ListCell *lc;
	List *result;

	result = NIL;

	foreach(lc, result)
	{
		result = lcons(lfirst(lc), result);
	}

	return result;
}

/*
 * Reverses a list in place
 */

void
reverseCellsInPlace (List *list)
{
	ListCell *lc;
	ListCell *last;
	ListCell *new;

	lc = list->head;
	last = lc;
	list->head = list->tail;
	list->tail = last;

	if (list_length(list) == 2)
	{
		list->head->next = list->tail;
		list->tail->next = NULL;
		return;
	}

	lc = lc->next;
	while(lc != NULL)
	{
		new = lc;
		lc = lc->next;

		new->next = last;
		last = new;
	}

	list->tail->next = NULL;
}

/*
 * generates a list that has "count" elements, each of them is a copy of elem.
 */

List *
generateDuplicatesList (void *elem, int count)
{
	List *result;
	int i;

	result = NIL;

	for(i = 0; i < count; i++)
	{
		result = lappend(result, elem);
	}

	return result;
}

/*
 *
 */

List *
listNthFirstInts (int numElems, int offset)
{
	List *result;

	result = NIL;

	while(numElems-- > 0)
	{
		result = lcons_int(numElems + offset, result);
	}

	return result;
}


/*
 * Appends the name of a rewrite strategy used during a rewrite. This is used by explain to output the
 * strategies that were applied to a query during rewrite.
 */

void
addUsedMethod (char *method)
{
	rewriteMethodStack = lappend(rewriteMethodStack, makeString(pstrdup(method)));
}

/*
 * Removes the elements from list specified as position in list "pos".
 */

List *
removeElems (List **list, List *pos)
{
	ListCell *lc;
	ListCell *before;
	ListCell *posLc;
	int i;

	/* if remove list is empty return */
	if (list_length(pos) == 0)
		return *list;

	/* sort ascending */
	pos = sortIntList(&pos, false);
	posLc = pos->head;
	before = NULL;

	/* walk trough list, keeping track of current position, and remove any cell at a position from pos */
	foreachi(lc, i, *list)
	{
		if (i == lfirst_int(posLc))	//TODO free cell
		{
			if (before != NULL)
				before->next = lc->next;
			else
				(*list)->head = lc->next;

			posLc = posLc->next;
			if (posLc != NULL)
				break;
		}

		before = lc;
	}

	return *list;
}

/*
 * Removes a single element from a list. No error is raised if the element is not found.
 */

void
removeNodeElem (List *list, Node *node)
{
	ListCell *lc;
	ListCell *before;
	Node *curNode;

	before = NULL;

	foreach(lc, list)
	{
		curNode = (Node *) lfirst(lc);

		if (equal(node,curNode))
		{
			if (before != NULL)
				before->next = lc->next;
			else
				list->head = lc->next;

			return;
		}

		before = lc;
	}
}

/*
 * Removes all elements from a list that fulfill the provided predicate function.
 */

void
removeNodeForPred (List *list, bool (*predicate) (Node *node, void *context), void *context)
{
	ListCell *lc;
	ListCell *before;
	Node *curNode;

	before = NULL;

	foreach(lc, list)
	{
		curNode = (Node *) lfirst(lc);

		if (predicate(curNode, context))
		{
			if (before != NULL)
				before->next = lc->next;
			else
				list->head = lc->next;

			return;
		}

		before = lc;
	}
}

/*
 * Remove all list elements after position pos
 */

void
removeAfterPos (List *list, int pos)
{
	ListCell *lc;
	int i;

	foreachi(lc, i , list)
	{
		if (i == pos - 1)
		{
			lc->next = NULL;
			list->tail = lc;
			list->length = pos;
			break;
		}
	}
}

