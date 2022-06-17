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

#include "streamInc.h"
#include "ttimer.h"

int32_t streamInit() {
  int8_t old;
  while (1) {
    old = atomic_val_compare_exchange_8(&streamEnv.inited, 0, 2);
    if (old != 2) break;
  }

  if (old == 0) {
    streamEnv.timer = taosTmrInit(10000, 100, 10000, "STREAM");
    if (streamEnv.timer == NULL) {
      atomic_store_8(&streamEnv.inited, 0);
      return -1;
    }
    atomic_store_8(&streamEnv.inited, 1);
  }
  return 0;
}

void streamCleanUp() {
  int8_t old;
  while (1) {
    old = atomic_val_compare_exchange_8(&streamEnv.inited, 1, 2);
    if (old != 2) break;
  }

  if (old == 1) {
    taosTmrCleanUp(streamEnv.timer);
    atomic_store_8(&streamEnv.inited, 0);
  }
}

void streamTriggerByTimer(void* param, void* tmrId) {
  SStreamTask* pTask = (void*)param;

  if (atomic_load_8(&pTask->triggerStatus) == TASK_TRIGGER_STATUS__ACTIVE) {
    SStreamTrigger* trigger = taosAllocateQitem(sizeof(SStreamTrigger), DEF_QITEM);
    if (trigger == NULL) return;
    trigger->type = STREAM_INPUT__TRIGGER;
    trigger->pBlock = taosMemoryCalloc(1, sizeof(SSDataBlock));
    if (trigger->pBlock == NULL) {
      taosFreeQitem(trigger);
      return;
    }
    trigger->pBlock->info.type = STREAM_GET_ALL;

    atomic_store_8(&pTask->triggerStatus, TASK_TRIGGER_STATUS__IN_ACTIVE);

    streamTaskInput(pTask, (SStreamQueueItem*)trigger);
    streamLaunchByWrite(pTask, pTask->nodeId, pTask->pMsgCb);
  }

  taosTmrReset(streamTriggerByTimer, (int32_t)pTask->triggerParam, pTask, streamEnv.timer, &pTask->timer);
}

int32_t streamSetupTrigger(SStreamTask* pTask) {
  if (pTask->triggerParam != 0) {
    if (streamInit() < 0) {
      return -1;
    }
    pTask->timer = taosTmrStart(streamTriggerByTimer, (int32_t)pTask->triggerParam, pTask, streamEnv.timer);
    pTask->triggerStatus = TASK_TRIGGER_STATUS__IN_ACTIVE;
  }
  return 0;
}

int32_t streamLaunchByWrite(SStreamTask* pTask, int32_t vgId, SMsgCb* pMsgCb) {
  int8_t execStatus = atomic_load_8(&pTask->status);
  if (execStatus == TASK_STATUS__IDLE || execStatus == TASK_STATUS__CLOSING) {
    SStreamTaskRunReq* pRunReq = rpcMallocCont(sizeof(SStreamTaskRunReq));
    if (pRunReq == NULL) return -1;

    // TODO: do we need htonl?
    pRunReq->head.vgId = vgId;
    pRunReq->streamId = pTask->streamId;
    pRunReq->taskId = pTask->taskId;
    SRpcMsg msg = {
        .msgType = TDMT_STREAM_TASK_RUN,
        .pCont = pRunReq,
        .contLen = sizeof(SStreamTaskRunReq),
    };
    tmsgPutToQueue(pMsgCb, FETCH_QUEUE, &msg);
  }
  return 0;
}

int32_t streamTaskEnqueue(SStreamTask* pTask, SStreamDispatchReq* pReq, SRpcMsg* pRsp) {
  SStreamDataBlock* pData = taosAllocateQitem(sizeof(SStreamDataBlock), DEF_QITEM);
  int8_t            status;

  // enqueue
  if (pData != NULL) {
    pData->type = STREAM_DATA_TYPE_SSDATA_BLOCK;
    pData->sourceVg = pReq->sourceVg;
    // decode
    /*pData->blocks = pReq->data;*/
    /*pBlock->sourceVer = pReq->sourceVer;*/
    streamDispatchReqToData(pReq, pData);
    if (streamTaskInput(pTask, (SStreamQueueItem*)pData) == 0) {
      status = TASK_INPUT_STATUS__NORMAL;
    } else {
      status = TASK_INPUT_STATUS__FAILED;
    }
  } else {
    streamTaskInputFail(pTask);
    status = TASK_INPUT_STATUS__FAILED;
  }

  // rsp by input status
  void* buf = rpcMallocCont(sizeof(SMsgHead) + sizeof(SStreamDispatchRsp));
  ((SMsgHead*)buf)->vgId = htonl(pReq->upstreamNodeId);
  SStreamDispatchRsp* pCont = POINTER_SHIFT(buf, sizeof(SMsgHead));
  pCont->inputStatus = status;
  pCont->streamId = pReq->streamId;
  pCont->taskId = pReq->sourceTaskId;
  pRsp->pCont = buf;
  pRsp->contLen = sizeof(SMsgHead) + sizeof(SStreamDispatchRsp);
  tmsgSendRsp(pRsp);
  return status == TASK_INPUT_STATUS__NORMAL ? 0 : -1;
}

int32_t streamProcessDispatchReq(SStreamTask* pTask, SMsgCb* pMsgCb, SStreamDispatchReq* pReq, SRpcMsg* pRsp) {
  // 1. handle input
  streamTaskEnqueue(pTask, pReq, pRsp);

  // 2. try exec
  // 2.1. idle: exec
  // 2.2. executing: return
  // 2.3. closing: keep trying
  if (pTask->execType != TASK_EXEC__NONE) {
    streamExec(pTask, pMsgCb);
  } else {
    ASSERT(pTask->sinkType != TASK_SINK__NONE);
    while (1) {
      void* data = streamQueueNextItem(pTask->inputQueue);
      if (data == NULL) return 0;
      if (streamTaskOutput(pTask, data) < 0) {
        ASSERT(0);
      }
    }
  }

  // 3. handle output
  // 3.1 check and set status
  // 3.2 dispatch / sink
  if (pTask->dispatchType != TASK_DISPATCH__NONE) {
    streamDispatch(pTask, pMsgCb);
  }

  return 0;
}

int32_t streamProcessDispatchRsp(SStreamTask* pTask, SMsgCb* pMsgCb, SStreamDispatchRsp* pRsp) {
  ASSERT(pRsp->inputStatus == TASK_OUTPUT_STATUS__NORMAL || pRsp->inputStatus == TASK_OUTPUT_STATUS__BLOCKED);
  int8_t old = atomic_exchange_8(&pTask->outputStatus, pRsp->inputStatus);
  ASSERT(old == TASK_OUTPUT_STATUS__WAIT);
  if (pRsp->inputStatus == TASK_INPUT_STATUS__BLOCKED) {
    // TODO: init recover timer
    return 0;
  }
  // continue dispatch
  streamDispatch(pTask, pMsgCb);
  return 0;
}

int32_t streamTaskProcessRunReq(SStreamTask* pTask, SMsgCb* pMsgCb) {
  streamExec(pTask, pMsgCb);
  if (pTask->dispatchType != TASK_DISPATCH__NONE) {
    streamDispatch(pTask, pMsgCb);
  }
  return 0;
}

int32_t streamProcessRecoverReq(SStreamTask* pTask, SMsgCb* pMsgCb, SStreamTaskRecoverReq* pReq, SRpcMsg* pMsg) {
  //
  return 0;
}

int32_t streamProcessRecoverRsp(SStreamTask* pTask, SStreamTaskRecoverRsp* pRsp) {
  //
  return 0;
}
