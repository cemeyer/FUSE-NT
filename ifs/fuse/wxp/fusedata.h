/*++

Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    FuseData.c

Abstract:

    This module declares the global data used by the Fuse file system.


--*/

#ifndef _FUSEDATA_
#define _FUSEDATA_

extern IO_STATUS_BLOCK FuseGarbageIosb;

extern NPAGED_LOOKASIDE_LIST FuseIrpContextLookasideList;
extern NPAGED_LOOKASIDE_LIST FuseNonPagedFcbLookasideList;
extern NPAGED_LOOKASIDE_LIST FuseEResourceLookasideList;

extern PAGED_LOOKASIDE_LIST FuseFcbLookasideList;
extern PAGED_LOOKASIDE_LIST FuseCcbLookasideList;

extern SLIST_HEADER FuseCloseContextSList;
extern FAST_MUTEX FuseCloseQueueMutex;

//
//  The global Fuse debug level variable, its values are:
//
//      0x00000000      Always gets printed (used when about to bug check)
//
//      0x00000001      Error conditions
//      0x00000002      Debug hooks
//      0x00000004      Catch exceptions before completing Irp
//      0x00000008
//
//      0x00000010
//      0x00000020
//      0x00000040
//      0x00000080
//
//      0x00000100
//      0x00000200
//      0x00000400
//      0x00000800
//
//      0x00001000
//      0x00002000
//      0x00004000
//      0x00008000
//
//      0x00010000
//      0x00020000
//      0x00040000
//      0x00080000
//
//      0x00100000
//      0x00200000
//      0x00400000
//      0x00800000
//
//      0x01000000
//      0x02000000
//      0x04000000
//      0x08000000
//
//      0x10000000
//      0x20000000
//      0x40000000
//      0x80000000
//

#ifdef FUSEDBG

#define DEBUG_TRACE_ERROR                (0x00000001)
#define DEBUG_TRACE_DEBUG_HOOKS          (0x00000002)
#define DEBUG_TRACE_CATCH_EXCEPTIONS     (0x00000004)
#define DEBUG_TRACE_UNWIND               (0x00000008)
#define DEBUG_TRACE_CLEANUP              (0x00000010)
#define DEBUG_TRACE_CLOSE                (0x00000020)
#define DEBUG_TRACE_CREATE               (0x00000040)
#define DEBUG_TRACE_DIRCTRL              (0x00000080)
#define DEBUG_TRACE_EA                   (0x00000100)
#define DEBUG_TRACE_FILEINFO             (0x00000200)
#define DEBUG_TRACE_FSCTRL               (0x00000400)
#define DEBUG_TRACE_LOCKCTRL             (0x00000800)
#define DEBUG_TRACE_READ                 (0x00001000)
#define DEBUG_TRACE_VOLINFO              (0x00002000)
#define DEBUG_TRACE_WRITE                (0x00004000)
#define DEBUG_TRACE_FLUSH                (0x00008000)
#define DEBUG_TRACE_DEVCTRL              (0x00010000)
#define DEBUG_TRACE_SHUTDOWN             (0x00020000)
#define DEBUG_TRACE_FUSEDATA              (0x00040000)
#define DEBUG_TRACE_PNP                  (0x00080000)
#define DEBUG_TRACE_ACCHKSUP             (0x00100000)
#define DEBUG_TRACE_ALLOCSUP             (0x00200000)
#define DEBUG_TRACE_DIRSUP               (0x00400000)
#define DEBUG_TRACE_FILOBSUP             (0x00800000)
#define DEBUG_TRACE_NAMESUP              (0x01000000)
#define DEBUG_TRACE_VERFYSUP             (0x02000000)
#define DEBUG_TRACE_CACHESUP             (0x04000000)
#define DEBUG_TRACE_SPLAYSUP             (0x08000000)
#define DEBUG_TRACE_DEVIOSUP             (0x10000000)
#define DEBUG_TRACE_STRUCSUP             (0x20000000)
#define DEBUG_TRACE_FSP_DISPATCHER       (0x40000000)
#define DEBUG_TRACE_FSP_DUMP             (0x80000000)

extern LONG FuseDebugTraceLevel;
extern LONG FuseDebugTraceIndent;

#define DebugTrace(INDENT,LEVEL,X,Y) {                      \
    LONG _i;                                                \
    if (((LEVEL) == 0) || (FuseDebugTraceLevel & (LEVEL))) { \
        _i = (ULONG)PsGetCurrentThread();                   \
        DbgPrint("%08lx:",_i);                              \
        if ((INDENT) < 0) {                                 \
            FuseDebugTraceIndent += (INDENT);                \
        }                                                   \
        if (FuseDebugTraceIndent < 0) {                      \
            FuseDebugTraceIndent = 0;                        \
        }                                                   \
        for (_i = 0; _i < FuseDebugTraceIndent; _i += 1) {   \
            DbgPrint(" ");                                  \
        }                                                   \
        DbgPrint(X,Y);                                      \
        if ((INDENT) > 0) {                                 \
            FuseDebugTraceIndent += (INDENT);                \
        }                                                   \
    }                                                       \
}

#define DebugDump(STR,LEVEL,PTR) {                          \
    ULONG _i;                                               \
    VOID FuseDump(IN PVOID Ptr);                             \
    if (((LEVEL) == 0) || (FuseDebugTraceLevel & (LEVEL))) { \
        _i = (ULONG)PsGetCurrentThread();                   \
        DbgPrint("%08lx:",_i);                              \
        DbgPrint(STR);                                      \
        if (PTR != NULL) {FuseDump(PTR);}                    \
        DbgBreakPoint();                                    \
    }                                                       \
}

#define DebugUnwind(X) {                                                      \
    if (AbnormalTermination()) {                                             \
        DebugTrace(0, DEBUG_TRACE_UNWIND, #X ", Abnormal termination.\n", 0); \
    }                                                                         \
}

//
//  The following variables are used to keep track of the total amount
//  of requests processed by the file system, and the number of requests
//  that end up being processed by the Fsp thread.  The first variable
//  is incremented whenever an Irp context is created (which is always
//  at the start of an Fsd entry point) and the second is incremented
//  by read request.
//

extern ULONG FuseFsdEntryCount;
extern ULONG FuseFspEntryCount;
extern ULONG FuseIoCallDriverCount;
extern ULONG FuseTotalTicks[];

#define DebugDoit(X)                     {X;}

extern LONG FusePerformanceTimerLevel;

#define TimerStart(LEVEL) {                     \
    LARGE_INTEGER TStart, TEnd;                 \
    LARGE_INTEGER TElapsed;                     \
    TStart = KeQueryPerformanceCounter( NULL ); \

#define TimerStop(LEVEL,s)                                    \
    TEnd = KeQueryPerformanceCounter( NULL );                 \
    TElapsed.QuadPart = TEnd.QuadPart - TStart.QuadPart;      \
    FuseTotalTicks[FuseLogOf(LEVEL)] += TElapsed.LowPart;       \
    if (FlagOn( FusePerformanceTimerLevel, (LEVEL))) {         \
        DbgPrint("Time of %s %ld\n", (s), TElapsed.LowPart ); \
    }                                                         \
}

//
//  I need this because C can't support conditional compilation within
//  a macro.
//

extern PVOID FuseNull;

#else

#define DebugTrace(INDENT,LEVEL,X,Y)     {NOTHING;}
#define DebugDump(STR,LEVEL,PTR)         {NOTHING;}
#define DebugUnwind(X)                   {NOTHING;}
#define DebugDoit(X)                     {NOTHING;}

#define TimerStart(LEVEL)
#define TimerStop(LEVEL,s)

#define FuseNull NULL

#endif // FASTFUSEDBG

//
//  The following macro is for all people who compile with the DBG switch
//  set, not just fuse dbg users
//

#if DBG

#define DbgDoit(X)                       {X;}

#else

#define DbgDoit(X)                       {NOTHING;}

#endif // DBG

#if DBG

extern NTSTATUS FuseAssertNotStatus;
extern BOOLEAN FuseTestRaisedStatus;

#endif

#endif // _FUSEDATA_


