/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id$
 */

#ifndef _rox_path_h
#define _rox_path_h

/* Pass results to g_free() when done */
extern gchar *rox_path_get_local(const gchar *uri);
extern gchar *rox_path_get_server(const gchar *uri);
extern gchar *rox_path_get_path(const gchar *uri);

#endif

/*
 * $Log$
 */
