//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#ifndef APX_DEBUG_ENABLE
#define APX_DEBUG_ENABLE 0
#endif
#ifndef APX_MSQ_QUEUE_WARN_THRESHOLD
#define APX_MSQ_QUEUE_WARN_THRESHOLD 2
#endif
#include <string.h>
#include <assert.h>
#if APX_DEBUG_ENABLE || defined(UNIT_TEST)
#include <stdio.h>
#endif
#include <limits.h>
#include "apx_es_fileManager.h"
#include "apx_msg.h"

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static int32_t apx_es_fileManager_runEventLoop(apx_es_fileManager_t *self);
static int32_t apx_es_fileManager_processPendingMessage(apx_es_fileManager_t *self);
static void apx_es_fileManager_parseCmdMsg(apx_es_fileManager_t *self, const uint8_t *msgBuf, int32_t msgLen);
static void apx_es_fileManager_parseDataMsg(apx_es_fileManager_t *self, uint32_t address, const uint8_t *dataBuf, int32_t dataLen, bool more_bit);
static void apx_es_fileManager_processRemoteFileInfo(apx_es_fileManager_t *self, const rmf_fileInfo_t *fileInfo);
static void apx_es_fileManager_processOpenFile(apx_es_fileManager_t *self, const rmf_cmdOpenFile_t *cmdOpenFile);
static int32_t apx_es_processPendingWrite(apx_es_fileManager_t *self);
static inline int32_t apx_es_genFileSendMsg(uint8_t* msgBuf, uint32_t headerLen,
                                            apx_es_fileManager_t* self,
                                            apx_file_t* file,
                                            uint32_t offset, uint32_t dataLen,
                                            uint32_t msgLen, bool more_bit);
static void apx_es_queueWriteNotifyUnlessQueued(rbfs_t* rbf, const uint8_t* u8Data);
static void apx_es_processQueuedWriteNotify(apx_es_fileManager_t *self);
static void apx_es_resetConnectionState(apx_es_fileManager_t *self);
static void apx_es_transmitMsg(apx_es_fileManager_t *self);
static void apx_es_setupTransmitBuf(apx_es_fileManager_t *self);
#ifndef UNIT_TEST
DYN_STATIC int8_t apx_es_fileManager_removeRequestedAt(apx_es_fileManager_t *self, int32_t removeIndex);
#endif

//////////////////////////////////////////////////////////////////////////////
// LOCAL VARIABLES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
int8_t apx_es_fileManager_create(apx_es_fileManager_t *self, uint8_t *messageQueueBuf, uint16_t messageQueueLen, uint8_t *receiveBuf, uint16_t receiveBufLen)
{
   int8_t retval = -1;
   if ( (self != 0) && (messageQueueBuf != 0) && (receiveBuf != 0))
   {
      rbfs_create(&self->messageQueue, messageQueueBuf, messageQueueLen, (uint8_t) sizeof(apx_msg_t));
      self->receiveBuf = receiveBuf;
      self->receiveBufLen = receiveBufLen;
      apx_es_fileMap_create(&self->localFileMap);
      apx_es_fileMap_create(&self->remoteFileMap);
      apx_es_fileManager_setTransmitHandler(self, 0);
      self->numRequestedFiles = 0;
      apx_es_resetConnectionState(self);
      retval = 0;
   }
   return retval;
}

void apx_es_fileManager_attachLocalFile(apx_es_fileManager_t *self, apx_file_t *localFile)
{
   if ( (self != 0) && (localFile != 0) )
   {
      apx_es_fileMap_autoInsert(&self->localFileMap, localFile);
   }
}

void apx_es_fileManager_requestRemoteFile(apx_es_fileManager_t *self, apx_file_t *requestedFile)
{
   if ( (self != 0) && (requestedFile != 0) )
   {
      int32_t i;
      if ( self->numRequestedFiles >= APX_ES_FILEMANAGER_MAX_NUM_REQUEST_FILES)
      {
         return;
      }
      //prevent duplicates
      for(i=0;i<self->numRequestedFiles;i++)
      {
         apx_file_t *file = self->requestedFileList[i];
         if (strcmp(requestedFile->fileInfo.name, file->fileInfo.name)==0)
         {
            return;
         }
      }
      self->requestedFileList[self->numRequestedFiles++] = requestedFile;
   }
}

void apx_es_fileManager_setTransmitHandler(apx_es_fileManager_t *self, apx_transmitHandler_t *handler)
{
   if (self != 0)
   {
      if (handler == 0)
      {
         memset(&self->transmitHandler, 0, sizeof(apx_transmitHandler_t));
      }
      else
      {
         memcpy(&self->transmitHandler, handler, sizeof(apx_transmitHandler_t));
      }
      if (self->transmitHandler.arg != 0)
      {
         memcpy(&self->transmitHandlerOptimalWriteSize,
                self->transmitHandler.arg, sizeof(uint32_t));
      }
      else
      {
         self->transmitHandlerOptimalWriteSize = 0;
      }
   }
}

void apx_es_fileManager_onConnected(apx_es_fileManager_t *self)
{
   if (self != 0)
   {
      int32_t end;
      int32_t i;
      self->isConnected = true;
      end = apx_es_fileMap_length(&self->localFileMap);
      for(i=0;i<end;i++)
      {
         apx_file_t *file = apx_es_fileMap_get(&self->localFileMap,i);
         if (file != 0)
         {
            apx_msg_t msg = {RMF_MSG_FILEINFO,0,0,0};
            msg.msgData3 = file;
#if APX_DEBUG_ENABLE
            if (rbfs_free(&self->messageQueue) <= APX_MSQ_QUEUE_WARN_THRESHOLD)
            {
               fprintf(stderr, "messageQueue fill warning for RMF_MSG_FILEINFO. Free before add: %d\n", rbfs_free(&self->messageQueue));
            }
#endif
            rbfs_insert(&self->messageQueue,(uint8_t*) &msg);
         }
      }
   }
}

void apx_es_fileManager_onDisconnected(apx_es_fileManager_t *self)
{
   if (self != 0)
   {
      apx_es_fileMap_clear(&self->remoteFileMap);
      apx_es_resetConnectionState(self);
   }
}

/**
 * triggered by lower layer when a message has been received
 */
void apx_es_fileManager_onMsgReceived(apx_es_fileManager_t *self, const uint8_t *msgBuf, int32_t msgLen)
{
   rmf_msg_t msg;
   int32_t result = rmf_unpackMsg(msgBuf, msgLen, &msg);
   if ( (result > 0) && (self != 0) )
   {
#if APX_DEBUG_ENABLE
      printf("[APX_ES_FILEMANAGER] address: %08X\n", msg.address);
      printf("[APX_ES_FILEMANAGER] length: %d\n", msg.dataLen);
      printf("[APX_ES_FILEMANAGER] more_bit: %d\n", (int) msg.more_bit);
#endif
      if (msg.address == RMF_CMD_START_ADDR)
      {
         apx_es_fileManager_parseCmdMsg(self, msg.data, msg.dataLen);
      }
      else if (msg.address < RMF_CMD_START_ADDR)
      {
         apx_es_fileManager_parseDataMsg(self, msg.address, msg.data, msg.dataLen, msg.more_bit);
      }
      else
      {
         //discard
      }
   }
   else if (result < 0)
   {
#if APX_DEBUG_ENABLE
      fprintf(stderr, "rmf_unpackMsg failed with %d\n", result);
#endif
   }
   else
   {
      //MISRA
   }
}

void apx_es_fileManager_onFileUpdate(apx_es_fileManager_t *self, apx_file_t *file, uint32_t offset, uint32_t length)
{
   if ( (self != 0) && (file != 0) && (length > 0) && self->isConnected)
   {
      apx_msg_t msg = {RMF_MSG_WRITE_NOTIFY, 0, 0, 0}; //{msgType,msgData1,msgData2,msgData3}
      msg.msgData1=offset;
      msg.msgData2=length;
      msg.msgData3=(void*)file;
      if (self->queuedWriteNotify.msgType == RMF_MSG_WRITE_NOTIFY)
      {
         // Check if sequential write to the last file. If so append to the queued write notification
         uint32_t potentialSequentialWriteSize = self->queuedWriteNotify.msgData2 + length;
         if ( (msg.msgData3 == self->queuedWriteNotify.msgData3) &&
              ( (self->queuedWriteNotify.msgData1 + self->queuedWriteNotify.msgData2) == offset) &&
              ( potentialSequentialWriteSize <= (APX_ES_FILE_WRITE_MSG_FRAGMENTATION_THRESHOLD - RMF_HIGH_ADDRESS_SIZE) )
         {
            self->queuedWriteNotify.msgData2 = potentialSequentialWriteSize;
         }
#if APX_ES_FILEMANAGER_OPTIMIZE_WRITE_NOTIFICATIONS == 1
         else if ( (self->queuedWriteNotify.msgData1 > offset) ||
                   ( (self->queuedWriteNotify.msgData1 + self->queuedWriteNotify.msgData2) <
                     (offset + length) ) )
         {
            // Not writing inside the queuedWriteNotify
#else
         else
         {
#fi
#if APX_DEBUG_ENABLE
            if (rbfs_free(&self->messageQueue) <= APX_MSQ_QUEUE_WARN_THRESHOLD)
            {
               fprintf(stderr, "messageQueue fill warning for RMF_MSG_WRITE_NOTIFY. Free before add: %d\n", rbfs_free(&self->messageQueue));
            }
#endif
#if APX_ES_FILEMANAGER_OPTIMIZE_WRITE_NOTIFICATIONS == 1
            apx_es_queueWriteNotifyUnlessQueued(&self->messageQueue,(const uint8_t*) &self->queuedWriteNotify);
#else
            rbfs_insert(&self->messageQueue,(const uint8_t*) &self->queuedWriteNotify);
#fi
            self->queuedWriteNotify = msg;
         }
#if APX_ES_FILEMANAGER_OPTIMIZE_WRITE_NOTIFICATIONS == 1
         else
         {
            // File written to same location multiple times before run()
            // This can happen for instance if run() is starved by sending
            // some other large file via pendingWrite
            // In this case there is no need to queue multiple apx_file reads
            // since all reads in the queue would read the same data
         }
#endif
      }
      else
      {
         self->queuedWriteNotify = msg;
      }
   }
}

/**
 * runs the message handler until either the sendbuffer is full or the are no more messages to process
 */
void apx_es_fileManager_run(apx_es_fileManager_t *self)
{
   int32_t result;
   if ( (self == 0) || !self->isConnected )
   {
      return;
   }
   if (self->pendingWrite == true)
   {
      apx_es_setupTransmitBuf(self);
      result = apx_es_processPendingWrite(self);
      if (result < 0)
      {
#if APX_DEBUG_ENABLE
         fprintf(stderr, "apx_es_processPendingWrite returned %d",result);
#endif
      }
   }
   if (self->pendingMsg.msgType != RMF_CMD_INVALID_MSG)
   {
      apx_es_setupTransmitBuf(self);
      result = apx_es_fileManager_processPendingMessage(self);
      if (result < 0)
      {
#if APX_DEBUG_ENABLE
         fprintf(stderr, "pendingMsg processing returned %d",result);
#endif
      }
      else if (result > 0)
      {
         self->pendingMsg.msgType = RMF_CMD_INVALID_MSG;
         self->transmitBufUsed = (unit32_t) result;
         apx_es_transmitMsg(self);
      }
      else
      {
         // Pending message could still not be processed
      }
   }
   if (self->queuedWriteNotify.msgType == RMF_MSG_WRITE_NOTIFY)
   {
      // Add any pending write notificatins to the msg queue
      apx_es_processQueuedWriteNotify(self);
   }

   // Trigger refresh of the transmitBufLen before loop
   self->transmitBuf = 0;

   result = -1;
   while ( (result != 0) &&
           (self->pendingWrite == false) &&
           (self->pendingMsg.msgType == RMF_CMD_INVALID_MSG) )
   {
      result = apx_es_fileManager_runEventLoop(self);

      if (result > 0)
      {
         // Work completed ok. result shows the size appended to transmitBuf
         self->transmitBufUsed += (unit32_t)result;
         if (self->transmitBufUsed >= self->transmitHandlerOptimalWriteSize)
         {
            apx_es_transmitMsg(self);
         }
      }
      else if (result < 0)
      {
#if APX_DEBUG_ENABLE
         fprintf(stderr, "apx_es_fileManager_runEventLoop returned %d",result);
#endif
      }
      else
      {
         // 0 = no work performed this loop
      }
   }
   // Transmit pending data if any
   if (self->transmitBufUsed > 0)
   {
      apx_es_transmitMsg(self);
   }
}

//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
static void apx_es_transmitMsg(apx_es_fileManager_t *self)
{
   int32_t result = self->transmitHandler.send(self->transmitHandler.arg, 0,
                                               self->transmitBufUsed);
#if APX_DEBUG_ENABLE
   if(result < 0)
   {
      fprintf(stderr, "apx_es_transmitMsg failed: %d\n", result);
   }
#endif
   // Mark transmited and new buffersize check needed
   self->transmitBuf = 0;
   self->transmitBufLen = 0;
   self->transmitBufUsed = 0;

   // Make sure transmitHandler accepted the send of the size previously reserved
   assert(result >= 0);
}

static void apx_es_resetConnectionState(apx_es_fileManager_t *self)
{
   rbfs_clear(&self->messageQueue);
   self->receiveBufOffset = 0;
   self->receiveStartAddress = RMF_INVALID_ADDRESS;
   self->transmitBuf = 0;
   self->transmitBufLen = 0;
   self->transmitBufUsed = 0;
   self->queuedWriteNotify.msgType = RMF_CMD_INVALID_MSG;
   self->pendingMsg.msgType = RMF_CMD_INVALID_MSG;
   self->pendingWrite = false;
   self->dropMessage = false;
   self->isConnected = false;
   self->curFile = 0;
   memset(&self->fileWriteInfo, 0, sizeof(apx_es_file_write_t));
}

static void apx_es_processQueuedWriteNotify(apx_es_fileManager_t *self)
{
#if APX_DEBUG_ENABLE
   if (rbfs_free(&self->messageQueue) <= APX_MSQ_QUEUE_WARN_THRESHOLD)
   {
      fprintf(stderr, "messageQueue fill warning for delayed RMF_MSG_WRITE_NOTIFY. Free before add: %d\n", rbfs_free(&self->messageQueue));
   }
#endif
#if APX_ES_FILEMANAGER_OPTIMIZE_WRITE_NOTIFICATIONS == 1
   apx_es_queueWriteNotifyUnlessQueued(&self->messageQueue,(const uint8_t*) &self->queuedWriteNotify);
#else
   rbfs_insert(&self->messageQueue,(const uint8_t*) &self->queuedWriteNotify);
#fi
   self->queuedWriteNotify.msgType = RMF_CMD_INVALID_MSG;
}

#if APX_ES_FILEMANAGER_OPTIMIZE_WRITE_NOTIFICATIONS == 1
static void apx_es_queueWriteNotifyUnlessQueued(rbfs_t* rbf, const uint8_t* u8Data)
{
   if (E_BUF_UNDERFLOW == rbfs_exists(rbf, u8Data))
   {
      rbfs_insert(rbf, u8Data);
   }
}
#endif

/**
 * runs internal event loop
 * returns 0 when no more messages can be processed, -1 on error and 1 on success
 */
static int32_t apx_es_fileManager_runEventLoop(apx_es_fileManager_t *self)
{
   int32_t retval = 0;
   int8_t rc = rbfs_remove(&self->messageQueue, (uint8_t*) &self->pendingMsg);
   if (rc == E_BUF_OK)
   {
      if (self->transmitBuf == 0)
      {
         apx_es_setupTransmitBuf(self);
      }
      retval = apx_es_fileManager_processPendingMessage(self);
   }
   return retval;
}

static void apx_es_setupTransmitBuf(apx_es_fileManager_t *self)
{
   self->transmitBufUsed = 0;
   if (self->transmitHandler.getSendAvail != 0)
   {
      assert(self->transmitHandler.send != 0);
      assert(self->transmitHandler.getSendBuffer != 0);
      self->transmitBufLen = (uint32_t )self->transmitHandler.getSendAvail(self->transmitHandler.arg);
      self->transmitBuf = self->transmitHandler.getSendBuffer(self->transmitHandler.arg, self->transmitBufLen);
      if (self->transmitBuf == 0)
      {
         // Some other thread may have consumed the avaliable buffer prior to our reservation
         self->transmitBufLen = 0;
      }
   }
   else
   {
      self->transmitBufLen = 0;
      self->transmitBuf = 0;
   }
}

/**
 * This is the apx_es_fileManager worker (by the main event loop)
 * Prior to call the transmitBuf needs to be prepared
 */
static int32_t apx_es_fileManager_processPendingMessage(apx_es_fileManager_t *self)
{
   int32_t retval = 0;
   if (self->transmitBuf == 0)
   {
      retval = -1;
   }
   else
   {
      uint32_t sendAvail = self->transmitBufLen - self->transmitBufUsed;
      uint8_t* msgBuf = &self->transmitBuf[self->transmitBufUsed];
 
      switch(self->pendingMsg.msgType)
      {
      case RMF_MSG_CONNECT:
         break;
      case RMF_MSG_FILEINFO:
         {
            uint32_t headerLen = RMF_HIGH_ADDRESS_SIZE;
            uint32_t dataLen;
            uint32_t msgLen;
            apx_file_t *file = (apx_file_t*) self->pendingMsg.msgData3;
            int32_t nameLen=strlen(file->fileInfo.name);
            dataLen=CMD_FILE_INFO_BASE_SIZE+nameLen+1; //+1 for null terminator
            msgLen=headerLen+dataLen;
            if (msgLen <= sendAvail)
            {
               retval = msgLen;
               if (dataLen != rmf_serialize_cmdFileInfo(&msgBuf[headerLen], dataLen, &file->fileInfo))
               {
                  retval = -1;
               }
               if (headerLen != rmf_packHeaderBeforeData(&msgBuf[headerLen], headerLen, RMF_CMD_START_ADDR, false))
               {
                  retval = -1;
               }
            }
            else
            {
               self->pendingMsg = *msg;
            }
         }
         break;
      case RMF_MSG_FILE_OPEN: //sends file open command
         {
            uint32_t headerLen = RMF_HIGH_ADDRESS_SIZE;
            uint32_t dataLen = (uint32_t) RMF_FILE_OPEN_CMD_LEN;
            uint32_t msgLen = RMF_HIGH_ADDRESS_SIZE + (uint32_t) RMF_FILE_OPEN_CMD_LEN;
            rmf_cmdOpenFile_t cmdOpenFile;
            //add file to remoteFileMap
            apx_es_fileMap_insert(&self->remoteFileMap, (apx_file_t*) self->pendingMsg.msgData3);
            apx_file_open((apx_file_t*) self->pendingMsg.msgData3);
            cmdOpenFile.address = self->pendingMsg.msgData1;
            if (msgLen<=sendAvail)
            {
               retval = msgLen;
               if (dataLen != rmf_serialize_cmdOpenFile(&msgBuf[headerLen], dataLen, &cmdOpenFile))
               {
                  retval = -1;
               }
               if (headerLen != rmf_packHeaderBeforeData(&msgBuf[headerLen], headerLen, RMF_CMD_START_ADDR, false))
               {
                  retval = -1;
               }
            }
            else
            {
               self->pendingMsg = *msg;
            }
         }
         break;
      case RMF_MSG_WRITE_NOTIFY:
         {
            uint32_t address;
            uint32_t msgLen;
            uint32_t headerLen;
            uint32_t dataLen;
            uint32_t offset;
            apx_file_t *file;
            offset = self->pendingMsg.msgData1;
            dataLen = self->pendingMsg.msgData2;
            file = (apx_file_t*) self->pendingMsg.msgData3;
            //calculate address
            address = file->fileInfo.address+offset;
            headerLen = (address < RMF_DATA_HIGH_MIN_ADDR)? RMF_LOW_ADDRESS_SIZE : RMF_HIGH_ADDRESS_SIZE;
            msgLen = headerLen+dataLen;
            //Attempt to deliver the notification as one non-fragmented write
            if (msgLen <= sendAvail)
            {
               if (headerLen == rmf_packHeader(msgBuf, headerLen, address, false);
               {
                  apx_file_read(file, &msgBuf[headerLen], offset, dataLen);
                  retval = msgLen;
               }
               else
               {
                  assert(false);
               }
            }
            else
            {
               //queue as pending write
               self->pendingWrite = true;
               self->fileWriteInfo.localFile = file;
               self->fileWriteInfo.readOffset = offset;
               self->fileWriteInfo.writeAddress = address;
               self->fileWriteInfo.remain = dataLen;
               if (sendAvail >= (APX_ES_FILE_WRITE_MSG_FRAGMENTATION_THRESHOLD * 3))
               {
                  // If it is a very large write get started despite fragmentation
                  apx_es_processPendingWrite(self);
                  // processPendingWrite takes care of the send itself
                  // Ensure no additional messages are processed
                  assert(retval == 0);
               }
            }
         }
         break;
      case RMF_MSG_FILE_SEND: //sends file to remote side
         {
            bool sendFileLater = false;
            apx_file_t *file = (apx_file_t*) self->pendingMsg.msgData3;
#if APX_DEBUG_ENABLE
            int32_t bytesToSend = file->fileInfo.length;
            printf("Opened %s, bytes to send: %d\n", file->fileInfo.name, bytesToSend);
#endif
            apx_file_open(file);
            if (sendAvail >= APX_ES_FILE_WRITE_MSG_FRAGMENTATION_THRESHOLD)
            {
               uint32_t dataLen;
               uint32_t msgLen;
               bool more_bit = false;
               uint32_t headerLen = (file->fileInfo.address < RMF_DATA_LOW_MAX_ADDR)? RMF_LOW_ADDRESS_SIZE : RMF_HIGH_ADDRESS_SIZE;
               msgLen = file->fileInfo.length + headerLen;
               if (msgLen > sendAvail)
               {
                  dataLen = sendAvail-headerLen;
                  msgLen = sendAvail;
                  more_bit = true;
               }
               else
               {
                  dataLen = file->fileInfo.length;
               }
               retval = apx_es_genFileSendMsg(msgBuf, headerLen, self, file, 0, dataLen, msgLen, more_bit);
            }
            else
            {
               sendFileLater = true;
            }

            if (sendFileLater)
            {
               self->pendingWrite = true;
               self->fileWriteInfo.localFile=file;
               self->fileWriteInfo.readOffset=0;
               self->fileWriteInfo.writeAddress=file->fileInfo.address;
               self->fileWriteInfo.remain=file->fileInfo.length;
            }
         }
         break;
      case RMF_MSG_FILE_WRITE:
         {
         }
         break;
      default:
         break;
      }
   }
   return retval;
}

static inline int32_t apx_es_genFileSendMsg(uint8_t* msgBuf, uint32_t headerLen,
                                            apx_es_fileManager_t* self,
                                            apx_file_t* file,
                                            uint32_t offset, uint32_t dataLen,
                                            uint32_t msgLen, bool more_bit)
{
   int32_t retval = apx_file_read(file, &msgBuf[headerLen], offset, dataLen);
   if (headerLen != rmf_packHeaderBeforeData(&msgBuf[headerLen], headerLen, file->fileInfo.address, more_bit))
   {
      retval = -1;
   }
   if (retval == 0)
   {
      retval = msgLen;
      if (dataLen < file->fileInfo.length)
      {
         self->pendingWrite = true;
         self->fileWriteInfo.localFile = file;
         self->fileWriteInfo.readOffset = dataLen;
         self->fileWriteInfo.writeAddress = file->fileInfo.address + dataLen;
         self->fileWriteInfo.remain = file->fileInfo.length - dataLen;
      }
   }
   else
   {
      retval = -1;
   }
   return retval;
}

static void apx_es_fileManager_parseCmdMsg(apx_es_fileManager_t *self, const uint8_t *msgBuf, int32_t msgLen)
{
   if (msgBuf != 0)
   {
      uint32_t cmdType;
      int32_t result;
      result = rmf_deserialize_cmdType(msgBuf, msgLen, &cmdType);
      //printf("apx_fileManager_parseCmdMsg(%d)\n", msgLen);
      if (result > 0)
      {
         switch(cmdType)
         {
            case RMF_CMD_FILE_INFO:
               {
                  rmf_fileInfo_t fileInfo;
                  result = rmf_deserialize_cmdFileInfo(msgBuf, msgLen, &fileInfo);
                  if (result > 0)
                  {
                     apx_es_fileManager_processRemoteFileInfo(self, &fileInfo);
                  }
                  else if (result < 0)
                  {
#if APX_DEBUG_ENABLE
                     fprintf(stderr, "rmf_deserialize_cmdFileInfo failed with %d\n", result);
#endif
                  }
                  else
                  {
#if APX_DEBUG_ENABLE
                     fprintf(stderr, "rmf_deserialize_cmdFileInfo returned 0\n");
#endif
                  }
               }
               break;
            case RMF_CMD_FILE_OPEN:
               {
                  rmf_cmdOpenFile_t cmdOpenFile;
                  result = rmf_deserialize_cmdOpenFile(msgBuf, msgLen, &cmdOpenFile);
                  if (result > 0)
                  {
                     apx_es_fileManager_processOpenFile(self, &cmdOpenFile);
                  }
                  else if (result < 0)
                  {
#if APX_DEBUG_ENABLE
                     fprintf(stderr, "rmf_deserialize_cmdOpenFile failed with %d\n", result);
#endif
                  }
                  else
                  {
#if APX_DEBUG_ENABLE
                     fprintf(stderr, "rmf_deserialize_cmdOpenFile returned 0\n");
#endif
                  }
               }
               break;
            default:
#if APX_DEBUG_ENABLE
               fprintf(stderr, "not implemented cmdType: %u\n", cmdType);
#endif
               break;

         }
      }
   }
}

/**
 * called when a data message has been received.
 */
static void apx_es_fileManager_parseDataMsg(apx_es_fileManager_t *self, uint32_t address, const uint8_t *dataBuf, int32_t dataLen, bool more_bit)
{
   if ( (dataBuf != 0) && (dataLen>=0) )
   {
      uint32_t offset;
      if ( (self->receiveStartAddress == RMF_INVALID_ADDRESS) )
      {
         //new reception
         apx_file_t *remoteFile = apx_es_fileMap_findByAddress(&self->remoteFileMap, address);
         if ( (remoteFile != 0) && (remoteFile->isOpen == true) )
         {
            offset=address-remoteFile->fileInfo.address;
            if (more_bit == false)
            {
               apx_file_write(remoteFile, dataBuf, offset, dataLen);
            }
            else if(((uint32_t)dataLen) <= self->receiveBufLen)
            {
               //start multi-message recepetion
               self->curFile = remoteFile;
               self->receiveStartAddress = address;
               memcpy(&self->receiveBuf[0], dataBuf, dataLen);
               self->receiveBufOffset = dataLen;
            }
            else
            {
               //drop message
               self->receiveStartAddress = address;
               self->dropMessage = true; //message too long
#if APX_DEBUG_ENABLE
               fprintf(stderr, "[APX_ES_FILEMANAGER] message too long (%d bytes), message dropped\n",dataLen);
#endif
            }
         }
      }
      else
      {
         if (self->dropMessage)
         {
            offset = self->receiveBufOffset;
         }
         else
         {
            offset = address-self->curFile->fileInfo.address;
         }
         if (offset != self->receiveBufOffset)
         {
            self->dropMessage = true; //drop message since offsets don't match
#if APX_DEBUG_ENABLE
            fprintf(stderr, "[APX_ES_FILEMANAGER] invalid offset (%u), message dropped\n",offset);
#endif
         }
         else if((offset+dataLen) <= self->receiveBufLen)
         {
            //copy data
            memcpy(&self->receiveBuf[offset], dataBuf, dataLen);
            self->receiveBufOffset = offset + dataLen;
         }
         else
         {
            self->dropMessage = true; //message too long
#if APX_DEBUG_ENABLE
            fprintf(stderr, "[APX_ES_FILEMANAGER] message too long (%d bytes), message dropped\n",dataLen);
#endif
         }

         if (more_bit==false)
         {
            if (self->dropMessage == false)
            {
               //send message to upper layer
               uint32_t startOffset=self->receiveStartAddress-self->curFile->fileInfo.address;
               apx_file_write(self->curFile, self->receiveBuf, startOffset, self->receiveBufOffset);
            }
            //reset variables for next reception
            self->dropMessage=false;
            self->curFile=0;
            self->receiveStartAddress=RMF_INVALID_ADDRESS;
            self->receiveBufOffset=0;
         }

      }
      //printf("apx_es_fileManager_parseDataMsg %08X, %d, %d\n",address, (int) dataLen, (int) more_bit);
   }
}

/**
 * called when we see a new rmf_cmdFileInfo_t in the input/parse stream
 */
static void apx_es_fileManager_processRemoteFileInfo(apx_es_fileManager_t *self, const rmf_fileInfo_t *fileInfo)
{
   if (fileInfo != 0)
   {
      int32_t i;
      apx_file_t *file=0;
      int32_t removeIndex=-1;
      for(i=0;i<self->numRequestedFiles;i++)
      {
         file = self->requestedFileList[i];
         if (file != 0)
         {
            if (strcmp(file->fileInfo.name, fileInfo->name)==0)
            {
               if (file->fileInfo.length == fileInfo->length)
               {
                  removeIndex=i;
                  break;
               }
               else
               {
#if APX_DEBUG_ENABLE
                  fprintf(stderr, "[APX_ES_FILEMANAGER] unexpected file size of file %s. Expected %d, got %d\n",file->fileInfo.name,
                        file->fileInfo.length, fileInfo->length);
#endif
               }
            }
         }
      }
      if (removeIndex>=0)
      {
         apx_msg_t msg = {RMF_MSG_FILE_OPEN, 0, 0, 0};
#if APX_DEBUG_ENABLE
         printf("Opening requested file: %s\n", fileInfo->name);
#endif
         //remove file from requestedFileList
         int8_t rc = apx_es_fileManager_removeRequestedAt(self, removeIndex);
         assert(rc == 0);
         assert(file != 0);
         //copy fileInfo data into and file->fileInfo
         file->fileInfo.address = fileInfo->address;
         file->fileInfo.fileType = fileInfo->fileType;
         file->fileInfo.digestType = fileInfo->digestType;
         memcpy(&file->fileInfo.digestData, fileInfo->digestData, RMF_DIGEST_SIZE);
         msg.msgData1 = file->fileInfo.address;
         msg.msgData3 = (void*) file;
#if APX_DEBUG_ENABLE
         if (rbfs_free(&self->messageQueue) <= APX_MSQ_QUEUE_WARN_THRESHOLD)
         {
            fprintf(stderr, "messageQueue fill warning for RMF_MSG_FILE_OPEN. Free before add: %d\n", rbfs_free(&self->messageQueue));
         }
#endif
         rbfs_insert(&self->messageQueue, (const uint8_t*) &msg);
      }
   }
}

static void apx_es_fileManager_processOpenFile(apx_es_fileManager_t *self, const rmf_cmdOpenFile_t *cmdOpenFile)
{
   if (cmdOpenFile != 0)
   {
      apx_file_t *localFile = apx_es_fileMap_findByAddress(&self->localFileMap, cmdOpenFile->address);
      if (localFile != 0)
      {
         apx_msg_t msg = {RMF_MSG_FILE_SEND,0,0,0};
         msg.msgData3 = localFile;
#if APX_DEBUG_ENABLE
         if (rbfs_free(&self->messageQueue) <= APX_MSQ_QUEUE_WARN_THRESHOLD)
         {
            fprintf(stderr, "messageQueue fill warning for RMF_MSG_FILE_SEND. Free before add: %d\n", rbfs_free(&self->messageQueue));
         }
#endif
         rbfs_insert(&self->messageQueue,(uint8_t*) &msg);
      }
   }
}


static int32_t apx_es_processPendingWrite(apx_es_fileManager_t *self)
{
   if (self->transmitBuf != 0)
   {
      int32_t retval=0;
      uint32_t sendAvail = self->transmitBufLen - self->transmitBufUsed;
      if (sendAvail >= APX_ES_FILE_WRITE_MSG_FRAGMENTATION_THRESHOLD)
      {
#if APX_DEBUG_ENABLE
         printf("apx_es_processPendingWrite, remain=%d, offset=%d, address=%08X\n",
               (int) self->fileWriteInfo.remain, (int) self->fileWriteInfo.readOffset, self->fileWriteInfo.writeAddress);
#endif
         uint32_t dataLen;
         uint32_t msgLen;
         bool more_bit = false;
         uint8_t* msgBuf = &self->transmitBuf[self->transmitBufUsed];
         uint32_t headerLen = (self->fileWriteInfo.writeAddress < RMF_DATA_LOW_MAX_ADDR)? RMF_LOW_ADDRESS_SIZE : RMF_HIGH_ADDRESS_SIZE;
         msgLen = headerLen + self->fileWriteInfo.remain;
         if (msgLen > sendAvail)
         {
            // TODO ask the apx_file for the closest boundary where atomic read is needed
            dataLen = sendAvail-headerLen;
            msgLen = sendAvail;
            more_bit = true;
         }
         else
         {
            dataLen = self->fileWriteInfo.remain;
         }
         if (msgBuf != 0)
         {
            assert(msgLen > 0);
            if ( (headerLen ==
                  rmf_packHeaderBeforeData(&msgBuf[headerLen],
                                           headerLen,
                                           self->fileWriteInfo.writeAddress,
                                           more_bit) ) &&
                 (0 == apx_file_read(self->fileWriteInfo.localFile,
                                     &msgBuf[headerLen],
                                     self->fileWriteInfo.readOffset,
                                     dataLen) ) )
            {
               retval = msgLen;
               self->fileWriteInfo.remain-=dataLen;
               if (self->fileWriteInfo.remain==0)
               {
                  assert(more_bit == false);
                  self->pendingWrite = false;
                  memset(&self->fileWriteInfo, 0, sizeof(apx_es_file_write_t));
               }
               else
               {
                  self->fileWriteInfo.writeAddress+=dataLen;
                  self->fileWriteInfo.readOffset+=dataLen;
               }
            }
            else
            {
               retval = -1;
            }
         }
         else
         {
            retval = -1;
         }
      }
      if (retval > 0)
      {
         self->transmitBufUsed += (uint32_t)retval;
      }
      if (self->transmitBufUsed > 0)
      {
         // Responsible for flushing own and others pending send
         apx_es_transmitMsg(self);
      }
      return retval;
   }
   return -1;
}

/**
 * returns -1 on failure, 0 on success
 */
DYN_STATIC int8_t apx_es_fileManager_removeRequestedAt(apx_es_fileManager_t *self, int32_t removeIndex)
{
   if ( (self != 0) && (removeIndex>=0) && (removeIndex < self->numRequestedFiles) )
   {
      int32_t i;
      for(i=removeIndex+1; i<self->numRequestedFiles;i++)
      {
         //move item from removeIndex to removeIndex-1
         self->requestedFileList[i-1] = self->requestedFileList[i];
      }
      self->numRequestedFiles--; //remove last item
      return 0;
   }
   return -1;
}
