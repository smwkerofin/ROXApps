/*
 * error.c - display error message
 *
 * $Id: error.c,v 1.6 2005/08/14 16:07:00 stephen Exp $
 */

/**
 * @file error.c
 * @brief Display error message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include "rox.h"
#include "error.h"
#include "rox-clib.h"

GQuark rox_error_quark=0;

static GQueue *err_queue=NULL;

/**
 * Initialize error system.  This is called by rox_init_with_domain() and
 * should not be called directly.
 */
void rox_error_init(void)
{
  rox_error_quark=g_quark_from_static_string("ROX-CLib");
  err_queue=g_queue_new();
}

/**
 * Display an error message in a modal dialogue box.
 *
 * @param[in] fmt printf-style format string, followed by the arguments, to
 * generate the message
 */
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

/**
 * Report an error (via rox_error()) stored in a GError type.
 *
 * @param[in,out] err error to report
 * @param[in] del if non-zero the error is passed to g_error_free() after
 * reporting.
 */
void rox_error_report_gerror(GError *err, int del)
{
  rox_error("%s: %s", g_quark_to_string(err->domain), err->message);
  if(del)
    g_error_free(err);
}

/**
 * Push an error onto the end of the error queue.  ROX-CLib assumes
 * responsibilty for deleting the error.
 * @param[in] err error to queue
 */
void rox_error_queue(GError *err)
{
  g_queue_push_tail(err_queue, err);
}

/**
 * @return @c TRUE if the are no errors in the queue.
 */
gboolean rox_error_queue_empty(void)
{
  return g_queue_is_empty(err_queue);
}

/**
 * Return the error at the head of the error queue, leaving it on the queue.
 * @return error, or @c NULL if no errors in queue
 */
GError *rox_error_queue_peek(void)
{
  return (GError *) g_queue_peek_head(err_queue);
}

/**
 * Return the error at the head of the error queue and remove it from the
 * queue.  The caller assumes
 * responsibilty for deleting the error.
 * @return error, or @c NULL if no errors in queue
 */
GError *rox_error_queue_fetch(void)
{
  return (GError *) g_queue_pop_head(err_queue);
}

/**
 * Return the error at the end of the error queue, leaving it on the queue.
 * @return error, or @c NULL if no errors in queue
 */
GError *rox_error_queue_peek_last(void)
{
  return (GError *) g_queue_peek_tail(err_queue);
}

/**
 * Return the error at the end of the error queue and remove it from the
 * queue.  The caller assumes
 * responsibilty for deleting the error.
 * @return error, or @c NULL if no errors in queue
 */
GError *rox_error_queue_fetch_last(void)
{
  return (GError *) g_queue_pop_tail(err_queue);
}

/**
 * Delete all errors from the error queue.
 */
void rox_error_queue_flush(void)
{
  GError *err;

  do {
    err=(GError *) g_queue_pop_head(err_queue);
    if(err)
      g_error_free(err);
  } while(err);
}

/**
 * Report all errors (using rox_error_report_gerror()) in the queue in the
 * order in which they were queued.
 */
void rox_error_queue_report(void)
{
  GError *err;

  do {
    err=(GError *) g_queue_pop_head(err_queue);
    if(err)
      rox_error_report_gerror(err, TRUE);
  } while(err);
}


/*
 * $Log: error.c,v $
 * Revision 1.6  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.5  2003/10/22 17:17:33  stephen
 * Put name of program (if we have it...) in error box
 *
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
