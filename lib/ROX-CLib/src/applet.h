/*
 * $Id: applet.h,v 1.1 2002/01/10 15:14:38 stephen Exp $
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

#if 0
extern PanelLocation applet_get_panel_location(GtkWidget *plug);
extern void applet_show_menu(GtkWidget *menu, GdkEventButton *evbut);
#endif

extern void applet_popup_menu(GtkWidget *plug, GtkWidget *menu,
			      GdkEventButton *evbut);
#endif

/*
 * $Log: applet.h,v $
 * Revision 1.1  2002/01/10 15:14:38  stephen
 * Added utility function for placing an applets menu relative to the
 * panel.
 *
 */

