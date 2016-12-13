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
** This module contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.  This module is responsible for
** generating the code that loops through a table looking for applicable
** rows.  Indices are selected and used to speed the search when doing
** so is applicable.  Because this module is responsible for selecting
** indices, you might also think of this module as the "query optimizer".
**
** $Id: where.c,v 1.319 2008/08/01 17:37:41 danielk1977 Exp $
*/
#include "sqliteInt.h"

/*
** The number of bits in a Bitmask.  "BMS" means "BitMask Size".
*/
#define BMS  (sizeof(Bitmask)*8)

/*
** Trace output macros
*/
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
int sqlite3WhereTrace = 0;
#endif
#if 0
# define WHERETRACE(X)  if(sqlite3WhereTrace) sqlite3DebugPrintf X
#else
# define WHERETRACE(X)
#endif

/* Forward reference
*/
typedef struct WhereClause WhereClause;
typedef struct ExprMaskSet ExprMaskSet;

/*
** The query generator uses an array of instances of this structure to
** help it analyze the subexpressions of the WHERE clause.  Each WHERE
** clause subexpression is separated from the others by an AND operator.
**
** All WhereTerms are collected into a single WhereClause structure.  
** The following identity holds:
**
**        WhereTerm.pWC->a[WhereTerm.idx] == WhereTerm
**
** When a term is of the form:
**
**              X <op> <expr>
**
** where X is a column name and <op> is one of certain operators,
** then WhereTerm.leftCursor and WhereTerm.leftColumn record the
** cursor number and column number for X.  WhereTerm.operator records
** the <op> using a bitmask encoding defined by WO_xxx below.  The
** use of a bitmask encoding for the operator allows us to search
** quickly for terms that match any of several different operators.
**
** prereqRight and prereqAll record sets of cursor numbers,
** but they do so indirectly.  A single ExprMaskSet structure translates
** cursor number into bits and the translated bit is stored in the prereq
** fields.  The translation is used in order to maximize the number of
** bits that will fit in a Bitmask.  The VDBE cursor numbers might be
** spread out over the non-negative integers.  For example, the cursor
** numbers might be 3, 8, 9, 10, 20, 23, 41, and 45.  The ExprMaskSet
** translates these sparse cursor numbers into consecutive integers
** beginning with 0 in order to make the best possible use of the available
** bits in the Bitmask.  So, in the example above, the cursor numbers
** would be mapped into integers 0 through 7.
*/
typedef struct WhereTerm WhereTerm;
struct WhereTerm {
  Expr *pExpr;            /* Pointer to the subexpression */
  i16 iParent;            /* Disable pWC->a[iParent] when this term disabled */
  i16 leftCursor;         /* Cursor number of X in "X <op> <expr>" */
  i16 leftColumn;         /* Column number of X in "X <op> <expr>" */
  u16 eOperator;          /* A WO_xx value describing <op> */
  u8 flags;               /* Bit flags.  See below */
  u8 nChild;              /* Number of children that must disable us */
  WhereClause *pWC;       /* The clause this term is part of */
  Bitmask prereqRight;    /* Bitmask of tables used by pRight */
  Bitmask prereqAll;      /* Bitmask of tables referenced by p */
};

/*
** Allowed values of WhereTerm.flags
*/
#define TERM_DYNAMIC    0x01   /* Need to call sqlite3ExprDelete(db, pExpr) */
#define TERM_VIRTUAL    0x02   /* Added by the optimizer.  Do not code */
#define TERM_CODED      0x04   /* This term is already coded */
#define TERM_COPIED     0x08   /* Has a child */
#define TERM_OR_OK      0x10   /* Used during OR-clause processing */

/*
** An instance of the following structure holds all information about a
** WHERE clause.  Mostly this is a container for one or more WhereTerms.
*/
struct WhereClause {
  Parse *pParse;           /* The parser context */
  ExprMaskSet *pMaskSet;   /* Mapping of table indices to bitmasks */
  int nTerm;               /* Number of terms */
  int nSlot;               /* Number of entries in a[] */
  WhereTerm *a;            /* Each a[] describes a term of the WHERE cluase */
  WhereTerm aStatic[10];   /* Initial static space for a[] */
};

/*
** An instance of the following structure keeps track of a mapping
** between VDBE cursor numbers and bits of the bitmasks in WhereTerm.
**
** The VDBE cursor numbers are small integers contained in 
** SrcList_item.iCursor and Expr.iTable fields.  For any given WHERE 
** clause, the cursor numbers might not begin with 0 and they might
** contain gaps in the numbering sequence.  But we want to make maximum
** use of the bits in our bitmasks.  This structure provides a mapping
** from the sparse cursor numbers into consecutive integers beginning
** with 0.
**
** If ExprMaskSet.ix[A]==B it means that The A-th bit of a Bitmask
** corresponds VDBE cursor number B.  The A-th bit of a bitmask is 1<<A.
**
** For example, if the WHERE clause expression used these VDBE
** cursors:  4, 5, 8, 29, 57, 73.  Then the  ExprMaskSet structure
** would map those cursor numbers into bits 0 through 5.
**
** Note that the mapping is not necessarily ordered.  In the example
** above, the mapping might go like this:  4->3, 5->1, 8->2, 29->0,
** 57->5, 73->4.  Or one of 719 other combinations might be used. It
** does not really matter.  What is important is that sparse cursor
** numbers all get mapped into bit numbers that begin with 0 and contain
** no gaps.
*/
struct ExprMaskSet {
  int n;                        /* Number of assigned cursor values */
  int ix[sizeof(Bitmask)*8];    /* Cursor assigned to each bit */
};


/*
** Bitmasks for the operators that indices are able to exploit.  An
** OR-ed combination of these values can be used when searching for
** terms in the where clause.
*/
#define WO_IN     1
#define WO_EQ     2
#define WO_LT     (WO_EQ<<(TK_LT-TK_EQ))
#define WO_LE     (WO_EQ<<(TK_LE-TK_EQ))
#define WO_GT     (WO_EQ<<(TK_GT-TK_EQ))
#define WO_GE     (WO_EQ<<(TK_GE-TK_EQ))
#define WO_MATCH  64
#define WO_ISNULL 128

/*
** Value for flags returned by bestIndex().  
**
** The least significant byte is reserved as a mask for WO_ values above.
** The WhereLevel.flags field is usually set to WO_IN|WO_EQ|WO_ISNULL.
** But if the table is the right table of a left join, WhereLevel.flags
** is set to WO_IN|WO_EQ.  The WhereLevel.flags field can then be used as
** the "op" parameter to findTerm when we are resolving equality constraints.
** ISNULL constraints will then not be used on the right table of a left
** join.  Tickets #2177 and #2189.
*/
#define WHERE_ROWID_EQ     0x000100   /* rowid=EXPR or rowid IN (...) */
#define WHERE_ROWID_RANGE  0x000200   /* rowid<EXPR and/or rowid>EXPR */
#define WHERE_COLUMN_EQ    0x001000   /* x=EXPR or x IN (...) */
#define WHERE_COLUMN_RANGE 0x002000   /* x<EXPR and/or x>EXPR */
#define WHERE_COLUMN_IN    0x004000   /* x IN (...) */
#define WHERE_TOP_LIMIT    0x010000   /* x<EXPR or x<=EXPR constraint */
#define WHERE_BTM_LIMIT    0x020000   /* x>EXPR or x>=EXPR constraint */
#define WHERE_IDX_ONLY     0x080000   /* Use index only - omit table */
#define WHERE_ORDERBY      0x100000   /* Output will appear in correct order */
#define WHERE_REVERSE      0x200000   /* Scan in reverse order */
#define WHERE_UNIQUE       0x400000   /* Selects no more than one row */
#define WHERE_VIRTUALTABLE 0x800000   /* Use virtual-table processing */

/*
** Initialize a preallocated WhereClause structure.
*/
static void whereClauseInit(
  WhereClause *pWC,        /* The WhereClause to be initialized */
  Parse *pParse,           /* The parsing context */
  ExprMaskSet *pMaskSet    /* Mapping from table indices to bitmasks */
){
  pWC->pParse = pParse;
  pWC->pMaskSet = pMaskSet;
  pWC->nTerm = 0;
  pWC->nSlot = ArraySize(pWC->aStatic);
  pWC->a = pWC->aStatic;
}

/*
** Deallocate a WhereClause structure.  The WhereClause structure
** itself is not freed.  This routine is the inverse of whereClauseInit().
*/
static void whereClauseClear(WhereClause *pWC){
  int i;
  WhereTerm *a;
  sqlite3 *db = pWC->pParse->db;
  for(i=pWC->nTerm-1, a=pWC->a; i>=0; i--, a++){
    if( a->flags & TERM_DYNAMIC ){
      sqlite3ExprDelete(db, a->pExpr);
    }
  }
  if( pWC->a!=pWC->aStatic ){
    sqlite3DbFree(db, pWC->a);
  }
}

/*
** Add a new entries to the WhereClause structure.  Increase the allocated
** space as necessary.
**
** If the flags argument includes TERM_DYNAMIC, then responsibility
** for freeing the expression p is assumed by the WhereClause object.
**
** WARNING:  This routine might reallocate the space used to store
** WhereTerms.  All pointers to WhereTerms should be invalidated after
** calling this routine.  Such pointers may be reinitialized by referencing
** the pWC->a[] array.
*/
static int whereClauseInsert(WhereClause *pWC, Expr *p, int flags){
  WhereTerm *pTerm;
  int idx;
  if( pWC->nTerm>=pWC->nSlot ){
    WhereTerm *pOld = pWC->a;
    sqlite3 *db = pWC->pParse->db;
    pWC->a = sqlite3DbMallocRaw(db, sizeof(pWC->a[0])*pWC->nSlot*2 );
    if( pWC->a==0 ){
      if( flags & TERM_DYNAMIC ){
        sqlite3ExprDelete(db, p);
      }
      pWC->a = pOld;
      return 0;
    }
    memcpy(pWC->a, pOld, sizeof(pWC->a[0])*pWC->nTerm);
    if( pOld!=pWC->aStatic ){
      sqlite3DbFree(db, pOld);
    }
    pWC->nSlot *= 2;
  }
  pTerm = &pWC->a[idx = pWC->nTerm];
  pWC->nTerm++;
  pTerm->pExpr = p;
  pTerm->flags = flags;
  pTerm->pWC = pWC;
  pTerm->iParent = -1;
  return idx;
}

/*
** This routine identifies subexpressions in the WHERE clause where
** each subexpression is separated by the AND operator or some other
** operator specified in the op parameter.  The WhereClause structure
** is filled with pointers to subexpressions.  For example:
**
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
**
** The original WHERE clause in pExpr is unaltered.  All this routine
** does is make slot[] entries point to substructure within pExpr.
**
** In the previous sentence and in the diagram, "slot[]" refers to
** the WhereClause.a[] array.  This array grows as needed to contain
** all terms of the WHERE clause.
*/
static void whereSplit(WhereClause *pWC, Expr *pExpr, int op){
  if( pExpr==0 ) return;
  if( pExpr->op!=op ){
    whereClauseInsert(pWC, pExpr, 0);
  }else{
    whereSplit(pWC, pExpr->pLeft, op);
    whereSplit(pWC, pExpr->pRight, op);
  }
}

/*
** Initialize an expression mask set
*/
#define initMaskSet(P)  memset(P, 0, sizeof(*P))

/*
** Return the bitmask for the given cursor number.  Return 0 if
** iCursor is not in the set.
*/
static Bitmask getMask(ExprMaskSet *pMaskSet, int iCursor){
  int i;
  for(i=0; i<pMaskSet->n; i++){
    if( pMaskSet->ix[i]==iCursor ){
      return ((Bitmask)1)<<i;
    }
  }
  return 0;
}

/*
** Create a new mask for cursor iCursor.
**
** There is one cursor per table in the FROM clause.  The number of
** tables in the FROM clause is limited by a test early in the
** sqlite3WhereBegin() routine.  So we know that the pMaskSet->ix[]
** array will never overflow.
*/
static void createMask(ExprMaskSet *pMaskSet, int iCursor){
  assert( pMaskSet->n < ArraySize(pMaskSet->ix) );
  pMaskSet->ix[pMaskSet->n++] = iCursor;
}

/*
** This routine walks (recursively) an expression tree and generates
** a bitmask indicating which tables are used in that expression
** tree.
**
** In order for this routine to work, the calling function must have
** previously invoked sqlite3ExprResolveNames() on the expression.  See
** the header comment on that routine for additional information.
** The sqlite3ExprResolveNames() routines looks for column names and
** sets their opcodes to TK_COLUMN and their Expr.iTable fields to
** the VDBE cursor number of the table.  This routine just has to
** translate the cursor numbers into bitmask values and OR all
** the bitmasks together.
*/
static Bitmask exprListTableUsage(ExprMaskSet*, ExprList*);
static Bitmask exprSelectTableUsage(ExprMaskSet*, Select*);
static Bitmask exprTableUsage(ExprMaskSet *pMaskSet, Expr *p){
  Bitmask mask = 0;
  if( p==0 ) return 0;
  if( p->op==TK_COLUMN ){
    mask = getMask(pMaskSet, p->iTable);
    return mask;
  }
  mask = exprTableUsage(pMaskSet, p->pRight);
  mask |= exprTableUsage(pMaskSet, p->pLeft);
  mask |= exprListTableUsage(pMaskSet, p->pList);
  mask |= exprSelectTableUsage(pMaskSet, p->pSelect);
  return mask;
}
static Bitmask exprListTableUsage(ExprMaskSet *pMaskSet, ExprList *pList){
  int i;
  Bitmask mask = 0;
  if( pList ){
    for(i=0; i<pList->nExpr; i++){
      mask |= exprTableUsage(pMaskSet, pList->a[i].pExpr);
    }
  }
  return mask;
}
static Bitmask exprSelectTableUsage(ExprMaskSet *pMaskSet, Select *pS){
  Bitmask mask = 0;
  while( pS ){
    mask |= exprListTableUsage(pMaskSet, pS->pEList);
    mask |= exprListTableUsage(pMaskSet, pS->pGroupBy);
    mask |= exprListTableUsage(pMaskSet, pS->pOrderBy);
    mask |= exprTableUsage(pMaskSet, pS->pWhere);
    mask |= exprTableUsage(pMaskSet, pS->pHaving);
    pS = pS->pPrior;
  }
  return mask;
}

/*
** Return TRUE if the given operator is one of the operators that is
** allowed for an indexable WHERE clause term.  The allowed operators are
** "=", "<", ">", "<=", ">=", and "IN".
*/
static int allowedOp(int op){
  assert( TK_GT>TK_EQ && TK_GT<TK_GE );
  assert( TK_LT>TK_EQ && TK_LT<TK_GE );
  assert( TK_LE>TK_EQ && TK_LE<TK_GE );
  assert( TK_GE==TK_EQ+4 );
  return op==TK_IN || (op>=TK_EQ && op<=TK_GE) || op==TK_ISNULL;
}

/*
** Swap two objects of type T.
*/
#define SWAP(TYPE,A,B) {TYPE t=A; A=B; B=t;}

/*
** Commute a comparison operator.  Expressions of the form "X op Y"
** are converted into "Y op X".
**
** If a collation sequence is associated with either the left or right
** side of the comparison, it remains associated with the same side after
** the commutation. So "Y collate NOCASE op X" becomes 
** "X collate NOCASE op Y". This is because any collation sequence on
** the left hand side of a comparison overrides any collation sequence 
** attached to the right. For the same reason the EP_ExpCollate flag
** is not commuted.
*/
static void exprCommute(Expr *pExpr){
  u16 expRight = (pExpr->pRight->flags & EP_ExpCollate);
  u16 expLeft = (pExpr->pLeft->flags & EP_ExpCollate);
  assert( allowedOp(pExpr->op) && pExpr->op!=TK_IN );
  SWAP(CollSeq*,pExpr->pRight->pColl,pExpr->pLeft->pColl);
  pExpr->pRight->flags = (pExpr->pRight->flags & ~EP_ExpCollate) | expLeft;
  pExpr->pLeft->flags = (pExpr->pLeft->flags & ~EP_ExpCollate) | expRight;
  SWAP(Expr*,pExpr->pRight,pExpr->pLeft);
  if( pExpr->op>=TK_GT ){
    assert( TK_LT==TK_GT+2 );
    assert( TK_GE==TK_LE+2 );
    assert( TK_GT>TK_EQ );
    assert( TK_GT<TK_LE );
    assert( pExpr->op>=TK_GT && pExpr->op<=TK_GE );
    pExpr->op = ((pExpr->op-TK_GT)^2)+TK_GT;
  }
}

/*
** Translate from TK_xx operator to WO_xx bitmask.
*/
static int operatorMask(int op){
  int c;
  assert( allowedOp(op) );
  if( op==TK_IN ){
    c = WO_IN;
  }else if( op==TK_ISNULL ){
    c = WO_ISNULL;
  }else{
    c = WO_EQ<<(op-TK_EQ);
  }
  assert( op!=TK_ISNULL || c==WO_ISNULL );
  assert( op!=TK_IN || c==WO_IN );
  assert( op!=TK_EQ || c==WO_EQ );
  assert( op!=TK_LT || c==WO_LT );
  assert( op!=TK_LE || c==WO_LE );
  assert( op!=TK_GT || c==WO_GT );
  assert( op!=TK_GE || c==WO_GE );
  return c;
}

/*
** Search for a term in the WHERE clause that is of the form "X <op> <expr>"
** where X is a reference to the iColumn of table iCur and <op> is one of
** the WO_xx operator codes specified by the op parameter.
** Return a pointer to the term.  Return 0 if not found.
*/
static WhereTerm *findTerm(
  WhereClause *pWC,     /* The WHERE clause to be searched */
  int iCur,             /* Cursor number of LHS */
  int iColumn,          /* Column number of LHS */
  Bitmask notReady,     /* RHS must not overlap with this mask */
  u16 op,               /* Mask of WO_xx values describing operator */
  Index *pIdx           /* Must be compatible with this index, if not NULL */
){
  WhereTerm *pTerm;
  int k;
  assert( iCur>=0 );
  for(pTerm=pWC->a, k=pWC->nTerm; k; k--, pTerm++){
    if( pTerm->leftCursor==iCur
       && (pTerm->prereqRight & notReady)==0
       && pTerm->leftColumn==iColumn
       && (pTerm->eOperator & op)!=0
    ){
      if( pIdx && pTerm->eOperator!=WO_ISNULL ){
        Expr *pX = pTerm->pExpr;
        CollSeq *pColl;
        char idxaff;
        int j;
        Parse *pParse = pWC->pParse;

        idxaff = pIdx->pTable->aCol[iColumn].affinity;
        if( !sqlite3IndexAffinityOk(pX, idxaff) ) continue;

        /* Figure out the collation sequence required from an index for
        ** it to be useful for optimising expression pX. Store this
        ** value in variable pColl.
        */
        assert(pX->pLeft);
        pColl = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);
        if( !pColl ){
          pColl = pParse->db->pDfltColl;
        }

        for(j=0; pIdx->aiColumn[j]!=iColumn; j++){
          if( NEVER(j>=pIdx->nColumn) ) return 0;
        }
        if( sqlite3StrICmp(pColl->zName, pIdx->azColl[j]) ) continue;
      }
      return pTerm;
    }
  }
  return 0;
}

/* Forward reference */
static void exprAnalyze(SrcList*, WhereClause*, int);

/*
** Call exprAnalyze on all terms in a WHERE clause.  
**
**
*/
static void exprAnalyzeAll(
  SrcList *pTabList,       /* the FROM clause */
  WhereClause *pWC         /* the WHERE clause to be analyzed */
){
  int i;
  for(i=pWC->nTerm-1; i>=0; i--){
    exprAnalyze(pTabList, pWC, i);
  }
}

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
/*
** Check to see if the given expression is a LIKE or GLOB operator that
** can be optimized using inequality constraints.  Return TRUE if it is
** so and false if not.
**
** In order for the operator to be optimizible, the RHS must be a string
** literal that does not begin with a wildcard.  
*/
static int isLikeOrGlob(
  sqlite3 *db,      /* The database */
  Expr *pExpr,      /* Test this expression */
  int *pnPattern,   /* Number of non-wildcard prefix characters */
  int *pisComplete, /* True if the only wildcard is % in the last character */
  int *pnoCase      /* True if uppercase is equivalent to lowercase */
){
  const char *z;
  Expr *pRight, *pLeft;
  ExprList *pList;
  int c, cnt;
  char wc[3];
  CollSeq *pColl;

  if( !sqlite3IsLikeFunction(db, pExpr, pnoCase, wc) ){
    return 0;
  }
#ifdef SQLITE_EBCDIC
  if( *pnoCase ) return 0;
#endif
  pList = pExpr->pList;
  pRight = pList->a[0].pExpr;
  if( pRight->op!=TK_STRING
   && (pRight->op!=TK_REGISTER || pRight->iColumn!=TK_STRING) ){
    return 0;
  }
  pLeft = pList->a[1].pExpr;
  if( pLeft->op!=TK_COLUMN ){
    return 0;
  }
  pColl = pLeft->pColl;
  assert( pColl!=0 || pLeft->iColumn==-1 );
  if( pColl==0 ){
    /* No collation is defined for the ROWID.  Use the default. */
    pColl = db->pDfltColl;
  }
  if( (pColl->type!=SQLITE_COLL_BINARY || *pnoCase) &&
      (pColl->type!=SQLITE_COLL_NOCASE || !*pnoCase) ){
    return 0;
  }
  sqlite3DequoteExpr(db, pRight);
  z = (char *)pRight->token.z;
  cnt = 0;
  if( z ){
    while( (c=z[cnt])!=0 && c!=wc[0] && c!=wc[1] && c!=wc[2] ){ cnt++; }
  }
  if( cnt==0 || 255==(u8)z[cnt] ){
    return 0;
  }
  *pisComplete = z[cnt]==wc[0] && z[cnt+1]==0;
  *pnPattern = cnt;
  return 1;
}
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */


#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Check to see if the given expression is of the form
**
**         column MATCH expr
**
** If it is then return TRUE.  If not, return FALSE.
*/
static int isMatchOfColumn(
  Expr *pExpr      /* Test this expression */
){
  ExprList *pList;

  if( pExpr->op!=TK_FUNCTION ){
    return 0;
  }
  if( pExpr->token.n!=5 ||
       sqlite3StrNICmp((const char*)pExpr->token.z,"match",5)!=0 ){
    return 0;
  }
  pList = pExpr->pList;
  if( pList->nExpr!=2 ){
    return 0;
  }
  if( pList->a[1].pExpr->op != TK_COLUMN ){
    return 0;
  }
  return 1;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** If the pBase expression originated in the ON or USING clause of
** a join, then transfer the appropriate markings over to derived.
*/
static void transferJoinMarkings(Expr *pDerived, Expr *pBase){
  pDerived->flags |= pBase->flags & EP_FromJoin;
  pDerived->iRightJoinTable = pBase->iRightJoinTable;
}

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
/*
** Return TRUE if the given term of an OR clause can be converted
** into an IN clause.  The iCursor and iColumn define the left-hand
** side of the IN clause.
**
** The context is that we have multiple OR-connected equality terms
** like this:
**
**           a=<expr1> OR  a=<expr2> OR b=<expr3>  OR ...
**
** The pOrTerm input to this routine corresponds to a single term of
** this OR clause.  In order for the term to be a candidate for
** conversion to an IN operator, the following must be true:
**
**     *  The left-hand side of the term must be the column which
**        is identified by iCursor and iColumn.
**
**     *  If the right-hand side is also a column, then the affinities
**        of both right and left sides must be such that no type
**        conversions are required on the right.  (Ticket #2249)
**
** If both of these conditions are true, then return true.  Otherwise
** return false.
*/
static int orTermIsOptCandidate(WhereTerm *pOrTerm, int iCursor, int iColumn){
  int affLeft, affRight;
  assert( pOrTerm->eOperator==WO_EQ );
  if( pOrTerm->leftCursor!=iCursor ){
    return 0;
  }
  if( pOrTerm->leftColumn!=iColumn ){
    return 0;
  }
  affRight = sqlite3ExprAffinity(pOrTerm->pExpr->pRight);
  if( affRight==0 ){
    return 1;
  }
  affLeft = sqlite3ExprAffinity(pOrTerm->pExpr->pLeft);
  if( affRight!=affLeft ){
    return 0;
  }
  return 1;
}

/*
** Return true if the given term of an OR clause can be ignored during
** a check to make sure all OR terms are candidates for optimization.
** In other words, return true if a call to the orTermIsOptCandidate()
** above returned false but it is not necessary to disqualify the
** optimization.
**
** Suppose the original OR phrase was this:
**
**           a=4  OR  a=11  OR  a=b
**
** During analysis, the third term gets flipped around and duplicate
** so that we are left with this:
**
**           a=4  OR  a=11  OR  a=b  OR  b=a
**
** Since the last two terms are duplicates, only one of them
** has to qualify in order for the whole phrase to qualify.  When
** this routine is called, we know that pOrTerm did not qualify.
** This routine merely checks to see if pOrTerm has a duplicate that
** might qualify.  If there is a duplicate that has not yet been
** disqualified, then return true.  If there are no duplicates, or
** the duplicate has also been disqualified, return false.
*/
static int orTermHasOkDuplicate(WhereClause *pOr, WhereTerm *pOrTerm){
  if( pOrTerm->flags & TERM_COPIED ){
    /* This is the original term.  The duplicate is to the left had
    ** has not yet been analyzed and thus has not yet been disqualified. */
    return 1;
  }
  if( (pOrTerm->flags & TERM_VIRTUAL)!=0
     && (pOr->a[pOrTerm->iParent].flags & TERM_OR_OK)!=0 ){
    /* This is a duplicate term.  The original qualified so this one
    ** does not have to. */
    return 1;
  }
  /* This is either a singleton term or else it is a duplicate for
  ** which the original did not qualify.  Either way we are done for. */
  return 0;
}
#endif /* !SQLITE_OMIT_OR_OPTIMIZATION && !SQLITE_OMIT_SUBQUERY */

/*
** The input to this routine is an WhereTerm structure with only the
** "pExpr" field filled in.  The job of this routine is to analyze the
** subexpression and populate all the other fields of the WhereTerm
** structure.
**
** If the expression is of the form "<expr> <op> X" it gets commuted
** to the standard form of "X <op> <expr>".  If the expression is of
** the form "X <op> Y" where both X and Y are columns, then the original
** expression is unchanged and a new virtual expression of the form
** "Y <op> X" is added to the WHERE clause and analyzed separately.
*/
static void exprAnalyze(
  SrcList *pSrc,            /* the FROM clause */
  WhereClause *pWC,         /* the WHERE clause */
  int idxTerm               /* Index of the term to be analyzed */
){
  WhereTerm *pTerm;
  ExprMaskSet *pMaskSet;
  Expr *pExpr;
  Bitmask prereqLeft;
  Bitmask prereqAll;
  Bitmask extraRight = 0;
  int nPattern;
  int isComplete;
  int noCase;
  int op;
  Parse *pParse = pWC->pParse;
  sqlite3 *db = pParse->db;

  if( db->mallocFailed ){
    return;
  }
  pTerm = &pWC->a[idxTerm];
  pMaskSet = pWC->pMaskSet;
  pExpr = pTerm->pExpr;
  prereqLeft = exprTableUsage(pMaskSet, pExpr->pLeft);
  op = pExpr->op;
  if( op==TK_IN ){
    assert( pExpr->pRight==0 );
    pTerm->prereqRight = exprListTableUsage(pMaskSet, pExpr->pList)
                          | exprSelectTableUsage(pMaskSet, pExpr->pSelect);
  }else if( op==TK_ISNULL ){
    pTerm->prereqRight = 0;
  }else{
    pTerm->prereqRight = exprTableUsage(pMaskSet, pExpr->pRight);
  }
  prereqAll = exprTableUsage(pMaskSet, pExpr);
  if( ExprHasProperty(pExpr, EP_FromJoin) ){
    Bitmask x = getMask(pMaskSet, pExpr->iRightJoinTable);
    prereqAll |= x;
    extraRight = x-1;  /* ON clause terms may not be used with an index
                       ** on left table of a LEFT JOIN.  Ticket #3015 */
  }
  pTerm->prereqAll = prereqAll;
  pTerm->leftCursor = -1;
  pTerm->iParent = -1;
  pTerm->eOperator = 0;
  if( allowedOp(op) && (pTerm->prereqRight & prereqLeft)==0 ){
    Expr *pLeft = pExpr->pLeft;
    Expr *pRight = pExpr->pRight;
    if( pLeft->op==TK_COLUMN ){
      pTerm->leftCursor = pLeft->iTable;
      pTerm->leftColumn = pLeft->iColumn;
      pTerm->eOperator = operatorMask(op);
    }
    if( pRight && pRight->op==TK_COLUMN ){
      WhereTerm *pNew;
      Expr *pDup;
      if( pTerm->leftCursor>=0 ){
        int idxNew;
        pDup = sqlite3ExprDup(db, pExpr);
        if( db->mallocFailed ){
          sqlite3ExprDelete(db, pDup);
          return;
        }
        idxNew = whereClauseInsert(pWC, pDup, TERM_VIRTUAL|TERM_DYNAMIC);
        if( idxNew==0 ) return;
        pNew = &pWC->a[idxNew];
        pNew->iParent = idxTerm;
        pTerm = &pWC->a[idxTerm];
        pTerm->nChild = 1;
        pTerm->flags |= TERM_COPIED;
      }else{
        pDup = pExpr;
        pNew = pTerm;
      }
      exprCommute(pDup);
      pLeft = pDup->pLeft;
      pNew->leftCursor = pLeft->iTable;
      pNew->leftColumn = pLeft->iColumn;
      pNew->prereqRight = prereqLeft;
      pNew->prereqAll = prereqAll;
      pNew->eOperator = operatorMask(pDup->op);
    }
  }

#ifndef SQLITE_OMIT_BETWEEN_OPTIMIZATION
  /* If a term is the BETWEEN operator, create two new virtual terms
  ** that define the range that the BETWEEN implements.
  */
  else if( pExpr->op==TK_BETWEEN ){
    ExprList *pList = pExpr->pList;
    int i;
    static const u8 ops[] = {TK_GE, TK_LE};
    assert( pList!=0 );
    assert( pList->nExpr==2 );
    for(i=0; i<2; i++){
      Expr *pNewExpr;
      int idxNew;
      pNewExpr = sqlite3Expr(db, ops[i], sqlite3ExprDup(db, pExpr->pLeft),
                             sqlite3ExprDup(db, pList->a[i].pExpr), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
      exprAnalyze(pSrc, pWC, idxNew);
      pTerm = &pWC->a[idxTerm];
      pWC->a[idxNew].iParent = idxTerm;
    }
    pTerm->nChild = 2;
  }
#endif /* SQLITE_OMIT_BETWEEN_OPTIMIZATION */

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
  /* Attempt to convert OR-connected terms into an IN operator so that
  ** they can make use of indices.  Example:
  **
  **      x = expr1  OR  expr2 = x  OR  x = expr3
  **
  ** is converted into
  **
  **      x IN (expr1,expr2,expr3)
  **
  ** This optimization must be omitted if OMIT_SUBQUERY is defined because
  ** the compiler for the the IN operator is part of sub-queries.
  */
  else if( pExpr->op==TK_OR ){
    int ok;
    int i, j;
    int iColumn, iCursor;
    WhereClause sOr;
    WhereTerm *pOrTerm;

    assert( (pTerm->flags & TERM_DYNAMIC)==0 );
    whereClauseInit(&sOr, pWC->pParse, pMaskSet);
    whereSplit(&sOr, pExpr, TK_OR);
    exprAnalyzeAll(pSrc, &sOr);
    assert( sOr.nTerm>=2 );
    j = 0;
    if( db->mallocFailed ) goto or_not_possible;
    do{
      assert( j<sOr.nTerm );
      iColumn = sOr.a[j].leftColumn;
      iCursor = sOr.a[j].leftCursor;
      ok = iCursor>=0;
      for(i=sOr.nTerm-1, pOrTerm=sOr.a; i>=0 && ok; i--, pOrTerm++){
        if( pOrTerm->eOperator!=WO_EQ ){
          goto or_not_possible;
        }
        if( orTermIsOptCandidate(pOrTerm, iCursor, iColumn) ){
          pOrTerm->flags |= TERM_OR_OK;
        }else if( orTermHasOkDuplicate(&sOr, pOrTerm) ){
          pOrTerm->flags &= ~TERM_OR_OK;
        }else{
          ok = 0;
        }
      }
    }while( !ok && (sOr.a[j++].flags & TERM_COPIED)!=0 && j<2 );
    if( ok ){
      ExprList *pList = 0;
      Expr *pNew, *pDup;
      Expr *pLeft = 0;
      for(i=sOr.nTerm-1, pOrTerm=sOr.a; i>=0; i--, pOrTerm++){
        if( (pOrTerm->flags & TERM_OR_OK)==0 ) continue;
        pDup = sqlite3ExprDup(db, pOrTerm->pExpr->pRight);
        pList = sqlite3ExprListAppend(pWC->pParse, pList, pDup, 0);
        pLeft = pOrTerm->pExpr->pLeft;
      }
      assert( pLeft!=0 );
      pDup = sqlite3ExprDup(db, pLeft);
      pNew = sqlite3Expr(db, TK_IN, pDup, 0, 0);
      if( pNew ){
        int idxNew;
        transferJoinMarkings(pNew, pExpr);
        pNew->pList = pList;
        idxNew = whereClauseInsert(pWC, pNew, TERM_VIRTUAL|TERM_DYNAMIC);
        exprAnalyze(pSrc, pWC, idxNew);
        pTerm = &pWC->a[idxTerm];
        pWC->a[idxNew].iParent = idxTerm;
        pTerm->nChild = 1;
      }else{
        sqlite3ExprListDelete(db, pList);
      }
    }
or_not_possible:
    whereClauseClear(&sOr);
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
  /* Add constraints to reduce the search space on a LIKE or GLOB
  ** operator.
  **
  ** A like pattern of the form "x LIKE 'abc%'" is changed into constraints
  **
  **          x>='abc' AND x<'abd' AND x LIKE 'abc%'
  **
  ** The last character of the prefix "abc" is incremented to form the
  ** termination condition "abd".
  */
  if( isLikeOrGlob(db, pExpr, &nPattern, &isComplete, &noCase) ){
    Expr *pLeft, *pRight;
    Expr *pStr1, *pStr2;
    Expr *pNewExpr1, *pNewExpr2;
    int idxNew1, idxNew2;

    pLeft = pExpr->pList->a[1].pExpr;
    pRight = pExpr->pList->a[0].pExpr;
    pStr1 = sqlite3PExpr(pParse, TK_STRING, 0, 0, 0);
    if( pStr1 ){
      sqlite3TokenCopy(db, &pStr1->token, &pRight->token);
      pStr1->token.n = nPattern;
      pStr1->flags = EP_Dequoted;
    }
    pStr2 = sqlite3ExprDup(db, pStr1);
    if( !db->mallocFailed ){
      u8 c, *pC;
      assert( pStr2->token.dyn );
      pC = (u8*)&pStr2->token.z[nPattern-1];
      c = *pC;
      if( noCase ){
        if( c=='@' ) isComplete = 0;
        c = sqlite3UpperToLower[c];
      }
      *pC = c + 1;
    }
    pNewExpr1 = sqlite3PExpr(pParse, TK_GE, sqlite3ExprDup(db,pLeft), pStr1, 0);
    idxNew1 = whereClauseInsert(pWC, pNewExpr1, TERM_VIRTUAL|TERM_DYNAMIC);
    exprAnalyze(pSrc, pWC, idxNew1);
    pNewExpr2 = sqlite3PExpr(pParse, TK_LT, sqlite3ExprDup(db,pLeft), pStr2, 0);
    idxNew2 = whereClauseInsert(pWC, pNewExpr2, TERM_VIRTUAL|TERM_DYNAMIC);
    exprAnalyze(pSrc, pWC, idxNew2);
    pTerm = &pWC->a[idxTerm];
    if( isComplete ){
      pWC->a[idxNew1].iParent = idxTerm;
      pWC->a[idxNew2].iParent = idxTerm;
      pTerm->nChild = 2;
    }
  }
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */

#ifndef SQLITE_OMIT_VIRTUALTABLE
  /* Add a WO_MATCH auxiliary term to the constraint set if the
  ** current expression is of the form:  column MATCH expr.
  ** This information is used by the xBestIndex methods of
  ** virtual tables.  The native query optimizer does not attempt
  ** to do anything with MATCH functions.
  */
  if( isMatchOfColumn(pExpr) ){
    int idxNew;
    Expr *pRight, *pLeft;
    WhereTerm *pNewTerm;
    Bitmask prereqColumn, prereqExpr;

    pRight = pExpr->pList->a[0].pExpr;
    pLeft = pExpr->pList->a[1].pExpr;
    prereqExpr = exprTableUsage(pMaskSet, pRight);
    prereqColumn = exprTableUsage(pMaskSet, pLeft);
    if( (prereqExpr & prereqColumn)==0 ){
      Expr *pNewExpr;
      pNewExpr = sqlite3Expr(db, TK_MATCH, 0, sqlite3ExprDup(db, pRight), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
      pNewTerm = &pWC->a[idxNew];
      pNewTerm->prereqRight = prereqExpr;
      pNewTerm->leftCursor = pLeft->iTable;
      pNewTerm->leftColumn = pLeft->iColumn;
      pNewTerm->eOperator = WO_MATCH;
      pNewTerm->iParent = idxTerm;
      pTerm = &pWC->a[idxTerm];
      pTerm->nChild = 1;
      pTerm->flags |= TERM_COPIED;
      pNewTerm->prereqAll = pTerm->prereqAll;
    }
  }
#endif /* SQLITE_OMIT_VIRTUALTABLE */

  /* Prevent ON clause terms of a LEFT JOIN from being used to drive
  ** an index for tables to the left of the join.
  */
  pTerm->prereqRight |= extraRight;
}

/*
** Return TRUE if any of the expressions in pList->a[iFirst...] contain
** a reference to any table other than the iBase table.
*/
static int referencesOtherTables(
  ExprList *pList,          /* Search expressions in ths list */
  ExprMaskSet *pMaskSet,    /* Mapping from tables to bitmaps */
  int iFirst,               /* Be searching with the iFirst-th expression */
  int iBase                 /* Ignore references to this table */
){
  Bitmask allowed = ~getMask(pMaskSet, iBase);
  while( iFirst<pList->nExpr ){
    if( (exprTableUsage(pMaskSet, pList->a[iFirst++].pExpr)&allowed)!=0 ){
      return 1;
    }
  }
  return 0;
}


/*
** This routine decides if pIdx can be used to satisfy the ORDER BY
** clause.  If it can, it returns 1.  If pIdx cannot satisfy the
** ORDER BY clause, this routine returns 0.
**
** pOrderBy is an ORDER BY clause from a SELECT statement.  pTab is the
** left-most table in the FROM clause of that same SELECT statement and
** the table has a cursor number of "base".  pIdx is an index on pTab.
**
** nEqCol is the number of columns of pIdx that are used as equality
** constraints.  Any of these columns may be missing from the ORDER BY
** clause and the match can still be a success.
**
** All terms of the ORDER BY that match against the index must be either
** ASC or DESC.  (Terms of the ORDER BY clause past the end of a UNIQUE
** index do not need to satisfy this constraint.)  The *pbRev value is
** set to 1 if the ORDER BY clause is all DESC and it is set to 0 if
** the ORDER BY clause is all ASC.
*/
static int isSortingIndex(
  Parse *pParse,          /* Parsing context */
  ExprMaskSet *pMaskSet,  /* Mapping from table indices to bitmaps */
  Index *pIdx,            /* The index we are testing */
  int base,               /* Cursor number for the table to be sorted */
  ExprList *pOrderBy,     /* The ORDER BY clause */
  int nEqCol,             /* Number of index columns with == constraints */
  int *pbRev              /* Set to 1 if ORDER BY is DESC */
){
  int i, j;                       /* Loop counters */
  int sortOrder = 0;              /* XOR of index and ORDER BY sort direction */
  int nTerm;                      /* Number of ORDER BY terms */
  struct ExprList_item *pTerm;    /* A term of the ORDER BY clause */
  sqlite3 *db = pParse->db;

  assert( pOrderBy!=0 );
  nTerm = pOrderBy->nExpr;
  assert( nTerm>0 );

  /* Match terms of the ORDER BY clause against columns of
  ** the index.
  **
  ** Note that indices have pIdx->nColumn regular columns plus
  ** one additional column containing the rowid.  The rowid column
  ** of the index is also allowed to match against the ORDER BY
  ** clause.
  */
  for(i=j=0, pTerm=pOrderBy->a; j<nTerm && i<=pIdx->nColumn; i++){
    Expr *pExpr;       /* The expression of the ORDER BY pTerm */
    CollSeq *pColl;    /* The collating sequence of pExpr */
    int termSortOrder; /* Sort order for this term */
    int iColumn;       /* The i-th column of the index.  -1 for rowid */
    int iSortOrder;    /* 1 for DESC, 0 for ASC on the i-th index term */
    const char *zColl; /* Name of the collating sequence for i-th index term */

    pExpr = pTerm->pExpr;
    if( pExpr->op!=TK_COLUMN || pExpr->iTable!=base ){
      /* Can not use an index sort on anything that is not a column in the
      ** left-most table of the FROM clause */
      break;
    }
    pColl = sqlite3ExprCollSeq(pParse, pExpr);
    if( !pColl ){
      pColl = db->pDfltColl;
    }
    if( i<pIdx->nColumn ){
      iColumn = pIdx->aiColumn[i];
      if( iColumn==pIdx->pTable->iPKey ){
        iColumn = -1;
      }
      iSortOrder = pIdx->aSortOrder[i];
      zColl = pIdx->azColl[i];
    }else{
      iColumn = -1;
      iSortOrder = 0;
      zColl = pColl->zName;
    }
    if( pExpr->iColumn!=iColumn || sqlite3StrICmp(pColl->zName, zColl) ){
      /* Term j of the ORDER BY clause does not match column i of the index */
      if( i<nEqCol ){
        /* If an index column that is constrained by == fails to match an
        ** ORDER BY term, that is OK.  Just ignore that column of the index
        */
        continue;
      }else if( i==pIdx->nColumn ){
        /* Index column i is the rowid.  All other terms match. */
        break;
      }else{
        /* If an index column fails to match and is not constrained by ==
        ** then the index cannot satisfy the ORDER BY constraint.
        */
        return 0;
      }
    }
    assert( pIdx->aSortOrder!=0 );
    assert( pTerm->sortOrder==0 || pTerm->sortOrder==1 );
    assert( iSortOrder==0 || iSortOrder==1 );
    termSortOrder = iSortOrder ^ pTerm->sortOrder;
    if( i>nEqCol ){
      if( termSortOrder!=sortOrder ){
        /* Indices can only be used if all ORDER BY terms past the
        ** equality constraints are all either DESC or ASC. */
        return 0;
      }
    }else{
      sortOrder = termSortOrder;
    }
    j++;
    pTerm++;
    if( iColumn<0 && !referencesOtherTables(pOrderBy, pMaskSet, j, base) ){
      /* If the indexed column is the primary key and everything matches
      ** so far and none of the ORDER BY terms to the right reference other
      ** tables in the join, then we are assured that the index can be used 
      ** to sort because the primary key is unique and so none of the other
      ** columns will make any difference
      */
      j = nTerm;
    }
  }

  *pbRev = sortOrder!=0;
  if( j>=nTerm ){
    /* All terms of the ORDER BY clause are covered by this index so
    ** this index can be used for sorting. */
    return 1;
  }
  if( pIdx->onError!=OE_None && i==pIdx->nColumn
      && !referencesOtherTables(pOrderBy, pMaskSet, j, base) ){
    /* All terms of this index match some prefix of the ORDER BY clause
    ** and the index is UNIQUE and no terms on the tail of the ORDER BY
    ** clause reference other tables in a join.  If this is all true then
    ** the order by clause is superfluous. */
    return 1;
  }
  return 0;
}

/*
** Check table to see if the ORDER BY clause in pOrderBy can be satisfied
** by sorting in order of ROWID.  Return true if so and set *pbRev to be
** true for reverse ROWID and false for forward ROWID order.
*/
static int sortableByRowid(
  int base,               /* Cursor number for table to be sorted */
  ExprList *pOrderBy,     /* The ORDER BY clause */
  ExprMaskSet *pMaskSet,  /* Mapping from tables to bitmaps */
  int *pbRev              /* Set to 1 if ORDER BY is DESC */
){
  Expr *p;

  assert( pOrderBy!=0 );
  assert( pOrderBy->nExpr>0 );
  p = pOrderBy->a[0].pExpr;
  if( p->op==TK_COLUMN && p->iTable==base && p->iColumn==-1
    && !referencesOtherTables(pOrderBy, pMaskSet, 1, base) ){
    *pbRev = pOrderBy->a[0].sortOrder;
    return 1;
  }
  return 0;
}

/*
** Prepare a crude estimate of the logarithm of the input value.
** The results need not be exact.  This is only used for estimating
** the total cost of performing operations with O(logN) or O(NlogN)
** complexity.  Because N is just a guess, it is no great tragedy if
** logN is a little off.
*/
static double estLog(double N){
  double logN = 1;
  double x = 10;
  while( N>x ){
    logN += 1;
    x *= 10;
  }
  return logN;
}

/*
** Two routines for printing the content of an sqlite3_index_info
** structure.  Used for testing and debugging only.  If neither
** SQLITE_TEST or SQLITE_DEBUG are defined, then these routines
** are no-ops.
*/
#if !defined(SQLITE_OMIT_VIRTUALTABLE) && defined(SQLITE_DEBUG)
static void TRACE_IDX_INPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  constraint[%d]: col=%d termid=%d op=%d usabled=%d\n",
       i,
       p->aConstraint[i].iColumn,
       p->aConstraint[i].iTermOffset,
       p->aConstraint[i].op,
       p->aConstraint[i].usable);
  }
  for(i=0; i<p->nOrderBy; i++){
    sqlite3DebugPrintf("  orderby[%d]: col=%d desc=%d\n",
       i,
       p->aOrderBy[i].iColumn,
       p->aOrderBy[i].desc);
  }
}
static void TRACE_IDX_OUTPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  usage[%d]: argvIdx=%d omit=%d\n",
       i,
       p->aConstraintUsage[i].argvIndex,
       p->aConstraintUsage[i].omit);
  }
  sqlite3DebugPrintf("  idxNum=%d\n", p->idxNum);
  sqlite3DebugPrintf("  idxStr=%s\n", p->idxStr);
  sqlite3DebugPrintf("  orderByConsumed=%d\n", p->orderByConsumed);
  sqlite3DebugPrintf("  estimatedCost=%g\n", p->estimatedCost);
}
#else
#define TRACE_IDX_INPUTS(A)
#define TRACE_IDX_OUTPUTS(A)
#endif

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Compute the best index for a virtual table.
**
** The best index is computed by the xBestIndex method of the virtual
** table module.  This routine is really just a wrapper that sets up
** the sqlite3_index_info structure that is used to communicate with
** xBestIndex.
**
** In a join, this routine might be called multiple times for the
** same virtual table.  The sqlite3_index_info structure is created
** and initialized on the first invocation and reused on all subsequent
** invocations.  The sqlite3_index_info structure is also used when
** code is generated to access the virtual table.  The whereInfoDelete() 
** routine takes care of freeing the sqlite3_index_info structure after
** everybody has finished with it.
*/
static double bestVirtualIndex(
  Parse *pParse,                 /* The parsing context */
  WhereClause *pWC,              /* The WHERE clause */
  struct SrcList_item *pSrc,     /* The FROM clause term to search */
  Bitmask notReady,              /* Mask of cursors that are not available */
  ExprList *pOrderBy,            /* The order by clause */
  int orderByUsable,             /* True if we can potential sort */
  sqlite3_index_info **ppIdxInfo /* Index information passed to xBestIndex */
){
  Table *pTab = pSrc->pTab;
  sqlite3_vtab *pVtab = pTab->pVtab;
  sqlite3_index_info *pIdxInfo;
  struct sqlite3_index_constraint *pIdxCons;
  struct sqlite3_index_orderby *pIdxOrderBy;
  struct sqlite3_index_constraint_usage *pUsage;
  WhereTerm *pTerm;
  int i, j;
  int nOrderBy;
  int rc;

  /* If the sqlite3_index_info structure has not been previously
  ** allocated and initialized for this virtual table, then allocate
  ** and initialize it now
  */
  pIdxInfo = *ppIdxInfo;
  if( pIdxInfo==0 ){
    WhereTerm *pTerm;
    int nTerm;
    WHERETRACE(("Recomputing index info for %s...\n", pTab->zName));

    /* Count the number of possible WHERE clause constraints referring
    ** to this virtual table */
    for(i=nTerm=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
      if( pTerm->leftCursor != pSrc->iCursor ) continue;
      if( (pTerm->eOperator&(pTerm->eOperator-1))==0 );
      testcase( pTerm->eOperator==WO_IN );
      testcase( pTerm->eOperator==WO_ISNULL );
      if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;
      nTerm++;
    }

    /* If the ORDER BY clause contains only columns in the current 
    ** virtual table then allocate space for the aOrderBy part of
    ** the sqlite3_index_info structure.
    */
    nOrderBy = 0;
    if( pOrderBy ){
      for(i=0; i<pOrderBy->nExpr; i++){
        Expr *pExpr = pOrderBy->a[i].pExpr;
        if( pExpr->op!=TK_COLUMN || pExpr->iTable!=pSrc->iCursor ) break;
      }
      if( i==pOrderBy->nExpr ){
        nOrderBy = pOrderBy->nExpr;
      }
    }

    /* Allocate the sqlite3_index_info structure
    */
    pIdxInfo = sqlite3DbMallocZero(pParse->db, sizeof(*pIdxInfo)
                             + (sizeof(*pIdxCons) + sizeof(*pUsage))*nTerm
                             + sizeof(*pIdxOrderBy)*nOrderBy );
    if( pIdxInfo==0 ){
      sqlite3ErrorMsg(pParse, "out of memory");
      return 0.0;
    }
    *ppIdxInfo = pIdxInfo;

    /* Initialize the structure.  The sqlite3_index_info structure contains
    ** many fields that are declared "const" to prevent xBestIndex from
    ** changing them.  We have to do some funky casting in order to
    ** initialize those fields.
    */
    pIdxCons = (struct sqlite3_index_constraint*)&pIdxInfo[1];
    pIdxOrderBy = (struct sqlite3_index_orderby*)&pIdxCons[nTerm];
    pUsage = (struct sqlite3_index_constraint_usage*)&pIdxOrderBy[nOrderBy];
    *(int*)&pIdxInfo->nConstraint = nTerm;
    *(int*)&pIdxInfo->nOrderBy = nOrderBy;
    *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint = pIdxCons;
    *(struct sqlite3_index_orderby**)&pIdxInfo->aOrderBy = pIdxOrderBy;
    *(struct sqlite3_index_constraint_usage**)&pIdxInfo->aConstraintUsage =
                                                                     pUsage;

    for(i=j=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
      if( pTerm->leftCursor != pSrc->iCursor ) continue;
      if( (pTerm->eOperator&(pTerm->eOperator-1))==0 );
      testcase( pTerm->eOperator==WO_IN );
      testcase( pTerm->eOperator==WO_ISNULL );
      if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;
      pIdxCons[j].iColumn = pTerm->leftColumn;
      pIdxCons[j].iTermOffset = i;
      pIdxCons[j].op = pTerm->eOperator;
      /* The direct assignment in the previous line is possible only because
      ** the WO_ and SQLITE_INDEX_CONSTRAINT_ codes are identical.  The
      ** following asserts verify this fact. */
      assert( WO_EQ==SQLITE_INDEX_CONSTRAINT_EQ );
      assert( WO_LT==SQLITE_INDEX_CONSTRAINT_LT );
      assert( WO_LE==SQLITE_INDEX_CONSTRAINT_LE );
      assert( WO_GT==SQLITE_INDEX_CONSTRAINT_GT );
      assert( WO_GE==SQLITE_INDEX_CONSTRAINT_GE );
      assert( WO_MATCH==SQLITE_INDEX_CONSTRAINT_MATCH );
      assert( pTerm->eOperator & (WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE|WO_MATCH) );
      j++;
    }
    for(i=0; i<nOrderBy; i++){
      Expr *pExpr = pOrderBy->a[i].pExpr;
      pIdxOrderBy[i].iColumn = pExpr->iColumn;
      pIdxOrderBy[i].desc = pOrderBy->a[i].sortOrder;
    }
  }

  /* At this point, the sqlite3_index_info structure that pIdxInfo points
  ** to will have been initialized, either during the current invocation or
  ** during some prior invocation.  Now we just have to customize the
  ** details of pIdxInfo for the current invocation and pass it to
  ** xBestIndex.
  */

  /* The module name must be defined. Also, by this point there must
  ** be a pointer to an sqlite3_vtab structure. Otherwise
  ** sqlite3ViewGetColumnNames() would have picked up the error. 
  */
  assert( pTab->azModuleArg && pTab->azModuleArg[0] );
  assert( pVtab );
#if 0
  if( pTab->pVtab==0 ){
    sqlite3ErrorMsg(pParse, "undefined module %s for table %s",
        pTab->azModuleArg[0], pTab->zName);
    return 0.0;
  }
#endif

  /* Set the aConstraint[].usable fields and initialize all 
  ** output variables to zero.
  **
  ** aConstraint[].usable is true for constraints where the right-hand
  ** side contains only references to tables to the left of the current
  ** table.  In other words, if the constraint is of the form:
  **
  **           column = expr
  **
  ** and we are evaluating a join, then the constraint on column is 
  ** only valid if all tables referenced in expr occur to the left
  ** of the table containing column.
  **
  ** The aConstraints[] array contains entries for all constraints
  ** on the current table.  That way we only have to compute it once
  ** even though we might try to pick the best index multiple times.
  ** For each attempt at picking an index, the order of tables in the
  ** join might be different so we have to recompute the usable flag
  ** each time.
  */
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;
  pUsage = pIdxInfo->aConstraintUsage;
  for(i=0; i<pIdxInfo->nConstraint; i++, pIdxCons++){
    j = pIdxCons->iTermOffset;
    pTerm = &pWC->a[j];
    pIdxCons->usable =  (pTerm->prereqRight & notReady)==0;
  }
  memset(pUsage, 0, sizeof(pUsage[0])*pIdxInfo->nConstraint);
  if( pIdxInfo->needToFreeIdxStr ){
    sqlite3_free(pIdxInfo->idxStr);
  }
  pIdxInfo->idxStr = 0;
  pIdxInfo->idxNum = 0;
  pIdxInfo->needToFreeIdxStr = 0;
  pIdxInfo->orderByConsumed = 0;
  pIdxInfo->estimatedCost = SQLITE_BIG_DBL / 2.0;
  nOrderBy = pIdxInfo->nOrderBy;
  if( pIdxInfo->nOrderBy && !orderByUsable ){
    *(int*)&pIdxInfo->nOrderBy = 0;
  }

  (void)sqlite3SafetyOff(pParse->db);
  WHERETRACE(("xBestIndex for %s\n", pTab->zName));
  TRACE_IDX_INPUTS(pIdxInfo);
  rc = pVtab->pModule->xBestIndex(pVtab, pIdxInfo);
  TRACE_IDX_OUTPUTS(pIdxInfo);
  (void)sqlite3SafetyOn(pParse->db);

  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_NOMEM ){
      pParse->db->mallocFailed = 1;
    }else if( !pVtab->zErrMsg ){
      sqlite3ErrorMsg(pParse, "%s", sqlite3ErrStr(rc));
    }else{
      sqlite3ErrorMsg(pParse, "%s", pVtab->zErrMsg);
    }
  }
  sqlite3DbFree(pParse->db, pVtab->zErrMsg);
  pVtab->zErrMsg = 0;

  for(i=0; i<pIdxInfo->nConstraint; i++){
    if( !pIdxInfo->aConstraint[i].usable && pUsage[i].argvIndex>0 ){
      sqlite3ErrorMsg(pParse, 
          "table %s: xBestIndex returned an invalid plan", pTab->zName);
      return 0.0;
    }
  }

  *(int*)&pIdxInfo->nOrderBy = nOrderBy;
  return pIdxInfo->estimatedCost;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** Find the best index for accessing a particular table.  Return a pointer
** to the index, flags that describe how the index should be used, the
** number of equality constraints, and the "cost" for this index.
**
** The lowest cost index wins.  The cost is an estimate of the amount of
** CPU and disk I/O need to process the request using the selected index.
** Factors that influence cost include:
**
**    *  The estimated number of rows that will be retrieved.  (The
**       fewer the better.)
**
**    *  Whether or not sorting must occur.
**
**    *  Whether or not there must be separate lookups in the
**       index and in the main table.
**
*/
static double bestIndex(
  Parse *pParse,              /* The parsing context */
  WhereClause *pWC,           /* The WHERE clause */
  struct SrcList_item *pSrc,  /* The FROM clause term to search */
  Bitmask notReady,           /* Mask of cursors that are not available */
  ExprList *pOrderBy,         /* The order by clause */
  Index **ppIndex,            /* Make *ppIndex point to the best index */
  int *pFlags,                /* Put flags describing this choice in *pFlags */
  int *pnEq                   /* Put the number of == or IN constraints here */
){
  WhereTerm *pTerm;
  Index *bestIdx = 0;         /* Index that gives the lowest cost */
  double lowestCost;          /* The cost of using bestIdx */
  int bestFlags = 0;          /* Flags associated with bestIdx */
  int bestNEq = 0;            /* Best value for nEq */
  int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed */
  Index *pProbe;              /* An index we are evaluating */
  int rev;                    /* True to scan in reverse order */
  int flags;                  /* Flags associated with pProbe */
  int nEq;                    /* Number of == or IN constraints */
  int eqTermMask;             /* Mask of valid equality operators */
  double cost;                /* Cost of using pProbe */

  WHERETRACE(("bestIndex: tbl=%s notReady=%llx\n", pSrc->pTab->zName, notReady));
  lowestCost = SQLITE_BIG_DBL;
  pProbe = pSrc->pTab->pIndex;

  /* If the table has no indices and there are no terms in the where
  ** clause that refer to the ROWID, then we will never be able to do
  ** anything other than a full table scan on this table.  We might as
  ** well put it first in the join order.  That way, perhaps it can be
  ** referenced by other tables in the join.
  */
  if( pProbe==0 &&
     findTerm(pWC, iCur, -1, 0, WO_EQ|WO_IN|WO_LT|WO_LE|WO_GT|WO_GE,0)==0 &&
     (pOrderBy==0 || !sortableByRowid(iCur, pOrderBy, pWC->pMaskSet, &rev)) ){
    *pFlags = 0;
    *ppIndex = 0;
    *pnEq = 0;
    return 0.0;
  }

  /* Check for a rowid=EXPR or rowid IN (...) constraints
  */
  pTerm = findTerm(pWC, iCur, -1, notReady, WO_EQ|WO_IN, 0);
  if( pTerm ){
    Expr *pExpr;
    *ppIndex = 0;
    bestFlags = WHERE_ROWID_EQ;
    if( pTerm->eOperator & WO_EQ ){
      /* Rowid== is always the best pick.  Look no further.  Because only
      ** a single row is generated, output is always in sorted order */
      *pFlags = WHERE_ROWID_EQ | WHERE_UNIQUE;
      *pnEq = 1;
      WHERETRACE(("... best is rowid\n"));
      return 0.0;
    }else if( (pExpr = pTerm->pExpr)->pList!=0 ){
      /* Rowid IN (LIST): cost is NlogN where N is the number of list
      ** elements.  */
      lowestCost = pExpr->pList->nExpr;
      lowestCost *= estLog(lowestCost);
    }else{
      /* Rowid IN (SELECT): cost is NlogN where N is the number of rows
      ** in the result of the inner select.  We have no way to estimate
      ** that value so make a wild guess. */
      lowestCost = 200;
    }
    WHERETRACE(("... rowid IN cost: %.9g\n", lowestCost));
  }

  /* Estimate the cost of a table scan.  If we do not know how many
  ** entries are in the table, use 1 million as a guess.
  */
  cost = pProbe ? pProbe->aiRowEst[0] : 1000000;
  WHERETRACE(("... table scan base cost: %.9g\n", cost));
  flags = WHERE_ROWID_RANGE;

  /* Check for constraints on a range of rowids in a table scan.
  */
  pTerm = findTerm(pWC, iCur, -1, notReady, WO_LT|WO_LE|WO_GT|WO_GE, 0);
  if( pTerm ){
    if( findTerm(pWC, iCur, -1, notReady, WO_LT|WO_LE, 0) ){
      flags |= WHERE_TOP_LIMIT;
      cost /= 3;  /* Guess that rowid<EXPR eliminates two-thirds or rows */
    }
    if( findTerm(pWC, iCur, -1, notReady, WO_GT|WO_GE, 0) ){
      flags |= WHERE_BTM_LIMIT;
      cost /= 3;  /* Guess that rowid>EXPR eliminates two-thirds of rows */
    }
    WHERETRACE(("... rowid range reduces cost to %.9g\n", cost));
  }else{
    flags = 0;
  }

  /* If the table scan does not satisfy the ORDER BY clause, increase
  ** the cost by NlogN to cover the expense of sorting. */
  if( pOrderBy ){
    if( sortableByRowid(iCur, pOrderBy, pWC->pMaskSet, &rev) ){
      flags |= WHERE_ORDERBY|WHERE_ROWID_RANGE;
      if( rev ){
        flags |= WHERE_REVERSE;
      }
    }else{
      cost += cost*estLog(cost);
      WHERETRACE(("... sorting increases cost to %.9g\n", cost));
    }
  }
  if( cost<lowestCost ){
    lowestCost = cost;
    bestFlags = flags;
  }

  /* If the pSrc table is the right table of a LEFT JOIN then we may not
  ** use an index to satisfy IS NULL constraints on that table.  This is
  ** because columns might end up being NULL if the table does not match -
  ** a circumstance which the index cannot help us discover.  Ticket #2177.
  */
  if( (pSrc->jointype & JT_LEFT)!=0 ){
    eqTermMask = WO_EQ|WO_IN;
  }else{
    eqTermMask = WO_EQ|WO_IN|WO_ISNULL;
  }

  /* Look at each index.
  */
  for(; pProbe; pProbe=pProbe->pNext){
    int i;                       /* Loop counter */
    double inMultiplier = 1;

    WHERETRACE(("... index %s:\n", pProbe->zName));

    /* Count the number of columns in the index that are satisfied
    ** by x=EXPR constraints or x IN (...) constraints.
    */
    flags = 0;
    for(i=0; i<pProbe->nColumn; i++){
      int j = pProbe->aiColumn[i];
      pTerm = findTerm(pWC, iCur, j, notReady, eqTermMask, pProbe);
      if( pTerm==0 ) break;
      flags |= WHERE_COLUMN_EQ;
      if( pTerm->eOperator & WO_IN ){
        Expr *pExpr = pTerm->pExpr;
        flags |= WHERE_COLUMN_IN;
        if( pExpr->pSelect!=0 ){
          inMultiplier *= 25;
        }else if( ALWAYS(pExpr->pList) ){
          inMultiplier *= pExpr->pList->nExpr + 1;
        }
      }
    }
    cost = pProbe->aiRowEst[i] * inMultiplier * estLog(inMultiplier);
    nEq = i;
    if( pProbe->onError!=OE_None && (flags & WHERE_COLUMN_IN)==0
         && nEq==pProbe->nColumn ){
      flags |= WHERE_UNIQUE;
    }
    WHERETRACE(("...... nEq=%d inMult=%.9g cost=%.9g\n",nEq,inMultiplier,cost));

    /* Look for range constraints
    */
    if( nEq<pProbe->nColumn ){
      int j = pProbe->aiColumn[nEq];
      pTerm = findTerm(pWC, iCur, j, notReady, WO_LT|WO_LE|WO_GT|WO_GE, pProbe);
      if( pTerm ){
        flags |= WHERE_COLUMN_RANGE;
        if( findTerm(pWC, iCur, j, notReady, WO_LT|WO_LE, pProbe) ){
          flags |= WHERE_TOP_LIMIT;
          cost /= 3;
        }
        if( findTerm(pWC, iCur, j, notReady, WO_GT|WO_GE, pProbe) ){
          flags |= WHERE_BTM_LIMIT;
          cost /= 3;
        }
        WHERETRACE(("...... range reduces cost to %.9g\n", cost));
      }
    }

    /* Add the additional cost of sorting if that is a factor.
    */
    if( pOrderBy ){
      if( (flags & WHERE_COLUMN_IN)==0 &&
           isSortingIndex(pParse,pWC->pMaskSet,pProbe,iCur,pOrderBy,nEq,&rev) ){
        if( flags==0 ){
          flags = WHERE_COLUMN_RANGE;
        }
        flags |= WHERE_ORDERBY;
        if( rev ){
          flags |= WHERE_REVERSE;
        }
      }else{
        cost += cost*estLog(cost);
        WHERETRACE(("...... orderby increases cost to %.9g\n", cost));
      }
    }

    /* Check to see if we can get away with using just the index without
    ** ever reading the table.  If that is the case, then halve the
    ** cost of this index.
    */
    if( flags && pSrc->colUsed < (((Bitmask)1)<<(BMS-1)) ){
      Bitmask m = pSrc->colUsed;
      int j;
      for(j=0; j<pProbe->nColumn; j++){
        int x = pProbe->aiColumn[j];
        if( x<BMS-1 ){
          m &= ~(((Bitmask)1)<<x);
        }
      }
      if( m==0 ){
        flags |= WHERE_IDX_ONLY;
        cost /= 2;
        WHERETRACE(("...... idx-only reduces cost to %.9g\n", cost));
      }
    }

    /* If this index has achieved the lowest cost so far, then use it.
    */
    if( flags && cost < lowestCost ){
      bestIdx = pProbe;
      lowestCost = cost;
      bestFlags = flags;
      bestNEq = nEq;
    }
  }

  /* Report the best result
  */
  *ppIndex = bestIdx;
  WHERETRACE(("best index is %s, cost=%.9g, flags=%x, nEq=%d\n",
        bestIdx ? bestIdx->zName : "(none)", lowestCost, bestFlags, bestNEq));
  *pFlags = bestFlags | eqTermMask;
  *pnEq = bestNEq;
  return lowestCost;
}


/*
** Disable a term in the WHERE clause.  Except, do not disable the term
** if it controls a LEFT OUTER JOIN and it did not originate in the ON
** or USING clause of that join.
**
** Consider the term t2.z='ok' in the following queries:
**
**   (1)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x WHERE t2.z='ok'
**   (2)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x AND t2.z='ok'
**   (3)  SELECT * FROM t1, t2 WHERE t1.a=t2.x AND t2.z='ok'
**
** The t2.z='ok' is disabled in the in (2) because it originates
** in the ON clause.  The term is disabled in (3) because it is not part
** of a LEFT OUTER JOIN.  In (1), the term is not disabled.
**
** Disabling a term causes that term to not be tested in the inner loop
** of the join.  Disabling is an optimization.  When terms are satisfied
** by indices, we disable them to prevent redundant tests in the inner
** loop.  We would get the correct results if nothing were ever disabled,
** but joins might run a little slower.  The trick is to disable as much
** as we can without disabling too much.  If we disabled in (1), we'd get
** the wrong answer.  See ticket #813.
*/
static void disableTerm(WhereLevel *pLevel, WhereTerm *pTerm){
  if( pTerm
      && ALWAYS((pTerm->flags & TERM_CODED)==0)
      && (pLevel->iLeftJoin==0 || ExprHasProperty(pTerm->pExpr, EP_FromJoin))
  ){
    pTerm->flags |= TERM_CODED;
    if( pTerm->iParent>=0 ){
      WhereTerm *pOther = &pTerm->pWC->a[pTerm->iParent];
      if( (--pOther->nChild)==0 ){
        disableTerm(pLevel, pOther);
      }
    }
  }
}

/*
** Apply the affinities associated with the first n columns of index
** pIdx to the values in the n registers starting at base.
*/
static void codeApplyAffinity(Parse *pParse, int base, int n, Index *pIdx){
  if( n>0 ){
    Vdbe *v = pParse->pVdbe;
    assert( v!=0 );
    sqlite3VdbeAddOp2(v, OP_Affinity, base, n);
    sqlite3IndexAffinityStr(v, pIdx);
    sqlite3ExprCacheAffinityChange(pParse, base, n);
  }
}


/*
** Generate code for a single equality term of the WHERE clause.  An equality
** term can be either X=expr or X IN (...).   pTerm is the term to be 
** coded.
**
** The current value for the constraint is left in register iReg.
**
** For a constraint of the form X=expr, the expression is evaluated and its
** result is left on the stack.  For constraints of the form X IN (...)
** this routine sets up a loop that will iterate over all values of X.
*/
static int codeEqualityTerm(
  Parse *pParse,      /* The parsing context */
  WhereTerm *pTerm,   /* The term of the WHERE clause to be coded */
  WhereLevel *pLevel, /* When level of the FROM clause we are working on */
  int iTarget         /* Attempt to leave results in this register */
){
  Expr *pX = pTerm->pExpr;
  Vdbe *v = pParse->pVdbe;
  int iReg;                  /* Register holding results */

  if( iTarget<=0 ){
    iReg = iTarget = sqlite3GetTempReg(pParse);
  }
  if( pX->op==TK_EQ ){
    iReg = sqlite3ExprCodeTarget(pParse, pX->pRight, iTarget);
  }else if( pX->op==TK_ISNULL ){
    iReg = iTarget;
    sqlite3VdbeAddOp2(v, OP_Null, 0, iReg);
#ifndef SQLITE_OMIT_SUBQUERY
  }else{
    int eType;
    int iTab;
    struct InLoop *pIn;

    assert( pX->op==TK_IN );
    iReg = iTarget;
    eType = sqlite3FindInIndex(pParse, pX, 0);
    iTab = pX->iTable;
    sqlite3VdbeAddOp2(v, OP_Rewind, iTab, 0);
    VdbeComment((v, "%.*s", pX->span.n, pX->span.z));
    if( pLevel->nIn==0 ){
      pLevel->nxt = sqlite3VdbeMakeLabel(v);
    }
    pLevel->nIn++;
    pLevel->aInLoop = sqlite3DbReallocOrFree(pParse->db, pLevel->aInLoop,
                                    sizeof(pLevel->aInLoop[0])*pLevel->nIn);
    pIn = pLevel->aInLoop;
    if( pIn ){
      pIn += pLevel->nIn - 1;
      pIn->iCur = iTab;
      if( eType==IN_INDEX_ROWID ){
        pIn->topAddr = sqlite3VdbeAddOp2(v, OP_Rowid, iTab, iReg);
      }else{
        pIn->topAddr = sqlite3VdbeAddOp3(v, OP_Column, iTab, 0, iReg);
      }
      sqlite3VdbeAddOp1(v, OP_IsNull, iReg);
    }else{
      pLevel->nIn = 0;
    }
#endif
  }
  disableTerm(pLevel, pTerm);
  return iReg;
}

/*
** Generate code that will evaluate all == and IN constraints for an
** index.  The values for all constraints are left on the stack.
**
** For example, consider table t1(a,b,c,d,e,f) with index i1(a,b,c).
** Suppose the WHERE clause is this:  a==5 AND b IN (1,2,3) AND c>5 AND c<10
** The index has as many as three equality constraints, but in this
** example, the third "c" value is an inequality.  So only two 
** constraints are coded.  This routine will generate code to evaluate
** a==5 and b IN (1,2,3).  The current values for a and b will be left
** on the stack - a is the deepest and b the shallowest.
**
** In the example above nEq==2.  But this subroutine works for any value
** of nEq including 0.  If nEq==0, this routine is nearly a no-op.
** The only thing it does is allocate the pLevel->iMem memory cell.
**
** This routine always allocates at least one memory cell and puts
** the address of that memory cell in pLevel->iMem.  The code that
** calls this routine will use pLevel->iMem to store the termination
** key value of the loop.  If one or more IN operators appear, then
** this routine allocates an additional nEq memory cells for internal
** use.
*/
static int codeAllEqualityTerms(
  Parse *pParse,        /* Parsing context */
  WhereLevel *pLevel,   /* Which nested loop of the FROM we are coding */
  WhereClause *pWC,     /* The WHERE clause */
  Bitmask notReady,     /* Which parts of FROM have not yet been coded */
  int nExtraReg         /* Number of extra registers to allocate */
){
  int nEq = pLevel->nEq;        /* The number of == or IN constraints to code */
  Vdbe *v = pParse->pVdbe;      /* The virtual machine under construction */
  Index *pIdx = pLevel->pIdx;   /* The index being used for this loop */
  int iCur = pLevel->iTabCur;   /* The cursor of the table */
  WhereTerm *pTerm;             /* A single constraint term */
  int j;                        /* Loop counter */
  int regBase;                  /* Base register */

  /* Figure out how many memory cells we will need then allocate them.
  ** We always need at least one used to store the loop terminator
  ** value.  If there are IN operators we'll need one for each == or
  ** IN constraint.
  */
  pLevel->iMem = pParse->nMem + 1;
  regBase = pParse->nMem + 2;
  pParse->nMem += pLevel->nEq + 2 + nExtraReg;

  /* Evaluate the equality constraints
  */
  assert( pIdx->nColumn>=nEq );
  for(j=0; j<nEq; j++){
    int r1;
    int k = pIdx->aiColumn[j];
    pTerm = findTerm(pWC, iCur, k, notReady, pLevel->flags, pIdx);
    if( NEVER(pTerm==0) ) break;
    assert( (pTerm->flags & TERM_CODED)==0 );
    r1 = codeEqualityTerm(pParse, pTerm, pLevel, regBase+j);
    if( r1!=regBase+j ){
      sqlite3VdbeAddOp2(v, OP_SCopy, r1, regBase+j);
    }
    testcase( pTerm->eOperator & WO_ISNULL );
    testcase( pTerm->eOperator & WO_IN );
    if( (pTerm->eOperator & (WO_ISNULL|WO_IN))==0 ){
      sqlite3VdbeAddOp2(v, OP_IsNull, regBase+j, pLevel->brk);
    }
  }
  return regBase;
}

#if defined(SQLITE_TEST)
/*
** The following variable holds a text description of query plan generated
** by the most recent call to sqlite3WhereBegin().  Each call to WhereBegin
** overwrites the previous.  This information is used for testing and
** analysis only.
*/
char sqlite3_query_plan[BMS*2*40];  /* Text of the join */
static int nQPlan = 0;              /* Next free slow in _query_plan[] */

#endif /* SQLITE_TEST */


/*
** Free a WhereInfo structure
*/
static void whereInfoFree(WhereInfo *pWInfo){
  if( pWInfo ){
    int i;
    sqlite3 *db = pWInfo->pParse->db;
    for(i=0; i<pWInfo->nLevel; i++){
      sqlite3_index_info *pInfo = pWInfo->a[i].pIdxInfo;
      if( pInfo ){
        assert( pInfo->needToFreeIdxStr==0 );
        sqlite3DbFree(db, pInfo);
      }
    }
    sqlite3DbFree(db, pWInfo);
  }
}


/*
** Generate the beginning of the loop used for WHERE clause processing.
** The return value is a pointer to an opaque structure that contains
** information needed to terminate the loop.  Later, the calling routine
** should invoke sqlite3WhereEnd() with the return value of this function
** in order to complete the WHERE clause processing.
**
** If an error occurs, this routine returns NULL.
**
** The basic idea is to do a nested loop, one loop for each table in
** the FROM clause of a select.  (INSERT and UPDATE statements are the
** same as a SELECT with only a single table in the FROM clause.)  For
** example, if the SQL is this:
**
**       SELECT * FROM t1, t2, t3 WHERE ...;
**
** Then the code generated is conceptually like the following:
**
**      foreach row1 in t1 do       \    Code generated
**        foreach row2 in t2 do      |-- by sqlite3WhereBegin()
**          foreach row3 in t3 do   /
**            ...
**          end                     \    Code generated
**        end                        |-- by sqlite3WhereEnd()
**      end                         /
**
** Note that the loops might not be nested in the order in which they
** appear in the FROM clause if a different order is better able to make
** use of indices.  Note also that when the IN operator appears in
** the WHERE clause, it might result in additional nested loops for
** scanning through all values on the right-hand side of the IN.
**
** There are Btree cursors associated with each table.  t1 uses cursor
** number pTabList->a[0].iCursor.  t2 uses the cursor pTabList->a[1].iCursor.
** And so forth.  This routine generates code to open those VDBE cursors
** and sqlite3WhereEnd() generates the code to close them.
**
** The code that sqlite3WhereBegin() generates leaves the cursors named
** in pTabList pointing at their appropriate entries.  The [...] code
** can use OP_Column and OP_Rowid opcodes on these cursors to extract
** data from the various tables of the loop.
**
** If the WHERE clause is empty, the foreach loops must each scan their
** entire tables.  Thus a three-way join is an O(N^3) operation.  But if
** the tables have indices and there are terms in the WHERE clause that
** refer to those indices, a complete table scan can be avoided and the
** code will run much faster.  Most of the work of this routine is checking
** to see if there are indices that can be used to speed up the loop.
**
** Terms of the WHERE clause are also used to limit which rows actually
** make it to the "..." in the middle of the loop.  After each "foreach",
** terms of the WHERE clause that use only terms in that loop and outer
** loops are evaluated and if false a jump is made around all subsequent
** inner loops (or around the "..." if the test occurs within the inner-
** most loop)
**
** OUTER JOINS
**
** An outer join of tables t1 and t2 is conceptally coded as follows:
**
**    foreach row1 in t1 do
**      flag = 0
**      foreach row2 in t2 do
**        start:
**          ...
**          flag = 1
**      end
**      if flag==0 then
**        move the row2 cursor to a null row
**        goto start
**      fi
**    end
**
** ORDER BY CLAUSE PROCESSING
**
** *ppOrderBy is a pointer to the ORDER BY clause of a SELECT statement,
** if there is one.  If there is no ORDER BY clause or if this routine
** is called from an UPDATE or DELETE statement, then ppOrderBy is NULL.
**
** If an index can be used so that the natural output order of the table
** scan is correct for the ORDER BY clause, then that index is used and
** *ppOrderBy is set to NULL.  This is an optimization that prevents an
** unnecessary sort of the result set if an index appropriate for the
** ORDER BY clause already exists.
**
** If the where clause loops cannot be arranged to provide the correct
** output order, then the *ppOrderBy is unchanged.
*/
WhereInfo *sqlite3WhereBegin(
  Parse *pParse,        /* The parser context */
  SrcList *pTabList,    /* A list of all tables to be scanned */
  Expr *pWhere,         /* The WHERE clause */
  ExprList **ppOrderBy, /* An ORDER BY clause, or NULL */
  u8 wflags             /* One of the WHERE_* flags defined in sqliteInt.h */
){
  int i;                     /* Loop counter */
  WhereInfo *pWInfo;         /* Will become the return value of this function */
  Vdbe *v = pParse->pVdbe;   /* The virtual database engine */
  int brk, cont = 0;         /* Addresses used during code generation */
  Bitmask notReady;          /* Cursors that are not yet positioned */
  WhereTerm *pTerm;          /* A single term in the WHERE clause */
  ExprMaskSet maskSet;       /* The expression mask set */
  WhereClause wc;            /* The WHERE clause is divided into these terms */
  struct SrcList_item *pTabItem;  /* A single entry from pTabList */
  WhereLevel *pLevel;             /* A single level in the pWInfo list */
  int iFrom;                      /* First unused FROM clause element */
  int andFlags;              /* AND-ed combination of all wc.a[].flags */
  sqlite3 *db;               /* Database connection */
  ExprList *pOrderBy = 0;

  /* The number of tables in the FROM clause is limited by the number of
  ** bits in a Bitmask 
  */
  if( pTabList->nSrc>BMS ){
    sqlite3ErrorMsg(pParse, "at most %d tables in a join", BMS);
    return 0;
  }

  if( ppOrderBy ){
    pOrderBy = *ppOrderBy;
  }

  /* Split the WHERE clause into separate subexpressions where each
  ** subexpression is separated by an AND operator.
  */
  initMaskSet(&maskSet);
  whereClauseInit(&wc, pParse, &maskSet);
  sqlite3ExprCodeConstants(pParse, pWhere);
  whereSplit(&wc, pWhere, TK_AND);
    
  /* Allocate and initialize the WhereInfo structure that will become the
  ** return value.
  */
  db = pParse->db;
  pWInfo = sqlite3DbMallocZero(db,  
                      sizeof(WhereInfo) + pTabList->nSrc*sizeof(WhereLevel));
  if( db->mallocFailed ){
    goto whereBeginNoMem;
  }
  pWInfo->nLevel = pTabList->nSrc;
  pWInfo->pParse = pParse;
  pWInfo->pTabList = pTabList;
  pWInfo->iBreak = sqlite3VdbeMakeLabel(v);

  /* Special case: a WHERE clause that is constant.  Evaluate the
  ** expression and either jump over all of the code or fall thru.
  */
  if( pWhere && (pTabList->nSrc==0 || sqlite3ExprIsConstantNotJoin(pWhere)) ){
    sqlite3ExprIfFalse(pParse, pWhere, pWInfo->iBreak, SQLITE_JUMPIFNULL);
    pWhere = 0;
  }

  /* Assign a bit from the bitmask to every term in the FROM clause.
  **
  ** When assigning bitmask values to FROM clause cursors, it must be
  ** the case that if X is the bitmask for the N-th FROM clause term then
  ** the bitmask for all FROM clause terms to the left of the N-th term
  ** is (X-1).   An expression from the ON clause of a LEFT JOIN can use
  ** its Expr.iRightJoinTable value to find the bitmask of the right table
  ** of the join.  Subtracting one from the right table bitmask gives a
  ** bitmask for all tables to the left of the join.  Knowing the bitmask
  ** for all tables to the left of a left join is important.  Ticket #3015.
  */
  for(i=0; i<pTabList->nSrc; i++){
    createMask(&maskSet, pTabList->a[i].iCursor);
  }
#ifndef NDEBUG
  {
    Bitmask toTheLeft = 0;
    for(i=0; i<pTabList->nSrc; i++){
      Bitmask m = getMask(&maskSet, pTabList->a[i].iCursor);
      assert( (m-1)==toTheLeft );
      toTheLeft |= m;
    }
  }
#endif

  /* Analyze all of the subexpressions.  Note that exprAnalyze() might
  ** add new virtual terms onto the end of the WHERE clause.  We do not
  ** want to analyze these virtual terms, so start analyzing at the end
  ** and work forward so that the added virtual terms are never processed.
  */
  exprAnalyzeAll(pTabList, &wc);
  if( db->mallocFailed ){
    goto whereBeginNoMem;
  }

  /* Chose the best index to use for each table in the FROM clause.
  **
  ** This loop fills in the following fields:
  **
  **   pWInfo->a[].pIdx      The index to use for this level of the loop.
  **   pWInfo->a[].flags     WHERE_xxx flags associated with pIdx
  **   pWInfo->a[].nEq       The number of == and IN constraints
  **   pWInfo->a[].iFrom     When term of the FROM clause is being coded
  **   pWInfo->a[].iTabCur   The VDBE cursor for the database table
  **   pWInfo->a[].iIdxCur   The VDBE cursor for the index
  **
  ** This loop also figures out the nesting order of tables in the FROM
  ** clause.
  */
  notReady = ~(Bitmask)0;
  pTabItem = pTabList->a;
  pLevel = pWInfo->a;
  andFlags = ~0;
  WHERETRACE(("*** Optimizer Start ***\n"));
  for(i=iFrom=0, pLevel=pWInfo->a; i<pTabList->nSrc; i++, pLevel++){
    Index *pIdx;                /* Index for FROM table at pTabItem */
    int flags;                  /* Flags asssociated with pIdx */
    int nEq;                    /* Number of == or IN constraints */
    double cost;                /* The cost for pIdx */
    int j;                      /* For looping over FROM tables */
    Index *pBest = 0;           /* The best index seen so far */
    int bestFlags = 0;          /* Flags associated with pBest */
    int bestNEq = 0;            /* nEq associated with pBest */
    double lowestCost;          /* Cost of the pBest */
    int bestJ = 0;              /* The value of j */
    Bitmask m;                  /* Bitmask value for j or bestJ */
    int once = 0;               /* True when first table is seen */
    sqlite3_index_info *pIndex; /* Current virtual index */

    lowestCost = SQLITE_BIG_DBL;
    for(j=iFrom, pTabItem=&pTabList->a[j]; j<pTabList->nSrc; j++, pTabItem++){
      int doNotReorder;  /* True if this table should not be reordered */

      doNotReorder =  (pTabItem->jointype & (JT_LEFT|JT_CROSS))!=0;
      if( once && doNotReorder ) break;
      m = getMask(&maskSet, pTabItem->iCursor);
      if( (m & notReady)==0 ){
        if( j==iFrom ) iFrom++;
        continue;
      }
      assert( pTabItem->pTab );
#ifndef SQLITE_OMIT_VIRTUALTABLE
      if( IsVirtual(pTabItem->pTab) ){
        sqlite3_index_info **ppIdxInfo = &pWInfo->a[j].pIdxInfo;
        cost = bestVirtualIndex(pParse, &wc, pTabItem, notReady,
                                ppOrderBy ? *ppOrderBy : 0, i==0,
                                ppIdxInfo);
        flags = WHERE_VIRTUALTABLE;
        pIndex = *ppIdxInfo;
        if( pIndex && pIndex->orderByConsumed ){
          flags = WHERE_VIRTUALTABLE | WHERE_ORDERBY;
        }
        pIdx = 0;
        nEq = 0;
        if( (SQLITE_BIG_DBL/2.0)<cost ){
          /* The cost is not allowed to be larger than SQLITE_BIG_DBL (the
          ** inital value of lowestCost in this loop. If it is, then
          ** the (cost<lowestCost) test below will never be true and
          ** pLevel->pBestIdx never set.
          */ 
          cost = (SQLITE_BIG_DBL/2.0);
        }
      }else 
#endif
      {
        cost = bestIndex(pParse, &wc, pTabItem, notReady,
                         (i==0 && ppOrderBy) ? *ppOrderBy : 0,
                         &pIdx, &flags, &nEq);
        pIndex = 0;
      }
      if( cost<lowestCost ){
        once = 1;
        lowestCost = cost;
        pBest = pIdx;
        bestFlags = flags;
        bestNEq = nEq;
        bestJ = j;
        pLevel->pBestIdx = pIndex;
      }
      if( doNotReorder ) break;
    }
    WHERETRACE(("*** Optimizer selects table %d for loop %d\n", bestJ,
           pLevel-pWInfo->a));
    if( (bestFlags & WHERE_ORDERBY)!=0 ){
      *ppOrderBy = 0;
    }
    andFlags &= bestFlags;
    pLevel->flags = bestFlags;
    pLevel->pIdx = pBest;
    pLevel->nEq = bestNEq;
    pLevel->aInLoop = 0;
    pLevel->nIn = 0;
    if( pBest ){
      pLevel->iIdxCur = pParse->nTab++;
    }else{
      pLevel->iIdxCur = -1;
    }
    notReady &= ~getMask(&maskSet, pTabList->a[bestJ].iCursor);
    pLevel->iFrom = bestJ;
  }
  WHERETRACE(("*** Optimizer Finished ***\n"));

  /* If the total query only selects a single row, then the ORDER BY
  ** clause is irrelevant.
  */
  if( (andFlags & WHERE_UNIQUE)!=0 && ppOrderBy ){
    *ppOrderBy = 0;
  }

  /* If the caller is an UPDATE or DELETE statement that is requesting
  ** to use a one-pass algorithm, determine if this is appropriate.
  ** The one-pass algorithm only works if the WHERE clause constraints
  ** the statement to update a single row.
  */
  assert( (wflags & WHERE_ONEPASS_DESIRED)==0 || pWInfo->nLevel==1 );
  if( (wflags & WHERE_ONEPASS_DESIRED)!=0 && (andFlags & WHERE_UNIQUE)!=0 ){
    pWInfo->okOnePass = 1;
    pWInfo->a[0].flags &= ~WHERE_IDX_ONLY;
  }

  /* Open all tables in the pTabList and any indices selected for
  ** searching those tables.
  */
  sqlite3CodeVerifySchema(pParse, -1); /* Insert the cookie verifier Goto */
  for(i=0, pLevel=pWInfo->a; i<pTabList->nSrc; i++, pLevel++){
    Table *pTab;     /* Table to open */
    Index *pIx;      /* Index used to access pTab (if any) */
    int iDb;         /* Index of database containing table/index */
    int iIdxCur = pLevel->iIdxCur;

#ifndef SQLITE_OMIT_EXPLAIN
    if( pParse->explain==2 ){
      char *zMsg;
      struct SrcList_item *pItem = &pTabList->a[pLevel->iFrom];
      zMsg = sqlite3MPrintf(db, "TABLE %s", pItem->zName);
      if( pItem->zAlias ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s AS %s", zMsg, pItem->zAlias);
      }
      if( (pIx = pLevel->pIdx)!=0 ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s WITH INDEX %s", zMsg, pIx->zName);
      }else if( pLevel->flags & (WHERE_ROWID_EQ|WHERE_ROWID_RANGE) ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s USING PRIMARY KEY", zMsg);
      }
#ifndef SQLITE_OMIT_VIRTUALTABLE
      else if( pLevel->pBestIdx ){
        sqlite3_index_info *pBestIdx = pLevel->pBestIdx;
        zMsg = sqlite3MAppendf(db, zMsg, "%s VIRTUAL TABLE INDEX %d:%s", zMsg,
                    pBestIdx->idxNum, pBestIdx->idxStr);
      }
#endif
      if( pLevel->flags & WHERE_ORDERBY ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s ORDER BY", zMsg);
      }
      sqlite3VdbeAddOp4(v, OP_Explain, i, pLevel->iFrom, 0, zMsg, P4_DYNAMIC);
    }
#endif /* SQLITE_OMIT_EXPLAIN */
    pTabItem = &pTabList->a[pLevel->iFrom];
    pTab = pTabItem->pTab;
    iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
    if( pTab->isEphem || pTab->pSelect ) continue;
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( pLevel->pBestIdx ){
      int iCur = pTabItem->iCursor;
      sqlite3VdbeAddOp4(v, OP_VOpen, iCur, 0, 0,
                        (const char*)pTab->pVtab, P4_VTAB);
    }else
#endif
    if( (pLevel->flags & WHERE_IDX_ONLY)==0 ){
      int op = pWInfo->okOnePass ? OP_OpenWrite : OP_OpenRead;
      sqlite3OpenTable(pParse, pTabItem->iCursor, iDb, pTab, op);
      if( !pWInfo->okOnePass && pTab->nCol<(sizeof(Bitmask)*8) ){
        Bitmask b = pTabItem->colUsed;
        int n = 0;
        for(; b; b=b>>1, n++){}
        sqlite3VdbeChangeP2(v, sqlite3VdbeCurrentAddr(v)-2, n);
        assert( n<=pTab->nCol );
      }
    }else{
      sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);
    }
    pLevel->iTabCur = pTabItem->iCursor;
    if( (pIx = pLevel->pIdx)!=0 ){
      KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIx);
      assert( pIx->pSchema==pTab->pSchema );
      sqlite3VdbeAddOp2(v, OP_SetNumColumns, 0, pIx->nColumn+1);
      sqlite3VdbeAddOp4(v, OP_OpenRead, iIdxCur, pIx->tnum, iDb,
                        (char*)pKey, P4_KEYINFO_HANDOFF);
      VdbeComment((v, "%s", pIx->zName));
    }
    sqlite3CodeVerifySchema(pParse, iDb);
  }
  pWInfo->iTop = sqlite3VdbeCurrentAddr(v);

  /* Generate the code to do the search.  Each iteration of the for
  ** loop below generates code for a single nested loop of the VM
  ** program.
  */
  notReady = ~(Bitmask)0;
  for(i=0, pLevel=pWInfo->a; i<pTabList->nSrc; i++, pLevel++){
    int j;
    int iCur = pTabItem->iCursor;  /* The VDBE cursor for the table */
    Index *pIdx;       /* The index we will be using */
    int nxt;           /* Where to jump to continue with the next IN case */
    int iIdxCur;       /* The VDBE cursor for the index */
    int omitTable;     /* True if we use the index only */
    int bRev;          /* True if we need to scan in reverse order */

    pTabItem = &pTabList->a[pLevel->iFrom];
    iCur = pTabItem->iCursor;
    pIdx = pLevel->pIdx;
    iIdxCur = pLevel->iIdxCur;
    bRev = (pLevel->flags & WHERE_REVERSE)!=0;
    omitTable = (pLevel->flags & WHERE_IDX_ONLY)!=0;

    /* Create labels for the "break" and "continue" instructions
    ** for the current loop.  Jump to brk to break out of a loop.
    ** Jump to cont to go immediately to the next iteration of the
    ** loop.
    **
    ** When there is an IN operator, we also have a "nxt" label that
    ** means to continue with the next IN value combination.  When
    ** there are no IN operators in the constraints, the "nxt" label
    ** is the same as "brk".
    */
    brk = pLevel->brk = pLevel->nxt = sqlite3VdbeMakeLabel(v);
    cont = pLevel->cont = sqlite3VdbeMakeLabel(v);

    /* If this is the right table of a LEFT OUTER JOIN, allocate and
    ** initialize a memory cell that records if this table matches any
    ** row of the left table of the join.
    */
    if( pLevel->iFrom>0 && (pTabItem[0].jointype & JT_LEFT)!=0 ){
      pLevel->iLeftJoin = ++pParse->nMem;
      sqlite3VdbeAddOp2(v, OP_Integer, 0, pLevel->iLeftJoin);
      VdbeComment((v, "init LEFT JOIN no-match flag"));
    }

#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( pLevel->pBestIdx ){
      /* Case 0:  The table is a virtual-table.  Use the VFilter and VNext
      **          to access the data.
      */
      int j;
      int iReg;   /* P3 Value for OP_VFilter */
      sqlite3_index_info *pBestIdx = pLevel->pBestIdx;
      int nConstraint = pBestIdx->nConstraint;
      struct sqlite3_index_constraint_usage *aUsage =
                                                  pBestIdx->aConstraintUsage;
      const struct sqlite3_index_constraint *aConstraint =
                                                  pBestIdx->aConstraint;

      iReg = sqlite3GetTempRange(pParse, nConstraint+2);
      pParse->disableColCache++;
      for(j=1; j<=nConstraint; j++){
        int k;
        for(k=0; k<nConstraint; k++){
          if( aUsage[k].argvIndex==j ){
            int iTerm = aConstraint[k].iTermOffset;
            assert( pParse->disableColCache );
            sqlite3ExprCode(pParse, wc.a[iTerm].pExpr->pRight, iReg+j+1);
            break;
          }
        }
        if( k==nConstraint ) break;
      }
      assert( pParse->disableColCache );
      pParse->disableColCache--;
      sqlite3VdbeAddOp2(v, OP_Integer, pBestIdx->idxNum, iReg);
      sqlite3VdbeAddOp2(v, OP_Integer, j-1, iReg+1);
      sqlite3VdbeAddOp4(v, OP_VFilter, iCur, brk, iReg, pBestIdx->idxStr,
                        pBestIdx->needToFreeIdxStr ? P4_MPRINTF : P4_STATIC);
      sqlite3ReleaseTempRange(pParse, iReg, nConstraint+2);
      pBestIdx->needToFreeIdxStr = 0;
      for(j=0; j<nConstraint; j++){
        if( aUsage[j].omit ){
          int iTerm = aConstraint[j].iTermOffset;
          disableTerm(pLevel, &wc.a[iTerm]);
        }
      }
      pLevel->op = OP_VNext;
      pLevel->p1 = iCur;
      pLevel->p2 = sqlite3VdbeCurrentAddr(v);
    }else
#endif /* SQLITE_OMIT_VIRTUALTABLE */

    if( pLevel->flags & WHERE_ROWID_EQ ){
      /* Case 1:  We can directly reference a single row using an
      **          equality comparison against the ROWID field.  Or
      **          we reference multiple rows using a "rowid IN (...)"
      **          construct.
      */
      int r1;
      pTerm = findTerm(&wc, iCur, -1, notReady, WO_EQ|WO_IN, 0);
      assert( pTerm!=0 );
      assert( pTerm->pExpr!=0 );
      assert( pTerm->leftCursor==iCur );
      assert( omitTable==0 );
      r1 = codeEqualityTerm(pParse, pTerm, pLevel, 0);
      nxt = pLevel->nxt;
      sqlite3VdbeAddOp2(v, OP_MustBeInt, r1, nxt);
      sqlite3VdbeAddOp3(v, OP_NotExists, iCur, nxt, r1);
      VdbeComment((v, "pk"));
      pLevel->op = OP_Noop;
    }else if( pLevel->flags & WHERE_ROWID_RANGE ){
      /* Case 2:  We have an inequality comparison against the ROWID field.
      */
      int testOp = OP_Noop;
      int start;
      WhereTerm *pStart, *pEnd;

      assert( omitTable==0 );
      pStart = findTerm(&wc, iCur, -1, notReady, WO_GT|WO_GE, 0);
      pEnd = findTerm(&wc, iCur, -1, notReady, WO_LT|WO_LE, 0);
      if( bRev ){
        pTerm = pStart;
        pStart = pEnd;
        pEnd = pTerm;
      }
      if( pStart ){
        Expr *pX;
        int r1, regFree1;
        pX = pStart->pExpr;
        assert( pX!=0 );
        assert( pStart->leftCursor==iCur );
        r1 = sqlite3ExprCodeTemp(pParse, pX->pRight, &regFree1);
        sqlite3VdbeAddOp3(v, OP_ForceInt, r1, brk, 
                             pX->op==TK_LE || pX->op==TK_GT);
        sqlite3VdbeAddOp3(v, bRev ? OP_MoveLt : OP_MoveGe, iCur, brk, r1);
        VdbeComment((v, "pk"));
        sqlite3ReleaseTempReg(pParse, regFree1);
        disableTerm(pLevel, pStart);
      }else{
        sqlite3VdbeAddOp2(v, bRev ? OP_Last : OP_Rewind, iCur, brk);
      }
      if( pEnd ){
        Expr *pX;
        pX = pEnd->pExpr;
        assert( pX!=0 );
        assert( pEnd->leftCursor==iCur );
        pLevel->iMem = ++pParse->nMem;
        sqlite3ExprCode(pParse, pX->pRight, pLevel->iMem);
        if( pX->op==TK_LT || pX->op==TK_GT ){
          testOp = bRev ? OP_Le : OP_Ge;
        }else{
          testOp = bRev ? OP_Lt : OP_Gt;
        }
        disableTerm(pLevel, pEnd);
      }
      start = sqlite3VdbeCurrentAddr(v);
      pLevel->op = bRev ? OP_Prev : OP_Next;
      pLevel->p1 = iCur;
      pLevel->p2 = start;
      if( testOp!=OP_Noop ){
        int r1 = sqlite3GetTempReg(pParse);
        sqlite3VdbeAddOp2(v, OP_Rowid, iCur, r1);
        /* sqlite3VdbeAddOp2(v, OP_SCopy, pLevel->iMem, 0); */
        sqlite3VdbeAddOp3(v, testOp, pLevel->iMem, brk, r1);
        sqlite3VdbeChangeP5(v, SQLITE_AFF_NUMERIC | SQLITE_JUMPIFNULL);
        sqlite3ReleaseTempReg(pParse, r1);
      }
    }else if( pLevel->flags & (WHERE_COLUMN_RANGE|WHERE_COLUMN_EQ) ){
      /* Case 3: A scan using an index.
      **
      **         The WHERE clause may contain zero or more equality 
      **         terms ("==" or "IN" operators) that refer to the N
      **         left-most columns of the index. It may also contain
      **         inequality constraints (>, <, >= or <=) on the indexed
      **         column that immediately follows the N equalities. Only 
      **         the right-most column can be an inequality - the rest must
      **         use the "==" and "IN" operators. For example, if the 
      **         index is on (x,y,z), then the following clauses are all 
      **         optimized:
      **
      **            x=5
      **            x=5 AND y=10
      **            x=5 AND y<10
      **            x=5 AND y>5 AND y<10
      **            x=5 AND y=5 AND z<=10
      **
      **         The z<10 term of the following cannot be used, only
      **         the x=5 term:
      **
      **            x=5 AND z<10
      **
      **         N may be zero if there are inequality constraints.
      **         If there are no inequality constraints, then N is at
      **         least one.
      **
      **         This case is also used when there are no WHERE clause
      **         constraints but an index is selected anyway, in order
      **         to force the output order to conform to an ORDER BY.
      */  
      int aStartOp[] = {
        0,
        0,
        OP_Rewind,           /* 2: (!start_constraints && startEq &&  !bRev) */
        OP_Last,             /* 3: (!start_constraints && startEq &&   bRev) */
        OP_MoveGt,           /* 4: (start_constraints  && !startEq && !bRev) */
        OP_MoveLt,           /* 5: (start_constraints  && !startEq &&  bRev) */
        OP_MoveGe,           /* 6: (start_constraints  &&  startEq && !bRev) */
        OP_MoveLe            /* 7: (start_constraints  &&  startEq &&  bRev) */
      };
      int aEndOp[] = {
        OP_Noop,             /* 0: (!end_constraints) */
        OP_IdxGE,            /* 1: (end_constraints && !bRev) */
        OP_IdxLT             /* 2: (end_constraints && bRev) */
      };
      int nEq = pLevel->nEq;
      int isMinQuery = 0;          /* If this is an optimized SELECT min(x).. */
      int regBase;                 /* Base register holding constraint values */
      int r1;                      /* Temp register */
      WhereTerm *pRangeStart = 0;  /* Inequality constraint at range start */
      WhereTerm *pRangeEnd = 0;    /* Inequality constraint at range end */
      int startEq;                 /* True if range start uses ==, >= or <= */
      int endEq;                   /* True if range end uses ==, >= or <= */
      int start_constraints;       /* Start of range is constrained */
      int k = pIdx->aiColumn[nEq]; /* Column for inequality constraints */
      int nConstraint;             /* Number of constraint terms */
      int op;

      /* Generate code to evaluate all constraint terms using == or IN
      ** and store the values of those terms in an array of registers
      ** starting at regBase.
      */
      regBase = codeAllEqualityTerms(pParse, pLevel, &wc, notReady, 2);
      nxt = pLevel->nxt;

      /* If this loop satisfies a sort order (pOrderBy) request that 
      ** was passed to this function to implement a "SELECT min(x) ..." 
      ** query, then the caller will only allow the loop to run for
      ** a single iteration. This means that the first row returned
      ** should not have a NULL value stored in 'x'. If column 'x' is
      ** the first one after the nEq equality constraints in the index,
      ** this requires some special handling.
      */
      if( (wflags&WHERE_ORDERBY_MIN)!=0
       && (pLevel->flags&WHERE_ORDERBY)
       && (pIdx->nColumn>nEq)
      ){
        assert( pOrderBy->nExpr==1 );
        assert( pOrderBy->a[0].pExpr->iColumn==pIdx->aiColumn[nEq] );
        isMinQuery = 1;
      }

      /* Find any inequality constraint terms for the start and end 
      ** of the range. 
      */
      if( pLevel->flags & WHERE_TOP_LIMIT ){
        pRangeEnd = findTerm(&wc, iCur, k, notReady, (WO_LT|WO_LE), pIdx);
      }
      if( pLevel->flags & WHERE_BTM_LIMIT ){
        pRangeStart = findTerm(&wc, iCur, k, notReady, (WO_GT|WO_GE), pIdx);
      }

      /* If we are doing a reverse order scan on an ascending index, or
      ** a forward order scan on a descending index, interchange the 
      ** start and end terms (pRangeStart and pRangeEnd).
      */
      if( bRev==(pIdx->aSortOrder[nEq]==SQLITE_SO_ASC) ){
        SWAP(WhereTerm *, pRangeEnd, pRangeStart);
      }

      testcase( pRangeStart && pRangeStart->eOperator & WO_LE );
      testcase( pRangeStart && pRangeStart->eOperator & WO_GE );
      testcase( pRangeEnd && pRangeEnd->eOperator & WO_LE );
      testcase( pRangeEnd && pRangeEnd->eOperator & WO_GE );
      startEq = !pRangeStart || pRangeStart->eOperator & (WO_LE|WO_GE);
      endEq =   !pRangeEnd || pRangeEnd->eOperator & (WO_LE|WO_GE);
      start_constraints = pRangeStart || nEq>0;

      /* Seek the index cursor to the start of the range. */
      nConstraint = nEq;
      if( pRangeStart ){
        int dcc = pParse->disableColCache;
        if( pRangeEnd ){
          pParse->disableColCache++;
        }
        sqlite3ExprCode(pParse, pRangeStart->pExpr->pRight, regBase+nEq);
        pParse->disableColCache = dcc;
        sqlite3VdbeAddOp2(v, OP_IsNull, regBase+nEq, nxt);
        nConstraint++;
      }else if( isMinQuery ){
        sqlite3VdbeAddOp2(v, OP_Null, 0, regBase+nEq);
        nConstraint++;
        startEq = 0;
        start_constraints = 1;
      }
      codeApplyAffinity(pParse, regBase, nConstraint, pIdx);
      op = aStartOp[(start_constraints<<2) + (startEq<<1) + bRev];
      assert( op!=0 );
      testcase( op==OP_Rewind );
      testcase( op==OP_Last );
      testcase( op==OP_MoveGt );
      testcase( op==OP_MoveGe );
      testcase( op==OP_MoveLe );
      testcase( op==OP_MoveLt );
      sqlite3VdbeAddOp4(v, op, iIdxCur, nxt, regBase, 
                        SQLITE_INT_TO_PTR(nConstraint), P4_INT32);

      /* Load the value for the inequality constraint at the end of the
      ** range (if any).
      */
      nConstraint = nEq;
      if( pRangeEnd ){
        sqlite3ExprCode(pParse, pRangeEnd->pExpr->pRight, regBase+nEq);
        sqlite3VdbeAddOp2(v, OP_IsNull, regBase+nEq, nxt);
        codeApplyAffinity(pParse, regBase, nEq+1, pIdx);
        nConstraint++;
      }

      /* Top of the loop body */
      pLevel->p2 = sqlite3VdbeCurrentAddr(v);

      /* Check if the index cursor is past the end of the range. */
      op = aEndOp[(pRangeEnd || nEq) * (1 + bRev)];
      testcase( op==OP_Noop );
      testcase( op==OP_IdxGE );
      testcase( op==OP_IdxLT );
      sqlite3VdbeAddOp4(v, op, iIdxCur, nxt, regBase,
                        SQLITE_INT_TO_PTR(nConstraint), P4_INT32);
      sqlite3VdbeChangeP5(v, endEq!=bRev);

      /* If there are inequality constraints, check that the value
      ** of the table column that the inequality contrains is not NULL.
      ** If it is, jump to the next iteration of the loop.
      */
      r1 = sqlite3GetTempReg(pParse);
      testcase( pLevel->flags & WHERE_BTM_LIMIT );
      testcase( pLevel->flags & WHERE_TOP_LIMIT );
      if( pLevel->flags & (WHERE_BTM_LIMIT|WHERE_TOP_LIMIT) ){
        sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, nEq, r1);
        sqlite3VdbeAddOp2(v, OP_IsNull, r1, cont);
      }

      /* Seek the table cursor, if required */
      if( !omitTable ){
        sqlite3VdbeAddOp2(v, OP_IdxRowid, iIdxCur, r1);
        sqlite3VdbeAddOp3(v, OP_MoveGe, iCur, 0, r1);  /* Deferred seek */
      }
      sqlite3ReleaseTempReg(pParse, r1);

      /* Record the instruction used to terminate the loop. Disable 
      ** WHERE clause terms made redundant by the index range scan.
      */
      pLevel->op = bRev ? OP_Prev : OP_Next;
      pLevel->p1 = iIdxCur;
      disableTerm(pLevel, pRangeStart);
      disableTerm(pLevel, pRangeEnd);
    }else{
      /* Case 4:  There is no usable index.  We must do a complete
      **          scan of the entire table.
      */
      assert( omitTable==0 );
      assert( bRev==0 );
      pLevel->op = OP_Next;
      pLevel->p1 = iCur;
      pLevel->p2 = 1 + sqlite3VdbeAddOp2(v, OP_Rewind, iCur, brk);
    }
    notReady &= ~getMask(&maskSet, iCur);

    /* Insert code to test every subexpression that can be completely
    ** computed using the current set of tables.
    */
    for(pTerm=wc.a, j=wc.nTerm; j>0; j--, pTerm++){
      Expr *pE;
      testcase( pTerm->flags & TERM_VIRTUAL );
      testcase( pTerm->flags & TERM_CODED );
      if( pTerm->flags & (TERM_VIRTUAL|TERM_CODED) ) continue;
      if( (pTerm->prereqAll & notReady)!=0 ) continue;
      pE = pTerm->pExpr;
      assert( pE!=0 );
      if( pLevel->iLeftJoin && !ExprHasProperty(pE, EP_FromJoin) ){
        continue;
      }
      sqlite3ExprIfFalse(pParse, pE, cont, SQLITE_JUMPIFNULL);
      pTerm->flags |= TERM_CODED;
    }

    /* For a LEFT OUTER JOIN, generate code that will record the fact that
    ** at least one row of the right table has matched the left table.  
    */
    if( pLevel->iLeftJoin ){
      pLevel->top = sqlite3VdbeCurrentAddr(v);
      sqlite3VdbeAddOp2(v, OP_Integer, 1, pLevel->iLeftJoin);
      VdbeComment((v, "record LEFT JOIN hit"));
      sqlite3ExprClearColumnCache(pParse, pLevel->iTabCur);
      sqlite3ExprClearColumnCache(pParse, pLevel->iIdxCur);
      for(pTerm=wc.a, j=0; j<wc.nTerm; j++, pTerm++){
        testcase( pTerm->flags & TERM_VIRTUAL );
        testcase( pTerm->flags & TERM_CODED );
        if( pTerm->flags & (TERM_VIRTUAL|TERM_CODED) ) continue;
        if( (pTerm->prereqAll & notReady)!=0 ) continue;
        assert( pTerm->pExpr );
        sqlite3ExprIfFalse(pParse, pTerm->pExpr, cont, SQLITE_JUMPIFNULL);
        pTerm->flags |= TERM_CODED;
      }
    }
  }

#ifdef SQLITE_TEST  /* For testing and debugging use only */
  /* Record in the query plan information about the current table
  ** and the index used to access it (if any).  If the table itself
  ** is not used, its name is just '{}'.  If no index is used
  ** the index is listed as "{}".  If the primary key is used the
  ** index name is '*'.
  */
  for(i=0; i<pTabList->nSrc; i++){
    char *z;
    int n;
    pLevel = &pWInfo->a[i];
    pTabItem = &pTabList->a[pLevel->iFrom];
    z = pTabItem->zAlias;
    if( z==0 ) z = pTabItem->pTab->zName;
    n = strlen(z);
    if( n+nQPlan < sizeof(sqlite3_query_plan)-10 ){
      if( pLevel->flags & WHERE_IDX_ONLY ){
        memcpy(&sqlite3_query_plan[nQPlan], "{}", 2);
        nQPlan += 2;
      }else{
        memcpy(&sqlite3_query_plan[nQPlan], z, n);
        nQPlan += n;
      }
      sqlite3_query_plan[nQPlan++] = ' ';
    }
    testcase( pLevel->flags & WHERE_ROWID_EQ );
    testcase( pLevel->flags & WHERE_ROWID_RANGE );
    if( pLevel->flags & (WHERE_ROWID_EQ|WHERE_ROWID_RANGE) ){
      memcpy(&sqlite3_query_plan[nQPlan], "* ", 2);
      nQPlan += 2;
    }else if( pLevel->pIdx==0 ){
      memcpy(&sqlite3_query_plan[nQPlan], "{} ", 3);
      nQPlan += 3;
    }else{
      n = strlen(pLevel->pIdx->zName);
      if( n+nQPlan < sizeof(sqlite3_query_plan)-2 ){
        memcpy(&sqlite3_query_plan[nQPlan], pLevel->pIdx->zName, n);
        nQPlan += n;
        sqlite3_query_plan[nQPlan++] = ' ';
      }
    }
  }
  while( nQPlan>0 && sqlite3_query_plan[nQPlan-1]==' ' ){
    sqlite3_query_plan[--nQPlan] = 0;
  }
  sqlite3_query_plan[nQPlan] = 0;
  nQPlan = 0;
#endif /* SQLITE_TEST // Testing and debugging use only */

  /* Record the continuation address in the WhereInfo structure.  Then
  ** clean up and return.
  */
  pWInfo->iContinue = cont;
  whereClauseClear(&wc);
  return pWInfo;

  /* Jump here if malloc fails */
whereBeginNoMem:
  whereClauseClear(&wc);
  whereInfoFree(pWInfo);
  return 0;
}

/*
** Generate the end of the WHERE loop.  See comments on 
** sqlite3WhereBegin() for additional information.
*/
void sqlite3WhereEnd(WhereInfo *pWInfo){
  Parse *pParse = pWInfo->pParse;
  Vdbe *v = pParse->pVdbe;
  int i;
  WhereLevel *pLevel;
  SrcList *pTabList = pWInfo->pTabList;
  sqlite3 *db = pParse->db;

  /* Generate loop termination code.
  */
  sqlite3ExprClearColumnCache(pParse, -1);
  for(i=pTabList->nSrc-1; i>=0; i--){
    pLevel = &pWInfo->a[i];
    sqlite3VdbeResolveLabel(v, pLevel->cont);
    if( pLevel->op!=OP_Noop ){
      sqlite3VdbeAddOp2(v, pLevel->op, pLevel->p1, pLevel->p2);
    }
    if( pLevel->nIn ){
      struct InLoop *pIn;
      int j;
      sqlite3VdbeResolveLabel(v, pLevel->nxt);
      for(j=pLevel->nIn, pIn=&pLevel->aInLoop[j-1]; j>0; j--, pIn--){
        sqlite3VdbeJumpHere(v, pIn->topAddr+1);
        sqlite3VdbeAddOp2(v, OP_Next, pIn->iCur, pIn->topAddr);
        sqlite3VdbeJumpHere(v, pIn->topAddr-1);
      }
      sqlite3DbFree(db, pLevel->aInLoop);
    }
    sqlite3VdbeResolveLabel(v, pLevel->brk);
    if( pLevel->iLeftJoin ){
      int addr;
      addr = sqlite3VdbeAddOp1(v, OP_IfPos, pLevel->iLeftJoin);
      sqlite3VdbeAddOp1(v, OP_NullRow, pTabList->a[i].iCursor);
      if( pLevel->iIdxCur>=0 ){
        sqlite3VdbeAddOp1(v, OP_NullRow, pLevel->iIdxCur);
      }
      sqlite3VdbeAddOp2(v, OP_Goto, 0, pLevel->top);
      sqlite3VdbeJumpHere(v, addr);
    }
  }

  /* The "break" point is here, just past the end of the outer loop.
  ** Set it.
  */
  sqlite3VdbeResolveLabel(v, pWInfo->iBreak);

  /* Close all of the cursors that were opened by sqlite3WhereBegin.
  */
  for(i=0, pLevel=pWInfo->a; i<pTabList->nSrc; i++, pLevel++){
    struct SrcList_item *pTabItem = &pTabList->a[pLevel->iFrom];
    Table *pTab = pTabItem->pTab;
    assert( pTab!=0 );
    if( pTab->isEphem || pTab->pSelect ) continue;
    if( !pWInfo->okOnePass && (pLevel->flags & WHERE_IDX_ONLY)==0 ){
      sqlite3VdbeAddOp1(v, OP_Close, pTabItem->iCursor);
    }
    if( pLevel->pIdx!=0 ){
      sqlite3VdbeAddOp1(v, OP_Close, pLevel->iIdxCur);
    }

    /* If this scan uses an index, make code substitutions to read data
    ** from the index in preference to the table. Sometimes, this means
    ** the table need never be read from. This is a performance boost,
    ** as the vdbe level waits until the table is read before actually
    ** seeking the table cursor to the record corresponding to the current
    ** position in the index.
    ** 
    ** Calls to the code generator in between sqlite3WhereBegin and
    ** sqlite3WhereEnd will have created code that references the table
    ** directly.  This loop scans all that code looking for opcodes
    ** that reference the table and converts them into opcodes that
    ** reference the index.
    */
    if( pLevel->pIdx ){
      int k, j, last;
      VdbeOp *pOp;
      Index *pIdx = pLevel->pIdx;
      int useIndexOnly = pLevel->flags & WHERE_IDX_ONLY;

      assert( pIdx!=0 );
      pOp = sqlite3VdbeGetOp(v, pWInfo->iTop);
      last = sqlite3VdbeCurrentAddr(v);
      for(k=pWInfo->iTop; k<last; k++, pOp++){
        if( pOp->p1!=pLevel->iTabCur ) continue;
        if( pOp->opcode==OP_Column ){
          for(j=0; j<pIdx->nColumn; j++){
            if( pOp->p2==pIdx->aiColumn[j] ){
              pOp->p2 = j;
              pOp->p1 = pLevel->iIdxCur;
              break;
            }
          }
          assert(!useIndexOnly || j<pIdx->nColumn);
        }else if( pOp->opcode==OP_Rowid ){
          pOp->p1 = pLevel->iIdxCur;
          pOp->opcode = OP_IdxRowid;
        }else if( pOp->opcode==OP_NullRow && useIndexOnly ){
          pOp->opcode = OP_Noop;
        }
      }
    }
  }

  /* Final cleanup
  */
  whereInfoFree(pWInfo);
  return;
}
