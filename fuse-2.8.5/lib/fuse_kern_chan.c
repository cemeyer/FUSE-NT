/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_lowlevel.h"
#include "fuse_kernel.h"
#include "fuse_i.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#ifdef __CYGWIN__
# include "fusent_proto.h"
#endif

static int fuse_kern_chan_receive(struct fuse_chan **chp, char *buf,
				  size_t size)
{
	struct fuse_chan *ch = *chp;
	ssize_t res;
	struct fuse_session *se = fuse_chan_session(ch);
	assert(se != NULL);

#if defined __CYGWIN__
	IO_STATUS_BLOCK iosb;
	HANDLE ioevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	NTSTATUS stat = NtFsControlFile(fuse_chan_fd(ch), ioevent, NULL, NULL, &iosb, IRP_FUSE_MODULE_REQUEST, NULL, 0, buf, size);

	if (fuse_session_exited(se))
		return 0;

	if (stat != STATUS_PENDING) {
		CloseHandle(ioevent);
	}
	else {
		DWORD waitres = WaitForSingleObject(ioevent, INFINITE);
		CloseHandle(ioevent);

		if (waitres != WAIT_OBJECT_0) {
			fprintf(stderr, "fuse: waiting for receive() asyncio failed: (%08lx)\n", waitres);
			return -EFAULT;
		}
		stat = STATUS_SUCCESS;
	}

	if (stat != STATUS_SUCCESS) {
		fprintf(stderr, "fuse: reading device: got (%08lx), expected STATUS_SUCCESS\n", stat);
		return -EFAULT;
	}

	res = iosb.Information;
#else
	int err;

restart:
	res = read(fuse_chan_fd(ch), buf, size);
	err = errno;

	if (fuse_session_exited(se))
		return 0;
	if (res == -1) {
		/* ENOENT means the operation was interrupted, it's safe
		   to restart */
		if (err == ENOENT)
			goto restart;

		if (err == ENODEV) {
			fuse_session_exit(se);
			return 0;
		}
		/* Errors occuring during normal operation: EINTR (read
		   interrupted), EAGAIN (nonblocking I/O), ENODEV (filesystem
		   umounted) */
		if (err != EINTR && err != EAGAIN)
			perror("fuse: reading device");
		return -err;
	}
#endif

#if defined __CYGWIN__
	if ((size_t) res < sizeof(FUSENT_REQ))
#else
	if ((size_t) res < sizeof(struct fuse_in_header))
#endif
	{
		fprintf(stderr, "short read on fuse device\n");
		return -EIO;
	}

	return res;
}

static int fuse_kern_chan_send(struct fuse_chan *ch, const struct iovec iov[],
			       size_t count)
{
	if (!iov) return 0;

#if defined __CYGWIN__
	IO_STATUS_BLOCK iosb;
	int io;
	size_t total = 0, idx = 0;
	char *buf;

	// If only one iovec, skip the copy (should be the case for all of FUSE-NT)
	if (count == 1) {
		buf = iov[0].iov_base;
		total = iov[0].iov_len;
	}

	// Otherwise, merge each buf into a single long buf
	else {
		for (io = 0; io < count; io ++)
			total += iov[io].iov_len;

		buf = malloc(total);
		for (io = 0; io < count; io ++) {
			size_t iolen = iov[io].iov_len;
			if (!iolen) continue;
			memcpy(buf + idx, iov[io].iov_base, iolen);
			idx += iolen;
		}
	}

	HANDLE ioevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	NTSTATUS stat = NtFsControlFile(fuse_chan_fd(ch), ioevent, NULL, NULL, &iosb,
			IRP_FUSE_MODULE_RESPONSE, buf, total, NULL, 0);

	if (stat != STATUS_PENDING) {
		CloseHandle(ioevent);
	}
	else {
		DWORD waitres = WaitForSingleObject(ioevent, INFINITE);
		CloseHandle(ioevent);

		if (waitres != WAIT_OBJECT_0) {
			fprintf(stderr, "fuse: waiting for send() asyncio failed: (%08x)\n", waitres);
			if (count != 1)
				free(buf);
			return -EFAULT;
		}
		stat = STATUS_SUCCESS;
	}

	if (count != 1)
		free(buf);

	if (stat != STATUS_SUCCESS) {
		struct fuse_session *se = fuse_chan_session(ch);

		assert(se != NULL);

		if (fuse_session_exited(se)) return 0;

		fprintf(stderr, "fuse: writing device: got (%08x), expected STATUS_SUCCESS\n", stat);

		return -EFAULT;
	}
#else
	ssize_t res = writev(fuse_chan_fd(ch), iov, count);
	int err = errno;

	if (res == -1) {
		struct fuse_session *se = fuse_chan_session(ch);

		assert(se != NULL);

		/* ENOENT means the operation was interrupted */
		if (!fuse_session_exited(se) && err != ENOENT)
			perror("fuse: writing device");
		return -err;
	}
#endif
	return 0;
}

static void fuse_kern_chan_destroy(struct fuse_chan *ch)
{
#if defined __CYGWIN__
	CloseHandle(fuse_chan_fd(ch));
#else
	close(fuse_chan_fd(ch));
#endif
}

#define MIN_BUFSIZE 0x21000

#if defined __CYGWIN__
struct fuse_chan *fuse_kern_chan_new(HANDLE fd)
#else
struct fuse_chan *fuse_kern_chan_new(int fd)
#endif
{
	struct fuse_chan_ops op = {
		.receive = fuse_kern_chan_receive,
		.send = fuse_kern_chan_send,
		.destroy = fuse_kern_chan_destroy,
	};
	size_t bufsize = getpagesize() + 0x1000;
	bufsize = bufsize < MIN_BUFSIZE ? MIN_BUFSIZE : bufsize;
	return fuse_chan_new(&op, fd, bufsize, NULL);
}
