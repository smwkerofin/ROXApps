/*
 * Tail - GTK version of tail -f
 *
 * $Id: tail.c,v 1.11 2001/10/04 13:39:18 stephen Exp $
 */

#include "config.h"

#define DEBUG         1
#define USE_MENUBAR   0
#define USE_FILE_OPEN 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#include <fcntl.h>

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
static void show_error(const char *fmt, ...);
static gboolean got_uri_list(GtkWidget *widget, GSList *uris,
					gpointer data, gpointer udata);
static void window_updated(time_t when);

#if USE_FILE_OPEN
static void file_open_proc(void);
#endif
static void file_saveas_proc(void);
#if !USE_MENU_BAR
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win);
#endif

#if USE_MENU_BAR
static GtkItemFactoryEntry menu_items[] = {
  {"/_File", NULL, NULL, 0, "<Branch>"},
#if USE_FILE_OPEN
  {"/File/_Open file...", "<control>O", file_open_proc, 0, NULL},
#endif
  {"/File/_Save text...", "<control>S", file_saveas_proc, 0, NULL},
  {"/File/sep",     NULL,         NULL, 0, "<Separator>" },
  {"/File/E_xit","<control>X",  gtk_main_quit, 0, NULL },
  {"/_Help", NULL, NULL, 0, "<LastBranch>"},
  {"/Help/_About", "<control>A", show_info_win, 0, NULL},
};

#define MENU_TYPE GTK_TYPE_MENU_BAR
#define MENU_NAME "<main>"
#define MENU_FNAME "menus"

#else
static GtkItemFactoryEntry menu_items[] = {
  {"/_Info", "<control>I", show_info_win, 0, NULL},
#if USE_FILE_OPEN
  {"/_File", NULL, NULL, 0, "<Branch>"},
  {"/File/_Open file...", "<control>O", file_open_proc, 0, NULL},
  {"/File/_Save text...", "<control>S", file_saveas_proc, 0, NULL},
#else
  {"/_Save text...", "<control>S", file_saveas_proc, 0, NULL},
#endif
  {"/_Quit","<control>Q",  gtk_main_quit, 0, NULL },
};

#define MENU_TYPE GTK_TYPE_MENU
#define MENU_NAME "<popup>"
#define MENU_FNAME "popup_menus"

#endif

static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save(MENU_FNAME, PROJECT, TRUE);
  if (menurc) {
    gtk_item_factory_dump_rc(menurc, NULL, TRUE);
    g_free(menurc);
  }
}

static GtkWidget *get_main_menu(GtkWidget *window)
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
  item_factory = gtk_item_factory_new (MENU_TYPE, MENU_NAME, accel_group);
  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach (accel_group, GTK_OBJECT (window));

  menu=gtk_item_factory_get_widget (item_factory, MENU_NAME);
  
  menurc=choices_find_path_load(MENU_FNAME, PROJECT);
  if(menurc) {
    gtk_item_factory_parse_rc(menurc);
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

  printf("\nCompile time options:\n");
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
  printf("  Use a menu bar... %s\n", USE_MENUBAR? "yes": "no, use pop-ups");
  printf("  Have a \"File=>Open\" menu entry... %s\n",
	 USE_FILE_OPEN? "yes": "no, use drag and drop");
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

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  gtk_init(&argc, &argv);
  rox_debug_init(PROJECT);
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
#if !USE_MENU_BAR
  /* We want to pop up a menu on a button press */
  gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		     GTK_SIGNAL_FUNC(button_press), win);
  gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
#endif
  
  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

#if USE_MENU_BAR
  mbar=get_main_menu(win);
  gtk_box_pack_start(GTK_BOX(vbox), mbar, FALSE, FALSE, 2);
  gtk_widget_show(mbar);
#endif
  
  scr=gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_show(scr);
  gtk_widget_set_usize(scr, 640, 480/2);
  gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 2);
  
  text=gtk_text_new(NULL, NULL);
  gtk_widget_set_name(text, "tail text");
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scr), text);
  gtk_widget_show(text);

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

#if !USE_MENU_BAR
  /* Create the pop-up menu now so the menu accelerators are loaded */
  menu=get_main_menu(GTK_WIDGET(win));
#endif

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
  char buf[BUFSIZ];
  size_t m=sizeof(buf)-1;
  ssize_t nr;
  gint pos;
  int ready;

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

    gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buf, nr);
  } while(TRUE);

  gtk_text_set_point(GTK_TEXT(text), gtk_text_get_length(GTK_TEXT(text)));

  if(scroll) {
    GtkWidget *scr;
    GtkAdjustment *adj;
    
    scr=text->parent;
    adj=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scr));
    dprintf(3, "scr=%p, adj=%p", scr, adj);
    if(adj)
      gtk_adjustment_set_value(adj, adj->upper);
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
    gtk_text_freeze(GTK_TEXT(text));
    append_text_from_file(fd, TRUE);
    gtk_text_thaw(GTK_TEXT(text));
    window_updated(statb.st_mtime);
    
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
  struct stat statb;
  gchar *title, *tmp;
  int nfd;
  int npos;

  dprintf(1, "Set file to %s", name);

  nfd=open(name, O_RDONLY);
  if(nfd<0) {
    show_error("Failed to open\n%s\nfor reading", name);
    return;
  }

  if(fstat(nfd, &statb)<0) {
    show_error("Failed to scan\n%s", name);
    return;
  }

  dprintf(2, "file is on %d and %d long", nfd, statb.st_size);
  
  gtk_text_freeze(GTK_TEXT(text));
  gtk_editable_delete_text(GTK_EDITABLE(text), 0, -1);

  append_text_from_file(nfd, FALSE);
  npos=lseek(nfd, (off_t) 0, SEEK_END);

  gtk_text_thaw(GTK_TEXT(text));
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

static void show_error(const char *fmt, ...)
{
  va_list list;
  gchar *mess;

  if(!errwin) {
    /* Need to create it first */
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    
    errwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(errwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     errwin);
    gtk_window_set_title(GTK_WINDOW(errwin), "Error!");
    gtk_window_set_position(GTK_WINDOW(errwin), GTK_WIN_POS_CENTER);
    gtk_window_set_wmclass(GTK_WINDOW(errwin), "Error", PROGRAM);

    vbox=GTK_DIALOG(errwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

    errmess=gtk_label_new("");
    gtk_widget_show(errmess);
    gtk_box_pack_start(GTK_BOX(hbox), errmess, TRUE, TRUE, 2);
    
    hbox=GTK_DIALOG(errwin)->action_area;

    but=gtk_button_new_with_label("Ok");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, TRUE, TRUE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(hide_window), errwin);
  }
  
  va_start(list, fmt);
  mess=g_strdup_vprintf(fmt, list);
  va_end(list);

  gtk_label_set_text(GTK_LABEL(errmess), mess);
  gtk_widget_show(errwin);
  g_free(mess);
}

#if USE_FILE_OPEN
static void choose_file(GtkWidget *widget, GtkFileSelection *fs)
{
  const gchar *path=gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  set_file(path);
  
  gtk_widget_hide(GTK_WIDGET(fs));
}

static void close_filesel(GtkWidget *widget, GtkFileSelection *fs)
{
  gtk_widget_hide(GTK_WIDGET(fs));
}

static void file_open_proc(void)
{
  static GtkWidget *fs=NULL;

  if(!fs) {
    fs=gtk_file_selection_new("Select file to monitor");
    gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			"clicked", (GtkSignalFunc) choose_file, fs);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->
				  cancel_button),
		       "clicked", (GtkSignalFunc) close_filesel,fs);
    /*gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION(fs));*/
  }

  gtk_widget_show(fs);
}
#endif

static gint save_to_file(GtkSavebox *savebox, gchar *pathname)
{
  FILE *out;
  gint state=GTK_XDS_SAVED;

  errno=0;
  out=fopen(pathname, "w");
  if(out) {
    gchar *txt;
    guint len;
    size_t nb;

    txt=gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
    len=gtk_text_get_length(GTK_TEXT(text));
    if(txt) {
      nb=fwrite(txt, 1, len, out);

      if(nb<len) {
	show_error("Error writing to %s\n%s", pathname, strerror(errno));
	state=GTK_XDS_SAVE_ERROR;
      }
      
    } else {
      show_error("Failed to get text!");
      state=GTK_XDS_SAVE_ERROR;
    }

    fclose(out);
    
  } else {
    show_error("Failed to open %s for writing", pathname);
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
    GdkPixmap *pixmap;
    GdkPixmap *mask;
    gchar *ipath;
    
    savebox=gtk_savebox_new();
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
      pixmap=gdk_pixmap_create_from_xpm(savebox->window, &mask, NULL,
					ipath);
      g_free(ipath);
    } else {
      pixmap=gdk_pixmap_create_from_xpm_d(savebox->window, &mask, NULL,
					  default_xpm);
    }
    gtk_savebox_set_icon(GTK_SAVEBOX(savebox), pixmap, mask);

    
  }

  gtk_widget_show(savebox);
}

#if !USER_MENU_BAR
/* Button press in window */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    /* Pop up the menu */
    if(!menu) 
      menu=get_main_menu(GTK_WIDGET(win));
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}
#endif


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
