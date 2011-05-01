/*++

Copyright (c) 2011 FoW Group

Module Name:

    FuseIo.c

Abstract:

    This module implements the main routines for the FUSE filesystem driver

--*/

#include <ntdef.h>
#include "fuseprocs.h"
#include "ntproto.h"

NTSTATUS
FuseFsdFileSystemControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    WCHAR* InMessage, *OutMessage;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    InMessage = (WCHAR *) IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    OutMessage = (WCHAR *) Irp->UserBuffer;

    // outside of testing, this should be wrapped in a try-except with ProbeForRead/ProbeForWrite
    // this isn't printing the right thing currently...
    DbgPrint("Test program sent message: \"%S\"\n", InMessage);

    wcscpy(OutMessage, L"The FUSE driver says 'hello'");

    DbgPrint("FuseFsdFileSystemControl\n");

	return STATUS_SUCCESS;
}


NTSTATUS
FuseFsdCleanup (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdCleanup\n");

	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdClose (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdClose\n");
	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdCreate (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdCreate\n");
	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdRead (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdRead\n");
	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdWrite (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdWrite\n");
	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdDeviceControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdDeviceControl\n");

	return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdDirectoryControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdQueryEa (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdSetEa (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdQueryInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdSetInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdFlushBuffers (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdLockControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdPnp (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdShutdown (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdQueryVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseFsdSetVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonCleanup (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonClose (
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN Wait,
    OUT PBOOLEAN VcbDeleted OPTIONAL
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

VOID
FuseFspClose (   
    IN PVCB Vcb OPTIONAL
    )
{

}

NTSTATUS
FuseCommonCreate (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonDirectoryControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonDeviceControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonQueryEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonSetEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonQueryInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonSetInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonFlushBuffers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonLockControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonPnp (  
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonRead ( 
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonShutdown (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonQueryVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonSetVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseCommonWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

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
    )
{
	return TRUE;
}

BOOLEAN
FuseFastQueryBasicInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

BOOLEAN
FuseFastQueryStdInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

BOOLEAN
FuseFastQueryNetworkOpenInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

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
    )
{
	return TRUE;
}

BOOLEAN
FuseFastUnlockSingle (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

BOOLEAN
FuseFastUnlockAll (
    IN PFILE_OBJECT FileObject,
    PEPROCESS ProcessId,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

BOOLEAN
FuseFastUnlockAllByKey (
    IN PFILE_OBJECT FileObject,
    PVOID ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return TRUE;
}

NTSTATUS
FuseAcquireForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

NTSTATUS
FuseReleaseForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
	return STATUS_CLEANER_CARTRIDGE_INSTALLED;
}

