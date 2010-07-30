/*-------------------------------------------------------------------------
 *
 * provstack.h
 *		Interface for stack helper module for provrewrite
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/provstack.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PROVSTACK_H_
#define PROVSTACK_H_

/* global vars */
extern List *baseRelStack;
extern bool baseRelStackActive;
extern int curUniqueAttrNum;
extern int curUniqueRelNum;
extern List *pStack;
extern List *rewriteMethodStack;

/* prototypes */
extern char *appendIdToString (char *string, int *id);
extern void deactiveBaseRelStack (void);
extern void resetUniqueNameGens (void);

/* list searching */
extern int listPositionInt(List *list, int datum);
extern int listPositionExprTargetList (List *list, Node *node);
extern int nodePositionInList (List *list, Node *node);
extern bool nodeInList (List *list, Node *node);
extern bool list_overlap (List *left, List *right);
extern Node *getFirstNodeForPred (List *list, bool (*predicate) (), void *context);

/* stack manipulation */
extern List *getAllUntil (List *stack, int stopElem);
extern List *popAllUntil (List **stack, int stopElem);
extern List *popAllInListAndReverse (List **stack, List *elements);
extern List *popAllInList (List **stack, List *elements);
extern List *getAllInList (List *stack, List *elements);
extern List *popListAndReverse (List **stack, int numElem);
extern List *getListAndReverse (List *stack, int numElem);
extern void *pop (List **list);
extern void *deQueue (List **list);
extern void *popNth (List **list, int n);
extern int popNthInt (List **list, int n);
extern List *push (List **list, void *newElem);
extern List *insert (List **list, void *newElem, int afterPos);
extern void replaceNth (List *list, void *newElem, int pos);
extern List *reverseList (List *list);
extern void reverseCellsInPlace (List *list);
extern List *generateDuplicatesList (void *elem, int count);
extern List *listNthFirstInts (int numElems, int offset);

/* helpers to record the rewrite method used */
extern void addUsedMethod (char *method);

/* list sorting */
extern List *sortIntList (List **list, bool increasing);
extern List *sortList (List **list, int (*compare) (void *left, void *right), bool increasing);

/* comparison functions for sorting */
extern int compareTeOnRessortgroupref (void *left, void *right);
extern int compareVars (void *left, void *right);
extern int compareCopyMapRelEntryOnRtindex (void *left, void *right);

/* list removal */
extern List *removeElems (List **list, List *pos);
extern void removeNodeElem (List *list, Node *node);
extern void removeNodeForPred (List *list, bool (*predicate) (Node *node, void *context), void *context);
extern void removeAfterPos (List *list, int pos);

#endif /*PROVSTACK_H_*/
