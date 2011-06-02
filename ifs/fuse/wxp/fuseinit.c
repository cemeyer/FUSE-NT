/*++

Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    FuseInit.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for FUSE

--*/

#include <ntifs.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddstor.h>

#include "fuseprocs.h"


PDEVICE_OBJECT FuseFileSystemDeviceObject;

FAST_IO_DISPATCH FuseFastIoDispatch;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

DRIVER_UNLOAD FuseUnload;


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the FUSE file system
    device driver.  This routine creates the device object for the FileSystem
    device and performs all other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    USHORT MaxDepth;
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;

    UNICODE_STRING ValueName;
    ULONG Value;

    //
    // Create the device object for disks.  To avoid problems with filters who
    // know this name, we must keep it.
    //

    RtlInitUnicodeString( &UnicodeString, L"\\Device\\Fuse" );
    Status = IoCreateDevice( DriverObject,
                             0,
                             &UnicodeString,
                             FILE_DEVICE_DISK_FILE_SYSTEM,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &FuseFileSystemDeviceObject );

    if (!NT_SUCCESS( Status )) {
        return Status;
    }

    DriverObject->DriverUnload = FuseUnload;

#ifdef _PNP_POWER_
    //
    // This driver doesn't talk directly to a device, and (at the moment)
    // isn't otherwise concerned about power management.
    //

    FuseFileSystemDeviceObject->DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif

    //
    //  Note that because of the way data caching is done, we set neither
    //  the Direct I/O or Buffered I/O bit in DeviceObject->Flags.  If
    //  data is not in the cache, or the request is not buffered, we may,
    //  set up for Direct I/O by hand.
    //

    //
    // Initialize the driver object with this driver's entry points.
    //

    // do we need a separate function for each of these or can we have one
    // that just passes up the calls directly?

    DriverObject->MajorFunction[IRP_MJ_CREATE]                   = FuseFsdCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                    = FuseFsdClose;
    DriverObject->MajorFunction[IRP_MJ_READ]                     = FuseFsdRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                    = FuseFsdWrite;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]        = FuseFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]          = FuseFsdSetInformation;
    DriverObject->MajorFunction[IRP_MJ_QUERY_EA]                 = FuseFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_EA]                   = FuseFsdSetEa;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]            = FuseFsdFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FuseFsdQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]   = FuseFsdSetVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                  = FuseFsdCleanup;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]        = FuseFsdDirectoryControl;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]      = FuseFsdFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL]             = FuseFsdLockControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = FuseFsdDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]                 = FuseFsdShutdown;
    DriverObject->MajorFunction[IRP_MJ_PNP]                      = FuseFsdPnp;

    DriverObject->FastIoDispatch = &FuseFastIoDispatch;

    RtlZeroMemory(&FuseFastIoDispatch, sizeof(FuseFastIoDispatch));

    FuseFastIoDispatch.SizeOfFastIoDispatch =       sizeof(FAST_IO_DISPATCH);
    FuseFastIoDispatch.FastIoCheckIfPossible =      FuseFastIoCheckIfPossible;  //  CheckForFastIo
    FuseFastIoDispatch.FastIoRead =                 FsRtlCopyRead;             //  Read
    FuseFastIoDispatch.FastIoWrite =                FsRtlCopyWrite;            //  Write
    FuseFastIoDispatch.FastIoQueryBasicInfo =       FuseFastQueryBasicInfo;     //  QueryBasicInfo
    FuseFastIoDispatch.FastIoQueryStandardInfo =    FuseFastQueryStdInfo;       //  QueryStandardInfo
    FuseFastIoDispatch.FastIoLock =                 FuseFastLock;               //  Lock
    FuseFastIoDispatch.FastIoUnlockSingle =         FuseFastUnlockSingle;       //  UnlockSingle
    FuseFastIoDispatch.FastIoUnlockAll =            FuseFastUnlockAll;          //  UnlockAll
    FuseFastIoDispatch.FastIoUnlockAllByKey =       FuseFastUnlockAllByKey;     //  UnlockAllByKey
    FuseFastIoDispatch.FastIoQueryNetworkOpenInfo = FuseFastQueryNetworkOpenInfo;
    FuseFastIoDispatch.AcquireForCcFlush =          FuseAcquireForCcFlush;
    FuseFastIoDispatch.ReleaseForCcFlush =          FuseReleaseForCcFlush;

    //
    //  Register the file system with the I/O system
    //

    IoRegisterFileSystem(FuseFileSystemDeviceObject);
    ObReferenceObject (FuseFileSystemDeviceObject);

    //
    //  Set up FUSE module map and userspace map. The module map
    //  associates module names with module structs, which store
    //  IRPs representing requests for work from the module and
    //  from userspace applications as well as a mutex
    //

    mk_hmap(&ModuleMap, str_hash_fn, str_eq_fn, module_struct_delete);

    //
    //  And return to our caller
    //

    return( STATUS_SUCCESS );
}


VOID
FuseUnload (
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This is the unload routine for the filesystem

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None

--*/

{
    ObDereferenceObject(FuseFileSystemDeviceObject);
    free_hmap(&ModuleMap);
}