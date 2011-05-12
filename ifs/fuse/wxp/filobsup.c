/*++

Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    FilObSup.c

Abstract:

    This module implements the Fuse File object support routines.


--*/

#include "FuseProcs.h"

TYPE_OF_OPEN
FuseDecodeFileObject (
    IN PFILE_OBJECT FileObject,
    OUT PVCB *Vcb,
    OUT PFCB *FcbOrDcb,
    OUT PCCB *Ccb
    )

/*++

Routine Description:

    This procedure takes a pointer to a file object, that has already been
    opened by the Fuse file system and figures out what really is opened.

Arguments:

    FileObject - Supplies the file object pointer being interrogated

    Vcb - Receives a pointer to the Vcb for the file object.

    FcbOrDcb - Receives a pointer to the Fcb/Dcb for the file object, if
        one exists.

    Ccb - Receives a pointer to the Ccb for the file object, if one exists.

Return Value:

    TYPE_OF_OPEN - returns the type of file denoted by the input file object.

        UserFileOpen - The FO represents a user's opened data file.
            Ccb, FcbOrDcb, and Vcb are set.  FcbOrDcb points to an Fcb.

        UserDirectoryOpen - The FO represents a user's opened directory.
            Ccb, FcbOrDcb, and Vcb are set.  FcbOrDcb points to a Dcb/RootDcb

        UserVolumeOpen - The FO represents a user's opened volume.
            Ccb and Vcb are set. FcbOrDcb is null.

        VirtualVolumeFile - The FO represents the special virtual volume file.
            Vcb is set, and Ccb and FcbOrDcb are null.

        DirectoryFile - The FO represents a special directory file.
            Vcb and FcbOrDcb are set. Ccb is null.  FcbOrDcb points to a
            Dcb/RootDcb.

        EaFile - The FO represents an Ea Io stream file.
            FcbOrDcb, and Vcb are set.  FcbOrDcb points to an Fcb, and Ccb is
            null.

--*/

{
    TYPE_OF_OPEN TypeOfOpen;
    PVOID FsContext;
    PVOID FsContext2;

    DebugTrace(+1, Dbg, "FuseDecodeFileObject, FileObject = %08lx\n", FileObject);

    //
    //  Reference the fs context fields of the file object, and zero out
    //  the out pointer parameters.
    //

    FsContext = FileObject->FsContext;
    FsContext2 = FileObject->FsContext2;

    //
    //  Special case the situation where FsContext is null
    //

    if (FsContext == NULL) {

        *Ccb = NULL;
        *FcbOrDcb = NULL;
        *Vcb = NULL;

        TypeOfOpen = UnopenedFileObject;

    } else {

        //
        //  Now we can case on the node type code of the fscontext pointer
        //  and set the appropriate out pointers
        //

        switch (NodeType(FsContext)) {

        case FAT_NTC_VCB:

            *Ccb = FsContext2;
            *FcbOrDcb = NULL;
            *Vcb = FsContext;

            TypeOfOpen = ( *Ccb == NULL ? VirtualVolumeFile : UserVolumeOpen );

            break;

        case FAT_NTC_ROOT_DCB:
        case FAT_NTC_DCB:

            *Ccb = FsContext2;
            *FcbOrDcb = FsContext;
            *Vcb = (*FcbOrDcb)->Vcb;

            TypeOfOpen = ( *Ccb == NULL ? DirectoryFile : UserDirectoryOpen );


            break;

        case FAT_NTC_FCB:

            *Ccb = FsContext2;
            *FcbOrDcb = FsContext;
            *Vcb = (*FcbOrDcb)->Vcb;

            TypeOfOpen = ( *Ccb == NULL ? EaFile : UserFileOpen );


            break;
	//Removed a default case and a FuseBugCheckCall

        }
    }


    return TypeOfOpen;
}



