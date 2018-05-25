/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2018 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   scip_pricer.h
 * @ingroup PUBLICCOREAPI
 * @brief  public methods for variable pricer plugins
 * @author Tobias Achterberg
 * @author Timo Berthold
 * @author Thorsten Koch
 * @author Alexander Martin
 * @author Marc Pfetsch
 * @author Kati Wolter
 * @author Gregor Hendel
 * @author Robert Lion Gottwald
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_SCIP_PRICER_H__
#define __SCIP_SCIP_PRICER_H__


#include <stdio.h>

#include "scip/def.h"
#include "blockmemshell/memory.h"
#include "scip/type_retcode.h"
#include "scip/type_result.h"
#include "scip/type_clock.h"
#include "scip/type_misc.h"
#include "scip/type_timing.h"
#include "scip/type_paramset.h"
#include "scip/type_event.h"
#include "scip/type_lp.h"
#include "scip/type_nlp.h"
#include "scip/type_var.h"
#include "scip/type_prob.h"
#include "scip/type_tree.h"
#include "scip/type_scip.h"

#include "scip/type_bandit.h"
#include "scip/type_branch.h"
#include "scip/type_conflict.h"
#include "scip/type_cons.h"
#include "scip/type_dialog.h"
#include "scip/type_disp.h"
#include "scip/type_heur.h"
#include "scip/type_compr.h"
#include "scip/type_history.h"
#include "scip/type_nodesel.h"
#include "scip/type_presol.h"
#include "scip/type_pricer.h"
#include "scip/type_reader.h"
#include "scip/type_relax.h"
#include "scip/type_sepa.h"
#include "scip/type_table.h"
#include "scip/type_prop.h"
#include "nlpi/type_nlpi.h"
#include "scip/type_concsolver.h"
#include "scip/type_syncstore.h"
#include "scip/type_benders.h"
#include "scip/type_benderscut.h"

/* In debug mode, we include the SCIP's structure in scip.c, such that no one can access
 * this structure except the interface methods in scip.c.
 * In optimized mode, the structure is included in scip.h, because some of the methods
 * are implemented as defines for performance reasons (e.g. the numerical comparisons).
 * Additionally, the internal "set.h" is included, such that the defines in set.h are
 * available in optimized mode.
 */
#ifdef NDEBUG
#include "scip/struct_scip.h"
#include "scip/struct_stat.h"
#include "scip/set.h"
#include "scip/tree.h"
#include "scip/misc.h"
#include "scip/var.h"
#include "scip/cons.h"
#include "scip/solve.h"
#include "scip/debug.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**@addtogroup PublicPricerMethods
 *
 * @{
 */

/** creates a variable pricer and includes it in SCIP
 *  To use the variable pricer for solving a problem, it first has to be activated with a call to SCIPactivatePricer().
 *  This should be done during the problem creation stage.
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 *
 *  @note method has all pricer callbacks as arguments and is thus changed every time a new callback is added
 *        in future releases; consider using SCIPincludePricerBasic() and setter functions
 *        if you seek for a method which is less likely to change in future releases
 */
EXTERN
SCIP_RETCODE SCIPincludePricer(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           name,               /**< name of variable pricer */
   const char*           desc,               /**< description of variable pricer */
   int                   priority,           /**< priority of the variable pricer */
   SCIP_Bool             delay,              /**< should the pricer be delayed until no other pricers or already existing
                                              *   problem variables with negative reduced costs are found?
                                              *   if this is set to FALSE it may happen that the pricer produces columns
                                              *   that already exist in the problem (which are also priced in by the
                                              *   default problem variable pricing in the same round) */
   SCIP_DECL_PRICERCOPY  ((*pricercopy)),    /**< copy method of variable pricer or NULL if you don't want to copy your plugin into sub-SCIPs */
   SCIP_DECL_PRICERFREE  ((*pricerfree)),    /**< destructor of variable pricer */
   SCIP_DECL_PRICERINIT  ((*pricerinit)),    /**< initialize variable pricer */
   SCIP_DECL_PRICEREXIT  ((*pricerexit)),    /**< deinitialize variable pricer */
   SCIP_DECL_PRICERINITSOL((*pricerinitsol)),/**< solving process initialization method of variable pricer */
   SCIP_DECL_PRICEREXITSOL((*pricerexitsol)),/**< solving process deinitialization method of variable pricer */
   SCIP_DECL_PRICERREDCOST((*pricerredcost)),/**< reduced cost pricing method of variable pricer for feasible LPs */
   SCIP_DECL_PRICERFARKAS((*pricerfarkas)),  /**< Farkas pricing method of variable pricer for infeasible LPs */
   SCIP_PRICERDATA*      pricerdata          /**< variable pricer data */
   );

/** creates a variable pricer and includes it in SCIP with all non-fundamental callbacks set to NULL;
 *  if needed, these can be added afterwards via setter functions SCIPsetPricerCopy(), SCIPsetPricerFree(),
 *  SCIPsetPricerInity(), SCIPsetPricerExit(), SCIPsetPricerInitsol(), SCIPsetPricerExitsol(),
 *  SCIPsetPricerFarkas();
 *
 *  To use the variable pricer for solving a problem, it first has to be activated with a call to SCIPactivatePricer().
 *  This should be done during the problem creation stage.
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 *
 *  @note if you want to set all callbacks with a single method call, consider using SCIPincludePricer() instead
 */
EXTERN
SCIP_RETCODE SCIPincludePricerBasic(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER**         pricerptr,          /**< reference to a pricer, or NULL */
   const char*           name,               /**< name of variable pricer */
   const char*           desc,               /**< description of variable pricer */
   int                   priority,           /**< priority of the variable pricer */
   SCIP_Bool             delay,              /**< should the pricer be delayed until no other pricers or already existing
                                              *   problem variables with negative reduced costs are found?
                                              *   if this is set to FALSE it may happen that the pricer produces columns
                                              *   that already exist in the problem (which are also priced in by the
                                              *   default problem variable pricing in the same round) */
   SCIP_DECL_PRICERREDCOST((*pricerredcost)),/**< reduced cost pricing method of variable pricer for feasible LPs */
   SCIP_DECL_PRICERFARKAS((*pricerfarkas)),  /**< Farkas pricing method of variable pricer for infeasible LPs */
   SCIP_PRICERDATA*      pricerdata          /**< variable pricer data */
   );

/** sets copy method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerCopy(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICERCOPY ((*pricercopy))     /**< copy method of pricer or NULL if you don't want to copy your plugin into sub-SCIPs */
   );

/** sets destructor method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerFree(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICERFREE ((*pricerfree))      /**< destructor of pricer */
   );

/** sets initialization method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerInit(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICERINIT  ((*pricerinit))     /**< initialize pricer */
   );

/** sets deinitialization method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerExit(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICEREXIT  ((*pricerexit))     /**< deinitialize pricer */
   );

/** sets solving process initialization method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerInitsol(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICERINITSOL ((*pricerinitsol))/**< solving process initialization method of pricer */
   );

/** sets solving process deinitialization method of pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_INIT
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPsetPricerExitsol(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< pricer */
   SCIP_DECL_PRICEREXITSOL((*pricerexitsol)) /**< solving process deinitialization method of pricer */
   );

/** returns the variable pricer of the given name, or NULL if not existing */
EXTERN
SCIP_PRICER* SCIPfindPricer(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           name                /**< name of variable pricer */
   );

/** returns the array of currently available variable pricers; active pricers are in the first slots of the array */
EXTERN
SCIP_PRICER** SCIPgetPricers(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** returns the number of currently available variable pricers */
EXTERN
int SCIPgetNPricers(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** returns the number of currently active variable pricers, that are used in the LP solving loop */
EXTERN
int SCIPgetNActivePricers(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** sets the priority of a variable pricer */
EXTERN
SCIP_RETCODE SCIPsetPricerPriority(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer,             /**< variable pricer */
   int                   priority            /**< new priority of the variable pricer */
   );

/** activates pricer to be used for the current problem
 *  This method should be called during the problem creation stage for all pricers that are necessary to solve
 *  the problem model.
 *  The pricers are automatically deactivated when the problem is freed.
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_PROBLEM
 */
EXTERN
SCIP_RETCODE SCIPactivatePricer(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer              /**< variable pricer */
   );

/** deactivates pricer
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if SCIP is in one of the following stages:
 *       - \ref SCIP_STAGE_PROBLEM
 *       - \ref SCIP_STAGE_EXITSOLVE
 */
EXTERN
SCIP_RETCODE SCIPdeactivatePricer(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_PRICER*          pricer              /**< variable pricer */
   );

/* @} */

#ifdef __cplusplus
}
#endif

#endif
