/*
  FUSE-NT: Filesystem in Userspace (for Windows NT)
  Copyright (C) 2011  The FUSE-NT Authors

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file LGPLv2.txt.
*/

#ifdef __CYGWIN__
#include "fusent_routines.h"

// Decodes an IRP (and associated IO stack) to locate the current stack entry
// and the IRP major number.
//
// Returns non-negative on success.
int fusent_decode_irp(IRP *irp, IO_STACK_LOCATION *iosp, uint8_t *outirptype,
		IO_STACK_LOCATION **outiosp)
{
	// CurrentLocation is 1-indexed:
	*outiosp = &iosp[irp->CurrentLocation - 1];
	*outirptype = (*outiosp)->MajorFunction;

	return 0;
}

// Translates a string from one encoding to another.
// src, s_len - source string, and its length in bytes
// dst, d_len - destination buffer, and its length in bytes
// in_enc, out_enc - input/output charsets, e.g. "UTF-8"
//
// Returns negative on error, or the number of bytes output on success.
size_t fusent_transcode(void *src, size_t s_len, void *dst, size_t d_len, const char *in_enc, const char *out_enc)
{
	size_t res = -1;
	iconv_t cd;
	if ((cd = iconv_open(out_enc, in_enc)) == (iconv_t)-1) goto leave;

	char *in = src, *out = dst;
	size_t inb = s_len, outb = d_len;

	if (iconv(cd, &in, &inb, &out, &outb) == -1) goto close_cd;

	res = d_len - outb;

close_cd:
	iconv_close(cd);
leave:
	return res;
}

// Translates (roughly) a Unix time_t (seconds since unix epoch) to a Windows' LARGE_INTEGER time (100-ns intervals since Jan 1, 1601).
void fusent_unixtime_to_wintime(time_t t, LARGE_INTEGER *wintime)
{
	const int64_t ns100_per_s = 10000000;
	int64_t tmp = ns100_per_s * t; // t is now 100-ns intervals since Jan 1 1970

	// Very roughly translate the time back from the unix epoch to 1601 (estimates leap years, roughly):
	tmp += (1970 - 1601) * 365.242199 * 24 * 60 * 60 * ns100_per_s;

	wintime->QuadPart = tmp;
}

#endif /* __CYGWIN__ */
