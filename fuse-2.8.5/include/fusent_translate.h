/*
  FUSE-NT: Filesystem in Userspace (for Windows NT)
  Copyright (C) 2011  The FUSE-NT Authors

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file LGPLv2.txt.
*/

#ifdef __CYGWIN__
#ifndef FUSENT_TRANSLATE_H
#define FUSENT_TRANSLATE_H

#include "fusent_proto.h"

// Sets up any data structures fusent_translate will need to persist
// across calls.
void fusent_translate_setup(void);

// Destroys any persistant data structures at shut down.
void fusent_translate_teardown(void);

#endif /* FUSENT_TRANSLATE_H */
#endif /* __CYGWIN__ */
