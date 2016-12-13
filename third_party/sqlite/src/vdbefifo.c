/*
** 2005 June 16
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file implements a FIFO queue of rowids used for processing
** UPDATE and DELETE statements.
**
** $Id: vdbefifo.c,v 1.8 2008/07/28 19:34:54 drh Exp $
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

/*
** Constants FIFOSIZE_FIRST and FIFOSIZE_MAX are the initial
** number of entries in a fifo page and the maximum number of
** entries in a fifo page.
*/
#define FIFOSIZE_FIRST (((128-sizeof(FifoPage))/8)+1)
#ifdef SQLITE_MALLOC_SOFT_LIMIT
# define FIFOSIZE_MAX   (((SQLITE_MALLOC_SOFT_LIMIT-sizeof(FifoPage))/8)+1)
#else
# define FIFOSIZE_MAX   (((262144-sizeof(FifoPage))/8)+1)
#endif

/*
** Allocate a new FifoPage and return a pointer to it.  Return NULL if
** we run out of memory.  Leave space on the page for nEntry entries.
*/
static FifoPage *allocateFifoPage(sqlite3 *db, int nEntry){
  FifoPage *pPage;
  if( nEntry>FIFOSIZE_MAX ){
    nEntry = FIFOSIZE_MAX;
  }
  pPage = sqlite3DbMallocRaw(db, sizeof(FifoPage) + sizeof(i64)*(nEntry-1) );
  if( pPage ){
    pPage->nSlot = nEntry;
    pPage->iWrite = 0;
    pPage->iRead = 0;
    pPage->pNext = 0;
  }
  return pPage;
}

/*
** Initialize a Fifo structure.
*/
void sqlite3VdbeFifoInit(Fifo *pFifo, sqlite3 *db){
  memset(pFifo, 0, sizeof(*pFifo));
  pFifo->db = db;
}

/*
** Push a single 64-bit integer value into the Fifo.  Return SQLITE_OK
** normally.   SQLITE_NOMEM is returned if we are unable to allocate
** memory.
*/
int sqlite3VdbeFifoPush(Fifo *pFifo, i64 val){
  FifoPage *pPage;
  pPage = pFifo->pLast;
  if( pPage==0 ){
    pPage = pFifo->pLast = pFifo->pFirst =
         allocateFifoPage(pFifo->db, FIFOSIZE_FIRST);
    if( pPage==0 ){
      return SQLITE_NOMEM;
    }
  }else if( pPage->iWrite>=pPage->nSlot ){
    pPage->pNext = allocateFifoPage(pFifo->db, pFifo->nEntry);
    if( pPage->pNext==0 ){
      return SQLITE_NOMEM;
    }
    pPage = pFifo->pLast = pPage->pNext;
  }
  pPage->aSlot[pPage->iWrite++] = val;
  pFifo->nEntry++;
  return SQLITE_OK;
}

/*
** Extract a single 64-bit integer value from the Fifo.  The integer
** extracted is the one least recently inserted.  If the Fifo is empty
** return SQLITE_DONE.
*/
int sqlite3VdbeFifoPop(Fifo *pFifo, i64 *pVal){
  FifoPage *pPage;
  if( pFifo->nEntry==0 ){
    return SQLITE_DONE;
  }
  assert( pFifo->nEntry>0 );
  pPage = pFifo->pFirst;
  assert( pPage!=0 );
  assert( pPage->iWrite>pPage->iRead );
  assert( pPage->iWrite<=pPage->nSlot );
  assert( pPage->iRead<pPage->nSlot );
  assert( pPage->iRead>=0 );
  *pVal = pPage->aSlot[pPage->iRead++];
  pFifo->nEntry--;
  if( pPage->iRead>=pPage->iWrite ){
    pFifo->pFirst = pPage->pNext;
    sqlite3DbFree(pFifo->db, pPage);
    if( pFifo->nEntry==0 ){
      assert( pFifo->pLast==pPage );
      pFifo->pLast = 0;
    }else{
      assert( pFifo->pFirst!=0 );
    }
  }else{
    assert( pFifo->nEntry>0 );
  }
  return SQLITE_OK;
}

/*
** Delete all information from a Fifo object.   Free all memory held
** by the Fifo.
*/
void sqlite3VdbeFifoClear(Fifo *pFifo){
  FifoPage *pPage, *pNextPage;
  for(pPage=pFifo->pFirst; pPage; pPage=pNextPage){
    pNextPage = pPage->pNext;
    sqlite3DbFree(pFifo->db, pPage);
  }
  sqlite3VdbeFifoInit(pFifo, pFifo->db);
}
