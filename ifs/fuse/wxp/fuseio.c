/*++

Copyright (c) 2011 FoW Group

Module Name:

    FuseIo.c

Abstract:

    This module implements the main routines for the FUSE filesystem driver

--*/

#include <ntdef.h>
#include <NtStatus.h>
#include <Ntstrsafe.h>
#include "fuseprocs.h"
#include "fusent_proto.h"
#include "hashmap.h"
#include "fusestruc.h"

NTSTATUS
FuseAddUserspaceIrp (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

BOOLEAN
FuseAddIrpToModuleList (
    IN PIRP Irp,
    IN PMODULE_STRUCT ModuleStruct,
    IN BOOLEAN AddToModuleList
    );

VOID
FuseCheckForWork (
    IN PMODULE_STRUCT ModuleStruct
    );

NTSTATUS
FuseCopyResponse (
    IN PMODULE_STRUCT ModuleStruct,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

BOOLEAN
FuseCheckUnmountModule (
    IN PIO_STACK_LOCATION IrpSp
    );

//
//  This hash map implementation uses linear probing, which
//  is not very fast for densely populated maps, but we set
//  the initial size of the map to be large enough so that
//  the type of probing is somewhat irrelevant
//
hashmap ModuleMap;

NTSTATUS
FuseAddUserspaceIrp (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

{
    NTSTATUS Status;
    WCHAR* FileName = IrpSp->FileObject->FileName.Buffer + 1;
	PUNICODE_STRING FileNameString = &IrpSp->FileObject->FileName;
    WCHAR* ModuleName;
    WCHAR* BackslashPosition = NULL;
    PMODULE_STRUCT ModuleStruct;

    DbgPrint("Adding userspace IRP to queue for file %S\n", FileName);

    //
    //  Check if a module was opened, as opposed to just \Device\Fuse. There
    //  is nothing to be done if the file is not on a module
    //

    if (FileNameString->Length > 0) {

        //
        //  First, check if there is an entry in the hash map for the module.
        //  Shift FileName by one WCHAR so that we don't read the initial \
        //

	    BackslashPosition = wcsstr(FileName, L"\\");
		
		if(!BackslashPosition) {
			ModuleName = FileName;
		} else {
			ULONG ModuleNameLength = (ULONG)(BackslashPosition - FileName);
			ModuleName = (WCHAR*) ExAllocatePool(PagedPool, sizeof(WCHAR) * (ModuleNameLength + 1));
			memcpy(ModuleName, FileName, sizeof(WCHAR) * (ModuleNameLength));
			ModuleName[ModuleNameLength] = '\0';
		}

        DbgPrint("Parsed module name for userspace request is %S\n", ModuleName);

        ModuleStruct = (PMODULE_STRUCT) hmap_get(&ModuleMap, ModuleName);
        if(!ModuleStruct) {
            DbgPrint("No entry in map found for module %S. Completing request\n", ModuleName);

        
        } else {
            DbgPrint("Found entry in map for module %S. Adding userspace IRP to module queue\n", ModuleName);

            FuseAddIrpToModuleList(Irp, ModuleStruct, FALSE);

            //
            //  Mark the request as status pending. The module thread that calls
            //  NtFsControlFile with a response will complete it, or if there is
            //  a request to hand it off to right now, that request will complete it
            //

            IoMarkIrpPending(Irp);
            Status = STATUS_PENDING;

            FuseCheckForWork(ModuleStruct);
        }

        //
        //  If we allocated the module name string in memory, free it
        //

        if(ModuleName != FileName) {
            ExFreePool(ModuleName);
        }
	} else {
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
        Status = STATUS_SUCCESS;
    }

    return Status;
}

BOOLEAN
FuseAddIrpToModuleList (
    IN PIRP Irp,
    IN PMODULE_STRUCT ModuleStruct,
    IN BOOLEAN AddToModuleList
    )
//
//  Adds the given IRP to the module IRP list if the AddToModuleList flag is on, and
//  adds the given IRP to the userspace IRP list otherwise. Validates the user buffer
//  of module IRPs and returns FALSE if the validation fails, TRUE otherwise
//
{
    DbgPrint("Adding IRP to queue for module %S from a %s request\n",
        ModuleStruct->ModuleName,
        (AddToModuleList ? "module" : "userspace"));

    if(AddToModuleList) {
        ULONG ExpectedBufferLength;
        PIO_STACK_LOCATION ModuleIrpSp = IoGetCurrentIrpStackLocation(Irp);
            
        PWSTR ModuleName = ModuleStruct->ModuleName;
        ULONG FileNameLength = wcslen(ModuleName) * (sizeof(WCHAR) + 1);
        ULONG StackLength = Irp->StackCount * sizeof(IO_STACK_LOCATION);
        ExpectedBufferLength = sizeof(FUSENT_REQ) + StackLength + 4 + FileNameLength;

        //
        //  Check that the output buffer is large enough to contain the work request
        //

        if(ModuleIrpSp->Parameters.DeviceIoControl.OutputBufferLength < ExpectedBufferLength) {

            DbgPrint("Module %S supplied a bad request buffer. Required size: %d. Actual size: %d\n",
                ModuleStruct->ModuleName, ExpectedBufferLength, ModuleIrpSp->Parameters.DeviceIoControl.OutputBufferLength);
            
            return FALSE;
        }
    }

    //
    //  Acquire the lock for the module struct
    //

    ExAcquireFastMutex(&ModuleStruct->ModuleLock);

    __try {
        //
        //  Add to the list specified by Module--TRUE -> module list, FALSE -> userspace list
        //

        PIRP_LIST LastEntry = AddToModuleList ? ModuleStruct->ModuleIrpListEnd : ModuleStruct->UserspaceIrpListEnd;
        if(!LastEntry) {

            //
            //  If the first entry in the list is NULL, replace it with a new entry
            //

            if(AddToModuleList) {
                ModuleStruct->ModuleIrpList = (PIRP_LIST) ExAllocatePool(PagedPool, sizeof(IRP_LIST));
                LastEntry = ModuleStruct->ModuleIrpList;
            } else {
                ModuleStruct->UserspaceIrpList = (PIRP_LIST) ExAllocatePool(PagedPool, sizeof(IRP_LIST));
                LastEntry = ModuleStruct->UserspaceIrpList;
            }
        } else {

            //
            //  Insert the new entry at the end of the list
            //

            LastEntry->Next = (PIRP_LIST) ExAllocatePool(PagedPool, sizeof(IRP_LIST));
            LastEntry = LastEntry->Next;
        }

        //
        //  Set the fields of the new entry in the IRP list
        //

        LastEntry->Irp = Irp;
        LastEntry->Next = NULL;

        if(AddToModuleList) {
            ModuleStruct->ModuleIrpListEnd = LastEntry;
        } else {
            ModuleStruct->UserspaceIrpListEnd = LastEntry;
        }
    } __finally {
        ExReleaseFastMutex(&ModuleStruct->ModuleLock);
    }

    DbgPrint("Successfully added %s request IRP to queue\n", (AddToModuleList ? "module" : "userspace"));

    return TRUE;
}

VOID
FuseCheckForWork (
    IN PMODULE_STRUCT ModuleStruct
    )
{
    //
    //  Check if there is both a userspace request and a module
    //  IRP to which to hand it off
    //

    if(ModuleStruct->ModuleIrpList && ModuleStruct->UserspaceIrpList) {

        ExAcquireFastMutex(&ModuleStruct->ModuleLock);

        __try {

            //
            //  Perform the check again now that we have the lock
            //

            while(ModuleStruct->ModuleIrpList && ModuleStruct->UserspaceIrpList) {
                PIRP_LIST ModuleIrpListEntry = ModuleStruct->ModuleIrpList;
                PIRP_LIST UserspaceIrpListEntry = ModuleStruct->UserspaceIrpList;
                PIRP ModuleIrp = ModuleIrpListEntry->Irp;
                PIRP UserspaceIrp = UserspaceIrpListEntry->Irp;
                PIO_STACK_LOCATION UserspaceIrpSp;
                FUSENT_REQ* FuseNtReq;
                PULONG FileNameLengthField;

                PWSTR ModuleName = ModuleStruct->ModuleName;
                ULONG FileNameLength = wcslen(ModuleName) * sizeof(WCHAR);
                ULONG StackLength = UserspaceIrp->StackCount * sizeof(IO_STACK_LOCATION);

                DbgPrint("Work found for module %S. Pairing userspace request with module request for work\n", ModuleName);

                //
                //  Remove the head entry from the module IRP queue
                //

                ModuleStruct->ModuleIrpList = ModuleIrpListEntry->Next;

                //
                //  If we removed the first entry of the queue, update the end
                //  pointer to NULL so that it is not pointing to an invalid entry
                //

                if(!ModuleStruct->ModuleIrpList) {
                    ModuleStruct->ModuleIrpListEnd = NULL;
                }

                //
                //  Remove the head IRP from the userspace IRP queue
                //

                ModuleStruct->UserspaceIrpList = UserspaceIrpListEntry->Next;

                //
                //  If we removed the first entry of the queue, update the end
                //  pointer to NULL so that it is not pointing to an invalid entry
                //

                if(!ModuleStruct->UserspaceIrpList) {
                    ModuleStruct->UserspaceIrpListEnd = NULL;
                }

                //
                //  Fill out the FUSE module request using the userspace IRP
                //

                UserspaceIrpSp = IoGetCurrentIrpStackLocation(UserspaceIrp);

                FuseNtReq = (FUSENT_REQ*) ModuleIrp->AssociatedIrp.SystemBuffer;

                FuseNtReq->pirp = UserspaceIrp;
                FuseNtReq->fop = UserspaceIrpSp->FileObject;
                FuseNtReq->irp = *UserspaceIrp;
                memcpy(FuseNtReq->iostack, UserspaceIrp + 1, StackLength);

                FileNameLengthField = (PULONG) (((PCHAR) FuseNtReq->iostack) + StackLength);
                *FileNameLengthField = FileNameLength;

                memcpy(FileNameLengthField + 1, ModuleName, FileNameLength);
                ModuleIrp->IoStatus.Information = sizeof(FUSENT_REQ) + StackLength + 4 + FileNameLength;

                //
                //  Add the userspace IRP to the list of outstanding IRPs (i.e. the IRPs
                //  for which the module has yet to send a reply)
                //

                if(ModuleStruct->OutstandingIrpListEnd) {
                    ModuleStruct->OutstandingIrpListEnd->Next = UserspaceIrpListEntry;
                    ModuleStruct->OutstandingIrpListEnd = ModuleStruct->OutstandingIrpListEnd->Next;
                } else {
                    ModuleStruct->OutstandingIrpList = UserspaceIrpListEntry;
                    ModuleStruct->OutstandingIrpListEnd = ModuleStruct->OutstandingIrpList;
                }

                //
                //  Free the module IRP entry from memory
                //

                ExFreePool(ModuleIrpListEntry);

                //
                //  Complete the module's IRP to signal that the module should process it
                //

                ModuleIrp->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(ModuleIrp, IO_NO_INCREMENT);
            }
        } __finally {
            ExReleaseFastMutex(&ModuleStruct->ModuleLock);
        }
    }
}

NTSTATUS
FuseCopyResponse (
    IN PMODULE_STRUCT ModuleStruct,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
//
//  Attempt to find an outstanding userspace IRP whose pointer
//  matches that given in the module's response. If one is found
//  (i.e. the module's response is legitimate) then pair up the
//  response with the userspace IRP and complete both IRPs
//
{
    NTSTATUS Status;

    //
    //  First validate the user buffer
    //

    if(IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(FUSENT_RESP)) {

        FUSENT_RESP* FuseNtResp = (FUSENT_RESP*) Irp->AssociatedIrp.SystemBuffer;
        PIRP UserspaceIrp = FuseNtResp->pirp;

        //
        //  Do *not* attempt to complete the userspace IRP until we verify that the pointer
        //  is valid. A list of outstanding userspace IRPs is maintained for this purpose
        //

        ExAcquireFastMutex(&ModuleStruct->ModuleLock);

        __try {
            PIRP_LIST CurrentEntry = ModuleStruct->OutstandingIrpList;
            PIRP_LIST PreviousEntry = NULL;
            if(!CurrentEntry) {

                //
                //  The list is empty so the module's response is invalid
                //

                DbgPrint("Module %S sent a response but there are no outstanding userspace IRPs\n", ModuleStruct->ModuleName);

                Status = STATUS_INVALID_DEVICE_REQUEST;
            } else {
                BOOLEAN MatchFound = FALSE;

                while(CurrentEntry && !MatchFound) {
                    if(CurrentEntry->Irp == UserspaceIrp) {
                        MatchFound = TRUE;
                    } else {
                        PreviousEntry = CurrentEntry;
                        CurrentEntry = CurrentEntry->Next;
                    }
                }

                if(MatchFound) {

                    DbgPrint("Match found for response from module %S. Completing userspace request\n", ModuleStruct->ModuleName);

                    //
                    //  We've found a match. Complete the userspace request
                    //
                    //  TODO: copy buffers
                    UserspaceIrp->IoStatus.Status = -FuseNtResp->status;
                    IoCompleteRequest(UserspaceIrp, IO_NO_INCREMENT);

                    //
                    //  Remove the entry from the outstanding IRP queue
                    //

                    if(PreviousEntry) {
                        PreviousEntry->Next = PreviousEntry->Next->Next;
                    } else {
                        ModuleStruct->OutstandingIrpList = CurrentEntry->Next;
                    }

                    if(!CurrentEntry->Next) {
                        ModuleStruct->OutstandingIrpListEnd = PreviousEntry;
                    }

                    //
                    //  Free the entry from memory
                    //

                    ExFreePool(CurrentEntry);
                } else {

                    //
                    //  No userspace IRP with an address matching that given was found,
                    //  so the module's response is assumed to be invalid
                    //

                    DbgPrint("Module %S sent a response but no match for the userspace IRP %x was found\n", ModuleStruct->ModuleName, UserspaceIrp);

                    Status = STATUS_INVALID_DEVICE_REQUEST;
                }
            }
        } __finally {
            ExReleaseFastMutex(&ModuleStruct->ModuleLock);
        }

    } else {

        DbgPrint("Response from %S has a bad user buffer. Expected size: %d. Actual size: \n",
            ModuleStruct->ModuleName, sizeof(FUSENT_RESP), IrpSp->Parameters.DeviceIoControl.InputBufferLength);

        Status = STATUS_INVALID_USER_BUFFER;
    }

    return Status;
}

BOOLEAN
FuseCheckUnmountModule (
    IN PIO_STACK_LOCATION IrpSp
    )
//
//  Check whether the closure of the file represented by the given file object pointer
//  is an indication that a module should be unmounted. A module should be unmounted
//  when the file handle with which it was initially mounted is closed.
//
//  Returns whether a module was dismounted
//
{
    if(IrpSp->FileObject->FileName.Length > 0) {
        WCHAR* ModuleName = IrpSp->FileObject->FileName.Buffer + 1;
        PMODULE_STRUCT ModuleStruct = (PMODULE_STRUCT) hmap_get(&ModuleMap, ModuleName);

        if(ModuleStruct && ModuleStruct->ModuleFileObject == IrpSp->FileObject) {

            DbgPrint("Dismounting module %S, as the last reference to its handle has been lost\n", ModuleName);

            //
            //  The file object matches, so perform a dismount
            //

            ExAcquireFastMutex(&ModuleStruct->ModuleLock);

            hmap_remove(&ModuleMap, ModuleName);

            //
            //  No need to "release" the lock, as it has been freed
            //

            return TRUE;
        }
    }

    return FALSE;
}

NTSTATUS
FuseFsdFileSystemControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdFileSystemControl\n");

    //
    //  When a module requests a mount, its unique name of the form
    //  [module-name][process ID] is added to the module map and space is
    //  allocated for the module's storage structure
    //
    //  Modules will call NtFsControlFile with IRP_FUSE_MODULE_REQUEST
    //  potentially many times with many different buffers while waiting
    //  for work from userspace applications. When work comes in in the
    //  form of CreateFiles, ReadFiles, WriteFiles, etc., the driver hands
    //  off requests to the module by filling out and then completing IRPs,
    //  which signals to the module that there is work to be done
    //
    //  When a module completes work, it calls NtFsControlFile with
    //  IRP_FUSE_MODULE_RESPONSE, and the IRP for the corresponding
    //  CreateFile, ReadFile, WriteFile, etc. is filled out using the
    //  response from the module and then completed
    //

    if(IrpSp->Parameters.FileSystemControl.FsControlCode == IRP_FUSE_MOUNT) {
        WCHAR* ModuleName = IrpSp->FileObject->FileName.Buffer + 1;
        PMODULE_STRUCT ModuleStruct;

        DbgPrint("A mount has been requested for module %S\n", ModuleName);

        //
        //  Set up a new module struct for storing state and add it to the
        //  module hash map
        //

        ModuleStruct = (PMODULE_STRUCT) ExAllocatePool(PagedPool, sizeof(MODULE_STRUCT));
        RtlZeroMemory(ModuleStruct, sizeof(MODULE_STRUCT));
        ExInitializeFastMutex(&ModuleStruct->ModuleLock);

        //
        //  Store the module's file object so that we can later verify that the
        //  module is what is requesting or providing responses to work and not
        //  some malicious application or rogue group member
        //

        ModuleStruct->ModuleFileObject = IrpSp->FileObject;
        ModuleStruct->ModuleName = (WCHAR*) ExAllocatePool(PagedPool, sizeof(WCHAR) * (wcslen(ModuleName) + 1));
        RtlStringCchCopyW(ModuleStruct->ModuleName, sizeof(WCHAR) * (wcslen(ModuleName) + 1), ModuleName);
        hmap_add(&ModuleMap, ModuleStruct->ModuleName, ModuleStruct);

        Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } else if(IrpSp->Parameters.FileSystemControl.FsControlCode == IRP_FUSE_MODULE_REQUEST ||
        IrpSp->Parameters.FileSystemControl.FsControlCode == IRP_FUSE_MODULE_RESPONSE) {

        WCHAR* ModuleName = IrpSp->FileObject->FileName.Buffer + 1;
        PMODULE_STRUCT ModuleStruct = (PMODULE_STRUCT) hmap_get(&ModuleMap, ModuleName);


        //
        //  Verify that whoever is making the request is using the same file handle that
        //  the module initially used to set up communication
        //

        if(ModuleStruct->ModuleFileObject == IrpSp->FileObject) {

            if(IrpSp->Parameters.FileSystemControl.FsControlCode == IRP_FUSE_MODULE_REQUEST) {

                DbgPrint("Received request for work from module %S\n", ModuleName);

                //
                //  Save IRP in module struct and check if there is a userspace request to give it
                //

                if(!FuseAddIrpToModuleList(Irp, ModuleStruct, TRUE)) {

                    DbgPrint("Module %S supplied bad buffer; bailing\n", ModuleStruct->ModuleName);

                    //
                    //  If the addition of the IRP to the module list fails due to a bad buffer, bail out
                    //

                    Status = STATUS_INVALID_USER_BUFFER;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);
                } else {
                    FuseCheckForWork(ModuleStruct);

                    Status = STATUS_PENDING;
                    IoMarkIrpPending(Irp);
                }
            } else {

                DbgPrint("Received response for work from module %S\n", ModuleName);

                Status = FuseCopyResponse(ModuleStruct, Irp, IrpSp);

                if(Status == STATUS_SUCCESS) {
                    DbgPrint("Response for work from module %S processed successfully\n", ModuleName);
                } else {
                    DbgPrint("Response for work from module was unsuccessfully processed\n", ModuleName);
                }

                IoCompleteRequest(Irp, IO_NO_INCREMENT);
            }
        } else {

            //
            //  Oh noes! Someone is trying to trick us into thinking that they are a FUSE module
            //

            DbgPrint("FUSE request or response received from invalid source. Original file object \
                     pointer was %x and given file object pointer is %x for module %S\n",
                     ModuleStruct->ModuleFileObject, IrpSp->FileObject, IrpSp->FileObject->FileName.Buffer + 1);

            Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }
    } else {
        Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return Status;
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

    FuseCheckUnmountModule(IrpSp);

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

    DbgPrint("FuseFsdCreate called on '%wZ'\n", &IrpSp->FileObject->FileName);

    return FuseAddUserspaceIrp(Irp, IrpSp);
}

NTSTATUS
FuseFsdRead (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION IrpSp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdRead called on '%wZ'\n", &IrpSp->FileObject->FileName);
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

    DbgPrint("FuseFsdWrite called on '%wZ'\n", &IrpSp->FileObject->FileName);
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
    /* IN PIRP_CONTEXT IrpContext, */
    IN PIRP Irp
    )
{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;

    LONG Length;
    FILE_INFORMATION_CLASS FileInformationClass;
    PVOID Buffer;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    BOOLEAN FcbAcquired;

    PFILE_ALL_INFORMATION AllInfo;

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    FileObject = IrpSp->FileObject;

    //
    //  Reference our input parameters to make things easier
    //

    Length = (LONG)IrpSp->Parameters.QueryFile.Length;
    FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Decode the file object
    //

    TypeOfOpen = FuseDecodeFileObject( FileObject, &Vcb, &Fcb, &Ccb );
    Status = STATUS_SUCCESS;

        //
        //  Case on the type of open we're dealing with
        //

        switch (TypeOfOpen) {

        case UserVolumeOpen:

            //
            //  We cannot query the user volume open.
            //

            Status = STATUS_INVALID_PARAMETER;
            break;

        case UserFileOpen:

        case UserDirectoryOpen:

        case DirectoryFile:

            switch (FileInformationClass) {

            case FileAllInformation:

            case FileBasicInformation:

            case FileStandardInformation:

            case FileInternalInformation:

            case FileEaInformation:

            case FilePositionInformation:

            case FileNameInformation:

            case FileAlternateNameInformation:

            case FileNetworkOpenInformation:

                FuseFsdQueryInformation(Buffer, Irp);
                break;

            default:

                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            break;
 
        default:

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        //  If we overflowed the buffer, set the length to 0 and change the
        //  status to STATUS_BUFFER_OVERFLOW.
        //

        if ( Length < 0 ) {

            Status = STATUS_BUFFER_OVERFLOW;

            Length = 0;
        }

        //
        //  Set the information field to the number of bytes actually filled in
        //  and then complete the request
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryFile.Length - Length;

    return Status;
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

