/*
 * $Id: rox.h,v 1.1 2002/02/13 11:00:37 stephen Exp $
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

/* And again for the version of GTK+ we work with */
extern int rox_clib_gtk_version_number(void);
extern const char *rox_clib_gtk_version_string(void);

#endif

/*
 * $Log: rox.h,v $
 * Revision 1.1  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 */
