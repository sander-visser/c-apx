/*****************************************************************************
* \file      testsuite_apx_client.c
* \author    Conny Gustafsson
* \date      2020-01-27
* \brief     Unit tests for apx_client_t
*
* Copyright (c) 2020 Conny Gustafsson
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:

* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
******************************************************************************/
//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include "apx_client.h"
#include "apx_clientEventListenerSpy.h"
#include "CuTest.h"
#ifdef MEM_LEAK_CHECK
#include "CMemLeak.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
static const char *m_apx_definition1 = "APX/1.2\n"
      "N\"TestNode1\"\n"
      "P\"VehicleSpeed\"S:=65535\n"
      "\n";

static const char *m_apx_definition2 = "APX/1.2\n"
      "N\"TestNode2\"\n"
      "R\"VehicleSpeed\"S:=65535\n"
      "\n";
//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void test_apx_client_create(CuTest* tc);
static void test_apx_client_buildNodeFromString1(CuTest* tc);
static void test_apx_client_buildNodeFromString2(CuTest* tc);
static void test_apx_client_registerEventHandler(CuTest* tc);

//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
CuSuite* testSuite_apx_client(void)
{
   CuSuite* suite = CuSuiteNew();

   SUITE_ADD_TEST(suite, test_apx_client_create);
   SUITE_ADD_TEST(suite, test_apx_client_buildNodeFromString1);
   SUITE_ADD_TEST(suite, test_apx_client_buildNodeFromString2);
   SUITE_ADD_TEST(suite, test_apx_client_registerEventHandler);

   return suite;
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
static void test_apx_client_create(CuTest* tc)
{
   apx_client_t *client = apx_client_new();
   CuAssertPtrNotNull(tc, client);
   apx_client_delete(client);
}

static void test_apx_client_buildNodeFromString1(CuTest* tc)
{

   apx_client_t *client = apx_client_new();
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_client_buildNode_cstr(client, m_apx_definition1));
   apx_nodeInstance_t *node = apx_client_getLastAttachedNode(client);
   CuAssertPtrNotNull(tc, node);
   apx_nodeInfo_t *nodeInfo = apx_nodeInstance_getNodeInfo(node);
   CuAssertPtrNotNull(tc, nodeInfo);
   CuAssertIntEquals(tc, 1, apx_nodeInfo_getNumProvidePorts(nodeInfo));
   CuAssertIntEquals(tc, 0, apx_nodeInfo_getNumRequirePorts(nodeInfo));
   apx_client_delete(client);
}

static void test_apx_client_buildNodeFromString2(CuTest* tc)
{

   apx_client_t *client = apx_client_new();
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_client_buildNode_cstr(client, m_apx_definition2));
   apx_nodeInstance_t *node = apx_client_getLastAttachedNode(client);
   CuAssertPtrNotNull(tc, node);
   apx_nodeInfo_t *nodeInfo = apx_nodeInstance_getNodeInfo(node);
   CuAssertPtrNotNull(tc, nodeInfo);
   CuAssertIntEquals(tc, 0, apx_nodeInfo_getNumProvidePorts(nodeInfo));
   CuAssertIntEquals(tc, 1, apx_nodeInfo_getNumRequirePorts(nodeInfo));
   apx_client_delete(client);
}

static void test_apx_client_registerEventHandler(CuTest* tc)
{
   apx_client_t *client = apx_client_new();
   apx_clientEventListenerSpy_t *spy = apx_clientEventListenerSpy_new();
   CuAssertPtrNotNull(tc, client);
   CuAssertPtrNotNull(tc, spy);


   CuAssertIntEquals(tc, 0, apx_client_getNumEventListeners(client));
   void *handle = apx_clientEventListenerSpy_register(spy, client);
   CuAssertIntEquals(tc, 1, apx_client_getNumEventListeners(client));
   apx_client_unregisterEventListener(client, handle);
   CuAssertIntEquals(tc, 0, apx_client_getNumEventListeners(client));


   apx_client_delete(client);
   apx_clientEventListenerSpy_delete(spy);
}