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
** Main file for the SQLite library.  The routines in this file
** implement the programmer interface to the library.  Routines in
** other files are for internal use by SQLite and should not be
** accessed by users of the library.
**
** $Id: main.c,v 1.486 2008/08/04 20:13:27 drh Exp $
*/
#include "sqliteInt.h"
#include <ctype.h>

#ifdef SQLITE_ENABLE_FTS3
# include "fts3.h"
#endif
#ifdef SQLITE_ENABLE_RTREE
# include "rtree.h"
#endif

/*
** The version of the library
*/
const char sqlite3_version[] = SQLITE_VERSION;
const char *sqlite3_libversion(void){ return sqlite3_version; }
int sqlite3_libversion_number(void){ return SQLITE_VERSION_NUMBER; }
int sqlite3_threadsafe(void){ return SQLITE_THREADSAFE; }

#if !defined(SQLITE_OMIT_TRACE) && defined(SQLITE_ENABLE_IOTRACE)
/*
** If the following function pointer is not NULL and if
** SQLITE_ENABLE_IOTRACE is enabled, then messages describing
** I/O active are written using this function.  These messages
** are intended for debugging activity only.
*/
void (*sqlite3IoTrace)(const char*, ...) = 0;
#endif

/*
** If the following global variable points to a string which is the
** name of a directory, then that directory will be used to store
** temporary files.
**
** See also the "PRAGMA temp_store_directory" SQL command.
*/
char *sqlite3_temp_directory = 0;

/*
** Initialize SQLite.  
**
** This routine must be called to initialize the memory allocation,
** VFS, and mutex subsystesms prior to doing any serious work with
** SQLite.  But as long as you do not compile with SQLITE_OMIT_AUTOINIT
** this routine will be called automatically by key routines such as
** sqlite3_open().  
**
** This routine is a no-op except on its very first call for the process,
** or for the first call after a call to sqlite3_shutdown.
*/
int sqlite3_initialize(void){
  static int inProgress = 0;
  int rc;

  /* If SQLite is already initialized, this call is a no-op. */
  if( sqlite3Config.isInit ) return SQLITE_OK;

  /* Make sure the mutex system is initialized. */
  rc = sqlite3MutexInit();

  if( rc==SQLITE_OK ){

    /* Initialize the malloc() system and the recursive pInitMutex mutex.
    ** This operation is protected by the STATIC_MASTER mutex.
    */
    sqlite3_mutex *pMaster = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
    sqlite3_mutex_enter(pMaster);
    if( !sqlite3Config.isMallocInit ){
      rc = sqlite3MallocInit();
    }
    if( rc==SQLITE_OK ){
      sqlite3Config.isMallocInit = 1;
      if( !sqlite3Config.pInitMutex ){
        sqlite3Config.pInitMutex = sqlite3MutexAlloc(SQLITE_MUTEX_RECURSIVE);
        if( sqlite3Config.bCoreMutex && !sqlite3Config.pInitMutex ){
          rc = SQLITE_NOMEM;
        }
      }
    }
    sqlite3_mutex_leave(pMaster);
    if( rc!=SQLITE_OK ){
      return rc;
    }

    /* Enter the recursive pInitMutex mutex. After doing so, if the
    ** sqlite3Config.isInit flag is true, then some other thread has
    ** finished doing the initialization. If the inProgress flag is
    ** true, then this function is being called recursively from within
    ** the sqlite3_os_init() call below. In either case, exit early.
    */
    sqlite3_mutex_enter(sqlite3Config.pInitMutex);
    if( sqlite3Config.isInit || inProgress ){
      sqlite3_mutex_leave(sqlite3Config.pInitMutex);
      return SQLITE_OK;
    }
    sqlite3StatusReset();
    inProgress = 1;
    rc = sqlite3_os_init();
    inProgress = 0;
    sqlite3Config.isInit = (rc==SQLITE_OK ? 1 : 0);
    sqlite3_mutex_leave(sqlite3Config.pInitMutex);
  }

  /* Check NaN support. */
#ifndef NDEBUG
  /* This section of code's only "output" is via assert() statements. */
  if ( rc==SQLITE_OK ){
    u64 x = (((u64)1)<<63)-1;
    double y;
    assert(sizeof(x)==8);
    assert(sizeof(x)==sizeof(y));
    memcpy(&y, &x, 8);
    assert( sqlite3IsNaN(y) );
  }
#endif

  return rc;
}

/*
** Undo the effects of sqlite3_initialize().  Must not be called while
** there are outstanding database connections or memory allocations or
** while any part of SQLite is otherwise in use in any thread.  This
** routine is not threadsafe.  Not by a long shot.
*/
int sqlite3_shutdown(void){
  sqlite3_mutex_free(sqlite3Config.pInitMutex);
  sqlite3Config.pInitMutex = 0;
  sqlite3Config.isMallocInit = 0;
  if( sqlite3Config.isInit ){
    sqlite3_os_end();
  }
  if( sqlite3Config.m.xShutdown ){
    sqlite3MallocEnd();
  }
  if( sqlite3Config.mutex.xMutexEnd ){
    sqlite3MutexEnd();
  }
  sqlite3Config.isInit = 0;
  return SQLITE_OK;
}

/*
** This API allows applications to modify the global configuration of
** the SQLite library at run-time.
**
** This routine should only be called when there are no outstanding
** database connections or memory allocations.  This routine is not
** threadsafe.  Failure to heed these warnings can lead to unpredictable
** behavior.
*/
int sqlite3_config(int op, ...){
  va_list ap;
  int rc = SQLITE_OK;

  /* sqlite3_config() shall return SQLITE_MISUSE if it is invoked while
  ** the SQLite library is in use. */
  if( sqlite3Config.isInit ) return SQLITE_MISUSE;

  va_start(ap, op);
  switch( op ){
    case SQLITE_CONFIG_SINGLETHREAD: {
      /* Disable all mutexing */
      sqlite3Config.bCoreMutex = 0;
      sqlite3Config.bFullMutex = 0;
      break;
    }
    case SQLITE_CONFIG_MULTITHREAD: {
      /* Disable mutexing of database connections */
      /* Enable mutexing of core data structures */
      sqlite3Config.bCoreMutex = 1;
      sqlite3Config.bFullMutex = 0;
      break;
    }
    case SQLITE_CONFIG_SERIALIZED: {
      /* Enable all mutexing */
      sqlite3Config.bCoreMutex = 1;
      sqlite3Config.bFullMutex = 1;
      break;
    }
    case SQLITE_CONFIG_MALLOC: {
      /* Specify an alternative malloc implementation */
      sqlite3Config.m = *va_arg(ap, sqlite3_mem_methods*);
      break;
    }
    case SQLITE_CONFIG_GETMALLOC: {
      /* Retrieve the current malloc() implementation */
      if( sqlite3Config.m.xMalloc==0 ) sqlite3MemSetDefault();
      *va_arg(ap, sqlite3_mem_methods*) = sqlite3Config.m;
      break;
    }
    case SQLITE_CONFIG_MUTEX: {
      /* Specify an alternative mutex implementation */
      sqlite3Config.mutex = *va_arg(ap, sqlite3_mutex_methods*);
      break;
    }
    case SQLITE_CONFIG_GETMUTEX: {
      /* Retrieve the current mutex implementation */
      *va_arg(ap, sqlite3_mutex_methods*) = sqlite3Config.mutex;
      break;
    }
    case SQLITE_CONFIG_MEMSTATUS: {
      /* Enable or disable the malloc status collection */
      sqlite3Config.bMemstat = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_SCRATCH: {
      /* Designate a buffer for scratch memory space */
      sqlite3Config.pScratch = va_arg(ap, void*);
      sqlite3Config.szScratch = va_arg(ap, int);
      sqlite3Config.nScratch = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_PAGECACHE: {
      /* Designate a buffer for scratch memory space */
      sqlite3Config.pPage = va_arg(ap, void*);
      sqlite3Config.szPage = va_arg(ap, int);
      sqlite3Config.nPage = va_arg(ap, int);
      break;
    }

#if defined(SQLITE_ENABLE_MEMSYS3) || defined(SQLITE_ENABLE_MEMSYS5)
    case SQLITE_CONFIG_HEAP: {
      /* Designate a buffer for heap memory space */
      sqlite3Config.pHeap = va_arg(ap, void*);
      sqlite3Config.nHeap = va_arg(ap, int);
      sqlite3Config.mnReq = va_arg(ap, int);

      if( sqlite3Config.pHeap==0 ){
        /* If the heap pointer is NULL, then restore the malloc implementation
        ** back to NULL pointers too.  This will cause the malloc to go
        ** back to its default implementation when sqlite3_initialize() is
        ** run.
        */
        memset(&sqlite3Config.m, 0, sizeof(sqlite3Config.m));
      }else{
        /* The heap pointer is not NULL, then install one of the
        ** mem5.c/mem3.c methods. If neither ENABLE_MEMSYS3 nor
        ** ENABLE_MEMSYS5 is defined, return an error.
        ** the default case and return an error.
        */
#ifdef SQLITE_ENABLE_MEMSYS3
        sqlite3Config.m = *sqlite3MemGetMemsys3();
#endif
#ifdef SQLITE_ENABLE_MEMSYS5
        sqlite3Config.m = *sqlite3MemGetMemsys5();
#endif
      }
      break;
    }
#endif

#if defined(SQLITE_ENABLE_MEMSYS6)
    case SQLITE_CONFIG_CHUNKALLOC: {
      sqlite3Config.nSmall = va_arg(ap, int);
      sqlite3Config.m = *sqlite3MemGetMemsys6();
      break;
    }
#endif

    case SQLITE_CONFIG_LOOKASIDE: {
      sqlite3Config.szLookaside = va_arg(ap, int);
      sqlite3Config.nLookaside = va_arg(ap, int);
      break;
    }

    default: {
      rc = SQLITE_ERROR;
      break;
    }
  }
  va_end(ap);
  return rc;
}

/*
** Set up the lookaside buffers for a database connection.
** Return SQLITE_OK on success.  
** If lookaside is already active, return SQLITE_BUSY.
**
** The sz parameter is the number of bytes in each lookaside slot.
** The cnt parameter is the number of slots.  If pStart is NULL the
** space for the lookaside memory is obtained from sqlite3_malloc().
** If pStart is not NULL then it is sz*cnt bytes of memory to use for
** the lookaside memory.
*/
static int setupLookaside(sqlite3 *db, void *pBuf, int sz, int cnt){
  void *pStart;
  if( db->lookaside.nOut ){
    return SQLITE_BUSY;
  }
  if( sz<0 ) sz = 0;
  if( cnt<0 ) cnt = 0;
  sz = (sz+7)&~7;
  if( pBuf==0 ){
    sqlite3BeginBenignMalloc();
    pStart = sqlite3Malloc( sz*cnt );
    sqlite3EndBenignMalloc();
  }else{
    pStart = pBuf;
  }
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  db->lookaside.pStart = pStart;
  db->lookaside.pFree = 0;
  db->lookaside.sz = sz;
  db->lookaside.bMalloced = pBuf==0;
  if( pStart ){
    int i;
    LookasideSlot *p;
    p = (LookasideSlot*)pStart;
    for(i=cnt-1; i>=0; i--){
      p->pNext = db->lookaside.pFree;
      db->lookaside.pFree = p;
      p = (LookasideSlot*)&((u8*)p)[sz];
    }
    db->lookaside.pEnd = p;
    db->lookaside.bEnabled = 1;
  }else{
    db->lookaside.pEnd = 0;
    db->lookaside.bEnabled = 0;
  }
  return SQLITE_OK;
}

/*
** Configuration settings for an individual database connection
*/
int sqlite3_db_config(sqlite3 *db, int op, ...){
  va_list ap;
  int rc;
  va_start(ap, op);
  switch( op ){
    case SQLITE_DBCONFIG_LOOKASIDE: {
      void *pBuf = va_arg(ap, void*);
      int sz = va_arg(ap, int);
      int cnt = va_arg(ap, int);
      rc = setupLookaside(db, pBuf, sz, cnt);
      break;
    }
    default: {
      rc = SQLITE_ERROR;
      break;
    }
  }
  va_end(ap);
  return rc;
}

/*
** Routine needed to support the testcase() macro.
*/
#ifdef SQLITE_COVERAGE_TEST
void sqlite3Coverage(int x){
  static int dummy = 0;
  dummy += x;
}
#endif


/*
** Return true if the buffer z[0..n-1] contains all spaces.
*/
static int allSpaces(const char *z, int n){
  while( n>0 && z[n-1]==' ' ){ n--; }
  return n==0;
}

/*
** This is the default collating function named "BINARY" which is always
** available.
**
** If the padFlag argument is not NULL then space padding at the end
** of strings is ignored.  This implements the RTRIM collation.
*/
static int binCollFunc(
  void *padFlag,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int rc, n;
  n = nKey1<nKey2 ? nKey1 : nKey2;
  rc = memcmp(pKey1, pKey2, n);
  if( rc==0 ){
    if( padFlag
     && allSpaces(((char*)pKey1)+n, nKey1-n)
     && allSpaces(((char*)pKey2)+n, nKey2-n)
    ){
      /* Leave rc unchanged at 0 */
    }else{
      rc = nKey1 - nKey2;
    }
  }
  return rc;
}

/*
** Another built-in collating sequence: NOCASE. 
**
** This collating sequence is intended to be used for "case independant
** comparison". SQLite's knowledge of upper and lower case equivalents
** extends only to the 26 characters used in the English language.
**
** At the moment there is only a UTF-8 implementation.
*/
static int nocaseCollatingFunc(
  void *NotUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int r = sqlite3StrNICmp(
      (const char *)pKey1, (const char *)pKey2, (nKey1<nKey2)?nKey1:nKey2);
  if( 0==r ){
    r = nKey1-nKey2;
  }
  return r;
}

/*
** Return the ROWID of the most recent insert
*/
sqlite_int64 sqlite3_last_insert_rowid(sqlite3 *db){
  return db->lastRowid;
}

/*
** Return the number of changes in the most recent call to sqlite3_exec().
*/
int sqlite3_changes(sqlite3 *db){
  return db->nChange;
}

/*
** Return the number of changes since the database handle was opened.
*/
int sqlite3_total_changes(sqlite3 *db){
  return db->nTotalChange;
}

/*
** Close an existing SQLite database
*/
int sqlite3_close(sqlite3 *db){
  HashElem *i;
  int j;

  if( !db ){
    return SQLITE_OK;
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE;
  }
  sqlite3_mutex_enter(db->mutex);

#ifdef SQLITE_SSE
  {
    extern void sqlite3SseCleanup(sqlite3*);
    sqlite3SseCleanup(db);
  }
#endif 

  sqlite3ResetInternalSchema(db, 0);

  /* If a transaction is open, the ResetInternalSchema() call above
  ** will not have called the xDisconnect() method on any virtual
  ** tables in the db->aVTrans[] array. The following sqlite3VtabRollback()
  ** call will do so. We need to do this before the check for active
  ** SQL statements below, as the v-table implementation may be storing
  ** some prepared statements internally.
  */
  sqlite3VtabRollback(db);

  /* If there are any outstanding VMs, return SQLITE_BUSY. */
  if( db->pVdbe ){
    sqlite3Error(db, SQLITE_BUSY, 
        "Unable to close due to unfinalised statements");
    sqlite3_mutex_leave(db->mutex);
    return SQLITE_BUSY;
  }
  assert( sqlite3SafetyCheckSickOrOk(db) );

  for(j=0; j<db->nDb; j++){
    struct Db *pDb = &db->aDb[j];
    if( pDb->pBt ){
      sqlite3BtreeClose(pDb->pBt);
      pDb->pBt = 0;
      if( j!=1 ){
        pDb->pSchema = 0;
      }
    }
  }
  sqlite3ResetInternalSchema(db, 0);
  assert( db->nDb<=2 );
  assert( db->aDb==db->aDbStatic );
  for(i=sqliteHashFirst(&db->aFunc); i; i=sqliteHashNext(i)){
    FuncDef *pFunc, *pNext;
    for(pFunc = (FuncDef*)sqliteHashData(i); pFunc; pFunc=pNext){
      pNext = pFunc->pNext;
      sqlite3DbFree(db, pFunc);
    }
  }

  for(i=sqliteHashFirst(&db->aCollSeq); i; i=sqliteHashNext(i)){
    CollSeq *pColl = (CollSeq *)sqliteHashData(i);
    /* Invoke any destructors registered for collation sequence user data. */
    for(j=0; j<3; j++){
      if( pColl[j].xDel ){
        pColl[j].xDel(pColl[j].pUser);
      }
    }
    sqlite3DbFree(db, pColl);
  }
  sqlite3HashClear(&db->aCollSeq);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  for(i=sqliteHashFirst(&db->aModule); i; i=sqliteHashNext(i)){
    Module *pMod = (Module *)sqliteHashData(i);
    if( pMod->xDestroy ){
      pMod->xDestroy(pMod->pAux);
    }
    sqlite3DbFree(db, pMod);
  }
  sqlite3HashClear(&db->aModule);
#endif

  sqlite3HashClear(&db->aFunc);
  sqlite3Error(db, SQLITE_OK, 0); /* Deallocates any cached error strings. */
  if( db->pErr ){
    sqlite3ValueFree(db->pErr);
  }
  sqlite3CloseExtensions(db);

  db->magic = SQLITE_MAGIC_ERROR;

  /* The temp-database schema is allocated differently from the other schema
  ** objects (using sqliteMalloc() directly, instead of sqlite3BtreeSchema()).
  ** So it needs to be freed here. Todo: Why not roll the temp schema into
  ** the same sqliteMalloc() as the one that allocates the database 
  ** structure?
  */
  sqlite3DbFree(db, db->aDb[1].pSchema);
  sqlite3_mutex_leave(db->mutex);
  db->magic = SQLITE_MAGIC_CLOSED;
  sqlite3_mutex_free(db->mutex);
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  sqlite3_free(db);
  return SQLITE_OK;
}

/*
** Rollback all database files.
*/
void sqlite3RollbackAll(sqlite3 *db){
  int i;
  int inTrans = 0;
  assert( sqlite3_mutex_held(db->mutex) );
  sqlite3BeginBenignMalloc();
  for(i=0; i<db->nDb; i++){
    if( db->aDb[i].pBt ){
      if( sqlite3BtreeIsInTrans(db->aDb[i].pBt) ){
        inTrans = 1;
      }
      sqlite3BtreeRollback(db->aDb[i].pBt);
      db->aDb[i].inTrans = 0;
    }
  }
  sqlite3VtabRollback(db);
  sqlite3EndBenignMalloc();

  if( db->flags&SQLITE_InternChanges ){
    sqlite3ExpirePreparedStatements(db);
    sqlite3ResetInternalSchema(db, 0);
  }

  /* If one has been configured, invoke the rollback-hook callback */
  if( db->xRollbackCallback && (inTrans || !db->autoCommit) ){
    db->xRollbackCallback(db->pRollbackArg);
  }
}

/*
** Return a static string that describes the kind of error specified in the
** argument.
*/
const char *sqlite3ErrStr(int rc){
  const char *z;
  switch( rc & 0xff ){
    case SQLITE_ROW:
    case SQLITE_DONE:
    case SQLITE_OK:         z = "not an error";                          break;
    case SQLITE_ERROR:      z = "SQL logic error or missing database";   break;
    case SQLITE_PERM:       z = "access permission denied";              break;
    case SQLITE_ABORT:      z = "callback requested query abort";        break;
    case SQLITE_BUSY:       z = "database is locked";                    break;
    case SQLITE_LOCKED:     z = "database table is locked";              break;
    case SQLITE_NOMEM:      z = "out of memory";                         break;
    case SQLITE_READONLY:   z = "attempt to write a readonly database";  break;
    case SQLITE_INTERRUPT:  z = "interrupted";                           break;
    case SQLITE_IOERR:      z = "disk I/O error";                        break;
    case SQLITE_CORRUPT:    z = "database disk image is malformed";      break;
    case SQLITE_FULL:       z = "database or disk is full";              break;
    case SQLITE_CANTOPEN:   z = "unable to open database file";          break;
    case SQLITE_EMPTY:      z = "table contains no data";                break;
    case SQLITE_SCHEMA:     z = "database schema has changed";           break;
    case SQLITE_TOOBIG:     z = "String or BLOB exceeded size limit";    break;
    case SQLITE_CONSTRAINT: z = "constraint failed";                     break;
    case SQLITE_MISMATCH:   z = "datatype mismatch";                     break;
    case SQLITE_MISUSE:     z = "library routine called out of sequence";break;
    case SQLITE_NOLFS:      z = "large file support is disabled";        break;
    case SQLITE_AUTH:       z = "authorization denied";                  break;
    case SQLITE_FORMAT:     z = "auxiliary database format error";       break;
    case SQLITE_RANGE:      z = "bind or column index out of range";     break;
    case SQLITE_NOTADB:     z = "file is encrypted or is not a database";break;
    default:                z = "unknown error";                         break;
  }
  return z;
}

/*
** This routine implements a busy callback that sleeps and tries
** again until a timeout value is reached.  The timeout value is
** an integer number of milliseconds passed in as the first
** argument.
*/
static int sqliteDefaultBusyCallback(
 void *ptr,               /* Database connection */
 int count                /* Number of times table has been busy */
){
#if SQLITE_OS_WIN || (defined(HAVE_USLEEP) && HAVE_USLEEP)
  static const u8 delays[] =
     { 1, 2, 5, 10, 15, 20, 25, 25,  25,  50,  50, 100 };
  static const u8 totals[] =
     { 0, 1, 3,  8, 18, 33, 53, 78, 103, 128, 178, 228 };
# define NDELAY (sizeof(delays)/sizeof(delays[0]))
  sqlite3 *db = (sqlite3 *)ptr;
  int timeout = db->busyTimeout;
  int delay, prior;

  assert( count>=0 );
  if( count < NDELAY ){
    delay = delays[count];
    prior = totals[count];
  }else{
    delay = delays[NDELAY-1];
    prior = totals[NDELAY-1] + delay*(count-(NDELAY-1));
  }
  if( prior + delay > timeout ){
    delay = timeout - prior;
    if( delay<=0 ) return 0;
  }
  sqlite3OsSleep(db->pVfs, delay*1000);
  return 1;
#else
  sqlite3 *db = (sqlite3 *)ptr;
  int timeout = ((sqlite3 *)ptr)->busyTimeout;
  if( (count+1)*1000 > timeout ){
    return 0;
  }
  sqlite3OsSleep(db->pVfs, 1000000);
  return 1;
#endif
}

/*
** Invoke the given busy handler.
**
** This routine is called when an operation failed with a lock.
** If this routine returns non-zero, the lock is retried.  If it
** returns 0, the operation aborts with an SQLITE_BUSY error.
*/
int sqlite3InvokeBusyHandler(BusyHandler *p){
  int rc;
  if( NEVER(p==0) || p->xFunc==0 || p->nBusy<0 ) return 0;
  rc = p->xFunc(p->pArg, p->nBusy);
  if( rc==0 ){
    p->nBusy = -1;
  }else{
    p->nBusy++;
  }
  return rc; 
}

/*
** This routine sets the busy callback for an Sqlite database to the
** given callback function with the given argument.
*/
int sqlite3_busy_handler(
  sqlite3 *db,
  int (*xBusy)(void*,int),
  void *pArg
){
  sqlite3_mutex_enter(db->mutex);
  db->busyHandler.xFunc = xBusy;
  db->busyHandler.pArg = pArg;
  db->busyHandler.nBusy = 0;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
/*
** This routine sets the progress callback for an Sqlite database to the
** given callback function with the given argument. The progress callback will
** be invoked every nOps opcodes.
*/
void sqlite3_progress_handler(
  sqlite3 *db, 
  int nOps,
  int (*xProgress)(void*), 
  void *pArg
){
  sqlite3_mutex_enter(db->mutex);
  if( nOps>0 ){
    db->xProgress = xProgress;
    db->nProgressOps = nOps;
    db->pProgressArg = pArg;
  }else{
    db->xProgress = 0;
    db->nProgressOps = 0;
    db->pProgressArg = 0;
  }
  sqlite3_mutex_leave(db->mutex);
}
#endif


/*
** This routine installs a default busy handler that waits for the
** specified number of milliseconds before returning 0.
*/
int sqlite3_busy_timeout(sqlite3 *db, int ms){
  if( ms>0 ){
    db->busyTimeout = ms;
    sqlite3_busy_handler(db, sqliteDefaultBusyCallback, (void*)db);
  }else{
    sqlite3_busy_handler(db, 0, 0);
  }
  return SQLITE_OK;
}

/*
** Cause any pending operation to stop at its earliest opportunity.
*/
void sqlite3_interrupt(sqlite3 *db){
  db->u1.isInterrupted = 1;
}


/*
** This function is exactly the same as sqlite3_create_function(), except
** that it is designed to be called by internal code. The difference is
** that if a malloc() fails in sqlite3_create_function(), an error code
** is returned and the mallocFailed flag cleared. 
*/
int sqlite3CreateFunc(
  sqlite3 *db,
  const char *zFunctionName,
  int nArg,
  int enc,
  void *pUserData,
  void (*xFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*)
){
  FuncDef *p;
  int nName;

  assert( sqlite3_mutex_held(db->mutex) );
  if( zFunctionName==0 ||
      (xFunc && (xFinal || xStep)) || 
      (!xFunc && (xFinal && !xStep)) ||
      (!xFunc && (!xFinal && xStep)) ||
      (nArg<-1 || nArg>SQLITE_MAX_FUNCTION_ARG) ||
      (255<(nName = sqlite3Strlen(db, zFunctionName))) ){
    sqlite3Error(db, SQLITE_ERROR, "bad parameters");
    return SQLITE_ERROR;
  }
  
#ifndef SQLITE_OMIT_UTF16
  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  **
  ** If SQLITE_ANY is specified, add three versions of the function
  ** to the hash table.
  */
  if( enc==SQLITE_UTF16 ){
    enc = SQLITE_UTF16NATIVE;
  }else if( enc==SQLITE_ANY ){
    int rc;
    rc = sqlite3CreateFunc(db, zFunctionName, nArg, SQLITE_UTF8,
         pUserData, xFunc, xStep, xFinal);
    if( rc==SQLITE_OK ){
      rc = sqlite3CreateFunc(db, zFunctionName, nArg, SQLITE_UTF16LE,
          pUserData, xFunc, xStep, xFinal);
    }
    if( rc!=SQLITE_OK ){
      return rc;
    }
    enc = SQLITE_UTF16BE;
  }
#else
  enc = SQLITE_UTF8;
#endif
  
  /* Check if an existing function is being overridden or deleted. If so,
  ** and there are active VMs, then return SQLITE_BUSY. If a function
  ** is being overridden/deleted but there are no active VMs, allow the
  ** operation to continue but invalidate all precompiled statements.
  */
  p = sqlite3FindFunction(db, zFunctionName, nName, nArg, enc, 0);
  if( p && p->iPrefEnc==enc && p->nArg==nArg ){
    if( db->activeVdbeCnt ){
      sqlite3Error(db, SQLITE_BUSY, 
        "Unable to delete/modify user-function due to active statements");
      assert( !db->mallocFailed );
      return SQLITE_BUSY;
    }else{
      sqlite3ExpirePreparedStatements(db);
    }
  }

  p = sqlite3FindFunction(db, zFunctionName, nName, nArg, enc, 1);
  assert(p || db->mallocFailed);
  if( !p ){
    return SQLITE_NOMEM;
  }
  p->flags = 0;
  p->xFunc = xFunc;
  p->xStep = xStep;
  p->xFinalize = xFinal;
  p->pUserData = pUserData;
  p->nArg = nArg;
  return SQLITE_OK;
}

/*
** Create new user functions.
*/
int sqlite3_create_function(
  sqlite3 *db,
  const char *zFunctionName,
  int nArg,
  int enc,
  void *p,
  void (*xFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*)
){
  int rc;
  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3CreateFunc(db, zFunctionName, nArg, enc, p, xFunc, xStep, xFinal);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

#ifndef SQLITE_OMIT_UTF16
int sqlite3_create_function16(
  sqlite3 *db,
  const void *zFunctionName,
  int nArg,
  int eTextRep,
  void *p,
  void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*)
){
  int rc;
  char *zFunc8;
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  zFunc8 = sqlite3Utf16to8(db, zFunctionName, -1);
  rc = sqlite3CreateFunc(db, zFunc8, nArg, eTextRep, p, xFunc, xStep, xFinal);
  sqlite3DbFree(db, zFunc8);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}
#endif


/*
** Declare that a function has been overloaded by a virtual table.
**
** If the function already exists as a regular global function, then
** this routine is a no-op.  If the function does not exist, then create
** a new one that always throws a run-time error.  
**
** When virtual tables intend to provide an overloaded function, they
** should call this routine to make sure the global function exists.
** A global function must exist in order for name resolution to work
** properly.
*/
int sqlite3_overload_function(
  sqlite3 *db,
  const char *zName,
  int nArg
){
  int nName = sqlite3Strlen(db, zName);
  int rc;
  sqlite3_mutex_enter(db->mutex);
  if( sqlite3FindFunction(db, zName, nName, nArg, SQLITE_UTF8, 0)==0 ){
    sqlite3CreateFunc(db, zName, nArg, SQLITE_UTF8,
                      0, sqlite3InvalidFunction, 0, 0);
  }
  rc = sqlite3ApiExit(db, SQLITE_OK);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

#ifndef SQLITE_OMIT_TRACE
/*
** Register a trace function.  The pArg from the previously registered trace
** is returned.  
**
** A NULL trace function means that no tracing is executes.  A non-NULL
** trace is a pointer to a function that is invoked at the start of each
** SQL statement.
*/
void *sqlite3_trace(sqlite3 *db, void (*xTrace)(void*,const char*), void *pArg){
  void *pOld;
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pTraceArg;
  db->xTrace = xTrace;
  db->pTraceArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}
/*
** Register a profile function.  The pArg from the previously registered 
** profile function is returned.  
**
** A NULL profile function means that no profiling is executes.  A non-NULL
** profile is a pointer to a function that is invoked at the conclusion of
** each SQL statement that is run.
*/
void *sqlite3_profile(
  sqlite3 *db,
  void (*xProfile)(void*,const char*,sqlite_uint64),
  void *pArg
){
  void *pOld;
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pProfileArg;
  db->xProfile = xProfile;
  db->pProfileArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}
#endif /* SQLITE_OMIT_TRACE */

/*** EXPERIMENTAL ***
**
** Register a function to be invoked when a transaction comments.
** If the invoked function returns non-zero, then the commit becomes a
** rollback.
*/
void *sqlite3_commit_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  int (*xCallback)(void*),  /* Function to invoke on each commit */
  void *pArg                /* Argument to the function */
){
  void *pOld;
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pCommitArg;
  db->xCommitCallback = xCallback;
  db->pCommitArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}

/*
** Register a callback to be invoked each time a row is updated,
** inserted or deleted using this database connection.
*/
void *sqlite3_update_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*,int,char const *,char const *,sqlite_int64),
  void *pArg                /* Argument to the function */
){
  void *pRet;
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pUpdateArg;
  db->xUpdateCallback = xCallback;
  db->pUpdateArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
}

/*
** Register a callback to be invoked each time a transaction is rolled
** back by this database connection.
*/
void *sqlite3_rollback_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*), /* Callback function */
  void *pArg                /* Argument to the function */
){
  void *pRet;
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pRollbackArg;
  db->xRollbackCallback = xCallback;
  db->pRollbackArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
}

/*
** This routine is called to create a connection to a database BTree
** driver.  If zFilename is the name of a file, then that file is
** opened and used.  If zFilename is the magic name ":memory:" then
** the database is stored in memory (and is thus forgotten as soon as
** the connection is closed.)  If zFilename is NULL then the database
** is a "virtual" database for transient use only and is deleted as
** soon as the connection is closed.
**
** A virtual database can be either a disk file (that is automatically
** deleted when the file is closed) or it an be held entirely in memory,
** depending on the values of the SQLITE_TEMP_STORE compile-time macro and the
** db->temp_store variable, according to the following chart:
**
**   SQLITE_TEMP_STORE     db->temp_store     Location of temporary database
**   -----------------     --------------     ------------------------------
**   0                     any                file
**   1                     1                  file
**   1                     2                  memory
**   1                     0                  file
**   2                     1                  file
**   2                     2                  memory
**   2                     0                  memory
**   3                     any                memory
*/
int sqlite3BtreeFactory(
  const sqlite3 *db,        /* Main database when opening aux otherwise 0 */
  const char *zFilename,    /* Name of the file containing the BTree database */
  int omitJournal,          /* if TRUE then do not journal this file */
  int nCache,               /* How many pages in the page cache */
  int vfsFlags,             /* Flags passed through to vfsOpen */
  Btree **ppBtree           /* Pointer to new Btree object written here */
){
  int btFlags = 0;
  int rc;
  
  assert( sqlite3_mutex_held(db->mutex) );
  assert( ppBtree != 0);
  if( omitJournal ){
    btFlags |= BTREE_OMIT_JOURNAL;
  }
  if( db->flags & SQLITE_NoReadlock ){
    btFlags |= BTREE_NO_READLOCK;
  }
  if( zFilename==0 ){
#if SQLITE_TEMP_STORE==0
    /* Do nothing */
#endif
#ifndef SQLITE_OMIT_MEMORYDB
#if SQLITE_TEMP_STORE==1
    if( db->temp_store==2 ) zFilename = ":memory:";
#endif
#if SQLITE_TEMP_STORE==2
    if( db->temp_store!=1 ) zFilename = ":memory:";
#endif
#if SQLITE_TEMP_STORE==3
    zFilename = ":memory:";
#endif
#endif /* SQLITE_OMIT_MEMORYDB */
  }

  if( (vfsFlags & SQLITE_OPEN_MAIN_DB)!=0 && (zFilename==0 || *zFilename==0) ){
    vfsFlags = (vfsFlags & ~SQLITE_OPEN_MAIN_DB) | SQLITE_OPEN_TEMP_DB;
  }
  rc = sqlite3BtreeOpen(zFilename, (sqlite3 *)db, ppBtree, btFlags, vfsFlags);

  /* If the B-Tree was successfully opened, set the pager-cache size to the
  ** default value. Except, if the call to BtreeOpen() returned a handle
  ** open on an existing shared pager-cache, do not change the pager-cache 
  ** size.
  */
  if( rc==SQLITE_OK && 0==sqlite3BtreeSchema(*ppBtree, 0, 0) ){
    sqlite3BtreeSetCacheSize(*ppBtree, nCache);
  }
  return rc;
}

/*
** Return UTF-8 encoded English language explanation of the most recent
** error.
*/
const char *sqlite3_errmsg(sqlite3 *db){
  const char *z;
  if( !db ){
    return sqlite3ErrStr(SQLITE_NOMEM);
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return sqlite3ErrStr(SQLITE_MISUSE);
  }
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  z = (char*)sqlite3_value_text(db->pErr);
  assert( !db->mallocFailed );
  if( z==0 ){
    z = sqlite3ErrStr(db->errCode);
  }
  sqlite3_mutex_leave(db->mutex);
  return z;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Return UTF-16 encoded English language explanation of the most recent
** error.
*/
const void *sqlite3_errmsg16(sqlite3 *db){
  /* Because all the characters in the string are in the unicode
  ** range 0x00-0xFF, if we pad the big-endian string with a 
  ** zero byte, we can obtain the little-endian string with
  ** &big_endian[1].
  */
  static const char outOfMemBe[] = {
    0, 'o', 0, 'u', 0, 't', 0, ' ', 
    0, 'o', 0, 'f', 0, ' ', 
    0, 'm', 0, 'e', 0, 'm', 0, 'o', 0, 'r', 0, 'y', 0, 0, 0
  };
  static const char misuseBe [] = {
    0, 'l', 0, 'i', 0, 'b', 0, 'r', 0, 'a', 0, 'r', 0, 'y', 0, ' ', 
    0, 'r', 0, 'o', 0, 'u', 0, 't', 0, 'i', 0, 'n', 0, 'e', 0, ' ', 
    0, 'c', 0, 'a', 0, 'l', 0, 'l', 0, 'e', 0, 'd', 0, ' ', 
    0, 'o', 0, 'u', 0, 't', 0, ' ', 
    0, 'o', 0, 'f', 0, ' ', 
    0, 's', 0, 'e', 0, 'q', 0, 'u', 0, 'e', 0, 'n', 0, 'c', 0, 'e', 0, 0, 0
  };

  const void *z;
  if( !db ){
    return (void *)(&outOfMemBe[SQLITE_UTF16NATIVE==SQLITE_UTF16LE?1:0]);
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return (void *)(&misuseBe[SQLITE_UTF16NATIVE==SQLITE_UTF16LE?1:0]);
  }
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  z = sqlite3_value_text16(db->pErr);
  if( z==0 ){
    sqlite3ValueSetStr(db->pErr, -1, sqlite3ErrStr(db->errCode),
         SQLITE_UTF8, SQLITE_STATIC);
    z = sqlite3_value_text16(db->pErr);
  }
  /* A malloc() may have failed within the call to sqlite3_value_text16()
  ** above. If this is the case, then the db->mallocFailed flag needs to
  ** be cleared before returning. Do this directly, instead of via
  ** sqlite3ApiExit(), to avoid setting the database handle error message.
  */
  db->mallocFailed = 0;
  sqlite3_mutex_leave(db->mutex);
  return z;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Return the most recent error code generated by an SQLite routine. If NULL is
** passed to this function, we assume a malloc() failed during sqlite3_open().
*/
int sqlite3_errcode(sqlite3 *db){
  if( db && !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE;
  }
  if( !db || db->mallocFailed ){
    return SQLITE_NOMEM;
  }
  return db->errCode & db->errMask;
}

/*
** Create a new collating function for database "db".  The name is zName
** and the encoding is enc.
*/
static int createCollation(
  sqlite3* db, 
  const char *zName, 
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  CollSeq *pColl;
  int enc2;
  int nName;
  
  assert( sqlite3_mutex_held(db->mutex) );

  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  */
  enc2 = enc & ~SQLITE_UTF16_ALIGNED;
  if( enc2==SQLITE_UTF16 ){
    enc2 = SQLITE_UTF16NATIVE;
  }
  if( (enc2&~3)!=0 ){
    return SQLITE_MISUSE;
  }

  /* Check if this call is removing or replacing an existing collation 
  ** sequence. If so, and there are active VMs, return busy. If there
  ** are no active VMs, invalidate any pre-compiled statements.
  */
  nName = sqlite3Strlen(db, zName);
  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, nName, 0);
  if( pColl && pColl->xCmp ){
    if( db->activeVdbeCnt ){
      sqlite3Error(db, SQLITE_BUSY, 
        "Unable to delete/modify collation sequence due to active statements");
      return SQLITE_BUSY;
    }
    sqlite3ExpirePreparedStatements(db);

    /* If collation sequence pColl was created directly by a call to
    ** sqlite3_create_collation, and not generated by synthCollSeq(),
    ** then any copies made by synthCollSeq() need to be invalidated.
    ** Also, collation destructor - CollSeq.xDel() - function may need
    ** to be called.
    */ 
    if( (pColl->enc & ~SQLITE_UTF16_ALIGNED)==enc2 ){
      CollSeq *aColl = sqlite3HashFind(&db->aCollSeq, zName, nName);
      int j;
      for(j=0; j<3; j++){
        CollSeq *p = &aColl[j];
        if( p->enc==pColl->enc ){
          if( p->xDel ){
            p->xDel(p->pUser);
          }
          p->xCmp = 0;
        }
      }
    }
  }

  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, nName, 1);
  if( pColl ){
    pColl->xCmp = xCompare;
    pColl->pUser = pCtx;
    pColl->xDel = xDel;
    pColl->enc = enc2 | (enc & SQLITE_UTF16_ALIGNED);
  }
  sqlite3Error(db, SQLITE_OK, 0);
  return SQLITE_OK;
}


/*
** This array defines hard upper bounds on limit values.  The
** initializer must be kept in sync with the SQLITE_LIMIT_*
** #defines in sqlite3.h.
*/
static const int aHardLimit[] = {
  SQLITE_MAX_LENGTH,
  SQLITE_MAX_SQL_LENGTH,
  SQLITE_MAX_COLUMN,
  SQLITE_MAX_EXPR_DEPTH,
  SQLITE_MAX_COMPOUND_SELECT,
  SQLITE_MAX_VDBE_OP,
  SQLITE_MAX_FUNCTION_ARG,
  SQLITE_MAX_ATTACHED,
  SQLITE_MAX_LIKE_PATTERN_LENGTH,
  SQLITE_MAX_VARIABLE_NUMBER,
};

/*
** Make sure the hard limits are set to reasonable values
*/
#if SQLITE_MAX_LENGTH<100
# error SQLITE_MAX_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH<100
# error SQLITE_MAX_SQL_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH>SQLITE_MAX_LENGTH
# error SQLITE_MAX_SQL_LENGTH must not be greater than SQLITE_MAX_LENGTH
#endif
#if SQLITE_MAX_COMPOUND_SELECT<2
# error SQLITE_MAX_COMPOUND_SELECT must be at least 2
#endif
#if SQLITE_MAX_VDBE_OP<40
# error SQLITE_MAX_VDBE_OP must be at least 40
#endif
#if SQLITE_MAX_FUNCTION_ARG<0 || SQLITE_MAX_FUNCTION_ARG>127
# error SQLITE_MAX_FUNCTION_ARG must be between 0 and 127
#endif
#if SQLITE_MAX_ATTACH<0 || SQLITE_MAX_ATTACH>30
# error SQLITE_MAX_ATTACH must be between 0 and 30
#endif
#if SQLITE_MAX_LIKE_PATTERN_LENGTH<1
# error SQLITE_MAX_LIKE_PATTERN_LENGTH must be at least 1
#endif
#if SQLITE_MAX_VARIABLE_NUMBER<1
# error SQLITE_MAX_VARIABLE_NUMBER must be at least 1
#endif


/*
** Change the value of a limit.  Report the old value.
** If an invalid limit index is supplied, report -1.
** Make no changes but still report the old value if the
** new limit is negative.
**
** A new lower limit does not shrink existing constructs.
** It merely prevents new constructs that exceed the limit
** from forming.
*/
int sqlite3_limit(sqlite3 *db, int limitId, int newLimit){
  int oldLimit;
  if( limitId<0 || limitId>=SQLITE_N_LIMIT ){
    return -1;
  }
  oldLimit = db->aLimit[limitId];
  if( newLimit>=0 ){
    if( newLimit>aHardLimit[limitId] ){
      newLimit = aHardLimit[limitId];
    }
    db->aLimit[limitId] = newLimit;
  }
  return oldLimit;
}

/*
** This routine does the work of opening a database on behalf of
** sqlite3_open() and sqlite3_open16(). The database filename "zFilename"  
** is UTF-8 encoded.
*/
static int openDatabase(
  const char *zFilename, /* Database filename UTF-8 encoded */
  sqlite3 **ppDb,        /* OUT: Returned database handle */
  unsigned flags,        /* Operational flags */
  const char *zVfs       /* Name of the VFS to use */
){
  sqlite3 *db;
  int rc;
  CollSeq *pColl;
  int isThreadsafe = 1;

#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif

  if( flags&SQLITE_OPEN_NOMUTEX ){
    isThreadsafe = 0;
  }

  /* Remove harmful bits from the flags parameter */
  flags &=  ~( SQLITE_OPEN_DELETEONCLOSE |
               SQLITE_OPEN_MAIN_DB |
               SQLITE_OPEN_TEMP_DB | 
               SQLITE_OPEN_TRANSIENT_DB | 
               SQLITE_OPEN_MAIN_JOURNAL | 
               SQLITE_OPEN_TEMP_JOURNAL | 
               SQLITE_OPEN_SUBJOURNAL | 
               SQLITE_OPEN_MASTER_JOURNAL |
               SQLITE_OPEN_NOMUTEX
             );

  /* Allocate the sqlite data structure */
  db = sqlite3MallocZero( sizeof(sqlite3) );
  if( db==0 ) goto opendb_out;
  if( sqlite3Config.bFullMutex && isThreadsafe ){
    db->mutex = sqlite3MutexAlloc(SQLITE_MUTEX_RECURSIVE);
    if( db->mutex==0 ){
      sqlite3_free(db);
      db = 0;
      goto opendb_out;
    }
  }
  sqlite3_mutex_enter(db->mutex);
  db->errMask = 0xff;
  db->priorNewRowid = 0;
  db->nDb = 2;
  db->magic = SQLITE_MAGIC_BUSY;
  db->aDb = db->aDbStatic;

  assert( sizeof(db->aLimit)==sizeof(aHardLimit) );
  memcpy(db->aLimit, aHardLimit, sizeof(db->aLimit));
  db->autoCommit = 1;
  db->nextAutovac = -1;
  db->nextPagesize = 0;
  db->flags |= SQLITE_ShortColNames
#if SQLITE_DEFAULT_FILE_FORMAT<4
                 | SQLITE_LegacyFileFmt
#endif
#ifdef SQLITE_ENABLE_LOAD_EXTENSION
                 | SQLITE_LoadExtension
#endif
      ;
  sqlite3HashInit(&db->aFunc, SQLITE_HASH_STRING, 0);
  sqlite3HashInit(&db->aCollSeq, SQLITE_HASH_STRING, 0);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  sqlite3HashInit(&db->aModule, SQLITE_HASH_STRING, 0);
#endif

  db->pVfs = sqlite3_vfs_find(zVfs);
  if( !db->pVfs ){
    rc = SQLITE_ERROR;
    db->magic = SQLITE_MAGIC_SICK;
    sqlite3Error(db, rc, "no such vfs: %s", zVfs);
    goto opendb_out;
  }

  /* Add the default collation sequence BINARY. BINARY works for both UTF-8
  ** and UTF-16, so add a version for each to avoid any unnecessary
  ** conversions. The only error that can occur here is a malloc() failure.
  */
  createCollation(db, "BINARY", SQLITE_UTF8, 0, binCollFunc, 0);
  createCollation(db, "BINARY", SQLITE_UTF16BE, 0, binCollFunc, 0);
  createCollation(db, "BINARY", SQLITE_UTF16LE, 0, binCollFunc, 0);
  createCollation(db, "RTRIM", SQLITE_UTF8, (void*)1, binCollFunc, 0);
  if( db->mallocFailed ){
    db->magic = SQLITE_MAGIC_SICK;
    goto opendb_out;
  }
  db->pDfltColl = sqlite3FindCollSeq(db, SQLITE_UTF8, "BINARY", 6, 0);
  assert( db->pDfltColl!=0 );

  /* Also add a UTF-8 case-insensitive collation sequence. */
  createCollation(db, "NOCASE", SQLITE_UTF8, 0, nocaseCollatingFunc, 0);

  /* Set flags on the built-in collating sequences */
  db->pDfltColl->type = SQLITE_COLL_BINARY;
  pColl = sqlite3FindCollSeq(db, SQLITE_UTF8, "NOCASE", 6, 0);
  if( pColl ){
    pColl->type = SQLITE_COLL_NOCASE;
  }

  /* Open the backend database driver */
  db->openFlags = flags;
  rc = sqlite3BtreeFactory(db, zFilename, 0, SQLITE_DEFAULT_CACHE_SIZE, 
                           flags | SQLITE_OPEN_MAIN_DB,
                           &db->aDb[0].pBt);
  if( rc!=SQLITE_OK ){
    sqlite3Error(db, rc, 0);
    db->magic = SQLITE_MAGIC_SICK;
    goto opendb_out;
  }
  db->aDb[0].pSchema = sqlite3SchemaGet(db, db->aDb[0].pBt);
  db->aDb[1].pSchema = sqlite3SchemaGet(db, 0);


  /* The default safety_level for the main database is 'full'; for the temp
  ** database it is 'NONE'. This matches the pager layer defaults.  
  */
  db->aDb[0].zName = "main";
  db->aDb[0].safety_level = 3;
#ifndef SQLITE_OMIT_TEMPDB
  db->aDb[1].zName = "temp";
  db->aDb[1].safety_level = 1;
#endif

  db->magic = SQLITE_MAGIC_OPEN;
  if( db->mallocFailed ){
    goto opendb_out;
  }

  /* Register all built-in functions, but do not attempt to read the
  ** database schema yet. This is delayed until the first time the database
  ** is accessed.
  */
  sqlite3Error(db, SQLITE_OK, 0);
  sqlite3RegisterBuiltinFunctions(db);

  /* Load automatic extensions - extensions that have been registered
  ** using the sqlite3_automatic_extension() API.
  */
  (void)sqlite3AutoLoadExtensions(db);
  if( sqlite3_errcode(db)!=SQLITE_OK ){
    goto opendb_out;
  }

#ifdef SQLITE_ENABLE_FTS1
  if( !db->mallocFailed ){
    extern int sqlite3Fts1Init(sqlite3*);
    rc = sqlite3Fts1Init(db);
  }
#endif

#ifdef SQLITE_ENABLE_FTS2
  if( !db->mallocFailed && rc==SQLITE_OK ){
    extern int sqlite3Fts2Init(sqlite3*);
    rc = sqlite3Fts2Init(db);
  }
#endif

#ifdef SQLITE_ENABLE_FTS3
  if( !db->mallocFailed && rc==SQLITE_OK ){
    rc = sqlite3Fts3Init(db);
  }
#endif

#ifdef SQLITE_ENABLE_ICU
  if( !db->mallocFailed && rc==SQLITE_OK ){
    extern int sqlite3IcuInit(sqlite3*);
    rc = sqlite3IcuInit(db);
  }
#endif

#ifdef SQLITE_ENABLE_RTREE
  if( !db->mallocFailed && rc==SQLITE_OK){
    rc = sqlite3RtreeInit(db);
  }
#endif

  sqlite3Error(db, rc, 0);

  /* -DSQLITE_DEFAULT_LOCKING_MODE=1 makes EXCLUSIVE the default locking
  ** mode.  -DSQLITE_DEFAULT_LOCKING_MODE=0 make NORMAL the default locking
  ** mode.  Doing nothing at all also makes NORMAL the default.
  */
#ifdef SQLITE_DEFAULT_LOCKING_MODE
  db->dfltLockMode = SQLITE_DEFAULT_LOCKING_MODE;
  sqlite3PagerLockingMode(sqlite3BtreePager(db->aDb[0].pBt),
                          SQLITE_DEFAULT_LOCKING_MODE);
#endif

  /* Enable the lookaside-malloc subsystem */
  setupLookaside(db, 0, sqlite3Config.szLookaside, sqlite3Config.nLookaside);

opendb_out:
  if( db ){
    assert( db->mutex!=0 || isThreadsafe==0 || sqlite3Config.bFullMutex==0 );
    sqlite3_mutex_leave(db->mutex);
  }
  if( SQLITE_NOMEM==(rc = sqlite3_errcode(db)) ){
    sqlite3_close(db);
    db = 0;
  }
  *ppDb = db;
  return sqlite3ApiExit(0, rc);
}

/*
** Open a new database handle.
*/
int sqlite3_open(
  const char *zFilename, 
  sqlite3 **ppDb 
){
  return openDatabase(zFilename, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
}
int sqlite3_open_v2(
  const char *filename,   /* Database filename (UTF-8) */
  sqlite3 **ppDb,         /* OUT: SQLite db handle */
  int flags,              /* Flags */
  const char *zVfs        /* Name of VFS module to use */
){
  return openDatabase(filename, ppDb, flags, zVfs);
}

#ifndef SQLITE_OMIT_UTF16
/*
** Open a new database handle.
*/
int sqlite3_open16(
  const void *zFilename, 
  sqlite3 **ppDb
){
  char const *zFilename8;   /* zFilename encoded in UTF-8 instead of UTF-16 */
  sqlite3_value *pVal;
  int rc;

  assert( zFilename );
  assert( ppDb );
  *ppDb = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  pVal = sqlite3ValueNew(0);
  sqlite3ValueSetStr(pVal, -1, zFilename, SQLITE_UTF16NATIVE, SQLITE_STATIC);
  zFilename8 = sqlite3ValueText(pVal, SQLITE_UTF8);
  if( zFilename8 ){
    rc = openDatabase(zFilename8, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    assert( *ppDb || rc==SQLITE_NOMEM );
    if( rc==SQLITE_OK && !DbHasProperty(*ppDb, 0, DB_SchemaLoaded) ){
      ENC(*ppDb) = SQLITE_UTF16NATIVE;
    }
  }else{
    rc = SQLITE_NOMEM;
  }
  sqlite3ValueFree(pVal);

  return sqlite3ApiExit(0, rc);
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation(
  sqlite3* db, 
  const char *zName, 
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  int rc;
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  rc = createCollation(db, zName, enc, pCtx, xCompare, 0);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation_v2(
  sqlite3* db, 
  const char *zName, 
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  int rc;
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  rc = createCollation(db, zName, enc, pCtx, xCompare, xDel);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation16(
  sqlite3* db, 
  const void *zName,
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  int rc = SQLITE_OK;
  char *zName8;
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  zName8 = sqlite3Utf16to8(db, zName, -1);
  if( zName8 ){
    rc = createCollation(db, zName8, enc, pCtx, xCompare, 0);
    sqlite3DbFree(db, zName8);
  }
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed(
  sqlite3 *db, 
  void *pCollNeededArg, 
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*)
){
  sqlite3_mutex_enter(db->mutex);
  db->xCollNeeded = xCollNeeded;
  db->xCollNeeded16 = 0;
  db->pCollNeededArg = pCollNeededArg;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed16(
  sqlite3 *db, 
  void *pCollNeededArg, 
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*)
){
  sqlite3_mutex_enter(db->mutex);
  db->xCollNeeded = 0;
  db->xCollNeeded16 = xCollNeeded16;
  db->pCollNeededArg = pCollNeededArg;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}
#endif /* SQLITE_OMIT_UTF16 */

#ifndef SQLITE_OMIT_GLOBALRECOVER
/*
** This function is now an anachronism. It used to be used to recover from a
** malloc() failure, but SQLite now does this automatically.
*/
int sqlite3_global_recover(void){
  return SQLITE_OK;
}
#endif

/*
** Test to see whether or not the database connection is in autocommit
** mode.  Return TRUE if it is and FALSE if not.  Autocommit mode is on
** by default.  Autocommit is disabled by a BEGIN statement and reenabled
** by the next COMMIT or ROLLBACK.
**
******* THIS IS AN EXPERIMENTAL API AND IS SUBJECT TO CHANGE ******
*/
int sqlite3_get_autocommit(sqlite3 *db){
  return db->autoCommit;
}

#ifdef SQLITE_DEBUG
/*
** The following routine is subtituted for constant SQLITE_CORRUPT in
** debugging builds.  This provides a way to set a breakpoint for when
** corruption is first detected.
*/
int sqlite3Corrupt(void){
  return SQLITE_CORRUPT;
}
#endif

/*
** This is a convenience routine that makes sure that all thread-specific
** data for this thread has been deallocated.
**
** SQLite no longer uses thread-specific data so this routine is now a
** no-op.  It is retained for historical compatibility.
*/
void sqlite3_thread_cleanup(void){
}

/*
** Return meta information about a specific column of a database table.
** See comment in sqlite3.h (sqlite.h.in) for details.
*/
#ifdef SQLITE_ENABLE_COLUMN_METADATA
int sqlite3_table_column_metadata(
  sqlite3 *db,                /* Connection handle */
  const char *zDbName,        /* Database name or NULL */
  const char *zTableName,     /* Table name */
  const char *zColumnName,    /* Column name */
  char const **pzDataType,    /* OUTPUT: Declared data type */
  char const **pzCollSeq,     /* OUTPUT: Collation sequence name */
  int *pNotNull,              /* OUTPUT: True if NOT NULL constraint exists */
  int *pPrimaryKey,           /* OUTPUT: True if column part of PK */
  int *pAutoinc               /* OUTPUT: True if column is auto-increment */
){
  int rc;
  char *zErrMsg = 0;
  Table *pTab = 0;
  Column *pCol = 0;
  int iCol;

  char const *zDataType = 0;
  char const *zCollSeq = 0;
  int notnull = 0;
  int primarykey = 0;
  int autoinc = 0;

  /* Ensure the database schema has been loaded */
  sqlite3_mutex_enter(db->mutex);
  (void)sqlite3SafetyOn(db);
  sqlite3BtreeEnterAll(db);
  rc = sqlite3Init(db, &zErrMsg);
  sqlite3BtreeLeaveAll(db);
  if( SQLITE_OK!=rc ){
    goto error_out;
  }

  /* Locate the table in question */
  pTab = sqlite3FindTable(db, zTableName, zDbName);
  if( !pTab || pTab->pSelect ){
    pTab = 0;
    goto error_out;
  }

  /* Find the column for which info is requested */
  if( sqlite3IsRowid(zColumnName) ){
    iCol = pTab->iPKey;
    if( iCol>=0 ){
      pCol = &pTab->aCol[iCol];
    }
  }else{
    for(iCol=0; iCol<pTab->nCol; iCol++){
      pCol = &pTab->aCol[iCol];
      if( 0==sqlite3StrICmp(pCol->zName, zColumnName) ){
        break;
      }
    }
    if( iCol==pTab->nCol ){
      pTab = 0;
      goto error_out;
    }
  }

  /* The following block stores the meta information that will be returned
  ** to the caller in local variables zDataType, zCollSeq, notnull, primarykey
  ** and autoinc. At this point there are two possibilities:
  ** 
  **     1. The specified column name was rowid", "oid" or "_rowid_" 
  **        and there is no explicitly declared IPK column. 
  **
  **     2. The table is not a view and the column name identified an 
  **        explicitly declared column. Copy meta information from *pCol.
  */ 
  if( pCol ){
    zDataType = pCol->zType;
    zCollSeq = pCol->zColl;
    notnull = pCol->notNull!=0;
    primarykey  = pCol->isPrimKey!=0;
    autoinc = pTab->iPKey==iCol && pTab->autoInc;
  }else{
    zDataType = "INTEGER";
    primarykey = 1;
  }
  if( !zCollSeq ){
    zCollSeq = "BINARY";
  }

error_out:
  (void)sqlite3SafetyOff(db);

  /* Whether the function call succeeded or failed, set the output parameters
  ** to whatever their local counterparts contain. If an error did occur,
  ** this has the effect of zeroing all output parameters.
  */
  if( pzDataType ) *pzDataType = zDataType;
  if( pzCollSeq ) *pzCollSeq = zCollSeq;
  if( pNotNull ) *pNotNull = notnull;
  if( pPrimaryKey ) *pPrimaryKey = primarykey;
  if( pAutoinc ) *pAutoinc = autoinc;

  if( SQLITE_OK==rc && !pTab ){
    sqlite3DbFree(db, zErrMsg);
    zErrMsg = sqlite3MPrintf(db, "no such table column: %s.%s", zTableName,
        zColumnName);
    rc = SQLITE_ERROR;
  }
  sqlite3Error(db, rc, (zErrMsg?"%s":0), zErrMsg);
  sqlite3DbFree(db, zErrMsg);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}
#endif

/*
** Sleep for a little while.  Return the amount of time slept.
*/
int sqlite3_sleep(int ms){
  sqlite3_vfs *pVfs;
  int rc;
  pVfs = sqlite3_vfs_find(0);
  if( pVfs==0 ) return 0;

  /* This function works in milliseconds, but the underlying OsSleep() 
  ** API uses microseconds. Hence the 1000's.
  */
  rc = (sqlite3OsSleep(pVfs, 1000*ms)/1000);
  return rc;
}

/*
** Enable or disable the extended result codes.
*/
int sqlite3_extended_result_codes(sqlite3 *db, int onoff){
  sqlite3_mutex_enter(db->mutex);
  db->errMask = onoff ? 0xffffffff : 0xff;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

/*
** Invoke the xFileControl method on a particular database.
*/
int sqlite3_file_control(sqlite3 *db, const char *zDbName, int op, void *pArg){
  int rc = SQLITE_ERROR;
  int iDb;
  sqlite3_mutex_enter(db->mutex);
  if( zDbName==0 ){
    iDb = 0;
  }else{
    for(iDb=0; iDb<db->nDb; iDb++){
      if( strcmp(db->aDb[iDb].zName, zDbName)==0 ) break;
    }
  }
  if( iDb<db->nDb ){
    Btree *pBtree = db->aDb[iDb].pBt;
    if( pBtree ){
      Pager *pPager;
      sqlite3_file *fd;
      sqlite3BtreeEnter(pBtree);
      pPager = sqlite3BtreePager(pBtree);
      assert( pPager!=0 );
      fd = sqlite3PagerFile(pPager);
      assert( fd!=0 );
      if( fd->pMethods ){
        rc = sqlite3OsFileControl(fd, op, pArg);
      }
      sqlite3BtreeLeave(pBtree);
    }
  }
  sqlite3_mutex_leave(db->mutex);
  return rc;   
}

/*
** Interface to the testing logic.
*/
int sqlite3_test_control(int op, ...){
  int rc = 0;
#ifndef SQLITE_OMIT_BUILTIN_TEST
  va_list ap;
  va_start(ap, op);
  switch( op ){

    /*
    ** Save the current state of the PRNG.
    */
    case SQLITE_TESTCTRL_PRNG_SAVE: {
      sqlite3PrngSaveState();
      break;
    }

    /*
    ** Restore the state of the PRNG to the last state saved using
    ** PRNG_SAVE.  If PRNG_SAVE has never before been called, then
    ** this verb acts like PRNG_RESET.
    */
    case SQLITE_TESTCTRL_PRNG_RESTORE: {
      sqlite3PrngRestoreState();
      break;
    }

    /*
    ** Reset the PRNG back to its uninitialized state.  The next call
    ** to sqlite3_randomness() will reseed the PRNG using a single call
    ** to the xRandomness method of the default VFS.
    */
    case SQLITE_TESTCTRL_PRNG_RESET: {
      sqlite3PrngResetState();
      break;
    }

    /*
    **  sqlite3_test_control(BITVEC_TEST, size, program)
    **
    ** Run a test against a Bitvec object of size.  The program argument
    ** is an array of integers that defines the test.  Return -1 on a
    ** memory allocation error, 0 on success, or non-zero for an error.
    ** See the sqlite3BitvecBuiltinTest() for additional information.
    */
    case SQLITE_TESTCTRL_BITVEC_TEST: {
      int sz = va_arg(ap, int);
      int *aProg = va_arg(ap, int*);
      rc = sqlite3BitvecBuiltinTest(sz, aProg);
      break;
    }

    /*
    **  sqlite3_test_control(BENIGN_MALLOC_HOOKS, xBegin, xEnd)
    **
    ** Register hooks to call to indicate which malloc() failures 
    ** are benign.
    */
    case SQLITE_TESTCTRL_BENIGN_MALLOC_HOOKS: {
      typedef void (*void_function)(void);
      void_function xBenignBegin;
      void_function xBenignEnd;
      xBenignBegin = va_arg(ap, void_function);
      xBenignEnd = va_arg(ap, void_function);
      sqlite3BenignMallocHooks(xBenignBegin, xBenignEnd);
      break;
    }
  }
  va_end(ap);
#endif /* SQLITE_OMIT_BUILTIN_TEST */
  return rc;
}
