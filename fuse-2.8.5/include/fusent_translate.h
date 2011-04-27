#ifdef __CYGWIN__
#ifndef FUSENT_TRANSLATE_H
#define FUSENT_TRANSLATE_H

#include "fusent_proto.h"

// Sets up any data structures fusent_translate will need to persist
// across calls.
void fusent_translate_setup();

// Destroys any persistant data structures at shut down.
void fusent_translate_teardown();

// Translates a FUSENT kernel request (`req') into a FUSE request, filling in
// `outbuf'; `sz' takes on the length of `outbuf' in bytes.
//
// `outbuf' is assumed to be large enough to hold any FUSE request packet.
//
// Returns non-negative on success.
int fusent_translate_nt_to_fuse(FUSENT_REQ *req, char *outbuf, int *sz);

#endif /* FUSENT_TRANSLATE_H */
#endif /* __CYGWIN__ */
