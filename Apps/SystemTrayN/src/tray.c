/*
 * $Id: tray.c,v 1.2 2006/08/24 21:20:10 stephen Exp $
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

/* tray.c - code for the system tray window */

#include "config.h"

#include "main.h"
#include "applet.h"
#include "tray.h"
#include "balloon.h"


GdkAtom system_tray_g_atom, selection_g_atom, system_tray_data_g_atom;
Atom system_tray_atom, selection_atom, system_tray_data_atom;

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static int exiting = 0;

static ROXOption o_min_size;

static GList *trays=NULL;

static void show_selection_error();
static GdkFilterReturn client_event_filter(XEvent *xevent,
				    GdkEvent *event,
				    gpointer data);
static void manager_selection_announce(tray *t);
static void manager_selection_acquire(tray *t);
static void manager_selection_release(tray *t);
static gboolean handle_lost_selection(GtkWidget *widget,
			       GdkEventSelection *event,
			       gpointer user_data);
static gboolean handle_remove(GtkSocket *socket, gpointer user_data);
static void check_size(tray *t);
static void remove_item(GtkWidget *widget, gpointer data);
static void option_changed(void);

void tray_init(void)
{
  rox_option_add_int(&o_min_size, "min_size", 16);

  rox_option_add_notify(option_changed);
}

GtkWidget *tray_new(GtkWidget *top, ROXPanelLocation side)
{
  GtkWidget *box;
  tray *t;

  t = g_new(tray, 1);
  t->toplevel = top;
  t->location = side;
  t->icon_count=0;

  if (t->location == PANEL_UNKNOWN)
    t->location = PANEL_BOTTOM;

  switch (t->location) 
    {
    case PANEL_TOP:
    case PANEL_BOTTOM:
      t->box = gtk_hbox_new(FALSE, 0);
      t->gliph = GTK_SEPARATOR(gtk_vseparator_new());
      t->ishoriz=TRUE;
      break;
    case PANEL_RIGHT:
    case PANEL_LEFT:
      t->box = gtk_vbox_new(FALSE, 0);
      t->gliph = GTK_SEPARATOR(gtk_hseparator_new());
      t->ishoriz=TRUE;
      break;
    }  

  gtk_widget_set_size_request(t->box, o_min_size.int_value,
			      o_min_size.int_value);
  gtk_box_pack_start (GTK_BOX(t->box), GTK_WIDGET(t->gliph), TRUE, TRUE, 0);
  g_object_set_data(G_OBJECT(t->box), "tray", t);
  check_size(t);

  gtk_main_iteration_do(FALSE);//FIME: do I need this?
  manager_selection_acquire(t);
  gtk_widget_show_all(t->box);

  balloon_init(t);
  trays=g_list_append(trays, t);
  
  return t->box;
}

void tray_destroy(GtkWidget *tray_widget)
{
  tray *t = g_object_get_data(G_OBJECT(tray_widget), "tray");
  balloon_cleanup(t);
  gtk_container_foreach(GTK_CONTAINER(t->box),
			&remove_item,
			(gpointer)t);

  manager_selection_release(t);
  trays=g_list_remove(trays, t);
}

static void option_changed(void)
{
  GList *p;

  for(p=trays; p; p=g_list_next(p)) {
    check_size((tray *) p->data);
  }
}

static void show_selection_error()
{
  GtkWidget *dialog;
  const char *text = _("Another system tray is already running.");
  dialog = gtk_message_dialog_new(NULL,
				  0,
				  GTK_MESSAGE_ERROR,
				  GTK_BUTTONS_OK,
				  text);
  gtk_dialog_run(GTK_DIALOG(dialog));
}

static GdkFilterReturn client_event_filter(XEvent *xevent,
			  GdkEvent *event,
			  gpointer data)
{
  Display *display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    tray *t=(tray *)data;

  if (xevent->xclient.message_type == system_tray_atom) {
    
    if (xevent->xclient.data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      GtkWidget *socket;
      GtkWidget *label;

      socket = gtk_socket_new();

      gtk_box_pack_start_defaults(GTK_BOX(t->box), socket);
      gtk_widget_show(socket);
      gtk_socket_add_id(GTK_SOCKET(socket), xevent->xclient.data.l[2]);
      g_signal_connect(socket, "plug_removed", 
		       GTK_SIGNAL_FUNC(handle_remove), 
		       data);

      t->icon_count++;
      check_size(t);
      
    } else if (xevent->xclient.data.l[1] == SYSTEM_TRAY_BEGIN_MESSAGE) {
      new_balloon(t, &xevent->xclient);
      
    } else if (xevent->xclient.data.l[1] == SYSTEM_TRAY_CANCEL_MESSAGE) {
      cancel_balloon(t, &xevent->xclient);
      
    }
    return TRUE;
    
  } else if (xevent && xevent->xclient.message_type==system_tray_data_atom) {
    add_balloon_data(t, &xevent->xclient);
    
    return TRUE;
  }

  return GDK_FILTER_REMOVE;
}

static void manager_selection_announce(tray *t)
{
  XClientMessageEvent xev;
  int timestamp;
  Display *display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

  timestamp = gdk_x11_get_server_time (t->manager_window->window);

  xev.type = ClientMessage;
  xev.window = GDK_ROOT_WINDOW();
  xev.message_type = XInternAtom (display, "MANAGER", False);
  
  xev.format = 32;
  xev.data.l[0] = timestamp;
  xev.data.l[1] = selection_atom;
  xev.data.l[2] = GDK_WINDOW_XWINDOW (t->manager_window->window);
  xev.data.l[3] = 0; 
  xev.data.l[4] = 0; 
  
  XSendEvent (display,
	      GDK_ROOT_WINDOW(),
	      False, StructureNotifyMask, (XEvent *)&xev);
}

static void manager_selection_acquire(tray *t)
{
  Display *display;
  Window selection_window;

  t->manager_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_realize(t->manager_window);

  g_signal_connect(t->manager_window, "selection-clear-event", 
		   GTK_SIGNAL_FUNC(handle_lost_selection), 
		   (gpointer)t);

  gtk_widget_add_events(t->manager_window, GDK_ALL_EVENTS_MASK);
  display = GDK_DISPLAY();

  system_tray_data_g_atom = gdk_atom_intern("_NET_SYSTEM_TRAY_MESSAGE_DATA", FALSE);

  system_tray_data_atom = gdk_x11_atom_to_xatom(system_tray_data_g_atom);
  system_tray_g_atom = gdk_atom_intern("_NET_SYSTEM_TRAY_OPCODE", FALSE);
  system_tray_atom = gdk_x11_atom_to_xatom(system_tray_g_atom);
  gdk_add_client_message_filter(system_tray_g_atom,
				(GdkFilterFunc)client_event_filter,
				(gpointer)t);
  gdk_add_client_message_filter(system_tray_data_g_atom,
				(GdkFilterFunc)client_event_filter,
				(gpointer)t);
  selection_g_atom = gdk_atom_intern("_NET_SYSTEM_TRAY_S0", FALSE);
  selection_atom = gdk_x11_atom_to_xatom(selection_g_atom);
  selection_window = XGetSelectionOwner(display, selection_atom);
  if (selection_window == None)
    {
      gtk_selection_owner_set(t->manager_window, selection_g_atom, CurrentTime);
      if (XGetSelectionOwner(display, selection_atom) 
	  != GDK_WINDOW_XID(t->manager_window->window))
	{
	  show_selection_error();
	  exit(0);
	}
      else
	{
	  manager_selection_announce(t);
	}
    }
  else
    {
      show_selection_error();
      exit(0);
    }
}

static void manager_selection_release(tray *t)
{
  gint timestamp;

  exiting = 1;
  if (t->manager_window)
    {
      gdk_error_trap_push();
      timestamp = gdk_x11_get_server_time (t->manager_window->window);
      gtk_selection_owner_set(NULL, selection_g_atom, timestamp);
      gtk_widget_destroy(t->manager_window);
      t->manager_window = NULL;
      gdk_error_trap_pop();
    }
}


static gboolean handle_lost_selection(GtkWidget *widget,
			       GdkEventSelection *event,
			       gpointer user_data)
{
  if (!exiting)
    {
      tray_destroy(widget);
      gtk_main_quit();
    }
  return TRUE;
}

static gboolean handle_remove(GtkSocket *socket, gpointer user_data)
{
  tray *t=(tray *) user_data;
  t->icon_count--;
  check_size(t);

  return FALSE;
}

static void remove_item(GtkWidget *widget, gpointer data)
{
  gtk_container_remove(GTK_CONTAINER(((tray *)data)->box), widget);
}

static void check_size(tray *t)
{
  int msize=o_min_size.int_value;
  int other;

  if(t->icon_count<1)
    other=msize;
  else
    other=-1;

  if(t->ishoriz)
    gtk_widget_set_size_request(t->box, other, msize);
  else
    gtk_widget_set_size_request(t->box, msize, other);
}

