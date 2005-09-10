/*
 * rox_dnd.c - utilities for using drag & drop with ROX apps.
 *
 * $Id: rox_dnd.h,v 1.3 2001/08/20 15:28:09 stephen Exp $
 */

/**
 * @file rox_dnd.h
 * @brief  Utilities for using drag & drop with ROX apps.
 *
 * @author Stephen Watson
 * @version $Id$
 */

#ifndef _rox_dnd_h
#define _rox_dnd_h

/**
 * Function which responds to drop on a widget by processing a list of URIs.
 *
 * @param[in] widget the widget that was the target of the drop
 * @param[in] uris list of URIs to process
 * @param[in] data internal data, ignore.
 * @param[in] udata user data passed when the callback was registered in
 * rox_dnd_register_full()
 * @return @c TRUE if handled successfully, @c FALSE otherwise.
 */
typedef gboolean (*rox_dnd_handle_uris)(GtkWidget *widget,
					GSList *uris,
					gpointer data,
					gpointer udata);

/**
 * Function which responds to drop on a widget by the XDS protocol.  The
 * system negotiates a temporary file to store the transfered file.
 *
 * @param[in] widget the widget that was the target of the drop
 * @param[in] path path to the file the dropped data was stored in.  Delete
 * this file when it is finished with.
 * @param[in] data internal data, ignore.
 * @param[in] udata user data passed when the callback was registered in
 * rox_dnd_register_full()
 * @return @c TRUE if handled successfully, @c FALSE otherwise.
 */
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

/**
 * Register a handler for a widget that handles URIs only.
 * @param[in] w widget
 * @param[in] f flags
 * @param[in] u callback function
 * @param[in] d additional data to pass
 */
#define rox_dnd_register_uris(w, f, u, d) \
              rox_dnd_register_full(w, f, u, NULL, d)

/**
 * Register a handler for a widget that handles URIs only.
 * @param[in] w widget
 * @param[in] f flags
 * @param[in] x callback function
 * @param[in] d additional data to pass
 */
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
 * Revision 1.3  2001/08/20 15:28:09  stephen
 * added rox_dnd_local_free
 *
 * Revision 1.2  2001/07/23 12:58:21  stephen
 * XDS needs to pass the path (or uri) to the callback
 *
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
