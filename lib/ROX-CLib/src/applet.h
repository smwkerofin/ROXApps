/*
 * $Id$
 *
 * applet.h - Utilities for ROX applets
 */

#ifndef _applet_h
#define _applet_h

enum panel_location {
  PANEL_TOP, PANEL_BOTTOM, PANEL_LEFT, PANEL_RIGHT,
  PANEL_UNKNOWN
};
typedef enum panel_location PanelLocation;

extern PanelLocation applet_get_panel_location(GtkWidget *plug);
extern void applet_show_menu(GtkWidget *menu, GdkEventButton *evbut);

#endif

/*
 * $Log$
 */

