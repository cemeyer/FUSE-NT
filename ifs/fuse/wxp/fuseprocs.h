/*++

Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    FuseProcs.h

Abstract:

    This module defines all of the globally used procedures in the Fuse
    file system.


--*/

#ifndef _FUSEPROCS_
#define _FUSEPROCS_

#include <ntifs.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddstor.h>

#include "nodetype.h"
#include "FuseStruc.h"
#include "FuseData.h"

#ifndef INLINE
#define INLINE __inline
#endif

// taken from fatprocs.h
typedef enum _FUSE_FLUSH_TYPE {
    
    NoFlush = 0,
    Flush,
    FlushAndInvalidate,
    FlushWithoutPurge

} FUSE_FLUSH_TYPE;

typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile

} TYPE_OF_OPEN;

//
//  The FSD Level dispatch routines.   These routines are called by the
//  I/O system via the dispatch table in the Driver Object.
//
//  They each accept as input a pointer to a device object (actually most
//  expect a volume device object, with the exception of the file system
//  control function which can also take a file system device object), and
//  a pointer to the IRP.  They either perform the function at the FSD level
//  or post the request to the FSP work queue for FSP level processing.
//

NTSTATUS
FuseFsdCleanup (                         //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdClose (                           //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdCreate (                          //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdDeviceControl (                   //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdDirectoryControl (                //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdQueryEa (                         //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdSetEa (                           //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdQueryInformation (                //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdSetInformation (                  //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdFlushBuffers (                    //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdFileSystemControl (               //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdLockControl (                     //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdPnp (                            //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdRead (                            //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdShutdown (                        //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdQueryVolumeInformation (          //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdSetVolumeInformation (            //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseFsdWrite (                           //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseMount (                              //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FuseRequest (                            //  implemented in FuseIo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

//
//  The following macro is used to determine if an FSD thread can block
//  for I/O or wait for a resource.  It returns TRUE if the thread can
//  block and FALSE otherwise.  This attribute can then be used to call
//  the FSD & FSP common work routine with the proper wait value.
//

#define CanFsdWait(IRP) IoIsOperationSynchronous(Irp)


//
//  The FSP level dispatch/main routine.  This is the routine that takes
//  IRP's off of the work queue and calls the appropriate FSP level
//  work routine.
//

VOID
FuseFspDispatch (                        //  implemented in FuseIo.c
    IN PVOID Context
    );

//
//  The following routines are the FSP work routines that are called
//  by the preceding FuseFspDispath routine.  Each takes as input a pointer
//  to the IRP, perform the function, and return a pointer to the volume
//  device object that they just finished servicing (if any).  The return
//  pointer is then used by the main Fsp dispatch routine to check for
//  additional IRPs in the volume's overflow queue.
//
//  Each of the following routines is also responsible for completing the IRP.
//  We moved this responsibility from the main loop to the individual routines
//  to allow them the ability to complete the IRP and continue post processing
//  actions.
//

NTSTATUS
FuseCommonCleanup (                      //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonClose (                        //  implemented in FuseIo.c
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN Wait,
    OUT PBOOLEAN VcbDeleted OPTIONAL
    );

VOID
FuseFspClose (                           //  implemented in FuseIo.c
    IN PVCB Vcb OPTIONAL
    );

NTSTATUS
FuseCommonCreate (                       //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonDirectoryControl (             //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonDeviceControl (                //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonQueryEa (                      //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonSetEa (                        //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonQueryInformation (             //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonSetInformation (               //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonFlushBuffers (                 //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonFileSystemControl (            //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonLockControl (                  //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonPnp (                          //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonRead (                         //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonShutdown (                     //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

	
NTSTATUS
FuseCommonQueryVolumeInfo (              //  implemented in FuseIo.c
    // IN PIRP_CONTEXT IrpContext, -- see implementation.
    IN PIRP Irp
    );

NTSTATUS
FuseQueryFsVolumeInfo (
    IN PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PULONG Length
    );

	
NTSTATUS
FuseCommonSetVolumeInfo (                //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FuseCommonWrite (                        //  implemented in FuseIo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

//
//  The following is implemented in FuseIo.c, and does what is says.
//

NTSTATUS
FuseFlushFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN FUSE_FLUSH_TYPE FlushType
    );

NTSTATUS
FuseFlushDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDCB Dcb,
    IN FUSE_FLUSH_TYPE FlushType
    );

NTSTATUS
FuseFlushFuse (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
FuseFlushVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN FUSE_FLUSH_TYPE FlushType
    );

NTSTATUS
FuseHijackIrpAndFlushDevice (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PDEVICE_OBJECT TargetDeviceObject
    );

VOID
FuseFlushFuseEntries (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG Cluster,
    IN ULONG Count
);

VOID
FuseFlushDirentForFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
);



//
//  The following procedure is used by the FSP and FSD routines to complete
//  an IRP.
//
//  Note that this macro allows either the Irp or the IrpContext to be
//  null, however the only legal order to do this in is:
//
//      FuseCompleteRequest( NULL, Irp, Status );  // completes Irp & preserves context
//      ...
//      FuseCompleteRequest( IrpContext, NULL, DontCare ); // deallocates context
//
//  This would typically be done in order to pass a "naked" IrpContext off to
//  the Fsp for post processing, such as read ahead.
//

VOID
FuseCompleteRequest_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN NTSTATUS Status
    );

#define FuseCompleteRequest(IRPCONTEXT,IRP,STATUS) { \
    FuseCompleteRequest_Real(IRPCONTEXT,IRP,STATUS); \
}

BOOLEAN
FuseIsIrpTopLevel (
    IN PIRP Irp
    );

//
//  The Following routine makes a popup
//

VOID
FusePopUpFileCorrupt (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

//
//  Here are the callbacks used by the I/O system for checking for fast I/O or
//  doing a fast query info call, or doing fast lock calls.
//

BOOLEAN
FuseFastIoCheckIfPossible (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastQueryBasicInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastQueryStdInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastQueryNetworkOpenInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastLock (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    BOOLEAN FailImmediately,
    BOOLEAN ExclusiveLock,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastUnlockSingle (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastUnlockAll (
    IN PFILE_OBJECT FileObject,
    PEPROCESS ProcessId,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FuseFastUnlockAllByKey (
    IN PFILE_OBJECT FileObject,
    PVOID ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
FuseAcquireForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
FuseReleaseForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

//
//  The following macro is used to determine is a file has been deleted.
//
//      BOOLEAN
//      IsFileDeleted (
//          IN PIRP_CONTEXT IrpContext,
//          IN PFCB Fcb
//          );
//

#define IsFileDeleted(IRPCONTEXT,FCB)                      \
    (FlagOn((FCB)->FcbState, FCB_STATE_DELETE_ON_CLOSE) && \
     ((FCB)->UncleanCount == 0))

//
//  The following macro is used by the dispatch routines to determine if
//  an operation is to be done with or without Write Through.
//
//      BOOLEAN
//      IsFileWriteThrough (
//          IN PFILE_OBJECT FileObject,
//          IN PVCB Vcb
//          );
//

#define IsFileWriteThrough(FO,VCB) (             \
    BooleanFlagOn((FO)->Flags, FO_WRITE_THROUGH) \
)

//
//  The following macro is used to set the is fast i/o possible field in
//  the common part of the nonpaged fcb
//
//
//      BOOLEAN
//      FuseIsFastIoPossible (
//          IN PFCB Fcb
//          );
//

#define FuseIsFastIoPossible(FCB) ((BOOLEAN)                                                            \
    (((FCB)->FcbCondition != FcbGood || !FsRtlOplockIsFastIoPossible( &(FCB)->Specific.Fcb.Oplock )) ? \
        FastIoIsNotPossible                                                                            \
    :                                                                                                  \
        (!FsRtlAreThereCurrentFileLocks( &(FCB)->Specific.Fcb.FileLock ) &&                            \
         ((FCB)->NonPaged->OutstandingAsyncWrites == 0) &&                                               \
         !FlagOn( (FCB)->Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED ) ?                             \
            FastIoIsPossible                                                                           \
        :                                                                                              \
            FastIoIsQuestionable                                                                       \
        )                                                                                              \
    )                                                                                                  \
)

//
//  The following macro is used to detemine if the file object is opened
//  for read only access (i.e., it is not also opened for write access or
//  delete access).
//
//      BOOLEAN
//      IsFileObjectReadOnly (
//          IN PFILE_OBJECT FileObject
//          );
//

#define IsFileObjectReadOnly(FO) (!((FO)->WriteAccess | (FO)->DeleteAccess))


//
//  The following two macro are used by the Fsd/Fsp exception handlers to
//  process an exception.  The first macro is the exception filter used in the
//  Fsd/Fsp to decide if an exception should be handled at this level.
//  The second macro decides if the exception is to be finished off by
//  completing the IRP, and cleaning up the Irp Context, or if we should
//  bugcheck.  Exception values such as STATUS_FILE_INVALID (raised by
//  VerfySup.c) cause us to complete the Irp and cleanup, while exceptions
//  such as accvio cause us to bugcheck.
//
//  The basic structure for fsd/fsp exception handling is as follows:
//
//  FuseFsdXxx(...)
//  {
//      try {
//
//          ...
//
//      } except(FuseExceptionFilter( IrpContext, GetExceptionCode() )) {
//
//          Status = FuseProcessException( IrpContext, Irp, GetExceptionCode() );
//      }
//
//      Return Status;
//  }
//
//  To explicitly raise an exception that we expect, such as
//  STATUS_FILE_INVALID, use the below macro FuseRaiseStatus().  To raise a
//  status from an unknown origin (such as CcFlushCache()), use the macro
//  FuseNormalizeAndRaiseStatus.  This will raise the status if it is expected,
//  or raise STATUS_UNEXPECTED_IO_ERROR if it is not.
//
//  If we are vicariously handling exceptions without using FuseProcessException(),
//  if there is the possibility that we raised that exception, one *must*
//  reset the IrpContext so a subsequent raise in the course of handling this
//  request that is *not* explicit, i.e. like a pagein error, does not get
//  spoofed into believing that the first raise status is the reason the second
//  occured.  This could have really nasty consequences.
//
//  It is an excellent idea to always FuseResetExceptionState in these cases.
//
//  Note that when using these two macros, the original status is placed in
//  IrpContext->ExceptionStatus, signaling FuseExceptionFilter and
//  FuseProcessException that the status we actually raise is by definition
//  expected.
//

ULONG
FuseExceptionFilter (
    IN PIRP_CONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

#if DBG
ULONG
FuseBugCheckExceptionFilter (
    IN PEXCEPTION_POINTERS ExceptionPointer
    );
#endif

NTSTATUS
FuseProcessException (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN NTSTATUS ExceptionCode
    );

//
//  VOID
//  FuseRaiseStatus (
//      IN PRIP_CONTEXT IrpContext,
//      IN NT_STATUS Status
//  );
//
//

#if DBG
#define DebugBreakOnStatus(S) {                                                      \
    if (FuseTestRaisedStatus) {                                                       \
        if ((S) == STATUS_DISK_CORRUPT_ERROR || (S) == STATUS_FILE_CORRUPT_ERROR) {  \
            DbgPrint( "Fuse: Breaking on interesting raised status (%08x)\n", (S) );  \
            DbgPrint( "Fuse: Set FuseTestRaisedStatus @ %08x to 0 to disable\n",       \
                      &FuseTestRaisedStatus );                                        \
            DbgBreakPoint();                                                         \
        }                                                                            \
    }                                                                                \
}
#else
#define DebugBreakOnStatus(S)
#endif

#define FuseRaiseStatus(IRPCONTEXT,STATUS) {             \
    (IRPCONTEXT)->ExceptionStatus = (STATUS);           \
    DebugBreakOnStatus( (STATUS) )                      \
    ExRaiseStatus( (STATUS) );                          \
}
    
#define FuseResetExceptionState( IRPCONTEXT ) {          \
    (IRPCONTEXT)->ExceptionStatus = STATUS_SUCCESS;     \
}

//
//  VOID
//  FuseNormalAndRaiseStatus (
//      IN PRIP_CONTEXT IrpContext,
//      IN NT_STATUS Status
//  );
//

#define FuseNormalizeAndRaiseStatus(IRPCONTEXT,STATUS) {                         \
    (IRPCONTEXT)->ExceptionStatus = (STATUS);                                   \
    ExRaiseStatus(FsRtlNormalizeNtstatus((STATUS),STATUS_UNEXPECTED_IO_ERROR)); \
}


//
//  The following macros are used to establish the semantics needed
//  to do a return from within a try-finally clause.  As a rule every
//  try clause must end with a label call try_exit.  For example,
//
//      try {
//              :
//              :
//
//      try_exit: NOTHING;
//      } finally {
//
//              :
//              :
//      }
//
//  Every return statement executed inside of a try clause should use the
//  try_return macro.  If the compiler fully supports the try-finally construct
//  then the macro should be
//
//      #define try_return(S)  { return(S); }
//
//  If the compiler does not support the try-finally construct then the macro
//  should be
//
//      #define try_return(S)  { S; goto try_exit; }
//

#define try_return(S) { S; goto try_exit; }
#define try_leave(S) { S; leave; }

//
//  These routines define the FileId for Fuse.  Lacking a fixed/uniquifiable
//  notion, we simply come up with one which is unique in a given snapshot
//  of the volume.  As long as the parent directory is not moved or compacted,
//  it may even be permanent.
//

//
//  The internal information used to identify the fcb/dcb on the
//  volume is the byte offset of the dirent of the file on disc.
//  Our root always has fileid 0.  Fuse32 roots are chains and can
//  use the LBO of the cluster, 12/16 roots use the lbo in the Vcb.
//

#define FuseGenerateFileIdFromDirentOffset(ParentDcb,DirentOffset)                                   \
    ((ParentDcb) ? ((NodeType(ParentDcb) != Fuse_NTC_ROOT_DCB || FuseIsFuse32((ParentDcb)->Vcb)) ?   \
                  FuseGetLboFromIndex( (ParentDcb)->Vcb,                                             \
                                      (ParentDcb)->FirstClusterOfFile ) :                            \
                  (ParentDcb)->Vcb->AllocationSupport.RootDirectoryLbo) +                            \
                 (DirentOffset)                                                                      \
                  :                                                                                  \
                 0)

//
//

#define FuseGenerateFileIdFromFcb(Fcb)                                                               \
        FuseGenerateFileIdFromDirentOffset( (Fcb)->ParentDcb, (Fcb)->DirentOffsetWithinDirectory )

//
//  Wrap to handle the ./.. cases appropriately.  Note that we commute NULL parent to 0. This would
//  only occur in an illegal root ".." entry.
//

#define FuseDOT    ((ULONG)0x2020202E)
#define FuseDOTDOT ((ULONG)0x20202E2E)

#define FuseGenerateFileIdFromDirentAndOffset(Dcb,Dirent,DirentOffset)                               \
    ((*((PULONG)(Dirent)->FileName)) == FuseDOT ? FuseGenerateFileIdFromFcb(Dcb) :                    \
     ((*((PULONG)(Dirent)->FileName)) == FuseDOTDOT ? ((Dcb)->ParentDcb ?                            \
                                                       FuseGenerateFileIdFromFcb((Dcb)->ParentDcb) : \
                                                       0) :                                         \
      FuseGenerateFileIdFromDirentOffset(Dcb,DirentOffset)))


//
//  BOOLEAN
//  FuseDeviceIsFuseFsdo(
//      IN PDEVICE_OBJECT D
//      );
//
//  Evaluates to TRUE if the supplied device object is one of the file system devices
//  we created at initialisation.
//

#define FuseDeviceIsFuseFsdo( D)  (((D) == FuseData.DiskFileSystemDeviceObject) || ((D) == FuseData.CdromFileSystemDeviceObject))

#endif // _FUSEPROCS_


