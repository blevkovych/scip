/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2016 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License.             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   objheur.h
 * @brief  C++ wrapper for primal heuristics
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_OBJHEUR_H__
#define __SCIP_OBJHEUR_H__

#include <cstring>

#include "scip/scip.h"
#include "objscip/objcloneable.h"

namespace scip
{

/** @brief C++ wrapper for primal heuristics
 *
 *  This class defines the interface for primal heuristics implemented in C++. Note that there is a pure virtual
 *  function (this function has to be implemented). This function is: scip_exec().
 *
 *  - \ref HEUR "Instructions for implementing a primal heuristic"
 *  - \ref PRIMALHEURISTICS "List of available primal heuristics"
 *  - \ref type_heur.h "Corresponding C interface"
 */
class ObjHeur : public ObjCloneable
{
public:
   /*lint --e{1540}*/

   /** SCIP data structure */
   SCIP* scip_;

   /** name of the primal heuristic */
   char* scip_name_;

   /** description of the primal heuristic */
   char* scip_desc_;

   /** display character of primal heuristic */
   const char scip_dispchar_;

   /** default priority of the primal heuristic */
   const int scip_priority_;

   /** frequency for calling primal heuristic */
   const int scip_freq_;

   /** frequency offset for calling primal heuristic */
   const int scip_freqofs_;

   /** maximal depth level to call heuristic at (-1: no limit) */
   const int scip_maxdepth_;

   /** positions in the node solving loop where heuristic should be executed */
   const unsigned int scip_timingmask_;

   /** does the heuristic use a secondary SCIP instance? */
   const SCIP_Bool scip_usessubscip_;

   /** default constructor */
   ObjHeur(
      SCIP*              scip,               /**< SCIP data structure */
      const char*        name,               /**< name of primal heuristic */
      const char*        desc,               /**< description of primal heuristic */
      char               dispchar,           /**< display character of primal heuristic */
      int                priority,           /**< priority of the primal heuristic */
      int                freq,               /**< frequency for calling primal heuristic */
      int                freqofs,            /**< frequency offset for calling primal heuristic */
      int                maxdepth,           /**< maximal depth level to call heuristic at (-1: no limit) */
      unsigned int       timingmask,         /**< positions in the node solving loop where heuristic should be executed;
                                              *   see definition of SCIP_HeurTiming for possible values */
      SCIP_Bool          usessubscip         /**< does the heuristic use a secondary SCIP instance? */
      )
      : scip_(scip),
        scip_name_(0),
        scip_desc_(0),
        scip_dispchar_(dispchar),
        scip_priority_(priority),
        scip_freq_(freq),
        scip_freqofs_(freqofs),
        scip_maxdepth_(maxdepth),
        scip_timingmask_(timingmask),
        scip_usessubscip_(usessubscip)
   {
      /* the macro SCIPduplicateMemoryArray does not need the first argument: */
      SCIP_CALL_ABORT( SCIPduplicateMemoryArray(scip_, &scip_name_, name, std::strlen(name)+1) );
      SCIP_CALL_ABORT( SCIPduplicateMemoryArray(scip_, &scip_desc_, desc, std::strlen(desc)+1) );
   }

   /** destructor */
   virtual ~ObjHeur()
   {
      /* the macro SCIPfreeMemoryArray does not need the first argument: */
      /*lint --e{64}*/
      SCIPfreeMemoryArray(scip_, &scip_name_);
      SCIPfreeMemoryArray(scip_, &scip_desc_);
   }

   /** destructor of primal heuristic to free user data (called when SCIP is exiting)
    *
    *  @see SCIP_DECL_HEURFREE(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEURFREE(scip_free)
   {  /*lint --e{715}*/
      return SCIP_OKAY;
   }

   /** initialization method of primal heuristic (called after problem was transformed)
    *
    *  @see SCIP_DECL_HEURINIT(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEURINIT(scip_init)
   {  /*lint --e{715}*/
      return SCIP_OKAY;
   }

   /** deinitialization method of primal heuristic (called before transformed problem is freed)
    *
    *  @see SCIP_DECL_HEUREXIT(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEUREXIT(scip_exit)
   {  /*lint --e{715}*/
      return SCIP_OKAY;
   }

   /** solving process initialization method of primal heuristic (called when branch and bound process is about to begin)
    *
    *  @see SCIP_DECL_HEURINITSOL(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEURINITSOL(scip_initsol)
   {  /*lint --e{715}*/
      return SCIP_OKAY;
   }

   /** solving process deinitialization method of primal heuristic (called before branch and bound process data is freed)
    *
    *  @see SCIP_DECL_HEUREXITSOL(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEUREXITSOL(scip_exitsol)
   {  /*lint --e{715}*/
      return SCIP_OKAY;
   }

   /** execution method of primal heuristic
    *
    *  @see SCIP_DECL_HEUREXEC(x) in @ref type_heur.h
    */
   virtual SCIP_DECL_HEUREXEC(scip_exec) = 0;
};

} /* namespace scip */



/** creates the primal heuristic for the given primal heuristic object and includes it in SCIP
 *
 *  The method should be called in one of the following ways:
 *
 *   1. The user is resposible of deleting the object:
 *       SCIP_CALL( SCIPcreate(&scip) );
 *       ...
 *       MyHeur* myheur = new MyHeur(...);
 *       SCIP_CALL( SCIPincludeObjHeur(scip, &myheur, FALSE) );
 *       ...
 *       SCIP_CALL( SCIPfree(&scip) );
 *       delete myheur;    // delete heur AFTER SCIPfree() !
 *
 *   2. The object pointer is passed to SCIP and deleted by SCIP in the SCIPfree() call:
 *       SCIP_CALL( SCIPcreate(&scip) );
 *       ...
 *       SCIP_CALL( SCIPincludeObjHeur(scip, new MyHeur(...), TRUE) );
 *       ...
 *       SCIP_CALL( SCIPfree(&scip) );  // destructor of MyHeur is called here
 */
EXTERN
SCIP_RETCODE SCIPincludeObjHeur(
   SCIP*                 scip,               /**< SCIP data structure */
   scip::ObjHeur*        objheur,            /**< primal heuristic object */
   SCIP_Bool             deleteobject        /**< should the primal heuristic object be deleted when heuristic is freed? */
   );

/** returns the heur object of the given name, or 0 if not existing */
EXTERN
scip::ObjHeur* SCIPfindObjHeur(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           name                /**< name of primal heuristic */
   );

/** returns the heur object for the given primal heuristic */
EXTERN
scip::ObjHeur* SCIPgetObjHeur(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_HEUR*            heur                /**< primal heuristic */
   );

#endif
