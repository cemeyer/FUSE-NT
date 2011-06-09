#include <ntdef.h>
#include <NtStatus.h>
#include "fuseprocs.h"
#include "hashmap.h"
#include "fusent_proto.h"
#include "fuseutil.h"

NTSTATUS
FuseCopyDirectoryControl (
    IN OUT PIRP Irp,
    IN PFILE_DIRECTORY_INFORMATION ModuleDirInformation,
    IN ULONG ModuleDirInformationLength
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    FILE_INFORMATION_CLASS FileInformationClass;
    LONG Length;
    PVOID Buffer;
    // strip off the WCHAR[1] from our size calculations
    ULONG TruncateLength = sizeof(WCHAR);

    PFILE_DIRECTORY_INFORMATION DirInfo;
    PFILE_NAMES_INFORMATION NamesInfo;
    PFILE_DIRECTORY_INFORMATION LastDirInfo = NULL;
    PFILE_NAMES_INFORMATION LastNamesInfo = NULL;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    
    Length = IrpSp->Parameters.QueryDirectory.Length;
    Buffer = FuseMapUserBuffer(Irp);

    DirInfo = (PFILE_DIRECTORY_INFORMATION) Buffer;
    NamesInfo = (PFILE_NAMES_INFORMATION) Buffer;

    FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;

    RtlZeroMemory(Buffer, Length);

    switch (FileInformationClass) {

    case FileDirectoryInformation:
    case FileFullDirectoryInformation:
    case FileIdFullDirectoryInformation:
    case FileBothDirectoryInformation:
    case FileIdBothDirectoryInformation:

        //
        //  Make sure that we can at least read the basic fields of the directory struct.
        //  The first WCHAR of the file name string is included in the size of the info struct
        //

        while(ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) &&
            ModuleDirInformationLength >= ModuleDirInformation->NextEntryOffset &&
            ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) + ModuleDirInformation->FileNameLength - TruncateLength &&
            Length >= (LONG) (sizeof(FILE_DIRECTORY_INFORMATION) + ModuleDirInformation->FileNameLength - TruncateLength + sizeof(LONGLONG))) {

            ULONG FileNameLength = ModuleDirInformation->FileNameLength;
            WCHAR* FileNameField;
            WCHAR* ShortNameField = NULL;
            PCCHAR ShortNameLengthField = NULL;
            ULONG DirInformationLength;

            if(FileInformationClass == FileDirectoryInformation) {

                DirInformationLength = sizeof(FILE_DIRECTORY_INFORMATION);
                FileNameField = DirInfo->FileName;
            } else if(FileInformationClass == FileFullDirectoryInformation) {

                PFILE_FULL_DIR_INFORMATION FullInformation = (PFILE_FULL_DIR_INFORMATION) DirInfo;

                DirInformationLength = sizeof(FILE_FULL_DIR_INFORMATION);
                FileNameField = FullInformation->FileName;
            } else if(FileInformationClass == FileIdFullDirectoryInformation) {

                PFILE_ID_FULL_DIR_INFORMATION FullIdInformation = (PFILE_ID_FULL_DIR_INFORMATION) DirInfo;

                DirInformationLength = sizeof(FILE_ID_FULL_DIR_INFORMATION);
                FileNameField = FullIdInformation->FileName;
            } else if(FileInformationClass == FileBothDirectoryInformation) {

                PFILE_BOTH_DIR_INFORMATION BothInformation = (PFILE_BOTH_DIR_INFORMATION) DirInfo;

                DirInformationLength = sizeof(FILE_BOTH_DIR_INFORMATION);
                FileNameField = BothInformation->FileName;
                ShortNameField = BothInformation->ShortName;
                ShortNameLengthField = &BothInformation->ShortNameLength;
            } else if(FileInformationClass == FileIdBothDirectoryInformation) {

                PFILE_ID_BOTH_DIR_INFORMATION IdBothInformation = (PFILE_ID_BOTH_DIR_INFORMATION) DirInfo;

                DirInformationLength = sizeof(FILE_ID_BOTH_DIR_INFORMATION);
                FileNameField = IdBothInformation->FileName;
                ShortNameField = IdBothInformation->ShortName;
                ShortNameLengthField = &IdBothInformation->ShortNameLength;
            }
                
            DirInformationLength += FileNameLength - sizeof(WCHAR) - TruncateLength;

            //
            //  Directory entries must be LONGLONG aligned
            //

            DirInformationLength += sizeof(LONGLONG) - DirInformationLength % sizeof(LONGLONG);

            //
            //  If the buffer is large enough, perform the file name copy and fill in the extra fields
            //

            if(Length >= (LONG) DirInformationLength) {
                RtlZeroMemory(DirInfo, DirInformationLength);

                memcpy(DirInfo, ModuleDirInformation, sizeof(FILE_DIRECTORY_INFORMATION) - TruncateLength);
                memcpy(FileNameField, ModuleDirInformation->FileName, FileNameLength);

                if(ShortNameField && ShortNameLengthField) {
                    CCHAR ShortNameLength = (CCHAR) min(FileNameLength, sizeof(WCHAR) * 12);
                    memcpy(ShortNameField, ModuleDirInformation->FileName, ShortNameLength);
                    *ShortNameLengthField = ShortNameLength;
                }

                LastDirInfo = DirInfo;

                Length -= DirInformationLength;
                ModuleDirInformationLength -= sizeof(FILE_DIRECTORY_INFORMATION) + ModuleDirInformation->FileNameLength - TruncateLength;
                ModuleDirInformation = (PFILE_DIRECTORY_INFORMATION) (((PCHAR) ModuleDirInformation) + ModuleDirInformation->NextEntryOffset);
                DirInfo = (PFILE_DIRECTORY_INFORMATION) (((PCHAR) DirInfo) + DirInformationLength);

                // ... leave the rest of the fields in the extended directory listing blank for now

            } else {
                    
                DirInfo->NextEntryOffset = 0;
                DirInfo->FileNameLength = 0;

                break;
            }
        }

        if(LastDirInfo) {

            LastDirInfo->NextEntryOffset = 0;
        }

        break;

    case FileNamesInformation:

        while(ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) &&
            ModuleDirInformationLength >= ModuleDirInformation->NextEntryOffset &&
            ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) + ModuleDirInformation->FileNameLength - TruncateLength &&
            Length >= (LONG) (sizeof(FILE_NAMES_INFORMATION) + ModuleDirInformation->FileNameLength - TruncateLength)) {

            WCHAR* FileName = ModuleDirInformation->FileName;
            ULONG FileNameLength = ModuleDirInformation->FileNameLength;
            ULONG NamesInformationLength = sizeof(FILE_NAMES_INFORMATION) + FileNameLength - TruncateLength;

            //
            //  Align the names struct on an 8-byte (LONGLONG) boundary 
            //

            NamesInformationLength += sizeof(LONGLONG) - NamesInformationLength % sizeof(LONGLONG);

            memcpy(NamesInfo->FileName, FileName, FileNameLength);
            NamesInfo->FileNameLength = FileNameLength;
            NamesInfo->FileIndex = str_hash_fn(FileName);
            NamesInfo->NextEntryOffset = NamesInformationLength;

            LastNamesInfo = NamesInfo;

            Length -= NamesInformationLength;
            ModuleDirInformationLength -= ModuleDirInformation->NextEntryOffset;
            ModuleDirInformation = (PFILE_DIRECTORY_INFORMATION) (((PCHAR) ModuleDirInformation) + ModuleDirInformation->NextEntryOffset);
            NamesInfo = (PFILE_NAMES_INFORMATION) (((PCHAR) NamesInfo) + NamesInformationLength);
        }

        if(LastNamesInfo) {
            LastNamesInfo->NextEntryOffset = 0;
        }

        break;

    default:

        Status = STATUS_INVALID_INFO_CLASS;
    }
    
    return Status;
}

NTSTATUS
FuseCopyInformation (
    IN OUT PIRP Irp,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG Length;
    FILE_INFORMATION_CLASS FileInformationClass;
    PFILE_ALL_INFORMATION AllInfo;
    LARGE_INTEGER Time;
    LARGE_INTEGER FileSize;
    
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DbgPrint("FuseFsdQueryInformation\n");

    Length = (LONG) IrpSp->Parameters.QueryFile.Length;
    FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;
    AllInfo = (PFILE_ALL_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

    switch (FileInformationClass) {

    case FileAllInformation:

        //
        //  For the all information class we'll typecast a local
        //  pointer to the output buffer and then call the
        //  individual routines to fill in the buffer.
        //

        FuseQueryBasicInfo(&AllInfo->BasicInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);
        FuseQueryStandardInfo(&AllInfo->StandardInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);
        FuseQueryNameInfo(IrpSp, &AllInfo->NameInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);

        break;

    case FileBasicInformation:

        FuseQueryBasicInfo(&AllInfo->BasicInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);
        break;

    case FileStandardInformation:

        FuseQueryStandardInfo(&AllInfo->StandardInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);
        break;

    case FileNameInformation:

        FuseQueryNameInfo(IrpSp, &AllInfo->NameInformation, ModuleFileInformation, ModuleFileInformationLength, &Length);
        break;

    default:

        Status = STATUS_INVALID_PARAMETER;
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
    //

    Irp->IoStatus.Information = IrpSp->Parameters.QueryFile.Length - Length;
    
    return Status;
}

NTSTATUS
FuseQueryBasicInfo (
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_BASIC_INFORMATION);

    DbgPrint("FuseQueryBasicInfo\n");

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        Buffer->FileAttributes = ModuleFileInformation->FileAttributes;
        Buffer->ChangeTime = ModuleFileInformation->ChangeTime;
        Buffer->CreationTime = ModuleFileInformation->CreationTime;
        Buffer->LastAccessTime = ModuleFileInformation->LastAccessTime;
        Buffer->LastWriteTime = ModuleFileInformation->LastWriteTime;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryStandardInfo (
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_STANDARD_INFORMATION);
    LARGE_INTEGER FileSize;

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryStandardInfo\n");
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        FileSize.QuadPart = 0;

        // the size of the file as allocated on disk--for us this will just be the file size
        Buffer->AllocationSize = ModuleFileInformation->AllocationSize;

        // the position of the byte following the last byte in this file
        Buffer->EndOfFile = ModuleFileInformation->EndOfFile;

        Buffer->DeletePending = ModuleFileInformation->DeletePending;
        Buffer->Directory = ModuleFileInformation->Directory;
        Buffer->NumberOfLinks = ModuleFileInformation->NumberOfLinks;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryNameInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN FUSENT_FILE_INFORMATION* ModuleFileInformation,
    IN ULONG ModuleFileInformationLength,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    WCHAR* FileName = IrpSp->FileObject->FileName.Buffer;
    ULONG FileNameLength = sizeof(WCHAR) * IrpSp->FileObject->FileName.Length;
    LONG InformationLength = sizeof(FILE_NAME_INFORMATION) + FileNameLength;

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryNameInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        memcpy(Buffer->FileName, FileName, FileNameLength);
        Buffer->FileNameLength = FileNameLength;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseCopyVolumeInformation (
    IN OUT PIRP Irp
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;

    LONG Length;
    FS_INFORMATION_CLASS FsInformationClass;
    PVOID Buffer;
    ULONG VolumeLabelLength;
    LARGE_INTEGER Time;

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

#ifdef FUSE_DEBUG1
    DbgPrint("FuseCopyVolumeInformation called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif
    
    //
    //  Reference our input parameters to make things easier
    //

    Length = IrpSp->Parameters.QueryVolume.Length;
    FsInformationClass = IrpSp->Parameters.QueryVolume.FsInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;

    switch (FsInformationClass) {

    case FileFsVolumeInformation:

        Status = FuseQueryFsVolumeInfo( Irp, IrpSp, (PFILE_FS_VOLUME_INFORMATION) Buffer, &Length );
        break;

    case FileFsAttributeInformation:

        Status = FuseQueryFsAttributeInfo( Irp, IrpSp, (PFILE_FS_ATTRIBUTE_INFORMATION) Buffer, &Length );
        break;

    case FileFsSizeInformation:

        Status = FuseQueryFsSizeInfo( IrpSp, (PFILE_FS_SIZE_INFORMATION) Buffer, &Length );
        break;

    case FileFsDeviceInformation:

        Status = FuseQueryFsDeviceInfo( IrpSp, (PFILE_FS_DEVICE_INFORMATION) Buffer, &Length );
        break;

    case FileFsFullSizeInformation:

        Status = FuseQueryFsFullSizeInfo( IrpSp, (PFILE_FS_FULL_SIZE_INFORMATION) Buffer, &Length );
        break;
    
    default:

        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    Irp->IoStatus.Information = IrpSp->Parameters.QueryVolume.Length - Length;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return Status;
}

NTSTATUS
FuseQueryFsVolumeInfo (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength;
    LARGE_INTEGER Time;
    WCHAR* VolumeLabel;
    ULONG VolumeLabelLength;

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryFsVolumeInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  Get the length of the module name
    //

    FuseExtractModuleName(Irp, &VolumeLabelLength);

    //
    //  First check if there is enough space to write the information to the buffer
    //

    InformationLength = sizeof(FILE_FS_VOLUME_INFORMATION) + sizeof(WCHAR) * VolumeLabelLength;

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        FuseCopyModuleName(Irp, Buffer->VolumeLabel, &VolumeLabelLength);
        Buffer->VolumeLabelLength = sizeof(WCHAR) * VolumeLabelLength;
        Buffer->VolumeSerialNumber = 0x1337;

        KeQuerySystemTime(&Time);
        Buffer->VolumeCreationTime = Time;
        Buffer->SupportsObjects = FALSE;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryFsSizeInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_SIZE_INFORMATION Buffer,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_FS_SIZE_INFORMATION);

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryFsSizeInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        // let's pretend we have a 64GB filesystem
        Buffer->BytesPerSector = 4096;
        Buffer->SectorsPerAllocationUnit = 4096;
        Buffer->AvailableAllocationUnits.LowPart = 4096;
        Buffer->TotalAllocationUnits.LowPart = 4096;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryFsDeviceInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_DEVICE_INFORMATION Buffer,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_FS_DEVICE_INFORMATION);

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryFsDeviceInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        Buffer->DeviceType = FILE_DEVICE_FILE_SYSTEM;
        Buffer->Characteristics = 0;

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryFsAttributeInfo (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_FS_ATTRIBUTE_INFORMATION);

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryFsAttributeInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        ULONG ModuleNameLength;

        RtlZeroMemory(Buffer, InformationLength);

        Buffer->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK;
        FuseCopyModuleName(Irp, Buffer->FileSystemName, &ModuleNameLength);
        Buffer->FileSystemNameLength = ModuleNameLength;
        Buffer->MaximumComponentNameLength = 256 * sizeof(WCHAR);

        *Length -= InformationLength;
    }

    return Status;
}

NTSTATUS
FuseQueryFsFullSizeInfo (
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFILE_FS_FULL_SIZE_INFORMATION Buffer,
    IN OUT PLONG Length
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    LONG InformationLength = sizeof(FILE_FS_FULL_SIZE_INFORMATION);

#ifdef FUSE_DEBUG1
    DbgPrint("FuseQueryFsFullSizeInfo called on '%wZ'\n", &IrpSp->FileObject->FileName);
#endif

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        // let's pretend we have a 64GB filesystem
        Buffer->BytesPerSector = 4098;
        Buffer->SectorsPerAllocationUnit = 4098;
        Buffer->CallerAvailableAllocationUnits.LowPart = 4098;
        Buffer->ActualAvailableAllocationUnits.LowPart = 4098;
        Buffer->TotalAllocationUnits.LowPart = 4098;

        *Length -= InformationLength;
    }

    return Status;
}