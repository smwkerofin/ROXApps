/*
 * Tail - GTK version of tail -f
 *
 * $Id: tail.c,v 1.9 2001/08/22 11:11:17 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "config.h"

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

#define DEBUG         1
#define USE_MENUBAR   0
#define USE_FILE_OPEN 0

void dprintf(int unused, const char *fmt, ...);

static GtkWidget *win;
static GtkWidget *text;
static GtkWidget *errwin=NULL;
static GtkWidget *errmess=NULL;
static GtkWidget *menu;

static int fd=-1;
static off_t pos;
static off_t size;
static gchar *fname=NULL;
static guint update_tag=0;

static gint check_file(gpointer unused);
static void set_file(const char *);
static void set_fd(int nfd);
static void show_info_win(void);
static void show_error(const char *fmt, ...);

static void dnd_init(void);
static void make_drop_target(GtkWidget *widget);
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

int main(int argc, char *argv[])
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *scr;
  GtkWidget *mbar;
  gchar *rcfile;

  gtk_init(&argc, &argv);
  choices_init();
  dnd_init();

  rcfile=choices_find_path_load("gtkrc", PROJECT);
  if(rcfile) {
    gtk_rc_parse(rcfile);
    g_free(rcfile);
  }

  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");
  gtk_window_set_title(GTK_WINDOW(win), "Tail");
  make_drop_target(win);
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
  
  /*
  hbox=gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);
  */

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

  gtk_widget_show(win);

  if(argv[1] && strcmp(argv[1], "-")!=0)
    set_file(argv[1]);
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

void dprintf(int level, const char *fmt, ...)
{
#if DEBUG
  va_list list;
  static int dlevel=-1;

  if(dlevel==-1) {
    gchar *val=g_getenv("TAIL_DEBUG_LEVEL");
    if(val)
      dlevel=atoi(val);
    if(dlevel<0)
      dlevel=0;
  }

  if(level>dlevel)
    return;

  va_start(list, fmt);
  /*vfprintf(stderr, fmt, list);*/
  g_logv(PROJECT, G_LOG_LEVEL_DEBUG, fmt, list);
  va_end(list);
#endif
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

  title=g_strconcat("Tail: ", name, NULL);
  gtk_window_set_title(GTK_WINDOW(win), title);
  dprintf(2, "set title %s", title);
  g_free(title);

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
  
  gtk_window_set_title(GTK_WINDOW(win), "Tail: (stdin)");
  if(fd)
    close(fd);
  fd=nfd;
  pos=lseek(fd, (off_t) 0, SEEK_CUR);
  if(fname)
    g_free(fname);
  fname=NULL;
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

/*
 * Drag and drop code
 */
enum {
	TARGET_RAW,
	TARGET_URI_LIST,
	TARGET_XDS,
	TARGET_STRING,
};

#define MAXURILEN 4096

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context,
			  gint x, gint y, guint time, gpointer data);
static void drag_data_received(GtkWidget *widget, GdkDragContext *context,
			       gint x, gint y,
			       GtkSelectionData *selection_data,
			       guint info, guint32 time, gpointer user_data);

static GdkAtom XdndDirectSave0;
static GdkAtom xa_text_plain;
static GdkAtom text_uri_list;
static GdkAtom application_octet_stream;

static void dnd_init(void)
{
  XdndDirectSave0 = gdk_atom_intern("XdndDirectSave0", FALSE);
  xa_text_plain = gdk_atom_intern("text/plain", FALSE);
  text_uri_list = gdk_atom_intern("text/uri-list", FALSE);
  application_octet_stream = gdk_atom_intern("application/octet-stream",
					     FALSE);
}

static void make_drop_target(GtkWidget *widget)
{
  static GtkTargetEntry target_table[]={
    {"text/uri-list", 0, TARGET_URI_LIST},
    /*{"XdndDirectSave0", 0, TARGET_XDS},*/
  };
  static const int ntarget=sizeof(target_table)/sizeof(target_table[0]);
  
  gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, target_table, ntarget,
		    GDK_ACTION_COPY|GDK_ACTION_PRIVATE);

  /*gtk_signal_connect(GTK_OBJECT(widget), "drag_drop",
		     GTK_SIGNAL_FUNC(drag_drop), NULL);*/
  gtk_signal_connect(GTK_OBJECT(widget), "drag_data_received",
		     GTK_SIGNAL_FUNC(drag_data_received), NULL);

  dprintf(3, "made %p a drop target", widget);
}

/* Is the sender willing to supply this target type? */
gboolean provides(GdkDragContext *context, GdkAtom target)
{
  GList	    *targets = context->targets;

  while (targets && ((GdkAtom) targets->data != target))
    targets = targets->next;
  
  return targets != NULL;
}

static char *get_xds_prop(GdkDragContext *context)
{
  guchar	*prop_text;
  gint	length;

  if (gdk_property_get(context->source_window,
		       XdndDirectSave0,
		       xa_text_plain,
		       0, MAXURILEN,
		       FALSE,
		       NULL, NULL,
		       &length, &prop_text) && prop_text) {
    /* Terminate the string */
    prop_text = g_realloc(prop_text, length + 1);
    prop_text[length] = '\0';
    return prop_text;
  }

  return NULL;
}

/* Convert a list of URIs into a list of strings.
 * Lines beginning with # are skipped.
 * The text block passed in is zero terminated (after the final CRLF)
 */
GSList *uri_list_to_gslist(char *uri_list)
{
  GSList   *list = NULL;

  while (*uri_list) {
    char	*linebreak;
    char	*uri;
    int	length;

    linebreak = strchr(uri_list, 13);

    if (!linebreak || linebreak[1] != 10) {
      show_error("uri_list_to_gslist: Incorrect or missing line "
		      "break in text/uri-list data");
      return list;
    }
    
    length = linebreak - uri_list;

    if (length && uri_list[0] != '#') {
	uri = g_malloc(sizeof(char) * (length + 1));
	strncpy(uri, uri_list, length);
	uri[length] = 0;
	list = g_slist_append(list, uri);
      }

    uri_list = linebreak + 2;
  }

  return list;
}

/* User has tried to drop some data on us. Decide what format we would
 * like the data in.
 */
static gboolean drag_drop(GtkWidget 	  *widget,
			  GdkDragContext  *context,
			  gint            x,
			  gint            y,
			  guint           time,
			  gpointer	  data)
{
  char *leafname=NULL;
  char *path=NULL;
  GdkAtom target = GDK_NONE;
  
  dprintf(3, "in drag_drop(%p, %p, %d, %d, %u, %p)", widget, context, x, y,
	 time, data);
  
  
  /*g_dataset_foreach(context, print_context, NULL);*/

  if(provides(context, XdndDirectSave0)) {
    leafname = get_xds_prop(context);
    if (leafname) {
      dprintf(2, "leaf is %s", leafname);
      target = XdndDirectSave0;
      g_dataset_set_data_full(context, "leafname", leafname, g_free);
    }
  } else if(provides(context, text_uri_list))
    target = text_uri_list;

  gtk_drag_get_data(widget, context, target, time);

  return TRUE;
}

char *get_local_path(const char *uri)
{
  char *host;
  char *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    host=g_strndup(host, end-host);

    if(strcmp(host, "localhost")==0) {
      g_free(host);
      return g_strdup(end);
    }

    g_free(host);
  }

  return NULL;
}

char *get_server(const char *uri)
{
  char *host, *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup("localhost");
    
    host=(char *) uri+2;
    end=strchr(host, '/');
    return g_strndup(host, end-host);
  }

  return g_strdup("localhost");
}

char *get_path(const char *uri)
{
  char *host;
  char *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    
    return g_strdup(end);
  }

  return NULL;
}

/* We've got a list of URIs from somewhere (probably a filer window).
 * If the files are on the local machine then use the name,
 * otherwise, try /net/server/path.
 */
static void got_uri_list(GtkWidget 		*widget,
			 GdkDragContext 	*context,
			 GtkSelectionData 	*selection_data,
			 guint32             	time)
{
  GSList		*uri_list;
  char		*error = NULL;
  GSList		*next_uri;
  gboolean	send_reply = TRUE;
  char		*path=NULL, *server=NULL;
  const char		*uri;

  uri_list = uri_list_to_gslist(selection_data->data);

  if (!uri_list)
    error = "No URIs in the text/uri-list (nothing to do!)";
  else {
    /*
    for(next_uri=uri_list; next_uri; next_uri=next_uri->next)
      printf("%s\n", next_uri->data);
      */

    uri=uri_list->data;

    path=get_local_path(uri);
    if(!path) {
      char *tpath=get_path(uri);
      server=get_server(uri);
      path=g_strconcat("/net/", server, tpath, NULL);
      g_free(server);
      g_free(tpath);
    }
      
  }

  if (error) {
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    /*fprintf(stderr, "%s: %s\n", "got_uri_list", error);*/
    show_error("%s: %s", "got_uri_list", error);
  } else {
    gtk_drag_finish(context, TRUE, FALSE, time);    /* Success! */

    set_file(path);
  }
  
  next_uri = uri_list;
  while (next_uri) {
      g_free(next_uri->data);
      next_uri = next_uri->next;
  }
  g_slist_free(uri_list);

  if(path)
    g_free(path);
}

/* Called when some data arrives from the remote app (which we asked for
 * in drag_drop).
 */
static void drag_data_received(GtkWidget      	*widget,
			       GdkDragContext  	*context,
			       gint            	x,
			       gint            	y,
			       GtkSelectionData *selection_data,
			       guint            info,
			       guint32          time,
			       gpointer		user_data)
{
  dprintf(4, "%p in drag_data_received", widget);
  
  if (!selection_data->data) {
    /* Timeout? */
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    return;
  }

  switch(info) {
  case TARGET_XDS:
    dprintf(3, "XDS");
    gtk_drag_finish(context, FALSE, FALSE, time);
    break;
  case TARGET_URI_LIST:
    dprintf(3, "URI list");
    got_uri_list(widget, context, selection_data, time);
    break;
  default:
    /*fprintf(stderr, "drag_data_received: unknown target\n");*/
    gtk_drag_finish(context, FALSE, FALSE, time);
    show_error("drag_data_received: unknown target");
    break;
  }
}

/*
 * $Log: tail.c,v $
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
