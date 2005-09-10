/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.h,v 1.3 2004/10/02 13:10:22 stephen Exp $
 */

/**
 * @file rox_path.h
 * @brief Utilities for path handling, including support for drag & drop.
 *
 * @author Stephen Watson
 * @version $Id$
 */

#ifndef _rox_path_h
#define _rox_path_h

/* Pass results to g_free() when done */

/* file://thishost/path -> /path */
extern gchar *rox_path_get_local(const gchar *uri);
/* file://host/path -> host */
extern gchar *rox_path_get_server(const gchar *uri);
/* file://host/path -> /path */
extern gchar *rox_path_get_path(const gchar *uri);

extern int rox_hostname_is_local(const char *hname);

#include "uri.h"
/* Old names */
#define unescape_uri(u) rox_unescape_uri(u)
#define encode_path_as_uri(p) rox_encode_path_as_uri(p)
#define escape_uri_path(p) rox_escape_uri_path(p)

#endif

/*
 * $Log: rox_path.h,v $
 * Revision 1.3  2004/10/02 13:10:22  stephen
 * Added uri.h and rox_uri_launch() (and moved some stuff from rox_path
 * there) to better handle launching URIs.  ROXInfoWin now uses it.
 *
 * Revision 1.2  2003/12/13 19:26:05  stephen
 * Exposed functions to escape and unescape uri's.
 * rox_path_get_local() and rox_path_get_path() now unescape uri's.
 *
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
