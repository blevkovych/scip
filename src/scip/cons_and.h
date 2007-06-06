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
#pragma ident "@(#) $Id: cons_and.h,v 1.24 2007/06/06 11:25:13 bzfpfend Exp $"

/**@file   cons_and.h
 * @brief  constraint handler for and constraints
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_CONS_AND_H__
#define __SCIP_CONS_AND_H__


#include "scip/scip.h"


/** creates the handler for and constraints and includes it in SCIP */
extern
SCIP_RETCODE SCIPincludeConshdlrAnd(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** creates and captures a and constraint */
extern
SCIP_RETCODE SCIPcreateConsAnd(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS**           cons,               /**< pointer to hold the created constraint */
   const char*           name,               /**< name of constraint */
   SCIP_VAR*             resvar,             /**< resultant variable of the operation */
   int                   nvars,              /**< number of operator variables in the constraint */
   SCIP_VAR**            vars,               /**< array with operator variables of constraint */
   SCIP_Bool             initial,            /**< should the LP relaxation of constraint be in the initial LP? */
   SCIP_Bool             separate,           /**< should the constraint be separated during LP processing? */
   SCIP_Bool             enforce,            /**< should the constraint be enforced during node processing? */
   SCIP_Bool             check,              /**< should the constraint be checked for feasibility? */
   SCIP_Bool             propagate,          /**< should the constraint be propagated during node processing? */
   SCIP_Bool             local,              /**< is constraint only valid locally? */
   SCIP_Bool             modifiable,         /**< is constraint modifiable (subject to column generation)? */
   SCIP_Bool             dynamic,            /**< is constraint subject to aging? */
   SCIP_Bool             removable,          /**< should the relaxation be removed from the LP due to aging or cleanup? */
   SCIP_Bool             stickingatnode      /**< should the node always be kept at the node where it was added, even
                                              *   if it may be moved to a more global node? */
   );

/** gets number of variables in and constraint */
extern
int SCIPgetNVarsAnd(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons                /**< constraint data */
   );

/** gets array of variables in and constraint */
extern
SCIP_VAR** SCIPgetVarsAnd(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons                /**< constraint data */
   );

#endif
