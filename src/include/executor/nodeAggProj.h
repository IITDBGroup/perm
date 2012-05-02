/*-------------------------------------------------------------------------
 *
 * nodeAggProj.h
 *	  prototypes for nodeAggProj.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEAGGPROJ_H
#define NODEAGGPROJ_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsAggProj(AggProj *node);
extern AggProjState *ExecInitAggProj(AggProj *node, EState *estate, int eflags);
extern TupleTableSlot *ExecAggProj(AggProjState *node);
extern void ExecEndAggProj(AggProjState *node);
extern void ExecReScanAggProj(AggProjState *node, ExprContext *exprCtxt);

#endif   /* NODEAGGPROJ_H */
