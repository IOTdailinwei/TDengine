/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "syncIO.h"
#include <tdatablock.h>
#include "syncMessage.h"
#include "syncUtil.h"
#include "tglobal.h"
#include "ttimer.h"
#include "tutil.h"

SSyncIO *gSyncIO = NULL;

// local function ------------
static SSyncIO *syncIOCreate(char *host, uint16_t port);
static int32_t  syncIODestroy(SSyncIO *io);
static int32_t  syncIOStartInternal(SSyncIO *io);
static int32_t  syncIOStopInternal(SSyncIO *io);

static void   *syncIOConsumerFunc(void *param);
static void    syncIOProcessRequest(void *pParent, SRpcMsg *pMsg, SEpSet *pEpSet);
static void    syncIOProcessReply(void *pParent, SRpcMsg *pMsg, SEpSet *pEpSet);
static int32_t syncIOAuth(void *parent, char *meterId, char *spi, char *encrypt, char *secret, char *ckey);

static int32_t syncIOStartQ(SSyncIO *io);
static int32_t syncIOStopQ(SSyncIO *io);
static int32_t syncIOStartPing(SSyncIO *io);
static int32_t syncIOStopPing(SSyncIO *io);
static void    syncIOTickQ(void *param, void *tmrId);
static void    syncIOTickPing(void *param, void *tmrId);
// ----------------------------

// public function ------------
int32_t syncIOStart(char *host, uint16_t port) {
  int32_t ret = 0;
  gSyncIO = syncIOCreate(host, port);
  assert(gSyncIO != NULL);

  taosSeedRand(taosGetTimestampSec());
  ret = syncIOStartInternal(gSyncIO);
  assert(ret == 0);

  sTrace("syncIOStart ok, gSyncIO:%p", gSyncIO);
  return ret;
}

int32_t syncIOStop() {
  int32_t ret = syncIOStopInternal(gSyncIO);
  assert(ret == 0);

  ret = syncIODestroy(gSyncIO);
  assert(ret == 0);
  return ret;
}

int32_t syncIOSendMsg(void *clientRpc, const SEpSet *pEpSet, SRpcMsg *pMsg) {
  assert(pEpSet->inUse == 0);
  assert(pEpSet->numOfEps == 1);

  int32_t ret = 0;
  char    logBuf[256];
  snprintf(logBuf, sizeof(logBuf), "==syncIOSendMsg== %s:%d", pEpSet->eps[0].fqdn, pEpSet->eps[0].port);
  syncRpcMsgPrint2(logBuf, pMsg);

  pMsg->handle = NULL;
  pMsg->noResp = 1;
  rpcSendRequest(clientRpc, pEpSet, pMsg, NULL);
  return ret;
}

int32_t syncIOEqMsg(void *queue, SRpcMsg *pMsg) {
  int32_t ret = 0;
  char    logBuf[128];
  syncRpcMsgPrint2((char *)"==syncIOEqMsg==", pMsg);

  SRpcMsg *pTemp;
  pTemp = taosAllocateQitem(sizeof(SRpcMsg));
  memcpy(pTemp, pMsg, sizeof(SRpcMsg));

  STaosQueue *pMsgQ = queue;
  taosWriteQitem(pMsgQ, pTemp);

  return ret;
}

int32_t syncIOQTimerStart() {
  int32_t ret = syncIOStartQ(gSyncIO);
  assert(ret == 0);
  return ret;
}

int32_t syncIOQTimerStop() {
  int32_t ret = syncIOStopQ(gSyncIO);
  assert(ret == 0);
  return ret;
}

int32_t syncIOPingTimerStart() {
  int32_t ret = syncIOStartPing(gSyncIO);
  assert(ret == 0);
  return ret;
}

int32_t syncIOPingTimerStop() {
  int32_t ret = syncIOStopPing(gSyncIO);
  assert(ret == 0);
  return ret;
}

// local function ------------
static SSyncIO *syncIOCreate(char *host, uint16_t port) {
  SSyncIO *io = (SSyncIO *)taosMemoryMalloc(sizeof(SSyncIO));
  memset(io, 0, sizeof(*io));

  io->pMsgQ = taosOpenQueue();
  io->pQset = taosOpenQset();
  taosAddIntoQset(io->pQset, io->pMsgQ, NULL);

  io->myAddr.inUse = 0;
  io->myAddr.numOfEps = 0;
  addEpIntoEpSet(&io->myAddr, host, port);

  io->qTimerMS = TICK_Q_TIMER_MS;
  io->pingTimerMS = TICK_Ping_TIMER_MS;

  return io;
}

static int32_t syncIODestroy(SSyncIO *io) {
  int32_t ret = 0;
  int8_t  start = atomic_load_8(&io->isStart);
  assert(start == 0);

  if (io->serverRpc != NULL) {
    rpcClose(io->serverRpc);
    io->serverRpc = NULL;
  }

  if (io->clientRpc != NULL) {
    rpcClose(io->clientRpc);
    io->clientRpc = NULL;
  }

  taosCloseQueue(io->pMsgQ);
  taosCloseQset(io->pQset);

  return ret;
}

static int32_t syncIOStartInternal(SSyncIO *io) {
  int32_t ret = 0;
  taosBlockSIGPIPE();

  rpcInit();
  tsRpcForceTcp = 1;

  // cient rpc init
  {
    SRpcInit rpcInit;
    memset(&rpcInit, 0, sizeof(rpcInit));
    rpcInit.localPort = 0;
    rpcInit.label = "SYNC-IO-CLIENT";
    rpcInit.numOfThreads = 1;
    rpcInit.cfp = syncIOProcessReply;
    rpcInit.sessions = 100;
    rpcInit.idleTime = 100;
    rpcInit.user = "sync-io";
    rpcInit.secret = "sync-io";
    rpcInit.ckey = "key";
    rpcInit.spi = 0;
    rpcInit.connType = TAOS_CONN_CLIENT;

    io->clientRpc = rpcOpen(&rpcInit);
    if (io->clientRpc == NULL) {
      sError("failed to initialize RPC");
      return -1;
    }
  }

  // server rpc init
  {
    SRpcInit rpcInit;
    memset(&rpcInit, 0, sizeof(rpcInit));
    rpcInit.localPort = io->myAddr.eps[0].port;
    rpcInit.label = "SYNC-IO-SERVER";
    rpcInit.numOfThreads = 1;
    rpcInit.cfp = syncIOProcessRequest;
    rpcInit.sessions = 1000;
    rpcInit.idleTime = 2 * 1500;
    rpcInit.afp = syncIOAuth;
    rpcInit.parent = io;
    rpcInit.connType = TAOS_CONN_SERVER;

    void *pRpc = rpcOpen(&rpcInit);
    if (pRpc == NULL) {
      sError("failed to start RPC server");
      return -1;
    }
  }

  // start consumer thread
  {
    if (taosThreadCreate(&io->consumerTid, NULL, syncIOConsumerFunc, io) != 0) {
      sError("failed to create sync consumer thread since %s", strerror(errno));
      terrno = TAOS_SYSTEM_ERROR(errno);
      return -1;
    }
  }

  // start tmr thread
  io->timerMgr = taosTmrInit(1000, 50, 10000, "SYNC-IO");

  atomic_store_8(&io->isStart, 1);
  return ret;
}

static int32_t syncIOStopInternal(SSyncIO *io) {
  int32_t ret = 0;
  atomic_store_8(&io->isStart, 0);
  taosThreadJoin(io->consumerTid, NULL);
  taosTmrCleanUp(io->timerMgr);
  return ret;
}

static void *syncIOConsumerFunc(void *param) {
  SSyncIO   *io = param;
  STaosQall *qall;
  SRpcMsg   *pRpcMsg, rpcMsg;
  qall = taosAllocateQall();

  while (1) {
    int numOfMsgs = taosReadAllQitemsFromQset(io->pQset, qall, NULL, NULL);
    sTrace("syncIOConsumerFunc %d msgs are received", numOfMsgs);
    if (numOfMsgs <= 0) {
      break;
    }

    for (int i = 0; i < numOfMsgs; ++i) {
      taosGetQitem(qall, (void **)&pRpcMsg);
      syncRpcMsgLog2((char *)"==syncIOConsumerFunc==", pRpcMsg);

      // use switch case instead of if else
      if (pRpcMsg->msgType == TDMT_VND_SYNC_PING) {
        if (io->FpOnSyncPing != NULL) {
          SyncPing *pSyncMsg = syncPingFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncPing(io->pSyncNode, pSyncMsg);
          syncPingDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == TDMT_VND_SYNC_PING_REPLY) {
        if (io->FpOnSyncPingReply != NULL) {
          SyncPingReply *pSyncMsg = syncPingReplyFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncPingReply(io->pSyncNode, pSyncMsg);
          syncPingReplyDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == SYNC_CLIENT_REQUEST) {
        if (io->FpOnSyncClientRequest != NULL) {
          SyncClientRequest *pSyncMsg = syncClientRequestFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncClientRequest(io->pSyncNode, pSyncMsg);
          syncClientRequestDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == SYNC_REQUEST_VOTE) {
        if (io->FpOnSyncRequestVote != NULL) {
          SyncRequestVote *pSyncMsg = syncRequestVoteFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncRequestVote(io->pSyncNode, pSyncMsg);
          syncRequestVoteDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == SYNC_REQUEST_VOTE_REPLY) {
        if (io->FpOnSyncRequestVoteReply != NULL) {
          SyncRequestVoteReply *pSyncMsg = syncRequestVoteReplyFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncRequestVoteReply(io->pSyncNode, pSyncMsg);
          syncRequestVoteReplyDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == SYNC_APPEND_ENTRIES) {
        if (io->FpOnSyncAppendEntries != NULL) {
          SyncAppendEntries *pSyncMsg = syncAppendEntriesFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncAppendEntries(io->pSyncNode, pSyncMsg);
          syncAppendEntriesDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == SYNC_APPEND_ENTRIES_REPLY) {
        if (io->FpOnSyncAppendEntriesReply != NULL) {
          SyncAppendEntriesReply *pSyncMsg = syncAppendEntriesReplyFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncAppendEntriesReply(io->pSyncNode, pSyncMsg);
          syncAppendEntriesReplyDestroy(pSyncMsg);
        }

      } else if (pRpcMsg->msgType == TDMT_VND_SYNC_TIMEOUT) {
        if (io->FpOnSyncTimeout != NULL) {
          SyncTimeout *pSyncMsg = syncTimeoutFromRpcMsg2(pRpcMsg);
          assert(pSyncMsg != NULL);
          io->FpOnSyncTimeout(io->pSyncNode, pSyncMsg);
          syncTimeoutDestroy(pSyncMsg);
        }
      } else {
        sTrace("unknown msgType:%d, no operator", pRpcMsg->msgType);
      }
    }

    taosResetQitems(qall);
    for (int i = 0; i < numOfMsgs; ++i) {
      taosGetQitem(qall, (void **)&pRpcMsg);
      rpcFreeCont(pRpcMsg->pCont);

      /*
            if (pRpcMsg->handle != NULL) {
              int msgSize = 32;
              memset(&rpcMsg, 0, sizeof(rpcMsg));
              rpcMsg.msgType = SYNC_RESPONSE;
              rpcMsg.pCont = rpcMallocCont(msgSize);
              rpcMsg.contLen = msgSize;
              snprintf(rpcMsg.pCont, rpcMsg.contLen, "%s", "give a reply");
              rpcMsg.handle = pRpcMsg->handle;
              rpcMsg.code = 0;

              syncRpcMsgPrint2((char *)"syncIOConsumerFunc rpcSendResponse --> ", &rpcMsg);
              rpcSendResponse(&rpcMsg);
            }
      */

      taosFreeQitem(pRpcMsg);
    }
  }

  taosFreeQall(qall);
  return NULL;
}

static void syncIOProcessRequest(void *pParent, SRpcMsg *pMsg, SEpSet *pEpSet) {
  syncRpcMsgPrint2((char *)"==syncIOProcessRequest==", pMsg);
  SSyncIO *io = pParent;
  SRpcMsg *pTemp;
  pTemp = taosAllocateQitem(sizeof(SRpcMsg));
  memcpy(pTemp, pMsg, sizeof(SRpcMsg));
  taosWriteQitem(io->pMsgQ, pTemp);
}

static void syncIOProcessReply(void *pParent, SRpcMsg *pMsg, SEpSet *pEpSet) {
  if (pMsg->msgType == SYNC_RESPONSE) {
    sTrace("==syncIOProcessReply==");
  } else {
    syncRpcMsgPrint2((char *)"==syncIOProcessReply==", pMsg);
  }
  rpcFreeCont(pMsg->pCont);
}

static int32_t syncIOAuth(void *parent, char *meterId, char *spi, char *encrypt, char *secret, char *ckey) {
  // app shall retrieve the auth info based on meterID from DB or a data file
  // demo code here only for simple demo
  int32_t ret = 0;
  return ret;
}

static int32_t syncIOStartQ(SSyncIO *io) {
  int32_t ret = 0;
  taosTmrReset(syncIOTickQ, io->qTimerMS, io, io->timerMgr, &io->qTimer);
  return ret;
}

static int32_t syncIOStopQ(SSyncIO *io) {
  int32_t ret = 0;
  taosTmrStop(io->qTimer);
  io->qTimer = NULL;
  return ret;
}

static int32_t syncIOStartPing(SSyncIO *io) {
  int32_t ret = 0;
  taosTmrReset(syncIOTickPing, io->pingTimerMS, io, io->timerMgr, &io->pingTimer);
  return ret;
}

static int32_t syncIOStopPing(SSyncIO *io) {
  int32_t ret = 0;
  taosTmrStop(io->pingTimer);
  io->pingTimer = NULL;
  return ret;
}

static void syncIOTickQ(void *param, void *tmrId) {
  SSyncIO *io = (SSyncIO *)param;

  SRaftId srcId, destId;
  srcId.addr = syncUtilAddr2U64(io->myAddr.eps[0].fqdn, io->myAddr.eps[0].port);
  srcId.vgId = -1;
  destId.addr = syncUtilAddr2U64(io->myAddr.eps[0].fqdn, io->myAddr.eps[0].port);
  destId.vgId = -1;
  SyncPingReply *pMsg = syncPingReplyBuild2(&srcId, &destId, "syncIOTickQ");

  SRpcMsg rpcMsg;
  syncPingReply2RpcMsg(pMsg, &rpcMsg);
  SRpcMsg *pTemp;
  pTemp = taosAllocateQitem(sizeof(SRpcMsg));
  memcpy(pTemp, &rpcMsg, sizeof(SRpcMsg));
  syncRpcMsgPrint2((char *)"==syncIOTickQ==", &rpcMsg);
  taosWriteQitem(io->pMsgQ, pTemp);
  syncPingReplyDestroy(pMsg);

  taosTmrReset(syncIOTickQ, io->qTimerMS, io, io->timerMgr, &io->qTimer);
}

static void syncIOTickPing(void *param, void *tmrId) {
  SSyncIO *io = (SSyncIO *)param;

  SRaftId srcId, destId;
  srcId.addr = syncUtilAddr2U64(io->myAddr.eps[0].fqdn, io->myAddr.eps[0].port);
  srcId.vgId = -1;
  destId.addr = syncUtilAddr2U64(io->myAddr.eps[0].fqdn, io->myAddr.eps[0].port);
  destId.vgId = -1;
  SyncPing *pMsg = syncPingBuild2(&srcId, &destId, "syncIOTickPing");
  // SyncPing *pMsg = syncPingBuild3(&srcId, &destId);

  SRpcMsg rpcMsg;
  syncPing2RpcMsg(pMsg, &rpcMsg);
  syncRpcMsgPrint2((char *)"==syncIOTickPing==", &rpcMsg);
  rpcSendRequest(io->clientRpc, &io->myAddr, &rpcMsg, NULL);
  syncPingDestroy(pMsg);

  taosTmrReset(syncIOTickPing, io->pingTimerMS, io, io->timerMgr, &io->pingTimer);
}