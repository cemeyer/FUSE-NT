// Protocol structs, shared types, etc.

#include <ddk/winddk.h>
#include <stdint.h>

// Requests from Kernel to Userspace

typedef struct FUSENT_GENERIC_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	uint8_t irpstack[0];
};

typedef struct FUSENT_CREATE_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	uint32_t fnamelen; // in bytes
	uint8_t rem[0]; // irp stack, followed by fnamelen
	// bytes of UTF-16LE file name
};

typedef struct FUSENT_WRITE_REQ {
	PIRP pirp;
	PFILE_OBJECT fop;
	IRP irp;
	uint32_t buflen;
	uint8_t rem[0]; // irp stack, followed by buflen bytes of write data
};

// Responses (Userspace to Kernelspace)

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
