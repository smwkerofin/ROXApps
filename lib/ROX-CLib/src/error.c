/*
 * error.c - display error message
 *
 * $Id: error.c,v 1.3 2002/04/29 08:17:24 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include "error.h"


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

/*
 * $Log: error.c,v $
 * Revision 1.3  2002/04/29 08:17:24  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.2  2001/07/17 14:43:15  stephen
 * Changed name of call
 *
 * Revision 1.1.1.1  2001/05/29 14:09:58  stephen
 * Initial version of the library
 *
 */
