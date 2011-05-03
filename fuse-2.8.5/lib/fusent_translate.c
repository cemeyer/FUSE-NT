#ifdef __CYGWIN__

#include <iconv.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "fuse_kernel.h"
#include "fusent_proto.h"
#include "fusent_irpdecode.h"

// Sets up any data structures fusent_translate will need to persist
// across calls.
iconv_t cd_utf16le_to_utf8;
void fusent_translate_setup()
{
	cd_utf16le_to_utf8 = iconv_open("UTF-8//IGNORE", "UTF-16LE");
}

// Destroys any persistant data structures at shut down.
void fusent_translate_teardown()
{
	iconv_close(cd_utf16le_to_utf8);
}

// Translates a FUSENT kernel request (`req') into a FUSE request, filling in
// `outbuf'; `sz' takes on the length of `outbuf' in bytes.
//
// `outbuf' is assumed to be large enough to hold any FUSE request packet.
//
// Returns non-negative on success.
int fusent_translate_nt_to_fuse(FUSENT_REQ *req, char *outbuf, int *sz)
{
	int status;
	IO_STACK_LOCATION *iosp;
	uint8_t irptype;

	status = fusent_decode_irp(&req->irp, &req->iostack[0], &irptype, &iosp);
	if (status < 0) return status;

	struct fuse_in_header *header = (struct fuse_in_header *)outbuf;
	void *arguments = outbuf + sizeof(struct fuse_in_header);
	*sz = sizeof(struct fuse_in_header);

	switch (irptype) {
		case IRP_MJ_CREATE:
			// TODO(cemeyer): Some of the behavior here probably doesn't match
			// one-to-one with NT. Does anyone actually use
			// FILE_SUPERSEDE? We ignore a lot of CreateOptions flags.

			{	// Scope is here so we can define more stack variables:
			uint32_t CreateOptions = iosp->Parameters.Create.Options;
			uint16_t ShareAccess = iosp->Parameters.Create.ShareAccess;
			uint8_t CreateDisp = CreateOptions >> 24;

			int fuse_flags = 0, mode = S_IRWXU | S_IRWXG | S_IRWXO;
			if (CreateDisp != FILE_OPEN && CreateDisp != FILE_OVERWRITE)
				fuse_flags |= O_CREAT;
			if (CreateDisp == FILE_CREATE) fuse_flags |= O_EXCL;
			if (CreateDisp == FILE_OVERWRITE || CreateDisp == FILE_OVERWRITE_IF ||
					CreateDisp == FILE_SUPERSEDE)
				fuse_flags |= O_TRUNC;
			if (CreateOptions & FILE_DIRECTORY_FILE) fuse_flags |= O_DIRECTORY;

			header->unique = 0;		// as far as I can tell, this is only used for interrupting ops
			header->nodeid = XXX;		// inode number
			header->uid = 0;		// TODO: do these even have meaningful values?
			header->gid = 0;
			header->pid = getpid();

			uint32_t fnamelen;
			uint16_t *fnamep;
			fusent_decode_request_create((FUSENT_CREATE_REQ *)req,
					&fnamelen, &fnamep);

			if (fuse_flags & O_CREAT) {
				header->opcode = FUSE_CREATE;
				struct fuse_create_in *fc = (struct fuse_create_in *)arguments;
				*sz += sizeof(struct fuse_create_in);

				char *inbuf = fnamep;
				size_t inbytes = fnamelen, outbytes = 255;
				char *outbuf = PARAM(fc);

				// Encode the UTF-16LE name from windows into UTF-8
				iconv(cd_utf16le_to_utf8, &inbuf, &inbytes, &outbuf, &outbytes);

				fc->flags = fuse_flags;
				fc->mode = mode;
				fc->umask = S_IWGRP | S_IWOTH;

				*sz += (255 - outbytes); // size of utf-8 name
				header->len = *sz;
			}
			else {
				header->opcode = FUSE_OPEN;
				struct fuse_open_in *fo = (struct fuse_open_in *)arguments;
				*sz += sizeof(struct fuse_open_in);

				fo->flags = fuse_flags;
				// TODO: verify this is the intended purpose of ->len field:
				*sz = fih->len = sizeof(struct fuse_in_header) +
					sizeof(struct fuse_open_in);
			}
			}

			break;
		case IRP_MJ_READ:
			XXX;
			break;
		case IRP_MJ_WRITE:
			XXX;
			break;
		default:
			return -1;
	}

	return 0;
}

#endif /* __CYGWIN__ */
