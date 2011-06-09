/*++

Copyright 2011 FUSE-NT authors

Module Name:

    FuseProcs.h

Abstract:

    This module defines all of the globally used procedures in FUSE


--*/

#ifndef _FUSEPROCS_
#define _FUSEPROCS_

#include <ntifs.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddstor.h>

#include "hashmap.h"
#include "fusent_proto.h"

#ifndef INLINE
#define INLINE __inline
#endif

extern hashmap ModuleMap;
extern hashmap UserspaceMap;

//
//  Uncomment FUSE_DEBUG0 to get detailed driver structure interaction output,
//  and FUSE_DEBUG1 to get detailed information on which methods are being called
//

//#define FUSE_DEBUG0
//#define FUSE_DEBUG1

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

// implemented in FuseIo.c
__drv_dispatchType(IRP_MJ_CLEANUP)
DRIVER_DISPATCH FuseFsdCleanup;

__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH FuseFsdClose;

__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH FuseFsdCreate;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH FuseFsdDeviceControl;

__drv_dispatchType(IRP_MJ_DIRECTORY_CONTROL)
DRIVER_DISPATCH FuseFsdDirectoryControl;

__drv_dispatchType(IRP_MJ_SET_EA)
DRIVER_DISPATCH FuseFsdSetEa;

__drv_dispatchType(IRP_MJ_QUERY_INFORMATION)
__drv_dispatchType(IRP_MJ_QUERY_EA)
DRIVER_DISPATCH FuseFsdQueryInformation;

__drv_dispatchType(IRP_MJ_SET_INFORMATION)
DRIVER_DISPATCH FuseFsdSetInformation;

__drv_dispatchType(IRP_MJ_WRITE)
DRIVER_DISPATCH FuseFsdWrite;

__drv_dispatchType(IRP_MJ_READ)
DRIVER_DISPATCH FuseFsdRead;

__drv_dispatchType(IRP_MJ_FLUSH_BUFFERS)
DRIVER_DISPATCH FuseFsdFlushBuffers;

__drv_dispatchType(IRP_MJ_QUERY_VOLUME_INFORMATION)
DRIVER_DISPATCH FuseFsdQueryVolumeInformation;

__drv_dispatchType(IRP_MJ_SET_VOLUME_INFORMATION)
DRIVER_DISPATCH FuseFsdSetVolumeInformation;

__drv_aliasesMem
__drv_dispatchType(IRP_MJ_FILE_SYSTEM_CONTROL)
DRIVER_DISPATCH FuseFsdFileSystemControl;

__drv_dispatchType(IRP_MJ_LOCK_CONTROL)
DRIVER_DISPATCH FuseFsdLockControl;

__drv_dispatchType(IRP_MJ_SHUTDOWN)
DRIVER_DISPATCH FuseFsdShutdown;

__drv_dispatchType(IRP_MJ_PNP)
DRIVER_DISPATCH FuseFsdPnp;

//
//  Here are the callbacks used by the I/O system for checking for fast I/O or
//  doing a fast query info call, or doing fast lock calls.
//

FAST_IO_CHECK_IF_POSSIBLE FuseFastIoCheckIfPossible;

FAST_IO_QUERY_BASIC_INFO FuseFastQueryBasicInfo;

FAST_IO_QUERY_STANDARD_INFO FuseFastQueryStdInfo;

FAST_IO_QUERY_NETWORK_OPEN_INFO FuseFastQueryNetworkOpenInfo;

FAST_IO_LOCK FuseFastLock;

FAST_IO_UNLOCK_SINGLE FuseFastUnlockSingle;

FAST_IO_UNLOCK_ALL FuseFastUnlockAll;

FAST_IO_UNLOCK_ALL_BY_KEY FuseFastUnlockAllByKey;

FAST_IO_ACQUIRE_FOR_CCFLUSH FuseAcquireForCcFlush;

FAST_IO_RELEASE_FOR_CCFLUSH FuseReleaseForCcFlush;

//
//  Utility functions
//

NTSTATUS
FuseCopyDirectoryControl (
    IN OUT PIRP Irp,
    IN PFILE_DIRECTORY_INFORMATION ModuleDirInformation,
    IN ULONG ModuleDirInformationLength
    );

NTSTATUS
FuseCopyInformation (
    IN OUT PIRP Irp,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength
    );

NTSTATUS
FuseCopyVolumeInformation (
    IN OUT PIRP Irp
    );

NTSTATUS
FuseQueryBasicInfo (
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryStandardInfo (
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryNameInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryFsVolumeInfo (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryFsSizeInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_SIZE_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryFsDeviceInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_DEVICE_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryFsAttributeInfo (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryFsFullSizeInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_FULL_SIZE_INFORMATION Buffer,
    IN OUT PLONG Length
    );

#endif // _FUSEPROCS_


