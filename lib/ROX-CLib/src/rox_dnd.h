/*
 * rox_dnd.c - utilities for using drag & drop with ROX apps.
 *
 * $Id$
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
				  
/*
 * Scans list of URIs and picks out only those that refer to local files.
 * Free result with g_slist_free after freeing data with g_free
 */
extern GSList *rox_dnd_filter_local(GSList *uris);

#endif

/*
 * $Log$
 */
