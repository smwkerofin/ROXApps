/*
 * $Id: applet.c,v 1.7 2005/10/12 11:00:22 stephen Exp $
 *
 * applet.c - Utilities for rox applets
 */
/**
 * @file applet.c
 * @brief Implementing ROX applets
 *
 * @version $Id: applet.c,v 1.7 2005/10/12 11:00:22 stephen Exp $
 * @author Stephen Watson stephen@kerofin.demon.co.uk
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "rox.h"
#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#include "applet.h"

#define MENU_MARGIN 32

static ROXPanelLocation panel_loc=PANEL_UNKNOWN;
static gint panel_size=MENU_MARGIN;

static struct name_loc {
  const char *name;
  ROXPanelLocation loc;
} locations[]={
  {"Top", PANEL_TOP},   {"Bottom", PANEL_BOTTOM},
  {"Left", PANEL_LEFT}, {"Right", PANEL_RIGHT},

  {NULL, PANEL_UNKNOWN}
};

static gint screen_height=-1;
static gint screen_width=-1;

/* Parse the property string */
static void decode_location2(const char *prop, ROXAppletInfo *inf)
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

/**
 * Return the location of an applet's icon.
 *
 * @param[in] plug the widget that the applet created in the panel.
 * @return pointer to applet info, pass to g_free() when done.
 */
ROXAppletInfo *rox_applet_get_position(GtkWidget *plug)
{
  GdkWindow *gwin;
  Window xwin, parent, root, *children;
  int nchild;
  GdkAtom apos;
  GdkAtom string;
  ROXAppletInfo *ainfo=NULL;

  g_return_val_if_fail(plug!=NULL, NULL);
  
  dprintf(3, "panel_loc=%d", panel_loc);

  /* Get screen size */
  if(screen_width<0) {
    gdk_drawable_get_size(GDK_ROOT_PARENT(), &screen_width, &screen_height);
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
    gint format;
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

      ainfo=g_new(ROXAppletInfo, 1);
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

/**
 * Return the location of an applet's icon.
 *
 * @param[in] plug the widget that the applet created in the panel.
 * @return pointer to applet info, pass to g_free() when done.
 *
 * @deprecated Use rox_applet_get_position() instead.
 */
ROXAppletInfo *applet_get_position(GtkWidget *plug)
{
  ROX_CLIB_DEPRECATED("rox_applet_get_position");
  return rox_applet_get_position(plug);
}

static void position_menu2(GtkMenu *menu, gint *x, gint *y,
			  gboolean *push_in,
			  gpointer data)
{
  ROXAppletInfo *ainfo = (ROXAppletInfo *) data;
  GtkRequisition requisition;

  dprintf(2, "position_menu(%p, &%d, &%d, %p)", menu, *x, *y, data);
  if(!ainfo)
    return;
  dprintf(3, "ROXAppletInfo={%d, %d}", ainfo->loc, ainfo->margin);
  
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

/**
 * Popup a menu in the appropriate place for an applet's icon.
 *
 * @param[in,out] plug the GtkPlug object used to hold the applet
 * @param[in] menu the menu to show
 * @param[in] evbut the button event that triggered the menu, or
 * @c NULL if the menu was triggered by some other means
 */
void rox_applet_popup_menu(GtkWidget *plug, GtkWidget *menu,
			   GdkEventButton *evbut)
{
  ROXAppletInfo *pos;

  pos=g_object_get_data(G_OBJECT(plug), "applet-menu-pos");
  if(!pos) {
    pos=rox_applet_get_position(plug);
    if(pos) {
      g_object_set_data(G_OBJECT(plug), "applet-menu-pos", pos);
      g_signal_connect_swapped(G_OBJECT(plug), "destroy",
				G_CALLBACK(g_free), pos);
    }
  }
  if(evbut)
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, position_menu2, pos,
		   evbut->button, evbut->time);
  else
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, position_menu2, pos,
		   0, gtk_get_current_event_time());
}

/**
 * Popup a menu in the appropriate place for an applet's icon.
 *
 * @param[in,out] plug the GtkPlug object used to hold the applet
 * @param[in] menu the menu to show
 * @param[in] evbut the button event that triggered the menu, or
 * @c NULL if the menu was triggered by some other means
 *
 * @deprecated Use rox_applet_popup_menu() instead.
 */
void applet_popup_menu(GtkWidget *plug, GtkWidget *menu, GdkEventButton *evbut)
{
  ROX_CLIB_DEPRECATED("rox_applet_popup_menu");
  rox_applet_popup_menu(plug, menu, evbut);
}

/*
 * $Log: applet.c,v $
 * Revision 1.7  2005/10/12 11:00:22  stephen
 * Externally visible symbols have rox_ or ROX prefixes.
 * All still exist under the old names but in general will produce a warning message.
 *
 * Revision 1.6  2005/10/02 11:35:47  stephen
 * Properly declare an internal function that SystemTray was using and shouldn't
 * have been.
 *
 * Revision 1.5  2005/08/21 13:06:38  stephen
 * Added doxygen comments
 *
 * Revision 1.4  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 * Revision 1.3  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 */
