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

#ifndef _TD_VNODE_TSDB_H_
#define _TD_VNODE_TSDB_H_

#include "vnodeInt.h"

#ifdef __cplusplus
extern "C" {
#endif

// tsdbDebug ================
// clang-format off
#define tsdbFatal(...) do { if (tsdbDebugFlag & DEBUG_FATAL) { taosPrintLog("TSDB FATAL ", DEBUG_FATAL, 255, __VA_ARGS__); }}     while(0)
#define tsdbError(...) do { if (tsdbDebugFlag & DEBUG_ERROR) { taosPrintLog("TSDB ERROR ", DEBUG_ERROR, 255, __VA_ARGS__); }}     while(0)
#define tsdbWarn(...)  do { if (tsdbDebugFlag & DEBUG_WARN)  { taosPrintLog("TSDB WARN ", DEBUG_WARN, 255, __VA_ARGS__); }}       while(0)
#define tsdbInfo(...)  do { if (tsdbDebugFlag & DEBUG_INFO)  { taosPrintLog("TSDB ", DEBUG_INFO, 255, __VA_ARGS__); }}            while(0)
#define tsdbDebug(...) do { if (tsdbDebugFlag & DEBUG_DEBUG) { taosPrintLog("TSDB ", DEBUG_DEBUG, tsdbDebugFlag, __VA_ARGS__); }} while(0)
#define tsdbTrace(...) do { if (tsdbDebugFlag & DEBUG_TRACE) { taosPrintLog("TSDB ", DEBUG_TRACE, tsdbDebugFlag, __VA_ARGS__); }} while(0)
// clang-format on

typedef struct TSDBROW TSDBROW;
typedef struct TSDBKEY TSDBKEY;
typedef struct TABLEID TABLEID;
typedef struct SDelOp  SDelOp;

static int tsdbKeyCmprFn(const void *p1, const void *p2);

// tsdbMemTable ==============================================================================================
typedef struct STbData     STbData;
typedef struct SMemTable   SMemTable;
typedef struct STbDataIter STbDataIter;
typedef struct SMergeInfo  SMergeInfo;
typedef struct STable      STable;

// SMemTable
int32_t tsdbMemTableCreate(STsdb *pTsdb, SMemTable **ppMemTable);
void    tsdbMemTableDestroy(SMemTable *pMemTable);
void    tsdbGetTbDataFromMemTable(SMemTable *pMemTable, tb_uid_t suid, tb_uid_t uid, STbData **ppTbData);

// STbDataIter
int32_t tsdbTbDataIterCreate(STbData *pTbData, TSDBKEY *pFrom, int8_t backward, STbDataIter **ppIter);
void   *tsdbTbDataIterDestroy(STbDataIter *pIter);
void    tsdbTbDataIterOpen(STbData *pTbData, TSDBKEY *pFrom, int8_t backward, STbDataIter *pIter);
bool    tsdbTbDataIterNext(STbDataIter *pIter);
bool    tsdbTbDataIterGet(STbDataIter *pIter, TSDBROW *pRow);

int tsdbLoadDataFromCache(STsdb *pTsdb, STable *pTable, STbDataIter *pIter, TSKEY maxKey, int maxRowsToRead,
                          SDataCols *pCols, TKEY *filterKeys, int nFilterKeys, bool keepDup, SMergeInfo *pMergeInfo);

// tsdbMemTable2.c ==============================================================================================
// typedef struct SMemTable2   SMemTable2;
// typedef struct SMemData     SMemData;
// typedef struct SMemDataIter SMemDataIter;

// int32_t tsdbMemTableCreate2(STsdb *pTsdb, SMemTable2 **ppMemTable);
// void    tsdbMemTableDestroy2(SMemTable2 *pMemTable);
// int32_t tsdbInsertTableData2(STsdb *pTsdb, int64_t version, SVSubmitBlk *pSubmitBlk);
// int32_t tsdbDeleteTableData2(STsdb *pTsdb, int64_t version, tb_uid_t suid, tb_uid_t uid, TSKEY sKey, TSKEY eKey);

// /* SMemDataIter */
// void tsdbMemDataIterOpen(SMemData *pMemData, TSDBKEY *pKey, int8_t backward, SMemDataIter *pIter);
// bool tsdbMemDataIterNext(SMemDataIter *pIter);
// void tsdbMemDataIterGet(SMemDataIter *pIter, TSDBROW **ppRow);

// // tsdbCommit2.c ==============================================================================================
// int32_t tsdbBegin2(STsdb *pTsdb);
// int32_t tsdbCommit2(STsdb *pTsdb);

// tsdbFile.c ==============================================================================================
typedef int32_t          TSDB_FILE_T;
typedef struct SDFInfo   SDFInfo;
typedef struct SDFile    SDFile;
typedef struct SDFileSet SDFileSet;

// SDFile
void    tsdbInitDFile(STsdb *pRepo, SDFile *pDFile, SDiskID did, int fid, uint32_t ver, TSDB_FILE_T ftype);
void    tsdbInitDFileEx(SDFile *pDFile, SDFile *pODFile);
int     tsdbOpenDFile(SDFile *pDFile, int flags);
void    tsdbCloseDFile(SDFile *pDFile);
int64_t tsdbSeekDFile(SDFile *pDFile, int64_t offset, int whence);
int64_t tsdbWriteDFile(SDFile *pDFile, void *buf, int64_t nbyte);
void    tsdbUpdateDFileMagic(SDFile *pDFile, void *pCksm);
int     tsdbAppendDFile(SDFile *pDFile, void *buf, int64_t nbyte, int64_t *offset);
int     tsdbRemoveDFile(SDFile *pDFile);
int64_t tsdbReadDFile(SDFile *pDFile, void *buf, int64_t nbyte);
int     tsdbCopyDFile(SDFile *pSrc, SDFile *pDest);
int     tsdbEncodeSDFile(void **buf, SDFile *pDFile);
void   *tsdbDecodeSDFile(STsdb *pRepo, void *buf, SDFile *pDFile);
int     tsdbCreateDFile(STsdb *pRepo, SDFile *pDFile, bool updateHeader, TSDB_FILE_T fType);
int     tsdbUpdateDFileHeader(SDFile *pDFile);
int     tsdbLoadDFileHeader(SDFile *pDFile, SDFInfo *pInfo);
int     tsdbParseDFilename(const char *fname, int *vid, int *fid, TSDB_FILE_T *ftype, uint32_t *version);

// SDFileSet
void  tsdbInitDFileSet(STsdb *pRepo, SDFileSet *pSet, SDiskID did, int fid, uint32_t ver);
void  tsdbInitDFileSetEx(SDFileSet *pSet, SDFileSet *pOSet);
int   tsdbEncodeDFileSet(void **buf, SDFileSet *pSet);
void *tsdbDecodeDFileSet(STsdb *pRepo, void *buf, SDFileSet *pSet);
int   tsdbEncodeDFileSetEx(void **buf, SDFileSet *pSet);
void *tsdbDecodeDFileSetEx(void *buf, SDFileSet *pSet);
int   tsdbApplyDFileSetChange(SDFileSet *from, SDFileSet *to);
int   tsdbCreateDFileSet(STsdb *pRepo, SDFileSet *pSet, bool updateHeader);
int   tsdbUpdateDFileSetHeader(SDFileSet *pSet);
int   tsdbScanAndTryFixDFileSet(STsdb *pRepo, SDFileSet *pSet);
void  tsdbCloseDFileSet(SDFileSet *pSet);
int   tsdbOpenDFileSet(SDFileSet *pSet, int flags);
void  tsdbRemoveDFileSet(SDFileSet *pSet);
int   tsdbCopyDFileSet(SDFileSet *pSrc, SDFileSet *pDest);
void  tsdbGetFidKeyRange(int days, int8_t precision, int fid, TSKEY *minKey, TSKEY *maxKey);

// tsdbFS.c ==============================================================================================
typedef struct STsdbFS     STsdbFS;
typedef struct SFSIter     SFSIter;
typedef struct STsdbFSMeta STsdbFSMeta;

STsdbFS *tsdbNewFS(const STsdbKeepCfg *pCfg);
void    *tsdbFreeFS(STsdbFS *pfs);
int      tsdbOpenFS(STsdb *pRepo);
void     tsdbCloseFS(STsdb *pRepo);
void     tsdbStartFSTxn(STsdb *pRepo, int64_t pointsAdd, int64_t storageAdd);
int      tsdbEndFSTxn(STsdb *pRepo);
int      tsdbEndFSTxnWithError(STsdbFS *pfs);
void     tsdbUpdateFSTxnMeta(STsdbFS *pfs, STsdbFSMeta *pMeta);
// void     tsdbUpdateMFile(STsdbFS *pfs, const SMFile *pMFile);
int tsdbUpdateDFileSet(STsdbFS *pfs, const SDFileSet *pSet);

void       tsdbFSIterInit(SFSIter *pIter, STsdbFS *pfs, int direction);
void       tsdbFSIterSeek(SFSIter *pIter, int fid);
SDFileSet *tsdbFSIterNext(SFSIter *pIter);
int        tsdbLoadMetaCache(STsdb *pRepo, bool recoverMeta);
int        tsdbRLockFS(STsdbFS *pFs);
int        tsdbWLockFS(STsdbFS *pFs);
int        tsdbUnLockFS(STsdbFS *pFs);

// structs
typedef struct {
  int   minFid;
  int   midFid;
  int   maxFid;
  TSKEY minKey;
} SRtn;

#define TSDB_DATA_DIR_LEN 6  // adapt accordingly
struct STsdb {
  char         *path;
  SVnode       *pVnode;
  TdThreadMutex mutex;
  char          dir[TSDB_DATA_DIR_LEN];
  bool          repoLocked;
  STsdbKeepCfg  keepCfg;
  SMemTable    *mem;
  SMemTable    *imem;
  SRtn          rtn;
  STsdbFS      *fs;
};

#if 1  // ======================================

struct STable {
  uint64_t  suid;
  uint64_t  uid;
  STSchema *pSchema;       // latest schema
  STSchema *pCacheSchema;  // cached cache
};

// int tsdbPrepareCommit(STsdb *pTsdb);
typedef enum {
  TSDB_FILE_HEAD = 0,  // .head
  TSDB_FILE_DATA,      // .data
  TSDB_FILE_LAST,      // .last
  TSDB_FILE_SMAD,      // .smad(Block-wise SMA)
  TSDB_FILE_SMAL,      // .smal(Block-wise SMA)
  TSDB_FILE_MAX,       //
  TSDB_FILE_META,      // meta
} E_TSDB_FILE_T;

struct SDFInfo {
  uint32_t magic;
  uint32_t fver;
  uint32_t len;
  uint32_t totalBlocks;
  uint32_t totalSubBlocks;
  uint32_t offset;
  uint64_t size;
  uint64_t tombSize;
};

struct SDFile {
  SDFInfo   info;
  STfsFile  f;
  TdFilePtr pFile;
  uint8_t   state;
};

struct SDFileSet {
  int      fid;
  int8_t   state;  // -128~127
  uint8_t  ver;    // 0~255, DFileSet version
  uint16_t reserve;
  SDFile   files[TSDB_FILE_MAX];
};

struct TSDBKEY {
  int64_t version;
  TSKEY   ts;
};

typedef struct SMemSkipListNode SMemSkipListNode;
struct SMemSkipListNode {
  int8_t            level;
  SMemSkipListNode *forwards[0];
};
typedef struct SMemSkipList {
  uint32_t          seed;
  int64_t           size;
  int8_t            maxLevel;
  int8_t            level;
  SMemSkipListNode *pHead;
  SMemSkipListNode *pTail;
} SMemSkipList;

struct STbData {
  tb_uid_t     suid;
  tb_uid_t     uid;
  TSDBKEY      minKey;
  TSDBKEY      maxKey;
  SDelOp      *pHead;
  SDelOp      *pTail;
  SMemSkipList sl;
};

struct SMemTable {
  SRWLatch latch;
  STsdb   *pTsdb;
  int32_t  nRef;
  TSDBKEY  minKey;
  TSDBKEY  maxKey;
  int64_t  nRow;
  int64_t  nDelOp;
  SArray  *aTbData;  // SArray<STbData>
};

struct STsdbFSMeta {
  uint32_t version;       // Commit version from 0 to increase
  int64_t  totalPoints;   // total points
  int64_t  totalStorage;  // Uncompressed total storage
};

// ==================
typedef struct {
  STsdbFSMeta meta;       // FS meta
  SDFile      cacheFile;  // cache file
  SDFile      tombstone;  // tomestome file
  SArray     *df;         // data file array
  SArray     *sf;         // sma data file array    v2f1900.index_name_1
} SFSStatus;

struct STsdbFS {
  TdThreadRwlock lock;

  SFSStatus *cstatus;  // current status
  bool       intxn;
  SFSStatus *nstatus;  // new status
};

#define REPO_ID(r)        TD_VID((r)->pVnode)
#define REPO_CFG(r)       (&(r)->pVnode->config.tsdbCfg)
#define REPO_KEEP_CFG(r)  (&(r)->keepCfg)
#define REPO_FS(r)        ((r)->fs)
#define REPO_META(r)      ((r)->pVnode->pMeta)
#define REPO_TFS(r)       ((r)->pVnode->pTfs)
#define IS_REPO_LOCKED(r) ((r)->repoLocked)

int tsdbLockRepo(STsdb *pTsdb);
int tsdbUnlockRepo(STsdb *pTsdb);

static FORCE_INLINE STSchema *tsdbGetTableSchemaImpl(STsdb *pTsdb, STable *pTable, bool lock, bool copy,
                                                     int32_t version) {
  if ((version < 0) || (schemaVersion(pTable->pSchema) == version)) {
    return pTable->pSchema;
  }

  if (!pTable->pCacheSchema || (schemaVersion(pTable->pCacheSchema) != version)) {
    taosMemoryFreeClear(pTable->pCacheSchema);
    pTable->pCacheSchema = metaGetTbTSchema(REPO_META(pTsdb), pTable->uid, version);
  }
  return pTable->pCacheSchema;
}

// tsdbMemTable.h
struct SMergeInfo {
  int   rowsInserted;
  int   rowsUpdated;
  int   rowsDeleteSucceed;
  int   rowsDeleteFailed;
  int   nOperations;
  TSKEY keyFirst;
  TSKEY keyLast;
};

static void  *taosTMalloc(size_t size);
static void  *taosTCalloc(size_t nmemb, size_t size);
static void  *taosTRealloc(void *ptr, size_t size);
static void  *taosTZfree(void *ptr);
static size_t taosTSizeof(void *ptr);
static void   taosTMemset(void *ptr, int c);

struct TSDBROW {
  int64_t version;
  STSRow *pTSRow;
};

static FORCE_INLINE STSRow *tsdbNextIterRow(STbDataIter *pIter) {
  TSDBROW row;

  if (pIter == NULL) return NULL;

  if (tsdbTbDataIterGet(pIter, &row)) {
    return row.pTSRow;
  }

  return NULL;
}

static FORCE_INLINE TSKEY tsdbNextIterKey(STbDataIter *pIter) {
  STSRow *row = tsdbNextIterRow(pIter);
  if (row == NULL) return TSDB_DATA_TIMESTAMP_NULL;

  return TD_ROW_KEY(row);
}

// tsdbReadImpl
typedef struct SReadH SReadH;

typedef struct {
  uint64_t suid;
  uint64_t uid;
  uint32_t len;
  uint32_t offset;
  uint32_t hasLast : 2;
  uint32_t numOfBlocks : 30;
  TSDBKEY  maxKey;
} SBlockIdx;

typedef enum {
  TSDB_SBLK_VER_0 = 0,
  TSDB_SBLK_VER_MAX,
} ESBlockVer;

#define SBlockVerLatest TSDB_SBLK_VER_0

typedef struct {
  uint8_t  last : 1;
  uint8_t  hasDupKey : 1;  // 0: no dup TS key, 1: has dup TS key(since supporting Multi-Version)
  uint8_t  blkVer : 6;
  uint8_t  numOfSubBlocks;
  col_id_t numOfCols;    // not including timestamp column
  uint32_t len;          // data block length
  uint32_t keyLen : 20;  // key column length, keyOffset = offset+sizeof(SBlockData)+sizeof(SBlockCol)*numOfCols
  uint32_t algorithm : 4;
  uint32_t reserve : 8;
  col_id_t numOfBSma;
  uint16_t numOfRows;
  int64_t  offset;
  uint64_t aggrStat : 1;
  uint64_t aggrOffset : 63;
  TSDBKEY  minKey;
  TSDBKEY  maxKey;
} SBlock;

typedef struct {
  int32_t  delimiter;  // For recovery usage
  uint64_t suid;
  uint64_t uid;
  SBlock   blocks[];
} SBlockInfo;

typedef struct {
  int16_t  colId;
  uint16_t type : 6;
  uint16_t blen : 10;  // 0 no bitmap if all rows are NORM, > 0 bitmap length
  uint32_t len;        // data length + bitmap length
  uint32_t offset;
} SBlockCol;

typedef struct {
  int16_t colId;
  int16_t maxIndex;
  int16_t minIndex;
  int16_t numOfNull;
  int64_t sum;
  int64_t max;
  int64_t min;
} SAggrBlkCol;

typedef struct {
  int32_t   delimiter;  // For recovery usage
  int32_t   numOfCols;  // For recovery usage
  uint64_t  uid;        // For recovery usage
  SBlockCol cols[];
} SBlockData;

typedef void SAggrBlkData;  // SBlockCol cols[];

struct SReadH {
  STsdb        *pRepo;
  SDFileSet     rSet;     // FSET to read
  SArray       *aBlkIdx;  // SBlockIdx array
  STable       *pTable;   // table to read
  SBlockIdx    *pBlkIdx;  // current reading table SBlockIdx
  int           cidx;
  SBlockInfo   *pBlkInfo;
  SBlockData   *pBlkData;      // Block info
  SAggrBlkData *pAggrBlkData;  // Aggregate Block info
  SDataCols    *pDCols[2];
  void         *pBuf;    // buffer
  void         *pCBuf;   // compression buffer
  void         *pExBuf;  // extra buffer
};

#define TSDB_READ_REPO(rh)      ((rh)->pRepo)
#define TSDB_READ_REPO_ID(rh)   REPO_ID(TSDB_READ_REPO(rh))
#define TSDB_READ_FSET(rh)      (&((rh)->rSet))
#define TSDB_READ_TABLE(rh)     ((rh)->pTable)
#define TSDB_READ_TABLE_UID(rh) ((rh)->pTable->uid)
#define TSDB_READ_HEAD_FILE(rh) TSDB_DFILE_IN_SET(TSDB_READ_FSET(rh), TSDB_FILE_HEAD)
#define TSDB_READ_DATA_FILE(rh) TSDB_DFILE_IN_SET(TSDB_READ_FSET(rh), TSDB_FILE_DATA)
#define TSDB_READ_LAST_FILE(rh) TSDB_DFILE_IN_SET(TSDB_READ_FSET(rh), TSDB_FILE_LAST)
#define TSDB_READ_SMAD_FILE(rh) TSDB_DFILE_IN_SET(TSDB_READ_FSET(rh), TSDB_FILE_SMAD)
#define TSDB_READ_SMAL_FILE(rh) TSDB_DFILE_IN_SET(TSDB_READ_FSET(rh), TSDB_FILE_SMAL)
#define TSDB_READ_BUF(rh)       ((rh)->pBuf)
#define TSDB_READ_COMP_BUF(rh)  ((rh)->pCBuf)
#define TSDB_READ_EXBUF(rh)     ((rh)->pExBuf)

#define TSDB_BLOCK_STATIS_SIZE(ncols, blkVer) (sizeof(SBlockData) + sizeof(SBlockCol) * (ncols) + sizeof(TSCKSUM))

static FORCE_INLINE size_t tsdbBlockStatisSize(int nCols, uint32_t blkVer) {
  switch (blkVer) {
    case TSDB_SBLK_VER_0:
    default:
      return TSDB_BLOCK_STATIS_SIZE(nCols, 0);
  }
}

#define TSDB_BLOCK_AGGR_SIZE(ncols, blkVer) (sizeof(SAggrBlkCol) * (ncols) + sizeof(TSCKSUM))

static FORCE_INLINE size_t tsdbBlockAggrSize(int nCols, uint32_t blkVer) {
  switch (blkVer) {
    case TSDB_SBLK_VER_0:
    default:
      return TSDB_BLOCK_AGGR_SIZE(nCols, 0);
  }
}

int  tsdbInitReadH(SReadH *pReadh, STsdb *pRepo);
void tsdbDestroyReadH(SReadH *pReadh);
int  tsdbSetAndOpenReadFSet(SReadH *pReadh, SDFileSet *pSet);
void tsdbCloseAndUnsetFSet(SReadH *pReadh);
int  tsdbLoadBlockIdx(SReadH *pReadh);
int  tsdbSetReadTable(SReadH *pReadh, STable *pTable);
int  tsdbLoadBlockInfo(SReadH *pReadh, void *pTarget);
int  tsdbLoadBlockData(SReadH *pReadh, SBlock *pBlock, SBlockInfo *pBlockInfo);
int tsdbLoadBlockDataCols(SReadH *pReadh, SBlock *pBlock, SBlockInfo *pBlkInfo, const int16_t *colIds, int numOfColsIds,
                          bool mergeBitmap);
int tsdbLoadBlockStatis(SReadH *pReadh, SBlock *pBlock);
int tsdbEncodeSBlockIdx(void **buf, SBlockIdx *pIdx);
void *tsdbDecodeSBlockIdx(void *buf, SBlockIdx *pIdx);
void  tsdbGetBlockStatis(SReadH *pReadh, SColumnDataAgg *pStatis, int numOfCols, SBlock *pBlock);

static FORCE_INLINE int tsdbMakeRoom(void **ppBuf, size_t size) {
  void  *pBuf = *ppBuf;
  size_t tsize = taosTSizeof(pBuf);

  if (tsize < size) {
    if (tsize == 0) tsize = 1024;

    while (tsize < size) {
      tsize *= 2;
    }

    *ppBuf = taosTRealloc(pBuf, tsize);
    if (*ppBuf == NULL) {
      terrno = TSDB_CODE_TDB_OUT_OF_MEMORY;
      return -1;
    }
  }

  return 0;
}

// tsdbMemory
static FORCE_INLINE void *taosTMalloc(size_t size) {
  if (size <= 0) return NULL;

  void *ret = taosMemoryMalloc(size + sizeof(size_t));
  if (ret == NULL) return NULL;

  *(size_t *)ret = size;

  return (void *)((char *)ret + sizeof(size_t));
}

static FORCE_INLINE void *taosTCalloc(size_t nmemb, size_t size) {
  size_t tsize = nmemb * size;
  void  *ret = taosTMalloc(tsize);
  if (ret == NULL) return NULL;

  taosTMemset(ret, 0);
  return ret;
}

static FORCE_INLINE size_t taosTSizeof(void *ptr) { return (ptr) ? (*(size_t *)((char *)ptr - sizeof(size_t))) : 0; }

static FORCE_INLINE void taosTMemset(void *ptr, int c) { memset(ptr, c, taosTSizeof(ptr)); }

static FORCE_INLINE void *taosTRealloc(void *ptr, size_t size) {
  if (ptr == NULL) return taosTMalloc(size);

  if (size <= taosTSizeof(ptr)) return ptr;

  void  *tptr = (void *)((char *)ptr - sizeof(size_t));
  size_t tsize = size + sizeof(size_t);
  void  *tptr1 = taosMemoryRealloc(tptr, tsize);
  if (tptr1 == NULL) return NULL;
  tptr = tptr1;

  *(size_t *)tptr = size;

  return (void *)((char *)tptr + sizeof(size_t));
}

static FORCE_INLINE void *taosTZfree(void *ptr) {
  if (ptr) {
    taosMemoryFree((void *)((char *)ptr - sizeof(size_t)));
  }
  return NULL;
}

// tsdbCommit

void tsdbGetRtnSnap(STsdb *pRepo, SRtn *pRtn);

static FORCE_INLINE int TSDB_KEY_FID(TSKEY key, int32_t minutes, int8_t precision) {
  if (key < 0) {
    return (int)((key + 1) / tsTickPerMin[precision] / minutes - 1);
  } else {
    return (int)((key / tsTickPerMin[precision] / minutes));
  }
}

static FORCE_INLINE int tsdbGetFidLevel(int fid, SRtn *pRtn) {
  if (fid >= pRtn->maxFid) {
    return 0;
  } else if (fid >= pRtn->midFid) {
    return 1;
  } else if (fid >= pRtn->minFid) {
    return 2;
  } else {
    return -1;
  }
}

// tsdbFile
#define TSDB_FILE_HEAD_SIZE  512
#define TSDB_FILE_DELIMITER  0xF00AFA0F
#define TSDB_FILE_INIT_MAGIC 0xFFFFFFFF
#define TSDB_IVLD_FID        INT_MIN
#define TSDB_FILE_STATE_OK   0
#define TSDB_FILE_STATE_BAD  1

#define TSDB_FILE_F(tf)            (&((tf)->f))
#define TSDB_FILE_PFILE(tf)        ((tf)->pFile)
#define TSDB_FILE_FULL_NAME(tf)    (TSDB_FILE_F(tf)->aname)
#define TSDB_FILE_OPENED(tf)       (TSDB_FILE_PFILE(tf) != NULL)
#define TSDB_FILE_CLOSED(tf)       (!TSDB_FILE_OPENED(tf))
#define TSDB_FILE_SET_CLOSED(f)    (TSDB_FILE_PFILE(f) = NULL)
#define TSDB_FILE_LEVEL(tf)        (TSDB_FILE_F(tf)->did.level)
#define TSDB_FILE_ID(tf)           (TSDB_FILE_F(tf)->did.id)
#define TSDB_FILE_DID(tf)          (TSDB_FILE_F(tf)->did)
#define TSDB_FILE_REL_NAME(tf)     (TSDB_FILE_F(tf)->rname)
#define TSDB_FILE_ABS_NAME(tf)     (TSDB_FILE_F(tf)->aname)
#define TSDB_FILE_FSYNC(tf)        taosFsyncFile(TSDB_FILE_PFILE(tf))
#define TSDB_FILE_STATE(tf)        ((tf)->state)
#define TSDB_FILE_SET_STATE(tf, s) ((tf)->state = (s))
#define TSDB_FILE_IS_OK(tf)        (TSDB_FILE_STATE(tf) == TSDB_FILE_STATE_OK)
#define TSDB_FILE_IS_BAD(tf)       (TSDB_FILE_STATE(tf) == TSDB_FILE_STATE_BAD)

typedef enum {
  TSDB_FS_VER_0 = 0,
  TSDB_FS_VER_MAX,
} ETsdbFsVer;

#define TSDB_LATEST_FVER    TSDB_FS_VER_0  // latest version for DFile
#define TSDB_LATEST_SFS_VER TSDB_FS_VER_0  // latest version for 'current' file

static FORCE_INLINE uint32_t tsdbGetDFSVersion(TSDB_FILE_T fType) {  // latest version for DFile
  switch (fType) {
    case TSDB_FILE_HEAD:  // .head
    case TSDB_FILE_DATA:  // .data
    case TSDB_FILE_LAST:  // .last
    case TSDB_FILE_SMAD:  // .smad(Block-wise SMA)
    case TSDB_FILE_SMAL:  // .smal(Block-wise SMA)
    default:
      return TSDB_LATEST_FVER;
  }
}

// =============== SDFileSet

#define TSDB_LATEST_FSET_VER    0
#define TSDB_FSET_FID(s)        ((s)->fid)
#define TSDB_FSET_STATE(s)      ((s)->state)
#define TSDB_FSET_VER(s)        ((s)->ver)
#define TSDB_DFILE_IN_SET(s, t) ((s)->files + (t))
#define TSDB_FSET_LEVEL(s)      TSDB_FILE_LEVEL(TSDB_DFILE_IN_SET(s, 0))
#define TSDB_FSET_ID(s)         TSDB_FILE_ID(TSDB_DFILE_IN_SET(s, 0))
#define TSDB_FSET_SET_CLOSED(s)                                                \
  do {                                                                         \
    for (TSDB_FILE_T ftype = TSDB_FILE_HEAD; ftype < TSDB_FILE_MAX; ftype++) { \
      TSDB_FILE_SET_CLOSED(TSDB_DFILE_IN_SET(s, ftype));                       \
    }                                                                          \
  } while (0);
#define TSDB_FSET_FSYNC(s)                                                     \
  do {                                                                         \
    for (TSDB_FILE_T ftype = TSDB_FILE_HEAD; ftype < TSDB_FILE_MAX; ftype++) { \
      TSDB_FILE_FSYNC(TSDB_DFILE_IN_SET(s, ftype));                            \
    }                                                                          \
  } while (0);

static FORCE_INLINE bool tsdbFSetIsOk(SDFileSet *pSet) {
  for (TSDB_FILE_T ftype = 0; ftype < TSDB_FILE_MAX; ftype++) {
    if (TSDB_FILE_IS_BAD(TSDB_DFILE_IN_SET(pSet, ftype))) {
      return false;
    }
  }

  return true;
}

// tsdbFS
// ================== TSDB global config
extern bool tsdbForceKeepFile;

// ================== CURRENT file header info
typedef struct {
  uint32_t version;  // Current file system version (relating to code)
  uint32_t len;      // Encode content length (including checksum)
} SFSHeader;

// ================== TSDB File System Meta
#define FS_CURRENT_STATUS(pfs) ((pfs)->cstatus)
#define FS_NEW_STATUS(pfs)     ((pfs)->nstatus)
#define FS_IN_TXN(pfs)         (pfs)->intxn
#define FS_VERSION(pfs)        ((pfs)->cstatus->meta.version)
#define FS_TXN_VERSION(pfs)    ((pfs)->nstatus->meta.version)

struct SFSIter {
  int        direction;
  uint64_t   version;  // current FS version
  STsdbFS   *pfs;
  int        index;  // used to position next fset when version the same
  int        fid;    // used to seek when version is changed
  SDFileSet *pSet;
};

#define TSDB_FS_ITER_FORWARD  TSDB_ORDER_ASC
#define TSDB_FS_ITER_BACKWARD TSDB_ORDER_DESC

struct TABLEID {
  tb_uid_t suid;
  tb_uid_t uid;
};

struct SDelOp {
  int64_t version;
  TSKEY   sKey;  // included
  TSKEY   eKey;  // included
  SDelOp *pNext;
};

typedef struct {
  tb_uid_t suid;
  tb_uid_t uid;
  int64_t  version;
  TSKEY    sKey;
  TSKEY    eKey;
} SDelInfo;

struct SMemTable2 {
  STsdb  *pTsdb;
  int32_t nRef;
  TSDBKEY minKey;
  TSDBKEY maxKey;
  int64_t nRows;
  int64_t nDelOp;
  SArray *aSkmInfo;
  SArray *aMemData;
};

static FORCE_INLINE int tsdbKeyCmprFn(const void *p1, const void *p2) {
  TSDBKEY *pKey1 = (TSDBKEY *)p1;
  TSDBKEY *pKey2 = (TSDBKEY *)p2;

  if (pKey1->ts < pKey2->ts) {
    return -1;
  } else if (pKey1->ts > pKey2->ts) {
    return 1;
  }

  if (pKey1->version < pKey2->version) {
    return -1;
  } else if (pKey1->version > pKey2->version) {
    return 1;
  }

  return 0;
}

struct SMemData {
  tb_uid_t     suid;
  tb_uid_t     uid;
  TSDBKEY      minKey;
  TSDBKEY      maxKey;
  SDelOp      *delOpHead;
  SDelOp      *delOpTail;
  SMemSkipList sl;
};

struct SMemDataIter {
  STbData          *pMemData;
  int8_t            backward;
  TSDBROW          *pRow;
  SMemSkipListNode *pNode;  // current node
  TSDBROW           row;
};

struct STbDataIter {
  STbData          *pTbData;
  int8_t            backward;
  SMemSkipListNode *pNode;
};

#endif

#ifdef __cplusplus
}
#endif

#endif /*_TD_VNODE_TSDB_H_*/