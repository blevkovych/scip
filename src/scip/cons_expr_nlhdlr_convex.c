/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2020 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   cons_expr_nlhdlr_convex.c
 * @brief  nonlinear handlers for convex and concave expressions
 * @author Benjamin Mueller
 * @author Stefan Vigerske
 *
 * TODO convex: perturb reference point if separation fails due to too large numbers
 */

#include <string.h>

#include "scip/cons_expr_nlhdlr_convex.h"
#include "scip/cons_expr.h"
#include "scip/cons_expr_iterator.h"
#include "scip/cons_expr_var.h"
#include "scip/cons_expr_value.h"
#include "scip/cons_expr_product.h"
#include "scip/cons_expr_pow.h"
#include "scip/cons_expr_sum.h"
#include "scip/dbldblarith.h"

/* fundamental nonlinear handler properties */
#define CONVEX_NLHDLR_NAME             "convex"
#define CONVEX_NLHDLR_DESC             "handler that identifies and estimates convex expressions"
#define CONVEX_NLHDLR_DETECTPRIORITY   50
#define CONVEX_NLHDLR_ENFOPRIORITY     50

#define CONCAVE_NLHDLR_NAME            "concave"
#define CONCAVE_NLHDLR_DESC            "handler that identifies and estimates concave expressions"
#define CONCAVE_NLHDLR_DETECTPRIORITY  40
#define CONCAVE_NLHDLR_ENFOPRIORITY    40

#define DEFAULT_DETECTSUM      FALSE
#define DEFAULT_PREFEREXTENDED TRUE
#define DEFAULT_CVXQUADRATIC_CONVEX   TRUE
#define DEFAULT_CVXQUADRATIC_CONCAVE  FALSE
#define DEFAULT_CVXSIGNOMIAL   TRUE
#define DEFAULT_CVXPRODCOMP    TRUE
#define DEFAULT_HANDLETRIVIAL  FALSE

/*
 * Data structures
 */

/** nonlinear handler expression data */
struct SCIP_ConsExpr_NlhdlrExprData
{
   SCIP_CONSEXPR_EXPR*   nlexpr;             /**< expression (copy) for which this nlhdlr estimates */
   SCIP_HASHMAP*         nlexpr2origexpr;    /**< mapping of our copied expression to original expression */

   int                   nleafs;             /**< number of distinct leafs of nlexpr, i.e., number of distinct (auxiliary) variables handled */
   SCIP_CONSEXPR_EXPR**  leafexprs;          /**< distinct leaf expressions (excluding value-expressions), thus variables */
};

/** nonlinear handler data */
struct SCIP_ConsExpr_NlhdlrData
{
   SCIP_Bool             isnlhdlrconvex;     /**< whether this data is used for the convex nlhdlr (TRUE) or the concave one (FALSE) */
   SCIP_SOL*             evalsol;            /**< solution used for evaluating expression in a different point, e.g., for facet computation of vertex-polyhedral function */

   /* parameters */
   SCIP_Bool             detectsum;          /**< whether to run detection when the root of an expression is a non-quadratic sum */
   SCIP_Bool             preferextended;     /**< whether to prefer extended formulations */

   /* advanced parameters (maybe remove some day) */
   SCIP_Bool             cvxquadratic;       /**< whether to use convexity check on quadratics */
   SCIP_Bool             cvxsignomial;       /**< whether to use convexity check on signomials */
   SCIP_Bool             cvxprodcomp;        /**< whether to use convexity check on product composition f(h)*h */
   SCIP_Bool             handletrivial;      /**< whether to handle trivial expressions, i.e., those where all children are variables */
};

/** data struct to be be passed on to vertexpoly-evalfunction (see SCIPcomputeFacetVertexPolyhedral) */
typedef struct
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata;
   SCIP_SOL*                     evalsol;
   SCIP*                         scip;
   SCIP_CONSHDLR*                conshdlr;
} VERTEXPOLYFUN_EVALDATA;

/** stack used in constructExpr to store expressions that need to be investigated ("to do list") */
typedef struct
{
   SCIP_CONSEXPR_EXPR**  stack;              /**< stack elements */
   int                   stacksize;          /**< allocated space (in number of pointers) */
   int                   stackpos;           /**< position of top element of stack */
} EXPRSTACK;

#define DECL_CURVCHECK(x) SCIP_RETCODE x( \
   SCIP*                 scip,               /**< SCIP data structure */ \
   SCIP_CONSHDLR*        conshdlr,           /**< constraint handler */ \
   SCIP_CONSEXPR_EXPR*   nlexpr,             /**< nlhdlr-expr to check */ \
   SCIP_Bool             isrootexpr,         /**< whether nlexpr is the root from where detection has been started */ \
   EXPRSTACK*            stack,              /**< stack where to add generated leafs */ \
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping from our expression copy to original expression */ \
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata,     /**< data of nlhdlr */ \
   SCIP_HASHMAP*         assumevarfixed,     /**< hashmap containing variables that should be assumed to be fixed, or NULL */ \
   SCIP_Bool*            success             /**< whether we found something */ \
   )


/*
 * static methods
 */

/** create nlhdlr-expression
 *
 * does not create children, i.e., assumes that this will be a leaf
 */
static
SCIP_RETCODE nlhdlrExprCreate(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping from copied to original expression */
   SCIP_CONSEXPR_EXPR**  nlhdlrexpr,         /**< buffer to store created expr */
   SCIP_CONSEXPR_EXPR*   origexpr,           /**< original expression to be copied */
   SCIP_EXPRCURV         curv                /**< curvature to achieve */
)
{
   assert(scip != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(nlhdlrexpr != NULL);
   assert(origexpr != NULL);

   if( SCIPgetConsExprExprNChildren(origexpr) == 0 )
   {
      /* for leaves, do not copy */
      *nlhdlrexpr = origexpr;
      SCIPcaptureConsExprExpr(*nlhdlrexpr);
      if( !SCIPhashmapExists(nlexpr2origexpr, (void*)*nlhdlrexpr) )
      {
         SCIP_CALL( SCIPhashmapInsert(nlexpr2origexpr, (void*)*nlhdlrexpr, (void*)origexpr) );
      }
      return SCIP_OKAY;
   }

   /* create copy of expression, but without children */
   SCIP_CALL( SCIPduplicateConsExprExpr(scip, conshdlr, origexpr, nlhdlrexpr, FALSE) );
   assert(*nlhdlrexpr != NULL);  /* copies within the same SCIP must always work */

   /* store the curvature we want to get in the curvature flag of the copied expression
    * it's a bit of a misuse, but once we are done with everything, this is actually correct
    */
   SCIPsetConsExprExprCurvature(*nlhdlrexpr, curv);

   /* remember which the original expression was */
   SCIP_CALL( SCIPhashmapInsert(nlexpr2origexpr, (void*)*nlhdlrexpr, (void*)origexpr) );

   return SCIP_OKAY;
}

/** expand nlhdlr-expression by adding children according to original expression */
static
SCIP_RETCODE nlhdlrExprGrowChildren(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping from copied to original expression */
   SCIP_CONSEXPR_EXPR*   nlhdlrexpr,         /**< expression for which to create children */
   SCIP_EXPRCURV*        childrencurv        /**< curvature required for children, or NULL if to set to UNKNOWN */
   )
{
   SCIP_CONSEXPR_EXPR* origexpr;
   SCIP_CONSEXPR_EXPR* child;
   int nchildren;
   int i;

   assert(scip != NULL);
   assert(nlhdlrexpr != NULL);
   assert(SCIPgetConsExprExprNChildren(nlhdlrexpr) == 0);

   origexpr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlhdlrexpr);

   nchildren = SCIPgetConsExprExprNChildren(origexpr);
   if( nchildren == 0 )
      return SCIP_OKAY;

   for( i = 0; i < nchildren; ++i )
   {
      SCIP_CALL( nlhdlrExprCreate(scip, conshdlr, nlexpr2origexpr, &child, SCIPgetConsExprExprChildren(origexpr)[i],
         childrencurv != NULL ? childrencurv[i] : SCIP_EXPRCURV_UNKNOWN) );
      SCIP_CALL( SCIPappendConsExprExpr(scip, nlhdlrexpr, child) );
      /* append captures child, so we can release the capture from nlhdlrExprCreate */
      SCIP_CALL( SCIPreleaseConsExprExpr(scip, &child) );
   }

   assert(SCIPgetConsExprExprNChildren(nlhdlrexpr) == SCIPgetConsExprExprNChildren(origexpr));

   return SCIP_OKAY;
}

static
SCIP_DECL_VERTEXPOLYFUN(nlhdlrExprEvalConcave)
{
   VERTEXPOLYFUN_EVALDATA* evaldata = (VERTEXPOLYFUN_EVALDATA*)funcdata;
   int i;

   assert(args != NULL);
   assert(nargs == evaldata->nlhdlrexprdata->nleafs);
   assert(evaldata != NULL);

#ifdef SCIP_MORE_DEBUG
   SCIPdebugMsg(evaldata->scip, "eval vertexpolyfun at\n");
#endif
   for( i = 0; i < nargs; ++i )
   {
#ifdef SCIP_MORE_DEBUG
      SCIPdebugMsg(evaldata->scip, "  <%s> = %g\n", SCIPvarGetName(SCIPgetConsExprExprVarVar(evaldata->nlhdlrexprdata->leafexprs[i])), args[i]);
#endif
      SCIP_CALL_ABORT( SCIPsetSolVal(evaldata->scip, evaldata->evalsol, SCIPgetConsExprExprVarVar(evaldata->nlhdlrexprdata->leafexprs[i]), args[i]) );
   }

   SCIP_CALL_ABORT( SCIPevalConsExprExpr(evaldata->scip, evaldata->conshdlr, evaldata->nlhdlrexprdata->nlexpr, evaldata->evalsol, 0) );

   return SCIPgetConsExprExprValue(evaldata->nlhdlrexprdata->nlexpr);
}

static
SCIP_RETCODE exprstackInit(
   SCIP*                 scip,               /**< SCIP data structure */
   EXPRSTACK*            exprstack,          /**< stack to initialize */
   int                   initsize            /**< initial size */
   )
{
   assert(scip != NULL);
   assert(exprstack != NULL);
   assert(initsize > 0);

   SCIP_CALL( SCIPallocBufferArray(scip, &exprstack->stack, initsize) );
   exprstack->stacksize = initsize;
   exprstack->stackpos = -1;

   return SCIP_OKAY;
}

static
void exprstackFree(
   SCIP*                 scip,               /**< SCIP data structure */
   EXPRSTACK*            exprstack           /**< free expression stack */
   )
{
   assert(scip != NULL);
   assert(exprstack != NULL);

   SCIPfreeBufferArray(scip, &exprstack->stack);
}

static
SCIP_RETCODE exprstackPush(
   SCIP*                 scip,               /**< SCIP data structure */
   EXPRSTACK*            exprstack,          /**< expression stack */
   int                   nexprs,             /**< number of expressions to push */
   SCIP_CONSEXPR_EXPR**  exprs               /**< expressions to push */
   )
{
   assert(scip != NULL);
   assert(exprstack != NULL);

   if( nexprs == 0 )
      return SCIP_OKAY;

   assert(exprs != NULL);

   if( exprstack->stackpos+1 + nexprs > exprstack->stacksize )
   {
      exprstack->stacksize = SCIPcalcMemGrowSize(scip, exprstack->stackpos+1 + nexprs);
      SCIP_CALL( SCIPreallocBufferArray(scip, &exprstack->stack, exprstack->stacksize) );
   }

   memcpy(exprstack->stack + (exprstack->stackpos+1), exprs, nexprs * sizeof(SCIP_CONSEXPR_EXPR*));
   exprstack->stackpos += nexprs;

   return SCIP_OKAY;
}

static
SCIP_CONSEXPR_EXPR* exprstackPop(
   EXPRSTACK*            exprstack           /**< expression stack */
   )
{
   assert(exprstack != NULL);
   assert(exprstack->stackpos >= 0);

   return exprstack->stack[exprstack->stackpos--];
}

static
SCIP_Bool exprstackIsEmpty(
   EXPRSTACK*            exprstack           /**< expression stack */
   )
{
   assert(exprstack != NULL);

   return exprstack->stackpos < 0;
}

/** looks whether given expression is (proper) quadratic and has a given curvature
 *
 * If having a given curvature, currently require all arguments of quadratic to be linear.
 * Hence, not using this for a simple square term, as curvCheckExprhdlr may provide a better condition on argument curvature then.
 * Also we wouldn't do anything useful for a single bilinear term.
 * Thus, run on sum's only.
 */
static
DECL_CURVCHECK(curvCheckQuadratic)
{  /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_QUADEXPR* quaddata;
   SCIP_EXPRCURV presentcurv;
   SCIP_EXPRCURV wantedcurv;
   int nbilinexprs;
   int nquadexprs;
   int i;

   assert(nlexpr != NULL);
   assert(stack != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(success != NULL);

   *success = FALSE;

   if( !nlhdlrdata->cvxquadratic )
      return SCIP_OKAY;

   if( SCIPgetConsExprExprHdlr(nlexpr) != SCIPgetConsExprExprHdlrSum(conshdlr) )
      return SCIP_OKAY;

   wantedcurv = SCIPgetConsExprExprCurvature(nlexpr);
   if( wantedcurv == SCIP_EXPRCURV_LINEAR )
      return SCIP_OKAY;
   assert(wantedcurv == SCIP_EXPRCURV_CONVEX || wantedcurv == SCIP_EXPRCURV_CONCAVE);

   expr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr);
   assert(expr != NULL);

   /* check whether quadratic */
   SCIP_CALL( SCIPgetConsExprQuadratic(scip, conshdlr, expr, &quaddata) );

   /* if not quadratic, then give up here */
   if( quaddata == NULL )
      return SCIP_OKAY;

   SCIPgetConsExprQuadraticData(quaddata, NULL, NULL, NULL, NULL, &nquadexprs, &nbilinexprs);

   /* if only single square term (+linear), then give up here (let curvCheckExprhdlr handle this) */
   if( nquadexprs <= 1 )
      return SCIP_OKAY;

   /* if root expression is only sum of squares (+linear) and detectsum is disabled, then give up here, too */
   if( isrootexpr && !nlhdlrdata->detectsum && nbilinexprs == 0 )
      return SCIP_OKAY;

   /* get curvature of quadratic
    * TODO as we know what curvature we want, we could first do some simple checks like computing xQx for a random x
    */
   SCIP_CALL( SCIPgetConsExprQuadraticCurvature(scip, quaddata, &presentcurv, assumevarfixed) );

   /* if not having desired curvature, return */
   if( presentcurv != wantedcurv )
      return SCIP_OKAY;

   *success = TRUE;

   /* add immediate children to nlexpr */
   SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, nlexpr, NULL) );
   assert(SCIPgetConsExprExprNChildren(nlexpr) == SCIPgetConsExprExprNChildren(expr));

   /* put children that are not square or product on stack
    * grow child for children that are square or product and put this child on stack
    * require all children to be linear
    */
   for( i = 0; i < SCIPgetConsExprExprNChildren(nlexpr); ++i )
   {
      SCIP_CONSEXPR_EXPR* child;
      SCIP_EXPRCURV curvlinear[2] = { SCIP_EXPRCURV_LINEAR, SCIP_EXPRCURV_LINEAR };

      child = SCIPgetConsExprExprChildren(nlexpr)[i];
      assert(child != NULL);

      assert(SCIPhashmapGetImage(nlexpr2origexpr, (void*)child) == SCIPgetConsExprExprChildren(expr)[i]);

      if( SCIPgetConsExprExprHdlr(child) == SCIPgetConsExprExprHdlrPower(conshdlr) &&
         SCIPgetConsExprExprPowExponent(child) == 2.0 )
      {
         /* square term */
         SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, child, curvlinear) );
         assert(SCIPgetConsExprExprNChildren(child) == 1);
         SCIP_CALL( exprstackPush(scip, stack, 1, SCIPgetConsExprExprChildren(child)) );
      }
      else if( SCIPgetConsExprExprHdlr(child) == SCIPgetConsExprExprHdlrProduct(conshdlr) &&
         SCIPgetConsExprExprNChildren(SCIPgetConsExprExprChildren(expr)[i]) == 2 )
         /* using original version of child here as NChildren(child)==0 atm */
      {
         /* bilinear term */
         SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, child, curvlinear) );
         assert(SCIPgetConsExprExprNChildren(child) == 2);
         SCIP_CALL( exprstackPush(scip, stack, 2, SCIPgetConsExprExprChildren(child)) );
      }
      else
      {
         /* linear term (or term to be considered as linear) */
         SCIPsetConsExprExprCurvature(child, SCIP_EXPRCURV_LINEAR);
         SCIP_CALL( exprstackPush(scip, stack, 1, &child) );
      }
   }

   return SCIP_OKAY;
}

/** looks whether top of given expression looks like a signomial that can have a given curvature
 * e.g., sqrt(x)*sqrt(y) is convex if x,y >= 0 and x and y are convex
 * unfortunately, doesn't work for tls, because i) it's originally sqrt(x*y), and ii) it is expanded into some sqrt(z*y+y)
 * but works for cvxnonsep_nsig
 */
static
DECL_CURVCHECK(curvCheckSignomial)
{  /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* child;
   SCIP_Real* exponents;
   SCIP_INTERVAL* bounds;
   SCIP_EXPRCURV* curv;
   int nfactors;
   int i;

   assert(nlexpr != NULL);
   assert(stack != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(success != NULL);

   *success = FALSE;

   if( !nlhdlrdata->cvxsignomial )
      return SCIP_OKAY;

   if( SCIPgetConsExprExprHdlr(nlexpr) != SCIPgetConsExprExprHdlrProduct(conshdlr) )
      return SCIP_OKAY;

   expr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr);
   assert(expr != NULL);

   nfactors = SCIPgetConsExprExprNChildren(expr);
   if( nfactors <= 1 )  /* boooring */
      return SCIP_OKAY;

   SCIP_CALL( SCIPallocBufferArray(scip, &exponents, nfactors) );
   SCIP_CALL( SCIPallocBufferArray(scip, &bounds, nfactors) );
   SCIP_CALL( SCIPallocBufferArray(scip, &curv, nfactors) );

   for( i = 0; i < nfactors; ++i )
   {
      child = SCIPgetConsExprExprChildren(expr)[i];
      assert(child != NULL);

      if( SCIPgetConsExprExprHdlr(child) != SCIPgetConsExprExprHdlrPower(conshdlr) )
      {
         exponents[i] = 1.0;
         bounds[i] = SCIPgetConsExprExprActivity(scip, child);
      }
      else
      {
         exponents[i] = SCIPgetConsExprExprPowExponent(child);
         bounds[i] = SCIPgetConsExprExprActivity(scip, SCIPgetConsExprExprChildren(child)[0]);
      }
   }

   if( !SCIPexprcurvMonomialInv(SCIPexprcurvMultiply(SCIPgetConsExprExprProductCoef(expr), SCIPgetConsExprExprCurvature(nlexpr)), nfactors, exponents, bounds, curv) )
      goto TERMINATE;

   /* add immediate children to nlexpr
    * some entries in curv actually apply to arguments of pow's, will correct this next
    */
   SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, nlexpr, curv) );
   assert(SCIPgetConsExprExprNChildren(nlexpr) == nfactors);

   /* put children that are not power on stack
    * grow child for children that are power and put this child on stack
    * if preferextended, then require children with more than one child to be linear
    * unless they are linear, an auxvar will be introduced for them and thus they will be handled as var here
    */
   for( i = 0; i < nfactors; ++i )
   {
      child = SCIPgetConsExprExprChildren(nlexpr)[i];
      assert(child != NULL);

      if( SCIPgetConsExprExprHdlr(child) == SCIPgetConsExprExprHdlrPower(conshdlr) )
      {
         SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, child, &curv[i]) );
         assert(SCIPgetConsExprExprNChildren(child) == 1);
         child = SCIPgetConsExprExprChildren(child)[0];
      }
      assert(SCIPgetConsExprExprNChildren(child) == 0);

      if( nlhdlrdata->preferextended && SCIPgetConsExprExprNChildren(child) > 1 )
      {
         SCIPsetConsExprExprCurvature(child, SCIP_EXPRCURV_LINEAR);
#ifdef SCIP_DEBUG
         SCIPinfoMessage(scip, NULL, "Extendedform: Require linearity for ");
         SCIPprintConsExprExpr(scip, conshdlr, child, NULL);
         SCIPinfoMessage(scip, NULL, "\n");
#endif
      }

      SCIP_CALL( exprstackPush(scip, stack, 1, &child) );
   }

   *success = TRUE;

TERMINATE:
   SCIPfreeBufferArray(scip, &curv);
   SCIPfreeBufferArray(scip, &bounds);
   SCIPfreeBufferArray(scip, &exponents);

   return SCIP_OKAY;
}

/** looks for f(c*h(x)+d)*h(x) * constant-factor
 *
 * Assume h is univariate:
 * - First derivative is f'(c h + d) c h' h + f(c h + d) h'.
 * - Second derivative is f''(c h + d) c h' c h' h + f'(c h + d) (c h'' h + c h' h') + f'(c h + d) c h' h' + f(c h + d) h''
 *   = f''(c h + d) c^2 h'^2 h + f'(c h + d) c h'' h + 2 f'(c h + d) c h'^2 + f(c h + d) h''
 *   Remove always positive factors: f''(c h + d) h, f'(c h + d) c h'' h, f'(c h + d) c, f(c h + d) h''
 *   For convexity we want all these terms to be nonnegative. For concavity we want all of them to be nonpositive.
 *   Note, that in each term either f'(c h + d) and c occur, or none of them.
 * - Thus, f(c h(x) + d)h(x) is convex if c*f is monotonically increasing (c f' >= 0) and either
 *   - f is convex (f'' >= 0) and h is nonnegative (h >= 0) and h is convex (h'' >= 0) and [f is nonnegative (f >= 0) or h is linear (h''=0)], or
 *   - f is concave (f'' <= 0) and h is nonpositive (h <= 0) and h is concave (h'' <= 0) and [f is nonpositive (f <= 0) or h is linear (h''=0)]
 *   and f(c h(x) + d)h(x) is concave if c*f is monotonically decreasing (c f' <= 0) and either
 *   - f is convex (f'' >= 0) and h is nonpositive (h <= 0) and h is concave (h'' <= 0) and [f is nonnegative (f >= 0) or h is linear (h''=0)], or
 *   - f is concave (f'' <= 0) and h is nonnegative (h >= 0) and h is convex (h'' >= 0) and [f is nonpositive (f <= 0) or h is linear (h''=0)]
 *
 * This should hold also for multivariate and linear h, as things are invariant under linear transformations.
 * Similar to signomial, I'll assume that this will also hold for other multivariate h (someone has a formal proof?).
 */
static
DECL_CURVCHECK(curvCheckProductComposite)
{  /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* f;
   SCIP_CONSEXPR_EXPR* h = NULL;
   SCIP_Real c = 0.0;
   SCIP_CONSEXPR_EXPR* ch = NULL; /* c * h */
   SCIP_INTERVAL fbounds;
   SCIP_INTERVAL hbounds;
   SCIP_MONOTONE fmonotonicity;
   SCIP_EXPRCURV desiredcurv;
   SCIP_EXPRCURV hcurv;
   SCIP_EXPRCURV dummy;
   int fidx;

   assert(nlexpr != NULL);
   assert(stack != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(success != NULL);

   *success = FALSE;

   if( !nlhdlrdata->cvxprodcomp )
      return SCIP_OKAY;

   if( SCIPgetConsExprExprHdlr(nlexpr) != SCIPgetConsExprExprHdlrProduct(conshdlr) )
      return SCIP_OKAY;

   expr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr);
   assert(expr != NULL);

   if( SCIPgetConsExprExprNChildren(expr) != 2 )
      return SCIP_OKAY;

   /* check whether we have f(c * h(x)) * h(x) or h(x) * f(c * h(x)) */
   for( fidx = 0; fidx <= 1; ++fidx )
   {
      f = SCIPgetConsExprExprChildren(expr)[fidx];

      if( SCIPgetConsExprExprNChildren(f) != 1 )
         continue;

      ch = SCIPgetConsExprExprChildren(f)[0];
      c = 1.0;
      h = ch;

      /* check whether ch is of the form c*h(x), then switch h to child ch */
      if( SCIPgetConsExprExprHdlr(ch) == SCIPgetConsExprExprHdlrSum(conshdlr) && SCIPgetConsExprExprNChildren(ch) == 1 )
      {
         c = SCIPgetConsExprExprSumCoefs(ch)[0];
         h = SCIPgetConsExprExprChildren(ch)[0];
         assert(c != 1.0 || SCIPgetConsExprExprSumConstant(ch) != 0.0);  /* we could handle this, but it should have been simplified away */
      }

#ifndef NLHDLR_CONVEX_UNITTEST
      /* can assume that duplicate subexpressions have been identified and comparing pointer is sufficient */
      if( SCIPgetConsExprExprChildren(expr)[1-fidx] == h )
#else
      /* called from unittest -> duplicate subexpressions were not identified -> compare more expensively */
      if( SCIPcompareConsExprExprs(SCIPgetConsExprExprChildren(expr)[1-fidx], h) == 0 )
#endif
         break;
   }
   if( fidx == 2 )
      return SCIP_OKAY;

#ifdef SCIP_MORE_DEBUG
   SCIPinfoMessage(scip, NULL, "f(c*h+d)*h with f = %s, c = %g, d = %g, h = ", SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(f)), c, h != ch ? SCIPgetConsExprExprSumConstant(ch) : 0.0);
   SCIPprintConsExprExpr(scip, conshdlr, h, NULL);
   SCIPinfoMessage(scip, NULL, "\n");
#endif

   assert(c != 0.0);

   fbounds = SCIPgetConsExprExprActivity(scip, f);
   hbounds = SCIPgetConsExprExprActivity(scip, h);

   /* if h has mixed sign, then cannot conclude anything */
   if( hbounds.inf < 0.0 && hbounds.sup > 0.0 )
      return SCIP_OKAY;

   fmonotonicity = SCIPgetConsExprExprMonotonicity(scip, f, 0);

   /* if f is not monotone, then cannot conclude anything */
   if( fmonotonicity == SCIP_MONOTONE_UNKNOWN )
      return SCIP_OKAY;

   /* curvature we want to achieve (negate if product has negative coef) */
   desiredcurv = SCIPexprcurvMultiply(SCIPgetConsExprExprProductCoef(nlexpr), SCIPgetConsExprExprCurvature(nlexpr));

   /* now check the conditions as stated above */
   if( desiredcurv == SCIP_EXPRCURV_CONVEX )
   {
      /* f(c h(x)+d)h(x) is convex if c*f is monotonically increasing (c f' >= 0) and either
      *   - f is convex (f'' >= 0) and h is nonnegative (h >= 0) and h is convex (h'' >= 0) and [f is nonnegative (f >= 0) or h is linear (h''=0)], or
      *   - f is concave (f'' <= 0) and h is nonpositive (h <= 0) and h is concave (h'' <= 0) and [f is nonpositive (f <= 0) or h is linear (h''=0)]
      *  as the curvature requirements on f are on f only and not the composition f(h), we can ignore the requirements returned by SCIPcurvatureConsExprExprHdlr (last arg)
      */
      if( (c > 0.0 && fmonotonicity != SCIP_MONOTONE_INC) || (c < 0.0 && fmonotonicity != SCIP_MONOTONE_DEC) )
         return SCIP_OKAY;

      /* check whether f can be convex (h>=0) or concave (h<=0), resp., and derive requirements for h */
      if( hbounds.inf >= 0 )
      {
         SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, f, SCIP_EXPRCURV_CONVEX, success, &dummy) );

         /* now h also needs to be convex; and if f < 0, then h actually needs to be linear */
         if( fbounds.inf < 0.0 )
            hcurv = SCIP_EXPRCURV_LINEAR;
         else
            hcurv = SCIP_EXPRCURV_CONVEX;
      }
      else
      {
         SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, f, SCIP_EXPRCURV_CONCAVE, success, &dummy) );

         /* now h also needs to be concave; and if f > 0, then h actually needs to be linear */
         if( fbounds.sup > 0.0 )
            hcurv = SCIP_EXPRCURV_LINEAR;
         else
            hcurv = SCIP_EXPRCURV_CONCAVE;
      }

   }
   else
   {
      /* f(c h(x)+d)*h(x) is concave if c*f is monotonically decreasing (c f' <= 0) and either
      *   - f is convex (f'' >= 0) and h is nonpositive (h <= 0) and h is concave (h'' <= 0) and [f is nonnegative (f >= 0) or h is linear (h''=0)], or
      *   - f is concave (f'' <= 0) and h is nonnegative (h >= 0) and h is convex (h'' >= 0) and [f is nonpositive (f <= 0) or h is linear (h''=0)]
      *  as the curvature requirements on f are on f only and not the composition f(h), we can ignore the requirements returned by SCIPcurvatureConsExprExprHdlr (last arg)
      */
      if( (c > 0.0 && fmonotonicity != SCIP_MONOTONE_DEC) || (c < 0.0 && fmonotonicity != SCIP_MONOTONE_INC) )
         return SCIP_OKAY;

      /* check whether f can be convex (h<=0) or concave (h>=0), resp., and derive requirements for h */
      if( hbounds.sup <= 0 )
      {
         SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, f, SCIP_EXPRCURV_CONVEX, success, &dummy) );

         /* now h also needs to be concave; and if f < 0, then h actually needs to be linear */
         if( fbounds.inf < 0.0 )
            hcurv = SCIP_EXPRCURV_LINEAR;
         else
            hcurv = SCIP_EXPRCURV_CONCAVE;
      }
      else
      {
         SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, f, SCIP_EXPRCURV_CONCAVE, success, &dummy) );

         /* now h also needs to be convex; and if f > 0, then h actually needs to be linear */
         if( fbounds.sup > 0.0 )
            hcurv = SCIP_EXPRCURV_LINEAR;
         else
            hcurv = SCIP_EXPRCURV_CONVEX;
      }
   }

   if( !*success )
      return SCIP_OKAY;

   /* add immediate children (f and ch) to nlexpr; we set required curvature for h further below */
   SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, nlexpr, NULL) );
   assert(SCIPgetConsExprExprNChildren(nlexpr) == 2);

   /* copy of f (and h) should have same child position in nlexpr as f (and h) has on expr (resp) */
   assert(SCIPhashmapGetImage(nlexpr2origexpr, (void*)SCIPgetConsExprExprChildren(nlexpr)[fidx]) == (void*)f);
#ifndef NLHDLR_CONVEX_UNITTEST
   assert(SCIPhashmapGetImage(nlexpr2origexpr, (void*)SCIPgetConsExprExprChildren(nlexpr)[1-fidx]) == (void*)h);
#endif
   /* push this h onto stack for further checking */
   SCIP_CALL( exprstackPush(scip, stack, 1, &(SCIPgetConsExprExprChildren(nlexpr)[1-fidx])) );

   /* h-child of product should have curvature hcurv */
   SCIPsetConsExprExprCurvature(SCIPgetConsExprExprChildren(nlexpr)[1-fidx], hcurv);

   if( h != ch )
   {
      /* add copy of ch as child to copy of f */
      SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, SCIPgetConsExprExprChildren(nlexpr)[fidx], NULL) );
      assert(SCIPgetConsExprExprNChildren(SCIPgetConsExprExprChildren(nlexpr)[fidx]) == 1);
      assert(SCIPhashmapGetImage(nlexpr2origexpr, (void*)SCIPgetConsExprExprChildren(SCIPgetConsExprExprChildren(nlexpr)[fidx])[0]) == (void*)ch);

      /* add copy of h (created above as child of product) as child in copy of ch */
      SCIP_CALL( SCIPappendConsExprExpr(scip,
         SCIPgetConsExprExprChildren(SCIPgetConsExprExprChildren(nlexpr)[fidx])[0] /* copy of ch */,
         SCIPgetConsExprExprChildren(nlexpr)[1-fidx] /* copy of h */) );
   }
   else
   {
      /* add copy of h (created above as child of product) as child in copy of f */
      SCIP_CALL( SCIPappendConsExprExpr(scip,
         SCIPgetConsExprExprChildren(nlexpr)[fidx] /* copy of f */,
         SCIPgetConsExprExprChildren(nlexpr)[1-fidx] /* copy of h */) );
   }

   return SCIP_OKAY;
}

/** use expression handlers curvature callback to check whether given curvature can be achieved */
static
DECL_CURVCHECK(curvCheckExprhdlr)
{  /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* origexpr;
   int nchildren;
   SCIP_EXPRCURV* childcurv;

   assert(nlexpr != NULL);
   assert(stack != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(success != NULL);

   origexpr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, nlexpr);
   assert(origexpr != NULL);
   nchildren = SCIPgetConsExprExprNChildren(origexpr);

   if( nchildren == 0 )
   {
      /* if originally no children, then should be var or value, which should have every curvature, so should always be success */
      SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, origexpr, SCIPgetConsExprExprCurvature(nlexpr), success, NULL) );
      assert(*success);

      return SCIP_OKAY;
   }

   /* ignore sums if > 1 children
    * NOTE: this means that for something like 1+f(x), even if f is a trivial convex expression, we would handle 1+f(x)
    * with this nlhdlr, instead of formulating this as 1+z and handling z=f(x) with the default nlhdlr, i.e., the exprhdlr
    * today, I prefer handling this here, as it avoids introducing an extra auxiliary variable
    */
   if( isrootexpr && !nlhdlrdata->detectsum && SCIPgetConsExprExprHdlr(nlexpr) == SCIPgetConsExprExprHdlrSum(conshdlr) && nchildren > 1 )
      return SCIP_OKAY;

   SCIP_CALL( SCIPallocBufferArray(scip, &childcurv, nchildren) );

   /* check whether and under which conditions origexpr can have desired curvature */
   SCIP_CALL( SCIPcurvatureConsExprExprHdlr(scip, conshdlr, origexpr, SCIPgetConsExprExprCurvature(nlexpr), success, childcurv) );
#ifdef SCIP_MORE_DEBUG
   SCIPprintConsExprExpr(scip, conshdlr, origexpr, NULL);
   SCIPinfoMessage(scip, NULL, " is %s? %d\n", SCIPexprcurvGetName(SCIPgetConsExprExprCurvature(nlexpr)), *success);
#endif
   if( !*success )
      goto TERMINATE;

   /* if origexpr can have curvature curv, then don't treat it as leaf, but include its children */
   SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, nlexpr, childcurv) );
   assert(SCIPgetConsExprExprChildren(nlexpr) != NULL);
   assert(SCIPgetConsExprExprNChildren(nlexpr) == nchildren);

   /* If more than one child and we prefer extended formulations, then require all children to be linear.
    * Unless they are, auxvars will be introduced and they will be handles as variables, which can be an advantage in the context of extended formulations.
    */
   if( nchildren > 1 && nlhdlrdata->preferextended )
   {
      int i;
      for( i = 0; i < nchildren; ++i )
         SCIPsetConsExprExprCurvature(SCIPgetConsExprExprChildren(nlexpr)[i], SCIP_EXPRCURV_LINEAR);
#ifdef SCIP_DEBUG
      SCIPinfoMessage(scip, NULL, "require linearity for children of ");
      SCIPprintConsExprExpr(scip, conshdlr, origexpr, NULL);
      SCIPinfoMessage(scip, NULL, "\n");
#endif
   }

   /* add children expressions to to-do list (stack) */
   SCIP_CALL( exprstackPush(scip, stack, nchildren, SCIPgetConsExprExprChildren(nlexpr)) );

TERMINATE:
   SCIPfreeBufferArray(scip, &childcurv);

   return SCIP_OKAY;
}

/** curvature check and expression-growing methods
 * some day this could be plugins added by users at runtime, but for now we have a fixed list here
 * NOTE: curvCheckExprhdlr should be last
 */
static DECL_CURVCHECK((*CURVCHECKS[])) = { curvCheckProductComposite, curvCheckSignomial, curvCheckQuadratic, curvCheckExprhdlr };
/** number of curvcheck methods */
static const int NCURVCHECKS = sizeof(CURVCHECKS) / sizeof(void*);

/** checks whether expression is a sum with more than one child and each child being a variable ...
 *
 * ... or going to be a variable if expr is a nlhdlr-specific copy
 * Within constructExpr(), we can have an expression of any type which is a copy of an original expression,
 * but without children. At the end of constructExpr() (after the loop with the stack), these expressions
 * will remain as leafs and will eventually be turned into variables in collectLeafs(). Thus we treat
 * every child that has no children as if it were a variable. Theoretically, there is still the possibility
 * that it could be a constant (value-expression), but simplify should have removed these.
 */
static
SCIP_Bool exprIsMultivarLinear(
   SCIP_CONSHDLR*        conshdlr,           /**< constraint handler */
   SCIP_CONSEXPR_EXPR*   expr                /**< expression to check */
   )
{
   int nchildren;
   int c;

   assert(conshdlr != NULL);
   assert(expr != NULL);

   if( SCIPgetConsExprExprHdlr(expr) != SCIPgetConsExprExprHdlrSum(conshdlr) )
      return FALSE;

   nchildren = SCIPgetConsExprExprNChildren(expr);
   if( nchildren <= 1 )
      return FALSE;

   for( c = 0; c < nchildren; ++c )
      /*if( SCIPgetConsExprExprHdlr(SCIPgetConsExprExprChildren(expr)[c]) != SCIPgetConsExprExprHdlrVar(conshdlr) ) */
      if( SCIPgetConsExprExprNChildren(SCIPgetConsExprExprChildren(expr)[c]) > 0 )
         return FALSE;

   return TRUE;
}

/** construct a subexpression (as nlhdlr-expression) of maximal size that has a given curvature
 *
 * If the curvature cannot be achieved for an expression in the original expression graph,
 * then this expression becomes a leaf in the nlhdlr-expression.
 *
 * Sets *rootnlexpr to NULL if failed.
 */
static
SCIP_RETCODE constructExpr(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< constraint handler */
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata,     /**< nonlinear handler data */
   SCIP_CONSEXPR_EXPR**  rootnlexpr,         /**< buffer to store created expression */
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping from our expression copy to original expression */
   int*                  nleafs,             /**< number of leafs in constructed expression */
   SCIP_CONSEXPR_EXPR*   rootexpr,           /**< expression */
   SCIP_EXPRCURV         curv,               /**< curvature to achieve */
   SCIP_HASHMAP*         assumevarfixed,     /**< hashmap containing variables that should be assumed to be fixed, or NULL */
   SCIP_Bool*            curvsuccess         /**< pointer to store whether the curvature could be achieved w.r.t. the original variables (might be NULL) */
   )
{
   SCIP_CONSEXPR_EXPR* nlexpr;
   EXPRSTACK stack; /* to do list: expressions where to check whether they can have the desired curvature when taking their children into account */
   int oldstackpos;
   SCIP_Bool isrootexpr = TRUE;

   assert(scip != NULL);
   assert(nlhdlrdata != NULL);
   assert(rootnlexpr != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(nleafs != NULL);
   assert(rootexpr != NULL);
   assert(curv == SCIP_EXPRCURV_CONVEX || curv == SCIP_EXPRCURV_CONCAVE);

   /* create root expression */
   SCIP_CALL( nlhdlrExprCreate(scip, conshdlr, nlexpr2origexpr, rootnlexpr, rootexpr, curv) );

   *nleafs = 0;
   if( curvsuccess != NULL )
      *curvsuccess = TRUE;

   SCIP_CALL( exprstackInit(scip, &stack, 20) );
   SCIP_CALL( exprstackPush(scip, &stack, 1, rootnlexpr) );
   while( !exprstackIsEmpty(&stack) )
   {
      /* take expression from stack */
      nlexpr = exprstackPop(&stack);
      assert(nlexpr != NULL);
      assert(SCIPgetConsExprExprNChildren(nlexpr) == 0);

      oldstackpos = stack.stackpos;
      if( nlhdlrdata->isnlhdlrconvex && !SCIPhasConsExprExprHdlrBwdiff(SCIPgetConsExprExprHdlr(nlexpr)) )
      {
         /* if bwdiff is not implemented, then we could not generate cuts in the convex nlhdlr, so "stop" (treat nlexpr as variable) */
      }
      else if( !nlhdlrdata->isnlhdlrconvex && exprIsMultivarLinear(conshdlr, (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr)) )
      {
         /* if we are in the concave handler, we would like to treat linear multivariate subexpressions by a new auxvar always,
          * e.g., handle log(x+y) as log(z), z=x+y, because the estimation problem will be smaller then without making the estimator worse
          * (cons_nonlinear does this, too)
          * this check takes care of this when x and y are original variables
          * however, it isn't unlikely that we will have sums that become linear after we add auxvars for some children
          * this will be handled in a postprocessing below
          * for now, the check is performed on the original expression since there is not enough information in nlexpr yet
          */
#ifdef SCIP_MORE_DEBUG
         SCIPprintConsExprExpr(scip, conshdlr, SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr), NULL);
         SCIPinfoMessage(scip, NULL, "... is a multivariate linear sum that we'll treat as auxvar\n");
#endif
      }
      else if( SCIPgetConsExprExprCurvature(nlexpr) != SCIP_EXPRCURV_UNKNOWN )
      {
         /* if we are here, either convexity or concavity is required; try to check for this curvature */
         SCIP_Bool success;
         int method;

         /* try through curvature check methods until one succeeds */
         for( method = 0; method < NCURVCHECKS; ++method )
         {
            SCIP_CALL( CURVCHECKS[method](scip, conshdlr, nlexpr, isrootexpr, &stack, nlexpr2origexpr, nlhdlrdata, assumevarfixed, &success) );
            if( success )
               break;
         }
      }
      else
      {
         /* if we don't care about curvature in this subtree anymore (very unlikely),
          * then only continue iterating this subtree to assemble leaf expressions
          */
         SCIP_CALL( nlhdlrExprGrowChildren(scip, conshdlr, nlexpr2origexpr, nlexpr, NULL) );

         /* add children expressions, if any, to to-do list (stack) */
         SCIP_CALL( exprstackPush(scip, &stack, SCIPgetConsExprExprNChildren(nlexpr), SCIPgetConsExprExprChildren(nlexpr)) );
      }
      assert(stack.stackpos >= oldstackpos);  /* none of the methods above should have removed something from the stack */

      isrootexpr = FALSE;

      /* if nothing was added, then none of the successors of nlexpr were added to the stack
       * this is either because nlexpr was already a variable or value expressions, thus a leaf,
       * or because the desired curvature could not be achieved, so it will be handled as variables, thus a leaf
       */
      if( stack.stackpos == oldstackpos )
      {
         ++*nleafs;

         /* check whether the new leaf is not an original variable (or constant) */
         if( curvsuccess != NULL && !SCIPisConsExprExprVar(nlexpr) && !SCIPisConsExprExprValue(nlexpr) )
            *curvsuccess = FALSE;
      }
   }

   exprstackFree(scip, &stack);

   if( !nlhdlrdata->isnlhdlrconvex && *rootnlexpr != NULL )
   {
      /* remove multivariate linear subexpressions, that is, change some f(z1+z2) into f(z3) (z3=z1+z2 will be done by nlhdlr_default)
       * this handles the case that was not covered by the above check, which could recognize f(x+y) for x, y original variables
       */
      SCIP_CONSEXPR_ITERATOR* it;

      SCIP_CALL( SCIPexpriteratorCreate(&it, conshdlr, SCIPblkmem(scip)) );
      SCIP_CALL( SCIPexpriteratorInit(it, *rootnlexpr, SCIP_CONSEXPRITERATOR_DFS, FALSE) );
      SCIPexpriteratorSetStagesDFS(it, SCIP_CONSEXPRITERATOR_VISITINGCHILD);

      while( !SCIPexpriteratorIsEnd(it) )
      {
         SCIP_CONSEXPR_EXPR* child;

         child = SCIPexpriteratorGetChildExprDFS(it);
         assert(child != NULL);

         /* We want to change some f(x+y+z) into just f(), where f is the expression the iterator points to
          * and x+y+z is child. A child of a child, e.g., z, may not be a variable yet (these are added in collectLeafs later),
          * but an expression of some nonlinear type without children.
          */
         if( exprIsMultivarLinear(conshdlr, child) )
         {
            /* turn child (x+y+z) into a sum without children
             * collectLeafs() should then replace this by an auxvar
             */
#ifdef SCIP_MORE_DEBUG
            SCIPprintConsExprExpr(scip, conshdlr, child, NULL);
            SCIPinfoMessage(scip, NULL, "... is a multivariate linear sum that we'll treat as auxvar instead (postprocess)\n");
#endif

            /* TODO remove children from nlexpr2origexpr ?
             * should also do this if they are not used somewhere else; we could check nuses for this
             * however, it shouldn't matter to have some stray entries in the hashmap either
             */
            SCIP_CALL( SCIPremoveConsExprExprChildren(scip, child) );
            assert(SCIPgetConsExprExprNChildren(child) == 0);

            (void) SCIPexpriteratorSkipDFS(it);
         }
         else
         {
            (void) SCIPexpriteratorGetNext(it);
         }
      }

      SCIPexpriteratorFree(&it);
   }

   if( *rootnlexpr != NULL )
   {
      SCIP_Bool istrivial = TRUE;

      /* if handletrivial is enabled, then only require that rootnlexpr itself has required curvature (so has children; see below) and
       * that we are not a trivial sum  (because the previous implementation of this nlhdlr didn't allow this, either)
       */
      if( !nlhdlrdata->handletrivial || SCIPgetConsExprExprHdlr(*rootnlexpr) == SCIPgetConsExprExprHdlrSum(conshdlr) )
      {
         /* if all children do not have children, i.e., are variables, or will be replaced by auxvars, then free
          * also if rootnlexpr has no children, then free
          */
         int i;
         for( i = 0; i < SCIPgetConsExprExprNChildren(*rootnlexpr); ++i )
         {
            if( SCIPgetConsExprExprNChildren(SCIPgetConsExprExprChildren(*rootnlexpr)[i]) > 0 )
            {
               istrivial = FALSE;
               break;
            }
         }
      }
      else if( SCIPgetConsExprExprNChildren(*rootnlexpr) > 0 )  /* if handletrivial, then just require children */
            istrivial = FALSE;

      if( istrivial )
      {
         SCIP_CALL( SCIPreleaseConsExprExpr(scip, rootnlexpr) );
      }
   }

   return SCIP_OKAY;
}

/** collect (non-value) leaf expressions and ensure that they correspond to a variable (original or auxiliary)
 *
 * For children where we could not achieve the desired curvature, introduce an auxvar and replace the child by a var-expression that points to this auxvar.
 * Collect all leaf expressions (if not a value-expression) and index them.
 */
static
SCIP_RETCODE collectLeafs(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< constraint handler */
   SCIP_CONSEXPR_EXPR*   nlexpr,             /**< nlhdlr-expr */
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping from our expression copy to original */
   SCIP_HASHMAP*         leaf2index,         /**< mapping from leaf to index */
   int*                  nindices,           /**< number of indices */
   SCIP_Bool*            usingaux            /**< buffer to store whether auxiliary variable are used */
   )
{
   SCIP_CONSEXPR_ITERATOR* it;

   assert(nlexpr != NULL);
   assert(nlexpr2origexpr != NULL);
   assert(leaf2index != NULL);
   assert(nindices != NULL);
   assert(usingaux != NULL);

   assert(SCIPgetConsExprExprNChildren(nlexpr) > 0);
   assert(SCIPgetConsExprExprChildren(nlexpr) != NULL);

   *usingaux = FALSE;

   SCIP_CALL( SCIPexpriteratorCreate(&it, conshdlr, SCIPblkmem(scip)) );
   SCIP_CALL( SCIPexpriteratorInit(it, nlexpr, SCIP_CONSEXPRITERATOR_DFS, FALSE) );
   SCIPexpriteratorSetStagesDFS(it, SCIP_CONSEXPRITERATOR_VISITINGCHILD);

   for( nlexpr = SCIPexpriteratorGetCurrent(it); !SCIPexpriteratorIsEnd(it); nlexpr = SCIPexpriteratorGetNext(it) ) /*lint !e441*/
   {
      SCIP_CONSEXPR_EXPR* child;

      assert(nlexpr != NULL);

      /* check whether to-be-visited child needs to be replaced by a new expression (representing the auxvar) */
      child = SCIPexpriteratorGetChildExprDFS(it);
      if( SCIPgetConsExprExprNChildren(child) == 0 )
      {
         SCIP_CONSEXPR_EXPR* origexpr;

         origexpr = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlexpr2origexpr, (void*)child);
         assert(origexpr != NULL);

         if( SCIPgetConsExprExprNChildren(origexpr) > 0 )
         {
            SCIP_CONSEXPR_EXPR* newchild;
            int childidx;
            SCIP_VAR* var;

            /* having a child that had children in original but not in copy means that we could not achieve the desired curvature
             * thus, replace by a new child that points to the auxvar of the original expression
             */
            SCIP_CALL( SCIPcreateConsExprExprAuxVar(scip, conshdlr, origexpr, &var) );
            assert(var != NULL);
            SCIP_CALL( SCIPcreateConsExprExprVar(scip, conshdlr, &newchild, var) );  /* this captures newchild once */

            childidx = SCIPexpriteratorGetChildIdxDFS(it);
            SCIP_CALL( SCIPreplaceConsExprExprChild(scip, nlexpr, childidx, newchild) );  /* this captures newchild again */

            /* do not remove child->origexpr from hashmap, as child may appear again due to common subexprs (created by curvCheckProductComposite, for example)
             * if it doesn't reappear, though, but the memory address is reused, we need to make sure it points to the right origexpr
             */
            /* SCIP_CALL( SCIPhashmapRemove(nlexpr2origexpr, (void*)child) ); */
            SCIP_CALL( SCIPhashmapSetImage(nlexpr2origexpr, (void*)newchild, (void*)origexpr) );

            if( !SCIPhashmapExists(leaf2index, (void*)newchild) )
            {
               /* new leaf -> new index and remember in hashmap */
               SCIP_CALL( SCIPhashmapInsertInt(leaf2index, (void*)newchild, (*nindices)++) );
            }

            child = newchild;
            SCIP_CALL( SCIPreleaseConsExprExpr(scip, &newchild) );  /* because it was captured by both create and replace */

            /* remember that we use an auxvar */
            *usingaux = TRUE;
         }
         else if( SCIPisConsExprExprVar(child) )
         {
            /* if variable, then add to hashmap, if not already there */
            if( !SCIPhashmapExists(leaf2index, (void*)child) )
            {
               SCIP_CALL( SCIPhashmapInsertInt(leaf2index, (void*)child, (*nindices)++) );
            }
         }
         /* else: it's probably a value-expression, nothing to do */

         /* update integrality flag for future leaf expressions: convex nlhdlr may use this information */
         SCIP_CALL( SCIPcomputeConsExprExprIntegral(scip, conshdlr, child) );
      }
   }

   SCIPexpriteratorFree(&it);

   return SCIP_OKAY;
}

/** creates nonlinear handler expression data structure */
static
SCIP_RETCODE createNlhdlrExprData(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_CONSEXPR_NLHDLREXPRDATA** nlhdlrexprdata, /**< pointer to store nlhdlr expression data */
   SCIP_CONSEXPR_EXPR*   expr,               /**< original expression */
   SCIP_CONSEXPR_EXPR*   nlexpr,             /**< our copy of expression */
   SCIP_HASHMAP*         nlexpr2origexpr,    /**< mapping of expression copy to original */
   int                   nleafs              /**< number of leafs as counted by constructExpr */
   )
{
   SCIP_HASHMAP* leaf2index;
   SCIP_Bool usingaux;
   int i;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(nlhdlrexprdata != NULL);
   assert(*nlhdlrexprdata == NULL);
   assert(nlexpr != NULL);
   assert(nlexpr2origexpr != NULL);

   SCIP_CALL( SCIPallocClearBlockMemory(scip, nlhdlrexprdata) );
   (*nlhdlrexprdata)->nlexpr = nlexpr;
   (*nlhdlrexprdata)->nlexpr2origexpr = nlexpr2origexpr;

   /* make sure there are auxvars and collect all variables */
   SCIP_CALL( SCIPhashmapCreate(&leaf2index, SCIPblkmem(scip), nleafs) );
   (*nlhdlrexprdata)->nleafs = 0;  /* we start a new count, this time skipping value-expressions */
   SCIP_CALL( collectLeafs(scip, conshdlr, nlexpr, nlexpr2origexpr, leaf2index, &(*nlhdlrexprdata)->nleafs, &usingaux) );
   assert((*nlhdlrexprdata)->nleafs <= nleafs);  /* we should not have seen more leafs now than in constructExpr */

   /* assemble auxvars array */
   assert((*nlhdlrexprdata)->nleafs > 0);
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &(*nlhdlrexprdata)->leafexprs, (*nlhdlrexprdata)->nleafs) );
   for( i = 0; i < SCIPhashmapGetNEntries(leaf2index); ++i )
   {
      SCIP_HASHMAPENTRY* entry;
      SCIP_CONSEXPR_EXPR* leaf;
      int idx;

      entry = SCIPhashmapGetEntry(leaf2index, i);
      if( entry == NULL )
         continue;

      leaf = (SCIP_CONSEXPR_EXPR*) SCIPhashmapEntryGetOrigin(entry);
      assert(leaf != NULL);
      assert(SCIPgetConsExprExprAuxVar(leaf) != NULL);

      idx = SCIPhashmapEntryGetImageInt(entry);
      assert(idx >= 0);
      assert(idx < (*nlhdlrexprdata)->nleafs);

      (*nlhdlrexprdata)->leafexprs[idx] = leaf;

      SCIPdebugMsg(scip, "leaf %d: <%s>\n", idx, SCIPvarGetName(SCIPgetConsExprExprAuxVar(leaf)));
   }

   SCIPhashmapFree(&leaf2index);

#ifdef SCIP_DEBUG
   SCIPprintConsExprExpr(scip, conshdlr, nlexpr, NULL);
   SCIPinfoMessage(scip, NULL, " (%p) is handled as %s\n", SCIPhashmapGetImage(nlexpr2origexpr, (void*)nlexpr), SCIPexprcurvGetName(SCIPgetConsExprExprCurvature(nlexpr)));
#endif

   /* If we don't work on the extended formulation, then set curvature also in original expression
    * (in case someone wants to pick this up; this might be removed again).
    * This doesn't ensure that every convex or concave original expression is actually marked here.
    * Not only because our tests are incomprehensive, but also because we may not detect on sums,
    * prefer extended formulations (in nlhdlr_convex), or introduce auxvars for linear subexpressions
    * on purpose (in nlhdlr_concave).
    */
   if( !usingaux )
      SCIPsetConsExprExprCurvature(expr, SCIPgetConsExprExprCurvature(nlexpr));

   return SCIP_OKAY;
}

/** adds an estimator for a vertex-polyhedral (e.g., concave) function to a given rowprep */
static
SCIP_RETCODE estimateVertexPolyhedral(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_CONSEXPR_NLHDLR* nlhdlr,             /**< nonlinear handler */
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata, /**< nonlinear handler expression data */
   SCIP_SOL*             sol,                /**< solution to use, unless usemidpoint is TRUE */
   SCIP_Bool             usemidpoint,        /**< whether to use the midpoint of the domain instead of sol */
   SCIP_Bool             overestimate,       /**< whether over- or underestimating */
   SCIP_Real             targetvalue,        /**< a target value to achieve; if not reachable, then can give up early */
   SCIP_ROWPREP*         rowprep,            /**< rowprep where to store estimator */
   SCIP_Bool*            success             /**< buffer to store whether successful */
   )
{
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;
   VERTEXPOLYFUN_EVALDATA evaldata;
   SCIP_Real* xstar;
   SCIP_Real* box;
   SCIP_Real facetconstant;
   SCIP_VAR* var;
   int i;
   SCIP_Bool allfixed;

   assert(scip != NULL);
   assert(nlhdlr != NULL);
   assert(nlhdlrexprdata != NULL);
   assert(rowprep != NULL);
   assert(success != NULL);

   *success = FALSE;

   /* caller is responsible to have checked whether we can estimate, i.e., expression curvature and overestimate flag match */
   assert( overestimate || SCIPgetConsExprExprCurvature(nlhdlrexprdata->nlexpr) == SCIP_EXPRCURV_CONCAVE);  /* if underestimate, then must be concave */
   assert(!overestimate || SCIPgetConsExprExprCurvature(nlhdlrexprdata->nlexpr) == SCIP_EXPRCURV_CONVEX);   /* if overestimate, then must be convex */

#ifdef SCIP_DEBUG
   SCIPinfoMessage(scip, NULL, "%sestimate expression ", overestimate ? "over" : "under");
   SCIPprintConsExprExpr(scip, conshdlr, nlhdlrexprdata->nlexpr, NULL);
   SCIPinfoMessage(scip, NULL, " at point\n");
   for( i = 0; i < nlhdlrexprdata->nleafs; ++i )
   {
      var = SCIPgetConsExprExprVarVar(nlhdlrexprdata->leafexprs[i]);
      assert(var != NULL);

      SCIPinfoMessage(scip, NULL, "  <%s> = %g [%g,%g]\n", SCIPvarGetName(var),
         usemidpoint ? 0.5 * (SCIPvarGetLbLocal(var) + SCIPvarGetUbLocal(var)) : SCIPgetSolVal(scip, sol, var),
        SCIPvarGetLbLocal(var), SCIPvarGetUbLocal(var));
   }
#endif

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);

   if( nlhdlrdata->evalsol == NULL )
   {
      SCIP_CALL( SCIPcreateSol(scip, &nlhdlrdata->evalsol, NULL) );
   }

   evaldata.nlhdlrexprdata = nlhdlrexprdata;
   evaldata.evalsol = nlhdlrdata->evalsol;
   evaldata.scip = scip;
   evaldata.conshdlr = conshdlr;

   SCIP_CALL( SCIPallocBufferArray(scip, &xstar, nlhdlrexprdata->nleafs) );
   SCIP_CALL( SCIPallocBufferArray(scip, &box, 2*nlhdlrexprdata->nleafs) );

   allfixed = TRUE;
   for( i = 0; i < nlhdlrexprdata->nleafs; ++i )
   {
      var = SCIPgetConsExprExprVarVar(nlhdlrexprdata->leafexprs[i]);
      assert(var != NULL);

      box[2*i] = SCIPvarGetLbLocal(var);
      if( SCIPisInfinity(scip, -box[2*i]) )
      {
         SCIPdebugMsg(scip, "lower bound at -infinity, no estimate possible\n");
         goto TERMINATE;
      }

      box[2*i+1] = SCIPvarGetUbLocal(var);
      if( SCIPisInfinity(scip, box[2*i+1]) )
      {
         SCIPdebugMsg(scip, "upper bound at +infinity, no estimate possible\n");
         goto TERMINATE;
      }

      if( !SCIPisRelEQ(scip, box[2*i], box[2*i+1]) )
         allfixed = FALSE;

      if( usemidpoint )
         xstar[i] = 0.5 * (box[2*i] + box[2*i+1]);
      else
         xstar[i] = SCIPgetSolVal(scip, sol, var);
      assert(xstar[i] != SCIP_INVALID);  /*lint !e777*/
   }

   if( allfixed )
   {
      /* SCIPcomputeFacetVertexPolyhedral prints a warning and does not succeed if all is fixed */
      SCIPdebugMsg(scip, "all variables fixed, skip estimate\n");
      goto TERMINATE;
   }

   SCIP_CALL( SCIPensureRowprepSize(scip, rowprep, nlhdlrexprdata->nleafs + 1) );

   SCIP_CALL( SCIPcomputeFacetVertexPolyhedral(scip, conshdlr, overestimate, nlhdlrExprEvalConcave, (void*)&evaldata,
      xstar, box, nlhdlrexprdata->nleafs, targetvalue, success, rowprep->coefs, &facetconstant) );

   if( !*success )
   {
      SCIPdebugMsg(scip, "failed to compute facet of convex hull\n");
      goto TERMINATE;
   }

   rowprep->local = TRUE;
   rowprep->side = -facetconstant;
   rowprep->nvars = nlhdlrexprdata->nleafs;
   for( i = 0; i < nlhdlrexprdata->nleafs; ++i )
      rowprep->vars[i] = SCIPgetConsExprExprVarVar(nlhdlrexprdata->leafexprs[i]);

#ifdef SCIP_DEBUG
   SCIPinfoMessage(scip, NULL, "computed estimator: ");
   SCIPprintRowprep(scip, rowprep, NULL);
#endif

 TERMINATE:
   SCIPfreeBufferArray(scip, &box);
   SCIPfreeBufferArray(scip, &xstar);

   return SCIP_OKAY;
}

/** adds an estimator computed via a gradient to a given rowprep */
static
SCIP_RETCODE estimateGradient(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata, /**< nonlinear handler expression data */
   SCIP_SOL*             sol,                /**< solution to use */
   SCIP_Real             auxvalue,           /**< value of nlexpr in sol - we may not be able to take this value from nlexpr if it was evaluated at a different sol recently */
   SCIP_ROWPREP*         rowprep,            /**< rowprep where to store estimator */
   SCIP_Bool*            success             /**< buffer to store whether successful */
   )
{
   SCIP_CONSEXPR_EXPR* nlexpr;
   SCIP_Real QUAD(constant);
   int i;

   assert(nlhdlrexprdata != NULL);
   assert(rowprep != NULL);
   assert(success != NULL);

   nlexpr = nlhdlrexprdata->nlexpr;
   assert(nlexpr != NULL);

#ifdef SCIP_DEBUG
   SCIPinfoMessage(scip, NULL, "estimate expression ");
   SCIPprintConsExprExpr(scip, conshdlr, nlexpr, NULL);
   SCIPinfoMessage(scip, NULL, " by gradient\n");
#endif

   *success = FALSE;

   /* evaluation error -> skip */
   if( auxvalue == SCIP_INVALID )  /*lint !e777*/
   {
      SCIPdebugMsg(scip, "evaluation error / too large value (%g) for %p\n", auxvalue, (void*)nlexpr);
      return SCIP_OKAY;
   }

   /* compute gradient (TODO: this also reevaluates (soltag=0), which shouldn't be necessary unless we tried ConvexSecant before) */
   SCIP_CALL( SCIPcomputeConsExprExprGradient(scip, conshdlr, nlexpr, sol, 0) );

   /* gradient evaluation error -> skip */
   if( SCIPgetConsExprExprDerivative(nlexpr) == SCIP_INVALID ) /*lint !e777*/
   {
      SCIPdebugMsg(scip, "gradient evaluation error for %p\n", (void*)nlexpr);
      return SCIP_OKAY;
   }

   /* add gradient underestimator to rowprep: f(sol) + (x - sol) \nabla f(sol)
    * constant will store f(sol) - sol * \nabla f(sol)
    * to avoid some cancellation errors when linear variables take huge values (like 1e20),
    * we use double-double arithemtic here
    */
   QUAD_ASSIGN(constant, SCIPgetConsExprExprValue(nlexpr)); /* f(sol) */
   for( i = 0; i < nlhdlrexprdata->nleafs; ++i )
   {
      SCIP_VAR* var;
      SCIP_Real deriv;
      SCIP_Real varval;

      var = SCIPgetConsExprExprAuxVar(nlhdlrexprdata->leafexprs[i]);
      assert(var != NULL);

      deriv = SCIPgetConsExprExprPartialDiff(scip, conshdlr, nlexpr, var);
      if( deriv == SCIP_INVALID ) /*lint !e777*/
      {
         SCIPdebugMsg(scip, "gradient evaluation error for component %d of %p\n", i, (void*)nlexpr);
         return SCIP_OKAY;
      }

      varval = SCIPgetSolVal(scip, sol, var);

      SCIPdebugMsg(scip, "add %g * (<%s> - %g) to rowprep\n", deriv, SCIPvarGetName(var), varval);

      /* add deriv * var to rowprep and deriv * (-varval) to constant */
      SCIP_CALL( SCIPaddRowprepTerm(scip, rowprep, var, deriv) );
      SCIPquadprecSumQD(constant, constant, -deriv * varval);
   }

   SCIPaddRowprepConstant(rowprep, QUAD_TO_DBL(constant));
   rowprep->local = FALSE;

   *success = TRUE;

   return SCIP_OKAY;
}

/** adds an estimator generated by putting a secant through the coordinates given by the two closest integer points */
static
SCIP_RETCODE estimateConvexSecant(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< expression constraint handler */
   SCIP_CONSEXPR_NLHDLR* nlhdlr,             /**< nonlinear handler */
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata, /**< nonlinear handler expression data */
   SCIP_SOL*             sol,                /**< solution to use, unless usemidpoint is TRUE */
   SCIP_ROWPREP*         rowprep,            /**< rowprep where to store estimator */
   SCIP_Bool*            success             /**< buffer to store whether successful */
   )
{
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;
   SCIP_CONSEXPR_EXPR* nlexpr;
   SCIP_VAR* var;
   SCIP_Real x;
   SCIP_Real left, right;
   SCIP_Real fleft, fright;

   assert(nlhdlrexprdata != NULL);
   assert(nlhdlrexprdata->nleafs == 1);
   assert(rowprep != NULL);
   assert(success != NULL);

   nlexpr = nlhdlrexprdata->nlexpr;
   assert(nlexpr != NULL);

   *success = FALSE;

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);

   var = SCIPgetConsExprExprVarVar(nlhdlrexprdata->leafexprs[0]);
   assert(var != NULL);

   x = SCIPgetSolVal(scip, sol, var);

#ifdef SCIP_DEBUG
   SCIPinfoMessage(scip, NULL, "estimate expression ");
   SCIPprintConsExprExpr(scip, conshdlr, nlexpr, NULL);
   SCIPinfoMessage(scip, NULL, " by secant\n");
   SCIPinfoMessage(scip, NULL, "integral variable <%s> = %g [%g,%g]\n", SCIPvarGetName(var), x, SCIPvarGetLbGlobal(var), SCIPvarGetUbGlobal(var));
#endif

   /* find out coordinates of var left and right to sol */
   if( SCIPisIntegral(scip, x) )
   {
      x = SCIPround(scip, x);
      if( SCIPisEQ(scip, x, SCIPvarGetLbGlobal(var)) )
      {
         left = x;
         right = left + 1.0;
      }
      else
      {
         right = x;
         left = right - 1.0;
      }
   }
   else
   {
      left = SCIPfloor(scip, x);
      right = SCIPceil(scip, x);
   }
   assert(left != right); /*lint !e777*/

   /* now evaluate at left and right */
   if( nlhdlrdata->evalsol == NULL )
   {
      SCIP_CALL( SCIPcreateSol(scip, &nlhdlrdata->evalsol, NULL) );
   }

   SCIP_CALL( SCIPsetSolVal(scip, nlhdlrdata->evalsol, var, left) );
   SCIP_CALL( SCIPevalConsExprExpr(scip, conshdlr, nlexpr, nlhdlrdata->evalsol, 0) );

   /* evaluation error or a too large constant -> skip */
   fleft = SCIPgetConsExprExprValue(nlexpr);
   if( SCIPisInfinity(scip, REALABS(fleft)) )
   {
      SCIPdebugMsg(scip, "evaluation error / too large value (%g) for %p\n", SCIPgetConsExprExprValue(nlexpr), (void*)nlexpr);
      return SCIP_OKAY;
   }

   SCIP_CALL( SCIPsetSolVal(scip, nlhdlrdata->evalsol, var, right) );
   SCIP_CALL( SCIPevalConsExprExpr(scip, conshdlr, nlexpr, nlhdlrdata->evalsol, 0) );

   /* evaluation error or a too large constant -> skip */
   fright = SCIPgetConsExprExprValue(nlexpr);
   if( SCIPisInfinity(scip, REALABS(fright)) )
   {
      SCIPdebugMsg(scip, "evaluation error / too large value (%g) for %p\n", SCIPgetConsExprExprValue(nlexpr), (void*)nlexpr);
      return SCIP_OKAY;
   }

   SCIPdebugMsg(scip, "f(%g)=%g, f(%g)=%g\n", left, fleft, right, fright);

   /* skip if too steep
    * for clay0204h, this resulted in a wrong cut from f(0)=1e12 f(1)=0.99998,
    * since due to limited precision, this was handled as if f(1)=1
    */
   if( (!SCIPisZero(scip, fleft)  && REALABS(fright/fleft)*SCIPepsilon(scip) > 1.0) ||
       (!SCIPisZero(scip, fright) && REALABS(fleft/fright)*SCIPepsilon(scip) > 1.0) )
   {
      SCIPdebugMsg(scip, "function is too steep, abandoning\n");
      return SCIP_OKAY;
   }

   /* now add f(left) + (f(right) - f(left)) * (x - left) as estimator to rowprep */
   SCIP_CALL( SCIPaddRowprepTerm(scip, rowprep, var, fright - fleft) );
   SCIPaddRowprepConstant(rowprep, fleft - (fright - fleft) * left);
   rowprep->local = FALSE;

   *success = TRUE;

   return SCIP_OKAY;
}

/*
 * Callback methods of nonlinear handler
 */

static
SCIP_DECL_CONSEXPR_NLHDLRFREEHDLRDATA(nlhdlrfreeHdlrDataConvexConcave)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(nlhdlrdata != NULL);
   assert(*nlhdlrdata != NULL);
   assert((*nlhdlrdata)->evalsol == NULL);

   SCIPfreeBlockMemory(scip, nlhdlrdata);

   return SCIP_OKAY;
}

/** callback to free expression specific data */
static
SCIP_DECL_CONSEXPR_NLHDLRFREEEXPRDATA(nlhdlrfreeExprDataConvexConcave)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(nlhdlrexprdata != NULL);
   assert(*nlhdlrexprdata != NULL);

   SCIPfreeBlockMemoryArray(scip, &(*nlhdlrexprdata)->leafexprs, (*nlhdlrexprdata)->nleafs);
   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &(*nlhdlrexprdata)->nlexpr) );
   SCIPhashmapFree(&(*nlhdlrexprdata)->nlexpr2origexpr);

   SCIPfreeBlockMemory(scip, nlhdlrexprdata);

   return SCIP_OKAY;
}

static
SCIP_DECL_CONSEXPR_NLHDLREXIT(nlhdlrExitConvex)
{
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);

   if( nlhdlrdata->evalsol != NULL )
   {
      SCIP_CALL( SCIPfreeSol(scip, &nlhdlrdata->evalsol) );
   }

   return SCIP_OKAY;
}

static
SCIP_DECL_CONSEXPR_NLHDLRDETECT(nlhdlrDetectConvex)
{ /*lint --e{715}*/
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;
   SCIP_CONSEXPR_EXPR* nlexpr = NULL;
   SCIP_HASHMAP* nlexpr2origexpr;
   int nleafs = 0;

   assert(scip != NULL);
   assert(nlhdlr != NULL);
   assert(expr != NULL);
   assert(enforcemethods != NULL);
   assert(enforcedbelow != NULL);
   assert(enforcedabove != NULL);
   assert(success != NULL);
   assert(nlhdlrexprdata != NULL);

   *success = FALSE;

   /* we currently cannot contribute in presolve */
   if( SCIPgetStage(scip) != SCIP_STAGE_SOLVING )
      return SCIP_OKAY;

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);
   assert(nlhdlrdata->isnlhdlrconvex);

   /* ignore pure constants and variables */
   if( SCIPgetConsExprExprNChildren(expr) == 0 )
      return SCIP_OKAY;

   SCIPdebugMsg(scip, "nlhdlr_convex detect for expr %p\n", (void*)expr);

   /* initialize mapping from copied expression to original one
    * 20 is not a bad estimate for the size of convex subexpressions that we can usually discover
    * when expressions will be allowed to store "user"data, we could get rid of this hashmap (TODO)
    */
   SCIP_CALL( SCIPhashmapCreate(&nlexpr2origexpr, SCIPblkmem(scip), 20) );

   if( !*enforcedbelow )
   {
      SCIP_CALL( constructExpr(scip, conshdlr, nlhdlrdata, &nlexpr, nlexpr2origexpr, &nleafs, expr,
         SCIP_EXPRCURV_CONVEX, NULL, NULL) );
      if( nlexpr != NULL )
      {
         assert(SCIPgetConsExprExprNChildren(nlexpr) > 0);  /* should not be trivial */

         *enforcedbelow = TRUE;
         *enforcemethods |= SCIP_CONSEXPR_EXPRENFO_SEPABELOW;
         *success = TRUE;

         SCIPdebugMsg(scip, "detected expr %p to be convex -> can enforce expr <= auxvar\n", (void*)expr);
      }
      else
      {
         SCIP_CALL( SCIPhashmapRemoveAll(nlexpr2origexpr) );
      }
   }

   if( !*enforcedabove && nlexpr == NULL )
   {
      SCIP_CALL( constructExpr(scip, conshdlr, nlhdlrdata, &nlexpr, nlexpr2origexpr, &nleafs, expr,
         SCIP_EXPRCURV_CONCAVE, NULL, NULL) );
      if( nlexpr != NULL )
      {
         assert(SCIPgetConsExprExprNChildren(nlexpr) > 0);  /* should not be trivial */

         *enforcedabove = TRUE;
         *enforcemethods |= SCIP_CONSEXPR_EXPRENFO_SEPAABOVE;
         *success = TRUE;

         SCIPdebugMsg(scip, "detected expr %p to be concave -> can enforce expr >= auxvar\n", (void*)expr);
      }
   }

   assert(*success || nlexpr == NULL);
   if( !*success )
   {
      SCIPhashmapFree(&nlexpr2origexpr);
      return SCIP_OKAY;
   }

   /* store variable expressions into the expression data of the nonlinear handler */
   SCIP_CALL( createNlhdlrExprData(scip, conshdlr, nlhdlrexprdata, expr, nlexpr, nlexpr2origexpr, nleafs) );

   return SCIP_OKAY;
}

/** auxiliary evaluation callback */
static
SCIP_DECL_CONSEXPR_NLHDLREVALAUX(nlhdlrEvalAuxConvexConcave)
{ /*lint --e{715}*/
   assert(nlhdlrexprdata != NULL);
   assert(nlhdlrexprdata->nlexpr != NULL);
   assert(auxvalue != NULL);

   SCIP_CALL( SCIPevalConsExprExpr(scip, SCIPfindConshdlr(scip, "expr"), nlhdlrexprdata->nlexpr, sol, 0) );
   *auxvalue = SCIPgetConsExprExprValue(nlhdlrexprdata->nlexpr);

   return SCIP_OKAY;
}

/** estimator callback */
static
SCIP_DECL_CONSEXPR_NLHDLRESTIMATE(nlhdlrEstimateConvex)
{ /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* nlexpr;
   SCIP_EXPRCURV curvature;
   SCIP_ROWPREP* rowprep;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(nlhdlrexprdata != NULL);

   nlexpr = nlhdlrexprdata->nlexpr;
   assert(nlexpr != NULL);
   assert(SCIPhashmapGetImage(nlhdlrexprdata->nlexpr2origexpr, (void*)nlexpr) == expr);
   assert(rowpreps != NULL);
   assert(success != NULL);

   *success = FALSE;
   *addedbranchscores = FALSE;

   /* if estimating on non-convex side, then do nothing */
   curvature = SCIPgetConsExprExprCurvature(nlexpr);
   assert(curvature == SCIP_EXPRCURV_CONVEX || curvature == SCIP_EXPRCURV_CONCAVE);
   if( ( overestimate && curvature == SCIP_EXPRCURV_CONVEX) ||
       (!overestimate && curvature == SCIP_EXPRCURV_CONCAVE) )
      return SCIP_OKAY;

   /* we can skip eval as nlhdlrEvalAux should have been called for same solution before */
   /* SCIP_CALL( nlhdlrExprEval(scip, nlexpr, sol) ); */
   assert(auxvalue == SCIPgetConsExprExprValue(nlexpr)); /* given value (originally from nlhdlrEvalAuxConvexConcave) should coincide with the one stored in nlexpr */  /*lint !e777*/

   SCIP_CALL( SCIPcreateRowprep(scip, &rowprep, overestimate ? SCIP_SIDETYPE_LEFT : SCIP_SIDETYPE_RIGHT, TRUE) );

   if( nlhdlrexprdata->nleafs == 1 && SCIPisConsExprExprIntegral(nlhdlrexprdata->leafexprs[0]) )
   {
      SCIP_CALL( estimateConvexSecant(scip, conshdlr, nlhdlr, nlhdlrexprdata, sol, rowprep, success) );

      (void) SCIPsnprintf(rowprep->name, SCIP_MAXSTRLEN, "%sestimate_convexsecant%p_%s%d",
         overestimate ? "over" : "under",
         (void*)expr,
         sol != NULL ? "sol" : "lp",
         sol != NULL ? SCIPsolGetIndex(sol) : SCIPgetNLPs(scip));
   }

   /* if secant method was not used or failed, then try with gradient */
   if( !*success )
   {
      SCIP_CALL( estimateGradient(scip, conshdlr, nlhdlrexprdata, sol, auxvalue, rowprep, success) );

      (void) SCIPsnprintf(rowprep->name, SCIP_MAXSTRLEN, "%sestimate_convexgradient%p_%s%d",
         overestimate ? "over" : "under",
         (void*)expr,
         sol != NULL ? "sol" : "lp",
         sol != NULL ? SCIPsolGetIndex(sol) : SCIPgetNLPs(scip));
   }

   if( *success )
   {
      SCIP_CALL( SCIPsetPtrarrayVal(scip, rowpreps, 0, rowprep) );
   }
   else
   {
      SCIPfreeRowprep(scip, &rowprep);
   }

   return SCIP_OKAY;
}

static
SCIP_DECL_CONSEXPR_NLHDLRCOPYHDLR(nlhdlrCopyhdlrConvex)
{ /*lint --e{715}*/
   assert(targetscip != NULL);
   assert(targetconsexprhdlr != NULL);
   assert(sourcenlhdlr != NULL);
   assert(strcmp(SCIPgetConsExprNlhdlrName(sourcenlhdlr), CONVEX_NLHDLR_NAME) == 0);

   SCIP_CALL( SCIPincludeConsExprNlhdlrConvex(targetscip, targetconsexprhdlr) );

   return SCIP_OKAY;
}

/** includes convex nonlinear handler to consexpr */
SCIP_RETCODE SCIPincludeConsExprNlhdlrConvex(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        consexprhdlr        /**< expression constraint handler */
   )
{
   SCIP_CONSEXPR_NLHDLR* nlhdlr;
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   assert(scip != NULL);
   assert(consexprhdlr != NULL);

   SCIP_CALL( SCIPallocBlockMemory(scip, &nlhdlrdata) );
   nlhdlrdata->isnlhdlrconvex = TRUE;
   nlhdlrdata->evalsol = NULL;

   SCIP_CALL( SCIPincludeConsExprNlhdlrBasic(scip, consexprhdlr, &nlhdlr, CONVEX_NLHDLR_NAME, CONVEX_NLHDLR_DESC,
      CONVEX_NLHDLR_DETECTPRIORITY, CONVEX_NLHDLR_ENFOPRIORITY, nlhdlrDetectConvex, nlhdlrEvalAuxConvexConcave, nlhdlrdata) );
   assert(nlhdlr != NULL);

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/detectsum",
      "whether to run convexity detection when the root of an expression is a non-quadratic sum",
      &nlhdlrdata->detectsum, FALSE, DEFAULT_DETECTSUM, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/preferextended",
      "whether to prefer extended formulations",
      &nlhdlrdata->preferextended, FALSE, DEFAULT_PREFEREXTENDED, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/cvxquadratic",
      "whether to use convexity check on quadratics",
      &nlhdlrdata->cvxquadratic, TRUE, DEFAULT_CVXQUADRATIC_CONVEX, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/cvxsignomial",
      "whether to use convexity check on signomials",
      &nlhdlrdata->cvxsignomial, TRUE, DEFAULT_CVXSIGNOMIAL, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/cvxprodcomp",
      "whether to use convexity check on product composition f(h)*h",
      &nlhdlrdata->cvxprodcomp, TRUE, DEFAULT_CVXPRODCOMP, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONVEX_NLHDLR_NAME "/handletrivial",
      "whether to also handle trivial convex expressions",
      &nlhdlrdata->handletrivial, TRUE, DEFAULT_HANDLETRIVIAL, NULL, NULL) );

   SCIPsetConsExprNlhdlrFreeHdlrData(scip, nlhdlr, nlhdlrfreeHdlrDataConvexConcave);
   SCIPsetConsExprNlhdlrCopyHdlr(scip, nlhdlr, nlhdlrCopyhdlrConvex);
   SCIPsetConsExprNlhdlrFreeExprData(scip, nlhdlr, nlhdlrfreeExprDataConvexConcave);
   SCIPsetConsExprNlhdlrSepa(scip, nlhdlr, NULL, NULL, nlhdlrEstimateConvex, NULL);
   SCIPsetConsExprNlhdlrInitExit(scip, nlhdlr, NULL, nlhdlrExitConvex);

   return SCIP_OKAY;
}






static
SCIP_DECL_CONSEXPR_NLHDLREXIT(nlhdlrExitConcave)
{
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);

   if( nlhdlrdata->evalsol != NULL )
   {
      SCIP_CALL( SCIPfreeSol(scip, &nlhdlrdata->evalsol) );
   }

   return SCIP_OKAY;
}

static
SCIP_DECL_CONSEXPR_NLHDLRDETECT(nlhdlrDetectConcave)
{ /*lint --e{715}*/
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;
   SCIP_CONSEXPR_EXPR* nlexpr = NULL;
   SCIP_HASHMAP* nlexpr2origexpr;
   int nleafs = 0;
   int c;

   assert(scip != NULL);
   assert(nlhdlr != NULL);
   assert(expr != NULL);
   assert(enforcemethods != NULL);
   assert(enforcedbelow != NULL);
   assert(enforcedabove != NULL);
   assert(success != NULL);
   assert(nlhdlrexprdata != NULL);

   *success = FALSE;

   /* we currently cannot contribute in presolve */
   if( SCIPgetStage(scip) != SCIP_STAGE_SOLVING )
      return SCIP_OKAY;

   nlhdlrdata = SCIPgetConsExprNlhdlrData(nlhdlr);
   assert(nlhdlrdata != NULL);
   assert(!nlhdlrdata->isnlhdlrconvex);

   /* ignore pure constants and variables */
   if( SCIPgetConsExprExprNChildren(expr) == 0 )
      return SCIP_OKAY;

   SCIPdebugMsg(scip, "nlhdlr_concave detect for expr %p\n", (void*)expr);

   /* initialize mapping from copied expression to original one
    * 20 is not a bad estimate for the size of concave subexpressions that we can usually discover
    * when expressions will be allowed to store "user"data, we could get rid of this hashmap (TODO)
    */
   SCIP_CALL( SCIPhashmapCreate(&nlexpr2origexpr, SCIPblkmem(scip), 20) );

   if( !*enforcedbelow )
   {
      SCIP_CALL( constructExpr(scip, conshdlr, nlhdlrdata, &nlexpr, nlexpr2origexpr, &nleafs, expr,
         SCIP_EXPRCURV_CONCAVE, NULL, NULL) );

      if( nlexpr != NULL && nleafs > SCIP_MAXVERTEXPOLYDIM )
      {
         SCIPdebugMsg(scip, "Too many variables (%d) in constructed expression. Will not be able to estimate. Rejecting.\n", nleafs);
         SCIP_CALL( SCIPreleaseConsExprExpr(scip, &nlexpr) );
      }

      if( nlexpr != NULL )
      {
         assert(SCIPgetConsExprExprNChildren(nlexpr) > 0);  /* should not be trivial */

         *enforcedbelow = TRUE;
         *enforcemethods |= SCIP_CONSEXPR_EXPRENFO_SEPABELOW;
         *success = TRUE;

         SCIPdebugMsg(scip, "detected expr %p to be concave -> can enforce expr <= auxvar\n", (void*)expr);
      }
      else
      {
         SCIP_CALL( SCIPhashmapRemoveAll(nlexpr2origexpr) );
      }
   }

   if( !*enforcedabove && nlexpr == NULL )
   {
      SCIP_CALL( constructExpr(scip, conshdlr, nlhdlrdata, &nlexpr, nlexpr2origexpr, &nleafs, expr,
         SCIP_EXPRCURV_CONVEX, NULL, NULL) );

      if( nlexpr != NULL && nleafs > SCIP_MAXVERTEXPOLYDIM )
      {
         SCIPdebugMsg(scip, "Too many variables (%d) in constructed expression. Will not be able to estimate. Rejecting.\n", nleafs);
         SCIP_CALL( SCIPreleaseConsExprExpr(scip, &nlexpr) );
      }

      if( nlexpr != NULL )
      {
         assert(SCIPgetConsExprExprNChildren(nlexpr) > 0);  /* should not be trivial */

         *enforcedabove = TRUE;
         *enforcemethods |= SCIP_CONSEXPR_EXPRENFO_SEPAABOVE;
         *success = TRUE;

         SCIPdebugMsg(scip, "detected expr %p to be convex -> can enforce expr >= auxvar\n", (void*)expr);
      }
   }

   assert(*success || nlexpr == NULL);
   if( !*success )
   {
      SCIPhashmapFree(&nlexpr2origexpr);
      return SCIP_OKAY;
   }

   /* store variable expressions into the expression data of the nonlinear handler */
   SCIP_CALL( createNlhdlrExprData(scip, conshdlr, nlhdlrexprdata, expr, nlexpr, nlexpr2origexpr, nleafs) );

   /* mark expressions whose bounds are important for constructing the estimators (basically all possible branching
    * candidates that are registered in nlhdlrEstimateConcave)
    */
   for( c = 0; c < (*nlhdlrexprdata)->nleafs; ++c )
   {
      SCIP_CONSEXPR_EXPR* leaf;

      leaf = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage((*nlhdlrexprdata)->nlexpr2origexpr, (*nlhdlrexprdata)->leafexprs[c]);
      assert(leaf != NULL);

      SCIP_CALL( SCIPincrementConsExprExprNDomainUses(scip, conshdlr, leaf) );
   }

   return SCIP_OKAY;
}

/** init sepa callback that initializes LP */
static
SCIP_DECL_CONSEXPR_NLHDLRINITSEPA(nlhdlrInitSepaConcave)
{
   SCIP_CONSEXPR_EXPR* nlexpr;
   SCIP_EXPRCURV curvature;
   SCIP_Bool success;
   SCIP_ROWPREP* rowprep = NULL;
   SCIP_ROW* row;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(nlhdlrexprdata != NULL);

   nlexpr = nlhdlrexprdata->nlexpr;
   assert(nlexpr != NULL);
   assert(SCIPhashmapGetImage(nlhdlrexprdata->nlexpr2origexpr, (void*)nlexpr) == expr);

   curvature = SCIPgetConsExprExprCurvature(nlexpr);
   assert(curvature == SCIP_EXPRCURV_CONVEX || curvature == SCIP_EXPRCURV_CONCAVE);
   /* we can only be estimating on non-convex side */
   if( curvature == SCIP_EXPRCURV_CONCAVE )
      overestimate = FALSE;
   else if( curvature == SCIP_EXPRCURV_CONVEX )
      underestimate = FALSE;
   if( !overestimate && !underestimate )
      return SCIP_OKAY;

   /* compute estimator and store in rowprep */
   SCIP_CALL( SCIPcreateRowprep(scip, &rowprep, overestimate ? SCIP_SIDETYPE_LEFT : SCIP_SIDETYPE_RIGHT, TRUE) );
   SCIP_CALL( estimateVertexPolyhedral(scip, conshdlr, nlhdlr, nlhdlrexprdata, NULL, TRUE, overestimate, overestimate ? SCIPinfinity(scip) : -SCIPinfinity(scip), rowprep, &success) );
   if( !success )
   {
      SCIPdebugMsg(scip, "failed to compute facet of convex hull\n");
      goto TERMINATE;
   }

   /* add auxiliary variable */
   SCIP_CALL( SCIPaddRowprepTerm(scip, rowprep, SCIPgetConsExprExprAuxVar(expr), -1.0) );

   /* straighten out numerics */
   SCIP_CALL( SCIPcleanupRowprep2(scip, rowprep, NULL, SCIP_CONSEXPR_CUTMAXRANGE, SCIPgetHugeValue(scip), &success) );
   if( !success )
   {
      SCIPdebugMsg(scip, "failed to cleanup rowprep numerics\n");
      goto TERMINATE;
   }

   (void) SCIPsnprintf(rowprep->name, SCIP_MAXSTRLEN, "%sestimate_concave%p_initsepa", overestimate ? "over" : "under", (void*)expr);
   SCIP_CALL( SCIPgetRowprepRowCons(scip, &row, rowprep, cons) );

#ifdef SCIP_DEBUG
   SCIPinfoMessage(scip, NULL, "initsepa computed row: ");
   SCIPprintRow(scip, row, NULL);
#endif

   SCIP_CALL( SCIPaddRow(scip, row, FALSE, infeasible) );
   SCIP_CALL( SCIPreleaseRow(scip, &row) );

 TERMINATE:
   if( rowprep != NULL )
      SCIPfreeRowprep(scip, &rowprep);

   return SCIP_OKAY;
}

/** estimator callback */
static
SCIP_DECL_CONSEXPR_NLHDLRESTIMATE(nlhdlrEstimateConcave)
{ /*lint --e{715}*/
   SCIP_CONSEXPR_EXPR* nlexpr;
   SCIP_EXPRCURV curvature;
   SCIP_ROWPREP* rowprep;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(nlhdlrexprdata != NULL);
   assert(rowpreps != NULL);
   assert(success != NULL);

   *success = FALSE;
   *addedbranchscores = FALSE;

   nlexpr = nlhdlrexprdata->nlexpr;
   assert(nlexpr != NULL);
   assert(SCIPhashmapGetImage(nlhdlrexprdata->nlexpr2origexpr, (void*)nlexpr) == expr);

   /* if estimating on non-concave side, then do nothing */
   curvature = SCIPgetConsExprExprCurvature(nlexpr);
   assert(curvature == SCIP_EXPRCURV_CONVEX || curvature == SCIP_EXPRCURV_CONCAVE);
   if( ( overestimate && curvature == SCIP_EXPRCURV_CONCAVE) ||
       (!overestimate && curvature == SCIP_EXPRCURV_CONVEX) )
      return SCIP_OKAY;

   SCIP_CALL( SCIPcreateRowprep(scip, &rowprep, overestimate ? SCIP_SIDETYPE_LEFT : SCIP_SIDETYPE_RIGHT, TRUE) );

   SCIP_CALL( estimateVertexPolyhedral(scip, conshdlr, nlhdlr, nlhdlrexprdata, sol, FALSE, overestimate, targetvalue, rowprep, success) );

   if( *success )
   {
      SCIP_CALL( SCIPsetPtrarrayVal(scip, rowpreps, 0, rowprep) );

      (void) SCIPsnprintf(rowprep->name, SCIP_MAXSTRLEN, "%sestimate_concave%p_%s%d",
         overestimate ? "over" : "under",
         (void*)expr,
         sol != NULL ? "sol" : "lp",
         sol != NULL ? SCIPsolGetIndex(sol) : SCIPgetNLPs(scip));
   }
   else
   {
      SCIPfreeRowprep(scip, &rowprep);
   }

   if( addbranchscores )
   {
      SCIP_Real violation;

      /* check how much is the violation on the side that we estimate */
      if( auxvalue == SCIP_INVALID ) /*lint !e777*/
      {
         /* if cannot evaluate, then always branch */
         violation = SCIPinfinity(scip);
      }
      else
      {
         SCIP_Real auxval;

         /* get value of auxiliary variable of this expression */
         assert(SCIPgetConsExprExprAuxVar(expr) != NULL);
         auxval = SCIPgetSolVal(scip, sol, SCIPgetConsExprExprAuxVar(expr));

         /* compute the violation
          * if we underestimate, then we enforce expr <= auxval, so violation is (positive part of) auxvalue - auxval
          * if we overestimate,  then we enforce expr >= auxval, so violation is (positive part of) auxval - auxvalue
          */
         if( !overestimate )
            violation = MAX(0.0, auxvalue - auxval);
         else
            violation = MAX(0.0, auxval - auxvalue);
      }
      assert(violation >= 0.0);

      /* add violation as branching-score to expressions; the core will take care distributing this onto variables */
      if( nlhdlrexprdata->nleafs == 1 )
      {
         SCIP_CONSEXPR_EXPR* e;
         e = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlhdlrexprdata->nlexpr2origexpr, nlhdlrexprdata->leafexprs[0]);
         SCIP_CALL( SCIPaddConsExprExprsViolScore(scip, conshdlr, &e, 1, violation, sol, addedbranchscores) );
      }
      else
      {
         SCIP_CONSEXPR_EXPR** exprs;
         int c;

         /* map leaf expressions back to original expressions
          * TODO do this once at end of detect and store in nlhdlrexprdata
          */
         SCIP_CALL( SCIPallocBufferArray(scip, &exprs, nlhdlrexprdata->nleafs) );
         for( c = 0; c < nlhdlrexprdata->nleafs; ++c )
               exprs[c] = (SCIP_CONSEXPR_EXPR*)SCIPhashmapGetImage(nlhdlrexprdata->nlexpr2origexpr, nlhdlrexprdata->leafexprs[c]);

         SCIP_CALL( SCIPaddConsExprExprsViolScore(scip, conshdlr, exprs, nlhdlrexprdata->nleafs, violation, sol, addedbranchscores) );

         SCIPfreeBufferArray(scip, &exprs);
      }
   }

   return SCIP_OKAY;
}

static
SCIP_DECL_CONSEXPR_NLHDLRCOPYHDLR(nlhdlrCopyhdlrConcave)
{ /*lint --e{715}*/
   assert(targetscip != NULL);
   assert(targetconsexprhdlr != NULL);
   assert(sourcenlhdlr != NULL);
   assert(strcmp(SCIPgetConsExprNlhdlrName(sourcenlhdlr), CONCAVE_NLHDLR_NAME) == 0);

   SCIP_CALL( SCIPincludeConsExprNlhdlrConcave(targetscip, targetconsexprhdlr) );

   return SCIP_OKAY;
}

/** includes concave nonlinear handler to consexpr */
SCIP_RETCODE SCIPincludeConsExprNlhdlrConcave(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        consexprhdlr        /**< expression constraint handler */
   )
{
   SCIP_CONSEXPR_NLHDLR* nlhdlr;
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   assert(scip != NULL);
   assert(consexprhdlr != NULL);

   SCIP_CALL( SCIPallocBlockMemory(scip, &nlhdlrdata) );
   nlhdlrdata->isnlhdlrconvex = FALSE;
   nlhdlrdata->evalsol = NULL;

   SCIP_CALL( SCIPincludeConsExprNlhdlrBasic(scip, consexprhdlr, &nlhdlr, CONCAVE_NLHDLR_NAME, CONCAVE_NLHDLR_DESC,
      CONCAVE_NLHDLR_DETECTPRIORITY, CONCAVE_NLHDLR_ENFOPRIORITY, nlhdlrDetectConcave, nlhdlrEvalAuxConvexConcave, nlhdlrdata) );
   assert(nlhdlr != NULL);

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/detectsum",
      "whether to run convexity detection when the root of an expression is a sum",
      &nlhdlrdata->detectsum, FALSE, DEFAULT_DETECTSUM, NULL, NULL) );

   /*SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/preferextended",
      "whether to prefer extended formulations",
      &nlhdlrdata->preferextended, FALSE, DEFAULT_PREFEREXTENDED, NULL, NULL) );*/
   /* "extended" formulations of a concave expressions can give worse estimators */
   nlhdlrdata->preferextended = FALSE;

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/cvxquadratic",
      "whether to use convexity check on quadratics",
      &nlhdlrdata->cvxquadratic, TRUE, DEFAULT_CVXQUADRATIC_CONCAVE, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/cvxsignomial",
      "whether to use convexity check on signomials",
      &nlhdlrdata->cvxsignomial, TRUE, DEFAULT_CVXSIGNOMIAL, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/cvxprodcomp",
      "whether to use convexity check on product composition f(h)*h",
      &nlhdlrdata->cvxprodcomp, TRUE, DEFAULT_CVXPRODCOMP, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "constraints/expr/nlhdlr/" CONCAVE_NLHDLR_NAME "/handletrivial",
      "whether to also handle trivial convex expressions",
      &nlhdlrdata->handletrivial, TRUE, DEFAULT_HANDLETRIVIAL, NULL, NULL) );

   SCIPsetConsExprNlhdlrFreeHdlrData(scip, nlhdlr, nlhdlrfreeHdlrDataConvexConcave);
   SCIPsetConsExprNlhdlrCopyHdlr(scip, nlhdlr, nlhdlrCopyhdlrConcave);
   SCIPsetConsExprNlhdlrFreeExprData(scip, nlhdlr, nlhdlrfreeExprDataConvexConcave);
   SCIPsetConsExprNlhdlrSepa(scip, nlhdlr, nlhdlrInitSepaConcave, NULL, nlhdlrEstimateConcave, NULL);
   SCIPsetConsExprNlhdlrInitExit(scip, nlhdlr, NULL, nlhdlrExitConcave);

   return SCIP_OKAY;
}

/** checks whether a given expression is convex or concave w.r.t. the original variables
 *
 * This function uses the methods that are used in the detection algorithm of the convex nonlinear handler.
 */
SCIP_RETCODE SCIPhasConsExprExprCurvature(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        conshdlr,           /**< constraint handler */
   SCIP_CONSEXPR_EXPR*   expr,               /**< expression */
   SCIP_EXPRCURV         curv,               /**< curvature to check for */
   SCIP_Bool*            success,            /**< buffer to store whether expression has curvature curv (w.r.t. original variables) */
   SCIP_HASHMAP*         assumevarfixed      /**< hashmap containing variables that should be assumed to be fixed, or NULL */
   )
{
   SCIP_CONSEXPR_NLHDLRDATA nlhdlrdata;
   SCIP_CONSEXPR_EXPR* rootnlexpr;
   SCIP_HASHMAP* nlexpr2origexpr;
   int nleafs;

   assert(conshdlr != NULL);
   assert(expr != NULL);
   assert(curv != SCIP_EXPRCURV_UNKNOWN);
   assert(success != NULL);

   /* create temporary hashmap */
   SCIP_CALL( SCIPhashmapCreate(&nlexpr2origexpr, SCIPblkmem(scip), 20) );

   /* prepare nonlinear handler data */
   nlhdlrdata.isnlhdlrconvex = TRUE;
   nlhdlrdata.evalsol = NULL;
   nlhdlrdata.detectsum = TRUE;
   nlhdlrdata.preferextended = FALSE;
   nlhdlrdata.cvxquadratic = TRUE;
   nlhdlrdata.cvxsignomial = TRUE;
   nlhdlrdata.cvxprodcomp = TRUE;
   nlhdlrdata.handletrivial = TRUE;

   SCIP_CALL( constructExpr(scip, conshdlr, &nlhdlrdata, &rootnlexpr, nlexpr2origexpr, &nleafs, expr, curv, assumevarfixed, success) );

   /* free created expression */
   if( rootnlexpr != NULL )
   {
      SCIP_CALL( SCIPreleaseConsExprExpr(scip, &rootnlexpr) );
   }

   /* free hashmap */
   SCIPhashmapFree(&nlexpr2origexpr);

   return SCIP_OKAY;
}
