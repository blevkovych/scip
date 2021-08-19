/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2021 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scipopt.org.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   type_nlpi.h
 * @ingroup TYPEDEFINITIONS
 * @brief  type definitions for NLP solver interfaces
 * @author Stefan Vigerske
 * @author Thorsten Gellermann
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_TYPE_NLPI_H__
#define __SCIP_TYPE_NLPI_H__

#include "scip/def.h"
#include "scip/type_scip.h"
#include "scip/type_expr.h"
#include "scip/type_nlp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SCIP_Nlpi          SCIP_NLPI;          /**< NLP solver interface */
typedef struct SCIP_NlpiData      SCIP_NLPIDATA;      /**< locally defined NLP solver interface data */
typedef struct SCIP_NlpiProblem   SCIP_NLPIPROBLEM;   /**< locally defined NLP solver interface data for a specific problem instance */

enum SCIP_NlpParam_FastFail
{
   SCIP_NLPPARAM_FASTFAIL_OFF          = 0,  /**< never stop if progress is still possible */
   SCIP_NLPPARAM_FASTFAIL_CONSERVATIVE = 1,  /**< stop if it seems unlikely that an improving point can be found */
   SCIP_NLPPARAM_FASTFAIL_AGGRESSIVE   = 2   /**< stop if convergence rate is low */
};
typedef enum SCIP_NlpParam_FastFail SCIP_NLPPARAM_FASTFAIL;

/** parameters for NLP solve */
struct SCIP_NlpParam
{
   SCIP_Real             lobjlimit;          /**< lower objective limit (cutoff) */
   SCIP_Real             feastol;            /**< feasibility tolerance (maximal allowed absolute violation of constraints and variable bounds) */
   SCIP_Real             opttol;             /**< optimality tolerance (maximal allowed absolute violation of optimality conditions) */
   SCIP_Real             solvertol;          /**< solver-specific tolerance on accuracy, e.g., maximal violation of feasibility and optimality in scaled problem (0.0: use solver default) */
   SCIP_Real             timelimit;          /**< time limit in seconds: use SCIP_REAL_MAX to use remaining time available for SCIP solve (limits/time - currenttime) */
   int                   iterlimit;          /**< iteration limit */
   unsigned short        verblevel;          /**< verbosity level of output of NLP solver to the screen: 0 off, 1 normal, 2 debug, > 2 more debug */
   SCIP_NLPPARAM_FASTFAIL fastfail;          /**< whether the NLP solver should stop early if convergence is slow */
   SCIP_Bool             expectinfeas;       /**< whether to expect an infeasible problem */
   SCIP_Bool             warmstart;          /**< whether to try to use solution of previous solve as starting point (if available) */
   const char*           caller;             /**< name of file from which NLP is solved (it's fine to set this to NULL) */
};
/** parameters for NLP solve */
typedef struct SCIP_NlpParam SCIP_NLPPARAM;

#if defined(SCIP_DEBUG) || defined(SCIP_MOREDEBUG) || defined(SCIP_EVENMOREDEBUG)
#define SCIP_NLPPARAM_DEFAULT_VERBLEVEL 1
#else
#define SCIP_NLPPARAM_DEFAULT_VERBLEVEL 0
#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1800
/** default values for parameters
 *
 * typical use for this define is the initialization of a SCIP_NLPPARAM struct, e.g.,
 *    SCIP_NLPPARAM nlpparam = { SCIP_NLPPARAM_DEFAULT(scip); }   //lint !e446
 * or
 *    SCIP_NLPPARAM nlpparam;
 *    nlpparam = (SCIP_NLPPARAM){ SCIP_NLPPARAM_DEFAULT(scip); }  //lint !e446
 */
#define SCIP_NLPPARAM_DEFAULT_INITS(scip)              \
   .lobjlimit   = SCIP_REAL_MIN,                       \
   .feastol     = SCIPfeastol(scip),                   \
   .opttol      = SCIPdualfeastol(scip),               \
   .solvertol   = 0.0,                                 \
   .timelimit   = SCIP_REAL_MAX,                       \
   .iterlimit   = INT_MAX,                             \
   .verblevel   = SCIP_NLPPARAM_DEFAULT_VERBLEVEL,     \
   .fastfail    = SCIP_NLPPARAM_FASTFAIL_CONSERVATIVE, \
   .expectinfeas= FALSE,                               \
   .warmstart   = FALSE,                               \
   .caller      = __FILE__

/** default values for parameters
 *
 * typical use for this define is the initialization of a SCIP_NLPPARAM struct, e.g.,
 *    SCIP_NLPPARAM nlpparam = SCIP_NLPPARAM_DEFAULT(scip);   //lint !e446
 * or
 *    SCIP_NLPPARAM nlpparam;
 *    nlpparam = SCIP_NLPPARAM_DEFAULT(scip);  //lint !e446
 */
#define SCIP_NLPPARAM_DEFAULT(scip) (SCIP_NLPPARAM){ SCIP_NLPPARAM_DEFAULT_INITS(scip) }

#else
/** default NLP parameters with static initialization; required for SCIPsolveNlpi macro with ancient MSVC */
static const SCIP_NLPPARAM SCIP_NLPPARAM_DEFAULT_STATIC = {
   SCIP_REAL_MIN, SCIP_DEFAULT_FEASTOL, SCIP_DEFAULT_DUALFEASTOL, 0.0, SCIP_REAL_MAX, INT_MAX, SCIP_NLPPARAM_DEFAULT_VERBLEVEL, SCIP_NLPPARAM_FASTFAIL_CONSERVATIVE, FALSE, FALSE, __FILE__
};
#define SCIP_NLPPARAM_DEFAULT(scip) SCIP_NLPPARAM_DEFAULT_STATIC
#endif

/** macro to help printing values of SCIP_NLPPARAM struct
 *
 * typical use for this define is something like
 *    SCIPdebugMsg(scip, "calling NLP solver with parameters " SCIP_NLPPARAM_PRINT(param));
 */
#define SCIP_NLPPARAM_PRINT(param) \
  "lobjlimit = %g, "    \
  "feastol = %g, "      \
  "opttol = %g, "       \
  "solvertol = %g, "    \
  "timelimit = %g, "    \
  "iterlimit = %d, "    \
  "verblevel = %hd, "   \
  "fastfail = %d, "     \
  "expectinfeas = %d, " \
  "warmstart = %d, "    \
  "called by %s\n",     \
  (param).lobjlimit, (param).feastol, (param).opttol, (param).solvertol, (param).timelimit, (param).iterlimit, \
  (param).verblevel, (param).fastfail, (param).expectinfeas, (param).warmstart, (param).caller != NULL ? (param).caller : "unknown"

/** NLP solution status */
enum SCIP_NlpSolStat
{
   SCIP_NLPSOLSTAT_GLOBOPT        = 0,    /**< solved to global optimality */
   SCIP_NLPSOLSTAT_LOCOPT         = 1,    /**< solved to local optimality */
   SCIP_NLPSOLSTAT_FEASIBLE       = 2,    /**< feasible solution found */
   SCIP_NLPSOLSTAT_LOCINFEASIBLE  = 3,    /**< solution found is local infeasible */
   SCIP_NLPSOLSTAT_GLOBINFEASIBLE = 4,    /**< problem is proven infeasible */
   SCIP_NLPSOLSTAT_UNBOUNDED      = 5,    /**< problem is unbounded */
   SCIP_NLPSOLSTAT_UNKNOWN        = 6     /**< unknown solution status (e.g., problem not solved yet) */
};
typedef enum SCIP_NlpSolStat SCIP_NLPSOLSTAT;      /** NLP solution status */

/** NLP solver termination status */
enum SCIP_NlpTermStat
{
   SCIP_NLPTERMSTAT_OKAY          = 0,    /**< terminated successfully */
   SCIP_NLPTERMSTAT_TIMELIMIT     = 1,    /**< time limit exceeded */
   SCIP_NLPTERMSTAT_ITERLIMIT     = 2,    /**< iteration limit exceeded */
   SCIP_NLPTERMSTAT_LOBJLIMIT     = 3,    /**< lower objective limit reached */
   SCIP_NLPTERMSTAT_INTERRUPT     = 4,    /**< SCIP has been asked to stop (SCIPinterruptSolve() called) */
   SCIP_NLPTERMSTAT_NUMERICERROR  = 5,    /**< stopped on numerical error */
   SCIP_NLPTERMSTAT_EVALERROR     = 6,    /**< stopped on function evaluation error */
   SCIP_NLPTERMSTAT_OUTOFMEMORY   = 7,    /**< memory exceeded */
   SCIP_NLPTERMSTAT_LICENSEERROR  = 8,    /**< problems with license of NLP solver */
   SCIP_NLPTERMSTAT_OTHER         = 9     /**< other error (= this should never happen) */
#ifndef _MSC_VER  /* MS __declspec(deprecated) not allowed within enums */
   ,/* for some backward compatibility */
   SCIP_NLPTERMSTAT_TILIM   SCIP_DEPRECATED = SCIP_NLPTERMSTAT_TIMELIMIT,
   SCIP_NLPTERMSTAT_ITLIM   SCIP_DEPRECATED = SCIP_NLPTERMSTAT_ITERLIMIT,
   SCIP_NLPTERMSTAT_LOBJLIM SCIP_DEPRECATED = SCIP_NLPTERMSTAT_LOBJLIMIT,
   SCIP_NLPTERMSTAT_NUMERR  SCIP_DEPRECATED = SCIP_NLPTERMSTAT_NUMERICERROR,
   SCIP_NLPTERMSTAT_EVALERR SCIP_DEPRECATED = SCIP_NLPTERMSTAT_EVALERROR,
   SCIP_NLPTERMSTAT_MEMERR  SCIP_DEPRECATED = SCIP_NLPTERMSTAT_OUTOFMEMORY,
   SCIP_NLPTERMSTAT_LICERR  SCIP_DEPRECATED = SCIP_NLPTERMSTAT_LICENSEERROR
#endif
};
typedef enum SCIP_NlpTermStat SCIP_NLPTERMSTAT;  /** NLP solver termination status */

/** Statistics from an NLP solve */
struct SCIP_NlpStatistics
{
   int                   niterations;        /**< number of iterations the NLP solver spend in the last solve command */
   SCIP_Real             totaltime;          /**< total time in CPU sections the NLP solver spend in the last solve command */
   SCIP_Real             evaltime;           /**< time spend in evaluation of functions and their derivatives (only measured if timing/nlpieval = TRUE) */

   SCIP_Real             consviol;           /**< maximal absolute constraint violation in current solution, or SCIP_INVALID if not available */
   SCIP_Real             boundviol;          /**< maximal absolute variable bound violation in current solution, or SCIP_INVALID if not available */
};
typedef struct SCIP_NlpStatistics SCIP_NLPSTATISTICS; /**< NLP solve statistics */

/** copy method of NLP interface (called when SCIP copies plugins)
 *
 * Implementation of this callback is optional.
 *
 *  - scip       : target SCIP where to include copy of NLPI
 *  - sourcenlpi : the NLP interface to copy
 */
#define SCIP_DECL_NLPICOPY(x) SCIP_RETCODE x (\
   SCIP*      scip, \
   SCIP_NLPI* sourcenlpi)

/** frees the data of the NLP interface
 * 
 *  - scip     : SCIP data structure
 *  - nlpi     : datastructure for solver interface
 *  - nlpidata : NLPI data to free
 */
#define SCIP_DECL_NLPIFREE(x) SCIP_RETCODE x (\
   SCIP*           scip, \
   SCIP_NLPI*      nlpi, \
   SCIP_NLPIDATA** nlpidata)

/** gets pointer to solver-internal NLP solver
 * 
 * Implementation of this callback is optional.
 *
 *  - scip : SCIP data structure
 *  - nlpi : datastructure for solver interface
 *  
 * return: void pointer to solver
 */
#define SCIP_DECL_NLPIGETSOLVERPOINTER(x) void* x (\
   SCIP*      scip, \
   SCIP_NLPI* nlpi)

/** creates a problem instance
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : pointer to store the problem data
 *  - name    : name of problem, can be NULL
 */
#define SCIP_DECL_NLPICREATEPROBLEM(x) SCIP_RETCODE x (\
   SCIP*              scip, \
   SCIP_NLPI*         nlpi, \
   SCIP_NLPIPROBLEM** problem, \
   const char*        name)

/** free a problem instance
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : pointer where problem data is stored
 */
#define SCIP_DECL_NLPIFREEPROBLEM(x) SCIP_RETCODE x (\
   SCIP*              scip, \
   SCIP_NLPI*         nlpi, \
   SCIP_NLPIPROBLEM** problem)

/** gets pointer to solver-internal problem instance
 * 
 * Implementation of this callback is optional.
 *
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  
 * return: void pointer to problem instance
 */
#define SCIP_DECL_NLPIGETPROBLEMPOINTER(x) void* x (\
   SCIP*             scip, \
   SCIP_NLPI*        nlpi, \
   SCIP_NLPIPROBLEM* problem)

/** adds variables
 *
 *  - scip     : SCIP data structure
 *  - nlpi     :  datastructure for solver interface
 *  - problem  : datastructure for problem instance
 *  - nvars    : number of variables
 *  - lbs      : lower bounds of variables, can be NULL if -infinity
 *  - ubs      : upper bounds of variables, can be NULL if +infinity
 *  - varnames : names of variables, can be NULL
 */
#define SCIP_DECL_NLPIADDVARS(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int               nvars,   \
   const SCIP_Real*  lbs,     \
   const SCIP_Real*  ubs,     \
   const char**      varnames)

/** add constraints
 * 
 *  - scip     : SCIP data structure
 *  - nlpi     : datastructure for solver interface
 *  - problem  : datastructure for problem instance
 *  - ncons    : number of added constraints
 *  - lhss     : left hand sides of constraints, can be NULL if -infinity
 *  - rhss     : right hand sides of constraints, can be NULL if +infinity
 *  - nlininds : number of linear coefficients for each constraint; may be NULL in case of no linear part
 *  - lininds  : indices of variables for linear coefficients for each constraint; may be NULL in case of no linear part
 *  - linvals  : values of linear coefficient for each constraint; may be NULL in case of no linear part
 *  - exprs    : expressions for nonlinear part of constraints; may be NULL or entries may be NULL when no nonlinear parts
 *  - names    : names of constraints; may be NULL or entries may be NULL
 */
#define SCIP_DECL_NLPIADDCONSTRAINTS(x) SCIP_RETCODE x (\
   SCIP*             scip,     \
   SCIP_NLPI*        nlpi,     \
   SCIP_NLPIPROBLEM* problem,  \
   int               nconss,   \
   const SCIP_Real*  lhss,     \
   const SCIP_Real*  rhss,     \
   const int*        nlininds, \
   int* const*       lininds,  \
   SCIP_Real* const* linvals,  \
   SCIP_EXPR**       exprs,    \
   const char**      names)

/** sets or overwrites objective, a minimization problem is expected
 *  May change sparsity pattern.
 * 
 *  - scip     : SCIP data structure
 *  - nlpi     : datastructure for solver interface
 *  - problem  : datastructure for problem instance
 *  - nlins    : number of linear variables
 *  - lininds  : variable indices; may be NULL in case of no linear part
 *  - linvals  : coefficient values; may be NULL in case of no linear part
 *  - expr     : expression for nonlinear part of objective function; may be NULL in case of no nonlinear part
 *  - constant : objective value offset
 */
#define SCIP_DECL_NLPISETOBJECTIVE(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int               nlins,   \
   const int*        lininds, \
   const SCIP_Real*  linvals, \
   SCIP_EXPR*        expr,    \
   const SCIP_Real   constant)

/** change variable bounds
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  - nvars   : number of variables to change bounds
 *  - indices : indices of variables to change bounds
 *  - lbs     : new lower bounds
 *  - ubs     : new upper bounds
 */
#define SCIP_DECL_NLPICHGVARBOUNDS(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   const int         nvars,   \
   const int*        indices, \
   const SCIP_Real*  lbs,     \
   const SCIP_Real*  ubs)

/** change constraint sides
 *
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  - nconss  : number of constraints to change sides
 *  - indices : indices of constraints to change sides
 *  - lhss    : new left hand sides
 *  - rhss    : new right hand sides
 */
#define SCIP_DECL_NLPICHGCONSSIDES(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int               nconss,  \
   const int*        indices, \
   const SCIP_Real*  lhss,    \
   const SCIP_Real*  rhss)

/** delete a set of variables
 * 
 *  - scip       : SCIP data structure
 *  - nlpi       : datastructure for solver interface
 *  - problem    : datastructure for problem instance
 *  - dstats     : deletion status of vars on input (1 if var should be deleted, 0 if not)
 *                 new position of var on output, -1 if var was deleted
 *  - dstatssize : size of the dstats array
 */
#define SCIP_DECL_NLPIDELVARSET(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int*              dstats,  \
   int               dstatssize)

/** delete a set of constraints
 * 
 *  - scip       : SCIP data structure
 *  - nlpi       : datastructure for solver interface
 *  - problem    : datastructure for problem instance
 *  - dstats     : deletion status of constraints on input (1 if constraint should be deleted, 0 if not)
 *                 new position of row on output, -1 if row was deleted
 *  - dstatssize : size of the dstats array
 */
#define SCIP_DECL_NLPIDELCONSSET(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int*              dstats,  \
   int               dstatssize)

/** changes (or adds) linear coefficients in a constraint or objective
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  - idx     : index of constraint or -1 for objective
 *  - nvals   : number of values in linear constraint to change
 *  - varidxs : indices of variables which coefficient to change
 *  - vals    : new values for coefficients
 */
#define SCIP_DECL_NLPICHGLINEARCOEFS(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int               idx,     \
   int               nvals,   \
   const int*        varidxs, \
   const SCIP_Real*  vals)

/** replaces the expression of a constraint or objective
 *
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  - idxcons : index of constraint or -1 for objective
 *  - expr    : new expression for constraint or objective, or NULL to only remove previous tree
 */
#define SCIP_DECL_NLPICHGEXPR(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   int               idxcons, \
   SCIP_EXPR*        expr)

/** change the constant offset in the objective
 *
 *  - scip        : SCIP data structure
 *  - nlpi        : datastructure for solver interface
 *  - problem     : datastructure for problem instance
 *  - objconstant : new value for objective constant
 */
#define SCIP_DECL_NLPICHGOBJCONSTANT(x) SCIP_RETCODE x (\
   SCIP*             scip,    \
   SCIP_NLPI*        nlpi,    \
   SCIP_NLPIPROBLEM* problem, \
   SCIP_Real         objconstant)

/** sets initial guess
 *
 * Implementation of this callback is optional.
 *
 *  - scip            : SCIP data structure
 *  - nlpi            : datastructure for solver interface
 *  - problem         : datastructure for problem instance
 *  - primalvalues    : initial primal values for variables, or NULL to clear previous values
 *  - consdualvalues  : initial dual values for constraints, or NULL to clear previous values
 *  - varlbdualvalues : initial dual values for variable lower bounds, or NULL to clear previous values
 *  - varubdualvalues : initial dual values for variable upper bounds, or NULL to clear previous values
 */
#define SCIP_DECL_NLPISETINITIALGUESS(x) SCIP_RETCODE x (\
   SCIP*             scip,            \
   SCIP_NLPI*        nlpi,            \
   SCIP_NLPIPROBLEM* problem,         \
   SCIP_Real*        primalvalues,    \
   SCIP_Real*        consdualvalues,  \
   SCIP_Real*        varlbdualvalues, \
   SCIP_Real*        varubdualvalues)

/** tries to solve NLP
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 *  - param   : parameters (e.g., working limits) to use
 */
#define SCIP_DECL_NLPISOLVE(x) SCIP_RETCODE x (\
   SCIP*             scip, \
   SCIP_NLPI*        nlpi, \
   SCIP_NLPIPROBLEM* problem, \
   SCIP_NLPPARAM     param)

/** gives solution status
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 * 
 * return: Solution Status
 */
#define SCIP_DECL_NLPIGETSOLSTAT(x) SCIP_NLPSOLSTAT x (\
   SCIP*             scip, \
   SCIP_NLPI*        nlpi, \
   SCIP_NLPIPROBLEM* problem)

/** gives termination reason
 * 
 *  - scip    : SCIP data structure
 *  - nlpi    : datastructure for solver interface
 *  - problem : datastructure for problem instance
 * 
 * return: Termination Status
 */
#define SCIP_DECL_NLPIGETTERMSTAT(x) SCIP_NLPTERMSTAT x (\
   SCIP*             scip, \
   SCIP_NLPI*        nlpi, \
   SCIP_NLPIPROBLEM* problem)

/** gives primal and dual solution values
 * 
 * solver can return NULL in dual values if not available
 * but if solver provides dual values for one side of variable bounds, then it must also provide those for the other side
 *
 * for a ranged constraint, the dual variable is positive if the right hand side is active and negative if the left hand side is active
 *
 *  - scip            : SCIP data structure
 *  - nlpi            : datastructure for solver interface
 *  - problem         : datastructure for problem instance
 *  - primalvalues    : buffer to store pointer to array to primal values, or NULL if not needed
 *  - consdualvalues  : buffer to store pointer to array to dual values of constraints, or NULL if not needed
 *  - varlbdualvalues : buffer to store pointer to array to dual values of variable lower bounds, or NULL if not needed
 *  - varubdualvalues : buffer to store pointer to array to dual values of variable lower bounds, or NULL if not needed
 *  - objval          : pointer to store the objective value, or NULL if not needed
 */
#define SCIP_DECL_NLPIGETSOLUTION(x) SCIP_RETCODE x (\
   SCIP*             scip,            \
   SCIP_NLPI*        nlpi,            \
   SCIP_NLPIPROBLEM* problem,         \
   SCIP_Real**       primalvalues,    \
   SCIP_Real**       consdualvalues,  \
   SCIP_Real**       varlbdualvalues, \
   SCIP_Real**       varubdualvalues, \
   SCIP_Real*        objval)

/** gives solve statistics
 * 
 *  - scip       : SCIP data structure
 *  - nlpi       : datastructure for solver interface
 *  - problem    : datastructure for problem instance
 *  - statistics : datastructure where to store statistics
 */
#define SCIP_DECL_NLPIGETSTATISTICS(x) SCIP_RETCODE x (\
   SCIP*               scip,    \
   SCIP_NLPI*          nlpi,    \
   SCIP_NLPIPROBLEM*   problem, \
   SCIP_NLPSTATISTICS* statistics)

#ifdef __cplusplus
}
#endif

#endif /*__SCIP_TYPE_NLPI_H__ */
