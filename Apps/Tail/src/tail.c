/*
 * Tail - GTK version of tail -f
 *
 * $Id: tail.c,v 1.13 2002/03/04 11:54:26 stephen Exp $
 */

#include "config.h"

#define DEBUG         1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#include <fcntl.h>

/* These next two are for internationalization */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkdnd.h>

#include "infowin.h"
#include "gtksavebox.h"
#include "choices.h"
#include "rox_debug.h"
#include "rox_dnd.h"

static GtkWidget *win;
static GtkWidget *text;
static GtkWidget *changed=NULL;
static GtkWidget *errwin=NULL;
static GtkWidget *errmess=NULL;
static GtkWidget *menu;

static int fd=-1;
static off_t pos;
static off_t size;
static gchar *fname=NULL;
static gchar *fixed_title=NULL;
static guint update_tag=0;
static time_t last_change;

static gint check_file(gpointer unused);
static void set_file(const char *);
static void set_fd(int nfd);
static void show_info_win(void);
static gboolean got_uri_list(GtkWidget *widget, GSList *uris,
					gpointer data, gpointer udata);
static void window_updated(time_t when);

static void file_saveas_proc(void);
/*
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win);
*/
static void add_menu_entries(GtkTextView *view, GtkMenu *menu, gpointer);
static gboolean show_menu(GtkWidget *widget, gpointer);

static GtkItemFactoryEntry menu_items[] = {
  {"/_Info", "<control>I", show_info_win, 0, NULL},
  {"/_Save text...", "<control>S", file_saveas_proc, 0, NULL},
  {"/_Quit","<control>Q",  gtk_main_quit, 0, NULL },
};

#define MENU_TYPE GTK_TYPE_MENU
#define MENU_NAME "<popup>"
#define MENU_FNAME "popup_menus"

static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save(MENU_FNAME, PROJECT, TRUE);
  if (menurc) {
    gtk_accel_map_save(menurc);
    g_free(menurc);
  }
}

static GtkWidget *get_main_menu(GtkWidget *window, const gchar *name)
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
  gchar *menurc;
  GtkWidget *menu;

  accel_group = gtk_accel_group_new ();

  /* This function initializes the item factory.
     Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
              or GTK_TYPE_OPTION_MENU.
     Param 2: The path of the menu.
     Param 3: A pointer to a gtk_accel_group.  The item factory sets up
              the accelerator table while generating menus.
  */
  item_factory = gtk_item_factory_new (MENU_TYPE, name, accel_group);
  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  menu=gtk_item_factory_get_widget (item_factory, name);
  
  menurc=choices_find_path_load(MENU_FNAME, PROJECT);
  if(menurc) {
    gtk_accel_map_load(menurc);
    g_free(menurc);
  }

  atexit(save_menus);

  return menu;
}

static void detach(GtkWidget *widget, GtkMenu *menu)
{
  dprintf(4, "detach");
}

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vhc] [-t title] file\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  -c\tdon't show last change time\n");
  printf("  -t title\ttitle string for window\n");
  printf("  file\tfile to monitor\n");
}

static void do_version(void)
{
  printf("%s %s\n", PROJECT, VERSION);
  printf("%s\n", PURPOSE);
  printf("%s\n", WEBSITE);
  printf("Copyright 2002 %s\n", AUTHOR);
  printf("Distributed under the terms of the GNU General Public License.\n");
  printf("(See the file COPYING in the Help directory).\n");
  printf("%s last compiled %s\n", __FILE__, __DATE__);
  printf("ROX-CLib version %s\n", rox_clib_version_string());

  printf("\nCompile time options:\n");
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
}

int main(int argc, char *argv[])
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *scr;
  GtkWidget *mbar;
  gchar *rcfile;
  gboolean show_change_time=TRUE;
  int c, do_exit, nerr;
  const gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  gtk_init(&argc, &argv);
  rox_debug_init(PROJECT);
  
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  /* What is the directory where our resources are? (set by AppRun) */
  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  /* More (untested) i18n support */
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif

  choices_init();
  rox_dnd_init();

  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, "vhct:"))!=EOF)
    switch(c) {
    case 'h':
      usage(argv[0]);
      do_exit=TRUE;
      break;
    case 'v':
      do_version();
      do_exit=TRUE;
      break;
    case 't':
      fixed_title=g_strdup(optarg);
      break;
    case 'c':
      show_change_time=FALSE;
      break;
    default:
      nerr++;
      break;
    }
  if(nerr) {
    fprintf(stderr, "%s: invalid options\n", argv[0]);
    usage(argv[0]);
    exit(10);
  }
  if(do_exit)
    exit(0);

  rcfile=choices_find_path_load("gtkrc", PROJECT);
  if(rcfile) {
    gtk_rc_parse(rcfile);
    g_free(rcfile);
  }

  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");
  gtk_window_set_title(GTK_WINDOW(win), fixed_title? fixed_title: "Tail");
  gtk_window_set_wmclass(GTK_WINDOW(win), "Tail", PROJECT);
  rox_dnd_register_uris(win, 0, got_uri_list, NULL);
  g_signal_connect(win, "popup-menu", G_CALLBACK(show_menu), win);
  
  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  scr=gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_show(scr);
  gtk_widget_set_usize(scr, 640, 480/2);
  gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 2);
  
  text=gtk_text_view_new();
  gtk_widget_set_name(text, "tail text");
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scr), text);
  gtk_widget_show(text);
  rox_dnd_register_uris(text, 0, got_uri_list, NULL);
  g_signal_connect(text, "populate-popup", G_CALLBACK(add_menu_entries),
		   win);

  if(show_change_time) {
    changed=gtk_label_new("Last change:");
    gtk_widget_show(changed);
    gtk_box_pack_start(GTK_BOX(vbox), changed, FALSE, FALSE, 2);
  }
  
  gtk_widget_show(win);

  if(argv[optind] && strcmp(argv[optind], "-")!=0)
    set_file(argv[optind]);
  else
    set_fd(fileno(stdin));
  
  update_tag=gtk_timeout_add(500, (GtkFunction) check_file, NULL);

  /* Create the pop-up menu now so the menu accelerators are loaded */
  menu=get_main_menu(GTK_WIDGET(win), MENU_NAME);

  gtk_main();

  return 0;
}

static void window_updated(time_t when)
{
  char buf[80];
  struct tm *tim;

  if(!changed)
    return;

  if(when<=0)
    time(&when);

  tim=localtime(&when);

  strftime(buf, sizeof(buf), "Last change: %c", tim);

  gtk_label_set_text(GTK_LABEL(changed), buf);
}

static void append_text_from_file(int fd, gboolean scroll)
{
  GtkTextBuffer *tbuf;
  char buf[BUFSIZ];
  size_t m=sizeof(buf)-1;
  ssize_t nr;
  gint pos;
  int ready;
  GtkTextIter start, end;

  tbuf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
  do {
    if(ioctl(fd, FIONREAD, &ready)<0)
      break;

    m=sizeof(buf)-1;
    dprintf(5, "%d %d %d", fd, m, ready);
    if(ready<m)
      m=ready;
    dprintf(5, "%d %d %d", fd, m, ready);

    nr=read(fd, buf, m);
    if(nr<1)
      break;
    buf[nr]=0;
    dprintf(4, "%d, %s", nr, buf);

    gtk_text_buffer_get_end_iter(tbuf, &end);
    gtk_text_buffer_insert(tbuf, &end, buf, nr);
  } while(TRUE);

  if(scroll) {
    gtk_text_buffer_get_end_iter(tbuf, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text), &end, 0.0, FALSE,
				 0.5, 1.0);
  }
    
}

static gint check_file(gpointer unused)
{
  struct stat statb;
  
  if(fd<0)
    return TRUE;

  dprintf(3, "check %d", fd);
  if(fstat(fd, &statb)<0) {
    perror(fname);
    return TRUE;
  }

  dprintf(3, "size=%d (was %d)", statb.st_size, size);
  if(statb.st_size==size)
    return TRUE;

  if(statb.st_size>size) {
    append_text_from_file(fd, TRUE);
    
    pos=lseek(fd, (off_t) 0, SEEK_END);
    size=statb.st_size;
    
  } else {
    if(fname)
      set_file(fname);
    else {
      /* stdin was truncated?? */
      dprintf(2, "file shorter and no file name set!");
      size=statb.st_size;
    }
  }

  return TRUE;
}

static void set_file(const char *name)
{
  GtkTextBuffer *tbuf;
  struct stat statb;
  gchar *title, *tmp;
  int nfd;
  int npos;
  GtkTextIter start, end;

  dprintf(1, "Set file to %s", name);

  nfd=open(name, O_RDONLY);
  if(nfd<0) {
    rox_error("Failed to open\n%s\nfor reading", name);
    return;
  }

  if(fstat(nfd, &statb)<0) {
    rox_error("Failed to scan\n%s", name);
    return;
  }

  dprintf(2, "file is on %d and %d long", nfd, statb.st_size);
  
  tbuf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
  gtk_text_buffer_get_start_iter(tbuf, &start);
  gtk_text_buffer_get_end_iter(tbuf, &end);
  gtk_text_buffer_delete(tbuf, &start, &end);

  append_text_from_file(nfd, FALSE);
  npos=lseek(nfd, (off_t) 0, SEEK_END);

  window_updated(statb.st_mtime);

  if(!fixed_title) {
    title=g_strconcat("Tail: ", name, NULL);
    gtk_window_set_title(GTK_WINDOW(win), title);
    dprintf(2, "set title %s", title);
    g_free(title);
  }

  if(fd)
    close(fd);
  fd=nfd;
  pos=npos;
  size=statb.st_size;

  tmp=g_strdup(name);
  if(fname)
    g_free(fname);
  fname=tmp;
}

static void set_fd(int nfd)
{
  append_text_from_file(nfd, FALSE);
  
  if(!fixed_title)
    gtk_window_set_title(GTK_WINDOW(win), "Tail: (stdin)");
  if(fd)
    close(fd);
  fd=nfd;
  pos=lseek(fd, (off_t) 0, SEEK_CUR);
  if(fname)
    g_free(fname);
  fname=NULL;

  window_updated((time_t) -1);
}

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    infowin=info_win_new(PROGRAM, PURPOSE, VERSION, AUTHOR, WEBSITE);
    gtk_window_set_wmclass(GTK_WINDOW(infowin), "Info", PROGRAM);
  }

  gtk_window_set_position(GTK_WINDOW(infowin), GTK_WIN_POS_MOUSE);
  gtk_widget_show(infowin);
}

static void hide_window(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(errwin);
}


static gint save_to_file(GtkSavebox *savebox, gchar *pathname)
{
  GtkTextBuffer *tbuf;
  FILE *out;
  gint state=GTK_XDS_SAVED;
  guint len;
  GtkTextIter start, end;
  gchar *txt;

  tbuf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
  gtk_text_buffer_get_start_iter(tbuf, &start);
  gtk_text_buffer_get_end_iter(tbuf, &end);

  errno=0;
  out=fopen(pathname, "w");
  if(out) {
    size_t nb;

    txt=gtk_text_buffer_get_text(tbuf, &start, &end, TRUE);

    if(txt) {
      len=g_utf8_strlen(txt, -1);
      nb=fwrite(txt, 1, len, out);

      if(nb<len) {
	rox_error("Error writing to %s\n%s", pathname, strerror(errno));
	state=GTK_XDS_SAVE_ERROR;
      }
      
    } else {
      rox_error("Failed to get text!");
      state=GTK_XDS_SAVE_ERROR;
    }

    fclose(out);
    
  } else {
    rox_error("Failed to open %s for writing", pathname);
    state=GTK_XDS_SAVE_ERROR;
  }

  return state;
}

static void saved_to_uri(GtkSavebox *savebox, gchar *uri)
{
    gtk_savebox_set_pathname(GTK_SAVEBOX(savebox), uri);
}

static void save_done(GtkSavebox *savebox)
{
  gtk_widget_hide(GTK_WIDGET(savebox));
}

#include "default.xpm"

static void file_saveas_proc(void)
{
  static GtkWidget *savebox=NULL;

  if(!savebox) {
    GdkPixbuf *pixbuf=NULL;
    gchar *ipath;
    GError *err=NULL;
    
    savebox=gtk_savebox_new(_("Save"));
    gtk_signal_connect(GTK_OBJECT(savebox), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     savebox);
    gtk_signal_connect(GTK_OBJECT(savebox), "save_to_file", 
		     GTK_SIGNAL_FUNC(save_to_file), 
		     savebox);
    gtk_signal_connect(GTK_OBJECT(savebox), "saved_to_uri", 
		     GTK_SIGNAL_FUNC(saved_to_uri), 
		     savebox);
    gtk_signal_connect(GTK_OBJECT(savebox), "save_done", 
		     GTK_SIGNAL_FUNC(save_done), 
		     savebox);
    gtk_savebox_set_pathname(GTK_SAVEBOX(savebox), "tail.txt");

    ipath=choices_find_path_load("text_plain.xpm", "MIME-icons");
    if(!ipath)
      ipath=choices_find_path_load("text.xpm", "MIME-icons");
    if(ipath) {
      pixbuf=gdk_pixbuf_new_from_file(ipath, &err);
      g_free(ipath);
    }
    if(!pixbuf) {
      pixbuf=gdk_pixbuf_new_from_xpm_data(default_xpm);
    }
    gtk_savebox_set_icon(GTK_SAVEBOX(savebox), pixbuf);

    
  }

  gtk_widget_show(savebox);
}

/* Button press in window */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    /* Pop up the menu */
    if(!menu) 
      menu=get_main_menu(GTK_WIDGET(win), MENU_NAME);
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

static gboolean show_menu(GtkWidget *widget, gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);
  GdkEvent *event;
  int button=0;
  guint32 time=0;
  static GtkWidget *popup_menu=NULL;

  /*printf("show_menu(%p, %p), is text view=%d\n", widget, data,
    GTK_IS_TEXT_VIEW(widget));*/
  
  if(GTK_IS_TEXT_VIEW(widget))
    return FALSE;

  event=gtk_get_current_event();
  switch(event->type) {
  case GDK_BUTTON_PRESS:
  case GDK_BUTTON_RELEASE:
    {
      GdkEventButton *bev=(GdkEventButton *) event;

      button=bev->button;
      time=bev->time;
    }
    break;
  case GDK_KEY_PRESS:
    {
      GdkEventKey *kev=(GdkEventKey *) event;
      time=kev->time;
    }
    break;
  }

  if(!popup_menu) 
    popup_menu=get_main_menu(GTK_WIDGET(win), MENU_NAME);
    
  gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
		 button, time);
  
  return TRUE;
}

static void add_menu_entries(GtkTextView *view, GtkMenu *menu,
			     gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);
  GtkWidget *popup_menu;
  GtkWidget *sep, *item;

  popup_menu=get_main_menu(win, "<text>");
    
  sep=gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
  gtk_widget_show(sep);

  item=gtk_menu_item_new_with_label(_("Tail"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), popup_menu);
  gtk_widget_show(item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

/* We've got a list of URIs from somewhere (probably a filer window). */
static gboolean got_uri_list(GtkWidget *widget, GSList *uris,
			 gpointer data, gpointer udata)
{
  GSList *files;

  files=rox_dnd_filter_local(uris);

  if(files) {
    set_file((const char *) files->data);
    rox_dnd_local_free(files);
    return TRUE;
  }

  return FALSE;
}

/*
 * $Log: tail.c,v $
 * Revision 1.13  2002/03/04 11:54:26  stephen
 * Stable release.
 *
 * Revision 1.12  2002/01/30 10:24:39  stephen
 * Add -h and -v options.
 *
 * Revision 1.11  2001/10/04 13:39:18  stephen
 * Switch over to ROX-CLib.
 * Can specify a title with -t option.
 * Add a status line giving time & date of last update, suppress with -c.
 *
 * Revision 1.10  2001/08/29 13:43:04  stephen
 * Fix drag & drop code to avoid double drops.
 *
 * Revision 1.9  2001/08/22 11:11:17  stephen
 * Improved menu handling, load a gtkrc file from choices, if there is
 * one.
 *
 * Revision 1.8  2001/07/31 07:44:48  stephen
 * Better debug output.  Save menu accelerators.  Scrolling to end of
 * window back in, unless truncated.
 *
 * Revision 1.7  2001/07/18 10:52:18  stephen
 * Better debug system, use show_error more
 *
 * Revision 1.6  2001/05/18 09:41:09  stephen
 * Made it much more ROX-like: pop-up menu, save box, etc.
 *
 * Revision 1.5  2001/05/09 10:20:05  stephen
 * Version number now in AppInfo.xml
 * Disable debug output!
 *
 * Revision 1.4  2001/04/26 13:21:13  stephen
 * Don't include sys/filio.h if it isn't there.
 * Remove code that scrolled to end of window, because it core dumped.
 * Report error if file can't be read.
 *
 * Revision 1.3  2001/04/13 11:46:09  stephen
 * Now works, including at the end of a pipe
 *
 * Revision 1.2  2001/04/12 13:59:02  stephen
 * Added info window and a load file selection
 *
 * Revision 1.1.1.1  2001/04/12 13:24:36  stephen
 * Initial version
 *
 */
