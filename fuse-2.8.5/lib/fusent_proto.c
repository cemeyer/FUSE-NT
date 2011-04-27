#ifdef __CYGWIN__
#include "fusent_proto.h"

// Takes a FUSE_CREATE_REQ and finds the variably-located fields:
void fusent_decode_request_create(FUSENT_CREATE_REQ *req, uint32_t *outfnamelen,
		uint16_t **outfnamep)
{
	uint32_t *fnamelenp = (uint32_t *)(req->iostack + req->irp.StackCount);
	*outfnamelen = *fnamelenp;
	*outfnamep = (uint16_t *)(fnamelenp + 1);
}

// Takes a FUSE_WRITE_REQ and finds the variably-located fields:
void fusent_decode_request_write(FUSENT_WRITE_REQ *req, uint32_t *outbuflen,
		uint8_t **outbufp)
{
	uint32_t *buflenp = (uint32_t *)(req->iostack + req->irp.StackCount);
	*outbuflen = *buflenp;
	*outbufp = (uint8_t *)(buflenp + 1);
}

#endif /* __CYGWIN__ */
