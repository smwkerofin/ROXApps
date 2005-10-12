/*
 * $Id: applet.h,v 1.5 2005/10/02 11:35:47 stephen Exp $
 *
 * applet.h - Utilities for ROX applets
 */

#ifndef _applet_h
#define _applet_h

/**
 * @file applet.h
 * @brief Implementing ROX applets.
 *
 * @version $Id$
 * @author Stephen Watson stephen@kerofin.demon.co.uk
 */

/**
 * Location of panel the applet is on.
 */
enum rox_panel_location {
  PANEL_TOP,            /**< Panel is on the top */
  PANEL_BOTTOM,         /**< Panel is on the bottom */
  PANEL_LEFT,           /**< Panel is on the left */
  PANEL_RIGHT,          /**< Panel is on the right */
  PANEL_UNKNOWN         /**< Panel location is unknown */
};
typedef enum rox_panel_location ROXPanelLocation;
#define PanelLocation ROXPanelLocation

typedef struct ROXAppletInfo ROXAppletInfo;
/**
 * @brief Details of the location of an applet's icon.
 */
struct ROXAppletInfo {
  PanelLocation loc;             /**< Location of the panel the icon is on. */
  int margin;                    /**< Margin in pixels from screen edge
				  * to place popup menu. */
};
#define AppletInfo ROXAppletInfo

extern void rox_applet_popup_menu(GtkWidget *plug, GtkWidget *menu,
			      GdkEventButton *evbut);
extern ROXAppletInfo *rox_applet_get_position(GtkWidget *plug);

extern void applet_popup_menu(GtkWidget *plug, GtkWidget *menu,
			      GdkEventButton *evbut);
extern ROXAppletInfo *applet_get_position(GtkWidget *plug);

#endif

/*
 * $Log: applet.h,v $
 * Revision 1.5  2005/10/02 11:35:47  stephen
 * Properly declare an internal function that SystemTray was using and shouldn't
 * have been.
 *
 * Revision 1.4  2005/08/21 13:06:38  stephen
 * Added doxygen comments
 *
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

