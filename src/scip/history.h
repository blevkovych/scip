/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2007 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2007 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: history.h,v 1.25 2007/06/06 11:25:18 bzfpfend Exp $"

/**@file   history.h
 * @brief  internal methods for branching and inference history
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_HISTORY_H__
#define __SCIP_HISTORY_H__


#include "scip/def.h"
#include "blockmemshell/memory.h"
#include "scip/type_retcode.h"
#include "scip/type_set.h"
#include "scip/type_history.h"

#ifdef NDEBUG
#include "scip/struct_history.h"
#endif



/** creates an empty history entry */
extern
SCIP_RETCODE SCIPhistoryCreate(
   SCIP_HISTORY**        history,            /**< pointer to store branching and inference history */
   BMS_BLKMEM*           blkmem              /**< block memory */
   );

/** frees a history entry */
extern
void SCIPhistoryFree(
   SCIP_HISTORY**        history,            /**< pointer to branching and inference history */
   BMS_BLKMEM*           blkmem              /**< block memory */
   );

/** resets history entry to zero */
extern
void SCIPhistoryReset(
   SCIP_HISTORY*         history             /**< branching and inference history */
   );

/** unites two history entries by adding the values of the second one to the first one */
extern
void SCIPhistoryUnite(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_HISTORY*         addhistory,         /**< history values to add to history */
   SCIP_Bool             switcheddirs        /**< should the history entries be united with switched directories */
   );
   
/** updates the pseudo costs for a change of "solvaldelta" in the variable's LP solution value and a change of "objdelta"
 *  in the LP's objective value
 */
extern
void SCIPhistoryUpdatePseudocost(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_Real             solvaldelta,        /**< difference of variable's new LP value - old LP value */
   SCIP_Real             objdelta,           /**< difference of new LP's objective value - old LP's objective value */
   SCIP_Real             weight              /**< weight of this update in pseudo cost sum (added to pscostcount) */
   );


#ifndef NDEBUG

/* In debug mode, the following methods are implemented as function calls to ensure
 * type validity.
 */

/** returns the opposite direction of the given branching direction */
extern
SCIP_BRANCHDIR SCIPbranchdirOpposite(
   SCIP_BRANCHDIR        dir                 /**< branching direction */
   );

/** returns the expected dual gain for moving the corresponding variable by "solvaldelta" */
extern
SCIP_Real SCIPhistoryGetPseudocost(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_Real             solvaldelta         /**< difference of variable's new LP value - old LP value */
   );

/** returns the (possible fractional) number of (partial) pseudo cost updates performed on this pseudo cost entry in 
 *  the given branching direction
 */
extern
SCIP_Real SCIPhistoryGetPseudocostCount(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** returns whether the pseudo cost entry is empty in the given branching direction (whether no value was added yet) */
extern
SCIP_Bool SCIPhistoryIsPseudocostEmpty(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** increases the conflict score of the history entry by the given weight */
extern
void SCIPhistoryIncConflictScore(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir,                /**< branching direction */
   SCIP_Real             weight              /**< weight of this update in conflict score */
   );

/** scales the conflict score values with the given scalar */
extern
void SCIPhistoryScaleConflictScores(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_Real             scalar              /**< scalar to multiply the conflict scores with */
   );

/** gets the conflict score of the history entry */
extern
SCIP_Real SCIPhistoryGetConflictScore(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction */
   );

/** increases the number of branchings counter */
extern
void SCIPhistoryIncNBranchings(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   int                   depth,              /**< depth at which the bound change took place */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** increases the number of inferences counter */
extern
void SCIPhistoryIncNInferences(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** increases the number of cutoffs counter */
extern
void SCIPhistoryIncNCutoffs(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** get number of branchings counter */
extern
SCIP_Longint SCIPhistoryGetNBranchings(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** get number of inferences counter */
extern
SCIP_Longint SCIPhistoryGetNInferences(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** returns the average number of inferences per branching */
extern
SCIP_Real SCIPhistoryGetAvgInferences(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** get number of cutoffs counter */
extern
SCIP_Longint SCIPhistoryGetNCutoffs(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** returns the average number of cutoffs per branching */
extern
SCIP_Real SCIPhistoryGetAvgCutoffs(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** returns the average depth of bound changes due to branching */
extern
SCIP_Real SCIPhistoryGetAvgBranchdepth(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

#else

/* In optimized mode, the methods are implemented as defines to reduce the number of function calls and
 * speed up the algorithms.
 */

#define SCIPbranchdirOpposite(dir)                                      \
   ((dir) == SCIP_BRANCHDIR_DOWNWARDS ? SCIP_BRANCHDIR_UPWARDS          \
      : ((dir) == SCIP_BRANCHDIR_UPWARDS ? SCIP_BRANCHDIR_DOWNWARDS : SCIP_BRANCHDIR_AUTO))
#define SCIPhistoryGetPseudocost(history,solvaldelta)                   \
   ( (solvaldelta) >= 0.0 ? (solvaldelta) * ((history)->pscostcount[1] > 0.0 \
      ? (history)->pscostsum[1] / (history)->pscostcount[1] : 1.0)      \
      : -(solvaldelta) * ((history)->pscostcount[0] > 0.0               \
         ? (history)->pscostsum[0] / (history)->pscostcount[0] : 1.0) )
#define SCIPhistoryGetPseudocostCount(history,dir) ((history)->pscostcount[dir])
#define SCIPhistoryIsPseudocostEmpty(history,dir)  ((history)->pscostcount[dir] == 0.0)
#define SCIPhistoryIncConflictScore(history,dir,weight) (history)->conflictscore[dir] += (weight)
#define SCIPhistoryScaleConflictScores(history,scalar) { (history)->conflictscore[0] *= (scalar); \
      (history)->conflictscore[1] *= (scalar); }
#define SCIPhistoryGetConflictScore(history,dir)   ((history)->conflictscore[dir])
#define SCIPhistoryIncNBranchings(history,depth,dir) { (history)->nbranchings[dir]++; \
      (history)->branchdepthsum[dir] += depth; }
#define SCIPhistoryIncNInferences(history,dir)     (history)->ninferences[dir]++
#define SCIPhistoryIncNCutoffs(history,dir)        (history)->ncutoffs[dir]++
#define SCIPhistoryGetNBranchings(history,dir)     ((history)->nbranchings[dir])
#define SCIPhistoryGetNInferences(history,dir)     ((history)->ninferences[dir])
#define SCIPhistoryGetAvgInferences(history,dir)   ((history)->nbranchings[dir] > 0 \
      ? (SCIP_Real)(history)->ninferences[dir]/(SCIP_Real)(history)->nbranchings[dir] : 0)
#define SCIPhistoryGetNCutoffs(history,dir)        ((history)->ncutoffs[dir])
#define SCIPhistoryGetAvgCutoffs(history,dir)      ((history)->nbranchings[dir] > 0 \
      ? (SCIP_Real)(history)->ncutoffs[dir]/(SCIP_Real)(history)->nbranchings[dir] : 0)
#define SCIPhistoryGetAvgBranchdepth(history,dir)  ((history)->nbranchings[dir] > 0 \
      ? (SCIP_Real)(history)->branchdepthsum[dir]/(SCIP_Real)(history)->nbranchings[dir] : 1)

#endif


#endif
