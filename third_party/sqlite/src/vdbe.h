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
** Header file for the Virtual DataBase Engine (VDBE)
**
** This header defines the interface to the virtual database engine
** or VDBE.  The VDBE implements an abstract machine that runs a
** simple program to access and modify the underlying database.
**
** $Id: vdbe.h,v 1.135 2008/08/01 20:10:08 drh Exp $
*/
#ifndef _SQLITE_VDBE_H_
#define _SQLITE_VDBE_H_
#include <stdio.h>

/*
** A single VDBE is an opaque structure named "Vdbe".  Only routines
** in the source file sqliteVdbe.c are allowed to see the insides
** of this structure.
*/
typedef struct Vdbe Vdbe;

/*
** The names of the following types declared in vdbeInt.h are required
** for the VdbeOp definition.
*/
typedef struct VdbeFunc VdbeFunc;
typedef struct Mem Mem;
typedef struct UnpackedRecord UnpackedRecord;

/*
** A single instruction of the virtual machine has an opcode
** and as many as three operands.  The instruction is recorded
** as an instance of the following structure:
*/
struct VdbeOp {
  u8 opcode;          /* What operation to perform */
  signed char p4type; /* One of the P4_xxx constants for p4 */
  u8 opflags;         /* Not currently used */
  u8 p5;              /* Fifth parameter is an unsigned character */
  int p1;             /* First operand */
  int p2;             /* Second parameter (often the jump destination) */
  int p3;             /* The third parameter */
  union {             /* forth parameter */
    int i;                 /* Integer value if p4type==P4_INT32 */
    void *p;               /* Generic pointer */
    char *z;               /* Pointer to data for string (char array) types */
    i64 *pI64;             /* Used when p4type is P4_INT64 */
    double *pReal;         /* Used when p4type is P4_REAL */
    FuncDef *pFunc;        /* Used when p4type is P4_FUNCDEF */
    VdbeFunc *pVdbeFunc;   /* Used when p4type is P4_VDBEFUNC */
    CollSeq *pColl;        /* Used when p4type is P4_COLLSEQ */
    Mem *pMem;             /* Used when p4type is P4_MEM */
    sqlite3_vtab *pVtab;   /* Used when p4type is P4_VTAB */
    KeyInfo *pKeyInfo;     /* Used when p4type is P4_KEYINFO */
    int *ai;               /* Used when p4type is P4_INTARRAY */
  } p4;
#ifdef SQLITE_DEBUG
  char *zComment;          /* Comment to improve readability */
#endif
#ifdef VDBE_PROFILE
  int cnt;                 /* Number of times this instruction was executed */
  u64 cycles;              /* Total time spent executing this instruction */
#endif
};
typedef struct VdbeOp VdbeOp;

/*
** A smaller version of VdbeOp used for the VdbeAddOpList() function because
** it takes up less space.
*/
struct VdbeOpList {
  u8 opcode;          /* What operation to perform */
  signed char p1;     /* First operand */
  signed char p2;     /* Second parameter (often the jump destination) */
  signed char p3;     /* Third parameter */
};
typedef struct VdbeOpList VdbeOpList;

/*
** Allowed values of VdbeOp.p3type
*/
#define P4_NOTUSED    0   /* The P4 parameter is not used */
#define P4_DYNAMIC  (-1)  /* Pointer to a string obtained from sqliteMalloc() */
#define P4_STATIC   (-2)  /* Pointer to a static string */
#define P4_COLLSEQ  (-4)  /* P4 is a pointer to a CollSeq structure */
#define P4_FUNCDEF  (-5)  /* P4 is a pointer to a FuncDef structure */
#define P4_KEYINFO  (-6)  /* P4 is a pointer to a KeyInfo structure */
#define P4_VDBEFUNC (-7)  /* P4 is a pointer to a VdbeFunc structure */
#define P4_MEM      (-8)  /* P4 is a pointer to a Mem*    structure */
#define P4_TRANSIENT (-9) /* P4 is a pointer to a transient string */
#define P4_VTAB     (-10) /* P4 is a pointer to an sqlite3_vtab structure */
#define P4_MPRINTF  (-11) /* P4 is a string obtained from sqlite3_mprintf() */
#define P4_REAL     (-12) /* P4 is a 64-bit floating point value */
#define P4_INT64    (-13) /* P4 is a 64-bit signed integer */
#define P4_INT32    (-14) /* P4 is a 32-bit signed integer */
#define P4_INTARRAY (-15) /* P4 is a vector of 32-bit integers */

/* When adding a P4 argument using P4_KEYINFO, a copy of the KeyInfo structure
** is made.  That copy is freed when the Vdbe is finalized.  But if the
** argument is P4_KEYINFO_HANDOFF, the passed in pointer is used.  It still
** gets freed when the Vdbe is finalized so it still should be obtained
** from a single sqliteMalloc().  But no copy is made and the calling
** function should *not* try to free the KeyInfo.
*/
#define P4_KEYINFO_HANDOFF (-16)
#define P4_KEYINFO_STATIC  (-17)

/*
** The Vdbe.aColName array contains 5n Mem structures, where n is the 
** number of columns of data returned by the statement.
*/
#define COLNAME_NAME     0
#define COLNAME_DECLTYPE 1
#define COLNAME_DATABASE 2
#define COLNAME_TABLE    3
#define COLNAME_COLUMN   4
#ifdef SQLITE_ENABLE_COLUMN_METADATA
# define COLNAME_N        5      /* Number of COLNAME_xxx symbols */
#else
# ifdef SQLITE_OMIT_DECLTYPE
#   define COLNAME_N      1      /* Store only the name */
# else
#   define COLNAME_N      2      /* Store the name and decltype */
# endif
#endif

/*
** The following macro converts a relative address in the p2 field
** of a VdbeOp structure into a negative number so that 
** sqlite3VdbeAddOpList() knows that the address is relative.  Calling
** the macro again restores the address.
*/
#define ADDR(X)  (-1-(X))

/*
** The makefile scans the vdbe.c source file and creates the "opcodes.h"
** header file that defines a number for each opcode used by the VDBE.
*/
#include "opcodes.h"

/*
** Prototypes for the VDBE interface.  See comments on the implementation
** for a description of what each of these routines does.
*/
Vdbe *sqlite3VdbeCreate(sqlite3*);
int sqlite3VdbeAddOp0(Vdbe*,int);
int sqlite3VdbeAddOp1(Vdbe*,int,int);
int sqlite3VdbeAddOp2(Vdbe*,int,int,int);
int sqlite3VdbeAddOp3(Vdbe*,int,int,int,int);
int sqlite3VdbeAddOp4(Vdbe*,int,int,int,int,const char *zP4,int);
int sqlite3VdbeAddOpList(Vdbe*, int nOp, VdbeOpList const *aOp);
void sqlite3VdbeChangeP1(Vdbe*, int addr, int P1);
void sqlite3VdbeChangeP2(Vdbe*, int addr, int P2);
void sqlite3VdbeChangeP3(Vdbe*, int addr, int P3);
void sqlite3VdbeChangeP5(Vdbe*, u8 P5);
void sqlite3VdbeJumpHere(Vdbe*, int addr);
void sqlite3VdbeChangeToNoop(Vdbe*, int addr, int N);
void sqlite3VdbeChangeP4(Vdbe*, int addr, const char *zP4, int N);
void sqlite3VdbeUsesBtree(Vdbe*, int);
VdbeOp *sqlite3VdbeGetOp(Vdbe*, int);
int sqlite3VdbeMakeLabel(Vdbe*);
void sqlite3VdbeDelete(Vdbe*);
void sqlite3VdbeMakeReady(Vdbe*,int,int,int,int);
int sqlite3VdbeFinalize(Vdbe*);
void sqlite3VdbeResolveLabel(Vdbe*, int);
int sqlite3VdbeCurrentAddr(Vdbe*);
#ifdef SQLITE_DEBUG
  void sqlite3VdbeTrace(Vdbe*,FILE*);
#endif
void sqlite3VdbeResetStepResult(Vdbe*);
int sqlite3VdbeReset(Vdbe*);
void sqlite3VdbeSetNumCols(Vdbe*,int);
int sqlite3VdbeSetColName(Vdbe*, int, int, const char *, int);
void sqlite3VdbeCountChanges(Vdbe*);
sqlite3 *sqlite3VdbeDb(Vdbe*);
void sqlite3VdbeSetSql(Vdbe*, const char *z, int n);
void sqlite3VdbeSwap(Vdbe*,Vdbe*);

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
int sqlite3VdbeReleaseMemory(int);
#endif
UnpackedRecord *sqlite3VdbeRecordUnpack(KeyInfo*,int,const void*,void*,int);
void sqlite3VdbeDeleteUnpackedRecord(UnpackedRecord*);
int sqlite3VdbeRecordCompare(int,const void*,UnpackedRecord*);


#ifndef NDEBUG
  void sqlite3VdbeComment(Vdbe*, const char*, ...);
# define VdbeComment(X)  sqlite3VdbeComment X
  void sqlite3VdbeNoopComment(Vdbe*, const char*, ...);
# define VdbeNoopComment(X)  sqlite3VdbeNoopComment X
#else
# define VdbeComment(X)
# define VdbeNoopComment(X)
#endif

#endif
