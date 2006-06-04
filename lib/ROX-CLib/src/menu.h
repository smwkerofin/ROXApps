/*
 * menu.h - Interface to the ROX menu system.
 *
 * $Id$
 */

/** @file menu.h
 * @brief Interface to the ROX menu system.
 */

#ifndef _rox_menu_h
#define _rox_menu_h

/**
 * Type of function called before a menu is shown.  This allows a program to
 * update the menu, record state information, or even veto the display
 * of the menu.
 *
 * @param[in,out] menu the menu to show
 * @param[in] window the widget which triggered the menu show event
 * @param[in,out] udata additional data passed to the function which attached
 * the menu to the window (either rox_menu_attach() or
 * rox_menu_attach_to_applet()).
 * @return @c TRUE to show the applet or @c FALSE to ignore the event
 */
typedef gboolean (*ROXMenuPreFilter)(GtkWidget *menu,
				     GtkWidget *window,
				     gpointer udata);

extern GtkWidget *rox_menu_build(GtkWidget *window,
				 GtkItemFactoryEntry *menu_items,
				 int n_menu, const gchar *menu_name,
				 const gchar *accel_name);

extern void rox_menu_attach(GtkWidget *menu,
			    GtkWidget *window,
			    gboolean on_button_press,
			    ROXMenuPreFilter filter,
			    gpointer udata);

extern void rox_menu_attach_to_applet(GtkWidget *menu,
				      GtkWidget *applet,
				      ROXMenuPreFilter filter,
				      gpointer udata);


#endif

/*
 * $Log$
 */
