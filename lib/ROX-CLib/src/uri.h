/*
 * uri.c - utilities for uri handling and launching
 *
 * $Id$
 */

#ifndef _rox_uri_h
#define _rox_uri_h

/* with%20space -> with space */
extern gchar *rox_unescape_uri(const char *uri);
/* /path/with space -> file://thishost/path/with%20space */
extern gchar *rox_encode_path_as_uri(const guchar *path);
/* /path/with space -> /path/with%20space */
extern gchar *rox_escape_uri_path(const char *path);

extern int rox_uri_launch(const char *uri);

extern const char *rox_hostname(void); /* Name to use for D&D */

#endif

/*
 * $Log$
 */
