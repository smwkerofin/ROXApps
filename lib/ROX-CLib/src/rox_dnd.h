/*
 * rox_dnd.c - utilities for using drag & drop with ROX apps.
 *
 * $Id: rox_dnd.h,v 1.2 2001/07/23 12:58:21 stephen Exp $
 */

#ifndef _rox_dnd_h
#define _rox_dnd_h

/*
 * Function which responds to drop on a widget by processing a list of URIs
 * Return TRUE if handled successfully, FALSE otherwise.
 */
typedef gboolean (*rox_dnd_handle_uris)(GtkWidget *widget,
					GSList *uris,
					gpointer data,
					gpointer udata);

typedef gboolean (*rox_dnd_handle_xds) (GtkWidget *widget,
					const gchar *path,
					gpointer data,
					gpointer udata);

extern void rox_dnd_init(void);

extern void rox_dnd_register_full(GtkWidget *widget,
				  guint flags,
				  rox_dnd_handle_uris,
				  rox_dnd_handle_xds,
				  gpointer udata);

#define rox_dnd_register_uris(w, f, u, d) \
              rox_dnd_register_full(w, f, u, NULL, d)
#define rox_dnd_register_xds(w, f, x, d) \
              rox_dnd_register_full(w, f, NULL, x, d)
				  
/*
 * Scans list of URIs and picks out only those that refer to local files.
 * Free result with rox_dnd_local_free
 */
extern GSList *rox_dnd_filter_local(GSList *uris);
extern void rox_dnd_local_free(GSList *paths);

#endif

/*
 * $Log: rox_dnd.h,v $
 * Revision 1.2  2001/07/23 12:58:21  stephen
 * XDS needs to pass the path (or uri) to the callback
 *
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
