/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2005 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2005 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic License.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: Heur2opt.cpp,v 1.1 2005/03/16 17:02:46 bzfpfend Exp $"

/**@file   heur2opt.cpp
 * @brief  C++ wrapper for primal heuristics
 * @author Timo Berthold
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <cassert>
#include <iostream>
#include "gminucut.h"
#include "Heur2opt.h"
#include "TSPProbData.h"



using namespace tsp;
using namespace std;


/** method finding the edge going from the node with id index1 to the node with id index2 */
static
GRAPHEDGE* findEdge(
   GRAPHNODE*    nodes,              /**< all nodes of the graph */
   GRAPHNODE*    node1,              /**< id of the node where the searched edge starts */
   GRAPHNODE*    node2               /**< id of the node where the searched edge ends */
   )
{
   GRAPHEDGE* startedge;
   GRAPHEDGE* edge;

   startedge = node1->first_edge;
   assert(startedge != NULL);
   edge = startedge;

   // regard every outgoing edge of node index1 and stop if adjacent to node index2
   do
   {
      if( edge->adjac == node2 )
         return edge;
      edge = edge->next;
   }
   while(startedge != edge);

   return NULL;
}

/** destructor of primal heuristic to free user data (called when SCIP is exiting) */
RETCODE Heur2opt::scip_free(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur                /**< the primal heuristic itself */
   )
{
   return SCIP_OKAY;
}
   
/** initialization method of primal heuristic (called after problem was transformed) */
RETCODE Heur2opt::scip_init(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur                /**< the primal heuristic itself */
   )
{
   return SCIP_OKAY;
}
   
/** deinitialization method of primal heuristic (called before transformed problem is freed) */
RETCODE Heur2opt::scip_exit(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur                /**< the primal heuristic itself */
   )
{
   return SCIP_OKAY;
}

/** solving process initialization method of primal heuristic (called when branch and bound process is about to begin)
 *
 *  This method is called when the presolving was finished and the branch and bound process is about to begin.
 *  The primal heuristic may use this call to initialize its branch and bound specific data.
 *
 */
RETCODE Heur2opt::scip_initsol(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur                /**< the primal heuristic itself */
   )
{
   TSPProbData* probdata = dynamic_cast<TSPProbData*>(SCIPgetObjProbData(scip));
   graph_ = probdata->getGraph();
   capture_graph(graph_);

   ncalls_ = 0;
   sol_ = NULL;
   CHECK_OKAY( SCIPallocMemoryArray(scip, &tour_, graph_->nnodes) );
   
   return SCIP_OKAY;
}
   
/** solving process deinitialization method of primal heuristic (called before branch and bound process data is freed)
 *
 *  This method is called before the branch and bound process is freed.
 *  The primal heuristic should use this call to clean up its branch and bound data.
 */
RETCODE Heur2opt::scip_exitsol(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur                /**< the primal heuristic itself */
   )
{
   release_graph(&graph_);
   SCIPfreeMemoryArray(scip, &tour_);

   return SCIP_OKAY;
}


/** execution method of primal heuristic 2-Opt */
RETCODE Heur2opt::scip_exec(
   SCIP*         scip,               /**< SCIP data structure */
   HEUR*         heur,               /**< the primal heuristic itself */
   RESULT*       result              /**< pointer to store the result of the heuristic call */
   )
{  
   assert( heur != NULL );
   SOL* sol = SCIPgetBestSol( scip );
   bool newsol;

   //check whether a new solution was found meanwhile
   if(sol != sol_)
   {
      sol_ = sol;
      ncalls_ = 0;
      newsol = true;
   }
   else
      newsol = false;

   ncalls_++;

   int nnodes = graph_->nnodes;

   // some cases need not to be handled
   if( nnodes < 4 || sol == NULL || ncalls_ >= nnodes )
   {
      *result = SCIP_DIDNOTRUN;
      return SCIP_OKAY;
   }

   *result= SCIP_DIDNOTFIND;

   GRAPHNODE* nodes = graph_->nodes; 

   //get tour from sol and sort edges by length, if new solution was found
   if(newsol)
   {
      GRAPHEDGE* edge;
      GRAPHEDGE* lastedge = NULL;
      GRAPHNODE* node = &nodes[0];
      int i = 0; 

      do
      {
         edge = node->first_edge;      
         assert( edge != NULL );

         do
         {
            // find the next edge of the tour 
            if( edge->back != lastedge && SCIPgetSolVal(scip, sol, edge->var) > 0.5 )
            {
               node = edge->adjac;
               lastedge = edge;

               int j;
               // shift edge through the (sorted) array 
               for(j = i; j > 0 && tour_[j-1]->length < edge->length; j-- )
               {
                  tour_[j] = tour_[j-1];
               }                
               // and insert the edge at the rigth position
               tour_[j] = edge; 

               i++;
               break;
            }

            edge = edge->next;

         }
         while ( edge != node->first_edge );
         assert( edge != node->first_edge );  

      }
      while ( node != &nodes[0] );
      assert( i == nnodes );

   }

   GRAPHEDGE** edges2test;
   CHECK_OKAY( SCIPallocBufferArray(scip, &edges2test, 4) ); 

   // test current edge with all 'longer' edges for improvement if swapping with crossing edges (though do 2Opt for one edge)
   for( int i = 0; i < ncalls_ && *result != SCIP_FOUNDSOL; i++ )
   {
      edges2test[0] = tour_[ncalls_];
      edges2test[1] = tour_[i];
      edges2test[2] = findEdge( nodes, edges2test[0]->back->adjac, edges2test[1]->back->adjac );  
      edges2test[3] = findEdge( nodes, edges2test[0]->adjac, edges2test[1]->adjac );
             
      // if the new solution is better, update and end
      if( edges2test[0]->length + edges2test[1]->length > edges2test[2]->length + edges2test[3]->length )
      {

         Bool success;
         SOL* swapsol; // copy of sol with 4 edges swapped 

         CHECK_OKAY( SCIPcreateSol (scip, &swapsol, heur) );
         // copy the old solution
         for( int j = 0; j < nnodes; j++)
         {
            CHECK_OKAY( SCIPsetSolVal(scip, swapsol, tour_[j]->var, 1.0) );
         }

         // and replace two edges
         CHECK_OKAY( SCIPsetSolVal(scip, swapsol, edges2test[0]->var, 0.0) );
         CHECK_OKAY( SCIPsetSolVal(scip, swapsol, edges2test[1]->var, 0.0) );
         CHECK_OKAY( SCIPsetSolVal(scip, swapsol, edges2test[2]->var, 1.0) );
         CHECK_OKAY( SCIPsetSolVal(scip, swapsol, edges2test[3]->var, 1.0) );
         CHECK_OKAY( SCIPaddSolFree(scip, &swapsol, &success) );

         assert(success);
         *result = SCIP_FOUNDSOL;                   
         ncalls_ = 0;

      }
   }
   SCIPfreeBufferArray(scip, &edges2test);

   return SCIP_OKAY;
}
