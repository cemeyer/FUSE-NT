#ifndef FUSE_INCLUDES_H_
#define FUSE_INCLUDES_H_

/* First, kernel headers: */

/* Work around buggy MingW64 headers with conflicting definitions ... */
#define __INTRINSIC_DEFINED_InterlockedBitTestAndSet
#define __INTRINSIC_DEFINED_InterlockedBitTestAndReset

#include <ntifs.h>

#if 0
#include <ntddcdrm.h>
#endif
#include <ntdddisk.h>
#include <ntddstor.h>

/* Disables driver verify annotation macros (gcc doesn't support it) */
#include <driverspecs.h>
#ifndef __drv_aliasesMem
# define __drv_aliasesMem
#endif
#ifndef __drv_releasesResource
# define __drv_releasesResource(xxx)
#endif


/* Finally, our internal headers: */

#ifndef INLINE
#define INLINE inline
#endif

#include "fusent_proto.h"
#include "fuseprocs.h"
#include "fuseutil.h"

/* 3rd-party hashmap library; TODO nuke, replace with kernel ADTs... */

#include "hashmap.h"

#endif
