/* test_client has been autogenerated by py-apx generator 0.3.4 */

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <stdio.h>
#include "ApxNode_test_client.h"
#include "pack.h"

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
#define APX_DEFINITON_LEN 187u
#define APX_IN_PORT_DATA_LEN 4u
#define APX_OUT_PORT_DATA_LEN 1u

//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// LOCAL VARIABLES
//////////////////////////////////////////////////////////////////////////////
static const uint8_t m_outPortInitData[APX_OUT_PORT_DATA_LEN]= {
   15
};

static uint8_t m_outPortdata[APX_OUT_PORT_DATA_LEN];
static uint8_t m_outPortDirtyFlags[APX_OUT_PORT_DATA_LEN];
static const uint8_t m_inPortInitData[APX_IN_PORT_DATA_LEN]= {
   3, 255, 255, 255
};

static uint8_t m_inPortdata[APX_IN_PORT_DATA_LEN];
static uint8_t m_inPortDirtyFlags[APX_IN_PORT_DATA_LEN];
static apx_nodeData_t m_nodeData;
static const char *m_apxDefinitionData=
"APX/1.2\n"
"N\"test_client\"\n"
"T\"OffOn_T\"C(0,3)\n"
"T\"Percent_T\"C\n"
"T\"VehicleSpeed_T\"S\n"
"P\"VehicleMode\"C(0,15):=15\n"
"R\"EngineRunningStatus\"T[0]:=3\n"
"R\"FuelLevelPercent\"T[1]:=255\n"
"R\"VehicleSpeed\"T[2]:=0xFFFF\n"
"\n";

//////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
apx_nodeData_t * ApxNode_Init_test_client(void)
{
   memcpy(&m_inPortdata[0], &m_inPortInitData[0], APX_IN_PORT_DATA_LEN);
   memset(&m_inPortDirtyFlags[0], 0, sizeof(m_inPortDirtyFlags));
   memcpy(&m_outPortdata[0], &m_outPortInitData[0], APX_OUT_PORT_DATA_LEN);
   memset(&m_outPortDirtyFlags[0], 0, sizeof(m_outPortDirtyFlags));
   apx_nodeData_create(&m_nodeData, "test_client", (uint8_t*) &m_apxDefinitionData[0], APX_DEFINITON_LEN, &m_inPortdata[0], &m_inPortDirtyFlags[0], APX_IN_PORT_DATA_LEN, &m_outPortdata[0], &m_outPortDirtyFlags[0], APX_OUT_PORT_DATA_LEN);
   return &m_nodeData;
}

apx_nodeData_t * ApxNode_GetNodeData_test_client(void)
{
   return &m_nodeData;
}

bool ApxNode_IsConnected_test_client(void)
{
   return ( apx_nodeData_isInPortDataOpen(&m_nodeData) && apx_nodeData_isOutPortDataOpen(&m_nodeData) );
}

Std_ReturnType ApxNode_Read_test_client_EngineRunningStatus(OffOn_T *val)
{
   *val = (uint8_t) m_inPortdata[0];
   return E_OK;
}

Std_ReturnType ApxNode_Read_test_client_FuelLevelPercent(Percent_T *val)
{
   *val = (uint8_t) m_inPortdata[1];
   return E_OK;
}

Std_ReturnType ApxNode_Read_test_client_VehicleSpeed(VehicleSpeed_T *val)
{
   apx_nodeData_lockInPortData(&m_nodeData);
   *val = (uint16_t) unpackLE(&m_inPortdata[2],(uint8_t) sizeof(uint16_t));
   apx_nodeData_unlockInPortData(&m_nodeData);
   return E_OK;
}

Std_ReturnType ApxNode_Write_test_client_VehicleMode(uint8 val)
{
   apx_nodeData_lockOutPortData(&m_nodeData);
   m_outPortdata[0]=(uint8_t) val;
   apx_nodeData_outPortDataWriteNotify(&m_nodeData, 0, 1, false);
   return E_OK;
}

void test_client_inPortDataWritten(void *arg, apx_nodeData_t *nodeData, uint32_t offset, uint32_t len)
{
   (void)arg;
   (void)nodeData;
   (void)offset;
   (void)len;
}
//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
