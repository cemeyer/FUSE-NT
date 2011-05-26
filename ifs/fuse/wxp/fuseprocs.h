/*++

Copyright (c) 1989-2000 Microsoft Corporation

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

#include "FuseStruc.h"
#include "hashmap.h"

#ifndef INLINE
#define INLINE __inline
#endif

extern hashmap ModuleMap;
extern hashmap UserspaceMap;

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
//  Utility functions
//

NTSTATUS
FuseCopyDirectoryControl (
    IN PIRP Irp
    );

NTSTATUS
FuseCopyInformation (
    IN PIRP Irp
    );

NTSTATUS
FuseCopyVolumeInformation (
    IN PIRP Irp
    );

NTSTATUS
FuseQueryBasicInfo (
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryStandardInfo (
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    IN OUT PLONG Length
    );

NTSTATUS
FuseQueryNameInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_NAME_INFORMATION Buffer,
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

//
//  Module name extraction
//

LPWSTR
FuseExtractModuleName (
    IN PIRP Irp,
    OUT PULONG Length
    );

LPWSTR
FuseAllocateModuleName (
    IN PIRP Irp
    );

VOID
FuseCopyModuleName (
    IN PIRP Irp,
    OUT WCHAR* Destination,
    OUT PULONG Length
    );

#endif // _FUSEPROCS_


