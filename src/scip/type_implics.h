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
#pragma ident "@(#) $Id: type_implics.h,v 1.7 2007/06/06 11:25:29 bzfpfend Exp $"

/**@file   type_implics.h
 * @brief  type definitions for implications, variable bounds, and cliques
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_TYPE_IMPLICS_H__
#define __SCIP_TYPE_IMPLICS_H__


typedef struct SCIP_VBounds SCIP_VBOUNDS;         /**< variable bounds of a variable x in the form x <= c*y or x >= c*y */
typedef struct SCIP_Implics SCIP_IMPLICS;         /**< implications in the form x <= 0 or x >= 1 ==> y <= b or y >= b for x binary, NULL if x nonbinary */
typedef struct SCIP_Clique SCIP_CLIQUE;           /**< single clique, stating that at most one of the binary variables can be fixed
                                              *   to the corresponding value */
typedef struct SCIP_CliqueTable SCIP_CLIQUETABLE; /**< collection of cliques */
typedef struct SCIP_CliqueList SCIP_CLIQUELIST;   /**< list of cliques for a single variable */


#endif
