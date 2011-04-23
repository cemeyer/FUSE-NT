#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fuse_kernel.h>
#include "ntproto.h"
#include "irpdecode.h"

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

	switch (irptype) {
		case IRP_MJ_CREATE:
			// TODO: Some of the behavior here probably doesn't match
			// one-to-one with NT. Does anyone actually use
			// FILE_SUPERSEDE? We ignore a lot of CreateOptions flags.

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

			*sz = 0;
			struct fuse_in_header *fih = (struct fuse_in_header *)outbuf;
			fih->opcode = XXX;
			fih->unique = XXX;
			fih->nodeid = XXX;
			fih->uid = XXX;
			fih->gid = XXX;
			fih->pid = XXX;

			if (fuse_flags & O_CREAT) {
				struct fuse_create_in *fc = (struct fuse_create_in *)(fih + 1);
				fc->flags = fuse_flags;
				fc->mode = mode;
				fc->umask = XXX;
				// TODO: verify this is the intended purpose of ->len field:
				*sz = fih->len = sizeof(struct fuse_in_header) +
					sizeof(struct fuse_create_in);
			}
			else {
				struct fuse_open_in *fo = (struct fuse_open_in *)(fih + 1);
				fo->fh = XXX;
				fo->open_flags = fuse_flags;
				// TODO: verify this is the intended purpose of ->len field:
				*sz = fih->len = sizeof(struct fuse_in_header) +
					sizeof(struct fuse_open_in);
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
