#ifdef __CYGWIN__
#ifndef NTPROTO_H
#define NTPROTO_H

// Protocol structs, shared types, etc.

#include <ddk/winddk.h>
#include <stdint.h>

// NtFsControl major function codes
#define IRP_FUSE_MOUNT  0x1337
#define IRP_FUSE_MODULE_RESPONSE 0x1338
#define IRP_FUSE_MODULE_REQUEST 0x1339

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
inline void fusent_decode_request_create(FUSENT_CREATE_REQ *req, uint32_t *outfnamelen,
		uint16_t **outfnamep)
{
	uint32_t *fnamelenp = (uint32_t *)(req->iostack + req->irp.StackCount);
	*outfnamelen = *fnamelenp;
	*outfnamep = (uint16_t *)(fnamelenp + 1);
}

typedef struct FUSENT_WRITE_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	IO_STACK_LOCATION iostack[0];

	// Followed by:
	// uint32_t buflen;
	// uint8_t buf[0]; // buflen bytes of write data
} FUSENT_WRITE_REQ;

// Takes a FUSE_WRITE_REQ and finds the variably-located fields:
inline void fusent_decode_request_write(FUSENT_WRITE_REQ *req, uint32_t *outbuflen,
		uint8_t **outbufp)
{
	uint32_t *buflenp = (uint32_t *)(req->iostack + req->irp.StackCount);
	*outbuflen = *buflenp;
	*outbufp = (uint8_t *)(buflenp + 1);
}

//
// Responses (Userspace to Kernelspace)
//

typedef struct FUSENT_MOUNT { // weird special-case "response"
	uint32_t mtptlen; // in bytes
	uint32_t mtoptslen; // in bytes
	uint8_t rem[0]; // mtptlen bytes of UTF-16LE mount path,
	// followed by mtoptslen bytes of mount options
	// (mtptlen should be word-sized and nul-padded to make
	// mtopts aligned)
};

typedef struct FUSENT_GENERIC_RESP {
	PIRP pirp;
	PFILE_OBJECT fop;
	int retval; // all high-level fuse operations return int
	union {
		struct {
			uint32_t buflen;
			uint8_t buf[0];
		} read;
		// potentially other kinds of responses here...
	} extradata;
};

#endif /* NTPROTO_H */
#endif /* __CYGWIN__ */
