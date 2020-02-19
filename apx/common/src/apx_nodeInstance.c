/*****************************************************************************
* \file      apx_nodeInstance.c
* \author    Conny Gustafsson
* \date      2019-12-02
* \brief     Parent container for all things node-related.
*
* Copyright (c) 2019-2020 Conny Gustafsson
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
#include <assert.h>
#include <stdio.h> //DEBUG ONLY
#include "apx_nodeInstance.h"
#include "apx_connectionBase.h"
#include "rmf.h"

#ifdef MEM_LEAK_CHECK
#include "CMemLeak.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
typedef apx_portDataProps_t* (apx_getPortDataPropsFunc)(const apx_nodeInfo_t *self, apx_portId_t portId);
//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static apx_error_t apx_nodeInstance_definitionFileWriteNotify(void *arg, apx_file_t *file, uint32_t offset, const uint8_t *src, uint32_t len);
static apx_error_t apx_nodeInstance_definitionFileOpenNotify(void *arg, struct apx_file_tag *file);
static apx_error_t apx_nodeInstance_definitionFileReadData(void *arg, apx_file_t*file, uint32_t offset, uint8_t *dest, uint32_t len);
static apx_error_t apx_nodeInstance_createFileInfo(apx_nodeInstance_t *self, const char *fileExtension, uint32_t fileSize, apx_fileInfo_t *fileInfo);
static apx_error_t apx_nodeInstance_providePortDataFileWriteNotify(void *arg, apx_file_t *file, uint32_t offset, const uint8_t *src, uint32_t len);
static apx_error_t apx_nodeInstance_providePortDataFileOpenNotify(void *arg, struct apx_file_tag *file);
static void apx_nodeInstance_initPortRefs(apx_nodeInstance_t *self, apx_portRef_t *portRefs, apx_portCount_t numPorts, uint32_t portIdMask, apx_getPortDataPropsFunc *getPortDataProps);


//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

void apx_nodeInstance_create(apx_nodeInstance_t *self, apx_mode_t mode)
{
   if ( (self != 0) && ( (mode == APX_CLIENT_MODE) || (mode == APX_SERVER_MODE) ))
   {
      memset(self, 0, sizeof(apx_nodeInstance_t));
      self->mode = mode;
      self->nodeState = APX_NODE_STATE_STAGING; ///TODO: should this be calculated instead?
      self->requirePortState = APX_REQUIRE_PORT_DATA_STATE_INIT;
      self->providePortState = APX_PROVIDE_PORT_DATE_STATE_INIT;
   }
}

void apx_nodeInstance_destroy(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      if (self->parseTree != 0)
      {
         apx_node_delete(self->parseTree);
      }
      if (self->nodeInfo != 0)
      {
         apx_nodeInfo_delete(self->nodeInfo);
      }
      if (self->nodeData != 0)
      {
         apx_nodeData_delete(self->nodeData);
      }
      if (self->requirePortReferences != 0)
      {
         free(self->requirePortReferences);
         self->requirePortReferences = 0;
      }
      if (self->providePortReferences != 0)
      {
         free(self->providePortReferences);
         self->providePortReferences = 0;
      }
      if (self->portTriggerList != 0)
      {
         apx_portTriggerList_delete(self->portTriggerList);
      }
   }
}

apx_nodeInstance_t *apx_nodeInstance_new(apx_mode_t mode)
{
   apx_nodeInstance_t *self = (apx_nodeInstance_t*) malloc(sizeof(apx_nodeInstance_t));
   if(self != 0)
   {
      apx_nodeInstance_create(self, mode);
      apx_nodeData_t *nodeData = apx_nodeInstance_createNodeData(self);
      if (nodeData == 0)
      {
         free(self);
         self = (apx_nodeInstance_t*) 0;
      }
   }
   return self;
}

void apx_nodeInstance_delete(apx_nodeInstance_t *self)
{
   if(self != 0)
   {
      apx_nodeInstance_destroy(self);
      free(self);
   }
}

void apx_nodeInstance_vdelete(void *arg)
{
   apx_nodeInstance_delete((apx_nodeInstance_t*) arg);
}

apx_nodeData_t* apx_nodeInstance_createNodeData(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      if (self->nodeData == 0)
      {
         self->nodeData = apx_nodeData_new();
      }
      return self->nodeData;
   }
   return (apx_nodeData_t*) 0;
}

apx_nodeData_t* apx_nodeInstance_getNodeData(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->nodeData;
   }
   return (apx_nodeData_t*) 0;
}




/**
 * SERVER MODE ONLY
 */
apx_error_t apx_nodeInstance_parseDefinition(apx_nodeInstance_t *self, apx_parser_t *parser)
{
   if ( (self != 0) && (parser != 0) )
   {
      if ( self->nodeData != 0)
      {
         apx_size_t definitionLen = apx_nodeData_getDefinitionDataLen(self->nodeData);
         if (definitionLen > 0)
         {
            apx_node_t *parseTree;
            apx_nodeData_lockDefinitionData(self->nodeData);
            parseTree = apx_parser_parseBuffer(parser, apx_nodeData_getDefinitionDataBuf(self->nodeData), definitionLen);
            apx_nodeData_unlockDefinitionData(self->nodeData);
            if (parseTree != 0)
            {
               apx_parser_clearNodes(parser);
               self->parseTree = parseTree;
               return APX_NO_ERROR;
            }
            else
            {
               return apx_parser_getLastError(parser);
            }
         }
         return APX_LENGTH_ERROR;
      }
      return APX_NULL_PTR_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_node_t *apx_nodeInstance_getParseTree(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->parseTree;
   }
   return (apx_node_t*) 0;
}

apx_error_t apx_nodeInstance_createDefinitionBuffer(apx_nodeInstance_t *self, apx_size_t bufferLen)
{
   if ( (self != 0) && (bufferLen > 0) && (bufferLen <= APX_MAX_DEFINITION_LEN))
   {
      if (self->nodeData != 0)
      {
         return apx_nodeData_createDefinitionBuffer(self->nodeData, bufferLen);
      }
      return APX_NULL_PTR_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_error_t apx_nodeInstance_createPortDataBuffers(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      apx_error_t retval = APX_NO_ERROR;
      apx_nodeInfo_t *nodeInfo;
      apx_nodeData_t *nodeData;
      apx_size_t requirePortDataLen;
      apx_size_t providePortDataLen;
      assert(self != 0);
      nodeInfo = apx_nodeInstance_getNodeInfo(self);
      nodeData = apx_nodeInstance_getNodeData(self);
      if(nodeInfo == 0 || nodeData == 0)
      {
         return APX_NULL_PTR_ERROR;
      }
      requirePortDataLen = apx_nodeInfo_calcRequirePortDataLen(nodeInfo);
      providePortDataLen = apx_nodeInfo_calcProvidePortDataLen(nodeInfo);
      if (providePortDataLen > 0u)
      {
         retval = apx_nodeData_createProvidePortBuffer(nodeData, providePortDataLen);
         if (retval == APX_NO_ERROR)
         {
            apx_size_t initDataSize = apx_nodeInfo_getProvidePortInitDataSize(nodeInfo);
            if (initDataSize == providePortDataLen)
            {
               const uint8_t *initData = apx_nodeInfo_getProvidePortInitDataPtr(nodeInfo);
               assert(initData != 0);
               apx_nodeData_writeProvidePortData(nodeData, initData, 0, initDataSize);
            }
            else
            {
               return APX_LENGTH_ERROR;
            }
         }
      }
      if ( (retval == APX_NO_ERROR) && (requirePortDataLen > 0u))
      {
         retval = apx_nodeData_createRequirePortBuffer(nodeData, requirePortDataLen);
         if (retval == APX_NO_ERROR)
         {
            apx_size_t initDataSize = apx_nodeInfo_getRequirePortInitDataSize(nodeInfo);
            if (initDataSize == requirePortDataLen)
            {
               const uint8_t *initData = apx_nodeInfo_getRequirePortInitDataPtr(nodeInfo);
               assert(initData != 0);
               apx_nodeData_writeRequirePortData(nodeData, initData, 0, initDataSize);
            }
            else
            {
               return APX_LENGTH_ERROR;
            }
         }
      }
      return retval;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}



apx_error_t apx_nodeInstance_updatePortDataDirect(apx_nodeInstance_t *destNode, apx_portDataProps_t *destDataProps, apx_nodeInstance_t *srcNode, apx_portDataProps_t *srcDataProps)
{
   return APX_NOT_IMPLEMENTED_ERROR;
}

/**
 * destPortId must reference a require-port in destNodeData. Likewise, srcPortId must reference a provide port in srcNodeData;
 */
apx_error_t apx_nodeInstance_updatePortDataDirectById(apx_nodeInstance_t *destNode, apx_portId_t destPortId, apx_nodeInstance_t *srcNode, apx_portId_t srcPortId)
{
   if ( (destNode != 0) && (destPortId >= 0) && (destPortId < destNode->nodeInfo->numRequirePorts) && (srcNode != 0) && (srcPortId >= 0) && (srcPortId < srcNode->nodeInfo->numProvidePorts) )
   {
      if ( (destNode->nodeData == 0) || (srcNode->nodeData == 0))
      {
         return APX_NULL_PTR_ERROR;
      }
      else
      {
         apx_portDataProps_t *destDataProps;
         apx_portDataProps_t *srcDataProps;
         destDataProps = apx_nodeInfo_getRequirePortDataProps(destNode->nodeInfo, destPortId);
         srcDataProps = apx_nodeInfo_getProvidePortDataProps(srcNode->nodeInfo, srcPortId);
         return apx_nodeInstance_updatePortDataDirect(destNode, destDataProps, srcNode, srcDataProps);
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_error_t apx_nodeInstance_buildNodeInfo(apx_nodeInstance_t *self, apx_programType_t *errProgramType, apx_uniquePortId_t *errPortId)
{
   if ( (self != 0) && (errProgramType != 0) && (errPortId != 0))
   {
      if (self->nodeInfo == 0)
      {
         self->nodeInfo = apx_nodeInfo_new();
         if (self->nodeInfo == 0)
         {
            return APX_MEM_ERROR;
         }
      }
      if (self->parseTree != 0)
      {
         apx_error_t rc;
         apx_compiler_t compiler;
         apx_compiler_create(&compiler);
         assert(self->nodeInfo != 0);
         rc = apx_nodeInfo_build(self->nodeInfo, self->parseTree, &compiler, self->mode, errProgramType, errPortId);
         apx_compiler_destroy(&compiler);
         if (rc != APX_NO_ERROR)
         {
            apx_nodeInfo_delete(self->nodeInfo);
            self->nodeInfo = 0;
         }
         return rc;
      }
      return APX_NULL_PTR_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_nodeInfo_t *apx_nodeInstane_getNodeInfo(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->nodeInfo;
   }
   return (apx_nodeInfo_t*) 0;
}

/**
 * Builds port reference data structures in this nodeInstance.
 * This call is only allowed after successfully calling apx_nodeInstance_buildNodeInfo (It's depending on data generated inside nodeInfo)
 */
apx_error_t apx_nodeInstance_buildPortRefs(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      uint32_t numRequirePorts;
      uint32_t numProvidePorts;
      size_t allocSize;
      if (self->nodeInfo == 0)
      {
         return APX_NULL_PTR_ERROR;
      }
      numRequirePorts = apx_nodeInfo_getNumRequirePorts(self->nodeInfo);
      numProvidePorts = apx_nodeInfo_getNumProvidePorts(self->nodeInfo);
      if (numRequirePorts > 0)
      {
         allocSize = numRequirePorts * sizeof(apx_portRef_t);
         self->requirePortReferences = (apx_portRef_t*) malloc(allocSize);
         if (self->requirePortReferences == 0)
         {
            return APX_MEM_ERROR;
         }
         else
         {
            apx_nodeInstance_initPortRefs(self, self->requirePortReferences, numRequirePorts, 0u, apx_nodeInfo_getRequirePortDataProps);
         }
      }
      if (numProvidePorts > 0)
      {
         allocSize = numProvidePorts * sizeof(apx_portRef_t);
         self->providePortReferences = (apx_portRef_t*) malloc(allocSize);
         if (self->providePortReferences == 0)
         {
            if (self->requirePortReferences != 0)
            {
               free(self->requirePortReferences);
               self->requirePortReferences = (apx_portRef_t*) 0;
            }
            return APX_MEM_ERROR;
         }
         else
         {
            apx_nodeInstance_initPortRefs(self, self->providePortReferences, numProvidePorts, APX_PORT_ID_PROVIDE_PORT,  apx_nodeInfo_getProvidePortDataProps);
         }
      }
      return APX_NO_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_portRef_t *apx_nodeInstance_getPortRef(apx_nodeInstance_t *self, apx_uniquePortId_t portId)
{
   if ( ((portId & APX_PORT_ID_PROVIDE_PORT) != 0u ))
   {
      return apx_nodeInstance_getProvidePortRef(self, portId & APX_PORT_ID_MASK);
   }
   return (apx_portRef_t*) 0;
}

apx_portRef_t *apx_nodeInstance_getRequirePortRef(apx_nodeInstance_t *self, apx_portId_t portId)
{
   if ( (self != 0) && (self->nodeInfo != 0) && (self->requirePortReferences != 0) )
   {
      apx_portCount_t numRequirePorts = apx_nodeInfo_getNumRequirePorts(self->nodeInfo);
      if (portId >= 0 && portId < numRequirePorts)
      {
         return &self->requirePortReferences[portId];
      }
   }
   return (apx_portRef_t*) 0;
}

apx_portRef_t *apx_nodeInstance_getProvidePortRef(apx_nodeInstance_t *self, apx_portId_t portId)
{
   if ( (self != 0) && (self->nodeInfo != 0) && (self->providePortReferences != 0) )
   {
      apx_portCount_t numProvidePorts = apx_nodeInfo_getNumProvidePorts(self->nodeInfo);
      if (portId >= 0 && portId < numProvidePorts)
      {
         return &self->providePortReferences[portId];
      }
   }
   return (apx_portRef_t*) 0;
}

void apx_nodeInstance_registerDefinitionFileHandler(apx_nodeInstance_t *self, apx_file_t *file)
{
   if ( (self != 0) && (file != 0) )
   {
      apx_fileNotificationHandler_t handler = {0, 0, 0};
      handler.arg = (void*) self;
      if (apx_file_isRemoteFile(file))
      {
         handler.writeNotify = apx_nodeInstance_definitionFileWriteNotify;
      }
      else
      {
         handler.openNotify = apx_nodeInstance_definitionFileOpenNotify;
      }
      apx_file_setNotificationHandler(file, &handler);
      self->definitionFile = file;
   }
}

void apx_nodeInstance_registerProvidePortFileHandler(apx_nodeInstance_t *self, apx_file_t *file)
{
   if ( (self != 0) && (file != 0) )
   {
      apx_fileNotificationHandler_t handler = {0, 0, 0};
      handler.arg = (void*) self;
      if (apx_file_isRemoteFile(file))
      {
         handler.writeNotify = apx_nodeInstance_providePortDataFileWriteNotify;
      }
      else
      {
         handler.openNotify = apx_nodeInstance_providePortDataFileOpenNotify;
      }
      apx_file_setNotificationHandler(file, &handler);
      self->providePortDataFile = file;
   }
}

const char *apx_nodeInstance_getName(apx_nodeInstance_t *self)
{
   if ( (self != 0) && (self->nodeInfo != 0) )
   {
      return apx_nodeInfo_getName(self->nodeInfo);
   }
   return (const char*) 0;
}

int32_t apx_nodeInstance_getNumProvidePorts(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      if (self->nodeInfo != 0)
      {
         return apx_nodeInfo_getNumProvidePorts(self->nodeInfo);
      }
      else if (self->parseTree != 0)
      {
         return apx_node_getNumProvidePorts(self->parseTree);
      }
      else
      {
         //MISRA
      }
   }
   return -1;
}

int32_t apx_nodeInstance_getNumRequirePorts(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      if (self->nodeInfo != 0)
      {
         return apx_nodeInfo_getNumRequirePorts(self->nodeInfo);
      }
      else if (self->parseTree != 0)
      {
         return apx_node_getNumRequirePorts(self->parseTree);
      }
      else
      {
         //MISRA
      }
   }
   return -1;
}


apx_error_t apx_nodeInstance_fillProvidePortDataFileInfo(apx_nodeInstance_t *self, apx_fileInfo_t *fileInfo)
{
   if ( (self != 0) && (fileInfo != 0))
   {
      if (self->nodeInfo != 0)
      {
         uint32_t fileSize;
         fileSize = (uint32_t) apx_nodeInfo_getProvidePortInitDataSize(self->nodeInfo);
         assert(fileSize > 0);
         return apx_nodeInstance_createFileInfo(self, APX_OUTDATA_FILE_EXT, fileSize, fileInfo);
      }
      return APX_NULL_PTR_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

apx_error_t apx_nodeInstance_fillDefinitionFileInfo(apx_nodeInstance_t *self, apx_fileInfo_t *fileInfo)
{
   if ( (self != 0) && (fileInfo != 0))
   {
      if (self->nodeData != 0)
      {
         uint32_t fileSize;
         fileSize = (uint32_t) apx_nodeData_getDefinitionDataLen(self->nodeData);
         assert(fileSize > 0);
         return apx_nodeInstance_createFileInfo(self, APX_DEFINITION_FILE_EXT, fileSize, fileInfo);
      }
      return APX_NULL_PTR_ERROR;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}



void apx_nodeInstance_setConnection(apx_nodeInstance_t *self, struct apx_connectionBase_tag *connection)
{
   if ( (self != 0) && (connection != 0) )
   {
      self->connection = connection;
   }
}

struct apx_connectionBase_tag* apx_nodeInstance_getConnection(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->connection;
   }
   return (struct apx_connectionBase_tag*) 0;
}

void apx_nodeInstance_cleanParseTree(apx_nodeInstance_t *self)
{
   if ( (self != 0) && (self->parseTree != 0) )
   {
      apx_node_delete(self->parseTree);
      self->parseTree = (apx_node_t*) 0;
   }
}

apx_nodeInfo_t *apx_nodeInstance_getNodeInfo(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->nodeInfo;
   }
   return (apx_nodeInfo_t*) 0;
}

void apx_nodeInstance_setState(apx_nodeInstance_t *self, apx_nodeState_t state)
{
   if (self != 0)
   {
      self->nodeState = state;
   }
}

apx_nodeState_t apx_nodeInstance_getState(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      return self->nodeState;
   }
   return APX_NODE_STATE_INVALID;
}

/********** Data API  ************/

apx_error_t apx_nodeInstance_writeDefinitionData(apx_nodeInstance_t *self, const uint8_t *src, uint32_t offset, uint32_t len)
{
   if ( (self != 0) && (src != 0) )
   {
      if (self->nodeData != 0)
      {
         return apx_nodeData_writeDefinitionData(self->nodeData, src, offset, len);
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

/*
apx_error_t apx_nodeInstance_readDefinitionData(apx_nodeInstance_t *self, uint8_t *dest, uint32_t offset, uint32_t len)
{
   if ( (self != 0) && (dest != 0) )
   {
      if (self->nodeData != 0)
      {
         return apx_nodeData_readDefinitionData(self->nodeData, dest, offset, len);
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}
*/

/**
 * Updates ProvidePortData in this node instance.
 * If the node is currently connected it will also forward the new data to remote side
 */
apx_error_t apx_nodeInstance_writeProvidePortData(apx_nodeInstance_t *self, const uint8_t *src, uint32_t offset, apx_size_t len)
{
   if ( (self != 0) && (src != 0) )
   {
      if (self->nodeData != 0)
      {
         apx_error_t rc = apx_nodeData_writeProvidePortData(self->nodeData, src, offset, len);
         if (rc != APX_NO_ERROR)
         {
            return rc;
         }
         if(self->connection != 0)
         {
            assert(self->providePortDataFile != 0);
            rc = apx_connectionBase_updateProvidePortDataDirect(self->connection, self->providePortDataFile, offset, src, len);
         }
         return rc;
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

/********** Port Connection Changes API  ************/
apx_portConnectorChangeTable_t* apx_nodeInstance_getRequirePortConnectorChanges(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      if (self->requirePortChanges == 0)
      {
         assert(self->nodeInfo != 0);
         self->requirePortChanges = apx_portConnectorChangeTable_new(apx_nodeInfo_getNumRequirePorts(self->nodeInfo));
      }
      return self->requirePortChanges;
   }
   return (apx_portConnectorChangeTable_t*) 0;
}

apx_portConnectorChangeTable_t* apx_nodeInstance_getProvidePortConnectorChanges(apx_nodeInstance_t *self)
{
   {
      if (self != 0)
      {
         if (self->providePortChanges == 0)
         {
            assert(self->nodeInfo != 0);
            self->providePortChanges = apx_portConnectorChangeTable_new(apx_nodeInfo_getNumProvidePorts(self->nodeInfo));
         }
         return self->providePortChanges;
      }
      return (apx_portConnectorChangeTable_t*) 0;
   }
}

/**
 * This clears the internal pointer. This implicitly means the caller of this function has now taken ownership of
 * the data structure and is now responsible for its memory management.
 */
void apx_nodeInstance_clearRequirePortConnectorChanges(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      self->requirePortChanges = (apx_portConnectorChangeTable_t*) 0;
   }
}

/**
 * This clears the internal pointer. This implicitly means the caller of this function has now taken ownership of
 * the data structure and is now responsible for its memory management.
 */
void apx_nodeInstance_clearProvidePortConnectorChanges(apx_nodeInstance_t *self)
{
   if (self != 0)
   {
      self->providePortChanges = (apx_portConnectorChangeTable_t*) 0;
   }
}



//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////


static apx_error_t apx_nodeInstance_definitionFileWriteNotify(void *arg, apx_file_t *file, uint32_t offset, const uint8_t *src, uint32_t len)
{
   (void) file;
   apx_nodeInstance_t *self = (apx_nodeInstance_t*) arg;
   if (self != 0)
   {
      apx_error_t retval;
#if APX_DEBUG_ENABLE
      //printf("definitionFileWriteNotify(%d, %d)\n", (int) offset, (int) len);
#endif
      //It's OK for definition data to be written directly by the node instance before notification
      retval = apx_nodeData_writeDefinitionData(self->nodeData, src, offset, len);
      if ( (self->connection != 0) && (retval == APX_NO_ERROR) )
      {
         retval = apx_connectionBase_nodeInstanceFileWriteNotify(self->connection, self, APX_DEFINITION_FILE_TYPE, offset, src, len);
      }
      return retval;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

#include <stdio.h>

static apx_error_t apx_nodeInstance_definitionFileOpenNotify(void *arg, struct apx_file_tag *file)
{
   apx_nodeInstance_t *self;
   self = (apx_nodeInstance_t*) arg;
   if (self != 0)
   {
      if (self->definitionFile == 0)
      {
         self->definitionFile = file;
      }
      assert(file->fileManager != 0);
      return apx_fileManager_writeConstData(apx_file_getFileManager(file), apx_file_getStartAddress(file), apx_file_getFileSize(file), apx_nodeInstance_definitionFileReadData, (void*) self);
   }
   return APX_NO_ERROR;
}

static apx_error_t apx_nodeInstance_definitionFileReadData(void *arg, apx_file_t*file, uint32_t offset, uint8_t *dest, uint32_t len)
{
   apx_nodeInstance_t *self = (apx_nodeInstance_t*) arg;
   if (self != 0)
   {
      apx_nodeData_t *nodeData = apx_nodeInstance_getNodeData(self);
      if (nodeData != 0)
      {
         return apx_nodeData_readDefinitionData(nodeData, dest, offset, len);
      }
      else
      {
         return APX_NULL_PTR_ERROR;
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}


static apx_error_t apx_nodeInstance_createFileInfo(apx_nodeInstance_t *self, const char *fileExtension, uint32_t fileSize, apx_fileInfo_t *fileInfo)
{
   if ( (self != 0) && (fileInfo != 0))
   {
      char fileName[RMF_MAX_FILE_NAME+1];
      const char *nodeName;
      nodeName = apx_nodeInstance_getName(self);
      if (nodeName == 0)
      {
         return APX_NAME_MISSING_ERROR;
      }
      if ( (strlen(nodeName) + APX_MAX_FILE_EXT_LEN) >= sizeof(fileName) )
      {
         return APX_NAME_TOO_LONG_ERROR;
      }
      strcpy(fileName, nodeName);
      strcat(fileName, fileExtension);
      return apx_fileInfo_create(fileInfo, RMF_INVALID_ADDRESS, fileSize, fileName, RMF_FILE_TYPE_FIXED, RMF_DIGEST_TYPE_NONE, (const uint8_t*) 0);
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

static apx_error_t apx_nodeInstance_providePortDataFileWriteNotify(void *arg, apx_file_t *file, uint32_t offset, const uint8_t *src, uint32_t len)
{
   apx_nodeInstance_t *self = (apx_nodeInstance_t*) arg;
   if ( (self != 0) && (file != 0) )
   {
      if (self->connection != 0)
      {
         //It's not OK for nodeInstance to manipulate it's own data without acquiring a global lock (either owned by server or client).
         //Forward this notification to parent connection to take correct action. The connection must write the data using the apx_nodeData API later.
         return apx_connectionBase_nodeInstanceFileWriteNotify(self->connection, self, apx_file_getApxFileType(file), offset, src, len);
      }
      else
      {
         return APX_NULL_PTR_ERROR;
      }
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

/**
 * Takes a snapshot of the providePortData buffer and sends it away to the file manager
 */
static apx_error_t apx_nodeInstance_providePortDataFileOpenNotify(void *arg, struct apx_file_tag *file)
{
   apx_nodeInstance_t *self = (apx_nodeInstance_t*) arg;
   if ( (self != 0) && (file != 0) )
   {
      uint8_t *dataBuf;
      size_t bufSize;
      apx_size_t fileSize;
      uint32_t fileStartAddress;
      apx_error_t rc;
      apx_fileManager_t *fileManager = apx_file_getFileManager(file);
      assert(fileManager != 0);
      assert(self->nodeData != 0);
      assert(self->providePortDataFile != 0);
      if (self->connection == 0)
      {
         return APX_NULL_PTR_ERROR;
      }
      bufSize = (size_t) apx_nodeData_getProvidePortDataLen(self->nodeData);
      fileSize = apx_file_getFileSize(file);
      fileStartAddress = apx_file_getStartAddress(file);
      assert(fileSize > 0);
      if (fileSize != bufSize)
      {
         if (bufSize == 0u)
         {
            return APX_MISSING_BUFFER_ERROR;
         }
         else
         {
            return APX_LENGTH_ERROR;
         }
      }
      if (fileSize > APX_MAX_FILE_SIZE)
      {
         return APX_FILE_TOO_LARGE_ERROR;
      }
      dataBuf = apx_connectionBase_alloc(self->connection, bufSize);
      if (dataBuf == 0)
      {
         return APX_MEM_ERROR;
      }
      rc = apx_nodeData_readProvidePortData(self->nodeData, dataBuf, 0u, bufSize);
      if (rc != APX_NO_ERROR)
      {
         apx_connectionBase_free(self->connection, dataBuf, bufSize);
      }
      return apx_fileManager_writeDynamicData(fileManager, fileStartAddress, fileSize, dataBuf);
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

static void apx_nodeInstance_initPortRefs(apx_nodeInstance_t *self, apx_portRef_t *portRefs, apx_portCount_t numPorts, uint32_t portIdMask, apx_getPortDataPropsFunc *getPortDataProps)
{
   apx_portId_t portId;
   assert(portRefs != 0);
   assert(self != 0);
   assert(self->nodeInfo != 0);
   assert(getPortDataProps != 0);
   for (portId=0; portId < numPorts; portId++)
   {
      const apx_portDataProps_t *dataProps = getPortDataProps(self->nodeInfo, portId);
      assert(dataProps != 0);
      apx_portRef_create(&portRefs[portId], self,  (portId|portIdMask), dataProps);
   }
}