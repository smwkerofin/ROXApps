/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.h,v 1.1 2001/07/17 14:44:50 stephen Exp $
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

/* with%20space -> with space */
extern gchar *unescape_uri(const char *uri);
/* /path/with space -> file://thishost/path/with%20space */
extern gchar *encode_path_as_uri(const guchar *path);
/* /path/with space -> /path/with%20space */
extern gchar *escape_uri_path(const char *path);

#endif

/*
 * $Log: rox_path.h,v $
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
