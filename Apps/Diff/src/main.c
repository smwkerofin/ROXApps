/*
 * Diff - show the difference between 2 files in some windows
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: main.c,v 1.15 2005/10/16 11:57:13 stephen Exp $
 */
#include "config.h"

#define DEBUG              1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
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

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <rox/rox.h>
#include <rox/rox_dnd.h>

#define WIN_WIDTH  320
#define WIN_HEIGHT 240

typedef struct diff_window {
  GtkWidget *win;
  GtkWidget *file[2];
  GtkWidget *diffs;
  gchar *fname[2];
  int tag;
  int pid;
  gboolean last_was_nl;
  GtkTextTag *ctag;
  GtkTextTag *add, *del, *chn, *ctl;
  gboolean unified;
} DiffWindow;

/* Currently only support one window at a time */
static DiffWindow *window;

static GdkColormap *cmap;
static GdkColor col_add={0, 0, 0x8000, 0};
static GdkColor col_del={0, 0xffff, 0, 0};
static GdkColor col_chn={0, 0, 0, 0xffff};
static GdkColor col_ctl={0, 0xffff, 0, 0xffff};

typedef struct options {
  gchar *font_name;
  gboolean use_unified;
} Options;

static Options options={
  "fixed", FALSE
};

static ROXOption o_font_name;
static ROXOption o_use_unified;

/* Declare functions in advance */
static DiffWindow *make_window(void);
static void add_menu_entries(GtkTextView *view, GtkMenu *menu, DiffWindow *);
static gboolean show_menu(GtkWidget *widget, DiffWindow *window);
static void setup_config(void);
static void show_info_win(void);        /* Show information box */
static void show_choices_win(void);     /* Show configuration window */
static void read_config(void);          /* Read configuration */

static gboolean load_from_uri(GtkWidget *widget, GSList *uris, gpointer data,
			      gpointer udata);
static gboolean load_from_xds(GtkWidget *widget, const char *path,
			      gpointer data, gpointer udata);
static void show_diffs(DiffWindow *win);

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vh]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
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
  printf("  libxml version %d\n", LIBXML_VERSION);
}

/* Main.  Here we set up the gui and enter the main loop */
int main(int argc, char *argv[])
{
  const gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  int c, do_exit, nerr;

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  /* What is the directory where our resources are? (set by AppRun) */
  app_dir=rox_get_app_dir();
#ifdef HAVE_BINDTEXTDOMAIN
  /* More (untested) i18n support */
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif
  
  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  /* Initialise ROX and X/GDK/GTK */
  rox_init_with_domain(PROJECT, "kerofin.demon.co.uk", &argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  cmap=gdk_rgb_get_cmap();
  gtk_widget_push_colormap(cmap);
  
  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, "vh"))!=EOF)
    switch(c) {
    case 'h':
      usage(argv[0]);
      do_exit=TRUE;
      break;
    case 'v':
      do_version();
      do_exit=TRUE;
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

  /* Init choices and read them in */
  setup_config();

  /* Set up colours */
  gdk_color_alloc(cmap, &col_add);
  gdk_color_alloc(cmap, &col_del);
  gdk_color_alloc(cmap, &col_chn);
  gdk_color_alloc(cmap, &col_ctl);

  /* Make a window */
  window=make_window();
  rox_add_window(window->win);

  /* Show our new window */
  gtk_widget_show(window->win);

  /* Main processing loop */
  rox_main_loop();

  return 0;
}

static void opts_changed(void)
{
  if(o_font_name.has_changed) {
    PangoFontDescription *pfd;
    
    pfd=pango_font_description_from_string(o_font_name.value);
    gtk_widget_modify_font(window->file[0], pfd);
    gtk_widget_modify_font(window->file[1], pfd);
    gtk_widget_modify_font(window->diffs, pfd);
    pango_font_description_free(pfd);
  }
  /*fprintf(stderr, "in opts_changed, unified=%d %d\n",
    o_use_unified.int_value, o_use_unified.has_changed);*/
}

static void setup_config(void)
{
  options.font_name=g_strdup(options.font_name);
  
  read_config();

  rox_option_add_string(&o_font_name, "font", options.font_name);
  rox_option_add_int(&o_use_unified, "unified", options.use_unified);

  rox_option_add_notify(opts_changed);
}

static DiffWindow *make_window()
{
  DiffWindow *window;
  GtkWidget *win;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *text;
  GtkWidget *scrw;
  GtkTextBuffer *buf;

  window=g_new(DiffWindow, 1);
  
  /* Create window */
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "diff");
  gtk_window_set_title(GTK_WINDOW(win), _("Diff"));
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");

  /* We want to pop up a menu */
  g_signal_connect(win, "popup-menu", G_CALLBACK(show_menu), window);

  window->win=win;

  /* A vbox contains widgets arranged vertically */
  vbox=gtk_vbox_new(FALSE, 4);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  /* An hbox contains widgets arranged horizontally.  We put it inside the
     vbox */
  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, WIN_WIDTH, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), scrw, TRUE, TRUE, 2);

  text=gtk_text_view_new();
  gtk_widget_set_name(text, "file 1");
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_NONE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);

  rox_dnd_register_full(text, 0, load_from_uri, load_from_xds, window);
  g_signal_connect(text, "populate-popup", G_CALLBACK(add_menu_entries),
		   window);
  window->file[0]=text;
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, WIN_WIDTH, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), scrw, TRUE, TRUE, 2);
  
  text=gtk_text_view_new();
  gtk_widget_set_name(text, "file 2");
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_NONE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);

  rox_dnd_register_full(text, 0, load_from_uri, load_from_xds, window);
  rox_dnd_register_full(scrw, 0, load_from_uri, load_from_xds, window);
  g_signal_connect(text, "populate-popup", G_CALLBACK(add_menu_entries),
		   window);
  window->file[1]=text;
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, -1, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(vbox), scrw, TRUE, TRUE, 2);
  
  text=gtk_text_view_new();
  gtk_widget_set_name(text, "diffs");
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_NONE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);
  g_signal_connect(text, "populate-popup", G_CALLBACK(add_menu_entries),
		   window);
  buf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));

  window->diffs=text;
  window->last_was_nl=TRUE;
  window->unified=FALSE;

  window->ctag=NULL;
  window->add=gtk_text_buffer_create_tag(buf, "add",
					 "foreground", "dark green", NULL);
  window->del=gtk_text_buffer_create_tag(buf, "delete", "foreground", "red",
					 NULL);
  window->chn=gtk_text_buffer_create_tag(buf, "change", "foreground", "blue",
					 NULL);
  window->ctl=gtk_text_buffer_create_tag(buf, "control",
					 "foreground", "orange", NULL);

  if(o_font_name.value) {
    PangoFontDescription *pfd;
    
    pfd=pango_font_description_from_string(o_font_name.value);
    gtk_widget_modify_font(window->file[0], pfd);
    gtk_widget_modify_font(window->file[1], pfd);
    gtk_widget_modify_font(window->diffs, pfd);
    pango_font_description_free(pfd);
  }
    
   return window;
}


/* Read in the config.  This is the old config, pre-options system.*/
static void read_config(void)
{
  gchar *fname;

  /* Use the choices system to locate the file to read */
  fname=rox_choices_load("options.xml", PROJECT, "kerofin.demon.co.uk");

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr node, root;
    const xmlChar *string;

    doc=xmlParseFile(fname);
    if(!doc) {
      g_free(fname);
      return;
    }

    root=xmlDocGetRootElement(doc);
    if(!root) {
      g_free(fname);
      xmlFreeDoc(doc);
      return;
    }

    if(strcmp((char *) root->name, PROJECT)!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process your elements here */
      if(strcmp((char *) node->name, "font")==0) {
	string=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!string)
	  continue;
	if(options.font_name)
	  g_free(options.font_name);
	options.font_name=g_strdup((char *) string);
	xmlFree(string);

      } else if(strcmp((char *) node->name, "unified")==0) {
	string=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!string)
	  continue;
	if(strcmp((char *) string, "yes")==0)
	  options.use_unified=TRUE;
	else if(strcmp((char *) string, "no")==0)
	  options.use_unified=FALSE;
	xmlFree(string);

      }
    }

    xmlFreeDoc(doc);
    g_free(fname);
  }
}

/*
 * Pop-up menu
 */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),       NULL, show_info_win, 0, "<StockItem>",
                                               GTK_STOCK_DIALOG_INFO },
  { N_("/Choices..."), NULL, show_choices_win, 0, "<StockItem>",
                                               GTK_STOCK_PREFERENCES },
  { N_("/Quit"),       NULL, rox_main_quit, 0, "<StockItem>",
                                               GTK_STOCK_QUIT},
};

/* Save user-defined menu accelerators */
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
static GtkWidget *menu_create_menu(GtkWidget *window, const gchar *name)
{
  GtkWidget *menu;
  static GtkItemFactory *item_factory;
  GtkAccelGroup	*accel_group;
  gint 		n_items = sizeof(menu_items) / sizeof(*menu_items);
  gchar *menurc;

  accel_group = gtk_accel_group_new();

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, name, 
				      accel_group);

  gtk_item_factory_create_items(item_factory, n_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Load any user-defined menu accelerators */
  menu = gtk_item_factory_get_widget(item_factory, name);

  menurc=rox_choices_load("menus", PROJECT, "kerofin.demon.co.uk");
  if(menurc) {
    gtk_accel_map_load(menurc);
    g_free(menurc);
  }

  /* Save updated accelerators when we exit */
  atexit(save_menus);

  return menu;
}

static gboolean show_menu(GtkWidget *widget, DiffWindow *window)
{
  GdkEvent *event;
  int button=0;
  guint32 time=0;
  static GtkWidget *popup_menu=NULL;
  
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
    popup_menu=menu_create_menu(GTK_WIDGET(window->win), "<system>");
    
  gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
		 button, time);
  
  return TRUE;
}

static void add_menu_entries(GtkTextView *view, GtkMenu *menu,
			     DiffWindow *window)
{
  GtkWidget *popup_menu;
  GtkWidget *sep, *item;

  popup_menu=menu_create_menu(window->win, "<text>");
    
  sep=gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
  gtk_widget_show(sep);

  item=gtk_menu_item_new_with_label(_("Diff"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), popup_menu);
  gtk_widget_show(item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

/* Show the info window */
static void show_info_win(void)
{
  GtkWidget *infowin;
  
  infowin=rox_info_win_new_from_appinfo(PROJECT);
  rox_add_window(infowin);

  gtk_widget_show(infowin);
}

static gboolean load_window_from(GtkWidget *widget, const char *fname,
				 DiffWindow *win)
{
  GtkTextBuffer *text;
  gboolean ok=FALSE;
  FILE *in;
  char buf[BUFSIZ];
  int nb;
  GdkFont *font=NULL;
  GtkTextIter start, end;
  
  g_return_val_if_fail(widget!=NULL, FALSE);
  g_return_val_if_fail(GTK_IS_TEXT_VIEW(widget), FALSE);
  g_return_val_if_fail(fname!=NULL, FALSE);
  g_return_val_if_fail(win!=NULL, FALSE);

  dprintf(3, "Load '%s' into %p (data at %p)", fname, widget, win);

  in=fopen(fname, "r");
  if(!in) {
    rox_error("Failed to read %s: %s", fname, g_strerror(errno));
    return FALSE;
  }

  text=gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
  gtk_text_buffer_get_start_iter(text, &start);
  gtk_text_buffer_get_end_iter(text, &end);
  /*gtk_text_freeze(text);*/
  gtk_text_buffer_delete(text, &start, &end);
  do {
    nb=fread(buf, 1, sizeof(buf), in);
    if(nb>0) {
      gtk_text_buffer_get_end_iter(text, &end);
      gtk_text_buffer_insert(text, &end, buf, nb);
    }
  } while(nb>0);
  if(nb==0)
    ok=TRUE;
  /*gtk_text_thaw(text);*/
  fclose(in);

  return ok;
}

static gboolean load_from_uri(GtkWidget *widget, GSList *uris, gpointer data,
			      gpointer udata)
{
  DiffWindow *win=(DiffWindow *) udata;
  GSList *files=rox_dnd_filter_local(uris);
  gboolean ok=FALSE;

  dprintf(2, "load_from_uri %p %p", uris, files);

  if(!files)
    return FALSE;

  dprintf(3, "first uri=%s", (char *) uris->data);

  if(files->data) {
    const char *fname=(const char *) files->data;

    ok=load_window_from(widget, fname, win);
  }

  rox_dnd_local_free(files);

  if(ok)
    show_diffs(win);

  return ok;
}

static gboolean load_from_xds(GtkWidget *widget, const char *path,
			      gpointer data, gpointer udata)
{
  DiffWindow *win=(DiffWindow *) udata;

  dprintf(3, "load_from_xds: %s", path);

  if(load_window_from(widget, path, win)) {
    show_diffs(win);
    return TRUE;
  }
  return FALSE;
}

static void clean_up(DiffWindow *win)
{
  int f;
  int stat;

  for(f=0; f<2; f++) {
    if(win->fname[f]) {
      unlink(win->fname[f]);
      g_free(win->fname[f]);
    }
  }

  gtk_input_remove(win->tag);
  waitpid((pid_t) win->pid, &stat, WNOHANG);
}

static void write_window_to(GtkWidget *widget, const char *fname,
			    DiffWindow *win)
{
  GtkTextBuffer *text;
  FILE *out;
  int nb;
  guint len;
  GtkTextIter start, end;
  gchar *txt;

  g_return_if_fail(widget!=NULL);
  g_return_if_fail(GTK_IS_TEXT_VIEW(widget));
  g_return_if_fail(fname!=NULL);
  g_return_if_fail(win!=NULL);

  text=gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
  gtk_text_buffer_get_start_iter(text, &start);
  gtk_text_buffer_get_end_iter(text, &end);

  out=fopen(fname, "w");
  if(!out) {
    rox_error("Failed to write to %s: %s", fname, g_strerror(errno));
    return;
  }

  txt=gtk_text_buffer_get_text(text, &start, &end, TRUE);
  len=g_utf8_strlen(txt, -1);

  do {
    nb=fwrite(txt, 1, len, out);
    if(nb<0) {
      rox_error("Failed to write to %s: %s", fname, g_strerror(errno));
      fclose(out);
      g_free(txt);
      return;
    }
    len-=nb;
  } while(len>0);

  g_free(txt);

  fclose(out);
}

static void select_colour(char *line, DiffWindow *win)
{
  win->ctag=NULL;
  if(!win->unified) {
    if(line[0]=='+')
      win->ctag=win->add;
    else if(strncmp(line, "- ", 2)==0)
      win->ctag=win->del;
    else if(line[0]=='!')
      win->ctag=win->chn;
    else if(line[0]=='*' || strncmp(line, "--", 2)==0)
      win->ctag=win->ctl;
  } else {
    if(strncmp(line, "--- ", 4)==0 || strncmp(line, "+++ ", 4)==0 ||
       line[0]=='@')
      win->ctag=win->ctl;
    else if(line[0]=='+')
      win->ctag=win->add;
    else if(line[0]=='-')
      win->ctag=win->del;
    else if(line[0]=='!')
      win->ctag=win->chn;
  }
}
static void process_diff_line(GtkWidget *text, char *line, DiffWindow *win)
{
  GtkTextBuffer *buf;
  GtkTextIter end;

  if(win->last_was_nl) {
    select_colour(line, win);
  }
  
  buf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_buffer_insert_with_tags(buf, &end, line, strlen(line),
				   win->ctag, NULL);
}

static void diff_reader(gpointer data, gint fd, GdkInputCondition condition)
{
  DiffWindow *win=(DiffWindow *) data;
  static char *buf=NULL;
  static int bufsize=0;
  int nbready, nbread;
  char *nl;
  gchar *str;

  if(ioctl(fd, FIONREAD, &nbready)<0)
    return;

  if(nbready<1) {
    close(fd);

    clean_up(win);
    return;
  }
  if(nbready>bufsize-1) {
    bufsize=nbready+1;
    buf=realloc(buf, bufsize);
  }

  nbread=read(fd, buf, nbready);
  if(nbread>1) {
    buf[nbread]=0;

    nl=buf-1;
    do {
      char *st=nl+1;
      nl=strchr(st, '\n');
      if(nl) {
	str=g_strndup(st, nl-st+1);
	process_diff_line(win->diffs, str, win);
	g_free(str);
	win->last_was_nl=TRUE;
      } else {
	process_diff_line(win->diffs, st, win);
	win->last_was_nl=FALSE;
      }
    } while(nl);
  }
}

static void show_diffs(DiffWindow *win)
{
  int p[2];
  int pid;
  gchar *cmd;
  GtkTextIter start, end;
  GtkTextBuffer *buf;
#ifdef HAVE_MKSTEMP
  char tnam[1024];
  int fd;
#endif

  win->unified=o_use_unified.int_value;

#ifdef HAVE_MKSTEMP
  strcpy(tnam, "/tmp/rox.Diff.XXXXXX");
  fd=mkstemp(tnam);
  close(fd);
  win->fname[0]=g_strdup(tnam);
#else
  win->fname[0]=g_strdup(tmpnam(NULL));
#endif
  write_window_to(win->file[0], win->fname[0], win);

#ifdef HAVE_MKSTEMP
  strcpy(tnam, "/tmp/rox.Diff.XXXXXX");
  fd=mkstemp(tnam);
  close(fd);
  win->fname[1]=g_strdup(tnam);
#else
  win->fname[1]=g_strdup(tmpnam(NULL));
#endif
  write_window_to(win->file[1], win->fname[1], win);

  pipe(p);
  pid=fork();
  if(pid<0) {
    rox_error("Failed to fork: %s", g_strerror(errno));
    return;
  }

  if(!pid) {
    dup2(p[1], 1);

#ifdef HAVE_UNIFIED_DIFF
    if(win->unified)
      cmd=g_strdup_printf("diff -u %s %s", win->fname[0], win->fname[1]);
    else
      cmd=g_strdup_printf("diff -c %s %s", win->fname[0], win->fname[1]);
#else
    cmd=g_strdup_printf("diff -c %s %s", win->fname[0], win->fname[1]);
#endif
    /*fprintf(stderr, "cmd=%s\n", cmd);*/
    execlp("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(1);
  }
  win->pid=pid;
  close(p[1]);

  /*gtk_text_freeze(GTK_TEXT(win->diffs));*/
  buf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->diffs));
  gtk_text_buffer_get_start_iter(buf, &start);
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_buffer_delete(buf, &start, &end);

  win->last_was_nl=TRUE;
  win->tag=gdk_input_add(p[0], GDK_INPUT_READ, diff_reader,
		    win);
}

static void show_choices_win(void)
{
  rox_options_show();
}

/*
 * $Log: main.c,v $
 * Revision 1.15  2005/10/16 11:57:13  stephen
 * Update for ROX-CLib changes, many externally visible symbols
 * (functions and types) now have rox_ or ROX prefixes.
 * Can get ROX-CLib via 0launch.
 *
 * Revision 1.14  2005/10/08 11:10:41  stephen
 * Bugfix: selection of unified mode from options was broken, reported by
 *         Peter Hyman.
 *
 * Revision 1.13  2004/11/21 13:11:28  stephen
 * Use new ROX-CLib features
 *
 * Revision 1.12  2004/04/13 10:31:49  stephen
 * Got stock icon wrong
 *
 * Revision 1.11  2004/04/13 10:25:32  stephen
 * Remove dead code.  Stock items in menus.
 *
 * Revision 1.10  2003/06/22 09:04:41  stephen
 * Use new options system.  Can set font for text windows
 *
 * Revision 1.9  2003/03/05 15:30:40  stephen
 * First pass at conversion to GTK 2.
 *
 * Revision 1.8  2002/12/14 15:55:20  stephen
 * Added support for unified diffs and running from paths with spaces.
 *
 * Revision 1.7  2002/08/24 16:39:58  stephen
 * Fix compilation problem with libxml2.
 *
 * Revision 1.6  2002/05/14 10:35:08  stephen
 * Fix for getting incomplete lines because of buffering
 *
 * Revision 1.5  2002/03/04 11:27:24  stephen
 * Added support for shared ROX-CLib.
 *
 * Revision 1.4  2002/01/30 10:17:41  stephen
 * Added -h and -v options.
 *
 * Revision 1.3  2001/12/21 09:54:26  stephen
 * Added some debug lines (not ready for new release yet).
 *
 * Revision 1.2  2001/11/29 15:52:33  stephen
 * Test for <sys/filio.h>
 *
 * Revision 1.1.1.1  2001/11/23 13:02:16  stephen
 * Coloured diff
 *
 */
