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

#ifndef _STREAM_INC_H_
#define _STREAM_INC_H_

#include "executor.h"
#include "tstream.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t streamExec(SStreamTask* pTask, SMsgCb* pMsgCb);
int32_t streamDispatch(SStreamTask* pTask, SMsgCb* pMsgCb);
int32_t streamDispatchReqToData(const SStreamDispatchReq* pReq, SStreamDataBlock* pData);
int32_t streamBuildDispatchMsg(SStreamTask* pTask, SStreamDataBlock* data, SRpcMsg* pMsg, SEpSet** ppEpSet);

#ifdef __cplusplus
}
#endif

#endif /* ifndef _STREAM_INC_H_ */
