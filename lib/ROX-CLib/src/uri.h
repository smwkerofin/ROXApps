/*
 * uri.c - utilities for uri handling and launching
 *
 * $Id: uri.h,v 1.2 2005/09/10 16:17:55 stephen Exp $
 */

#ifndef _rox_uri_h
#define _rox_uri_h

/**
 * @file uri.h
 * @brief Utilities for uri handling and launching.
 *
 * @author Stephen Watson
 * @version $Id: uri.h,v 1.2 2005/09/10 16:17:55 stephen Exp $
 */

/* with%20space -> with space */
extern gchar *rox_unescape_uri(const char *uri);
/* /path/with space -> file://thishost/path/with%20space */
extern gchar *rox_encode_path_as_uri(const guchar *path);
/* /path/with space -> /path/with%20space */
extern gchar *rox_escape_uri_path(const char *path);

extern gchar *rox_uri_get_handler(const char *scheme);
extern int rox_uri_launch_handler(const char *uri, gboolean block,
				  GError **err);

extern int rox_uri_launch(const char *uri);

extern const char *rox_hostname(void); /* Name to use for D&D */

#endif

/*
 * $Log: uri.h,v $
 * Revision 1.2  2005/09/10 16:17:55  stephen
 * Added doxygen comments
 *
 * Revision 1.1  2004/10/02 13:10:28  stephen
 * Added uri.h and rox_uri_launch() (and moved some stuff from rox_path
 * there) to better handle launching URIs.  ROXInfoWin now uses it.
 *
 */
