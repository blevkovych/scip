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

/**@file   binarytree.c
 * @brief  unittest for the binary tree datastructure in misc.c
 * @author Merlin Viernickel
 */

/*--+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>

#include "scip/scip.h"
#include "scip/pub_misc.h"

#include "include/scip_test.h"

static SCIP* scip;
static SCIP_BT* binarytree;
static int mydata = 4;

static
void setup(void)
{
   /* create scip */
   SCIP_CALL( SCIPcreate(&scip) );

   /* create binary tree */
   SCIP_CALL( SCIPbtCreate(&binarytree, SCIPblkmem(scip)) );
}


static
void teardown(void)
{
   /* free activity */
   SCIPbtFree(&binarytree);

   /* free scip */
   SCIP_CALL( SCIPfree(&scip) );
}


TestSuite(binarytree, .init = setup, .fini = teardown);

Test(binarytree, setup_and_teardown, .description = "test that setup and teardown work correctly")
{
}

Test(binarytree, test_binarytree_empty, .description = "test that the binary tree checks emptiness correctly.")
{
   cr_assert(SCIPbtIsEmpty(binarytree));
}

Test(binarytree, test_binarytree_full, .description = "test that the binary tree adds nodes correctly.")
{
   SCIP_BTNODE* root;
   SCIP_BTNODE* lchild;
   SCIP_BTNODE* rchild;

   /* create nodes */
   SCIP_CALL( SCIPbtnodeCreate(binarytree, &root, NULL) );
   SCIP_CALL( SCIPbtnodeCreate(binarytree, &lchild, NULL) );
   SCIP_CALL( SCIPbtnodeCreate(binarytree, &rchild, NULL) );

   /* set root */
   SCIPbtSetRoot(binarytree, root);

   /* set children */
   SCIPbtnodeSetLeftchild(root, lchild);
   SCIPbtnodeSetParent(lchild, root);
   SCIPbtnodeSetRightchild(root, rchild);
   SCIPbtnodeSetParent(rchild, root);

   /* check tree structure */
   cr_assert(SCIPbtnodeIsRoot(root));
   cr_assert(SCIPbtnodeIsLeftchild(lchild));
   cr_assert(SCIPbtnodeIsRightchild(rchild));
   cr_assert(SCIPbtnodeIsLeaf(lchild));
   cr_assert(SCIPbtnodeIsLeaf(rchild));
   cr_assert_eq(root, SCIPbtGetRoot(binarytree));
   cr_assert_eq(rchild, SCIPbtnodeGetSibling(lchild));
   cr_assert_eq(lchild, SCIPbtnodeGetSibling(rchild));
   cr_assert_eq(root, SCIPbtnodeGetParent(lchild));
   cr_assert_eq(root, SCIPbtnodeGetParent(rchild));
   cr_assert_eq(lchild, SCIPbtnodeGetLeftchild(root));
   cr_assert_eq(rchild, SCIPbtnodeGetRightchild(root));
}

Test(binarytree, test_binarytree_data, .description = "test that the binary tree stores entry data correctly.")
{
   SCIP_BTNODE* root;
   int* ptr;

   /* create node */
   SCIP_CALL( SCIPbtnodeCreate(binarytree, &root, NULL) );

   SCIPbtnodeSetData(root, (void*) &mydata);

   ptr = (int*) SCIPbtnodeGetData(root);
   cr_assert_eq(mydata, *ptr);

   SCIPbtnodeFree(binarytree, &root);
}
