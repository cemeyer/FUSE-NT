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
//
// Returns negative on error, or the number of bytes output on success.
size_t fusent_transcode(void *src, size_t s_len, void *dst, size_t d_len, const char *in_enc, const char *out_enc)
{
	size_t res = -1;
	iconv_t cd;
	if ((cd = iconv_open(out_enc, in_enc)) == -1) goto leave;

	char *in = src, *out = dst;
	size_t inb = s_len, outb = d_len;

	if (iconv(cd, &in, &inb, &out, &outb) == -1) goto close_cd;

	res = d_len - outb;

close_cd:
	iconv_close(cd);
leave:
	return res;
}

#endif /* __CYGWIN__ */
