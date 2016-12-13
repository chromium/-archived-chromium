/*
** 2008 Jan 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains code that modified the OS layer in order to simulate
** different device types (by overriding the return values of the 
** xDeviceCharacteristics() and xSectorSize() methods).
**
** $Id: test_devsym.c,v 1.7 2008/06/06 11:11:26 danielk1977 Exp $
*/
#if SQLITE_TEST          /* This file is used for testing only */

#include "sqlite3.h"
#include "sqliteInt.h"

/*
** Maximum pathname length supported by the devsym backend.
*/
#define DEVSYM_MAX_PATHNAME 512

/*
** Name used to identify this VFS.
*/
#define DEVSYM_VFS_NAME "devsym"

typedef struct devsym_file devsym_file;
struct devsym_file {
  sqlite3_file base;
  sqlite3_file *pReal;
};

/*
** Method declarations for devsym_file.
*/
static int devsymClose(sqlite3_file*);
static int devsymRead(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int devsymWrite(sqlite3_file*,const void*,int iAmt, sqlite3_int64 iOfst);
static int devsymTruncate(sqlite3_file*, sqlite3_int64 size);
static int devsymSync(sqlite3_file*, int flags);
static int devsymFileSize(sqlite3_file*, sqlite3_int64 *pSize);
static int devsymLock(sqlite3_file*, int);
static int devsymUnlock(sqlite3_file*, int);
static int devsymCheckReservedLock(sqlite3_file*, int *);
static int devsymFileControl(sqlite3_file*, int op, void *pArg);
static int devsymSectorSize(sqlite3_file*);
static int devsymDeviceCharacteristics(sqlite3_file*);

/*
** Method declarations for devsym_vfs.
*/
static int devsymOpen(sqlite3_vfs*, const char *, sqlite3_file*, int , int *);
static int devsymDelete(sqlite3_vfs*, const char *zName, int syncDir);
static int devsymAccess(sqlite3_vfs*, const char *zName, int flags, int *);
static int devsymFullPathname(sqlite3_vfs*, const char *zName, int, char *zOut);
#ifndef SQLITE_OMIT_LOAD_EXTENSION
static void *devsymDlOpen(sqlite3_vfs*, const char *zFilename);
static void devsymDlError(sqlite3_vfs*, int nByte, char *zErrMsg);
static void *devsymDlSym(sqlite3_vfs*,void*, const char *zSymbol);
static void devsymDlClose(sqlite3_vfs*, void*);
#endif /* SQLITE_OMIT_LOAD_EXTENSION */
static int devsymRandomness(sqlite3_vfs*, int nByte, char *zOut);
static int devsymSleep(sqlite3_vfs*, int microseconds);
static int devsymCurrentTime(sqlite3_vfs*, double*);

static sqlite3_vfs devsym_vfs = {
  1,                     /* iVersion */
  sizeof(devsym_file),      /* szOsFile */
  DEVSYM_MAX_PATHNAME,      /* mxPathname */
  0,                     /* pNext */
  DEVSYM_VFS_NAME,          /* zName */
  0,                     /* pAppData */
  devsymOpen,               /* xOpen */
  devsymDelete,             /* xDelete */
  devsymAccess,             /* xAccess */
  devsymFullPathname,       /* xFullPathname */
#ifndef SQLITE_OMIT_LOAD_EXTENSION
  devsymDlOpen,             /* xDlOpen */
  devsymDlError,            /* xDlError */
  devsymDlSym,              /* xDlSym */
  devsymDlClose,            /* xDlClose */
#else
  0,                        /* xDlOpen */
  0,                        /* xDlError */
  0,                        /* xDlSym */
  0,                        /* xDlClose */
#endif /* SQLITE_OMIT_LOAD_EXTENSION */
  devsymRandomness,         /* xRandomness */
  devsymSleep,              /* xSleep */
  devsymCurrentTime         /* xCurrentTime */
};

static sqlite3_io_methods devsym_io_methods = {
  1,                            /* iVersion */
  devsymClose,                      /* xClose */
  devsymRead,                       /* xRead */
  devsymWrite,                      /* xWrite */
  devsymTruncate,                   /* xTruncate */
  devsymSync,                       /* xSync */
  devsymFileSize,                   /* xFileSize */
  devsymLock,                       /* xLock */
  devsymUnlock,                     /* xUnlock */
  devsymCheckReservedLock,          /* xCheckReservedLock */
  devsymFileControl,                /* xFileControl */
  devsymSectorSize,                 /* xSectorSize */
  devsymDeviceCharacteristics       /* xDeviceCharacteristics */
};

struct DevsymGlobal {
  sqlite3_vfs *pVfs;
  int iDeviceChar;
  int iSectorSize;
};
struct DevsymGlobal g = {0, 0, 512};

/*
** Close an devsym-file.
*/
static int devsymClose(sqlite3_file *pFile){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsClose(p->pReal);
}

/*
** Read data from an devsym-file.
*/
static int devsymRead(
  sqlite3_file *pFile, 
  void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsRead(p->pReal, zBuf, iAmt, iOfst);
}

/*
** Write data to an devsym-file.
*/
static int devsymWrite(
  sqlite3_file *pFile, 
  const void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsWrite(p->pReal, zBuf, iAmt, iOfst);
}

/*
** Truncate an devsym-file.
*/
static int devsymTruncate(sqlite3_file *pFile, sqlite_int64 size){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsTruncate(p->pReal, size);
}

/*
** Sync an devsym-file.
*/
static int devsymSync(sqlite3_file *pFile, int flags){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsSync(p->pReal, flags);
}

/*
** Return the current file-size of an devsym-file.
*/
static int devsymFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsFileSize(p->pReal, pSize);
}

/*
** Lock an devsym-file.
*/
static int devsymLock(sqlite3_file *pFile, int eLock){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsLock(p->pReal, eLock);
}

/*
** Unlock an devsym-file.
*/
static int devsymUnlock(sqlite3_file *pFile, int eLock){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsUnlock(p->pReal, eLock);
}

/*
** Check if another file-handle holds a RESERVED lock on an devsym-file.
*/
static int devsymCheckReservedLock(sqlite3_file *pFile, int *pResOut){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsCheckReservedLock(p->pReal, pResOut);
}

/*
** File control method. For custom operations on an devsym-file.
*/
static int devsymFileControl(sqlite3_file *pFile, int op, void *pArg){
  devsym_file *p = (devsym_file *)pFile;
  return sqlite3OsFileControl(p->pReal, op, pArg);
}

/*
** Return the sector-size in bytes for an devsym-file.
*/
static int devsymSectorSize(sqlite3_file *pFile){
  return g.iSectorSize;
}

/*
** Return the device characteristic flags supported by an devsym-file.
*/
static int devsymDeviceCharacteristics(sqlite3_file *pFile){
  return g.iDeviceChar;
}

/*
** Open an devsym file handle.
*/
static int devsymOpen(
  sqlite3_vfs *pVfs,
  const char *zName,
  sqlite3_file *pFile,
  int flags,
  int *pOutFlags
){
  devsym_file *p = (devsym_file *)pFile;
  pFile->pMethods = &devsym_io_methods;
  p->pReal = (sqlite3_file *)&p[1];
  return sqlite3OsOpen(g.pVfs, zName, p->pReal, flags, pOutFlags);
}

/*
** Delete the file located at zPath. If the dirSync argument is true,
** ensure the file-system modifications are synced to disk before
** returning.
*/
static int devsymDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  return sqlite3OsDelete(g.pVfs, zPath, dirSync);
}

/*
** Test for access permissions. Return true if the requested permission
** is available, or false otherwise.
*/
static int devsymAccess(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int flags, 
  int *pResOut
){
  return sqlite3OsAccess(g.pVfs, zPath, flags, pResOut);
}

/*
** Populate buffer zOut with the full canonical pathname corresponding
** to the pathname in zPath. zOut is guaranteed to point to a buffer
** of at least (DEVSYM_MAX_PATHNAME+1) bytes.
*/
static int devsymFullPathname(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int nOut, 
  char *zOut
){
  return sqlite3OsFullPathname(g.pVfs, zPath, nOut, zOut);
}

#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** Open the dynamic library located at zPath and return a handle.
*/
static void *devsymDlOpen(sqlite3_vfs *pVfs, const char *zPath){
  return sqlite3OsDlOpen(g.pVfs, zPath);
}

/*
** Populate the buffer zErrMsg (size nByte bytes) with a human readable
** utf-8 string describing the most recent error encountered associated 
** with dynamic libraries.
*/
static void devsymDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
  sqlite3OsDlError(g.pVfs, nByte, zErrMsg);
}

/*
** Return a pointer to the symbol zSymbol in the dynamic library pHandle.
*/
static void *devsymDlSym(sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol){
  return sqlite3OsDlSym(g.pVfs, pHandle, zSymbol);
}

/*
** Close the dynamic library handle pHandle.
*/
static void devsymDlClose(sqlite3_vfs *pVfs, void *pHandle){
  sqlite3OsDlClose(g.pVfs, pHandle);
}
#endif /* SQLITE_OMIT_LOAD_EXTENSION */

/*
** Populate the buffer pointed to by zBufOut with nByte bytes of 
** random data.
*/
static int devsymRandomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut){
  return sqlite3OsRandomness(g.pVfs, nByte, zBufOut);
}

/*
** Sleep for nMicro microseconds. Return the number of microseconds 
** actually slept.
*/
static int devsymSleep(sqlite3_vfs *pVfs, int nMicro){
  return sqlite3OsSleep(g.pVfs, nMicro);
}

/*
** Return the current time as a Julian Day number in *pTimeOut.
*/
static int devsymCurrentTime(sqlite3_vfs *pVfs, double *pTimeOut){
  return sqlite3OsCurrentTime(g.pVfs, pTimeOut);
}

/*
** This procedure registers the devsym vfs with SQLite. If the argument is
** true, the devsym vfs becomes the new default vfs. It is the only publicly
** available function in this file.
*/
void devsym_register(int iDeviceChar, int iSectorSize){
  if( g.pVfs==0 ){
    g.pVfs = sqlite3_vfs_find(0);
    devsym_vfs.szOsFile += g.pVfs->szOsFile;
    sqlite3_vfs_register(&devsym_vfs, 0);
  }
  if( iDeviceChar>=0 ){
    g.iDeviceChar = iDeviceChar;
  }
  if( iSectorSize>=0 ){
    g.iSectorSize = iSectorSize;
  }
}

#endif
