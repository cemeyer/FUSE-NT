#include <ntdef.h>
#include <NtStatus.h>
#include <Ntstrsafe.h>
#include "fuseprocs.h"
#include "hashmap.h"
#include "fusent_proto.h"

NTSTATUS
FuseCopyDirectoryControl (
    IN OUT PIRP Irp,
    IN PFILE_DIRECTORY_INFORMATION ModuleDirInformation,
    IN ULONG ModuleDirInformationLength
    )
{
    NTSTATUS Status = STATUS_NO_MORE_FILES;
    PIO_STACK_LOCATION IrpSp;
    FILE_INFORMATION_CLASS FileInformationClass;
    ULONG FileIndex;
    LONG Length;
    PVOID Buffer;

    PFILE_DIRECTORY_INFORMATION DirInfo;
    PFILE_NAMES_INFORMATION NamesInfo;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    
    Length = IrpSp->Parameters.QueryDirectory.Length;
    Buffer = FuseMapUserBuffer(Irp);

    FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;

    RtlZeroMemory(Buffer, Length);

    // iterate through files, I suppose, filling in as many entries as possible

    switch (FileInformationClass) {

    case FileDirectoryInformation:
    case FileFullDirectoryInformation:
    case FileIdFullDirectoryInformation:
    case FileBothDirectoryInformation:
    case FileIdBothDirectoryInformation:

        DirInfo = (PFILE_DIRECTORY_INFORMATION) Buffer;

        if(!ModuleDirInformation) {
            LARGE_INTEGER Time;
            WCHAR* FileName;
            ULONG FileNameLength;

            FileName = L"hello";
            FileNameLength = wcslen(FileName);

            KeQuerySystemTime(&Time);

            DirInfo->NextEntryOffset = 0;
            DirInfo->FileIndex = 0;
            DirInfo->CreationTime = Time;
            DirInfo->LastAccessTime = Time;
            DirInfo->LastWriteTime = Time;
            DirInfo->ChangeTime = Time;
            DirInfo->EndOfFile.QuadPart = 0;
            DirInfo->AllocationSize = DirInfo->EndOfFile;
            DirInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            DirInfo->FileNameLength = FileNameLength;
            RtlStringCchCopyW(DirInfo->FileName, FileNameLength, FileName);

            if(FileInformationClass == FileDirectoryInformation) {

                Length -= sizeof(FILE_DIRECTORY_INFORMATION);
            } else if(FileDirectoryInformation == FileFullDirectoryInformation) {

                Length -= sizeof(FILE_FULL_DIR_INFORMATION);
            } else if(FileDirectoryInformation == FileIdFullDirectoryInformation) {

                Length -= sizeof(FILE_ID_FULL_DIR_INFORMATION);
            } else if(FileDirectoryInformation == FileBothDirectoryInformation) {

                Length -= sizeof(FILE_BOTH_DIR_INFORMATION);
            } else if(FileDirectoryInformation == FileIdBothDirectoryInformation) {

                Length -= sizeof(FILE_ID_BOTH_DIR_INFORMATION);
            }
        } else {

            PFILE_DIRECTORY_INFORMATION LastDirInfo = NULL;

            //
            //  Make sure that we can at least read the basic fields of the directory struct
            //

            while(ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) &&
                ModuleDirInformationLength >= ModuleDirInformation->NextEntryOffset &&
                ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) + sizeof(WCHAR) * (ModuleDirInformation->FileNameLength - 1) &&
                Length >= (LONG) (sizeof(FILE_DIRECTORY_INFORMATION) + sizeof(WCHAR) * (ModuleDirInformation->FileNameLength - 1))) {

                ULONG FileNameLength = ModuleDirInformation->FileNameLength;
                WCHAR* FileNameField;
                ULONG DirInformationLength;

                if(FileInformationClass == FileDirectoryInformation) {

                    DirInformationLength = sizeof(FILE_DIRECTORY_INFORMATION);
                    FileNameField = DirInfo->FileName;
                } else if(FileDirectoryInformation == FileFullDirectoryInformation) {

                    PFILE_FULL_DIR_INFORMATION FullInformation = (PFILE_FULL_DIR_INFORMATION) DirInfo;

                    DirInformationLength = sizeof(FILE_FULL_DIR_INFORMATION);
                    FileNameField = FullInformation->FileName;
                } else if(FileDirectoryInformation == FileIdFullDirectoryInformation) {

                    PFILE_ID_FULL_DIR_INFORMATION FullIdInformation = (PFILE_ID_FULL_DIR_INFORMATION) DirInfo;
                    FullIdInformation->EaSize = 0;
                    FullIdInformation->FileId.QuadPart = str_hash_fn(DirInfo->FileName);

                    DirInformationLength = sizeof(FILE_ID_FULL_DIR_INFORMATION);
                    FileNameField = FullIdInformation->FileName;
                } else if(FileDirectoryInformation == FileBothDirectoryInformation) {

                    PFILE_BOTH_DIR_INFORMATION BothInformation = (PFILE_BOTH_DIR_INFORMATION) DirInfo;

                    DirInformationLength = sizeof(FILE_BOTH_DIR_INFORMATION);
                    FileNameField = BothInformation->FileName;
                } else if(FileDirectoryInformation == FileIdBothDirectoryInformation) {

                    PFILE_ID_BOTH_DIR_INFORMATION IdBothInformation = (PFILE_ID_BOTH_DIR_INFORMATION) DirInfo;

                    DirInformationLength = sizeof(FILE_ID_BOTH_DIR_INFORMATION);
                    FileNameField = IdBothInformation->FileName;
                }
                
                DirInformationLength += sizeof(WCHAR) * (FileNameLength - 1);

                //
                //  Directory entries must be LONGLONG aligned
                //

                DirInformationLength += sizeof(LONGLONG) - DirInformationLength % sizeof(LONGLONG);

                //
                //  If the buffer is large enough, perform the file name copy and fill in the extra fields
                //

                if(Length >= (LONG) DirInformationLength) {
                    RtlZeroMemory(DirInfo, DirInformationLength);

                    memcpy(DirInfo, ModuleDirInformation, sizeof(FILE_DIRECTORY_INFORMATION));
                    memcpy(FileNameField, ModuleDirInformation->FileName, sizeof(WCHAR) * FileNameLength);

                    LastDirInfo = DirInfo;

                    Length -= DirInformationLength;
                    ModuleDirInformationLength -= ModuleDirInformation->NextEntryOffset;
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
        }

        break;

    case FileNamesInformation:

        NamesInfo = (PFILE_NAMES_INFORMATION) Buffer;
        
        if(!ModuleDirInformation) {

            WCHAR* FileName;
            ULONG FileNameLength;

            FileName = L"hello";
            FileNameLength = wcslen(FileName);

            NamesInfo->NextEntryOffset = 0;
            NamesInfo->FileIndex = 0;
            NamesInfo->FileNameLength = sizeof(WCHAR) * FileNameLength;
            RtlStringCchCopyW(NamesInfo->FileName, FileNameLength, FileName);
            
            Length -= sizeof(FILE_NAMES_INFORMATION);
        } else {

            PFILE_NAMES_INFORMATION LastNamesInfo = NULL;

            while(ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) &&
                ModuleDirInformationLength >= ModuleDirInformation->NextEntryOffset &&
                ModuleDirInformationLength >= sizeof(FILE_DIRECTORY_INFORMATION) + sizeof(WCHAR) * (ModuleDirInformation->FileNameLength - 1) &&
                Length >= (LONG) (sizeof(FILE_NAMES_INFORMATION) + sizeof(WCHAR) * (ModuleDirInformation->FileNameLength - 1))) {

                WCHAR* FileName = ModuleDirInformation->FileName;
                ULONG FileNameLength = ModuleDirInformation->FileNameLength;
                ULONG NamesInformationLength = sizeof(FILE_NAMES_INFORMATION) + sizeof(WCHAR) * (FileNameLength - 1);

                //
                //  Align the names struct on an 8-byte (LONGLONG) boundary 
                //

                NamesInformationLength += sizeof(LONGLONG) - NamesInformationLength % sizeof(LONGLONG);

                memcpy(NamesInfo->FileName, FileName, sizeof(WCHAR) * FileNameLength);
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
        }

        break;

    default:

        Status = STATUS_INVALID_INFO_CLASS;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
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
    //  and then complete the request
    //

    Irp->IoStatus.Information = IrpSp->Parameters.QueryFile.Length - Length;
    Irp->IoStatus.Status = Status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
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
    LARGE_INTEGER Time;

    DbgPrint("FuseQueryBasicInfo\n");

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        KeQuerySystemTime(&Time);

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

    DbgPrint("FuseQueryStandardInfo\n");

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
    WCHAR* FileName = L"hello";
    ULONG FileNameLength = wcslen(FileName);
    LONG InformationLength = sizeof(FILE_NAME_INFORMATION) + sizeof(WCHAR) * FileNameLength;

    DbgPrint("FuseQueryNameInfo\n");

    //
    //  First check if there is enough space to write the information to the buffer
    //

    if(*Length < InformationLength) {

        Status = STATUS_BUFFER_OVERFLOW;
    } else {

        RtlZeroMemory(Buffer, InformationLength);

        RtlStringCchCopyW(Buffer->FileName, sizeof(WCHAR) * FileNameLength, FileName);
        Buffer->FileNameLength = sizeof(WCHAR) * FileNameLength;

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

    DbgPrint("FuseFsdQueryVolumeInformation\n");

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

    DbgPrint("FuseQueryFsVolumeInfo\n");

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