/*
 * $Id$
 *
 * rox.h - Top level header for ROX-CLib
 */

#ifndef _rox_h
#define _rox_h

/*
 * Useful headers.  Most programs will need all of these.
 */
#include "rox_debug.h"
#include "choices.h"
#include "error.h"
#include "infowin.h"
#include "rox_resources.h"

/* Version number of ROX-CLib where x.y.z is encoded as xxyyzz */
extern int rox_clib_version_number(void);
extern const char *rox_clib_version_string(void);

#endif

/*
 * $Log$
 */
