/*
 * AppFactory - construct wrappers for non-ROX programs.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: main.c,v 1.1.1.1 2001/07/17 14:39:02 stephen Exp $
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

/* These next two are for internationalization */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>
#include "infowin.h"

#include "choices.h"
#define DEBUG              1   /* Allow debug output */
#include "rox_debug.h"
#include "rox_dnd.h"
#include "gtksavebox.h"

#define PROMPT_UTIL "promptArgs.py"

/* Declare functions in advance */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on window */
static void show_info_win(void);        /* Show information box */
static void read_config(void);          /* Read configuration */
static void write_config(void);         /* Write configuration */

static gboolean app_dropped(GtkWidget *, GSList *uris, gpointer data,
			    gpointer udata);
static gboolean icon_dropped(GtkWidget *, GSList *uris, gpointer data,
			    gpointer udata);
static gboolean help_dropped(GtkWidget *, GSList *uris, gpointer data,
			    gpointer udata);

static void begin_save(GtkWidget *widget, gpointer data);

static GtkWidget *win;
static GtkWidget *prog_name;
static GtkWidget *prog_icon;
static GtkWidget *prog_help;
static GtkWidget *prompt_args;

static GtkWidget *save=NULL;

/* Main.  Here we set up the gui and enter the main loop */
int main(int argc, char *argv[])
{
  GtkWidget *vbox, *vbox2;
  GtkWidget *hbox;
  GtkWidget *label;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  gchar *icon_path;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkWidget *but;
  GtkWidget *frame;

  rox_debug_init("AppFactory");

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  /* What is the directory where our resources are? (set by AppletRun) */
  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  /* More (untested) i18n support */
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif
  
  /* Initialise X/GDK/GTK */
  gtk_init(&argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  gtk_widget_push_colormap(gdk_rgb_get_cmap());
  
  /* Init choices and read them in */
  choices_init();
  read_config();

  rox_dnd_init();

  /* Create window */
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "app factory");
  gtk_window_set_title(GTK_WINDOW(win), _("Create Wrapper"));
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");

  /* We want to pop up a menu on a button press */
  gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		     GTK_SIGNAL_FUNC(button_press), win);
  gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
  gtk_widget_realize(win);

  /* A vbox contains widgets arranged vertically */
  vbox=gtk_vbox_new(FALSE, 4);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  /* An hbox contains widgets arranged horizontally.  We put it inside the
     vbox */
  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  /* A label is a text string the user cannot change.  We put it inside the
     hbox */
  label=gtk_label_new("Program:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  prog_name=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), prog_name, TRUE, TRUE, 0);
  gtk_widget_show(prog_name);

  rox_dnd_register_uris(hbox, 0, app_dropped, prog_name);

  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  label=gtk_label_new("Icon:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  icon_path=g_strconcat(app_dir, "/pixmaps/application.xpm", NULL);
  pixmap = gdk_pixmap_create_from_xpm(GTK_WIDGET(win)->window,  &mask,
				      NULL, icon_path);
  prog_icon=gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(hbox), prog_icon, TRUE, TRUE, 0);
  gtk_widget_show(prog_icon);
  gtk_object_set_data(GTK_OBJECT(prog_icon), "path", icon_path);
  
  rox_dnd_register_uris(hbox, 0, icon_dropped, prog_icon);

  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  label=gtk_label_new("Help files:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  prog_help=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), prog_help, TRUE, TRUE, 0);
  gtk_widget_show(prog_help);

  rox_dnd_register_uris(hbox, 0, help_dropped, prog_help);

  frame=gtk_frame_new("Options");
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show(frame);
  
  vbox2=gtk_vbox_new(FALSE, 4);
  gtk_container_add(GTK_CONTAINER(frame), vbox2);
  gtk_widget_show(vbox2);

  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  prompt_args=gtk_check_button_new_with_label("Prompt for program arguments");
  gtk_box_pack_start(GTK_BOX(hbox), prompt_args, TRUE, TRUE, 0);
  gtk_widget_show(prompt_args);  
  
  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  but=gtk_button_new_with_label("Create");
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     GTK_SIGNAL_FUNC(begin_save), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), but, TRUE, FALSE, 0);
  gtk_widget_show(but);

  /* Show our new window */
  gtk_widget_show(win);

  /* Main processing loop */
  gtk_main();

  /* We are done, save out config in case it changed */
  write_config();

  return 0;
}

static gboolean app_dropped(GtkWidget *widget, GSList *uris, gpointer data,
			    gpointer udata)
{
  GtkWidget *entry;
  GSList *local, *tmp;

  g_return_val_if_fail(GTK_IS_ENTRY(udata), FALSE);

  entry=GTK_WIDGET(udata);

  local=rox_dnd_filter_local(uris);

  if(local) {
    gtk_entry_set_text(GTK_ENTRY(entry), (const gchar *) local->data);

    for(tmp=local; tmp; tmp=tmp->next)
      g_free(tmp->data);
    g_slist_free(local);
  } else {
    gdk_beep();
    gtk_entry_set_text(GTK_ENTRY(entry), "");
  }
}

static gboolean is_dir(const gchar *dirname)
{
  struct stat statb;

  if(stat(dirname, &statb)!=0)
    return FALSE;

  return S_ISDIR(statb.st_mode);
}

static gboolean help_dropped(GtkWidget *widget, GSList *uris, gpointer data,
			    gpointer udata)
{
  GtkWidget *entry;
  GSList *local, *tmp;

  g_return_val_if_fail(GTK_IS_ENTRY(udata), FALSE);

  entry=GTK_WIDGET(udata);

  local=rox_dnd_filter_local(uris);

  if(local) {
    const gchar *dirname=(const gchar *) local->data;

    if(dirname && is_dir(dirname)) {
      gtk_entry_set_text(GTK_ENTRY(entry), dirname);
    } else {
      gdk_beep();
      gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
    
    for(tmp=local; tmp; tmp=tmp->next)
      g_free(tmp->data);
    g_slist_free(local);
  } else {
    gdk_beep();
    gtk_entry_set_text(GTK_ENTRY(entry), "");
  }
}

static gboolean icon_dropped(GtkWidget *widget, GSList *uris, gpointer data,
			    gpointer udata)
{
  GSList *local, *tmp;
  GtkWidget *icon;

  g_return_val_if_fail(GTK_IS_PIXMAP(udata), FALSE);

  icon=GTK_WIDGET(udata);

  local=rox_dnd_filter_local(uris);

  if(local) {
    const gchar *path=(const gchar *) local->data;
    gchar *oldpath;
    GdkPixmap *pixmap;
    GdkBitmap *mask;

    oldpath=gtk_object_get_data(GTK_OBJECT(icon), "path");

    pixmap=gdk_pixmap_create_from_xpm(GTK_WIDGET(win)->window,  &mask,
				      NULL, path);
    if(pixmap) {
      gtk_pixmap_set(GTK_PIXMAP(icon), pixmap, mask);
      g_free(oldpath);
      gtk_object_set_data(GTK_OBJECT(icon), "path", g_strdup(path));
    } else {
      gdk_beep();
    }
    
    for(tmp=local; tmp; tmp=tmp->next)
      g_free(tmp->data);
    g_slist_free(local);

  } else {
    gdk_beep();
  }
}

static gboolean on_path(const gchar *prog)
{
  static gchar **path=NULL;

  gchar *progdir=g_dirname(prog);
  gchar **dir;

  if(!path)
    path=g_strsplit(g_getenv("PATH"), ":", 0);

  for(dir=path; *dir; dir++)
    if(strcmp(progdir, *dir)==0) {
      g_free(progdir);
      return TRUE;
    }

  g_free(progdir);
  return FALSE;
}

static gint save_to_file(GtkWidget *widget, gchar *pathname, gpointer data)
{
  FILE *out;
  gchar *cmd, *acmd;
  gchar *mleaf;
  gchar *leaf;
  gchar *fname;
  gchar *src;
  gboolean prompt;

  prompt=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prompt_args));
  mleaf=g_basename(pathname);

  if(mkdir(pathname, 0777)!=0) {
    rox_error("Failed to make %s\n%s", pathname, strerror(errno));
    return GTK_XDS_SAVE_ERROR;
  }

  src=gtk_object_get_data(GTK_OBJECT(prog_icon), "path");
  if(src) {
    fname=g_strconcat(pathname, "/AppIcon.xpm", NULL);
    cmd=g_strdup_printf("cp %s %s", src, fname);
    if(system(cmd)!=0) {
      rox_error("Failed to make AppIcon.xpm\n%s", strerror(errno));
      g_free(fname);
      g_free(cmd);
      return GTK_XDS_SAVE_ERROR;
    }
    g_free(fname);
    g_free(cmd);
  }

  fname=g_strconcat(pathname, "/AppRun", NULL);
  out=fopen(fname, "w");
  if(!out) {
    rox_error("Failed to make %s\n%s", fname, strerror(errno));
    return GTK_XDS_SAVE_ERROR;
  }
  fprintf(out, "#!/bin/sh\n\n");
  fprintf(out, "APP_DIR=`dirname $0` export APP_DIR\n");
  cmd=gtk_editable_get_chars(GTK_EDITABLE(prog_name), 0, -1);
  if(on_path(cmd))
    acmd=g_basename(cmd);
  else
    acmd=cmd;
  if(!prompt) {
    fprintf(out, "exec %s \"$@\"\n", acmd);
  } else {
    fprintf(out, "args=`$APP_DIR/%s -p \"Options for %s\" \"$@\"`\n",
	    PROMPT_UTIL, mleaf);
    fprintf(out, "exec %s $args\n", acmd);
  }
  g_free(cmd);
  fclose(out);
  chmod(fname, 0755);
  g_free(fname);

  fname=g_strconcat(pathname, "/AppInfo.xml", NULL);
  out=fopen(fname, "w");
  if(!out) {
    rox_error("Failed to make %s\n%s", fname, strerror(errno));
  } else {
    fprintf(out, "<?xml version=\"1.0\"?>\n<AppInfo>\n  <About>\n");
    fprintf(out, "    <Purpose>Wrapper for %s</Purpose>\n", mleaf);
    fprintf(out, "    <Version>0.1.0 by AppFactory %s</Version>\n", VERSION);
    fprintf(out, "    <Authors>AppFactory by %s</Authors>\n", AUTHOR);
    fprintf(out, "    <License>GNU General Public License</License>\n");
    fprintf(out, "  </About>\n");
    fprintf(out, "  <Summary>ROX wrapper for %s</Summary>\n", mleaf);
    fprintf(out, "</AppInfo>\n");
    
    fclose(out);
  }
  g_free(fname);

  src=gtk_editable_get_chars(GTK_EDITABLE(prog_help), 0, -1);
  if(src && src[0]) {
    fname=g_strconcat(pathname, "/Help", NULL);
    cmd=g_strdup_printf("cp -r %s %s", src, pathname);
    if(system(cmd)!=0) {
      rox_error("Failed to make help files\n%s", strerror(errno));
    }
    g_free(cmd);
    g_free(fname);
  }
  g_free(src);

  if(prompt) {
    gchar *loc;

    loc=choices_find_path_load(PROMPT_UTIL, PROJECT);
    if(!loc) {
      gchar *app_dir=getenv("APP_DIR");
      if(app_dir)
	loc=g_strconcat(app_dir, "/", PROMPT_UTIL, NULL);
    }
    if(loc) {
      cmd=g_strdup_printf("cp %s %s", loc, pathname);
      if(system(cmd)!=0) {
	rox_error("Failed to copy prompt utility\n%s", strerror(errno));
      }
      g_free(cmd);
      g_free(loc);

      fname=g_strconcat(pathname, "/", PROMPT_UTIL, NULL);
      chmod(fname, 0755);
      g_free(fname);
      
    } else {
      rox_error("Failed to copy prompt utility\nfailed to find %s",
		PROMPT_UTIL);
    }
  }

  return GTK_XDS_SAVED;
}

static void saved_to_uri(GtkSavebox *savebox, gchar *uri)
{
    gtk_savebox_set_pathname(GTK_SAVEBOX(savebox), uri);
}

static void save_done(GtkSavebox *savebox)
{
  gtk_widget_hide(GTK_WIDGET(savebox));
}

/* Make a destroy-frame into a close, allowing us to re-use the window */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

static void begin_save(GtkWidget *widget, gpointer data)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  gchar *path;
  gchar *leaf;
  
  if(!save) {
    save=gtk_savebox_new();

    gtk_signal_connect(GTK_OBJECT(save), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     save);
    gtk_signal_connect(GTK_OBJECT(save), "save_to_file",
		       GTK_SIGNAL_FUNC(save_to_file), NULL);
    gtk_signal_connect(GTK_OBJECT(save), "saved_to_uri",
		       GTK_SIGNAL_FUNC(saved_to_uri), NULL);
    gtk_signal_connect(GTK_OBJECT(save), "save_done",
		       GTK_SIGNAL_FUNC(save_done), NULL);
  }

  gtk_pixmap_get(GTK_PIXMAP(prog_icon), &pixmap, &mask);
  gtk_savebox_set_icon(GTK_SAVEBOX(save), pixmap, mask);

  path=gtk_editable_get_chars(GTK_EDITABLE(prog_name), 0, -1);
  leaf=g_basename(path);
  leaf[0]=toupper(leaf[0]);
  gtk_savebox_set_pathname(GTK_SAVEBOX(save), leaf);
  g_free(path);

  gtk_widget_show(save);
}

/* Write the config to a file. We have no config to save, but you might...  */
static void write_config(void)
{
  gchar *fname;

  /* Use the choices system to get the name to save to */
  fname=choices_find_path_save("options", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    FILE *out;

    out=fopen(fname, "w");
    if(out) {
      time_t now;
      char buf[80];
      
      fprintf(out, _("# Config file for %s %s (%s)\n"), PROJECT, VERSION,
	      AUTHOR);
      fprintf(out, _("# Latest version at %s\n"), WEBSITE);
      time(&now);
      strftime(buf, 80, "%c", localtime(&now));
      fprintf(out, _("#\n# Written %s\n\n"), buf);

      /* Write config here... */
      fclose(out);
    }

    g_free(fname);
  }
}

/* Read in the config.  Again, nothing defined for this demo  */
static void read_config(void)
{
  guchar *fname;

  /* Use the choices system to locate the file to read */
  fname=choices_find_path_load("options", PROJECT);

  if(fname) {
    FILE *in;
    in=fopen(fname, "r");
    if(in) {
      char buf[1024], *line;
      char *end;

      do {
	line=fgets(buf, sizeof(buf), in);
	if(!line)
	  break;
	end=strchr(line, '\n');
	if(end)
	  *end=0;
	end=strchr(line, '#');  /* everything after # is a comment */
	if(end)
	  *end=0;

	/* Process the line here... */
	
      } while(!feof(in));

      fclose(in);
    }

    g_free(fname);
  }
}

/*
 * Pop-up menu
 * Just two entries, one shows our information window, the other quits the
 * applet
 */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0, NULL },
};

/* Save user-defined menu accelerators */
static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save("menus", PROJECT, TRUE);
  if (menurc) {
    gtk_item_factory_dump_rc(menurc, NULL, TRUE);
    g_free(menurc);
  }
}

/* Create the pop-up menu */
static GtkWidget *menu_create_menu(GtkWidget *window)
{
  GtkWidget *menu;
  GtkItemFactory	*item_factory;
  GtkAccelGroup	*accel_group;
  gint 		n_items = sizeof(menu_items) / sizeof(*menu_items);
  gchar *menurc;

  accel_group = gtk_accel_group_new();

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<system>", 
				      accel_group);

  gtk_item_factory_create_items(item_factory, n_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  /* Load any user-defined menu accelerators */
  menu = gtk_item_factory_get_widget(item_factory, "<system>");

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
    gtk_item_factory_parse_rc(menurc);
    g_free(menurc);
  }

  /* Save updated accelerators when we exit */
  atexit(save_menus);

  return menu;
}

/* Button press on our window */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  static GtkWidget *menu=NULL;
  
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    /* Pop up the menu */
    if(!menu) 
      menu=menu_create_menu(GTK_WIDGET(win));
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

/* Show the info window */
static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    /* Need to make it first.  The arguments are macros defined
     in config.h.in  */
    infowin=info_win_new(PROJECT, PURPOSE, VERSION, AUTHOR, WEBSITE);
    gtk_signal_connect(GTK_OBJECT(infowin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     infowin);
  }

  gtk_widget_show(infowin);
}
