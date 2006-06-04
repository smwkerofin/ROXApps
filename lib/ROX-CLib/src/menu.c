/*
 * menu.c - The ROX menu system.
 *
 * $Id$
 */

/** @file menu.c
 * @brief Help for building ROX-style menus.
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "rox.h"
#include "menu.h"

typedef struct _link {
  GtkWidget *menu;
  ROXMenuPreFilter filter;
  gpointer udata;
  gboolean is_applet;
} Link;

static Link *make_link(GtkWidget* window, GtkWidget *menu,
		       ROXMenuPreFilter filter, gpointer udata);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer udata); 
static gboolean popup_menu(GtkWidget *window, gpointer udata);
static void save_menus(void);
static gchar *translate_menu(const gchar *path, gpointer udata);

static gchar *accel=NULL;

/**
 * Create a menu.  The menu is built from a static definition and connected
 * to a window for the purposes of reacting to key accelerators.  It is
 * not attached to a window, use either rox_menu_attach() or
 * rox_menu_attach_to_applet().
 *
 * @param[in,out] window window listening for key events that will be used
 * for key accelerators (or @c NULL)
 * @param[in] menu_items array of menu definitions.
 * @param[in] n_menu number of items in @a menu_items
 * @param[in] menu_name name of menu to create, or @c NULL to use "<system>"
 * @param[in] accel_name file name to use when loading or saving key
 * accelerators, or @c NULL to disable loading and saving.
 */
GtkWidget *rox_menu_build(GtkWidget *window, GtkItemFactoryEntry *menu_items,
			  int n_menu, const gchar *menu_name,
			  const gchar *accel_name)
{
  GtkItemFactory	*item_factory;
  GtkAccelGroup	*accel_group;
  gchar *menurc;

  accel_group=gtk_accel_group_new();
  item_factory=gtk_item_factory_new(GTK_TYPE_MENU,
				    menu_name? menu_name: "<system>", 
				    accel_group);
  gtk_item_factory_create_items(item_factory, n_menu, menu_items, NULL);
  gtk_item_factory_set_translate_func(item_factory, translate_menu,
				      NULL, NULL);

  /* Attach the new accelerator group to the window. */
  if(window)
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  if(accel_name) {
    menurc=rox_choices_load(accel_name, rox_get_program_name(),
			    rox_get_program_domain());
    if(menurc) {
      gtk_accel_map_load(menurc);
      g_free(menurc);
    }

    if(!accel) {
      accel=g_strdup(accel_name);
      atexit(save_menus);
    }
  }

  return gtk_item_factory_get_widget(item_factory,
				     menu_name? menu_name: "<system>");
}

/**
 * Attach a menu to a window (or other widget).  The menu will be displayed
 * when the "popup-menu" signal is triggered on @a window and optionally
 * when the "button_press_event" signal is triggered.
 *
 * @param[in] menu menu to attach
 * @param[in,out] window widget to attach @a menu to
 * @param[in] on_button_press if not @c FALSE then pop-up the menu on the
 * "button_press_event" signal, otherwise just on the "popup-menu" signal.
 * @param[in] filter function to call prior to showing the menu, or @c NULL
 * @param[in] udata additional data to pass to @a filter when calling.
 */
void rox_menu_attach(GtkWidget *menu, GtkWidget *window,
		     gboolean on_button_press,
			    ROXMenuPreFilter filter,
			    gpointer udata)
{
  Link *link;

  link=make_link(window, menu, filter, udata);
  link->is_applet=FALSE;
  if(on_button_press) {
    g_signal_connect(G_OBJECT(window), "button_press_event",
		       G_CALLBACK(button_press), link);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
  }
  g_signal_connect(G_OBJECT(window), "popup-menu",
		     G_CALLBACK(popup_menu), link);
  gtk_widget_realize(window);
}

/**
 * Attach a menu to an applet's window.  The menu will be displayed
 * when the "popup-menu" signal or button_press_event" signal is triggered
 * on @a applet.
 *
 * @param[in] menu menu to attach
 * @param[in,out] applet widget to attach @a menu to
 * @param[in] filter function to call prior to showing the menu, or @c NULL
 * @param[in] udata additional data to pass to @a filter when calling.
 */
void rox_menu_attach_to_applet(GtkWidget *menu, GtkWidget *applet,
			    ROXMenuPreFilter filter,
			    gpointer udata)
{
  Link *link;

  link=make_link(applet, menu, filter, udata);
  link->is_applet=TRUE;
  g_signal_connect(G_OBJECT(applet), "button_press_event",
		     G_CALLBACK(button_press), link);
  gtk_widget_add_events(applet, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(G_OBJECT(applet), "popup-menu",
		     G_CALLBACK(popup_menu), link);
}

/* Local functions below */
static void delete_link(GtkWidget *widget, Link *link)
{
  rox_debug_printf(1, "delete link %p", link);
  g_free(link);
  rox_debug_printf(1, "deleted link %p", link);
}

static Link *make_link(GtkWidget* window, GtkWidget *menu,
		       ROXMenuPreFilter filter, gpointer udata)
{
  Link *link=g_new(Link, 1);

  link->menu=menu;
  link->filter=filter;
  link->udata=udata;

  rox_debug_printf(1, "made link %p for window %p", link, window);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(delete_link), link);

  return link;
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer udata)
{
  Link *link=(Link *) udata;

  g_return_val_if_fail(GTK_IS_WIDGET(window), FALSE);
  g_return_val_if_fail(GTK_IS_MENU(link->menu), FALSE);
  
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    if(link->filter && !link->filter(window, link->menu, link->udata))
      return FALSE;
       
    if(link->is_applet) 
      rox_applet_popup_menu(window, link->menu, bev);
    else
      gtk_menu_popup(GTK_MENU(link->menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

static gboolean popup_menu(GtkWidget *window, gpointer udata)
{
  Link *link=(Link *) udata;

  g_return_val_if_fail(GTK_IS_WIDGET(window), FALSE);
  g_return_val_if_fail(GTK_IS_MENU(link->menu), FALSE);
  
  if(link->filter && !link->filter(window, link->menu, link->udata))
    return;
     
  if(link->is_applet) 
    rox_applet_popup_menu(window, link->menu, NULL);
  else
    gtk_menu_popup(GTK_MENU(link->menu), NULL, NULL, NULL, NULL,
		   0, gtk_get_current_event_time());
}

static void save_menus(void)
{
  char	*menurc;
	
  menurc=rox_choices_save(accel, rox_get_program_name(),
			  rox_get_program_domain());
  if (menurc) {
    gtk_accel_map_save(menurc);
    g_free(menurc);
  }
}

static gchar *translate_menu(const gchar *path, gpointer udata)
{
  return gettext(path);
}

/*
 * $Log$
 */
