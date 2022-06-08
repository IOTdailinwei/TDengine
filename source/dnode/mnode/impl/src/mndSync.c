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

#define _DEFAULT_SOURCE
#include "mndSync.h"
#include "mndTrans.h"

int32_t mndSyncEqMsg(const SMsgCb *msgcb, SRpcMsg *pMsg) {
  SMsgHead *pHead = pMsg->pCont;
  pHead->contLen = htonl(pHead->contLen);
  pHead->vgId = htonl(pHead->vgId);

  return tmsgPutToQueue(msgcb, SYNC_QUEUE, pMsg);
}

int32_t mndSyncSendMsg(const SEpSet *pEpSet, SRpcMsg *pMsg) { return tmsgSendReq(pEpSet, pMsg); }

void mndSyncCommitMsg(struct SSyncFSM *pFsm, const SRpcMsg *pMsg, SFsmCbMeta cbMeta) {
  SMnode    *pMnode = pFsm->data;
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;
  SSdbRaw   *pRaw = pMsg->pCont;

  int32_t transId = sdbGetIdFromRaw(pMnode->pSdb, pRaw);
  pMgmt->errCode = cbMeta.code;
  mTrace("trans:%d, is proposed, savedTransId:%d code:0x%x, ver:%" PRId64 " term:%" PRId64 " role:%s raw:%p", transId,
         pMgmt->transId, cbMeta.code, cbMeta.index, cbMeta.term, syncStr(cbMeta.state), pRaw);

  if (pMgmt->errCode == 0) {
    sdbWriteWithoutFree(pMnode->pSdb, pRaw);
    sdbSetApplyIndex(pMnode->pSdb, cbMeta.index);
    sdbSetApplyTerm(pMnode->pSdb, cbMeta.term);
  }

  if (pMgmt->transId == transId) {
    if (pMgmt->errCode != 0) {
      mError("trans:%d, failed to propose since %s", transId, tstrerror(pMgmt->errCode));
    }
    tsem_post(&pMgmt->syncSem);
  } else {
    if (cbMeta.index - sdbGetApplyIndex(pMnode->pSdb) > 100) {
      sdbWriteFile(pMnode->pSdb);
    }
  }
}

int32_t mndSyncGetSnapshot(struct SSyncFSM *pFsm, SSnapshot *pSnapshot) {
  SMnode *pMnode = pFsm->data;
  pSnapshot->lastApplyIndex = sdbGetApplyIndex(pMnode->pSdb);
  pSnapshot->lastApplyTerm = sdbGetApplyTerm(pMnode->pSdb);
  return 0;
}

void mndRestoreFinish(struct SSyncFSM *pFsm) {
  SMnode *pMnode = pFsm->data;
  if (!pMnode->deploy) {
    mInfo("mnode sync restore finished, and will handle outstanding transactions");
    mndTransPullup(pMnode);
    mndSetRestore(pMnode, true);
  } else {
    mInfo("mnode sync restore finished, and will set ready after first deploy");
  }
}

void mndReConfig(struct SSyncFSM *pFsm, SSyncCfg newCfg, SReConfigCbMeta cbMeta) {
  SMnode    *pMnode = pFsm->data;
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;

  pMgmt->errCode = cbMeta.code;
  mInfo("trans:-1, sync reconfig is proposed, savedTransId:%d code:0x%x, curTerm:%" PRId64 " term:%" PRId64,
        pMgmt->transId, cbMeta.code, cbMeta.index, cbMeta.term);

  if (pMgmt->transId == -1) {
    if (pMgmt->errCode != 0) {
      mError("trans:-1, failed to propose sync reconfig since %s", tstrerror(pMgmt->errCode));
    }
    tsem_post(&pMgmt->syncSem);
  }
}

int32_t mndSnapshotStartRead(struct SSyncFSM *pFsm, void **ppReader) {
  mInfo("start to read snapshot from sdb");
  SMnode *pMnode = pFsm->data;
  return sdbStartRead(pMnode->pSdb, (SSdbIter **)ppReader);
}

int32_t mndSnapshotStopRead(struct SSyncFSM *pFsm, void *pReader) {
  mInfo("stop to read snapshot from sdb");
  SMnode *pMnode = pFsm->data;
  return sdbStopRead(pMnode->pSdb, pReader);
}

int32_t mndSnapshotDoRead(struct SSyncFSM *pFsm, void *pReader, void **ppBuf, int32_t *len) {
  SMnode *pMnode = pFsm->data;
  return sdbDoRead(pMnode->pSdb, pReader, ppBuf, len);
}

int32_t mndSnapshotStartWrite(struct SSyncFSM *pFsm, void **ppWriter) {
  mInfo("start to apply snapshot to sdb");
  SMnode *pMnode = pFsm->data;
  return sdbStartWrite(pMnode->pSdb, (SSdbIter **)ppWriter);
}

int32_t mndSnapshotStopWrite(struct SSyncFSM *pFsm, void *pWriter, bool isApply) {
  mInfo("stop to apply snapshot to sdb, apply:%d", isApply);
  SMnode *pMnode = pFsm->data;
  return sdbStopWrite(pMnode->pSdb, pWriter, isApply);
}

int32_t mndSnapshotDoWrite(struct SSyncFSM *pFsm, void *pWriter, void *pBuf, int32_t len) {
  SMnode *pMnode = pFsm->data;
  return sdbDoWrite(pMnode->pSdb, pWriter, pBuf, len);
}

SSyncFSM *mndSyncMakeFsm(SMnode *pMnode) {
  SSyncFSM *pFsm = taosMemoryCalloc(1, sizeof(SSyncFSM));
  pFsm->data = pMnode;
  pFsm->FpCommitCb = mndSyncCommitMsg;
  pFsm->FpPreCommitCb = NULL;
  pFsm->FpRollBackCb = NULL;
  pFsm->FpRestoreFinishCb = mndRestoreFinish;
  pFsm->FpReConfigCb = mndReConfig;
  pFsm->FpGetSnapshot = mndSyncGetSnapshot;
  pFsm->FpSnapshotStartRead = mndSnapshotStartRead;
  pFsm->FpSnapshotStopRead = mndSnapshotStopRead;
  pFsm->FpSnapshotDoRead = mndSnapshotDoRead;
  pFsm->FpSnapshotStartWrite = mndSnapshotStartWrite;
  pFsm->FpSnapshotStopWrite = mndSnapshotStopWrite;
  pFsm->FpSnapshotDoWrite = mndSnapshotDoWrite;
  return pFsm;
}

int32_t mndInitSync(SMnode *pMnode) {
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;

  char path[PATH_MAX + 20] = {0};
  snprintf(path, sizeof(path), "%s%swal", pMnode->path, TD_DIRSEP);
  SWalCfg cfg = {
      .vgId = 1,
      .fsyncPeriod = 0,
      .rollPeriod = -1,
      .segSize = -1,
      .retentionPeriod = -1,
      .retentionSize = -1,
      .level = TAOS_WAL_FSYNC,
  };

  pMgmt->pWal = walOpen(path, &cfg);
  if (pMgmt->pWal == NULL) {
    mError("failed to open wal since %s", terrstr());
    return -1;
  }

  SSyncInfo syncInfo = {.vgId = 1, .FpSendMsg = mndSyncSendMsg, .FpEqMsg = mndSyncEqMsg};
  snprintf(syncInfo.path, sizeof(syncInfo.path), "%s%ssync", pMnode->path, TD_DIRSEP);
  syncInfo.pWal = pMgmt->pWal;
  syncInfo.pFsm = mndSyncMakeFsm(pMnode);
  syncInfo.isStandBy = pMgmt->standby;

  SSyncCfg *pCfg = &syncInfo.syncCfg;
  pCfg->replicaNum = pMnode->replica;
  pCfg->myIndex = pMnode->selfIndex;
  mInfo("start to open mnode sync, replica:%d myindex:%d standby:%d", pCfg->replicaNum, pCfg->myIndex, pMgmt->standby);
  for (int32_t i = 0; i < pMnode->replica; ++i) {
    SNodeInfo *pNode = &pCfg->nodeInfo[i];
    tstrncpy(pNode->nodeFqdn, pMnode->replicas[i].fqdn, sizeof(pNode->nodeFqdn));
    pNode->nodePort = pMnode->replicas[i].port;
    mInfo("index:%d, fqdn:%s port:%d", i, pNode->nodeFqdn, pNode->nodePort);
  }

  tsem_init(&pMgmt->syncSem, 0, 0);
  pMgmt->sync = syncOpen(&syncInfo);
  if (pMgmt->sync <= 0) {
    mError("failed to open sync since %s", terrstr());
    return -1;
  }

  mDebug("mnode sync is opened, id:%" PRId64, pMgmt->sync);
  return 0;
}

void mndCleanupSync(SMnode *pMnode) {
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;
  syncStop(pMgmt->sync);
  mDebug("mnode sync is stopped, id:%" PRId64, pMgmt->sync);

  tsem_destroy(&pMgmt->syncSem);
  if (pMgmt->pWal != NULL) {
    walClose(pMgmt->pWal);
  }

  memset(pMgmt, 0, sizeof(SSyncMgmt));
}

int32_t mndSyncPropose(SMnode *pMnode, SSdbRaw *pRaw, int32_t transId) {
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;
  SRpcMsg    rsp = {.code = TDMT_MND_APPLY_MSG, .contLen = sdbGetRawTotalSize(pRaw)};
  rsp.pCont = rpcMallocCont(rsp.contLen);
  if (rsp.pCont == NULL) return -1;
  memcpy(rsp.pCont, pRaw, rsp.contLen);

  pMgmt->errCode = 0;
  pMgmt->transId = transId;
  mTrace("trans:%d, will be proposed", pMgmt->transId);

  const bool isWeak = false;
  int32_t    code = syncPropose(pMgmt->sync, &rsp, isWeak);
  if (code == 0) {
    tsem_wait(&pMgmt->syncSem);
  } else if (code == TAOS_SYNC_PROPOSE_NOT_LEADER) {
    terrno = TSDB_CODE_APP_NOT_READY;
  } else if (code == TAOS_SYNC_PROPOSE_OTHER_ERROR) {
    terrno = TSDB_CODE_SYN_INTERNAL_ERROR;
  } else {
    terrno = TSDB_CODE_APP_ERROR;
  }

  rpcFreeCont(rsp.pCont);
  if (code != 0) {
    mError("trans:%d, failed to propose, code:0x%x", pMgmt->transId, code);
    return code;
  }

  return pMgmt->errCode;
}

void mndSyncStart(SMnode *pMnode) {
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;
  syncSetMsgCb(pMgmt->sync, &pMnode->msgCb);

  if (pMgmt->standby) {
    syncStartStandBy(pMgmt->sync);
  } else {
    syncStart(pMgmt->sync);
  }
  mDebug("mnode sync started, id:%" PRId64 " standby:%d", pMgmt->sync, pMgmt->standby);
}

void mndSyncStop(SMnode *pMnode) {}

bool mndIsMaster(SMnode *pMnode) {
  SSyncMgmt *pMgmt = &pMnode->syncMgmt;

  ESyncState state = syncGetMyRole(pMgmt->sync);
  if (state != TAOS_SYNC_STATE_LEADER) {
    terrno = TSDB_CODE_SYN_NOT_LEADER;
    return false;
  }

  if (!pMnode->restored) {
    terrno = TSDB_CODE_APP_NOT_READY;
    return false;
  }

  return true;
}
