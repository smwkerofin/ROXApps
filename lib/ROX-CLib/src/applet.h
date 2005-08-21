/*
 * $Id: applet.h,v 1.3 2003/03/05 15:31:23 stephen Exp $
 *
 * applet.h - Utilities for ROX applets
 */

#ifndef _applet_h
#define _applet_h

/**
 * @file applet.h
 * @brief Implementing ROX applets
 */

/**
 * Location of panel the applet is one.
 */
enum panel_location {
  PANEL_TOP,            /**< Panel is on the top */
  PANEL_BOTTOM,         /**< Panel is on the bottom */
  PANEL_LEFT,           /**< Panel is on the left */
  PANEL_RIGHT,          /**< Panel is on the right */
  PANEL_UNKNOWN         /**< Panel location is unknown */
};
typedef enum panel_location PanelLocation;

extern void applet_popup_menu(GtkWidget *plug, GtkWidget *menu,
			      GdkEventButton *evbut);

#endif

/*
 * $Log: applet.h,v $
 * Revision 1.3  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.2  2002/04/29 08:17:23  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.1  2002/01/10 15:14:38  stephen
 * Added utility function for placing an applets menu relative to the
 * panel.
 *
 */

