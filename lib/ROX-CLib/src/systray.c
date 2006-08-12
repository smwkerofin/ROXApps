/*
 * systray.c - Interface to the system tray.
 *
 * $Id: systray.c,v 1.1 2005/06/07 10:22:54 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static GdkAtom system_tray, system_tray_data;

static int id=1;

/**
 * Initialize the system tray interface.  
 */
void rox_systray_init(void)
{
  system_tray=gdk_atom_intern("_NET_SYSTEM_TRAY_OPCODE", FALSE);
  system_tray_data=gdk_atom_intern("_NET_SYSTEM_TRAY_MESSAGE_DATA", FALSE);
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
  int sno;
  GdkAtom selection;
  gchar *selstr;
  GtkWidget *plug;
  GdkEventClient ev;
  GdkWindow *owner;
  Window xowner;
  GdkDisplay *display;

  g_return_val_if_fail(GDK_IS_SCREEN(screen), NULL);

  sno=gdk_screen_get_number(screen);
  selstr=g_strdup_printf("_NET_SYSTEM_TRAY_S%d", sno);
  selection=gdk_atom_intern(selstr, FALSE);
  g_free(selstr);

  display=gdk_screen_get_display(screen);
  xowner=XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			    gdk_x11_atom_to_xatom_for_display(display,
							      selection));

  if(!xowner)
    return NULL;
  
  plug=gtk_plug_new(0);
  ev.type=GDK_CLIENT_EVENT;
  ev.message_type=system_tray;
  ev.data_format=32;
  ev.data.l[0]=time(NULL);
  ev.data.l[1]=SYSTEM_TRAY_REQUEST_DOCK;
  ev.data.l[2]=gtk_plug_get_id(GTK_PLUG(plug));
  ev.data.l[3]=0;
  ev.data.l[4]=0;

  gdk_event_send_client_message((GdkEvent *) &ev, xowner);

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
  GdkScreen *screen;
  int sno;
  GdkAtom selection;
  gchar *selstr;
  Window xowner;
  GdkDisplay *display;
  GdkEventClient ev;
  int len, i;

  g_return_val_if_fail(GTK_IS_PLUG(systray), 0);

  len=strlen(message);

  screen=gtk_widget_get_screen(systray);
  sno=gdk_screen_get_number(screen);
  selstr=g_strdup_printf("_NET_SYSTEM_TRAY_S%d", sno);
  selection=gdk_atom_intern(selstr, FALSE);
  g_free(selstr);
  
  display=gdk_screen_get_display(screen);
  xowner=XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			    gdk_x11_atom_to_xatom_for_display(display,
							      selection));
  
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
  GdkScreen *screen;
  int sno;
  GdkAtom selection;
  gchar *selstr;
  Window xowner;
  GdkDisplay *display;
  GdkEventClient ev;

  g_return_if_fail(GTK_IS_PLUG(systray));

  if(id<1)
    
    return;
  screen=gtk_widget_get_screen(systray);
  sno=gdk_screen_get_number(screen);
  selstr=g_strdup_printf("_NET_SYSTEM_TRAY_S%d", sno);
  selection=gdk_atom_intern(selstr, FALSE);
  g_free(selstr);
  
  display=gdk_screen_get_display(screen);
  xowner=XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display),
			    gdk_x11_atom_to_xatom_for_display(display,
							      selection));
  
  ev.type=GDK_CLIENT_EVENT;
  ev.message_type=system_tray;
  ev.data_format=32;
  ev.data.l[0]=time(NULL);
  ev.data.l[1]=SYSTEM_TRAY_CANCEL_MESSAGE;
  ev.data.l[2]=id;

  gdk_event_send_client_message((GdkEvent *) &ev, xowner);
}

/*
 * $Log: systray.c,v $
 * Revision 1.1  2005/06/07 10:22:54  stephen
 * Added system tray interface
 *
 */

