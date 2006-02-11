/*
 * $Id: main.c,v 1.3 2006/01/21 14:55:57 stephen Exp $
 *
 * SystemTray, a notification area applet for rox
 * Copyright (C) 2003, Andy Hanton
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>

#include "main.h"
#include "i18n.h"
#include "applet.h"
#include "tray.h"
#include "balloon.h"

GtkWidget *plug;
const char *app_dir;
GtkWidget *t;

static GtkWidget *create_menu(GtkWidget *window);

#ifdef HAVE_GETOPT_LONG
#  define USAGE   N_("Try `SystemTrayN/AppRun --help' for more information.\n")
#  define SHORT_ONLY_WARNING ""
#else
#  define USAGE   N_("Try `SystemTrayN/AppRun -h' for more information.\n")
#  define SHORT_ONLY_WARNING	\
		_("NOTE: Your system does not support long options - \n" \
		"you must use the short versions instead.\n\n")
#endif

static void usage(const char *argv0)
{
  printf(_(SHORT_ONLY_WARNING));
  printf(_("Usage: %s [X-options] [gtk-options] [OPTION] [XID]\n" \
	   "where:\n\n" \
	   "  X-options\tstandard Xlib options\n" \
	   "  gtk-options\tstandard GTK+ options\n" \
	   "  -h, --help\tprint this help message\n" \
	   "  -v, --version\tdisplay version information\n" \
	   "  XID\tthe X id of the window to use for applet mode\n"), argv0);
}

static void do_version(void)
{
  printf("%s %s\n", PROJECT, VERSION);
  printf("%s\n", PURPOSE);
  printf("%s\n", WEBSITE);
  printf(_("Copyright 2003 %s\n"), AUTHOR);
  printf(_("Distributed under the terms of the GNU General Public License.\n" \
	   "(See the file COPYING in the Help directory).\n"));
}

void do_quit(void)
{
  tray_destroy(t);
  rox_main_quit();
}

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}



/* Show the info window */
static void show_info_win(void)
{
  GtkWidget *infowin;

  infowin=rox_info_win_new_from_appinfo(PROJECT);

  gtk_widget_show(infowin);
}

gboolean show_menu (GtkWidget *widget, GdkEventButton *event, 
		    gpointer user_data)
{
  static GtkWidget *menu = NULL;

  if (menu == NULL)
    menu = create_menu(plug);

  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      rox_applet_popup_menu(plug, menu, event);
      return TRUE;
    }
  return FALSE;
}

static void save_menus(void)
{
  char	*menurc;
	
  menurc = rox_choices_save("menus", PROJECT, "kerofin.demon.co.uk");
  if (menurc) {
    gtk_accel_map_save(menurc);
    g_free(menurc);
  }
}

/* Create the pop-up menu */
static GtkWidget *create_menu(GtkWidget *window)
{
  GtkWidget *item;
  GtkWidget *menu;
  GtkAccelGroup	*accel_group;
  gchar *menurc;
  GtkStockItem stock;

  menu = gtk_menu_new();
  /*item = gtk_menu_item_new_with_label(_("Info"));*/
  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DIALOG_INFO, NULL);
  gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), "/Info");
  if(gtk_stock_lookup(GTK_STOCK_DIALOG_INFO, &stock)) {
    gtk_accel_map_add_entry("/Info", stock.keyval, stock.modifier);
  }
  g_signal_connect(item, "activate", show_info_win, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show(item);

  /*item = gtk_menu_item_new_with_label(_("Quit"));*/
  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
  gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), "/Quit");
  if(gtk_stock_lookup(GTK_STOCK_QUIT, &stock)) {
    gtk_accel_map_add_entry("/Quit", stock.keyval, stock.modifier);
  }
  g_signal_connect(item, "activate", do_quit, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show(item);

  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
  
  menurc=rox_choices_load("menus", PROJECT, "kerofin.demon.co.uk");
  if(menurc) {
    gtk_accel_map_load(menurc);
    g_free(menurc);
  }

  atexit(save_menus);

  return menu;
}

gboolean handle_destroy(void)
{
  do_quit();
  return TRUE;
}

int (*gdk_handler) (Display *, XErrorEvent *);

int x_error_handler (Display *display, XErrorEvent *error)
{
  if (error->error_code == BadWindow || error->error_code == BadDrawable)
    return 0;
  else
    return gdk_handler(display, error);
}

#define SHORT_OPS "hv"

#ifdef HAVE_GETOPT_LONG
static struct option long_opts[] =
{
  {"help", 0, NULL, 'h'},
  {"version", 0, NULL, 'v'}
};
#endif


int main(int argc, char *argv[]) {
  ROXAppletInfo *info = NULL;
  long window_id;
  int c;
  ROXPanelLocation location = PANEL_UNKNOWN;
  int long_index;
  GtkWidget *align;
  GtkWidget *eventbox;

  app_dir = g_strdup(getenv("APP_DIR"));

  i18n_init();
  rox_init_with_domain(PROJECT, "kerofin.demon.co.uk", &argc, &argv);
  /*gdk_rgb_init();*/

  while(1)
    {
#ifdef HAVE_GETOPT_LONG

      c = getopt_long(argc, argv, SHORT_OPS,
		      long_opts, &long_index);
#else
      c = getopt(argc, argv, SHORT_OPS);
#endif

      if (c == -1)
	break;

      switch(c) {
      case 'h':
	usage(argv[0]);
	exit(0);
	break;
      case 'v':
	do_version();
	exit(0);
	break;
      default:
	printf(_(USAGE));
	return EXIT_FAILURE;
      }
    }

  gdk_handler = XSetErrorHandler(&x_error_handler);

  if (argc == 1) 
    {
      plug = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    }
  else 
    {
      window_id = atol(argv[1]);
      plug = gtk_plug_new(window_id);

      info = rox_applet_get_position(plug);
      if (info) 
	{
	  location = info->loc;
	  g_free(info);
	} 
    }

  g_signal_connect(plug, "destroy", GTK_SIGNAL_FUNC (handle_destroy),
		   NULL);
  rox_add_window(plug);

  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(plug), eventbox);
  g_signal_connect(eventbox, "button_press_event", GTK_SIGNAL_FUNC (show_menu),
		   NULL);
  gtk_widget_add_events(eventbox, GDK_BUTTON_PRESS_MASK);

  align = gtk_alignment_new (.5, .5, 0, 0);
  gtk_container_add(GTK_CONTAINER(eventbox), align);

  t = tray_new(plug, location);
  gtk_container_add(GTK_CONTAINER(align), t);
  gtk_widget_show_all(plug);

  rox_main_loop();
}

