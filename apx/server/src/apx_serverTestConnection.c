/*****************************************************************************
* \file      apx_serverTestConnection.c
* \author    Conny Gustafsson
* \date      2018-12-09
* \brief     Programmable server connection for the purpose of unit testing
*
* Copyright (c) 2018-2019 Conny Gustafsson
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
#include <malloc.h>
#include <stdio.h> //debug only
#include "apx_serverTestConnection.h"
#ifdef MEM_LEAK_CHECK
#include "CMemLeak.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void apx_serverTestConnection_registerTransmitHandler(apx_serverTestConnection_t *self);
static uint8_t *apx_serverTestConnection_getSendBuffer(void *arg, int32_t msgLen);
static int32_t apx_serverTestConnection_send(void *arg, int32_t offset, int32_t msgLen);
//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
apx_error_t apx_serverTestConnection_create(apx_serverTestConnection_t *self, struct apx_server_tag *server)
{
   if (self != 0)
   {
      apx_connectionBaseVTable_t vtable;
      self->pendingMsg = (adt_bytearray_t*) 0;
      apx_connectionBaseVTable_create(&vtable, apx_serverTestConnection_vdestroy, apx_serverTestConnection_vstart, apx_serverTestConnection_vclose);
      apx_error_t result = apx_serverConnectionBase_create(&self->base, server, &vtable);
      if (result == APX_NO_ERROR)
      {
         self->transmitLog = adt_ary_new(adt_bytearray_vdelete);
         if (self->transmitLog == 0)
         {
            result = APX_MEM_ERROR;
         }
         else
         {
            apx_serverTestConnection_registerTransmitHandler(self);
         }
      }
      return result;
   }
   return APX_INVALID_ARGUMENT_ERROR;
}

void apx_serverTestConnection_destroy(apx_serverTestConnection_t *self)
{
   if (self != 0)
   {
      apx_serverConnectionBase_destroy(&self->base);
      if (self->pendingMsg != 0)
      {
         adt_bytearray_delete(self->pendingMsg);
      }
      if (self->transmitLog != 0)
      {
         adt_ary_delete(self->transmitLog);
      }
   }
}

void apx_serverTestConnection_vdestroy(void *arg)
{
   apx_serverTestConnection_destroy((apx_serverTestConnection_t*) arg);
}

apx_serverTestConnection_t *apx_serverTestConnection_new(struct apx_server_tag *server)
{
   apx_serverTestConnection_t *self = (apx_serverTestConnection_t*) malloc(sizeof(apx_serverTestConnection_t));
   if (self != 0)
   {
      apx_error_t result = apx_serverTestConnection_create(self, server);
      if (result != APX_NO_ERROR)
      {
         free(self);
         self = 0;
      }
   }
   return self;
}

void apx_serverTestConnection_delete(apx_serverTestConnection_t *self)
{
   if (self != 0)
   {
      apx_serverTestConnection_destroy(self);
      free(self);
   }
}

void apx_serverTestConnection_start(apx_serverTestConnection_t *self)
{
   apx_serverConnectionBase_start(&self->base);
}

void apx_serverTestConnection_vstart(void *arg)
{
   apx_serverTestConnection_start((apx_serverTestConnection_t*) arg);
}

void apx_serverTestConnection_close(apx_serverTestConnection_t *self)
{

}

void apx_serverTestConnection_vclose(void *arg)
{
   apx_serverTestConnection_close((apx_serverTestConnection_t*) arg);
}


/**
 * Remote side has created a new remote file
 */
void apx_serverTestConnection_createRemoteFile(apx_serverTestConnection_t *self, const rmf_fileInfo_t *fileInfo)
{
   if ( (self != 0) && (fileInfo != 0) )
   {
      apx_fileManager_onRemoteCmdFileInfo(&self->base.base.fileManager, fileInfo);
   }
}

/**
 * Writes raw data to a remote address
 */
void apx_serverTestConnection_writeRemoteData(apx_serverTestConnection_t *self, uint32_t address, const uint8_t* dataBuf, uint32_t dataLen, bool more)
{
   if ( (self != 0) && (dataBuf != 0) )
   {
      apx_fileManager_onWriteRemoteData(&self->base.base.fileManager, address, dataBuf, dataLen, more);
   }
}

void apx_serverTestConnection_openRemoteFile(apx_serverTestConnection_t *self, uint32_t address)
{
   if (self != 0)
   {
      apx_fileManager_onRemoteCmdFileOpen(&self->base.base.fileManager, address);
   }
}

void apx_serverTestConnection_runEventLoop(apx_serverTestConnection_t *self)
{
   if (self != 0)
   {
#ifdef UNIT_TEST
      apx_connectionBase_runAll(&self->base.base);
#endif
   }
}

int32_t apx_serverTestConnection_getTransmitLogLen(apx_serverTestConnection_t *self)
{
   if (self != 0)
   {
      return adt_ary_length(self->transmitLog);
   }
   return -1;
}

adt_bytearray_t *apx_serverTestConnection_getTransmitLogMsg(apx_serverTestConnection_t *self, int32_t index)
{
   if (self != 0)
   {
      return (adt_bytearray_t*) adt_ary_value(self->transmitLog, index);
   }
   return (adt_bytearray_t*) 0;
}


//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
static void apx_serverTestConnection_registerTransmitHandler(apx_serverTestConnection_t *self)
{
   apx_transmitHandler_t serverTransmitHandler;
   serverTransmitHandler.arg = self;
   serverTransmitHandler.send = apx_serverTestConnection_send;
   serverTransmitHandler.getSendAvail = 0;
   serverTransmitHandler.getSendBuffer = apx_serverTestConnection_getSendBuffer;
   apx_fileManager_setTransmitHandler(&self->base.base.fileManager, &serverTransmitHandler);
}

static uint8_t *apx_serverTestConnection_getSendBuffer(void *arg, int32_t msgLen)
{
   apx_serverTestConnection_t *self = (apx_serverTestConnection_t*) arg;
   if ( (self != 0) && (msgLen > 0) )
   {
      if (self->pendingMsg == 0)
      {
         self->pendingMsg = adt_bytearray_new(ADT_BYTE_ARRAY_DEFAULT_GROW_SIZE);
      }
      if (self->pendingMsg != 0)
      {
         adt_error_t result = adt_bytearray_resize(self->pendingMsg, (uint32_t ) msgLen);
         if (result == ADT_NO_ERROR)
         {
            return adt_bytearray_data(self->pendingMsg);
         }
      }
      return 0;
   }
   return 0;
}

static int32_t apx_serverTestConnection_send(void *arg, int32_t offset, int32_t msgLen)
{
   apx_serverTestConnection_t *self = (apx_serverTestConnection_t*) arg;
   if ( (self != 0) && (msgLen > 0) )
   {
      if (self->pendingMsg != 0)
      {
         int32_t bufLen = (int32_t) adt_bytearray_length(self->pendingMsg);
         if ( (offset == 0u) && (bufLen == msgLen) )
         {
            adt_ary_push(self->transmitLog, self->pendingMsg);
            self->pendingMsg = 0;
         }
      }
   }
   return -1;
}
