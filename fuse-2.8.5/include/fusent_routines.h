#ifdef __CYGWIN__
#ifndef IRPDECODE_H
#define IRPDECODE_H

// Windows headers don't pull in their own dependencies... see fusent_proto.h:
#define NTOSAPI /**/
#ifndef DDKAPI
# define DDKAPI /**/
#endif
#include <windef.h>
#include <winnt.h>
#include <ntdef.h>
#include <ddk/ntddk.h>
#include <ddk/winddk.h>
#include <stdint.h>

#include <iconv.h>

// Decodes an IRP (and associated IO stack) to locate the current stack entry
// and the IRP major number.
//
// Returns non-negative on success.
int fusent_decode_irp(IRP *irp, IO_STACK_LOCATION *iosp, uint8_t *outirptype,
		IO_STACK_LOCATION **outiosp);

// Translates a string from one encoding to another.
//
// Returns negative on error, or the number of bytes output on success.
size_t fusent_transcode(void *src, size_t s_len, void *dst, size_t d_len, const char *in_enc, const char *out_enc);

#endif /* IRPDECODE_H */
#endif /* __CYGWIN__ */
