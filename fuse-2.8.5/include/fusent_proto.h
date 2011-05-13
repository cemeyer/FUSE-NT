#ifdef __CYGWIN__
#ifndef NTPROTO_H
#define NTPROTO_H

// Protocol structs, shared types, etc.

// Why don't any of the windows headers include the headers they depend on
// to define types (ULONG, NTSTATUS, CSHORT, WCHAR, ...)? Retarded.
//
// Why don't they just use the standard types (uint32_t, ...)?!?
#define NTOSAPI /**/
#define DDKAPI /**/
#include <windef.h>
#include <winnt.h>
#include <ntdef.h>
#include <ddk/ntddk.h>
#include <ddk/winddk.h>
#include <stdint.h>
#include <ddk/ntifs.h>

//
// NtFsControlFile major function codes
//

// The control code for a request by a module to the driver asking the
// driver to allocate any necessary structures before the module assigns
// itself a drive letter
#define IRP_FUSE_MOUNT  0x1337

// The control code for a request for work by a module to the driver.
// The driver takes requests for work from modules and assigns userspace
// requests to them
#define IRP_FUSE_MODULE_REQUEST 0x1339

// The control code for a response from a module to the driver. Whenever a
// module finishes a create, read, write, etc. as requested by a userspace
// application, it calls down to the driver with this control code and a
// FUSENT_RESP in its buffer
#define IRP_FUSE_MODULE_RESPONSE 0x1338


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
		// potentially other kinds of responses here...
	} params;
} FUSENT_RESP;

#endif /* NTPROTO_H */
#endif /* __CYGWIN__ */
