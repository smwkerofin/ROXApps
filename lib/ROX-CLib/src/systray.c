/*
 * systray.c - Interface to the system tray.
 *
 * $Id: systray.c,v 1.3 2006/08/12 17:04:56 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "choices.h"
#define DEBUG 1
#include "rox.h"
#include "systray.h"


/** @file systray.c
 * @brief Interface to the system tray.
 */

#define SYSTEM_TRAY_REQUEST_DOCK     0
#define SYSTEM_TRAY_BEGIN_MESSAGE    1
#define SYSTEM_TRAY_CANCEL_MESSAGE   2

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

static GdkAtom system_tray, system_tray_data;
static GdkAtom system_tray_orientation;
static GdkAtom manager;

static int id=1;

static Window get_selection_owner(GtkWidget *systray, GdkAtom selection);
static GdkFilterReturn manager_filter(GdkXEvent *xevent, 
				      GdkEvent  *event, 
				      gpointer   user_data);
static GdkFilterReturn root_filter(GdkXEvent *xevent, 
				      GdkEvent  *event, 
				      gpointer   user_data);
static void object_destroy(GtkObject *object, gpointer udata);
static void tray_realize(GtkWidget *tray, gpointer udata);
static gboolean tray_delete(GtkWidget *tray, GdkEventAny *event,
			    gpointer udata);

/**
 * Initialize the system tray interface.  
 */
void rox_systray_init(void)
{
  system_tray=gdk_atom_intern("_NET_SYSTEM_TRAY_OPCODE", FALSE);
  system_tray_data=gdk_atom_intern("_NET_SYSTEM_TRAY_MESSAGE_DATA", FALSE);
  system_tray_orientation=gdk_atom_intern("_NET_SYSTEM_TRAY_ORIENTATION",
					  FALSE);
  manager=gdk_atom_intern("MANAGER", FALSE);
}

/**
 * Create a new system tray widget for the default screen.  A container
 * widget is returned, you should add whatever widgets are needed to it.
 * rox_systray_init() must be called first.
 *
 * @return a pointer to a GtkContainer widget, or NULL if no system tray
 * manager was found.
 */
GtkWidget *rox_systray_new(void)
{
  return rox_systray_new_on_screen(gdk_screen_get_default());
}

/**
 * Create a new system tray widget for the specified screen.  A container
 * widget is returned, you should add whatever widgets are needed to it.
 * rox_systray_init() must be called first.
 *
 * @param[in] screen screen where the system tray manager is running
 * @return a pointer to a GtkContainer widget, or NULL if no system tray
 * manager was found for the screen.
 */
GtkWidget *rox_systray_new_on_screen(GdkScreen *screen)
{
  GdkDisplay *display;
  int sno;
  GdkAtom selection;
  gchar *selstr;
  GtkWidget *plug;
  GdkEventClient ev;
  Window xowner;
  GdkWindow *root_window, *gdkwin;
  GdkEventMask mask;

  g_return_val_if_fail(GDK_IS_SCREEN(screen), NULL);

  sno=gdk_screen_get_number(screen);
  selstr=g_strdup_printf("_NET_SYSTEM_TRAY_S%d", sno);
  selection=gdk_atom_intern(selstr, FALSE);
  g_free(selstr);

  display=gdk_screen_get_display(screen);
  root_window=gdk_screen_get_root_window(screen);
  
  xowner=XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			    gdk_x11_atom_to_xatom_for_display(display,
							      selection));
  plug=gtk_plug_new(0);
  g_signal_connect(G_OBJECT(plug), "destroy", G_CALLBACK(object_destroy),
		   NULL);
  
  if(xowner) {
    ev.type=GDK_CLIENT_EVENT;
    ev.message_type=system_tray;
    ev.data_format=32;
    ev.data.l[0]=time(NULL);
    ev.data.l[1]=SYSTEM_TRAY_REQUEST_DOCK;
    ev.data.l[2]=gtk_plug_get_id(GTK_PLUG(plug));
    ev.data.l[3]=0;
    ev.data.l[4]=0;
    
    gdk_event_send_client_message((GdkEvent *) &ev, xowner);

    gdkwin=gdk_window_lookup_for_display(display, xowner);
    if(gdkwin) {
      mask=gdk_window_get_events(gdkwin);
      gdk_window_set_events(gdkwin,
			    mask|GDK_STRUCTURE_MASK|GDK_PROPERTY_CHANGE_MASK);
      gdk_window_add_filter(gdkwin, manager_filter, plug);
    }
  }

  gdk_window_add_filter(root_window, root_filter, plug);

  g_object_set_data(G_OBJECT(plug), "rox-clib-selection", selection);
  g_object_set_data(G_OBJECT(plug), "rox-clib-root-window", root_window);
  g_object_set_data(G_OBJECT(plug), "rox-clib-xowner",
		    GINT_TO_POINTER(xowner));

  if(!GTK_WIDGET_REALIZED(plug)) {
    g_signal_connect(G_OBJECT(plug), "realize", G_CALLBACK(tray_realize),
		     NULL);
  }
  g_signal_connect(G_OBJECT(plug), "delete-event", G_CALLBACK(tray_delete),
		     NULL);

  return plug;
}

/**
 * Send a balloon message to the system tray icon.
 *
 * This shows the message immediately.  If instead you wish to show a tooltip
 * when the mouse hovers over the icon then you should instead pack your
 * icon in a GtkEventBox and set the tooltip on that instead.
 *
 * @param[in] systray system tray container as returned by rox_systray_new()
 * or rox_systray_new_on_screen().
 * @param[in] message message to display
 * @param[in] usec time in micro-seconds to display balloon message, or
 * 0 for no time out.
 * @return id code for message, 0 returned for failure
 */
int rox_systray_send_message(GtkWidget *systray,
			      const char *message,
				     unsigned usec)
{
  GdkAtom selection;
  Window xowner;
  GdkEventClient ev;
  int len, i;

  g_return_val_if_fail(GTK_IS_PLUG(systray), 0);

  len=strlen(message);

  selection=g_object_get_data(G_OBJECT(systray), "rox-clib-selection");
  
  xowner=get_selection_owner(systray, selection);
  
  ev.type=GDK_CLIENT_EVENT;
  ev.message_type=system_tray;
  ev.data_format=32;
  ev.data.l[0]=time(NULL);
  ev.data.l[1]=SYSTEM_TRAY_BEGIN_MESSAGE;
  ev.data.l[2]=usec;
  ev.data.l[3]=len;
  ev.data.l[4]=id;

  gdk_event_send_client_message((GdkEvent *) &ev, xowner);

  ev.message_type=system_tray_data;
  ev.data_format=8;
  ev.window=systray->window;

  for(i=0; i<len; i+=20) {
    strncpy(ev.data.b, message+i, 20);
    gdk_event_send_client_message((GdkEvent *) &ev, xowner);
  }

  return id++;
}

/**
 * Cancel a balloon help message.
 *
 * @param[in] systray system tray container as used in
 * rox_systray_send_message().
 * @param[in] id id code returned by rox_systray_send_message().
 */
void rox_systray_cancel_message(GtkWidget *systray, int id)
{
  GdkAtom selection;
  Window xowner;
  GdkEventClient ev;

  g_return_if_fail(GTK_IS_PLUG(systray));

  if(id<1)
    return;
  
  selection=g_object_get_data(G_OBJECT(systray), "rox-clib-selection");
  
  xowner=get_selection_owner(systray, selection);
  
  ev.type=GDK_CLIENT_EVENT;
  ev.message_type=system_tray;
  ev.data_format=32;
  ev.data.l[0]=time(NULL);
  ev.data.l[1]=SYSTEM_TRAY_CANCEL_MESSAGE;
  ev.data.l[2]=id;

  gdk_event_send_client_message((GdkEvent *) &ev, xowner);
}

/**
 * Determine the orientation of the system tray.
 *
 * @param[in] systray system tray container returned by
 * rox_systray_new().
 * @return @c TRUE if the system tray has a vertical orientation, or @c FALSE
 * if horizontal.
 */
gboolean rox_systray_is_vertical(GtkWidget *systray)
{
  GdkAtom selection;
  GdkAtom type;
  gint format, length;
  guchar *data;
  GdkWindow *owner;

  g_return_val_if_fail(GTK_IS_PLUG(systray), FALSE);
  
  selection=g_object_get_data(G_OBJECT(systray), "rox-clib-selection");
  owner=gdk_selection_owner_get(selection);

  if(gdk_property_get(owner, system_tray_orientation, GDK_NONE, 0, 4,
		      FALSE, &type, &format, &length, &data)) {
    int oren=*((int *) data);
    g_free(data);
    return oren==_NET_SYSTEM_TRAY_ORIENTATION_VERT;
  }

  return FALSE;
}

/* Local functions */
static void object_destroy(GtkObject *object, gpointer udata)
{
  GdkWindow *root_window=g_object_get_data(G_OBJECT(object),
					   "rox-clib-root-window");
  Window xowner=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(object),
						   "rox-clib-xowner"));

  gdk_window_remove_filter(root_window, root_filter, object);

  if(xowner) {
    GdkWindow *gdkwin;
    gdkwin=gdk_window_lookup_for_display(gtk_widget_get_display(GTK_WIDGET(object)),
					  xowner);
    gdk_window_remove_filter(gdkwin, manager_filter, object);
  }
}

static Window get_selection_owner(GtkWidget *systray, GdkAtom selection)
{
  GdkScreen *screen;
  GdkDisplay *display;

  screen=gtk_widget_get_screen(systray);
  display=gdk_screen_get_display(screen);
  
  return XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			    gdk_x11_atom_to_xatom_for_display(display,
							      selection));
}

static GdkFilterReturn root_filter(GdkXEvent *xevent, 
				      GdkEvent  *event, 
				      gpointer   user_data)
{
  GtkWidget *tray=GTK_WIDGET(user_data);
  GdkAtom selection;  
  XEvent *xev = (XEvent *)xevent;
  GdkEventClient ev;
  Window xowner;
  GdkDisplay *display;

  selection=g_object_get_data(G_OBJECT(tray), "rox-clib-selection");
  if(xev->xany.type==ClientMessage
     && xev->xclient.message_type==gdk_x11_atom_to_xatom(manager) &&
     xev->xclient.data.l[1]==gdk_x11_atom_to_xatom(selection)) {

    display=gtk_widget_get_display(tray);
    xowner=XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			      gdk_x11_atom_to_xatom_for_display(display,
								selection));
    
    ev.type=GDK_CLIENT_EVENT;
    ev.message_type=system_tray;
    ev.data_format=32;
    ev.data.l[0]=time(NULL);
    ev.data.l[1]=SYSTEM_TRAY_REQUEST_DOCK;
    ev.data.l[2]=gtk_plug_get_id(GTK_PLUG(tray));
    ev.data.l[3]=0;
    ev.data.l[4]=0;
    
    gdk_event_send_client_message((GdkEvent *) &ev, xowner);
    
    g_object_set_data(G_OBJECT(tray), "rox-clib-xowner",
		      GINT_TO_POINTER(xowner));
  
  }
  
  return GDK_FILTER_CONTINUE;   
}

/* This doesn't seem to get called (can't always get a GdkWindow for the
   manager window) */
static GdkFilterReturn manager_filter(GdkXEvent *xevent, 
				      GdkEvent  *event, 
				      gpointer   user_data)
{
  GtkWidget *tray=GTK_WIDGET(user_data);
  GdkAtom selection;  
  XEvent *xev = (XEvent *)xevent;

  selection=g_object_get_data(G_OBJECT(tray), "rox-clib-selection");
  if(xev->xany.type==DestroyNotify) {
    Window xowner=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tray),
						   "rox-clib-xowner"));

    if(xev->xany.window==xowner) {
      printf("manager dies\n");
    }
  }
  
  return GDK_FILTER_CONTINUE;   
}

static void tray_realize(GtkWidget *tray, gpointer udata)
{
  Window xowner=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tray),
						   "rox-clib-xowner"));
  GdkWindow *gdkwin;
  GdkDisplay *display;

  display=gtk_widget_get_display(tray);
    
  gdkwin=gdk_window_lookup_for_display(display, xowner);
  if(gdkwin) {
    gdk_window_add_filter(gdkwin, manager_filter, tray);
  }
}

static gboolean tray_delete(GtkWidget *tray, GdkEventAny *event,
			    gpointer udata)
{
  Window xowner=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tray),
						   "rox-clib-xowner"));
  
  if(xowner) {
   GdkWindow *gdkwin;
   
   gdkwin=gdk_window_lookup_for_display(gtk_widget_get_display(tray),
					xowner);
      
   gdk_window_remove_filter(gdkwin, manager_filter, tray);
      
   
   g_object_set_data(G_OBJECT(tray), "rox-clib-xowner", GINT_TO_POINTER(0));
 }
  
  return TRUE;
}

/*
 * $Log: systray.c,v $
 * Revision 1.3  2006/08/12 17:04:56  stephen
 * Fix most compilation warnings.
 *
 * Revision 1.2  2006/08/12 10:45:38  stephen
 * Add note to documentation of rox_systray_send_message().
 *
 * Revision 1.1  2005/06/07 10:22:54  stephen
 * Added system tray interface
 *
 */

