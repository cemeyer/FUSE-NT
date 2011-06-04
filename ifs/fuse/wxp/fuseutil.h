/*++

Module Name:

    fuseutil.h

Abstract:

    This module defines the utility functions used by the FUSE driver.
*/

#include <ntdef.h>
#include <ntifs.h>
#include <NtStatus.h>

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

PVOID
FuseMapUserBuffer (
    IN OUT PIRP Irp
    );

VOID
FusePrePostIrp (
    IN PIRP Irp
    );

VOID
FuseLockUserBuffer (
    IN OUT PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    );

