/*++

Copyright (c) 2011 FUSE-NT Authors

Module Name:

    fuseutil.c

Abstract:

    This module implements some utility routines for the FUSE driver.

--*/

#include <ntdef.h>
#include <NtStatus.h>
#include "fuseutil.h"

LPWSTR
FuseExtractModuleName (
    IN PIRP Irp,
    OUT PULONG Length
    )
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    WCHAR* FileName = IrpSp->FileObject->FileName.Buffer;
    WCHAR* ModuleName = FileName;
    WCHAR* BackslashPosition;
    ULONG FileNameLength = IrpSp->FileObject->FileName.Length;

    *Length = FileNameLength;

    if(FileNameLength > 0) {
        if(FileName[0] == L'\\') {
            ModuleName ++;
            *Length = FileNameLength - 1;
        }

        BackslashPosition = wcsstr(ModuleName, L"\\");

        if(BackslashPosition) {
            *Length = BackslashPosition - ModuleName;
        }
    }

    return ModuleName;
}

LPWSTR
FuseAllocateModuleName (
    IN PIRP Irp
    )
{
    WCHAR* ModuleName;
    WCHAR* ModuleNamePointer;
    ULONG ModuleNameLength;

    ModuleNamePointer = FuseExtractModuleName(Irp, &ModuleNameLength);
    ModuleName = (WCHAR*) ExAllocatePoolWithTag(PagedPool, sizeof(WCHAR) * (ModuleNameLength + 1), 'esuF');

    memcpy(ModuleName, ModuleNamePointer, sizeof(WCHAR) * ModuleNameLength);
    ModuleName[ModuleNameLength] = L'\0';

    return ModuleName;
}

VOID
FuseCopyModuleName (
    IN PIRP Irp,
    OUT LPWSTR Destination,
    OUT PULONG Length
    )
{
    WCHAR* ModuleName = FuseExtractModuleName(Irp, Length);

    memcpy(Destination, ModuleName, sizeof(WCHAR) * (*Length));
    Destination[*Length] = L'\0';
}

PVOID
FuseMapUserBuffer (
    IN OUT PIRP Irp
    )
{
    //
    // If there is no Mdl, then we must be in the Fsd, and we can simply
    // return the UserBuffer field from the Irp.
    //

    if (Irp->MdlAddress == NULL) {

        return Irp->UserBuffer;
    
    } else {

        PVOID Address = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

        if (Address == NULL) {

            ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
        }

        return Address;
    }
}

VOID
FusePrePostIrp (
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs any neccessary work before STATUS_PENDING is
    returned with the Fsd thread.  This routine is called within the
    filesystem and by the oplock package.

Arguments:

    Context - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION IrpSp;

    //
    //  If there is no Irp, we are done.
    //

    if (Irp == NULL) {

        return;
    }

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  We need to lock the user's buffer, unless this is an MDL-read,
    //  in which case there is no user buffer.
    //
    //  **** we need a better test than non-MDL (read or write)!

    if (IrpSp->MajorFunction == IRP_MJ_READ ||
        IrpSp->MajorFunction == IRP_MJ_WRITE) {

        //
        //  If not an Mdl request, lock the user's buffer.
        //

        if (!FlagOn( IrpSp->MinorFunction, IRP_MN_MDL )) {

            FuseLockUserBuffer( Irp,
                                (IrpSp->MajorFunction == IRP_MJ_READ) ?
                                IoWriteAccess : IoReadAccess,
                                (IrpSp->MajorFunction == IRP_MJ_READ) ?
                                IrpSp->Parameters.Read.Length : IrpSp->Parameters.Write.Length );
        }

    //
    //  We also need to check whether this is a query file operation.
    //

    } else if (IrpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL
               && IrpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

        FuseLockUserBuffer( Irp,
                            IoWriteAccess,
                            IrpSp->Parameters.QueryDirectory.Length );

    //
    //  We also need to check whether this is a query ea operation.
    //

    } else if (IrpSp->MajorFunction == IRP_MJ_QUERY_EA) {

        FuseLockUserBuffer( Irp,
                            IoWriteAccess,
                            IrpSp->Parameters.QueryEa.Length );

    //
    //  We also need to check whether this is a set ea operation.
    //

    } else if (IrpSp->MajorFunction == IRP_MJ_SET_EA) {

        FuseLockUserBuffer( Irp,
                            IoReadAccess,
                            IrpSp->Parameters.SetEa.Length );

    //
    //  These two FSCTLs use neither I/O, so check for them.
    //

    } else if ((IrpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
               ((IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_GET_VOLUME_BITMAP) ||
                (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_GET_RETRIEVAL_POINTERS))) {

        FuseLockUserBuffer( Irp,
                            IoWriteAccess,
                            IrpSp->Parameters.FileSystemControl.OutputBufferLength );
    }

    return;
}

VOID
FuseLockUserBuffer (
    IN OUT PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine locks the specified buffer for the specified type of
    access.  The file system requires this routine since it does not
    ask the I/O system to lock its buffers for direct I/O.  This routine
    may only be called from the Fsd while still in the user context.

    Note that this is the *input/output* buffer.

Arguments:

    Irp - Pointer to the Irp for which the buffer is to be locked.

    Operation - IoWriteAccess for read operations, or IoReadAccess for
                write operations.

    BufferLength - Length of user buffer.

Return Value:

    None

--*/

{
    PMDL Mdl = NULL;

    if (Irp->MdlAddress == NULL) {

        //
        // Allocate the Mdl, and Raise if we fail.
        //

        Mdl = IoAllocateMdl( Irp->UserBuffer, BufferLength, FALSE, FALSE, Irp );

        if (Mdl == NULL) {

            DbgPrint("Failed to allocate MDL\n");
        }

        //
        // Now probe the buffer described by the Irp.  If we get an exception,
        // deallocate the Mdl and return the appropriate "expected" status.
        //

        try {

            MmProbeAndLockPages( Mdl,
                                 Irp->RequestorMode,
                                 Operation );

        } except(EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl( Mdl );
            Irp->MdlAddress = NULL;
        }
    }
}

