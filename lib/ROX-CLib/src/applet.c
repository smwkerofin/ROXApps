/*
 * $Id: applet.c,v 1.1 2002/01/10 15:14:38 stephen Exp $
 *
 * applet.c - Utilities for rox applet.s
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#include "applet.h"

#define MENU_MARGIN 32

static enum panel_location panel_loc=PANEL_UNKNOWN;
static gint panel_size=MENU_MARGIN;

static struct name_loc {
  const char *name;
  enum panel_location loc;
} locations[]={
  {"Top", PANEL_TOP},   {"Bottom", PANEL_BOTTOM},
  {"Left", PANEL_LEFT}, {"Right", PANEL_RIGHT},

  {NULL, PANEL_UNKNOWN}
};

static gint screen_height=-1;
static gint screen_width=-1;

/* Parse the property string */
static void decode_location(const char *prop)
{
  int i;
  gchar **strs;

  strs=g_strsplit(prop, ",", 2);
  if(!strs)
    return;
  for(i=0; locations[i].name; i++)
    if(strcmp(locations[i].name, strs[0])==0) {
      panel_loc=locations[i].loc;
      break;
    }
  if(strs[1])
    panel_size=atoi(strs[1]);

  g_strfreev(strs);
}

/* Get the data we need */
PanelLocation applet_get_panel_location(GtkWidget *plug)
{
  guint32 xid;
  GdkWindow *gwin;
  Window xwin, parent, root, *children;
  int nchild;
  GdkAtom apos;
  GdkAtom string;

  g_return_val_if_fail(plug!=NULL, PANEL_UNKNOWN);
  
  dprintf(3, "panel_loc=%d", panel_loc);

  /* Have we the answer already */
  if(panel_loc!=PANEL_UNKNOWN)
    return panel_loc;

  /* Get screen size */
#ifdef GTK2
  gdk_drawable_get_size(GDK_ROOT_PARENT(), &screen_width, &screen_height);
#else
  gdk_window_get_size(GDK_ROOT_PARENT(), &screen_width, &screen_height);
#endif
  dprintf(3, "screen is %d,%d", screen_width, screen_height);

  apos=gdk_atom_intern("_ROX_PANEL_MENU_POS", FALSE);
  string=gdk_atom_intern("STRING", FALSE);

  /* Work our way up the list of windows until we find one with the property */
  gtk_widget_realize(plug);
  gwin=plug->window;
  xwin=GDK_WINDOW_XWINDOW(gwin);
  do {
    gint res;
    Atom xtype;
    GdkAtom type;
    gint format, length;
    gulong nitems, remain;
    guchar *data;

    xtype=0;
    res=XGetWindowProperty(GDK_DISPLAY(), xwin, gdk_x11_atom_to_xatom(apos),
			   0, -1, FALSE,
			   gdk_x11_atom_to_xatom(string),
			   &xtype, &format, &nitems, &remain, &data);
    type=gdk_x11_xatom_to_atom(xtype);
    dprintf(3, "res=%d type=%p %s, format=%d length=%d data=%p",
	    res, type, type!=GDK_NONE? gdk_atom_name(type): "NONE", 
	    format, nitems, data);
    if(res==Success && format!=0) {
      dprintf(2, "property=%s", (char *) data);

      decode_location((const char *) data);
      
      XFree(data);
      break;
    }

    /* Find the parent window */
    if(!XQueryTree(GDK_DISPLAY(), xwin, &root, &parent, &children, &nchild))
      break;
    XFree(children);
    
    dprintf(3, "xwin 0x%lx has parent 0x%lx", xwin, parent);
    xwin=parent;
  } while(xwin);

  return panel_loc;
}

/* Mostly taken from icon.c panel_position_menu */
static void position_menu(GtkMenu *menu, gint *x, gint *y,
#ifdef GTK2
			  gboolean *push_in,
#endif
			  gpointer data)
{
  int *pos = (int *) data;
  GtkRequisition requisition;

  dprintf(2, "position_menu(%p, &%d, &%d, %p)", menu, *x, *y, data);
  dprintf(3, "pos[]={%d, %d, %d}", pos[0], pos[1], pos[2]);
  
  gtk_widget_size_request(GTK_WIDGET(menu), &requisition);
  dprintf(3, "menu is %d,%d", requisition.width, requisition.height);

  if (pos[0] == -1)
    *x = screen_width - panel_size - requisition.width;
  else if (pos[0] == -2)
    *x = panel_size;
  else
    *x = pos[0] - (requisition.width >> 2);
		
  if (pos[1] == -1)
    *y = screen_height - panel_size - requisition.height;
  else if (pos[1] == -2)
    *y = panel_size;
  else
    *y = pos[1] - (requisition.height >> 2);

  dprintf(3, "at %d,%d", *x, *y);
  *x = CLAMP(*x, 0, screen_width - requisition.width);
  *y = CLAMP(*y, 0, screen_height - requisition.height);

  dprintf(3, "at %d,%d", *x, *y);
}

void applet_show_menu(GtkWidget *menu, GdkEventButton *evbut)
{
  if(panel_loc==PANEL_UNKNOWN)
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		   evbut->button, evbut->time);
  else {
    int pos[3];
  
    pos[0] = evbut->x_root;
    pos[1] = evbut->y_root;
    pos[2] = 1;

    switch(panel_loc) {
    case PANEL_TOP:
      pos[1]=-2;
      break;
    case PANEL_BOTTOM:
      pos[1]=-1;
      break;
    case PANEL_LEFT:
      pos[0]=-2;
      break;
    case PANEL_RIGHT:
      pos[0]=-1;
      break;
    default:
      rox_error("Unknown panel location (%d), can't place menu",
		panel_loc);
      return;
    }
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, position_menu, pos,
		   evbut->button, evbut->time);
  }
}

typedef struct applet_info {
  PanelLocation loc;
  int margin;
} AppletInfo;

/* Parse the property string */
static void decode_location2(const char *prop, AppletInfo *inf)
{
  int i;
  gchar **strs;

  strs=g_strsplit(prop, ",", 2);
  if(!strs)
    return;
  for(i=0; locations[i].name; i++)
    if(strcmp(locations[i].name, strs[0])==0) {
      inf->loc=locations[i].loc;
      break;
    }
  if(strs[1])
    inf->margin=atoi(strs[1]);
  else
    inf->margin=MENU_MARGIN;

  g_strfreev(strs);
}

/* Get the data we need */
AppletInfo *get_position(GtkWidget *plug)
{
  guint32 xid;
  GdkWindow *gwin;
  Window xwin, parent, root, *children;
  int nchild;
  GdkAtom apos;
  GdkAtom string;
  AppletInfo *ainfo=NULL;

  g_return_val_if_fail(plug!=NULL, NULL);
  
  dprintf(3, "panel_loc=%d", panel_loc);

  /* Get screen size */
  if(screen_width<0) {
#ifdef GTK2
    gdk_drawable_get_size(GDK_ROOT_PARENT(), &screen_width, &screen_height);
#else
    gdk_window_get_size(GDK_ROOT_PARENT(), &screen_width, &screen_height);
#endif
  }
  dprintf(3, "screen is %d,%d", screen_width, screen_height);

  apos=gdk_atom_intern("_ROX_PANEL_MENU_POS", FALSE);
  string=gdk_atom_intern("STRING", FALSE);

  /* Work our way up the list of windows until we find one with the property */
  gtk_widget_realize(plug);
  gwin=plug->window;
  xwin=GDK_WINDOW_XWINDOW(gwin);
  do {
    gint res;
    Atom xtype;
    GdkAtom type;
    gint format, length;
    gulong nitems, remain;
    guchar *data;

    xtype=0;
    res=XGetWindowProperty(GDK_DISPLAY(), xwin, gdk_x11_atom_to_xatom(apos),
			   0, -1, FALSE,
			   gdk_x11_atom_to_xatom(string),
			   &xtype, &format, &nitems, &remain, &data);
    type=gdk_x11_xatom_to_atom(xtype);
    dprintf(3, "res=%d type=%p %s, format=%d length=%d data=%p",
	    res, type, type!=GDK_NONE? gdk_atom_name(type): "NONE", 
	    format, nitems, data);
    if(res==Success && format!=0) {
      dprintf(2, "property=%s", (char *) data);

      ainfo=g_new(AppletInfo, 1);
      decode_location2((const char *) data, ainfo);
      
      XFree(data);
      break;
    }

    /* Find the parent window */
    if(!XQueryTree(GDK_DISPLAY(), xwin, &root, &parent, &children, &nchild))
      break;
    XFree(children);
    
    dprintf(3, "xwin 0x%lx has parent 0x%lx", xwin, parent);
    xwin=parent;
  } while(xwin);

  return ainfo;
}

static void position_menu2(GtkMenu *menu, gint *x, gint *y,
#ifdef GTK2
			  gboolean *push_in,
#endif
			  gpointer data)
{
  AppletInfo *ainfo = (AppletInfo *) data;
  GtkRequisition requisition;

  dprintf(2, "position_menu(%p, &%d, &%d, %p)", menu, *x, *y, data);
  if(!ainfo)
    return;
  dprintf(3, "AppletInfo={%d, %d}", ainfo->loc, ainfo->margin);
  
  gtk_widget_size_request(GTK_WIDGET(menu), &requisition);
  dprintf(3, "menu is %d,%d", requisition.width, requisition.height);

  if (ainfo->loc == PANEL_RIGHT)
    *x = screen_width - ainfo->margin - requisition.width;
  else if (ainfo->loc == PANEL_LEFT)
    *x = ainfo->margin;
  else
    *x = *x - (requisition.width >> 2);
		
  if (ainfo->loc == PANEL_BOTTOM)
    *y = screen_height - ainfo->margin - requisition.height;
  else if (ainfo->loc == PANEL_TOP)
    *y = panel_size;
  else
    *y = *y - (requisition.height >> 2);

  dprintf(3, "at %d,%d", *x, *y);
  *x = CLAMP(*x, 0, screen_width - requisition.width);
  *y = CLAMP(*y, 0, screen_height - requisition.height);

  dprintf(3, "at %d,%d", *x, *y);
}

void applet_popup_menu(GtkWidget *plug, GtkWidget *menu, GdkEventButton *evbut)
{
  AppletInfo *pos;

  pos=gtk_object_get_data(GTK_OBJECT(plug), "applet-menu-pos");
  if(!pos) {
    pos=get_position(plug);
    if(pos) {
      gtk_object_set_data(GTK_OBJECT(plug), "applet-menu-pos", pos);
      gtk_signal_connect_object(GTK_OBJECT(plug), "destroy",
				GTK_SIGNAL_FUNC(g_free), pos);
    }
  }
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, position_menu2, pos,
		 evbut->button, evbut->time);
}

