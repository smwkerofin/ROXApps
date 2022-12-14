/* GTK - The GIMP Toolkit
 * Copyright (C) 1991-the ROX-Filer team.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Note: This file is formatted like the Gtk+ sources, as it is/was hoped
 * to include it in Gtk+ at some point.
 */

/**
 * @file gtksavebox.c
 * @brief Drag and drop saving widget for GTK+
 *
 * This version has been altered for inclusion in ROX-CLib.  If it makes it
 * into Gtk+ then it will be removed from ROX-CLib.
 *
 * Behaviour:
 *
 * - Clicking Save or pressing Return:
 * 	- Emits 'save_to_file',
 * 	- Emits 'saved_to_uri' (with the same pathname),
 * 	- Destroys the widget.
 *   
 * - Clicking Cancel or pressing Escape:
 * 	- Destroys the widget.
 *
 * - Dragging the data somewhere:
 *      - Will either emit 'save_to_file' or get the selection,
 *      - Emits 'saved_to_uri' (possibly with a @c NULL URI),
 *      - Destroys the widget.
 *
 * - Clicking Discard:
 *	- Emits 'saved_to_uri' with a @c NULL URI,
 *	- Destroys the widget.
 *
 * To clarify: 'saved_to_uri' indicates that the save was successful. A
 * @c NULL URI just means that the data was saved to another application rather
 * than a fixed address. Data should only be marked unmodified when
 * saved_to_uri is called with a non-<code>NULL</code> URI.
 *
 * Discard is a bit like a successful save to a null device. The data should
 * be discarded when saved_to_uri is called, whatever URI is set to.
 *
 * 
 * Signals:
 * 
 * gint save_to_file (GtkSavebox *savebox, const gchar *pathname) 
 * 	Save the data to disk using this pathname. Return @c GTK_XDS_SAVED
 * 	on success, or @c GTK_XDS_SAVE_ERROR on failure (and report the error
 * 	to the user somehow). DO NOT mark the data unmodified or change
 * 	the pathname for the file - this might be a scrap file transfer.
 *
 * void saved_to_uri (GtkSavebox *savebox, const gchar *uri)
 *	The data is saved. If 'uri' is non-<code>NULL</code>, mark the file as unmodified
 *	and update the pathname/uri for the file to the one given.
 *
 * @author Thomas Leonard
 * @version $Id: gtksavebox.c,v 1.4 2005/08/21 13:06:38 stephen Exp $ based on
 * gtksavebox.c,v 1.28 2002/05/12 16:28:42 tal197 from ROX-Filer 1.3.5
 */

/*
 * This version has been altered for inclusion in ROX-CLib.  If it makes it
 * into Gtk+ then it will be removed from ROX-CLib.
 *
 * This is the GTK 2.0 version, taken from ROX-Filer 1.3.5:
 * gtksavebox.c,v 1.28 2002/05/12 16:28:42 tal197
 */

#include "rox-clib.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "gdk/gdkkeysyms.h"

#include "gtksavebox.h"
#include "gtk/gtkwidget.h"
#include "gtk/gtkalignment.h"
#include "gtk/gtkdnd.h"
#include "gtk/gtkbutton.h"
#include "gtk/gtksignal.h"
#include "gtk/gtkhbox.h"
#include "gtk/gtkhbbox.h"
#include "gtk/gtkeventbox.h"
#include "gtk/gtkentry.h"
#include "gtk/gtkmessagedialog.h"
#include "gtk/gtkhseparator.h"
#include "gtk/gtkvbox.h"
#include "gtk/gtkdialog.h"
#include "gtk/gtklabel.h"
#include "gtk/gtkimage.h"
#include "gtk/gtkstock.h"

#include "global.h"
#include "rox_path.h"

enum {
  PROP_0,
  PROP_HAS_DISCARD
};

enum
{
  SAVE_TO_FILE,
  SAVED_TO_URI,

  LAST_SIGNAL
};

static gpointer parent_class;
static guint savebox_signals[LAST_SIGNAL];

/* Longest possible XdndDirectSave0 property value */
#define XDS_MAXURILEN 4096

static GdkAtom XdndDirectSave;
static GdkAtom text_plain;
static GdkAtom xa_string;

static void gtk_savebox_class_init (GtkSaveboxClass   *klass);
static void gtk_savebox_init       (GtkSavebox	      *savebox);
static void button_press_over_icon (GtkWidget	      *drag_box,
				    GdkEventButton    *event,
				    GtkSavebox	      *savebox);
static void drag_data_get	   (GtkWidget	      *widget,
				    GdkDragContext    *context,
				    GtkSelectionData  *selection_data,
				    guint	      info,
				    guint32	      time);
static guchar *read_xds_property   (GdkDragContext    *context,
				    gboolean	      delete);
static void write_xds_property	   (GdkDragContext    *context,
				    const guchar      *value);
static void drag_end 		   (GtkWidget 	      *widget,
				    GdkDragContext    *context);
static void gtk_savebox_response   (GtkDialog	      *savebox,
				    gint	      response);
static void discard_clicked	   (GtkWidget	      *button,
				    GtkWidget	      *savebox);
static void do_save		   (GtkSavebox	      *savebox);
static void gtk_savebox_set_property (GObject         *object,
				      guint           prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec);
static void gtk_savebox_get_property (GObject	      *object,
				      guint           prop_id,
				      GValue          *value,
				      GParamSpec      *pspec);

static GtkWidget *button_new_mixed(const char *stock, const char *message);

void
marshal_INT__STRING (GClosure     *closure,
                     GValue       *return_value,
		     guint         n_param_values,
		     const GValue *param_values,
		     gpointer      invocation_hint,
		     gpointer      marshal_data);

/** @return type code for GtkSavebox */
GType
gtk_savebox_get_type (void)
{
  static GType my_type = 0;

  if (!my_type)
    {
      static const GTypeInfo info =
      {
	sizeof (GtkSaveboxClass),
	NULL,			/* base_init */
	NULL,			/* base_finalise */
	(GClassInitFunc) gtk_savebox_class_init,
	NULL,			/* class_finalise */
	NULL,			/* class_data */
	sizeof(GtkSavebox),
	0,			/* n_preallocs */
	(GInstanceInitFunc) gtk_savebox_init
      };

      my_type = g_type_register_static(GTK_TYPE_DIALOG, "GtkSavebox", &info, 0);
    }

  return my_type;
}

static void
gtk_savebox_class_init (GtkSaveboxClass *class)
{
  GObjectClass	 *object_class;
  GtkDialogClass *dialog = (GtkDialogClass *) class;
  
  XdndDirectSave = gdk_atom_intern ("XdndDirectSave0", FALSE);
  text_plain = gdk_atom_intern ("text/plain", FALSE);
  xa_string = gdk_atom_intern ("STRING", FALSE);

  parent_class = g_type_class_peek_parent (class);

  class->saved_to_uri = NULL;
  class->save_to_file = NULL;
  dialog->response = gtk_savebox_response;

  object_class = G_OBJECT_CLASS(class);

  savebox_signals[SAVE_TO_FILE] = g_signal_new(
					"save_to_file",
					G_TYPE_FROM_CLASS(object_class),
					G_SIGNAL_RUN_LAST,
					G_STRUCT_OFFSET(GtkSaveboxClass,
							save_to_file),
					NULL, NULL,
					marshal_INT__STRING,
					G_TYPE_INT, 1,
					G_TYPE_STRING);

  savebox_signals[SAVED_TO_URI] = g_signal_new(
					"saved_to_uri",
					G_TYPE_FROM_CLASS(object_class),
					G_SIGNAL_RUN_LAST,
					G_STRUCT_OFFSET(GtkSaveboxClass,
							saved_to_uri),
					NULL, NULL,
					g_cclosure_marshal_VOID__STRING,
					G_TYPE_NONE, 1,
					G_TYPE_STRING);

  object_class->set_property = gtk_savebox_set_property;
  object_class->get_property = gtk_savebox_get_property;

  g_object_class_install_property(object_class, PROP_HAS_DISCARD,
                                  g_param_spec_boolean("has_discard",
					 _("Has Discard"),
					 _("The dialog has a Discard button"),
					 TRUE,
					 G_PARAM_READWRITE));
}

static void
gtk_savebox_init (GtkSavebox *savebox)
{
  GtkWidget *alignment, *button;
  GtkDialog *dialog = (GtkDialog *) savebox;
  GtkTargetEntry targets[] = { {"XdndDirectSave0", 0, GTK_TARGET_XDS} };

  gtk_dialog_set_has_separator (dialog, FALSE);

  savebox->targets = gtk_target_list_new (targets,
					  sizeof (targets) / sizeof (*targets));
  savebox->icon = NULL;

  gtk_window_set_title (GTK_WINDOW (savebox), _("Save As:"));
  gtk_window_set_position (GTK_WINDOW (savebox), GTK_WIN_POS_MOUSE);
  gtk_window_set_wmclass (GTK_WINDOW (savebox), "savebox", "Savebox");

  alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), alignment, TRUE, TRUE, 0);

  savebox->drag_box = gtk_event_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (savebox->drag_box), 4);
  gtk_widget_add_events (savebox->drag_box, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (savebox->drag_box, "button_press_event",
		      G_CALLBACK (button_press_over_icon), savebox);
  g_signal_connect (savebox, "drag_end",
		      G_CALLBACK (drag_end), savebox);
  g_signal_connect (savebox, "drag_data_get",
		      G_CALLBACK (drag_data_get), savebox);
  gtk_container_add (GTK_CONTAINER (alignment), savebox->drag_box);

  savebox->entry = gtk_entry_new ();
  g_signal_connect_swapped (savebox->entry, "activate",
			     G_CALLBACK (do_save), savebox);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), savebox->entry, FALSE, TRUE, 4);
  
  gtk_widget_show_all (dialog->vbox);
  gtk_widget_grab_focus (savebox->entry);

  savebox->discard_area = gtk_hbutton_box_new();
  
  button = button_new_mixed (GTK_STOCK_DELETE, "_Discard");
  gtk_box_pack_start (GTK_BOX (savebox->discard_area), button, FALSE, TRUE, 2);
  g_signal_connect (button, "clicked", G_CALLBACK (discard_clicked), savebox);
  GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

  gtk_box_pack_end (GTK_BOX (dialog->vbox), savebox->discard_area,
		      FALSE, TRUE, 0);
  gtk_box_reorder_child (GTK_BOX (dialog->vbox), savebox->discard_area, 0);
}

/**
 * Create and return a new GtkSavebox widget.
 *
 * @param[in] action Label to use in the Save button
 * @return pointer to the widget
 */
GtkWidget*
gtk_savebox_new (const gchar *action)
{
  GtkWidget *button;
  GtkDialog *dialog;
  GList	    *list, *next;

  if(!action)
    action=_("Save");
  
  dialog = GTK_DIALOG (gtk_widget_new (gtk_savebox_get_type(), NULL));

  gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  button = button_new_mixed (GTK_STOCK_SAVE, action);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_show (button);
  gtk_dialog_add_action_widget (dialog, button, GTK_RESPONSE_OK);

  gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

  list = gtk_container_get_children (GTK_CONTAINER (dialog->action_area));
  for (next = list; next; next = next->next)
    GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET(next->data), GTK_CAN_FOCUS);
  g_list_free(list);

  return GTK_WIDGET(dialog);
}

/**
 * Set the icon used for dragging the file.
 *
 * @param[in,out] savebox Save box widget
 * @param[in] pixbuf image to use for icon
 */
void
gtk_savebox_set_icon (GtkSavebox *savebox, GdkPixbuf *pixbuf)
{
  g_return_if_fail (savebox != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (savebox));
  g_return_if_fail (pixbuf != NULL);

  if (savebox->icon)
    gtk_image_set_from_pixbuf (GTK_IMAGE (savebox->icon), pixbuf);
  else
    {
      savebox->icon = gtk_image_new_from_pixbuf (pixbuf);
      gtk_container_add (GTK_CONTAINER (savebox->drag_box), savebox->icon);
      gtk_widget_show(savebox->icon);
    }
}

/**
 * Set the initial path to show in the widget.
 *
 * @param[in,out] savebox Save box widget
 * @param[in] pathname path to use
 */
void
gtk_savebox_set_pathname (GtkSavebox *savebox, const gchar *pathname)
{
  const gchar *slash, *dot;
  gint	leaf;
  
  g_return_if_fail (savebox != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (savebox));
  g_return_if_fail (pathname != NULL);

  gtk_entry_set_text (GTK_ENTRY (savebox->entry), pathname);

  slash = strrchr (pathname, '/');
  
  leaf = slash ? g_utf8_pointer_to_offset(pathname, slash) + 1 : 0;
  dot = strchr(pathname + leaf, '.');
  
  gtk_editable_select_region (GTK_EDITABLE (savebox->entry), leaf,
			      dot ? g_utf8_pointer_to_offset (pathname, dot)
			      	  : -1);
}

/**
 * Set whether the discard area is show.  Normally this is hidden  but
 * contains a single "Discard" button.
 *
 * @param[in,out] savebox Save box widget
 * @param[in] setting non-zero to show discard area
 */
void
gtk_savebox_set_has_discard (GtkSavebox *savebox, gboolean setting)
{
  g_return_if_fail(GTK_IS_SAVEBOX(savebox));
  
  if (setting)
    gtk_widget_show_all (savebox->discard_area);
  else
    gtk_widget_hide (savebox->discard_area);
}

/**
 * Return the extension area, a vbox just above the buttons where
 * additional widgets may be placed.
 *
 * @param[in,out] savebox Save box widget
 */
GtkWidget* gtk_savebox_get_extension_area(GtkSavebox *savebox)
{
  g_return_val_if_fail(GTK_IS_SAVEBOX(savebox), NULL);
  
  if(!savebox->extend) {
    savebox->extend=gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(savebox)->vbox), savebox->extend,
		       TRUE, TRUE, 0);
  }

  return savebox->extend;
}

/* Local functions */

static void
button_press_over_icon (GtkWidget *drag_box, GdkEventButton *event,
			GtkSavebox *savebox)
{
  GdkDragContext  *context;
  const gchar	  *uri = NULL, *leafname;

  g_return_if_fail (savebox != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (savebox));
  g_return_if_fail (event != NULL);
  g_return_if_fail (savebox->icon != NULL);

  savebox->using_xds = FALSE;
  savebox->data_sent = FALSE;
  context = gtk_drag_begin (GTK_WIDGET (savebox),
			    savebox->targets, GDK_ACTION_COPY,
			    event->button, (GdkEvent *) event);

  uri = gtk_entry_get_text (GTK_ENTRY (savebox->entry));
  if (uri && *uri)
    leafname = g_basename (uri);
  else
    leafname = _("Unnamed");
  
  write_xds_property (context, leafname);

  gtk_drag_set_icon_pixbuf (context,
			    gtk_image_get_pixbuf (GTK_IMAGE (savebox->icon)),
			    event->x, event->y);

}

static void
drag_data_get (GtkWidget	*widget,
	       GdkDragContext   *context,
	       GtkSelectionData *selection_data,
               guint            info,
               guint32          time)
{
  GtkSavebox  *savebox;
  guchar      to_send = 'E';
  gchar	      *uri;
  const gchar *pathname;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (widget));
  g_return_if_fail (context != NULL);
  g_return_if_fail (selection_data != NULL);

  savebox = GTK_SAVEBOX (widget);

  /* We're only concerned with the XDS protocol. Responding to other requests
   * (including application/octet-stream) is the job of the application.
   */
  if (info != GTK_TARGET_XDS)
  {
    /* Assume that the data will be/has been sent */
    savebox->data_sent = TRUE;
    return;
  }

  uri = read_xds_property (context, FALSE);

  if (uri)
  {
    gint result = GTK_XDS_NO_HANDLER;

    pathname = rox_path_get_local (uri);
    if (!pathname)
      to_send = 'F';    /* Not on the local machine */
    else
      {
	g_signal_emit (widget, savebox_signals[SAVE_TO_FILE], 0,
		       pathname, &result);

	if (result == GTK_XDS_SAVED)
	  {
	    savebox->data_sent = TRUE;
	    to_send = 'S';
	  }
	else if (result != GTK_XDS_SAVE_ERROR)
	  g_warning ("No handler for saving to a file.\n");

	g_free (uri);
      }
  }
  else
  {
    g_warning (_("Remote application wants to use Direct Save, but I can't "
	       "read the XdndDirectSave0 (type text/plain) property.\n"));
  }

  if (to_send != 'E')
    savebox->using_xds = TRUE;
  gtk_selection_data_set (selection_data, xa_string, 8, &to_send, 1);
}

static guchar *
read_xds_property (GdkDragContext *context, gboolean delete)
{
  guchar  *prop_text;
  guint	  length;
  guchar  *retval = NULL;
  
  g_return_val_if_fail (context != NULL, NULL);

  if (gdk_property_get (context->source_window, XdndDirectSave, text_plain,
		       0, XDS_MAXURILEN, delete,
		       NULL, NULL, &length, &prop_text)
	    && prop_text)
  {
    /* Terminate the string */
    retval = g_realloc (prop_text, length + 1);
    retval[length] = '\0';
  }

  return retval;
}

static void
write_xds_property (GdkDragContext *context, const guchar *value)
{
  if (value)
    {
      gdk_property_change (context->source_window, XdndDirectSave,
			   text_plain, 8, GDK_PROP_MODE_REPLACE,
			   value, strlen (value));
    }
  else
    gdk_property_delete (context->source_window, XdndDirectSave);
}

static void drag_end (GtkWidget *widget, GdkDragContext *context)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (widget));
  g_return_if_fail (context != NULL);

  if (GTK_SAVEBOX (widget)->using_xds)
    {
      guchar  *uri;
      uri = read_xds_property (context, TRUE);

      if (uri)
	{
	  const gchar  *path;

	  path = rox_path_get_local (uri);
	  
	  g_signal_emit (widget, savebox_signals[SAVED_TO_URI], 0,
			 path ? path : (const gchar *) uri);
	  g_free(uri);

	  gtk_widget_destroy (widget);

	  return;
	}
    }
  else
      write_xds_property (context, NULL);

  if (GTK_SAVEBOX (widget)->data_sent)
    {
      g_signal_emit (widget, savebox_signals[SAVED_TO_URI], 0, NULL);
      gtk_widget_destroy (widget);
    }
}

static void discard_clicked (GtkWidget *button, GtkWidget *savebox)
{
  g_signal_emit (savebox, savebox_signals[SAVED_TO_URI], 0, NULL);
  gtk_widget_destroy (savebox);
}

/* User has clicked Save or pressed Return... */
static void do_save (GtkSavebox *savebox)
{
  gint	result = GTK_XDS_NO_HANDLER;
  const gchar  *pathname, *uri;

  g_return_if_fail (savebox != NULL);
  g_return_if_fail (GTK_IS_SAVEBOX (savebox));

  uri = gtk_entry_get_text (GTK_ENTRY (savebox->entry));
  pathname = rox_path_get_local (uri);

  if (!pathname)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (savebox),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			_("Drag the icon to a directory viewer\n"
				"(or enter a full pathname)"));

      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      return;
    }

  g_signal_emit (savebox, savebox_signals[SAVE_TO_FILE], 0, pathname, &result);

  if (result == GTK_XDS_SAVED)
    {
      g_signal_emit (savebox, savebox_signals[SAVED_TO_URI], 0, pathname);

      gtk_widget_destroy (GTK_WIDGET (savebox));
    }
  else if (result == GTK_XDS_NO_HANDLER)
    g_warning ("No handler for saving to a file.\n");
}

static void
gtk_savebox_response (GtkDialog *savebox, gint response)
{
	if (response == GTK_RESPONSE_OK)
	{
		do_save(GTK_SAVEBOX(savebox));
		return;
	}
	else if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy (GTK_WIDGET (savebox));
}

static void 
gtk_savebox_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkSavebox *savebox;
  
  savebox = GTK_SAVEBOX (object);

  switch (prop_id)
    {
    case PROP_HAS_DISCARD:
      gtk_savebox_set_has_discard (GTK_SAVEBOX(object),
		      		   g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_savebox_get_property (GObject     *object,
                          guint        prop_id,
                          GValue      *value,
                          GParamSpec  *pspec)
{
  GtkSavebox *savebox;
  
  savebox = GTK_SAVEBOX (object);
  
  switch (prop_id)
    {
    case PROP_HAS_DISCARD:
      g_value_set_boolean (value, GTK_WIDGET_VISIBLE(savebox->discard_area));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
marshal_INT__STRING (GClosure     *closure,
                     GValue       *return_value,
		     guint         n_param_values,
		     const GValue *param_values,
		     gpointer      invocation_hint,
		     gpointer      marshal_data)
{
  typedef gint (*GMarshalFunc_INT__STRING) (gpointer     data1,
                                            gpointer     arg_1,
                                            gpointer     data2);
  register GMarshalFunc_INT__STRING callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gint v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_INT__STRING)
	  	(marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1, param_values[1].data[0].v_pointer, data2);

  g_value_set_int (return_value, v_return);
}

static GtkWidget *button_new_mixed(const char *stock, const char *message)
{
	GtkWidget *button, *align, *image, *hbox, *label;
	
	button = gtk_button_new();
	label = gtk_label_new_with_mnemonic(message);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);

	image = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new(FALSE, 2);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_widget_show_all(align);

	return button;
}

/*
 * $Log: gtksavebox.c,v $
 * Revision 1.4  2005/08/21 13:06:38  stephen
 * Added doxygen comments
 *
 * Revision 1.3  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 */
