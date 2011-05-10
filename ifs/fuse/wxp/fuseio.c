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
    DbgPrint("FuseFsdDirectoryControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdQueryEa (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdQueryEa\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdSetEa (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdSetEa\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdQueryInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdQueryInformation\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdSetInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdSetInformation\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdFlushBuffers (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdFlushBuffers\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdLockControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdLockControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdPnp (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdPnp\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdShutdown (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdShutdown\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseFsdQueryVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdQueryVolumeInformation\n");
    return FuseCommonQueryVolumeInfo(Irp);
}

NTSTATUS
FuseFsdSetVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    DbgPrint("FuseFsdSetVolumeInformation\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonCleanup (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonCleanup\n");
    return STATUS_SUCCESS;
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
    DbgPrint("FuseCommonClose\n");
    return STATUS_SUCCESS;
}

VOID
FuseFspClose (   
    IN PVCB Vcb OPTIONAL
    )
{
    DbgPrint("FuseFspClose\n");
}

NTSTATUS
FuseCommonCreate (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonCreate\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonDirectoryControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonDirectoryControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonDeviceControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonDeviceControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonQueryEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonQueryEa\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonSetEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonSetEa\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonQueryInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonQueryInformation\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonSetInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonSetInformation\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonFlushBuffers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonFlushBuffers\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonFileSystemControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonLockControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonLockControl\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonPnp (  
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonPnp\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonRead ( 
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonRead\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonShutdown (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonShutdown\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonQueryVolumeInfo (
    // IN PIRP_CONTEXT IrpContext, -- Not sure where IRP_CONTEXTs come from yet. Not needed for current implementation anyways. Remove note in header file.
    IN PIRP Irp
    )
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    ULONG Length;
    FS_INFORMATION_CLASS FsInformationClass;
    PVOID Buffer;

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
	
    //
    //  Reference our input parameters to make things easier
    //

    Length = IrpSp->Parameters.QueryVolume.Length;
    FsInformationClass = IrpSp->Parameters.QueryVolume.FsInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;
	
	switch (FsInformationClass) {

        case FileFsVolumeInformation:
		
			Status = FuseQueryFsVolumeInfo(Buffer, &Length);
            break;

        case FileFsSizeInformation:

            Status = STATUS_SUCCESS;
            break;

        case FileFsDeviceInformation:

            Status = STATUS_SUCCESS;
            break;

        case FileFsAttributeInformation:

            Status = STATUS_SUCCESS;
            break;

        case FileFsFullSizeInformation:

            Status = STATUS_SUCCESS;
            break;

        default:

            Status = STATUS_SUCCESS;
            break;
	}
	
    return Status;
}

NTSTATUS
FuseQueryFsVolumeInfo (
    IN PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query volume info call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    ULONG BytesToCopy;

    NTSTATUS Status;

	PWCHAR HelloWorldLabel = L"HELLO_WORLD";
	
    //
    //  Zero out the buffer, then extract and fill up the non zero fields.
    //

    RtlZeroMemory( Buffer, sizeof(FILE_FS_VOLUME_INFORMATION) );

    Buffer->VolumeSerialNumber = 1337; // chosen at random

    Buffer->SupportsObjects = FALSE;

    *Length -= FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel[0]);

    //
    //  Check if the buffer we're given is long enough
    //

    if ( *Length >= (ULONG)sizeof(HelloWorldLabel) ) {

        BytesToCopy = sizeof(HelloWorldLabel);

        Status = STATUS_SUCCESS;

    } else {

        BytesToCopy = *Length;

        Status = STATUS_BUFFER_OVERFLOW;
    }

    //
    //  Copy over what we can of the volume label, and adjust *Length
    //

    Buffer->VolumeLabelLength = sizeof(HelloWorldLabel);

    RtlCopyMemory( &Buffer->VolumeLabel[0],
                   HelloWorldLabel,
                   BytesToCopy );

    *Length -= BytesToCopy;
	
    return Status;
}

NTSTATUS
FuseCommonSetVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonSetVolumeInfo\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseCommonWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
    DbgPrint("FuseCommonWrite\n");
    return STATUS_SUCCESS;
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
    DbgPrint("FuseFastIoCheckIfPossible\n");
    return FALSE;
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
    DbgPrint("FuseFastQueryBasicInfo\n");
    return FALSE;
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
    DbgPrint("FuseFastQueryStdInfo\n");
    return FALSE;
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
    DbgPrint("FuseFastQueryNetworkOpenInfo\n");
    return FALSE;
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
    DbgPrint("FuseFastLock\n");
    return FALSE;
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
    DbgPrint("FuseFastUnlockSingle\n");
    return FALSE;
}

BOOLEAN
FuseFastUnlockAll (
    IN PFILE_OBJECT FileObject,
    PEPROCESS ProcessId,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    DbgPrint("FuseFastUnlockAll\n");
    return FALSE;
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
    DbgPrint("FuseFastUnlockAllByKey\n");
    return FALSE;
}

NTSTATUS
FuseAcquireForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    DbgPrint("FuseAcquireForCcFlush\n");
    return STATUS_SUCCESS;
}

NTSTATUS
FuseReleaseForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    DbgPrint("FuseReleaseForCcFlush\n");
    return STATUS_SUCCESS;
}

