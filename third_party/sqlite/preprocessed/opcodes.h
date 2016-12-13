/* Automatically generated.  Do not edit */
/* See the mkopcodeh.awk script for details */
#define OP_VNext                                1
#define OP_Affinity                             2
#define OP_Column                               3
#define OP_SetCookie                            4
#define OP_Real                               125   /* same as TK_FLOAT    */
#define OP_Sequence                             5
#define OP_MoveGt                               6
#define OP_Ge                                  72   /* same as TK_GE       */
#define OP_RowKey                               7
#define OP_SCopy                                8
#define OP_Eq                                  68   /* same as TK_EQ       */
#define OP_OpenWrite                            9
#define OP_NotNull                             66   /* same as TK_NOTNULL  */
#define OP_If                                  10
#define OP_ToInt                              141   /* same as TK_TO_INT   */
#define OP_String8                             88   /* same as TK_STRING   */
#define OP_VRowid                              11
#define OP_CollSeq                             12
#define OP_OpenRead                            13
#define OP_Expire                              14
#define OP_AutoCommit                          15
#define OP_Gt                                  69   /* same as TK_GT       */
#define OP_Pagecount                           17
#define OP_IntegrityCk                         18
#define OP_Sort                                19
#define OP_Copy                                20
#define OP_Trace                               21
#define OP_Function                            22
#define OP_IfNeg                               23
#define OP_And                                 61   /* same as TK_AND      */
#define OP_Subtract                            79   /* same as TK_MINUS    */
#define OP_Noop                                24
#define OP_Return                              25
#define OP_Remainder                           82   /* same as TK_REM      */
#define OP_NewRowid                            26
#define OP_Multiply                            80   /* same as TK_STAR     */
#define OP_Variable                            27
#define OP_String                              28
#define OP_RealAffinity                        29
#define OP_VRename                             30
#define OP_ParseSchema                         31
#define OP_VOpen                               32
#define OP_Close                               33
#define OP_CreateIndex                         34
#define OP_IsUnique                            35
#define OP_NotFound                            36
#define OP_Int64                               37
#define OP_MustBeInt                           38
#define OP_Halt                                39
#define OP_Rowid                               40
#define OP_IdxLT                               41
#define OP_AddImm                              42
#define OP_Statement                           43
#define OP_RowData                             44
#define OP_MemMax                              45
#define OP_Or                                  60   /* same as TK_OR       */
#define OP_NotExists                           46
#define OP_Gosub                               47
#define OP_Divide                              81   /* same as TK_SLASH    */
#define OP_Integer                             48
#define OP_ToNumeric                          140   /* same as TK_TO_NUMERIC*/
#define OP_Prev                                49
#define OP_Concat                              83   /* same as TK_CONCAT   */
#define OP_BitAnd                              74   /* same as TK_BITAND   */
#define OP_VColumn                             50
#define OP_CreateTable                         51
#define OP_Last                                52
#define OP_IsNull                              65   /* same as TK_ISNULL   */
#define OP_IncrVacuum                          53
#define OP_IdxRowid                            54
#define OP_ShiftRight                          77   /* same as TK_RSHIFT   */
#define OP_ResetCount                          55
#define OP_FifoWrite                           56
#define OP_ContextPush                         57
#define OP_Yield                               58
#define OP_DropTrigger                         59
#define OP_DropIndex                           62
#define OP_IdxGE                               63
#define OP_IdxDelete                           64
#define OP_Vacuum                              73
#define OP_MoveLe                              84
#define OP_IfNot                               85
#define OP_DropTable                           86
#define OP_MakeRecord                          89
#define OP_ToBlob                             139   /* same as TK_TO_BLOB  */
#define OP_ResultRow                           90
#define OP_Delete                              91
#define OP_AggFinal                            92
#define OP_Compare                             93
#define OP_ShiftLeft                           76   /* same as TK_LSHIFT   */
#define OP_Goto                                94
#define OP_TableLock                           95
#define OP_FifoRead                            96
#define OP_Clear                               97
#define OP_MoveLt                              98
#define OP_Le                                  70   /* same as TK_LE       */
#define OP_VerifyCookie                        99
#define OP_AggStep                            100
#define OP_ToText                             138   /* same as TK_TO_TEXT  */
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_ToReal                             142   /* same as TK_TO_REAL  */
#define OP_SetNumColumns                      101
#define OP_Transaction                        102
#define OP_VFilter                            103
#define OP_Ne                                  67   /* same as TK_NE       */
#define OP_VDestroy                           104
#define OP_ContextPop                         105
#define OP_BitOr                               75   /* same as TK_BITOR    */
#define OP_Next                               106
#define OP_IdxInsert                          107
#define OP_Lt                                  71   /* same as TK_LT       */
#define OP_Insert                             108
#define OP_Destroy                            109
#define OP_ReadCookie                         110
#define OP_ForceInt                           111
#define OP_LoadAnalysis                       112
#define OP_Explain                            113
#define OP_OpenPseudo                         114
#define OP_OpenEphemeral                      115
#define OP_Null                               116
#define OP_Move                               117
#define OP_Blob                               118
#define OP_Add                                 78   /* same as TK_PLUS     */
#define OP_Rewind                             119
#define OP_MoveGe                             120
#define OP_VBegin                             121
#define OP_VUpdate                            122
#define OP_IfZero                             123
#define OP_BitNot                              87   /* same as TK_BITNOT   */
#define OP_VCreate                            124
#define OP_Found                              126
#define OP_IfPos                              127
#define OP_NullRow                            128
#define OP_Jump                               129
#define OP_Permutation                        130

/* The following opcode values are never used */
#define OP_NotUsed_131                        131
#define OP_NotUsed_132                        132
#define OP_NotUsed_133                        133
#define OP_NotUsed_134                        134
#define OP_NotUsed_135                        135
#define OP_NotUsed_136                        136
#define OP_NotUsed_137                        137


/* Properties such as "out2" or "jump" that are specified in
** comments following the "case" for each opcode in the vdbe.c
** are encoded into bitvectors as follows:
*/
#define OPFLG_JUMP            0x0001  /* jump:  P2 holds jmp target */
#define OPFLG_OUT2_PRERELEASE 0x0002  /* out2-prerelease: */
#define OPFLG_IN1             0x0004  /* in1:   P1 is an input */
#define OPFLG_IN2             0x0008  /* in2:   P2 is an input */
#define OPFLG_IN3             0x0010  /* in3:   P3 is an input */
#define OPFLG_OUT3            0x0020  /* out3:  P3 is an output */
#define OPFLG_INITIALIZER {\
/*   0 */ 0x00, 0x01, 0x00, 0x00, 0x10, 0x02, 0x11, 0x00,\
/*   8 */ 0x00, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00,\
/*  16 */ 0x04, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05,\
/*  24 */ 0x00, 0x04, 0x02, 0x02, 0x02, 0x04, 0x00, 0x00,\
/*  32 */ 0x00, 0x00, 0x02, 0x11, 0x11, 0x02, 0x05, 0x00,\
/*  40 */ 0x02, 0x11, 0x04, 0x00, 0x00, 0x0c, 0x11, 0x01,\
/*  48 */ 0x02, 0x01, 0x00, 0x02, 0x01, 0x01, 0x02, 0x00,\
/*  56 */ 0x04, 0x00, 0x00, 0x00, 0x2c, 0x2c, 0x00, 0x11,\
/*  64 */ 0x00, 0x05, 0x05, 0x15, 0x15, 0x15, 0x15, 0x15,\
/*  72 */ 0x15, 0x00, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,\
/*  80 */ 0x2c, 0x2c, 0x2c, 0x2c, 0x11, 0x05, 0x00, 0x04,\
/*  88 */ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,\
/*  96 */ 0x01, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x01,\
/* 104 */ 0x00, 0x00, 0x01, 0x08, 0x00, 0x02, 0x02, 0x05,\
/* 112 */ 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x01,\
/* 120 */ 0x11, 0x00, 0x00, 0x05, 0x00, 0x02, 0x11, 0x05,\
/* 128 */ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
/* 136 */ 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04,}
