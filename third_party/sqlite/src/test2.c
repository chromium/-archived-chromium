/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Code for testing the pager.c module in SQLite.  This code
** is not included in the SQLite library.  It is used for automated
** testing of the SQLite library.
**
** $Id: test2.c,v 1.59 2008/07/12 14:52:20 drh Exp $
*/
#include "sqliteInt.h"
#include "tcl.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
** Interpret an SQLite error number
*/
static char *errorName(int rc){
  char *zName;
  switch( rc ){
    case SQLITE_OK:         zName = "SQLITE_OK";          break;
    case SQLITE_ERROR:      zName = "SQLITE_ERROR";       break;
    case SQLITE_PERM:       zName = "SQLITE_PERM";        break;
    case SQLITE_ABORT:      zName = "SQLITE_ABORT";       break;
    case SQLITE_BUSY:       zName = "SQLITE_BUSY";        break;
    case SQLITE_NOMEM:      zName = "SQLITE_NOMEM";       break;
    case SQLITE_READONLY:   zName = "SQLITE_READONLY";    break;
    case SQLITE_INTERRUPT:  zName = "SQLITE_INTERRUPT";   break;
    case SQLITE_IOERR:      zName = "SQLITE_IOERR";       break;
    case SQLITE_CORRUPT:    zName = "SQLITE_CORRUPT";     break;
    case SQLITE_FULL:       zName = "SQLITE_FULL";        break;
    case SQLITE_CANTOPEN:   zName = "SQLITE_CANTOPEN";    break;
    case SQLITE_PROTOCOL:   zName = "SQLITE_PROTOCOL";    break;
    case SQLITE_EMPTY:      zName = "SQLITE_EMPTY";       break;
    case SQLITE_SCHEMA:     zName = "SQLITE_SCHEMA";      break;
    case SQLITE_CONSTRAINT: zName = "SQLITE_CONSTRAINT";  break;
    case SQLITE_MISMATCH:   zName = "SQLITE_MISMATCH";    break;
    case SQLITE_MISUSE:     zName = "SQLITE_MISUSE";      break;
    case SQLITE_NOLFS:      zName = "SQLITE_NOLFS";       break;
    default:                zName = "SQLITE_Unknown";     break;
  }
  return zName;
}

/*
** Page size and reserved size used for testing.
*/
static int test_pagesize = 1024;

/*
** Usage:   pager_open FILENAME N-PAGE
**
** Open a new pager
*/
static int pager_open(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  u16 pageSize;
  Pager *pPager;
  int nPage;
  int rc;
  char zBuf[100];
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FILENAME N-PAGE\"", 0);
    return TCL_ERROR;
  }
  if( Tcl_GetInt(interp, argv[2], &nPage) ) return TCL_ERROR;
  rc = sqlite3PagerOpen(sqlite3_vfs_find(0), &pPager, argv[1], 0, 0,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MAIN_DB);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  sqlite3PagerSetCachesize(pPager, nPage);
  pageSize = test_pagesize;
  sqlite3PagerSetPagesize(pPager, &pageSize);
  sqlite3_snprintf(sizeof(zBuf),zBuf,"%p",pPager);
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:   pager_close ID
**
** Close the given pager.
*/
static int pager_close(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerClose(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_rollback ID
**
** Rollback changes
*/
static int pager_rollback(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerRollback(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_commit ID
**
** Commit all changes
*/
static int pager_commit(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerCommitPhaseOne(pPager, 0, 0, 0);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  rc = sqlite3PagerCommitPhaseTwo(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_stmt_begin ID
**
** Start a new checkpoint.
*/
static int pager_stmt_begin(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerStmtBegin(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_stmt_rollback ID
**
** Rollback changes to a checkpoint
*/
static int pager_stmt_rollback(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerStmtRollback(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_stmt_commit ID
**
** Commit changes to a checkpoint
*/
static int pager_stmt_commit(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerStmtCommit(pPager);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   pager_stats ID
**
** Return pager statistics.
*/
static int pager_stats(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int i, *a;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  a = sqlite3PagerStats(pPager);
  for(i=0; i<9; i++){
    static char *zName[] = {
      "ref", "page", "max", "size", "state", "err",
      "hit", "miss", "ovfl",
    };
    char zBuf[100];
    Tcl_AppendElement(interp, zName[i]);
    sqlite3_snprintf(sizeof(zBuf),zBuf,"%d",a[i]);
    Tcl_AppendElement(interp, zBuf);
  }
  return TCL_OK;
}

/*
** Usage:   pager_pagecount ID
**
** Return the size of the database file.
*/
static int pager_pagecount(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  char zBuf[100];
  int nPage;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  sqlite3PagerPagecount(pPager, &nPage);
  sqlite3_snprintf(sizeof(zBuf), zBuf, "%d", nPage);
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:   page_get ID PGNO
**
** Return a pointer to a page from the database.
*/
static int page_get(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  char zBuf[100];
  DbPage *pPage;
  int pgno;
  int rc;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID PGNO\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  if( Tcl_GetInt(interp, argv[2], &pgno) ) return TCL_ERROR;
  rc = sqlite3PagerGet(pPager, pgno, &pPage);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  sqlite3_snprintf(sizeof(zBuf),zBuf,"%p",pPage);
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:   page_lookup ID PGNO
**
** Return a pointer to a page if the page is already in cache.
** If not in cache, return an empty string.
*/
static int page_lookup(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  char zBuf[100];
  DbPage *pPage;
  int pgno;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID PGNO\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  if( Tcl_GetInt(interp, argv[2], &pgno) ) return TCL_ERROR;
  pPage = sqlite3PagerLookup(pPager, pgno);
  if( pPage ){
    sqlite3_snprintf(sizeof(zBuf),zBuf,"%p",pPage);
    Tcl_AppendResult(interp, zBuf, 0);
  }
  return TCL_OK;
}

/*
** Usage:   pager_truncate ID PGNO
*/
static int pager_truncate(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  Pager *pPager;
  int rc;
  int pgno;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " ID PGNO\"", 0);
    return TCL_ERROR;
  }
  pPager = sqlite3TestTextToPtr(argv[1]);
  if( Tcl_GetInt(interp, argv[2], &pgno) ) return TCL_ERROR;
  rc = sqlite3PagerTruncate(pPager, pgno);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}


/*
** Usage:   page_unref PAGE
**
** Drop a pointer to a page.
*/
static int page_unref(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  DbPage *pPage;
  int rc;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " PAGE\"", 0);
    return TCL_ERROR;
  }
  pPage = (DbPage *)sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerUnref(pPage);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
** Usage:   page_read PAGE
**
** Return the content of a page
*/
static int page_read(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  char zBuf[100];
  DbPage *pPage;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " PAGE\"", 0);
    return TCL_ERROR;
  }
  pPage = sqlite3TestTextToPtr(argv[1]);
  memcpy(zBuf, sqlite3PagerGetData(pPage), sizeof(zBuf));
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:   page_number PAGE
**
** Return the page number for a page.
*/
static int page_number(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  char zBuf[100];
  DbPage *pPage;
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " PAGE\"", 0);
    return TCL_ERROR;
  }
  pPage = (DbPage *)sqlite3TestTextToPtr(argv[1]);
  sqlite3_snprintf(sizeof(zBuf), zBuf, "%d", sqlite3PagerPagenumber(pPage));
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:   page_write PAGE DATA
**
** Write something into a page.
*/
static int page_write(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  DbPage *pPage;
  char *pData;
  int rc;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " PAGE DATA\"", 0);
    return TCL_ERROR;
  }
  pPage = (DbPage *)sqlite3TestTextToPtr(argv[1]);
  rc = sqlite3PagerWrite(pPage);
  if( rc!=SQLITE_OK ){
    Tcl_AppendResult(interp, errorName(rc), 0);
    return TCL_ERROR;
  }
  pData = sqlite3PagerGetData(pPage);
  strncpy(pData, argv[2], test_pagesize-1);
  pData[test_pagesize-1] = 0;
  return TCL_OK;
}

#ifndef SQLITE_OMIT_DISKIO
/*
** Usage:   fake_big_file  N  FILENAME
**
** Write a few bytes at the N megabyte point of FILENAME.  This will
** create a large file.  If the file was a valid SQLite database, then
** the next time the database is opened, SQLite will begin allocating
** new pages after N.  If N is 2096 or bigger, this will test the
** ability of SQLite to write to large files.
*/
static int fake_big_file(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  sqlite3_vfs *pVfs;
  sqlite3_file *fd = 0;
  int rc;
  int n;
  i64 offset;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " N-MEGABYTES FILE\"", 0);
    return TCL_ERROR;
  }
  if( Tcl_GetInt(interp, argv[1], &n) ) return TCL_ERROR;

  pVfs = sqlite3_vfs_find(0);
  rc = sqlite3OsOpenMalloc(pVfs, argv[2], &fd, 
      (SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE|SQLITE_OPEN_MAIN_DB), 0
  );
  if( rc ){
    Tcl_AppendResult(interp, "open failed: ", errorName(rc), 0);
    return TCL_ERROR;
  }
  offset = n;
  offset *= 1024*1024;
  rc = sqlite3OsWrite(fd, "Hello, World!", 14, offset);
  sqlite3OsCloseFree(fd);
  if( rc ){
    Tcl_AppendResult(interp, "write failed: ", errorName(rc), 0);
    return TCL_ERROR;
  }
  return TCL_OK;
}
#endif


/*
** sqlite3BitvecBuiltinTest SIZE PROGRAM
**
** Invoke the SQLITE_TESTCTRL_BITVEC_TEST operator on test_control.
** See comments on sqlite3BitvecBuiltinTest() for additional information.
*/
static int testBitvecBuiltinTest(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  const char **argv      /* Text of each argument */
){
  int sz, rc;
  int nProg = 0;
  int aProg[100];
  const char *z;
  if( argc!=3 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                     " SIZE PROGRAM\"", (void*)0);
  }
  if( Tcl_GetInt(interp, argv[1], &sz) ) return TCL_ERROR;
  z = argv[2];
  while( nProg<99 && *z ){
    while( *z && !isdigit(*z) ){ z++; }
    if( *z==0 ) break;
    aProg[nProg++] = atoi(z);
    while( isdigit(*z) ){ z++; }
  }
  aProg[nProg] = 0;
  rc = sqlite3_test_control(SQLITE_TESTCTRL_BITVEC_TEST, sz, aProg);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(rc));
  return TCL_OK;
}  

/*
** Register commands with the TCL interpreter.
*/
int Sqlitetest2_Init(Tcl_Interp *interp){
  extern int sqlite3_io_error_persist;
  extern int sqlite3_io_error_pending;
  extern int sqlite3_io_error_hit;
  extern int sqlite3_io_error_hardhit;
  extern int sqlite3_diskfull_pending;
  extern int sqlite3_diskfull;
  extern int sqlite3_pager_n_sort_bucket;
  static struct {
    char *zName;
    Tcl_CmdProc *xProc;
  } aCmd[] = {
    { "pager_open",              (Tcl_CmdProc*)pager_open          },
    { "pager_close",             (Tcl_CmdProc*)pager_close         },
    { "pager_commit",            (Tcl_CmdProc*)pager_commit        },
    { "pager_rollback",          (Tcl_CmdProc*)pager_rollback      },
    { "pager_stmt_begin",        (Tcl_CmdProc*)pager_stmt_begin    },
    { "pager_stmt_commit",       (Tcl_CmdProc*)pager_stmt_commit   },
    { "pager_stmt_rollback",     (Tcl_CmdProc*)pager_stmt_rollback },
    { "pager_stats",             (Tcl_CmdProc*)pager_stats         },
    { "pager_pagecount",         (Tcl_CmdProc*)pager_pagecount     },
    { "page_get",                (Tcl_CmdProc*)page_get            },
    { "page_lookup",             (Tcl_CmdProc*)page_lookup         },
    { "page_unref",              (Tcl_CmdProc*)page_unref          },
    { "page_read",               (Tcl_CmdProc*)page_read           },
    { "page_write",              (Tcl_CmdProc*)page_write          },
    { "page_number",             (Tcl_CmdProc*)page_number         },
    { "pager_truncate",          (Tcl_CmdProc*)pager_truncate      },
#ifndef SQLITE_OMIT_DISKIO
    { "fake_big_file",           (Tcl_CmdProc*)fake_big_file       },
#endif
    { "sqlite3BitvecBuiltinTest",(Tcl_CmdProc*)testBitvecBuiltinTest},
  };
  int i;
  for(i=0; i<sizeof(aCmd)/sizeof(aCmd[0]); i++){
    Tcl_CreateCommand(interp, aCmd[i].zName, aCmd[i].xProc, 0, 0);
  }
  Tcl_LinkVar(interp, "sqlite_io_error_pending",
     (char*)&sqlite3_io_error_pending, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_io_error_persist",
     (char*)&sqlite3_io_error_persist, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_io_error_hit",
     (char*)&sqlite3_io_error_hit, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_io_error_hardhit",
     (char*)&sqlite3_io_error_hardhit, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_diskfull_pending",
     (char*)&sqlite3_diskfull_pending, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_diskfull",
     (char*)&sqlite3_diskfull, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_pending_byte",
     (char*)&sqlite3_pending_byte, TCL_LINK_INT);
  Tcl_LinkVar(interp, "sqlite_pager_n_sort_bucket",
     (char*)&sqlite3_pager_n_sort_bucket, TCL_LINK_INT);
  return TCL_OK;
}
