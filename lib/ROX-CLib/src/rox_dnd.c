/*
 * rox_dnd.c - utilities for using drag & drop with ROX apps.
 *
 * $Id$
 */

#include "rox-clib.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include "rox_dnd.h"
#include "rox_path.h"

struct dnd_data {
  GdkDragContext *context;
  gint x, y;
  guint time;
  GtkSelectionData *selection_data;
  guint info;
};

typedef struct dnd_data DnDData;

struct rox_data {
  guint flags;
  rox_dnd_handle_uris uris;
  rox_dnd_handle_xds  xds;
  DnDData data;
  gpointer udata;
};

typedef struct rox_data ROXData;

enum {
	TARGET_RAW,
	TARGET_URI_LIST,
	TARGET_XDS,
	TARGET_STRING,
};

#define MAXURILEN 4096

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context,
			  gint x, gint y, guint time, gpointer data);
static void drag_data_received(GtkWidget *widget, GdkDragContext *context,
			       gint x, gint y,
			       GtkSelectionData *selection_data,
			       guint info, guint32 time, gpointer user_data);

static GdkAtom XdndDirectSave0;
static GdkAtom xa_text_plain;
static GdkAtom text_uri_list;
static GdkAtom application_octet_stream;

void rox_dnd_init(void)
{
  XdndDirectSave0 = gdk_atom_intern("XdndDirectSave0", FALSE);
  xa_text_plain = gdk_atom_intern("text/plain", FALSE);
  text_uri_list = gdk_atom_intern("text/uri-list", FALSE);
  application_octet_stream = gdk_atom_intern("application/octet-stream",
					     FALSE);
}

void rox_dnd_register_full(GtkWidget *widget,
				  guint flags,
				  rox_dnd_handle_uris uris,
				  rox_dnd_handle_xds xds,
				  gpointer udata)
{
  static GtkTargetEntry target_table[]={
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"XdndDirectSave0", 0, TARGET_XDS},
  };
  static const int ntarget=sizeof(target_table)/sizeof(target_table[0]);
  
  ROXData *rdata;
  
  gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, target_table, ntarget,
		    GDK_ACTION_COPY|GDK_ACTION_PRIVATE);

  rdata=g_new(ROXData, 1);

  rdata->flags=flags;
  rdata->uris=uris;
  rdata->xds=xds;
  rdata->udata=udata;

  gtk_signal_connect(GTK_OBJECT(widget), "drag_drop",
		     GTK_SIGNAL_FUNC(drag_drop), rdata);
  gtk_signal_connect(GTK_OBJECT(widget), "drag_data_received",
		     GTK_SIGNAL_FUNC(drag_data_received), rdata);
}

/* Is the sender willing to supply this target type? */
static gboolean provides(GdkDragContext *context, GdkAtom target)
{
  GList	    *targets = context->targets;

  while (targets && ((GdkAtom) targets->data != target))
    targets = targets->next;
  
  return targets != NULL;
}

static char *get_xds_prop(GdkDragContext *context)
{
  guchar	*prop_text;
  gint	length;

  if (gdk_property_get(context->source_window,
		       XdndDirectSave0,
		       xa_text_plain,
		       0, MAXURILEN,
		       FALSE,
		       NULL, NULL,
		       &length, &prop_text) && prop_text) {
    /* Terminate the string */
    prop_text = g_realloc(prop_text, length + 1);
    prop_text[length] = '\0';
    return prop_text;
  }

  return NULL;
}

/* Convert a list of URIs into a list of strings.
 * Lines beginning with # are skipped.
 * The text block passed in is zero terminated (after the final CRLF)
 */
static GSList *uri_list_to_gslist(char *uri_list)
{
  GSList   *list = NULL;

  while (*uri_list) {
    char	*linebreak;
    char	*uri;
    int	length;

    linebreak = strchr(uri_list, 13);

    if (!linebreak || linebreak[1] != 10) {
      rox_error("uri_list_to_gslist: Incorrect or missing line "
		      "break in text/uri-list data");
      return list;
    }
    
    length = linebreak - uri_list;

    if (length && uri_list[0] != '#') {
	uri = g_malloc(sizeof(char) * (length + 1));
	strncpy(uri, uri_list, length);
	uri[length] = 0;
	list = g_slist_append(list, uri);
      }

    uri_list = linebreak + 2;
  }

  return list;
}

/* User has tried to drop some data on us. Decide what format we would
 * like the data in.
 */
static gboolean drag_drop(GtkWidget 	  *widget,
			  GdkDragContext  *context,
			  gint            x,
			  gint            y,
			  guint           time,
			  gpointer	  data)
{
  char *leafname=NULL;
  char *path=NULL;
  GdkAtom target = GDK_NONE;

  if(provides(context, XdndDirectSave0)) {
    leafname = get_xds_prop(context);
    if (leafname) {
      target = XdndDirectSave0;
      g_dataset_set_data_full(context, "leafname", leafname, g_free);
    }
  } else if(provides(context, text_uri_list))
    target = text_uri_list;

  gtk_drag_get_data(widget, context, target, time);

  return TRUE;
}

/* We've got a list of URIs from somewhere (probably a filer window).
 * If the files are on the local machine then use the name,
 */
static void got_uri_list(GtkWidget 		*widget,
			 GdkDragContext 	*context,
			 GtkSelectionData 	*selection_data,
			 guint32             	time,
			 ROXData *rdata)
{
  GSList		*uri_list;
  char		*error = NULL;
  GSList		*next_uri;
  gboolean	send_reply = TRUE;
  char		*path=NULL, *server=NULL;
  const char		*uri;

  uri_list = uri_list_to_gslist(selection_data->data);

  if (!uri_list)
    error = "No URIs in the text/uri-list (nothing to do!)";
  else if(rdata && rdata->uris) {
    rdata->uris(widget, uri_list, NULL, rdata->udata);
  }

  if (error) {
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    rox_error("%s: %s", "got_uri_list", error);
  } else {
    gtk_drag_finish(context, TRUE, FALSE, time);    /* Success! */
  }
  
  next_uri = uri_list;
  while (next_uri) {
      g_free(next_uri->data);
      next_uri = next_uri->next;
  }
  g_slist_free(uri_list);
}

GSList *rox_dnd_filter_local(GSList *uris)
{
  GSList *filt=NULL;

  while(uris) {
    gchar *lpath;

    lpath=rox_path_get_local((const char *) uris->data);
    if(lpath)
      filt=g_slist_append(filt, g_strdup(lpath));

    uris=uris->next;
  }

  return filt;
}

/* XDS not yet implemented... */
static void got_xds(GtkWidget 		*widget,
			 GdkDragContext 	*context,
			 GtkSelectionData 	*selection_data,
			 guint32             	time,
			 ROXData *rdata)
{
  gtk_drag_finish(context, FALSE, FALSE, time);
}

/* Called when some data arrives from the remote app (which we asked for
 * in drag_drop).
 */
static void drag_data_received(GtkWidget      	*widget,
			       GdkDragContext  	*context,
			       gint            	x,
			       gint            	y,
			       GtkSelectionData *selection_data,
			       guint            info,
			       guint32          time,
			       gpointer		user_data)
{
  if (!selection_data->data) {
    /* Timeout? */
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    return;
  }

  switch(info) {
  case TARGET_XDS:
    got_xds(widget, context, selection_data, time, (ROXData *) user_data);
    break;
  case TARGET_URI_LIST:
    got_uri_list(widget, context, selection_data, time, (ROXData *) user_data);
    break;
  default:
    /*fprintf(stderr, "drag_data_received: unknown target\n");*/
    gtk_drag_finish(context, FALSE, FALSE, time);
    rox_error("drag_data_received: unknown target");
    break;
  }
}
/*
 * $Log$
 */

/* I always prefered RQ to D&D personally... */
