/*
 * $Id: balloon.c,v 1.2 2006/01/21 14:56:50 stephen Exp $
 *
 * SystemTray, a notification area applet for rox
 * Copyright (C) 2003, Andy Hanton
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* balloon.c - code for handling system tray balloon messages */

#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "applet.h"
#include "tray.h"
#include "balloon.h"

static void add_balloon(balloon *b);
static void show_message(balloon *b);

balloon_data *get_bdata(tray *t)
{
  return (balloon_data *)(t->bdata);
}

static void add_balloon(balloon *b)
{
  //fprintf(stderr, "add balloon\n");
  if (get_bdata(b->t)->message_queue == NULL && b->balloon_window == NULL)
    show_message(b);
  else
    get_bdata(b->t)->message_queue = g_slist_append(get_bdata(b->t)->message_queue, (gpointer)b);
}

char *get_window_name(Window window)
{
  Display *display;
  Atom actual_type;
  int actual_format = 1;
  unsigned long bytes_returned;
  unsigned long bytes_remaining = 1;
  unsigned char *data;
  int offset = 0;
  char *ret;
  GString *str = g_string_new("");

  display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
  while(actual_format != None && bytes_remaining != 0)
    {
      XGetWindowProperty(display,
			 window,
			 XInternAtom(display, "_NET_WM_NAME", FALSE),
			 offset,
			 1000 / 4, //in longs
			 FALSE,
			 XInternAtom(display, "UTF8_STRING", FALSE),
			 &actual_type,
			 &actual_format,
			 &bytes_returned,
			 &bytes_remaining,
			 &data);
      offset += bytes_returned / 4;
      g_string_append(str, (gchar *) data);
    }

  ret = str->str;
  g_string_free(str, FALSE);
  return ret;
}

void balloon_init(tray *t)
{
  t->bdata = g_new(balloon_data, 1);
  get_bdata(t)->partial_messages = NULL;
  get_bdata(t)->message_queue = NULL;
  get_bdata(t)->current_balloon = NULL;
}

void balloon_cleanup(tray *t)
{
  g_free(get_bdata(t));
}

void new_balloon(tray *t, XClientMessageEvent *event)
{
  //printf("start baloon message\n");
  int timeout = event->data.l[2];       // timeout in 1/1000ths ofa second
                                        // zero means no timeout 
  int size = event->data.l[3];          // length of the message string
  int id = event->data.l[4];            // ID number for the message.

  balloon *b = g_new(balloon, 1);
  b->window = event->window;
  b->length = size;
  b->id = id;
  b->text = g_malloc(size + 1);
  b->text[0] = 0;
  b->timeout = timeout;
  b->window_name = get_window_name(b->window);
  b->balloon_window = NULL;
  b->t = t;

  if (size == 0)
    add_balloon(b);
  else
    {
      //fprintf(stderr, "adding balloon for window %d to the partial list\n", b->window);
      get_bdata(b->t)->partial_messages = g_slist_append(get_bdata(t)->partial_messages, (gpointer)b);
    }
}

void cancel_balloon(tray *t, XClientMessageEvent *event)
{
  int id = event->data.l[2];
  //fprintf(stderr, "cancel baloon message\n");
  if (get_bdata(t)->current_balloon == NULL)
    return;
  //fprintf(stderr, "id1: %d, id2: %d\n", get_bdata(t)->current_balloon->id, id);
  if (get_bdata(t)->current_balloon->window == event->window && get_bdata(t)->current_balloon->id == id)
    balloon_done(get_bdata(t)->current_balloon);
  else
    {
      GSList *it = get_bdata(t)->message_queue;
      balloon *b;

      while (it) {
	b = (balloon *)it->data;
	if (event->window == b->window && b->id == id) {
	  get_bdata(b->t)->message_queue = g_slist_remove(get_bdata(t)->partial_messages, (gpointer)it->data);
	  return;
	}
      }
    }      
}

gboolean balloon_done(gpointer data)
{
  balloon *b = (balloon *)data;
  tray *t = b->t;

  if (b->balloon_window)
    gtk_widget_destroy(b->balloon_window);

  get_bdata(b->t)->current_balloon = NULL;
  g_source_remove(b->timeout_id);
  b->use_count--;

  if (b->use_count == 0)
    g_free(b);

  if (get_bdata(t)->message_queue)
    {
      b = get_bdata(t)->message_queue->data;
      //fprintf(stderr, "message queue\n");
      get_bdata(t)->message_queue = g_slist_delete_link(get_bdata(t)->message_queue, get_bdata(t)->message_queue);
      show_message(b);
    }

  return FALSE;
}

extern PanelLocation location;

static void show_message(balloon *b)
{
  GtkWidget *label;
  GtkWidget *eventbox;
  gint top_x, top_y;
  gint top_width, top_height;
  gint message_width, message_height;
  gint message_x, message_y;
  gint icon_width, icon_height;
  gint icon_x, icon_y;
  PangoLayout *layout;
  char *text;
  GdkWindow *w;

  get_bdata(b->t)->current_balloon = b;
  b->use_count = 2;

  //fprintf(stderr, "balloon text: %s\n", b->text);

  w = gdk_window_lookup(b->window);
  gdk_drawable_get_size(GDK_DRAWABLE(w), &icon_width, &icon_height);
  gdk_window_get_origin(GDK_DRAWABLE(w), &icon_x, &icon_y);

  gdk_window_get_origin(b->t->toplevel->window, &top_x, &top_y);
  gtk_window_get_size(GTK_WINDOW(b->t->toplevel), &top_width, &top_height);

  b->balloon_window = gtk_window_new(GTK_WINDOW_POPUP);

  eventbox = gtk_event_box_new();
  g_signal_connect_swapped(eventbox, "button_release_event", GTK_SIGNAL_FUNC(balloon_done), b);
  gtk_widget_add_events(eventbox, GDK_BUTTON_RELEASE_MASK);
  gtk_container_add(GTK_CONTAINER(b->balloon_window), eventbox);

  text = g_strconcat(b->window_name, ":\n", b->text, NULL);
  label = gtk_label_new(text);
  g_free(text);
  gtk_label_set_line_wrap(GTK_LABEL(label), True);
  gtk_container_add(GTK_CONTAINER(eventbox), label);

  layout = gtk_label_get_layout(GTK_LABEL(label));
  pango_layout_get_pixel_size(layout, &message_width, &message_height);

  switch (b->t->location) 
    {
    case PANEL_BOTTOM:
      message_x = top_x + top_width / 2 - message_width / 2;
      message_y = top_y - message_height;
      break;
    case PANEL_TOP:
      message_x = top_x + top_width / 2  - message_width / 2;
      message_y = top_y + top_height;
      break;
    case PANEL_RIGHT:
      message_x = top_x - message_width;
      message_y = top_y + top_height / 2 - message_height / 2;
      break;
    case PANEL_LEFT:
      message_x = top_x + top_width;
      message_y = top_y + top_height / 2 - message_height / 2;
      break;
    }

  if (message_x < 0)
    message_x = 0;
  else if (message_x + message_width > gdk_screen_width())
    message_x  = gdk_screen_width() - message_width;

  if (message_y < 0)
    message_y = 0;
  else if (message_y + message_height > gdk_screen_height())
    message_y  = gdk_screen_height() - message_height;

  gtk_window_move(GTK_WINDOW(b->balloon_window), message_x, message_y);

  gtk_widget_set_name (b->balloon_window, "gtk-tooltips");

  gtk_widget_show_all(b->balloon_window);

  if (b->timeout != 0)
    b->timeout_id = g_timeout_add(b->timeout,
				  &balloon_done,
				  (gpointer)b);
}

void add_balloon_data(tray *t, XClientMessageEvent *event)
{
  GSList *it = get_bdata(t)->partial_messages;
  balloon *b = NULL;
  //fprintf(stderr, "looking for: %d\n", event->window);
  while (it)
    {
      b = (balloon *)it->data;
      //fprintf(stderr, "iterating over messages. window: %d\n", b->window);
      if (b == NULL)
	{
	  fprintf(stderr, "null balloon in list\n");
	  it = it->next;
	  continue;
	}
      if (event->window == b->window)
	{
	  int bytes_to_copy = b->length - strlen(b->text);
	  int old_len = strlen(b->text);
	  gboolean finished = FALSE;

	  if (bytes_to_copy > 20)
	    bytes_to_copy = 20;
	  else
	    finished = TRUE;

	  memcpy(b->text + old_len, event->data.b, bytes_to_copy);
	  b->text[old_len + bytes_to_copy] = 0;

	  if (finished)
	    {
	      get_bdata(t)->partial_messages = g_slist_remove(get_bdata(t)->partial_messages, (gpointer)b);
	      add_balloon(b);
	    }
	  return;
	}
      it = it->next;
    }
  fprintf(stderr, "message not found in list\n");
}
