/*++

Copyright (c) 2011 FUSE-NT Authors

Module Name:

    fusent_proto.h

Abstract:

    This module defines the common structs used to interact between userspace
    and kernelspace.

--*/

#ifndef NTPROTO_H
#define NTPROTO_H

#include "basictypes.h"
#include <ntdef.h>

//
//  NtFsControlFile FSCTL codes. These functions codes are chosen to be
//  outside of Microsoft's reserved range
//

// The control code for a request by a module to the driver asking the
// driver to allocate any necessary structures before the module assigns
// itself a drive letter
#define IRP_FUSE_MOUNT CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 3131, METHOD_NEITHER, FILE_ANY_ACCESS)

// The control code for a response from a module to the driver. Whenever a
// module finishes a create, read, write, etc. as requested by a userspace
// application, it calls down to the driver with this control code and a
// FUSENT_RESP in its buffer
#define IRP_FUSE_MODULE_RESPONSE CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 3132, METHOD_BUFFERED, FILE_ANY_ACCESS)

// The control code for a request for work by a module to the driver.
// The driver takes requests for work from modules and assigns userspace
// requests to them
#define IRP_FUSE_MODULE_REQUEST CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 3133, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Requests from Kernel to Userspace
//

typedef struct _FUSENT_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	IO_STACK_LOCATION iostack[0];
} FUSENT_REQ;

typedef struct _FUSENT_CREATE_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	IO_STACK_LOCATION iostack[0];

	// Followed by:
	// uint32_t fnamelen; // in bytes
	// uint16_t fname[0]; // fnamelen bytes of UTF-16LE file name
} FUSENT_CREATE_REQ;

// Takes a FUSE_CREATE_REQ and finds the variably-located fields:
void fusent_decode_request_create(FUSENT_CREATE_REQ *req, uint32_t *outfnamelen,
		uint16_t **outfnamep);

typedef struct _FUSENT_WRITE_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	IO_STACK_LOCATION iostack[0];

	// Followed by:
	// uint32_t buflen;
	// uint8_t buf[0]; // buflen bytes of write data
} FUSENT_WRITE_REQ;

// Takes a FUSE_WRITE_REQ and finds the variably-located fields:
void fusent_decode_request_write(FUSENT_WRITE_REQ *req, uint32_t *outbuflen,
		uint8_t **outbufp);

//
// Responses (Userspace to Kernelspace)
//

typedef struct _FUSENT_MOUNT { // weird special-case "response"
	uint32_t mtptlen; // in bytes
	uint32_t mtoptslen; // in bytes
	uint8_t rem[0]; // mtptlen bytes of UTF-16LE mount path,
	// followed by mtoptslen bytes of mount options
	// (mtptlen should be word-sized and nul-padded to make
	// mtopts aligned)
} FUSENT_MOUNT;

typedef struct _FUSENT_FILE_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
} FUSENT_FILE_INFORMATION;

typedef struct _FUSENT_RESP {
	PIRP pirp;
	PFILE_OBJECT fop;
	int error; // all high-level fuse operations return int
	// negative is error (-errno); zero is OK
	NTSTATUS status;
	union {
		struct {
			uint32_t buflen;
			//uint8_t buf[0]; buf is defined as following the FUSENT_RESP header.
		} read;
		struct {
			uint32_t written;
		} write;
		struct {
            uint32_t buflen;
			// defined as following the FUSENT_RESP header
		} query;
		struct {
			uint32_t buflen;
			// FILE_DIRECTORY_INFORMATION dirinfo[0]; defined as following the FUSENT_RESP header.
		} dirctrl;
		// potentially other kinds of responses here...
	} params;
} FUSENT_RESP;

#endif /* NTPROTO_H */
