/*
 * error.c - display error message
 *
 * $Id: error.c,v 1.2 2001/07/17 14:43:15 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include "error.h"

#ifndef GTK2
static GtkWidget *errwin=NULL;
static GtkWidget *errmess=NULL;

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

static void dismiss_error(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(errwin);
}

void rox_error(const char *fmt, ...)
{
  va_list list;
  gchar *mess;

  if(!errwin) {
    /* Need to create it first */
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    
    errwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(errwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     errwin);
    gtk_window_set_title(GTK_WINDOW(errwin), "Error!");
    gtk_window_set_position(GTK_WINDOW(errwin), GTK_WIN_POS_CENTER);

    vbox=GTK_DIALOG(errwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

    errmess=gtk_label_new("");
    gtk_widget_show(errmess);
    gtk_box_pack_start(GTK_BOX(hbox), errmess, TRUE, TRUE, 2);
    
    hbox=GTK_DIALOG(errwin)->action_area;

    but=gtk_button_new_with_label("Ok");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, TRUE, TRUE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(dismiss_error), errwin);
  }
  
  va_start(list, fmt);
  mess=g_strdup_vprintf(fmt, list);
  va_end(list);

  gtk_label_set_text(GTK_LABEL(errmess), mess);
  gtk_window_set_modal(GTK_WINDOW(errwin), TRUE);
  gtk_widget_show(errwin);
  g_free(mess);
}

#else

void rox_error(const char *fmt, ...)
{
  va_list list;
  gchar *mess;
  GtkWidget *errwin;
  
  va_start(list, fmt);
  mess=g_strdup_vprintf(fmt, list);
  va_end(list);

  errwin=gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK, "%s", mess);

  (void) gtk_dialog_run(GTK_DIALOG(errwin));

  gtk_widget_destroy(errwin);
  g_free(mess);
}
#endif

/*
 * $Log: error.c,v $
 * Revision 1.2  2001/07/17 14:43:15  stephen
 * Changed name of call
 *
 * Revision 1.1.1.1  2001/05/29 14:09:58  stephen
 * Initial version of the library
 *
 */
