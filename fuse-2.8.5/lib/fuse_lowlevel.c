/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include "fuse_common_compat.h"
#include "fuse_lowlevel_compat.h"

#ifdef __CYGWIN__
# include "fusent_proto.h"
# include "fusent_routines.h"
# include "st.h"

# include <ddk/ntifs.h>

# include <iconv.h>

# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <ctype.h>

# define FUSENT_MAX_PATH 256
#endif /* __CYGWIN__ */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define PARAM(inarg) (((char *)(inarg)) + sizeof(*(inarg)))
#define OFFSET_MAX 0x7fffffffffffffffLL

#ifdef __CYGWIN__
// Sets up any data structures the fusent translate layer will need to persist
// across calls.

// Conversion descriptor for iconv; used to convert between UTF-16LE Windows filenames
// and UTF-8 FUSE-hosted filesystem names.
static iconv_t cd_utf16le_to_utf8;

// Maps between open FileObject pointers (fops) and fuse_file_info pointers / inode numbers:
static st_table *fusent_fop_fi_map;
static st_table *fusent_fop_ino_map;
// Map to basenames:
static st_table *fusent_fop_basename_map;
// Map to open file offsets:
static st_table *fusent_fop_pos_map;
// Map to open file sync flag:
static st_table *fusent_fop_sync_map;

void fusent_translate_setup()
{
	cd_utf16le_to_utf8 = iconv_open("UTF-8//IGNORE", "UTF-16LE");
	fusent_fop_fi_map = st_init_numtable();
	fusent_fop_ino_map = st_init_numtable();
	fusent_fop_basename_map = st_init_numtable();
	fusent_fop_pos_map = st_init_numtable();
	fusent_fop_sync_map = st_init_numtable();
}

// Destroys any persistant data structures at shut down.
void fusent_translate_teardown()
{
	st_free_table(fusent_fop_sync_map);
	st_free_table(fusent_fop_pos_map);
	st_free_table(fusent_fop_basename_map);
	st_free_table(fusent_fop_ino_map);
	st_free_table(fusent_fop_fi_map);
	iconv_close(cd_utf16le_to_utf8);
}

static inline size_t max_sz(size_t a, size_t b)
{
	if (a < b) return b;
	return a;
}

// Returns 1 if sync, 0 if async, -1 on lookup fail:
static inline int fusent_is_sync(FILE_OBJECT *fop)
{
	st_data_t issync;
	if (!st_lookup(fusent_fop_sync_map, (st_data_t)fop, &issync))
		return -1;

	return (int)issync;
}

// Given a LARGE_INTEGER offset from an IO_STACK_LOCATION and the current file position,
// figure out where a read or write should be performed:
//
// Hack:
#ifndef FILE_USE_FILE_POINTER_POSITION
# define FILE_USE_FILE_POINTER_POSITION (-2)
#endif
static inline uint64_t fusent_readwrite_offset(FILE_OBJECT *fop, uint64_t curoff, LARGE_INTEGER off)
{
	int issync = fusent_is_sync(fop);

	if (issync < 0)
		fprintf(stderr, "Err: couldn't find sync flag for fop: %p\n", fop);

	// This is how I interpret http://msdn.microsoft.com/en-us/library/ff549327.aspx --cemeyer:
	if ((issync == 1) && (
				(off.LowPart == FILE_USE_FILE_POINTER_POSITION && off.HighPart == -1) ||
				!off.QuadPart))
		return curoff;

	if (off.QuadPart < 0)
		fprintf(stderr, "Err: Got negative offset? %lld\n", off.QuadPart);

	return (uint64_t)off.QuadPart; // w32 uses an i64, fuse wants u64
}

// Add the fh <-> fop mapping to our maps:
static inline void fusent_add_fop_mapping(PFILE_OBJECT fop, struct fuse_file_info *fi, fuse_ino_t ino, char *basename, int issync)
{
	st_insert(fusent_fop_fi_map, (st_data_t)fop, (st_data_t)fi);
	st_insert(fusent_fop_ino_map, (st_data_t)fop, (st_data_t)ino);
	uint64_t *fpos = calloc(1, sizeof(uint64_t));
	*fpos = 0;
	st_insert(fusent_fop_pos_map, (st_data_t)fop, (st_data_t)fpos);
	uintptr_t sync = issync;
	st_insert(fusent_fop_sync_map, (st_data_t)fop, (st_data_t)sync);

	// Transcode the basename into native Windows WCHARs:
	size_t bnlen = strlen(basename);
	WCHAR *wcbasename = malloc(sizeof(WCHAR) * (bnlen + 1));
	fusent_transcode(basename, bnlen, wcbasename, sizeof(WCHAR) * bnlen, "UTF-8", "UTF-16LE");
	wcbasename[bnlen] = L'\0';

	st_insert(fusent_fop_basename_map, (st_data_t)fop, (st_data_t)wcbasename);

	fprintf(stderr, "Added fop mapping: %p -> %p, %lu, `%s'\n", fop, fi, ino, basename);
}

// Lookup the corresponding file_info pointer for an open file handle (fop).
// Negative on error, zero on success.
static inline int fusent_fi_inode_basename_from_fop(PFILE_OBJECT fop, struct fuse_file_info **fi_out, fuse_ino_t *ino_out, WCHAR **bn_out)
{
	int res;
	st_data_t rfi, rino, rbn;

	res = st_lookup(fusent_fop_fi_map, (st_data_t)fop, &rfi) - 1;
	if (!res) *fi_out = (struct fuse_file_info *)rfi;
	else return res;

	res = st_lookup(fusent_fop_ino_map, (st_data_t)fop, &rino) - 1;
	if (!res) *ino_out = (fuse_ino_t)rino;
	else return res;

	res = st_lookup(fusent_fop_basename_map, (st_data_t)fop, &rbn) - 1;
	if (!res) *bn_out = (WCHAR *)rbn;
	return res;
}

// Removes a fop mapping
static inline void fusent_remove_fop_mapping(PFILE_OBJECT fop)
{
	struct fuse_file_info *rfi;
	fuse_ino_t ino;
	WCHAR *bn;
	if (fusent_fi_inode_basename_from_fop(fop, &rfi, &ino, &bn) < 0) return;

	free(rfi);
	free(bn);

	st_data_t irfop = (st_data_t)fop;
	st_delete(fusent_fop_fi_map, &irfop, NULL);

	irfop = (st_data_t)fop;
	st_delete(fusent_fop_ino_map, &irfop, NULL);

	irfop = (st_data_t)fop;
	st_delete(fusent_fop_basename_map, &irfop, NULL);

	irfop = (st_data_t)fop;
	st_data_t pos;
	if (st_delete(fusent_fop_pos_map, &irfop, &pos)) {
		if (pos) free((uint64_t *)pos);
	}

	irfop = (st_data_t)fop;
	st_delete(fusent_fop_sync_map, &irfop, NULL);
}

// Translates a unix mode_t to windows' FileAttributes ULONG
static void fusent_unixmode_to_winattr(mode_t m, ULONG *winattr)
{
	ULONG val = 0;
	if (S_ISBLK(m) || S_ISCHR(m))
		val |= FILE_ATTRIBUTE_DEVICE;
	if (!(m & S_IWUSR || m & S_IWGRP || m & S_IWOTH))
		val |= FILE_ATTRIBUTE_READONLY;
	// I'm not sure we should claim a link is a reparse point:
	//if (S_ISLNK(m))
	//	val |= FILE_ATTRIBUTE_REPARSE_POINT;
	if (S_ISREG(m))
		val |= FILE_ATTRIBUTE_NORMAL;
	else
		val |= FILE_ATTRIBUTE_SYSTEM;

	*winattr = val;
}
#endif /* __CYGWIN__ */

struct fuse_pollhandle {
	uint64_t kh;
	struct fuse_chan *ch;
	struct fuse_ll *f;
};

static void convert_stat(const struct stat *stbuf, struct fuse_attr *attr)
{
	attr->ino	= stbuf->st_ino;
	attr->mode	= stbuf->st_mode;
	attr->nlink	= stbuf->st_nlink;
	attr->uid	= stbuf->st_uid;
	attr->gid	= stbuf->st_gid;
	attr->rdev	= stbuf->st_rdev;
	attr->size	= stbuf->st_size;
	attr->blksize	= stbuf->st_blksize;
	attr->blocks	= stbuf->st_blocks;
	attr->atime	= stbuf->st_atime;
	attr->mtime	= stbuf->st_mtime;
	attr->ctime	= stbuf->st_ctime;
	attr->atimensec = ST_ATIM_NSEC(stbuf);
	attr->mtimensec = ST_MTIM_NSEC(stbuf);
	attr->ctimensec = ST_CTIM_NSEC(stbuf);
}

static void convert_attr(const struct fuse_setattr_in *attr, struct stat *stbuf)
{
	stbuf->st_mode	       = attr->mode;
	stbuf->st_uid	       = attr->uid;
	stbuf->st_gid	       = attr->gid;
	stbuf->st_size	       = attr->size;
	stbuf->st_atime	       = attr->atime;
	stbuf->st_mtime	       = attr->mtime;
	ST_ATIM_NSEC_SET(stbuf, attr->atimensec);
	ST_MTIM_NSEC_SET(stbuf, attr->mtimensec);
}

static inline size_t iov_length(const struct iovec *iov, size_t count)
{
	size_t seg;
	size_t ret = 0;

	for (seg = 0; seg < count; seg++)
		ret += iov[seg].iov_len;
	return ret;
}

static void list_init_req(struct fuse_req *req)
{
	req->next = req;
	req->prev = req;
}

static void list_del_req(struct fuse_req *req)
{
	struct fuse_req *prev = req->prev;
	struct fuse_req *next = req->next;
	prev->next = next;
	next->prev = prev;
}

static void list_add_req(struct fuse_req *req, struct fuse_req *next)
{
	struct fuse_req *prev = next->prev;
	req->next = next;
	req->prev = prev;
	prev->next = req;
	next->prev = req;
}

static void destroy_req(fuse_req_t req)
{
	pthread_mutex_destroy(&req->lock);
	free(req);
}

void fuse_free_req(fuse_req_t req)
{
	int ctr;
	struct fuse_ll *f = req->f;

	pthread_mutex_lock(&f->lock);
	req->u.ni.func = NULL;
	req->u.ni.data = NULL;
	list_del_req(req);
	ctr = --req->ctr;
	pthread_mutex_unlock(&f->lock);
	if (!ctr)
		destroy_req(req);
}

int fuse_send_reply_iov_nofree(fuse_req_t req, int error, struct iovec *iov,
			       int count)
{
	struct fuse_out_header out;

	if (error <= -1000 || error > 0) {
		fprintf(stderr, "fuse: bad error value: %i\n",	error);
		error = -ERANGE;
	}

	out.unique = req->unique;
	out.error = error;

	iov[0].iov_base = &out;
	iov[0].iov_len = sizeof(struct fuse_out_header);
	out.len = iov_length(iov, count);

	if (req->f->debug) {
		if (out.error) {
			fprintf(stderr,
				"   unique: %llu, error: %i (%s), outsize: %i\n",
				(unsigned long long) out.unique, out.error,
				strerror(-out.error), out.len);
		} else {
			fprintf(stderr,
				"   unique: %llu, success, outsize: %i\n",
				(unsigned long long) out.unique, out.len);
		}
	}

#ifdef __CYGWIN__
	if (req->response_hijack) {
		*req->response_hijack = out;
		if (req->response_hijack_buf && count > 1) {
			size_t len = iov[1].iov_len;
			printf("reply_iov_nofree copying 0x%.8x bytes from %p to %p\n", len, iov[1].iov_base, req->response_hijack_buf);
			// Ensure that buf is large enough to hold iov (copy as much as we can):
			if (len > req->response_hijack_buflen) len = req->response_hijack_buflen;
			
			memcpy(req->response_hijack_buf, iov[1].iov_base, len);

			// Report a possible short write:
			req->response_hijack_buflen = iov[1].iov_len;
		}
		return 0;
	}
#endif

	return fuse_chan_send(req->ch, iov, count);
}

static int send_reply_iov(fuse_req_t req, int error, struct iovec *iov,
			  int count)
{
	int res;

	res = fuse_send_reply_iov_nofree(req, error, iov, count);
	// fuse_free_req(req);
	return res;
}

static int send_reply(fuse_req_t req, int error, const void *arg,
		      size_t argsize)
{
	struct iovec iov[2];
	int count = 1;
	if (argsize) {
		iov[1].iov_base = (void *) arg;
		iov[1].iov_len = argsize;
		count++;
	}
	return send_reply_iov(req, error, iov, count);
}

int fuse_reply_iov(fuse_req_t req, const struct iovec *iov, int count)
{
	int res;
	struct iovec *padded_iov;

	padded_iov = malloc((count + 1) * sizeof(struct iovec));
	if (padded_iov == NULL)
		return fuse_reply_err(req, -ENOMEM);

	memcpy(padded_iov + 1, iov, count * sizeof(struct iovec));
	count++;

	res = send_reply_iov(req, 0, padded_iov, count);
	free(padded_iov);

	return res;
}

size_t fuse_dirent_size(size_t namelen)
{
	return FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + namelen);
}

char *fuse_add_dirent(char *buf, const char *name, const struct stat *stbuf,
		      off_t off)
{
	unsigned namelen = strlen(name);
	unsigned entlen = FUSE_NAME_OFFSET + namelen;
	unsigned entsize = fuse_dirent_size(namelen);
	unsigned padlen = entsize - entlen;
	struct fuse_dirent *dirent = (struct fuse_dirent *) buf;

	dirent->ino = stbuf->st_ino;
	dirent->off = off;
	dirent->namelen = namelen;
	dirent->type = (stbuf->st_mode & 0170000) >> 12;
	strncpy(dirent->name, name, namelen);
	if (padlen)
		memset(buf + entlen, 0, padlen);

	return buf + entsize;
}

size_t fuse_add_direntry(fuse_req_t req, char *buf, size_t bufsize,
			 const char *name, const struct stat *stbuf, off_t off)
{
	size_t entsize;

	(void) req;
	entsize = fuse_dirent_size(strlen(name));
	if (entsize <= bufsize && buf)
		fuse_add_dirent(buf, name, stbuf, off);
	return entsize;
}

static void convert_statfs(const struct statvfs *stbuf,
			   struct fuse_kstatfs *kstatfs)
{
	kstatfs->bsize	 = stbuf->f_bsize;
	kstatfs->frsize	 = stbuf->f_frsize;
	kstatfs->blocks	 = stbuf->f_blocks;
	kstatfs->bfree	 = stbuf->f_bfree;
	kstatfs->bavail	 = stbuf->f_bavail;
	kstatfs->files	 = stbuf->f_files;
	kstatfs->ffree	 = stbuf->f_ffree;
	kstatfs->namelen = stbuf->f_namemax;
}

static int send_reply_ok(fuse_req_t req, const void *arg, size_t argsize)
{
	return send_reply(req, 0, arg, argsize);
}

int fuse_reply_err(fuse_req_t req, int err)
{
	return send_reply(req, -err, NULL, 0);
}

void fuse_reply_none(fuse_req_t req)
{

#ifdef __CYGWIN__
	if (req->response_hijack)
		memset(req->response_hijack, 0, sizeof(struct fuse_out_header));
	else
#endif
		fuse_chan_send(req->ch, NULL, 0);
	fuse_free_req(req);
}

static unsigned long calc_timeout_sec(double t)
{
	if (t > (double) ULONG_MAX)
		return ULONG_MAX;
	else if (t < 0.0)
		return 0;
	else
		return (unsigned long) t;
}

static unsigned int calc_timeout_nsec(double t)
{
	double f = t - (double) calc_timeout_sec(t);
	if (f < 0.0)
		return 0;
	else if (f >= 0.999999999)
		return 999999999;
	else
		return (unsigned int) (f * 1.0e9);
}

static void fill_entry(struct fuse_entry_out *arg,
		       const struct fuse_entry_param *e)
{
	arg->nodeid = e->ino;
	arg->generation = e->generation;
	arg->entry_valid = calc_timeout_sec(e->entry_timeout);
	arg->entry_valid_nsec = calc_timeout_nsec(e->entry_timeout);
	arg->attr_valid = calc_timeout_sec(e->attr_timeout);
	arg->attr_valid_nsec = calc_timeout_nsec(e->attr_timeout);
	convert_stat(&e->attr, &arg->attr);
}

static void fill_open(struct fuse_open_out *arg,
		      const struct fuse_file_info *f)
{
	arg->fh = f->fh;
	if (f->direct_io)
		arg->open_flags |= FOPEN_DIRECT_IO;
	if (f->keep_cache)
		arg->open_flags |= FOPEN_KEEP_CACHE;
	if (f->nonseekable)
		arg->open_flags |= FOPEN_NONSEEKABLE;
}

int fuse_reply_entry(fuse_req_t req, const struct fuse_entry_param *e)
{
	struct fuse_entry_out arg;
	size_t size = req->f->conn.proto_minor < 9 ?
		FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(arg);

	/* before ABI 7.4 e->ino == 0 was invalid, only ENOENT meant
	   negative entry */
	if (!e->ino && req->f->conn.proto_minor < 4)
		return fuse_reply_err(req, ENOENT);

	memset(&arg, 0, sizeof(arg));
	fill_entry(&arg, e);
	return send_reply_ok(req, &arg, size);
}

int fuse_reply_create(fuse_req_t req, const struct fuse_entry_param *e,
		      const struct fuse_file_info *f)
{
	char buf[sizeof(struct fuse_entry_out) + sizeof(struct fuse_open_out)];
	size_t entrysize = req->f->conn.proto_minor < 9 ?
		FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(struct fuse_entry_out);
	struct fuse_entry_out *earg = (struct fuse_entry_out *) buf;
	struct fuse_open_out *oarg = (struct fuse_open_out *) (buf + entrysize);

	memset(buf, 0, sizeof(buf));
	fill_entry(earg, e);
	fill_open(oarg, f);
	return send_reply_ok(req, buf,
			     entrysize + sizeof(struct fuse_open_out));
}

int fuse_reply_attr(fuse_req_t req, const struct stat *attr,
		    double attr_timeout)
{
	struct fuse_attr_out arg;
	size_t size = req->f->conn.proto_minor < 9 ?
		FUSE_COMPAT_ATTR_OUT_SIZE : sizeof(arg);

	memset(&arg, 0, sizeof(arg));
	arg.attr_valid = calc_timeout_sec(attr_timeout);
	arg.attr_valid_nsec = calc_timeout_nsec(attr_timeout);
	convert_stat(attr, &arg.attr);

	return send_reply_ok(req, &arg, size);
}

int fuse_reply_readlink(fuse_req_t req, const char *linkname)
{
	return send_reply_ok(req, linkname, strlen(linkname));
}

int fuse_reply_open(fuse_req_t req, const struct fuse_file_info *f)
{
	struct fuse_open_out arg;

	memset(&arg, 0, sizeof(arg));
	fill_open(&arg, f);
	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_write(fuse_req_t req, size_t count)
{
	struct fuse_write_out arg;

	memset(&arg, 0, sizeof(arg));
	arg.size = count;

	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_buf(fuse_req_t req, const char *buf, size_t size)
{
	return send_reply_ok(req, buf, size);
}

int fuse_reply_statfs(fuse_req_t req, const struct statvfs *stbuf)
{
	struct fuse_statfs_out arg;
	size_t size = req->f->conn.proto_minor < 4 ?
		FUSE_COMPAT_STATFS_SIZE : sizeof(arg);

	memset(&arg, 0, sizeof(arg));
	convert_statfs(stbuf, &arg.st);

	return send_reply_ok(req, &arg, size);
}

int fuse_reply_xattr(fuse_req_t req, size_t count)
{
	struct fuse_getxattr_out arg;

	memset(&arg, 0, sizeof(arg));
	arg.size = count;

	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_lock(fuse_req_t req, struct flock *lock)
{
	struct fuse_lk_out arg;

	memset(&arg, 0, sizeof(arg));
	arg.lk.type = lock->l_type;
	if (lock->l_type != F_UNLCK) {
		arg.lk.start = lock->l_start;
		if (lock->l_len == 0)
			arg.lk.end = OFFSET_MAX;
		else
			arg.lk.end = lock->l_start + lock->l_len - 1;
	}
	arg.lk.pid = lock->l_pid;
	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_bmap(fuse_req_t req, uint64_t idx)
{
	struct fuse_bmap_out arg;

	memset(&arg, 0, sizeof(arg));
	arg.block = idx;

	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_ioctl_retry(fuse_req_t req,
			   const struct iovec *in_iov, size_t in_count,
			   const struct iovec *out_iov, size_t out_count)
{
	struct fuse_ioctl_out arg;
	struct iovec iov[4];
	size_t count = 1;

	memset(&arg, 0, sizeof(arg));
	arg.flags |= FUSE_IOCTL_RETRY;
	arg.in_iovs = in_count;
	arg.out_iovs = out_count;
	iov[count].iov_base = &arg;
	iov[count].iov_len = sizeof(arg);
	count++;

	if (in_count) {
		iov[count].iov_base = (void *)in_iov;
		iov[count].iov_len = sizeof(in_iov[0]) * in_count;
		count++;
	}

	if (out_count) {
		iov[count].iov_base = (void *)out_iov;
		iov[count].iov_len = sizeof(out_iov[0]) * out_count;
		count++;
	}

	return send_reply_iov(req, 0, iov, count);
}

int fuse_reply_ioctl(fuse_req_t req, int result, const void *buf, size_t size)
{
	struct fuse_ioctl_out arg;
	struct iovec iov[3];
	size_t count = 1;

	memset(&arg, 0, sizeof(arg));
	arg.result = result;
	iov[count].iov_base = &arg;
	iov[count].iov_len = sizeof(arg);
	count++;

	if (size) {
		iov[count].iov_base = (char *) buf;
		iov[count].iov_len = size;
		count++;
	}

	return send_reply_iov(req, 0, iov, count);
}

int fuse_reply_ioctl_iov(fuse_req_t req, int result, const struct iovec *iov,
			 int count)
{
	struct iovec *padded_iov;
	struct fuse_ioctl_out arg;
	int res;

	padded_iov = malloc((count + 2) * sizeof(struct iovec));
	if (padded_iov == NULL)
		return fuse_reply_err(req, -ENOMEM);

	memset(&arg, 0, sizeof(arg));
	arg.result = result;
	padded_iov[1].iov_base = &arg;
	padded_iov[1].iov_len = sizeof(arg);

	memcpy(&padded_iov[2], iov, count * sizeof(struct iovec));

	res = send_reply_iov(req, 0, padded_iov, count + 2);
	free(padded_iov);

	return res;
}

int fuse_reply_poll(fuse_req_t req, unsigned revents)
{
	struct fuse_poll_out arg;

	memset(&arg, 0, sizeof(arg));
	arg.revents = revents;

	return send_reply_ok(req, &arg, sizeof(arg));
}

static void do_lookup(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	char *name = (char *) inarg;

	if (req->f->op.lookup)
		req->f->op.lookup(req, nodeid, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_forget(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_forget_in *arg = (struct fuse_forget_in *) inarg;

	if (req->f->op.forget)
		req->f->op.forget(req, nodeid, arg->nlookup);
	else
		fuse_reply_none(req);
}

static void do_getattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_file_info *fip = NULL;
	struct fuse_file_info fi;

	if (req->f->conn.proto_minor >= 9) {
		struct fuse_getattr_in *arg = (struct fuse_getattr_in *) inarg;

		if (arg->getattr_flags & FUSE_GETATTR_FH) {
			memset(&fi, 0, sizeof(fi));
			fi.fh = arg->fh;
			fi.fh_old = fi.fh;
			fip = &fi;
		}
	}

	if (req->f->op.getattr)
		req->f->op.getattr(req, nodeid, fip);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_setattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_setattr_in *arg = (struct fuse_setattr_in *) inarg;

	if (req->f->op.setattr) {
		struct fuse_file_info *fi = NULL;
		struct fuse_file_info fi_store;
		struct stat stbuf;
		memset(&stbuf, 0, sizeof(stbuf));
		convert_attr(arg, &stbuf);
		if (arg->valid & FATTR_FH) {
			arg->valid &= ~FATTR_FH;
			memset(&fi_store, 0, sizeof(fi_store));
			fi = &fi_store;
			fi->fh = arg->fh;
			fi->fh_old = fi->fh;
		}
		arg->valid &=
			FUSE_SET_ATTR_MODE	|
			FUSE_SET_ATTR_UID	|
			FUSE_SET_ATTR_GID	|
			FUSE_SET_ATTR_SIZE	|
			FUSE_SET_ATTR_ATIME	|
			FUSE_SET_ATTR_MTIME	|
			FUSE_SET_ATTR_ATIME_NOW	|
			FUSE_SET_ATTR_MTIME_NOW;

		req->f->op.setattr(req, nodeid, &stbuf, arg->valid, fi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void do_access(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_access_in *arg = (struct fuse_access_in *) inarg;

	if (req->f->op.access)
		req->f->op.access(req, nodeid, arg->mask);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_readlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	(void) inarg;

	if (req->f->op.readlink)
		req->f->op.readlink(req, nodeid);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_mknod(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_mknod_in *arg = (struct fuse_mknod_in *) inarg;
	char *name = PARAM(arg);

	if (req->f->conn.proto_minor >= 12)
		req->ctx.umask = arg->umask;
	else
		name = (char *) inarg + FUSE_COMPAT_MKNOD_IN_SIZE;

	if (req->f->op.mknod)
		req->f->op.mknod(req, nodeid, name, arg->mode, arg->rdev);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_mkdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_mkdir_in *arg = (struct fuse_mkdir_in *) inarg;

	if (req->f->conn.proto_minor >= 12)
		req->ctx.umask = arg->umask;

	if (req->f->op.mkdir)
		req->f->op.mkdir(req, nodeid, PARAM(arg), arg->mode);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_unlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	char *name = (char *) inarg;

	if (req->f->op.unlink)
		req->f->op.unlink(req, nodeid, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_rmdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	char *name = (char *) inarg;

	if (req->f->op.rmdir)
		req->f->op.rmdir(req, nodeid, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_symlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	char *name = (char *) inarg;
	char *linkname = ((char *) inarg) + strlen((char *) inarg) + 1;

	if (req->f->op.symlink)
		req->f->op.symlink(req, linkname, nodeid, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_rename(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_rename_in *arg = (struct fuse_rename_in *) inarg;
	char *oldname = PARAM(arg);
	char *newname = oldname + strlen(oldname) + 1;

	if (req->f->op.rename)
		req->f->op.rename(req, nodeid, oldname, arg->newdir, newname);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_link(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_link_in *arg = (struct fuse_link_in *) inarg;

	if (req->f->op.link)
		req->f->op.link(req, arg->oldnodeid, nodeid, PARAM(arg));
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_create(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_create_in *arg = (struct fuse_create_in *) inarg;

	if (req->f->op.create) {
		struct fuse_file_info fi;
		char *name = PARAM(arg);

		memset(&fi, 0, sizeof(fi));
		fi.flags = arg->flags;

		if (req->f->conn.proto_minor >= 12)
			req->ctx.umask = arg->umask;
		else
			name = (char *) inarg + sizeof(struct fuse_open_in);

		req->f->op.create(req, nodeid, name, arg->mode, &fi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void do_open(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_open_in *arg = (struct fuse_open_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.flags = arg->flags;

	if (req->f->op.open)
		req->f->op.open(req, nodeid, &fi);
	else
		fuse_reply_open(req, &fi);
}

static void do_read(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_read_in *arg = (struct fuse_read_in *) inarg;

	if (req->f->op.read) {
		struct fuse_file_info fi;

		memset(&fi, 0, sizeof(fi));
		fi.fh = arg->fh;
		fi.fh_old = fi.fh;
		if (req->f->conn.proto_minor >= 9) {
			fi.lock_owner = arg->lock_owner;
			fi.flags = arg->flags;
		}
		req->f->op.read(req, nodeid, arg->size, arg->offset, &fi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void do_write(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_write_in *arg = (struct fuse_write_in *) inarg;
	struct fuse_file_info fi;
	char *param;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;
	fi.writepage = arg->write_flags & 1;

	if (req->f->conn.proto_minor < 9) {
		param = ((char *) arg) + FUSE_COMPAT_WRITE_IN_SIZE;
	} else {
		fi.lock_owner = arg->lock_owner;
		fi.flags = arg->flags;
		param = PARAM(arg);
	}

#ifdef __CYGWIN__
	param = req->response_hijack_buf; // abusing the crap out of this, oops
#endif

	if (req->f->op.write)
		req->f->op.write(req, nodeid, param, arg->size,
				 arg->offset, &fi);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_flush(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_flush_in *arg = (struct fuse_flush_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;
	fi.flush = 1;
	if (req->f->conn.proto_minor >= 7)
		fi.lock_owner = arg->lock_owner;

	if (req->f->op.flush)
		req->f->op.flush(req, nodeid, &fi);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_release(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_release_in *arg = (struct fuse_release_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.flags = arg->flags;
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;
	if (req->f->conn.proto_minor >= 8) {
		fi.flush = (arg->release_flags & FUSE_RELEASE_FLUSH) ? 1 : 0;
		fi.lock_owner = arg->lock_owner;
	}

	if (req->f->op.release)
		req->f->op.release(req, nodeid, &fi);
	else
		fuse_reply_err(req, 0);
}

static void do_fsync(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_fsync_in *arg = (struct fuse_fsync_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.fsync)
		req->f->op.fsync(req, nodeid, arg->fsync_flags & 1, &fi);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_opendir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_open_in *arg = (struct fuse_open_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.flags = arg->flags;

	if (req->f->op.opendir)
		req->f->op.opendir(req, nodeid, &fi);
	else
		fuse_reply_open(req, &fi);
}

//
// debug hack
//
static void do_readdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_read_in *arg = (struct fuse_read_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.readdir) {
		printf("do_readdir dispatching to op.readdir()\n");
		printf("req:%p\n", req);
		printf("req->f:%p\n", req->f);
		//printf("req->f->op:%.8x\n", req->f->op);
		printf("req->f->op.readdir:%p\n", req->f->op.readdir);
		//printf("addr of fuse_lib_readdir():%.8x\n", fuse_lib_readdir);
		printf("arg: %p\n", arg);
		printf("arg->size: %.8x\n", arg->size);
		printf("arg->offset: %.16llx\n", arg->offset);
		printf("nodeid:%.8lx, fi:%p\n", nodeid, &fi);
		req->f->op.readdir(req, nodeid, arg->size, arg->offset, &fi);
	}
	else {
		printf("do_readdir calling fuse_reply_err()\n");
		fuse_reply_err(req, ENOSYS);
	}
}

static void do_releasedir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_release_in *arg = (struct fuse_release_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.flags = arg->flags;
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.releasedir)
		req->f->op.releasedir(req, nodeid, &fi);
	else
		fuse_reply_err(req, 0);
}

static void do_fsyncdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_fsync_in *arg = (struct fuse_fsync_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.fsyncdir)
		req->f->op.fsyncdir(req, nodeid, arg->fsync_flags & 1, &fi);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_statfs(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	(void) nodeid;
	(void) inarg;

	if (req->f->op.statfs)
		req->f->op.statfs(req, nodeid);
	else {
		struct statvfs buf = {
			.f_namemax = 255,
			.f_bsize = 512,
		};
		fuse_reply_statfs(req, &buf);
	}
}

static void do_setxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_setxattr_in *arg = (struct fuse_setxattr_in *) inarg;
	char *name = PARAM(arg);
	char *value = name + strlen(name) + 1;

	if (req->f->op.setxattr)
		req->f->op.setxattr(req, nodeid, name, value, arg->size,
				    arg->flags);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_getxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_getxattr_in *arg = (struct fuse_getxattr_in *) inarg;

	if (req->f->op.getxattr)
		req->f->op.getxattr(req, nodeid, PARAM(arg), arg->size);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_listxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_getxattr_in *arg = (struct fuse_getxattr_in *) inarg;

	if (req->f->op.listxattr)
		req->f->op.listxattr(req, nodeid, arg->size);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_removexattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	char *name = (char *) inarg;

	if (req->f->op.removexattr)
		req->f->op.removexattr(req, nodeid, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void convert_fuse_file_lock(struct fuse_file_lock *fl,
				   struct flock *flock)
{
	memset(flock, 0, sizeof(struct flock));
	flock->l_type = fl->type;
	flock->l_whence = SEEK_SET;
	flock->l_start = fl->start;
	if (fl->end == OFFSET_MAX)
		flock->l_len = 0;
	else
		flock->l_len = fl->end - fl->start + 1;
	flock->l_pid = fl->pid;
}

static void do_getlk(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_lk_in *arg = (struct fuse_lk_in *) inarg;
	struct fuse_file_info fi;
	struct flock flock;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.lock_owner = arg->owner;

	convert_fuse_file_lock(&arg->lk, &flock);
	if (req->f->op.getlk)
		req->f->op.getlk(req, nodeid, &fi, &flock);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_setlk_common(fuse_req_t req, fuse_ino_t nodeid,
			    const void *inarg, int sleep)
{
	struct fuse_lk_in *arg = (struct fuse_lk_in *) inarg;
	struct fuse_file_info fi;
	struct flock flock;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.lock_owner = arg->owner;

	convert_fuse_file_lock(&arg->lk, &flock);
	if (req->f->op.setlk)
		req->f->op.setlk(req, nodeid, &fi, &flock, sleep);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_setlk(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	do_setlk_common(req, nodeid, inarg, 0);
}

static void do_setlkw(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	do_setlk_common(req, nodeid, inarg, 1);
}

static int find_interrupted(struct fuse_ll *f, struct fuse_req *req)
{
	struct fuse_req *curr;

	for (curr = f->list.next; curr != &f->list; curr = curr->next) {
		if (curr->unique == req->u.i.unique) {
			fuse_interrupt_func_t func;
			void *data;

			curr->ctr++;
			pthread_mutex_unlock(&f->lock);

			/* Ugh, ugly locking */
			pthread_mutex_lock(&curr->lock);
			pthread_mutex_lock(&f->lock);
			curr->interrupted = 1;
			func = curr->u.ni.func;
			data = curr->u.ni.data;
			pthread_mutex_unlock(&f->lock);
			if (func)
				func(curr, data);
			pthread_mutex_unlock(&curr->lock);

			pthread_mutex_lock(&f->lock);
			curr->ctr--;
			if (!curr->ctr)
				destroy_req(curr);

			return 1;
		}
	}
	for (curr = f->interrupts.next; curr != &f->interrupts;
	     curr = curr->next) {
		if (curr->u.i.unique == req->u.i.unique)
			return 1;
	}
	return 0;
}

static void do_interrupt(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_interrupt_in *arg = (struct fuse_interrupt_in *) inarg;
	struct fuse_ll *f = req->f;

	(void) nodeid;
	if (f->debug)
		fprintf(stderr, "INTERRUPT: %llu\n",
			(unsigned long long) arg->unique);

	req->u.i.unique = arg->unique;

	pthread_mutex_lock(&f->lock);
	if (find_interrupted(f, req))
		destroy_req(req);
	else
		list_add_req(req, &f->interrupts);
	pthread_mutex_unlock(&f->lock);
}

// This is only used by a Unix-only portion of fuse_ll_process:
#ifndef __CYGWIN__
static struct fuse_req *check_interrupt(struct fuse_ll *f, struct fuse_req *req)
{
	struct fuse_req *curr;

	for (curr = f->interrupts.next; curr != &f->interrupts;
	     curr = curr->next) {
		if (curr->u.i.unique == req->unique) {
			req->interrupted = 1;
			list_del_req(curr);
			free(curr);
			return NULL;
		}
	}
	curr = f->interrupts.next;
	if (curr != &f->interrupts) {
		list_del_req(curr);
		list_init_req(curr);
		return curr;
	} else
		return NULL;
}
#endif /* __CYGWIN__ */

static void do_bmap(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_bmap_in *arg = (struct fuse_bmap_in *) inarg;

	if (req->f->op.bmap)
		req->f->op.bmap(req, nodeid, arg->blocksize, arg->block);
	else
		fuse_reply_err(req, ENOSYS);
}

static void do_ioctl(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_ioctl_in *arg = (struct fuse_ioctl_in *) inarg;
	unsigned int flags = arg->flags;
	void *in_buf = arg->in_size ? PARAM(arg) : NULL;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.ioctl)
		req->f->op.ioctl(req, nodeid, arg->cmd,
				 (void *)(uintptr_t)arg->arg, &fi, flags,
				 in_buf, arg->in_size, arg->out_size);
	else
		fuse_reply_err(req, ENOSYS);
}

void fuse_pollhandle_destroy(struct fuse_pollhandle *ph)
{
	free(ph);
}

static void do_poll(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_poll_in *arg = (struct fuse_poll_in *) inarg;
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.fh = arg->fh;
	fi.fh_old = fi.fh;

	if (req->f->op.poll) {
		struct fuse_pollhandle *ph = NULL;

		if (arg->flags & FUSE_POLL_SCHEDULE_NOTIFY) {
			ph = malloc(sizeof(struct fuse_pollhandle));
			if (ph == NULL) {
				fuse_reply_err(req, ENOMEM);
				return;
			}
			ph->kh = arg->kh;
			ph->ch = req->ch;
			ph->f = req->f;
		}

		req->f->op.poll(req, nodeid, &fi, ph);
	} else {
		fuse_reply_err(req, ENOSYS);
	}
}

static void do_init(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_init_in *arg = (struct fuse_init_in *) inarg;
	struct fuse_init_out outarg;
	struct fuse_ll *f = req->f;
	size_t bufsize = fuse_chan_bufsize(req->ch);

	(void) nodeid;
	if (f->debug) {
		fprintf(stderr, "INIT: %u.%u\n", arg->major, arg->minor);
		if (arg->major == 7 && arg->minor >= 6) {
			fprintf(stderr, "flags=0x%08x\n", arg->flags);
			fprintf(stderr, "max_readahead=0x%08x\n",
				arg->max_readahead);
		}
	}
	f->conn.proto_major = arg->major;
	f->conn.proto_minor = arg->minor;
	f->conn.capable = 0;
	f->conn.want = 0;

	memset(&outarg, 0, sizeof(outarg));
	outarg.major = FUSE_KERNEL_VERSION;
	outarg.minor = FUSE_KERNEL_MINOR_VERSION;

	if (arg->major < 7) {
		fprintf(stderr, "fuse: unsupported protocol version: %u.%u\n",
			arg->major, arg->minor);
		fuse_reply_err(req, EPROTO);
		return;
	}

	if (arg->major > 7) {
		/* Wait for a second INIT request with a 7.X version */
		send_reply_ok(req, &outarg, sizeof(outarg));
		return;
	}

	if (arg->minor >= 6) {
		if (f->conn.async_read)
			f->conn.async_read = arg->flags & FUSE_ASYNC_READ;
		if (arg->max_readahead < f->conn.max_readahead)
			f->conn.max_readahead = arg->max_readahead;
		if (arg->flags & FUSE_ASYNC_READ)
			f->conn.capable |= FUSE_CAP_ASYNC_READ;
		if (arg->flags & FUSE_POSIX_LOCKS)
			f->conn.capable |= FUSE_CAP_POSIX_LOCKS;
		if (arg->flags & FUSE_ATOMIC_O_TRUNC)
			f->conn.capable |= FUSE_CAP_ATOMIC_O_TRUNC;
		if (arg->flags & FUSE_EXPORT_SUPPORT)
			f->conn.capable |= FUSE_CAP_EXPORT_SUPPORT;
		if (arg->flags & FUSE_BIG_WRITES)
			f->conn.capable |= FUSE_CAP_BIG_WRITES;
		if (arg->flags & FUSE_DONT_MASK)
			f->conn.capable |= FUSE_CAP_DONT_MASK;
	} else {
		f->conn.async_read = 0;
		f->conn.max_readahead = 0;
	}

	if (f->atomic_o_trunc)
		f->conn.want |= FUSE_CAP_ATOMIC_O_TRUNC;
	if (f->op.getlk && f->op.setlk && !f->no_remote_lock)
		f->conn.want |= FUSE_CAP_POSIX_LOCKS;
	if (f->big_writes)
		f->conn.want |= FUSE_CAP_BIG_WRITES;

	if (bufsize < FUSE_MIN_READ_BUFFER) {
		fprintf(stderr, "fuse: warning: buffer size too small: %zu\n",
			bufsize);
		bufsize = FUSE_MIN_READ_BUFFER;
	}

	bufsize -= 4096;
	if (bufsize < f->conn.max_write)
		f->conn.max_write = bufsize;

	f->got_init = 1;
	if (f->op.init)
		f->op.init(f->userdata, &f->conn);

	if (f->conn.async_read || (f->conn.want & FUSE_CAP_ASYNC_READ))
		outarg.flags |= FUSE_ASYNC_READ;
	if (f->conn.want & FUSE_CAP_POSIX_LOCKS)
		outarg.flags |= FUSE_POSIX_LOCKS;
	if (f->conn.want & FUSE_CAP_ATOMIC_O_TRUNC)
		outarg.flags |= FUSE_ATOMIC_O_TRUNC;
	if (f->conn.want & FUSE_CAP_EXPORT_SUPPORT)
		outarg.flags |= FUSE_EXPORT_SUPPORT;
	if (f->conn.want & FUSE_CAP_BIG_WRITES)
		outarg.flags |= FUSE_BIG_WRITES;
	if (f->conn.want & FUSE_CAP_DONT_MASK)
		outarg.flags |= FUSE_DONT_MASK;
	outarg.max_readahead = f->conn.max_readahead;
	outarg.max_write = f->conn.max_write;

	if (f->debug) {
		fprintf(stderr, "   INIT: %u.%u\n", outarg.major, outarg.minor);
		fprintf(stderr, "   flags=0x%08x\n", outarg.flags);
		fprintf(stderr, "   max_readahead=0x%08x\n",
			outarg.max_readahead);
		fprintf(stderr, "   max_write=0x%08x\n", outarg.max_write);
	}

	send_reply_ok(req, &outarg, arg->minor < 5 ? 8 : sizeof(outarg));
}

static void do_destroy(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	struct fuse_ll *f = req->f;

	(void) nodeid;
	(void) inarg;

	f->got_destroy = 1;
	if (f->op.destroy)
		f->op.destroy(f->userdata);

	send_reply_ok(req, NULL, 0);
}

static int send_notify_iov(struct fuse_ll *f, struct fuse_chan *ch,
			   int notify_code, struct iovec *iov, int count)
{
	struct fuse_out_header out;

	out.unique = 0;
	out.error = notify_code;
	iov[0].iov_base = &out;
	iov[0].iov_len = sizeof(struct fuse_out_header);
	out.len = iov_length(iov, count);

	if (f->debug)
		fprintf(stderr, "NOTIFY: code=%d count=%d length=%u\n",
			notify_code, count, out.len);

	return fuse_chan_send(ch, iov, count);
}

int fuse_lowlevel_notify_poll(struct fuse_pollhandle *ph)
{
	if (ph != NULL) {
		struct fuse_notify_poll_wakeup_out outarg;
		struct iovec iov[2];

		outarg.kh = ph->kh;

		iov[1].iov_base = &outarg;
		iov[1].iov_len = sizeof(outarg);

		return send_notify_iov(ph->f, ph->ch, FUSE_NOTIFY_POLL, iov, 2);
	} else {
		return 0;
	}
}

int fuse_lowlevel_notify_inval_inode(struct fuse_chan *ch, fuse_ino_t ino,
                                     off_t off, off_t len)
{
	struct fuse_notify_inval_inode_out outarg;
	struct fuse_ll *f;
	struct iovec iov[2];

	if (!ch)
		return -EINVAL;

	f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
	if (!f)
		return -ENODEV;

	outarg.ino = ino;
	outarg.off = off;
	outarg.len = len;

	iov[1].iov_base = &outarg;
	iov[1].iov_len = sizeof(outarg);

	return send_notify_iov(f, ch, FUSE_NOTIFY_INVAL_INODE, iov, 2);
}

int fuse_lowlevel_notify_inval_entry(struct fuse_chan *ch, fuse_ino_t parent,
                                     const char *name, size_t namelen)
{
	struct fuse_notify_inval_entry_out outarg;
	struct fuse_ll *f;
	struct iovec iov[3];

	if (!ch)
		return -EINVAL;

	f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
	if (!f)
		return -ENODEV;

	outarg.parent = parent;
	outarg.namelen = namelen;
	outarg.padding = 0;

	iov[1].iov_base = &outarg;
	iov[1].iov_len = sizeof(outarg);
	iov[2].iov_base = (void *)name;
	iov[2].iov_len = namelen + 1;

	return send_notify_iov(f, ch, FUSE_NOTIFY_INVAL_ENTRY, iov, 3);
}

void *fuse_req_userdata(fuse_req_t req)
{
	return req->f->userdata;
}

const struct fuse_ctx *fuse_req_ctx(fuse_req_t req)
{
	return &req->ctx;
}

/*
 * The size of fuse_ctx got extended, so need to be careful about
 * incompatibility (i.e. a new binary cannot work with an old
 * library).
 */
const struct fuse_ctx *fuse_req_ctx_compat24(fuse_req_t req);
const struct fuse_ctx *fuse_req_ctx_compat24(fuse_req_t req)
{
	return fuse_req_ctx(req);
}
FUSE_SYMVER(".symver fuse_req_ctx_compat24,fuse_req_ctx@FUSE_2.4");


void fuse_req_interrupt_func(fuse_req_t req, fuse_interrupt_func_t func,
			     void *data)
{
	pthread_mutex_lock(&req->lock);
	pthread_mutex_lock(&req->f->lock);
	req->u.ni.func = func;
	req->u.ni.data = data;
	pthread_mutex_unlock(&req->f->lock);
	if (req->interrupted && func)
		func(req, data);
	pthread_mutex_unlock(&req->lock);
}

int fuse_req_interrupted(fuse_req_t req)
{
	int interrupted;

	pthread_mutex_lock(&req->f->lock);
	interrupted = req->interrupted;
	pthread_mutex_unlock(&req->f->lock);

	return interrupted;
}

static struct {
	void (*func)(fuse_req_t, fuse_ino_t, const void *);
	const char *name;
} fuse_ll_ops[] = {
	[FUSE_LOOKUP]	   = { do_lookup,      "LOOKUP"	     },
	[FUSE_FORGET]	   = { do_forget,      "FORGET"	     },
	[FUSE_GETATTR]	   = { do_getattr,     "GETATTR"     },
	[FUSE_SETATTR]	   = { do_setattr,     "SETATTR"     },
	[FUSE_READLINK]	   = { do_readlink,    "READLINK"    },
	[FUSE_SYMLINK]	   = { do_symlink,     "SYMLINK"     },
	[FUSE_MKNOD]	   = { do_mknod,       "MKNOD"	     },
	[FUSE_MKDIR]	   = { do_mkdir,       "MKDIR"	     },
	[FUSE_UNLINK]	   = { do_unlink,      "UNLINK"	     },
	[FUSE_RMDIR]	   = { do_rmdir,       "RMDIR"	     },
	[FUSE_RENAME]	   = { do_rename,      "RENAME"	     },
	[FUSE_LINK]	   = { do_link,	       "LINK"	     },
	[FUSE_OPEN]	   = { do_open,	       "OPEN"	     },
	[FUSE_READ]	   = { do_read,	       "READ"	     },
	[FUSE_WRITE]	   = { do_write,       "WRITE"	     },
	[FUSE_STATFS]	   = { do_statfs,      "STATFS"	     },
	[FUSE_RELEASE]	   = { do_release,     "RELEASE"     },
	[FUSE_FSYNC]	   = { do_fsync,       "FSYNC"	     },
	[FUSE_SETXATTR]	   = { do_setxattr,    "SETXATTR"    },
	[FUSE_GETXATTR]	   = { do_getxattr,    "GETXATTR"    },
	[FUSE_LISTXATTR]   = { do_listxattr,   "LISTXATTR"   },
	[FUSE_REMOVEXATTR] = { do_removexattr, "REMOVEXATTR" },
	[FUSE_FLUSH]	   = { do_flush,       "FLUSH"	     },
	[FUSE_INIT]	   = { do_init,	       "INIT"	     },
	[FUSE_OPENDIR]	   = { do_opendir,     "OPENDIR"     },
	[FUSE_READDIR]	   = { do_readdir,     "READDIR"     },
	[FUSE_RELEASEDIR]  = { do_releasedir,  "RELEASEDIR"  },
	[FUSE_FSYNCDIR]	   = { do_fsyncdir,    "FSYNCDIR"    },
	[FUSE_GETLK]	   = { do_getlk,       "GETLK"	     },
	[FUSE_SETLK]	   = { do_setlk,       "SETLK"	     },
	[FUSE_SETLKW]	   = { do_setlkw,      "SETLKW"	     },
	[FUSE_ACCESS]	   = { do_access,      "ACCESS"	     },
	[FUSE_CREATE]	   = { do_create,      "CREATE"	     },
	[FUSE_INTERRUPT]   = { do_interrupt,   "INTERRUPT"   },
	[FUSE_BMAP]	   = { do_bmap,	       "BMAP"	     },
	[FUSE_IOCTL]	   = { do_ioctl,       "IOCTL"	     },
	[FUSE_POLL]	   = { do_poll,        "POLL"	     },
	[FUSE_DESTROY]	   = { do_destroy,     "DESTROY"     },
#if defined __CYGWIN__
	[CUSE_INIT]	   = { NULL,           "CUSE_INIT"   },
#else
	[CUSE_INIT]	   = { cuse_lowlevel_init, "CUSE_INIT"   },
#endif
};

#define FUSE_MAXOP (sizeof(fuse_ll_ops) / sizeof(fuse_ll_ops[0]))

// This is only used by a Unix-only part of fuse_ll_process:
#ifndef __CYGWIN__
static const char *opname(enum fuse_opcode opcode)
{
	if (opcode >= FUSE_MAXOP || !fuse_ll_ops[opcode].name)
		return "???";
	else
		return fuse_ll_ops[opcode].name;
}
#endif

#ifdef __CYGWIN__
// Does an in-place conversion of a Windows-formatted buffer to
// Unix-formatted. Probably buggy.
static void fusent_convert_win_path(char *buf, size_t *len) {
	size_t i;
	// For now:
	//   1) Replace backslashes with slashes
	for (i = 0; i < *len; i++) {
		if (buf[i] == '\\') buf[i] = '/';
	}

	//   2) Strip any beginning drive
	if (*len > 2 && isalpha((int)buf[0]) && buf[1] == ':') {
		*len = *len - 2;
		memmove(buf, &buf[2], *len);
	}
}

// Get the current read/write offset for the given file object
static uint64_t fusent_get_file_offset(PFILE_OBJECT fop)
{
	st_data_t poffset;

	if (st_lookup(fusent_fop_pos_map, (st_data_t)fop, &poffset))
		return *((uint64_t *) poffset);
	else
		return 0;
}

// Sets the current file offset to the given offset
static int fusent_set_file_offset(PFILE_OBJECT fop, uint64_t offset) {
	int res;
	st_data_t data_offset;

	res = st_lookup(fusent_fop_pos_map, (st_data_t)fop, &data_offset) - 1;
	if (!res)
		*((uint64_t *) data_offset) = offset;

	return res;
}

// Given a path in `fn' (assume unix format), find the inode of the parent.
//   e.g. /sbin/route -> inode of /sbin
// Additionally, locate the offset of the basename in the buffer and
// return a pointer to it.
//
// Returns negative if the parent can't be found or the path is invalid:
//      stdint.h -> err
static int fusent_get_parent_inode(fuse_req_t req, char *fn, char **bn, fuse_ino_t *in) {
	if (*fn != '/') return -1;
	if (!req->f->op.lookup) return -1;
	fn ++;

	const size_t buflen = sizeof(struct fuse_entry_out);

	fuse_ino_t curino = FUSE_ROOT_ID;
	char *hijackbuf = malloc(buflen);

	for (;;) {
		// This is totally fine in UTF-8, by the way:
		char *nextsl = strchr(fn, '/');

		// If there are no more slashes, we've located the basename and
		// parent inode number already:
		if (!nextsl) {
			*bn = fn;
			*in = curino;
			free(hijackbuf);
			return 0;
		}

		// Otherwise, lookup the next component of the path:
		*nextsl = '\0';
		struct fuse_out_header out;
		req->response_hijack = &out;
		req->response_hijack_buf = hijackbuf;
		req->response_hijack_buflen = buflen;

		fuse_ll_ops[FUSE_LOOKUP].func(req, curino, fn);

		req->response_hijack = NULL;
		req->response_hijack_buf = NULL;
		*nextsl = '/';

		if (out.error) {
			free(hijackbuf);
			return out.error;
		}

		struct fuse_entry_out *lookuparg = (struct fuse_entry_out *)hijackbuf;

		curino = lookuparg->nodeid;
		fn = nextsl + 1;
	}
}

// Given a path in `fn', locate the inode for `fn'.
// Returns negative if `fn' can't be found.
static int fusent_get_inode(fuse_req_t req, char *fn, fuse_ino_t *in, char **bn)
{
	char path[FUSENT_MAX_PATH + 3];
	size_t sl = strlen(fn);
	memcpy(path, fn, sl);

	// As a giant hack, fusenet_get_parent_inode doesn't care if the last
	// "parent" component is a directory, so let's just abuse that to
	// get the inode for now:
	path[sl]   = '/';
	path[sl+1] = 'a';
	path[sl+2] = '\0';

	char *bn_t;
	int res = fusent_get_parent_inode(req, path, &bn_t, in);
	if (bn && res >= 0) {
		*bn = fn + (bn_t - path);
	}
	return res;
}

static inline NTSTATUS fusent_translate_errno(int err)
{
	if (err >= 0) return STATUS_SUCCESS;

	switch (err) {
		case EACCES: return STATUS_ACCESS_DENIED;
		case EBADF: return STATUS_INVALID_HANDLE;
		case ENOENT: return STATUS_NO_SUCH_FILE;
		case EEXIST: return STATUS_CANNOT_MAKE;
		case ENOSPC: break;
		case ENOSYS: return STATUS_NOT_IMPLEMENTED;
		case EAGAIN: return STATUS_RETRY;
	}

	return STATUS_UNSUCCESSFUL;
}

static inline void fusent_fill_resp(FUSENT_RESP *resp, IRP *pirp, FILE_OBJECT *fop, int error)
{
	memset(resp, 0, sizeof(FUSENT_RESP));
	resp->pirp = pirp;
	resp->fop = fop;
	resp->error = error;
	resp->status = fusent_translate_errno(error);
}

// Sends a response to the kernel. len is not always == sizeof(FUSENT_RESP),
// so we need to take a parameter.
static inline void fusent_sendmsg(fuse_req_t req, FUSENT_RESP *resp, size_t len)
{
	struct iovec iov;
	iov.iov_base = resp;
	iov.iov_len = len;

	fuse_chan_send(req->ch, &iov, 1);
}

// Send an error reply back to the kernel:
static void fusent_reply_error(fuse_req_t req, PIRP pirp, PFILE_OBJECT fop, int error)
{
	FUSENT_RESP resp;
	fusent_fill_resp(&resp, pirp, fop, -error);
	fusent_sendmsg(req, &resp, sizeof(FUSENT_RESP));
}

// Send a successful response to an IRP_MJ_CREATE irp down to the kernel:
static void fusent_reply_create(fuse_req_t req, PIRP pirp, PFILE_OBJECT fop)
{
	fusent_reply_error(req, pirp, fop, 0);
}

// Send a successful response to an IRP_MJ_WRITE irp down to the kernel:
static void fusent_reply_write(fuse_req_t req, PIRP pirp, PFILE_OBJECT fop, uint32_t written)
{
	FUSENT_RESP resp;
	fusent_fill_resp(&resp, pirp, fop, 0);
	resp.params.write.written = written;
	fusent_sendmsg(req, &resp, sizeof(FUSENT_RESP));
}

// Send a successful response to an IRP_MJ_READ irp down to the kernel:
// buf should have space for a FUSENT_RESP at the beginning.
// buflen includes this.
static void fusent_reply_read(fuse_req_t req, PIRP pirp, PFILE_OBJECT fop, uint32_t buflen, char *buf)
{
	FUSENT_RESP *resp = (FUSENT_RESP *)buf;
	fusent_fill_resp(resp, pirp, fop, 0);

	uint32_t readlen = buflen - sizeof(FUSENT_RESP);
	resp->params.read.buflen = readlen;

	if (!readlen)
		resp->status = STATUS_END_OF_FILE;

	fusent_sendmsg(req, resp, buflen);
}

static void fusent_reply_query_information(fuse_req_t req, PIRP pirp, PFILE_OBJECT fop, struct fuse_attr *st, WCHAR *basename)
{
	size_t buflen = sizeof(FUSENT_RESP) + sizeof(FUSENT_FILE_INFORMATION);
	FUSENT_FILE_INFORMATION *fileinfo;
	FUSENT_RESP *resp = malloc(buflen);

	fileinfo = (FUSENT_FILE_INFORMATION*) (resp + 1);

	// Fill in the standard header bits:
	fusent_fill_resp(resp, pirp, fop, 0);

	resp->params.query.buflen = sizeof(FUSENT_FILE_INFORMATION);

	// Fill in the rest:
	fusent_unixtime_to_wintime(st->atime, &fileinfo->LastAccessTime);
	fusent_unixtime_to_wintime(st->mtime, &fileinfo->LastWriteTime);

	// Take the most recent of {mtime,ctime} for windows' "changetime"
	time_t ctime = (st->mtime > st->ctime)? st->mtime : st->ctime;
	fusent_unixtime_to_wintime(ctime, &fileinfo->ChangeTime);

	fusent_unixmode_to_winattr(st->mode, &fileinfo->FileAttributes);

	fileinfo->AllocationSize.QuadPart = ((int64_t)st->blocks) * 512;
	fileinfo->EndOfFile.QuadPart = (int64_t)st->size;
	fileinfo->NumberOfLinks = st->nlink;
	fileinfo->Directory = S_ISDIR(st->mode);

	fileinfo->DeletePending = FALSE;
	fusent_unixtime_to_wintime(0, &fileinfo->CreationTime);

	fusent_sendmsg(req, resp, buflen);
	free(resp);
}

// Handle an IRP_MJ_CREATE call
static void fusent_do_create(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	// Some of the behavior here probably doesn't match
	// one-to-one with NT. Does anyone actually use
	// FILE_SUPERSEDE? We ignore a lot of CreateOptions flags.
	// -- cemeyer

	// Decode NT operation flags from IRP:
	uint32_t CreateOptions = iosp->Parameters.Create.Options;
	uint16_t ShareAccess = iosp->Parameters.Create.ShareAccess;
	uint8_t CreateDisp = CreateOptions >> 24;

	int issync = 0;
	if ((CreateOptions & FILE_SYNCHRONOUS_IO_ALERT) ||
			(CreateOptions & FILE_SYNCHRONOUS_IO_NONALERT) ||
			(ntreq->irp.Flags & IRP_SYNCHRONOUS_API))
		issync = 1;

	int fuse_flags = 0, mode = S_IRWXU | S_IRWXG | S_IRWXO;
	if (CreateDisp != FILE_OPEN && CreateDisp != FILE_OVERWRITE)
		fuse_flags |= O_CREAT;
	if (CreateDisp == FILE_CREATE) fuse_flags |= O_EXCL;
	if (CreateDisp == FILE_OVERWRITE || CreateDisp == FILE_OVERWRITE_IF ||
			CreateDisp == FILE_SUPERSEDE)
		fuse_flags |= O_TRUNC;
	if (CreateOptions & FILE_DIRECTORY_FILE) fuse_flags |= O_DIRECTORY;

	// Extract the file path from the NT request:
	uint32_t fnamelen;
	uint16_t *fnamep;
	fusent_decode_request_create((FUSENT_CREATE_REQ *)ntreq,
			&fnamelen, &fnamep);

	// Translate it to UTF8:
	char *inbuf = (char *)fnamep;
	size_t inbytes = fnamelen, outbytes = FUSENT_MAX_PATH-1;

	char *llargs;
	fuse_ino_t llinode;
	int llop, err;

	fuse_ino_t fino = 0;;
	struct fuse_file_info *fi = NULL;

	char *basename;
	char *stbuf = malloc(FUSENT_MAX_PATH + max_sz(sizeof(struct fuse_create_in),
				sizeof(struct fuse_open_in)));
	char *outbuf, *outbuf2;

	if (fuse_flags & O_CREAT)
		outbuf = stbuf + sizeof(struct fuse_create_in);
	else
		outbuf = stbuf + sizeof(struct fuse_open_in);

	outbuf2 = outbuf;

	// Convert and then reset the cd
	iconv(cd_utf16le_to_utf8, &inbuf, &inbytes, &outbuf, &outbytes);
	iconv(cd_utf16le_to_utf8, NULL, NULL, NULL, NULL);
	size_t utf8len = outbuf - outbuf2;

	// Convert the path to a unix-like path
	fusent_convert_win_path(outbuf2, &utf8len);

	// A huge hack to make this work for helloworld.
	// TODO(cemeyer) make this general purpose (for any directory):
	// (This will require getattr'ing the file, I think.)
	if (!strncmp(outbuf2, "/", utf8len)) {
		fino = FUSE_ROOT_ID;
		basename = malloc(strlen("/") + 1);
		memcpy(basename, "/", strlen("/") + 1);
		goto reply_create_nt;
	}

	if (fuse_flags & O_CREAT) {
		// Look up inode for the parent directory of the file we want to create:
		struct fuse_create_in *args = (struct fuse_create_in *)stbuf;
		args->flags = fuse_flags;
		args->umask = S_IXGRP | S_IXOTH;

		fuse_ino_t par_inode;
		if (fusent_get_parent_inode(req, outbuf2, &basename, &par_inode) < 0) {
			err = ENOENT;
			fprintf(stderr, "CREATE failed because parent does not exist: (%d)`%s' fnamep: (%d)`%S'\n", utf8len, outbuf2, fnamelen, fnamep);
			goto reply_err_nt;
		}

		// creat() expects just the basename of the file:
		memmove(outbuf2, basename, outbuf - basename);
		outbuf2[outbuf - basename] = '\0';

		llop = FUSE_CREATE;
		llinode = par_inode;
		llargs = (char *)args;
	}
	else {
		// Lookup inode for the file we are to open:
		struct fuse_open_in *args = (struct fuse_open_in *)stbuf;
		args->flags = fuse_flags;

		fuse_ino_t inode;
		if (fusent_get_inode(req, outbuf2, &inode, &basename) < 0) {
			err = ENOENT;
			fprintf(stderr, "OPEN failed because path does not exist: `%s'\n", outbuf2);
			goto reply_err_nt;
		}

		fprintf(stderr, "OPEN: resolved `%s' -> %lu\n", outbuf2, inode);

		fino = inode;

		llop = FUSE_OPEN;
		llinode = inode;
		llargs = (char *)args;
	}

	struct fuse_out_header outh;

	// OPEN => fuse_open_out;
	// CREATE => fuse_entry_out + fuse_open_out
	const size_t buflen = sizeof(struct fuse_entry_out) +
		sizeof(struct fuse_open_out);

	char *giantbuf = malloc(buflen);
	req->response_hijack = &outh;
	req->response_hijack_buf = giantbuf;
	req->response_hijack_buflen = buflen;

	fuse_ll_ops[llop].func(req, llinode, llargs);

	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		// outh.error will be >= -1000 and <= 0:
		err = -outh.error;
		free(giantbuf);
		fprintf(stderr, "CREATE or OPEN failed (%s,%d): `%s'\n", strerror(-outh.error), -outh.error, outbuf2);
		goto reply_err_nt;
	}

	struct fuse_open_out *openresp = (struct fuse_open_out *)giantbuf;
	if (llop == FUSE_CREATE) {
		openresp = (struct fuse_open_out *)(giantbuf + sizeof(struct fuse_entry_out));
		fino = ((struct fuse_entry_out *)giantbuf)->nodeid;
	}

	fi = malloc(sizeof(struct fuse_file_info));
	fi->fh = openresp->fh;
	fi->flags = openresp->open_flags;
	free(giantbuf);

reply_create_nt:
	fprintf(stderr, "CREATE|OPEN: replying success!\n");
	fusent_add_fop_mapping(ntreq->fop, fi, fino, basename, issync);
	fusent_reply_create(req, ntreq->pirp, ntreq->fop);
	free(stbuf);
	return;

reply_err_nt:
	free(stbuf);
	fprintf(stderr, "CREATE|OPEN: replying error(%d) `%s'\n", err, strerror(err));
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_READ request
static void fusent_do_read(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	PFILE_OBJECT fop = ntreq->fop;
	ULONG len = iosp->Parameters.Read.Length;
	LARGE_INTEGER off = iosp->Parameters.Read.ByteOffset;
	int err;

	struct fuse_file_info *fi = NULL;
	fuse_ino_t inode = 0;
	WCHAR *bn = NULL;
	if (fusent_fi_inode_basename_from_fop(fop, &fi, &inode, &bn) < 0) {
		err = EBADF;
		goto reply_err_nt;
	}

	uint64_t current_offset = fusent_get_file_offset(fop);

	struct fuse_out_header outh;
	char *giantbuf = malloc(sizeof(FUSENT_RESP) + len);
	req->response_hijack = &outh;
	req->response_hijack_buf = giantbuf + sizeof(FUSENT_RESP);
	req->response_hijack_buflen = len;

	struct fuse_read_in readargs;
	readargs.fh = fi->fh;
	readargs.flags = fi->flags;
	readargs.lock_owner = fi->lock_owner;
	readargs.size = len;
	readargs.offset = fusent_readwrite_offset(fop, current_offset, off);

	fuse_ll_ops[FUSE_READ].func(req, inode, &readargs);

	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		free(giantbuf);
		err = -outh.error;
		goto reply_err_nt;
	}

	fusent_set_file_offset(fop, readargs.offset + outh.len - sizeof(struct fuse_out_header));

	fusent_reply_read(req, ntreq->pirp, ntreq->fop,
			outh.len - sizeof(struct fuse_out_header) + sizeof(FUSENT_RESP),
			giantbuf);

	free(giantbuf);
	return;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_WRITE request
static void fusent_do_write(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	PFILE_OBJECT fop = ntreq->fop;
	ULONG len = iosp->Parameters.Write.Length;
	LARGE_INTEGER off = iosp->Parameters.Write.ByteOffset;
	int err;

	struct fuse_file_info *fi = NULL;
	fuse_ino_t inode = 0;
	WCHAR *bn = NULL;
	if (fusent_fi_inode_basename_from_fop(fop, &fi, &inode, &bn) < 0) {
		err = EBADF;
		goto reply_err_nt;
	}

	uint32_t stoutbuf[sizeof(struct fuse_write_out) / sizeof(uint32_t)];
	uint8_t *outbufp;
	uint32_t outbuflen;
	fusent_decode_request_write((FUSENT_WRITE_REQ *)ntreq, &outbuflen, &outbufp);

	struct fuse_out_header outh;
	req->response_hijack = &outh;

	// If the buffer for us to write is large enough to serve dual purpose as
	// the output buffer, use it:
	if (outbuflen >= sizeof(struct fuse_write_out)) {
		req->response_hijack_buf = (char *)outbufp;
		req->response_hijack_buflen = outbuflen;
	}
	// Otherwise, use a temporary stack buffer:
	else {
		req->response_hijack_buf = (char *)stoutbuf;
		req->response_hijack_buflen = sizeof(struct fuse_write_out);
		memcpy(stoutbuf, outbufp, outbuflen);
	}

	uint64_t current_offset = fusent_get_file_offset(fop);

	struct fuse_write_in writeargs;
	writeargs.fh = fi->fh;
	writeargs.flags = fi->flags;
	writeargs.lock_owner = fi->lock_owner;
	writeargs.size = len;
	writeargs.offset = fusent_readwrite_offset(fop, current_offset, off);

	fuse_ll_ops[FUSE_WRITE].func(req, inode, &writeargs);

	uint32_t written = ((struct fuse_write_out *)req->response_hijack_buf)->size;
	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		err = -outh.error;
		goto reply_err_nt;
	}

	fusent_set_file_offset(fop, writeargs.offset + written);

	fusent_reply_write(req, ntreq->pirp, ntreq->fop, written);
	return;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_DIRECTORY_CONTROL request
static void fusent_do_directory_control(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	PFILE_OBJECT fop = ntreq->fop;
	int err;
	
	// For more info on these params, see the MSDN on IRP_MJ_DIRECTORY_CONTROL:
	// http://msdn.microsoft.com/en-us/library/ff548658(v=vs.85).aspx
	EXTENDED_IO_STACK_LOCATION *irpsp = (EXTENDED_IO_STACK_LOCATION *)iosp;
	UCHAR flags = irpsp->Flags;
	if (irpsp->MinorFunction != IRP_MN_QUERY_DIRECTORY) {
		err = ENOSYS;
		goto reply_err_nt;
	}
	
	// Make sure this file has already been opened:
	struct fuse_file_info *fi = NULL;
	fuse_ino_t inode = 0;
	WCHAR* bn = NULL;
	if (fusent_fi_inode_basename_from_fop(fop, &fi, &inode, &bn) < 0) {
		err = EBADF;
		goto reply_err_nt;
	}
	
	struct fuse_open_in openargs;
	memset(&openargs, 0, sizeof(openargs));
	openargs.flags = O_RDONLY;
	
	// For now, we ignore FILE_INFORMATION_CLASS and just return some set of fields
	// to the kernel, which sorts out which fields each request needs.
	// FILE_INFORMATION_CLASS fic = irpsp->Parameters.QueryDirectory.FileInformationClass;
	
	uint32_t len = 512; // I have no idea. //irpsp->Parameters.QueryDirectory.Length;
	fprintf(stderr, "dirctrl: Got buflen %u, we're using: %u\n", (unsigned)irpsp->Parameters.QueryDirectory.Length, (unsigned)len);

	struct fuse_out_header outh;
	char *giantbuf = malloc(len);
	req->response_hijack = &outh;
	req->response_hijack_buf = giantbuf;
	req->response_hijack_buflen = len;

	fuse_ll_ops[FUSE_OPENDIR].func(req, inode, &openargs);

	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		err = -outh.error;
		free(giantbuf);
		fprintf(stderr, "OPENDIR failed (%s, %d)\n", strerror(err), err);
		goto reply_err_nt;
	}
	
	struct fuse_open_out *outargs = (struct fuse_open_out *)req->response_hijack_buf;
	
	struct fuse_file_info fi2;
	memset(&fi2, 0, sizeof(fi2));
	fi2.fh = outargs->fh;
	fi2.flags = outargs->open_flags;
	
	fprintf(stderr, "OPENDIR succeeded, basename: '%S'\n", bn); 
	
	struct fuse_read_in readargs;
	readargs.fh = fi2.fh;
	readargs.flags = fi2.flags;
	readargs.lock_owner = fi2.lock_owner;
	readargs.size = len;
	readargs.offset = 0; // TODO: care about this
	
	req->response_hijack = &outh;
	req->response_hijack_buf = giantbuf;
	req->response_hijack_buflen = len;

	fuse_ll_ops[FUSE_READDIR].func(req, inode, &readargs);

	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		err = -outh.error;
		free(giantbuf);
		fprintf(stderr, "READDIR failed (%s, %d)\n", strerror(err), err);
		goto reply_err_nt;
	}
	
	char *p = giantbuf;
	size_t nbytes = outh.len - sizeof(struct fuse_out_header);

	// Hack (stolen from kernel/fuse_i.h):
#define FUSE_NAME_MAX 1024

	// Ripped more or less directly from the Linux kernel fuse fs function parse_dirfile in dir.c:

	while (nbytes >= FUSE_NAME_OFFSET) {
		struct fuse_dirent *dirent = (struct fuse_dirent *)p;
		size_t reclen = fuse_dirent_size(dirent->namelen);

		if (!dirent->namelen || dirent->namelen > FUSE_NAME_MAX) {
			err = EIO;
			goto reply_err_nt;
		}
		if (reclen > nbytes) break;

		fprintf(stderr, "current->name: %.*s\treclen:0x%.8x\n", (int)dirent->namelen, dirent->name, reclen);

		// TODO: do shit with this dirent

		p += reclen;
		nbytes -= reclen;
		fusent_set_file_offset(fop, fusent_get_file_offset(fop) + nbytes);
	}
	
	err = ENOENT;
	//goto reply_err_nt; // fall through

reply_err_nt:
	
	printf("%s\n", "fusent_do_directory_control -- 6");

	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_CLEANUP request
static void fusent_do_cleanup(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	// TODO flush
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, 0);
}

// Handle an IRP_MJ_CLOSE request
static void fusent_do_close(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: flush, release

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

/*
// Handle an IRP_MJ_DEVICE_CONTROL request
static void fusent_do_device_control(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_FILE_SYSTEM_CONTROL request
static void fusent_do_file_system_control(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}
*/

// Handle an IRP_MJ_FLUSH_BUFFERS request
static void fusent_do_flush_buffers(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

/*
// Handle an IRP_MJ_INTERNAL_DEVICE_CONTROL request
static void fusent_do_internal_device_control(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_PNP request
static void fusent_do_pnp(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_POWER request
static void fusent_do_power(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}
*/

// Handle an IRP_MJ_QUERY_INFORMATION request
static void fusent_do_query_information(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	PFILE_OBJECT fop = ntreq->fop;
	int err;

	struct fuse_file_info *fi = NULL;
	fuse_ino_t inode = 0;
	WCHAR *basename = NULL;
	if (fusent_fi_inode_basename_from_fop(fop, &fi, &inode, &basename) < 0) {
		err = EBADF;
		goto reply_err_nt;
	}

	struct fuse_out_header outh;
	struct fuse_attr_out attr;
	struct fuse_getattr_in args = { 0, 0, 0 };

	req->response_hijack = &outh;
	req->response_hijack_buf = (char *)&attr;
	req->response_hijack_buflen = sizeof(struct fuse_attr_out);

	fuse_ll_ops[FUSE_GETATTR].func(req, inode, &args);

	req->response_hijack = NULL;
	req->response_hijack_buf = NULL;

	if (outh.error) {
		err = -outh.error;
		fprintf(stderr, "QUERY_INFO failed (%s, %d)\n", strerror(err), err);
		goto reply_err_nt;
	}

	fusent_reply_query_information(req, ntreq->pirp, ntreq->fop, &attr.attr, basename);
	return;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

/*
// Handle an IRP_MJ_SET_INFORMATION request
static void fusent_do_set_information(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_SHUTDOWN request
static void fusent_do_shutdown(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

// Handle an IRP_MJ_SYSTEM_CONTROL request
static void fusent_do_system_control(FUSENT_REQ *ntreq, IO_STACK_LOCATION *iosp, fuse_req_t req)
{
	UCHAR flags = iosp->Flags;
	int err;

	// TODO: fill in this function stub

	err = ENOSYS;
	goto reply_err_nt;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}
*/

// Initialize FUSE (HACK)
static void fusent_do_init(struct fuse_ll *f)
{
	// TODO(cemeyer) call do_init or whatever fuse calls. low priority.

	f->got_init = 1;

	// stolen from the 2.6.38 kernel
	f->conn.proto_major = 7;
	f->conn.proto_minor = 16;

	f->conn.capable = 0;
	f->conn.want = 0;
	f->conn.async_read = 0;
	if (f->op.init) f->op.init(f->userdata, &f->conn);

	// This might be good enough:
	f->conn.max_write = 8192;
}

// Handle incoming FUSE-NT protocol messages:
static void fusent_ll_process(void *data, const char *buf, size_t len,
		struct fuse_chan *ch)
{
	struct fuse_ll *f = (struct fuse_ll *) data;
	struct fuse_req *req;
	int err;

	FUSENT_REQ *ntreq = (FUSENT_REQ *)buf;

	req = (struct fuse_req *) calloc(1, sizeof(struct fuse_req));
	if (req == NULL) {
		fprintf(stderr, "fuse: failed to allocate request\n");
		return;
	}

	if (!f->got_init) {
		fusent_do_init(f);
	}

	req->f = f;
	req->unique = 0; // this might not be needed except for interrupts --cemeyer
	req->ctx.uid = 0; // not sure these have any correct meanings
	req->ctx.gid = 0;
	req->ctx.pid = 0; // what is this used for? maybe need to pass as part of the request --cemeyer
	req->ch = ch;
	req->ctr = 1;
	req->response_hijack = NULL;
	list_init_req(req);
	fuse_mutex_init(&req->lock);

	int status;
	IO_STACK_LOCATION *iosp;
	uint8_t irptype;

	err = EIO;

	status = fusent_decode_irp(&ntreq->irp, &ntreq->iostack[0], &irptype, &iosp);
	if (status < 0) goto reply_err_nt;

	switch (irptype) {
		case IRP_MJ_CREATE:
			fprintf(stderr, "fusent: got CREATE on %p\n", ntreq->fop);
			fusent_do_create(ntreq, iosp, req);
			break;

		case IRP_MJ_READ:
			fprintf(stderr, "fusent: got READ on %p\n", ntreq->fop);
			fusent_do_read(ntreq, iosp, req);
			break;

		case IRP_MJ_WRITE:
			fusent_do_write(ntreq, iosp, req);
			break;

		case IRP_MJ_DIRECTORY_CONTROL:
			fprintf(stderr, "fusent: got DIRECTORY_CONTROL on %p\n", ntreq->fop);
			fusent_do_directory_control(ntreq, iosp, req);
			break;

		case IRP_MJ_CLEANUP:
			fprintf(stderr, "fusent: got CLEANUP on %p\n", ntreq->fop);
			fusent_do_cleanup(ntreq, iosp, req);
			break;

		case IRP_MJ_CLOSE:
			fprintf(stderr, "fusent: got CLOSE on %p\n", ntreq->fop);
			fusent_do_close(ntreq, iosp, req);
			break;

		/*
		case IRP_MJ_DEVICE_CONTROL:
			fusent_do_device_control(ntreq, iosp, req);
			break;

		case IRP_MJ_FILE_SYSTEM_CONTROL:
			fusent_do_file_system_control(ntreq, iosp, req);
			break;
		*/

		case IRP_MJ_FLUSH_BUFFERS:
			fusent_do_flush_buffers(ntreq, iosp, req);
			break;

		/*
		case IRP_MJ_INTERNAL_DEVICE_CONTROL:
			fusent_do_internal_device_control(ntreq, iosp, req);
			break;

		case IRP_MJ_PNP:
			fusent_do_pnp(ntreq, iosp, req);
			break;

		case IRP_MJ_POWER:
			fusent_do_power(ntreq, iosp, req);
			break;
		*/

		case IRP_MJ_QUERY_INFORMATION:
			fprintf(stderr, "fusent: got QUERY_INFORMATION on %p\n", ntreq->fop);
			fusent_do_query_information(ntreq, iosp, req);
			break;

		/*
		case IRP_MJ_SET_INFORMATION:
			fusent_do_set_information(ntreq, iosp, req);
			break;

		case IRP_MJ_SHUTDOWN:
			fusent_do_shutdown(ntreq, iosp, req);
			break;

		case IRP_MJ_SYSTEM_CONTROL:
			fusent_do_system_control(ntreq, iosp, req);
			break;
		*/

		default:
			err = ENOSYS;
			goto reply_err_nt;
	}
	return;

reply_err_nt:
	fusent_reply_error(req, ntreq->pirp, ntreq->fop, err);
}

#else /* __CYGWIN__ */

static void fuse_ll_process(void *data, const char *buf, size_t len,
			    struct fuse_chan *ch)
{
	struct fuse_ll *f = (struct fuse_ll *) data;
	struct fuse_req *req;
	int err;

	struct fuse_in_header *in = (struct fuse_in_header *) buf;
	const void *inarg = buf + sizeof(struct fuse_in_header);

	if (f->debug)
		fprintf(stderr,
				"unique: %llu, opcode: %s (%i), nodeid: %lu, insize: %zu\n",
				(unsigned long long) in->unique,
				opname((enum fuse_opcode) in->opcode), in->opcode,
				(unsigned long) in->nodeid, len);

	req = (struct fuse_req *) calloc(1, sizeof(struct fuse_req));
	if (req == NULL) {
		fprintf(stderr, "fuse: failed to allocate request\n");
		return;
	}

	req->f = f;
	req->unique = in->unique;
	req->ctx.uid = in->uid;
	req->ctx.gid = in->gid;
	req->ctx.pid = in->pid;
	req->ch = ch;
	req->ctr = 1;
	list_init_req(req);
	fuse_mutex_init(&req->lock);

	err = EIO;
	if (!f->got_init) {
		enum fuse_opcode expected;

		expected = f->cuse_data ? CUSE_INIT : FUSE_INIT;
		if (in->opcode != expected)
			goto reply_err;
	} else if (in->opcode == FUSE_INIT || in->opcode == CUSE_INIT)
		goto reply_err;

	err = EACCES;
	if (f->allow_root && in->uid != f->owner && in->uid != 0 &&
		 in->opcode != FUSE_INIT && in->opcode != FUSE_READ &&
		 in->opcode != FUSE_WRITE && in->opcode != FUSE_FSYNC &&
		 in->opcode != FUSE_RELEASE && in->opcode != FUSE_READDIR &&
		 in->opcode != FUSE_FSYNCDIR && in->opcode != FUSE_RELEASEDIR)
		goto reply_err;

	err = ENOSYS;
	if (in->opcode >= FUSE_MAXOP || !fuse_ll_ops[in->opcode].func)
		goto reply_err;
	if (in->opcode != FUSE_INTERRUPT) {
		struct fuse_req *intr;
		pthread_mutex_lock(&f->lock);
		intr = check_interrupt(f, req);
		list_add_req(req, &f->list);
		pthread_mutex_unlock(&f->lock);
		if (intr)
			fuse_reply_err(intr, EAGAIN);
	}
	fuse_ll_ops[in->opcode].func(req, in->nodeid, inarg);
	return;

reply_err:
	fuse_reply_err(req, err);
}
#endif /* !__CYGWIN__ */

enum {
	KEY_HELP,
	KEY_VERSION,
};

static struct fuse_opt fuse_ll_opts[] = {
	{ "debug", offsetof(struct fuse_ll, debug), 1 },
	{ "-d", offsetof(struct fuse_ll, debug), 1 },
	{ "allow_root", offsetof(struct fuse_ll, allow_root), 1 },
	{ "max_write=%u", offsetof(struct fuse_ll, conn.max_write), 0 },
	{ "max_readahead=%u", offsetof(struct fuse_ll, conn.max_readahead), 0 },
	{ "async_read", offsetof(struct fuse_ll, conn.async_read), 1 },
	{ "sync_read", offsetof(struct fuse_ll, conn.async_read), 0 },
	{ "atomic_o_trunc", offsetof(struct fuse_ll, atomic_o_trunc), 1},
	{ "no_remote_lock", offsetof(struct fuse_ll, no_remote_lock), 1},
	{ "big_writes", offsetof(struct fuse_ll, big_writes), 1},
	FUSE_OPT_KEY("max_read=", FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_END
};

static void fuse_ll_version(void)
{
	fprintf(stderr, "using FUSE kernel interface version %i.%i\n",
		FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);
}

static void fuse_ll_help(void)
{
	fprintf(stderr,
"    -o max_write=N         set maximum size of write requests\n"
"    -o max_readahead=N     set maximum readahead\n"
"    -o async_read          perform reads asynchronously (default)\n"
"    -o sync_read           perform reads synchronously\n"
"    -o atomic_o_trunc      enable atomic open+truncate support\n"
"    -o big_writes          enable larger than 4kB writes\n"
"    -o no_remote_lock      disable remote file locking\n");
}

static int fuse_ll_opt_proc(void *data, const char *arg, int key,
			    struct fuse_args *outargs)
{
	(void) data; (void) outargs;

	switch (key) {
	case KEY_HELP:
		fuse_ll_help();
		break;

	case KEY_VERSION:
		fuse_ll_version();
		break;

	default:
		fprintf(stderr, "fuse: unknown option `%s'\n", arg);
	}

	return -1;
}

int fuse_lowlevel_is_lib_option(const char *opt)
{
	return fuse_opt_match(fuse_ll_opts, opt);
}

static void fuse_ll_destroy(void *data)
{
	struct fuse_ll *f = (struct fuse_ll *) data;

	if (f->got_init && !f->got_destroy) {
		if (f->op.destroy)
			f->op.destroy(f->userdata);
	}

	pthread_mutex_destroy(&f->lock);
	free(f->cuse_data);
	free(f);
}

/*
 * always call fuse_lowlevel_new_common() internally, to work around a
 * misfeature in the FreeBSD runtime linker, which links the old
 * version of a symbol to internal references.
 */
struct fuse_session *fuse_lowlevel_new_common(struct fuse_args *args,
					      const struct fuse_lowlevel_ops *op,
					      size_t op_size, void *userdata)
{
	struct fuse_ll *f;
	struct fuse_session *se;
	struct fuse_session_ops sop = {
#if defined __CYGWIN__
		.process = fusent_ll_process,
#else
		.process = fuse_ll_process,
#endif
		.destroy = fuse_ll_destroy,
	};

	if (sizeof(struct fuse_lowlevel_ops) < op_size) {
		fprintf(stderr, "fuse: warning: library too old, some operations may not work\n");
		op_size = sizeof(struct fuse_lowlevel_ops);
	}

	f = (struct fuse_ll *) calloc(1, sizeof(struct fuse_ll));
	if (f == NULL) {
		fprintf(stderr, "fuse: failed to allocate fuse object\n");
		goto out;
	}

	f->conn.async_read = 1;
	f->conn.max_write = UINT_MAX;
	f->conn.max_readahead = UINT_MAX;
	f->atomic_o_trunc = 0;
	list_init_req(&f->list);
	list_init_req(&f->interrupts);
	fuse_mutex_init(&f->lock);

	if (fuse_opt_parse(args, f, fuse_ll_opts, fuse_ll_opt_proc) == -1)
		goto out_free;

	if (f->debug)
		fprintf(stderr, "FUSE library version: %s\n", PACKAGE_VERSION);

	memcpy(&f->op, op, op_size);
	f->owner = getuid();
	f->userdata = userdata;

	se = fuse_session_new(&sop, f);
	if (!se)
		goto out_free;

	return se;

out_free:
	free(f);
out:
	return NULL;
}


struct fuse_session *fuse_lowlevel_new(struct fuse_args *args,
				       const struct fuse_lowlevel_ops *op,
				       size_t op_size, void *userdata)
{
	return fuse_lowlevel_new_common(args, op, op_size, userdata);
}

#if defined(linux) && !defined(__CYGWIN__)
int fuse_req_getgroups(fuse_req_t req, int size, gid_t list[])
{
	char *buf;
	size_t bufsize = 1024;
	char path[128];
	int ret;
	int fd;
	unsigned long pid = req->ctx.pid;
	char *s;

	sprintf(path, "/proc/%lu/task/%lu/status", pid, pid);

retry:
	buf = malloc(bufsize);
	if (buf == NULL)
		return -ENOMEM;

	ret = -EIO;
	fd = open(path, O_RDONLY);
	if (fd == -1)
		goto out_free;

	ret = read(fd, buf, bufsize);
	close(fd);
	if (ret == -1) {
		ret = -EIO;
		goto out_free;
	}

	if (ret == bufsize) {
		free(buf);
		bufsize *= 4;
		goto retry;
	}

	ret = -EIO;
	s = strstr(buf, "\nGroups:");
	if (s == NULL)
		goto out_free;

	s += 8;
	ret = 0;
	while (1) {
		char *end;
		unsigned long val = strtoul(s, &end, 0);
		if (end == s)
			break;

		s = end;
		if (ret < size)
			list[ret] = val;
		ret++;
	}

out_free:
	free(buf);
	return ret;
}
#else /* linux */
/*
 * This is currently not implemented on other than Linux...
 */
int fuse_req_getgroups(fuse_req_t req, int size, gid_t list[])
{
	return -ENOSYS;
}
#endif

#if !defined __FreeBSD__ && !defined __CYGWIN__

static void fill_open_compat(struct fuse_open_out *arg,
			     const struct fuse_file_info_compat *f)
{
	arg->fh = f->fh;
	if (f->direct_io)
		arg->open_flags |= FOPEN_DIRECT_IO;
	if (f->keep_cache)
		arg->open_flags |= FOPEN_KEEP_CACHE;
}

static void convert_statfs_compat(const struct statfs *compatbuf,
				  struct statvfs *buf)
{
	buf->f_bsize	= compatbuf->f_bsize;
	buf->f_blocks	= compatbuf->f_blocks;
	buf->f_bfree	= compatbuf->f_bfree;
	buf->f_bavail	= compatbuf->f_bavail;
	buf->f_files	= compatbuf->f_files;
	buf->f_ffree	= compatbuf->f_ffree;
	buf->f_namemax	= compatbuf->f_namelen;
}

int fuse_reply_open_compat(fuse_req_t req,
			   const struct fuse_file_info_compat *f)
{
	struct fuse_open_out arg;

	memset(&arg, 0, sizeof(arg));
	fill_open_compat(&arg, f);
	return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_statfs_compat(fuse_req_t req, const struct statfs *stbuf)
{
	struct statvfs newbuf;

	memset(&newbuf, 0, sizeof(newbuf));
	convert_statfs_compat(stbuf, &newbuf);

	return fuse_reply_statfs(req, &newbuf);
}

struct fuse_session *fuse_lowlevel_new_compat(const char *opts,
				const struct fuse_lowlevel_ops_compat *op,
				size_t op_size, void *userdata)
{
	struct fuse_session *se;
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

	if (opts &&
	    (fuse_opt_add_arg(&args, "") == -1 ||
	     fuse_opt_add_arg(&args, "-o") == -1 ||
	     fuse_opt_add_arg(&args, opts) == -1)) {
		fuse_opt_free_args(&args);
		return NULL;
	}
	se = fuse_lowlevel_new(&args, (const struct fuse_lowlevel_ops *) op,
			       op_size, userdata);
	fuse_opt_free_args(&args);

	return se;
}

struct fuse_ll_compat_conf {
	unsigned max_read;
	int set_max_read;
};

static const struct fuse_opt fuse_ll_opts_compat[] = {
	{ "max_read=", offsetof(struct fuse_ll_compat_conf, set_max_read), 1 },
	{ "max_read=%u", offsetof(struct fuse_ll_compat_conf, max_read), 0 },
	FUSE_OPT_KEY("max_read=", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_END
};

int fuse_sync_compat_args(struct fuse_args *args)
{
	struct fuse_ll_compat_conf conf;

	memset(&conf, 0, sizeof(conf));
	if (fuse_opt_parse(args, &conf, fuse_ll_opts_compat, NULL) == -1)
		return -1;

	if (fuse_opt_insert_arg(args, 1, "-osync_read"))
		return -1;

	if (conf.set_max_read) {
		char tmpbuf[64];

		sprintf(tmpbuf, "-omax_readahead=%u", conf.max_read);
		if (fuse_opt_insert_arg(args, 1, tmpbuf) == -1)
			return -1;
	}
	return 0;
}

FUSE_SYMVER(".symver fuse_reply_statfs_compat,fuse_reply_statfs@FUSE_2.4");
FUSE_SYMVER(".symver fuse_reply_open_compat,fuse_reply_open@FUSE_2.4");
FUSE_SYMVER(".symver fuse_lowlevel_new_compat,fuse_lowlevel_new@FUSE_2.4");

#else /* __FreeBSD__ || __CYGWIN__ */

int fuse_sync_compat_args(struct fuse_args *args)
{
	(void) args;
	return 0;
}

#endif /* __FreeBSD__ || __CYGWIN__ */

#ifndef __CYGWIN__

struct fuse_session *fuse_lowlevel_new_compat25(struct fuse_args *args,
				const struct fuse_lowlevel_ops_compat25 *op,
				size_t op_size, void *userdata)
{
	if (fuse_sync_compat_args(args) == -1)
		return NULL;

	return fuse_lowlevel_new_common(args,
					(const struct fuse_lowlevel_ops *) op,
					op_size, userdata);
}

FUSE_SYMVER(".symver fuse_lowlevel_new_compat25,fuse_lowlevel_new@FUSE_2.5");

#endif /* __CYGWIN__ */
