/*
 * error.c - display error message
 *
 * $Id: error.c,v 1.4 2003/03/05 15:31:23 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include "rox.h"
#include "error.h"
#include "rox-clib.h"

void rox_error(const char *fmt, ...)
{
  va_list list;
  gchar *mess;
  GtkWidget *errwin;
  const gchar *program=rox_get_program_name();
  gchar *title=NULL;
  
  va_start(list, fmt);
  mess=g_strdup_vprintf(fmt, list);
  va_end(list);

  errwin=gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK, "%s", mess);
  if(program) {
    title=g_strdup_printf(_("Error from %s"), program);
    gtk_window_set_title(GTK_WINDOW(errwin), title);
    g_free(title);
  }

  (void) gtk_dialog_run(GTK_DIALOG(errwin));

  gtk_widget_destroy(errwin);
  g_free(mess);
}

/*
 * $Log: error.c,v $
 * Revision 1.4  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
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
