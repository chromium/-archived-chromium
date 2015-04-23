/*
** 2003 September 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This is the header file for information that is private to the
** VDBE.  This information used to all be at the top of the single
** source code file "vdbe.c".  When that file became too big (over
** 6000 lines long) it was split up into several smaller files and
** this header information was factored out.
**
** $Id: vdbeInt.h,v 1.153 2008/08/02 03:50:39 drh Exp $
*/
#ifndef _VDBEINT_H_
#define _VDBEINT_H_

/*
** intToKey() and keyToInt() used to transform the rowid.  But with
** the latest versions of the design they are no-ops.
*/
#define keyToInt(X)   (X)
#define intToKey(X)   (X)


/*
** SQL is translated into a sequence of instructions to be
** executed by a virtual machine.  Each instruction is an instance
** of the following structure.
*/
typedef struct VdbeOp Op;

/*
** Boolean values
*/
typedef unsigned char Bool;

/*
** A cursor is a pointer into a single BTree within a database file.
** The cursor can seek to a BTree entry with a particular key, or
** loop over all entries of the Btree.  You can also insert new BTree
** entries or retrieve the key or data from the entry that the cursor
** is currently pointing to.
** 
** Every cursor that the virtual machine has open is represented by an
** instance of the following structure.
**
** If the Cursor.isTriggerRow flag is set it means that this cursor is
** really a single row that represents the NEW or OLD pseudo-table of
** a row trigger.  The data for the row is stored in Cursor.pData and
** the rowid is in Cursor.iKey.
*/
struct Cursor {
  BtCursor *pCursor;    /* The cursor structure of the backend */
  int iDb;              /* Index of cursor database in db->aDb[] (or -1) */
  i64 lastRowid;        /* Last rowid from a Next or NextIdx operation */
  i64 nextRowid;        /* Next rowid returned by OP_NewRowid */
  Bool zeroed;          /* True if zeroed out and ready for reuse */
  Bool rowidIsValid;    /* True if lastRowid is valid */
  Bool atFirst;         /* True if pointing to first entry */
  Bool useRandomRowid;  /* Generate new record numbers semi-randomly */
  Bool nullRow;         /* True if pointing to a row with no data */
  Bool nextRowidValid;  /* True if the nextRowid field is valid */
  Bool pseudoTable;     /* This is a NEW or OLD pseudo-tables of a trigger */
  Bool ephemPseudoTable;
  Bool deferredMoveto;  /* A call to sqlite3BtreeMoveto() is needed */
  Bool isTable;         /* True if a table requiring integer keys */
  Bool isIndex;         /* True if an index containing keys only - no data */
  u8 bogusIncrKey;      /* Something for pIncrKey to point to if pKeyInfo==0 */
  i64 movetoTarget;     /* Argument to the deferred sqlite3BtreeMoveto() */
  Btree *pBt;           /* Separate file holding temporary table */
  int nData;            /* Number of bytes in pData */
  char *pData;          /* Data for a NEW or OLD pseudo-table */
  i64 iKey;             /* Key for the NEW or OLD pseudo-table row */
  u8 *pIncrKey;         /* Pointer to pKeyInfo->incrKey */
  KeyInfo *pKeyInfo;    /* Info about index keys needed by index cursors */
  int nField;           /* Number of fields in the header */
  i64 seqCount;         /* Sequence counter */
  sqlite3_vtab_cursor *pVtabCursor;  /* The cursor for a virtual table */
  const sqlite3_module *pModule;     /* Module for cursor pVtabCursor */

  /* Cached information about the header for the data record that the
  ** cursor is currently pointing to.  Only valid if cacheValid is true.
  ** aRow might point to (ephemeral) data for the current row, or it might
  ** be NULL.
  */
  int cacheStatus;      /* Cache is valid if this matches Vdbe.cacheCtr */
  int payloadSize;      /* Total number of bytes in the record */
  u32 *aType;           /* Type values for all entries in the record */
  u32 *aOffset;         /* Cached offsets to the start of each columns data */
  u8 *aRow;             /* Data for the current row, if all on one page */
};
typedef struct Cursor Cursor;

/*
** A value for Cursor.cacheValid that means the cache is always invalid.
*/
#define CACHE_STALE 0

/*
** Internally, the vdbe manipulates nearly all SQL values as Mem
** structures. Each Mem struct may cache multiple representations (string,
** integer etc.) of the same value.  A value (and therefore Mem structure)
** has the following properties:
**
** Each value has a manifest type. The manifest type of the value stored
** in a Mem struct is returned by the MemType(Mem*) macro. The type is
** one of SQLITE_NULL, SQLITE_INTEGER, SQLITE_REAL, SQLITE_TEXT or
** SQLITE_BLOB.
*/
struct Mem {
  union {
    i64 i;              /* Integer value. Or FuncDef* when flags==MEM_Agg */
    FuncDef *pDef;      /* Used only when flags==MEM_Agg */
  } u;
  double r;           /* Real value */
  sqlite3 *db;        /* The associated database connection */
  char *z;            /* String or BLOB value */
  int n;              /* Number of characters in string value, excluding '\0' */
  u16 flags;          /* Some combination of MEM_Null, MEM_Str, MEM_Dyn, etc. */
  u8  type;           /* One of SQLITE_NULL, SQLITE_TEXT, SQLITE_INTEGER, etc */
  u8  enc;            /* SQLITE_UTF8, SQLITE_UTF16BE, SQLITE_UTF16LE */
  void (*xDel)(void *);  /* If not null, call this function to delete Mem.z */
  char *zMalloc;      /* Dynamic buffer allocated by sqlite3_malloc() */
};

/* One or more of the following flags are set to indicate the validOK
** representations of the value stored in the Mem struct.
**
** If the MEM_Null flag is set, then the value is an SQL NULL value.
** No other flags may be set in this case.
**
** If the MEM_Str flag is set then Mem.z points at a string representation.
** Usually this is encoded in the same unicode encoding as the main
** database (see below for exceptions). If the MEM_Term flag is also
** set, then the string is nul terminated. The MEM_Int and MEM_Real 
** flags may coexist with the MEM_Str flag.
**
** Multiple of these values can appear in Mem.flags.  But only one
** at a time can appear in Mem.type.
*/
#define MEM_Null      0x0001   /* Value is NULL */
#define MEM_Str       0x0002   /* Value is a string */
#define MEM_Int       0x0004   /* Value is an integer */
#define MEM_Real      0x0008   /* Value is a real number */
#define MEM_Blob      0x0010   /* Value is a BLOB */

#define MemSetTypeFlag(p, f) \
  ((p)->flags = ((p)->flags&~(MEM_Int|MEM_Real|MEM_Null|MEM_Blob|MEM_Str))|f)

/* Whenever Mem contains a valid string or blob representation, one of
** the following flags must be set to determine the memory management
** policy for Mem.z.  The MEM_Term flag tells us whether or not the
** string is \000 or \u0000 terminated
*/
#define MEM_Term      0x0020   /* String rep is nul terminated */
#define MEM_Dyn       0x0040   /* Need to call sqliteFree() on Mem.z */
#define MEM_Static    0x0080   /* Mem.z points to a static string */
#define MEM_Ephem     0x0100   /* Mem.z points to an ephemeral string */
#define MEM_Agg       0x0400   /* Mem.z points to an agg function context */
#define MEM_Zero      0x0800   /* Mem.i contains count of 0s appended to blob */

#ifdef SQLITE_OMIT_INCRBLOB
  #undef MEM_Zero
  #define MEM_Zero 0x0000
#endif


/* A VdbeFunc is just a FuncDef (defined in sqliteInt.h) that contains
** additional information about auxiliary information bound to arguments
** of the function.  This is used to implement the sqlite3_get_auxdata()
** and sqlite3_set_auxdata() APIs.  The "auxdata" is some auxiliary data
** that can be associated with a constant argument to a function.  This
** allows functions such as "regexp" to compile their constant regular
** expression argument once and reused the compiled code for multiple
** invocations.
*/
struct VdbeFunc {
  FuncDef *pFunc;               /* The definition of the function */
  int nAux;                     /* Number of entries allocated for apAux[] */
  struct AuxData {
    void *pAux;                   /* Aux data for the i-th argument */
    void (*xDelete)(void *);      /* Destructor for the aux data */
  } apAux[1];                   /* One slot for each function argument */
};

/*
** The "context" argument for a installable function.  A pointer to an
** instance of this structure is the first argument to the routines used
** implement the SQL functions.
**
** There is a typedef for this structure in sqlite.h.  So all routines,
** even the public interface to SQLite, can use a pointer to this structure.
** But this file is the only place where the internal details of this
** structure are known.
**
** This structure is defined inside of vdbeInt.h because it uses substructures
** (Mem) which are only defined there.
*/
struct sqlite3_context {
  FuncDef *pFunc;       /* Pointer to function information.  MUST BE FIRST */
  VdbeFunc *pVdbeFunc;  /* Auxilary data, if created. */
  Mem s;                /* The return value is stored here */
  Mem *pMem;            /* Memory cell used to store aggregate context */
  int isError;          /* Error code returned by the function. */
  CollSeq *pColl;       /* Collating sequence */
};

/*
** A Set structure is used for quick testing to see if a value
** is part of a small set.  Sets are used to implement code like
** this:
**            x.y IN ('hi','hoo','hum')
*/
typedef struct Set Set;
struct Set {
  Hash hash;             /* A set is just a hash table */
  HashElem *prev;        /* Previously accessed hash elemen */
};

/*
** A FifoPage structure holds a single page of valves.  Pages are arranged
** in a list.
*/
typedef struct FifoPage FifoPage;
struct FifoPage {
  int nSlot;         /* Number of entries aSlot[] */
  int iWrite;        /* Push the next value into this entry in aSlot[] */
  int iRead;         /* Read the next value from this entry in aSlot[] */
  FifoPage *pNext;   /* Next page in the fifo */
  i64 aSlot[1];      /* One or more slots for rowid values */
};

/*
** The Fifo structure is typedef-ed in vdbeInt.h.  But the implementation
** of that structure is private to this file.
**
** The Fifo structure describes the entire fifo.  
*/
typedef struct Fifo Fifo;
struct Fifo {
  int nEntry;         /* Total number of entries */
  sqlite3 *db;        /* The associated database connection */
  FifoPage *pFirst;   /* First page on the list */
  FifoPage *pLast;    /* Last page on the list */
};

/*
** A Context stores the last insert rowid, the last statement change count,
** and the current statement change count (i.e. changes since last statement).
** The current keylist is also stored in the context.
** Elements of Context structure type make up the ContextStack, which is
** updated by the ContextPush and ContextPop opcodes (used by triggers).
** The context is pushed before executing a trigger a popped when the
** trigger finishes.
*/
typedef struct Context Context;
struct Context {
  i64 lastRowid;    /* Last insert rowid (sqlite3.lastRowid) */
  int nChange;      /* Statement changes (Vdbe.nChanges)     */
  Fifo sFifo;       /* Records that will participate in a DELETE or UPDATE */
};

/*
** An instance of the virtual machine.  This structure contains the complete
** state of the virtual machine.
**
** The "sqlite3_stmt" structure pointer that is returned by sqlite3_compile()
** is really a pointer to an instance of this structure.
**
** The Vdbe.inVtabMethod variable is set to non-zero for the duration of
** any virtual table method invocations made by the vdbe program. It is
** set to 2 for xDestroy method calls and 1 for all other methods. This
** variable is used for two purposes: to allow xDestroy methods to execute
** "DROP TABLE" statements and to prevent some nasty side effects of
** malloc failure when SQLite is invoked recursively by a virtual table 
** method function.
*/
struct Vdbe {
  sqlite3 *db;        /* The whole database */
  Vdbe *pPrev,*pNext; /* Linked list of VDBEs with the same Vdbe.db */
  int nOp;            /* Number of instructions in the program */
  int nOpAlloc;       /* Number of slots allocated for aOp[] */
  Op *aOp;            /* Space to hold the virtual machine's program */
  int nLabel;         /* Number of labels used */
  int nLabelAlloc;    /* Number of slots allocated in aLabel[] */
  int *aLabel;        /* Space to hold the labels */
  Mem **apArg;        /* Arguments to currently executing user function */
  Mem *aColName;      /* Column names to return */
  int nCursor;        /* Number of slots in apCsr[] */
  Cursor **apCsr;     /* One element of this array for each open cursor */
  int nVar;           /* Number of entries in aVar[] */
  Mem *aVar;          /* Values for the OP_Variable opcode. */
  char **azVar;       /* Name of variables */
  int okVar;          /* True if azVar[] has been initialized */
  int magic;              /* Magic number for sanity checking */
  int nMem;               /* Number of memory locations currently allocated */
  Mem *aMem;              /* The memory locations */
  int nCallback;          /* Number of callbacks invoked so far */
  int cacheCtr;           /* Cursor row cache generation counter */
  Fifo sFifo;             /* A list of ROWIDs */
  int contextStackTop;    /* Index of top element in the context stack */
  int contextStackDepth;  /* The size of the "context" stack */
  Context *contextStack;  /* Stack used by opcodes ContextPush & ContextPop*/
  int pc;                 /* The program counter */
  int rc;                 /* Value to return */
  unsigned uniqueCnt;     /* Used by OP_MakeRecord when P2!=0 */
  int errorAction;        /* Recovery action to do in case of an error */
  int inTempTrans;        /* True if temp database is transactioned */
  int nResColumn;         /* Number of columns in one row of the result set */
  char **azResColumn;     /* Values for one row of result */ 
  char *zErrMsg;          /* Error message written here */
  Mem *pResultSet;        /* Pointer to an array of results */
  u8 explain;             /* True if EXPLAIN present on SQL command */
  u8 changeCntOn;         /* True to update the change-counter */
  u8 expired;             /* True if the VM needs to be recompiled */
  u8 minWriteFileFormat;  /* Minimum file format for writable database files */
  u8 inVtabMethod;        /* See comments above */
  int nChange;            /* Number of db changes made since last reset */
  i64 startTime;          /* Time when query started - used for profiling */
  int btreeMask;          /* Bitmask of db->aDb[] entries referenced */
  BtreeMutexArray aMutex; /* An array of Btree used here and needing locks */
  int nSql;             /* Number of bytes in zSql */
  char *zSql;           /* Text of the SQL statement that generated this */
#ifdef SQLITE_DEBUG
  FILE *trace;        /* Write an execution trace here, if not NULL */
#endif
  int openedStatement;  /* True if this VM has opened a statement journal */
#ifdef SQLITE_SSE
  int fetchId;          /* Statement number used by sqlite3_fetch_statement */
  int lru;              /* Counter used for LRU cache replacement */
#endif
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
  Vdbe *pLruPrev;
  Vdbe *pLruNext;
#endif
};

/*
** An instance of the following structure holds information about a
** single index record that has already been parsed out into individual
** values.
**
** A record is an object that contains one or more fields of data.
** Records are used to store the content of a table row and to store
** the key of an index.  A blob encoding of a record is created by
** the OP_MakeRecord opcode of the VDBE and is disassemblied by the
** OP_Column opcode.
**
** This structure holds a record that has already been disassembled
** into its constitutent fields.
*/
struct UnpackedRecord {
  KeyInfo *pKeyInfo;  /* Collation and sort-order information */
  u16 nField;         /* Number of entries in apMem[] */
  u8 needFree;        /* True if memory obtained from sqlite3_malloc() */
  u8 needDestroy;     /* True if apMem[]s should be destroyed on close */
  Mem *aMem;          /* Values */
};

/*
** The following are allowed values for Vdbe.magic
*/
#define VDBE_MAGIC_INIT     0x26bceaa5    /* Building a VDBE program */
#define VDBE_MAGIC_RUN      0xbdf20da3    /* VDBE is ready to execute */
#define VDBE_MAGIC_HALT     0x519c2973    /* VDBE has completed execution */
#define VDBE_MAGIC_DEAD     0xb606c3c8    /* The VDBE has been deallocated */

/*
** Function prototypes
*/
void sqlite3VdbeFreeCursor(Vdbe *, Cursor*);
void sqliteVdbePopStack(Vdbe*,int);
int sqlite3VdbeCursorMoveto(Cursor*);
#if defined(SQLITE_DEBUG) || defined(VDBE_PROFILE)
void sqlite3VdbePrintOp(FILE*, int, Op*);
#endif
int sqlite3VdbeSerialTypeLen(u32);
u32 sqlite3VdbeSerialType(Mem*, int);
int sqlite3VdbeSerialPut(unsigned char*, int, Mem*, int);
int sqlite3VdbeSerialGet(const unsigned char*, u32, Mem*);
void sqlite3VdbeDeleteAuxData(VdbeFunc*, int);

int sqlite2BtreeKeyCompare(BtCursor *, const void *, int, int, int *);
int sqlite3VdbeIdxKeyCompare(Cursor*,UnpackedRecord *,int,const unsigned char*,int*);
int sqlite3VdbeIdxRowid(BtCursor *, i64 *);
int sqlite3MemCompare(const Mem*, const Mem*, const CollSeq*);
int sqlite3VdbeIdxRowidLen(const u8*, int, int*);
int sqlite3VdbeExec(Vdbe*);
int sqlite3VdbeList(Vdbe*);
int sqlite3VdbeHalt(Vdbe*);
int sqlite3VdbeChangeEncoding(Mem *, int);
int sqlite3VdbeMemTooBig(Mem*);
int sqlite3VdbeMemCopy(Mem*, const Mem*);
void sqlite3VdbeMemShallowCopy(Mem*, const Mem*, int);
void sqlite3VdbeMemMove(Mem*, Mem*);
int sqlite3VdbeMemNulTerminate(Mem*);
int sqlite3VdbeMemSetStr(Mem*, const char*, int, u8, void(*)(void*));
void sqlite3VdbeMemSetInt64(Mem*, i64);
void sqlite3VdbeMemSetDouble(Mem*, double);
void sqlite3VdbeMemSetNull(Mem*);
void sqlite3VdbeMemSetZeroBlob(Mem*,int);
int sqlite3VdbeMemMakeWriteable(Mem*);
int sqlite3VdbeMemStringify(Mem*, int);
i64 sqlite3VdbeIntValue(Mem*);
int sqlite3VdbeMemIntegerify(Mem*);
double sqlite3VdbeRealValue(Mem*);
void sqlite3VdbeIntegerAffinity(Mem*);
int sqlite3VdbeMemRealify(Mem*);
int sqlite3VdbeMemNumerify(Mem*);
int sqlite3VdbeMemFromBtree(BtCursor*,int,int,int,Mem*);
void sqlite3VdbeMemRelease(Mem *p);
void sqlite3VdbeMemReleaseExternal(Mem *p);
int sqlite3VdbeMemFinalize(Mem*, FuncDef*);
const char *sqlite3OpcodeName(int);
int sqlite3VdbeOpcodeHasProperty(int, int);
int sqlite3VdbeMemGrow(Mem *pMem, int n, int preserve);
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
int sqlite3VdbeReleaseBuffers(Vdbe *p);
#endif

#ifndef NDEBUG
  void sqlite3VdbeMemSanity(Mem*);
#endif
int sqlite3VdbeMemTranslate(Mem*, u8);
#ifdef SQLITE_DEBUG
  void sqlite3VdbePrintSql(Vdbe*);
  void sqlite3VdbeMemPrettyPrint(Mem *pMem, char *zBuf);
#endif
int sqlite3VdbeMemHandleBom(Mem *pMem);
void sqlite3VdbeFifoInit(Fifo*, sqlite3*);
int sqlite3VdbeFifoPush(Fifo*, i64);
int sqlite3VdbeFifoPop(Fifo*, i64*);
void sqlite3VdbeFifoClear(Fifo*);

#ifndef SQLITE_OMIT_INCRBLOB
  int sqlite3VdbeMemExpandBlob(Mem *);
#else
  #define sqlite3VdbeMemExpandBlob(x) SQLITE_OK
#endif

#endif /* !defined(_VDBEINT_H_) */
