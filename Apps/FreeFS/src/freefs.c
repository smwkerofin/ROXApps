/*
 * freefs - Monitor free space on a single file system.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: freefs.c,v 1.19 2002/04/29 08:19:39 stephen Exp $
 */
#include "config.h"

/* Select compilation options */
#define APPLET_DND         0     /* Support drag & drop to the applet */
#define DEBUG              1     /* Set to 1 for debug messages */
#define TRY_SERVER         1     /* Use SOAP to open windows on request */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
/* Sometimes getopt is in stdlib.h, sometimes in here.  And sometimes both
   and stdio.h */
#include <unistd.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <glibtop.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

#ifdef HAVE_XML
#include <libxml/tree.h>
#include <libxml/parser.h>
#endif

#if defined(HAVE_XML) && LIBXML_VERSION>=20400
#define USE_XML 1
#else
#define USE_XML 0
#endif

#if USE_XML && TRY_SERVER
#define USE_SERVER 1
#else
#define USE_SERVER 0
#endif

#include "rox.h"
#include "choices.h"
#include "infowin.h"
#include "rox_debug.h"
#include "rox_dnd.h"
#include "rox_resources.h"
#include "rox_filer_action.h"
#include "applet.h"
#if USE_SERVER
#include "rox_soap.h"
#include "rox_soap_server.h"
#endif

#define TIP_PRIVATE "For more information see the help file"

/* GTK+ objects */
static GtkWidget *menu=NULL;

typedef struct options {
  guint update_sec;          /* How often to update */
  guint applet_init_size;    /* Initial size of applet */
  gboolean applet_show_dir;  /* Print name of directory on applet version */
} Options;

typedef struct free_window {
  GtkWidget *win;
  GtkWidget *fs_name, *fs_total, *fs_used, *fs_free, *fs_per;
  
  gboolean is_applet;
  guint update_tag;
  
  char *df_dir;
  
  Options options;
} FreeWindow;

static Options default_options={
  5,
  36,
  TRUE
};

typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *init_size;
  GtkWidget *show_dir;

  FreeWindow *fwin;
} OptionWidgets;

static GList *windows=NULL;

static FreeWindow *current_window=NULL;

#if USE_SERVER
static ROXSOAPServer *server=NULL;
#define FREEFS_NAMESPACE_URL WEBSITE PROJECT
#endif

static GList *fs_exclude=NULL; /* File system types not to offer on menu */

/* Call backs & stuff */
static FreeWindow *make_window(guint32 socket, const char *dir);
static gboolean update_fs_values(FreeWindow *);
static void do_update(void);
static void read_choices(void);
static void write_choices(const Options *);
#if USE_XML
static gboolean read_choices_xml(void);
static void write_choices_xml(const Options *);
#endif
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);
static gboolean handle_uris(GtkWidget *widget, GSList *uris, gpointer data,
			   gpointer udata);
#if USE_SERVER
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid, const char *path);
#endif

static void usage(const char *argv0)
{
#if USE_SERVER
  printf("Usage: %s [X-options] [gtk-options] [-vhnr] [-a XID] [dir]\n",
	 argv0);
#else
  printf("Usage: %s [X-options] [gtk-options] [-vh] [-a XID] [dir]\n", argv0);
#endif
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  -a XID\tX id of window to use in applet mode\n");
#if USE_SERVER
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
#endif
  printf("  dir\tdirectory on file sustem to monitor\n");
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
  printf("ROX-CLib version %s for GTK+ %s (built with %d.%d.%d)\n",
	 rox_clib_version_string(), rox_clib_gtk_version_string(),
	 ROX_CLIB_VERSION/10000, (ROX_CLIB_VERSION%10000)/100,
	 ROX_CLIB_VERSION%100);

  printf("\nCompile time options:\n");
  printf("  GTK+... ");
#ifdef GTK2
  printf("version 2");
#else
  printf("version 1.2.x");
#endif
  printf(" (%d.%d.%d)\n", gtk_major_version, gtk_minor_version,
	 gtk_micro_version);
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
  printf("  Support drag & drop to applet... %s\n", APPLET_DND? "yes": "no");
  printf("  Using XML... ");
  if(USE_XML)
    printf("yes (libxml version %d)\n", LIBXML_VERSION);
  else {
    printf("no (");
    if(HAVE_XML)
      printf("libxml not found)\n");
    else
    printf("libxml version %d)\n", LIBXML_VERSION);
  }
  printf("  Using SOAP server... ");
  if(USE_SERVER)
    printf("yes\n");
  else {
    printf("no");
    if(!TRY_SERVER)
      printf(", disabled");
    if(!USE_XML)
      printf(", XML not available");
    printf("\n");
  }
}

int main(int argc, char *argv[])
{
  gchar *df_dir;
  gchar *fname;
  guint xid=0;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  int c, do_exit, nerr;
#if USE_SERVER
  const char *options="vha:nr";
  gboolean new_run=FALSE;
  gboolean replace_server=FALSE;
#else
  const char *options="vha:";
#endif

  rox_debug_init(PROJECT);
  choices_init();

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=rox_resources_find(PROGRAM, "Messages", ROX_RESOURCES_NO_LANG);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif
  
  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  dprintf(5, "%d %s -> %s", argc, argv[1]? argv[1]: "NULL", argv[argc-1]);
  gtk_init(&argc, &argv);
  dprintf(5, "%d %s -> %s", argc, argv[1]? argv[1]: "NULL", argv[argc-1]);

  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, options))!=EOF)
    switch(c) {
    case 'a':
      xid=atol(optarg);
      break;
    case 'h':
      usage(argv[0]);
      do_exit=TRUE;
      break;
    case 'v':
      do_version();
      do_exit=TRUE;
      break;
#if USE_SERVER
    case 'n':
      new_run=TRUE;
      break;
    case 'r':
      replace_server=TRUE;
      break;
#endif
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

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=rox_resources_find(PROJECT, "gtkrc", ROX_RESOURCES_DEFAULT_LANG);
  if(!fname)
    fname=choices_find_path_load("gtkrc", PROGRAM);
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  /* Figure out which directory we should be monitoring */
  if(optind<argc && argv[optind])
    df_dir=g_strdup(argv[optind]);
  else
    df_dir=g_dirname(argv[0]);
  if(!g_path_is_absolute(df_dir)) {
    gchar *tmp=df_dir;
    gchar *dir=g_get_current_dir();
    df_dir=g_strconcat(dir, "/", df_dir, NULL);
    g_free(dir);
    g_free(tmp);
  }
  
  /* Set up glibtop and check now */
  dprintf(4, "set up glibtop");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_FSUSAGE)|(1<<GLIBTOP_SYSDEPS_MOUNTLIST),
		 0);
  
  rox_dnd_init();

#if USE_SERVER
  if(replace_server || !rox_soap_ping(PROJECT)) {
    dprintf(1, "Making SOAP server");
    server=rox_soap_server_new(PROJECT, FREEFS_NAMESPACE_URL);
    rox_soap_server_add_action(server, "Open", "Path", "Parent",
			       rpc_Open, NULL);
  } else if(!new_run) {
    if(open_remote(xid, df_dir)) {
      dprintf(1, "success in open_remote(%lu), exiting", xid);
      return 0;
    }
  }
#endif
  (void) make_window(xid, df_dir);
  g_free(df_dir);
  
  gtk_main();

#if USE_SERVER
  if(server)
    rox_soap_server_delete(server);
#endif

  return 0;
}

static void remove_window(FreeWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);
  /*gtk_widget_hide(win->win);*/

  gtk_timeout_remove(win->update_tag);
  g_free(win->df_dir);
  g_free(win);

  dprintf(1, "windows=%p, number of active windows=%d", windows,
	  g_list_length(windows));
  if(g_list_length(windows)<1)
    gtk_main_quit();
}

static void window_gone(GtkWidget *widget, gpointer data)
{
  FreeWindow *fw=(FreeWindow *) data;

  dprintf(1, "Window gone: %p %p", widget, fw);

  remove_window(fw);
}

static FreeWindow *make_window(guint32 xid, const char *dir)
{
  FreeWindow *fwin;
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
#ifdef GTK2
  static GdkPixbuf *icon=NULL;
#else
  static GdkPixmap *pixmap=NULL;
  static GdkBitmap *mask=NULL;
  GtkStyle *style;
#endif
  static GtkTooltips *ttips=NULL;
  char tbuf[1024], *home;
  gchar *fname;

  dprintf(1, "make_window(%lu, %s)", xid, dir? dir: "NULL");
  
  if(!ttips)
    ttips=gtk_tooltips_new();
  
  fwin=g_new0(FreeWindow, 1);

  fwin->options=default_options;
  fwin->df_dir=g_strdup(dir);

  if(!xid) {
    /* Full window mode */

    /* Construct our window and bits */
    fwin->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(fwin->win, "fs free");
    gtk_window_set_title(GTK_WINDOW(fwin->win), dir);
#ifdef GTK2
    g_signal_connect(fwin->win, "destroy", 
		       G_CALLBACK(window_gone), 
		       fwin);
    g_signal_connect(fwin->win, "button_press_event",
		       G_CALLBACK(button_press), fwin);
#else
    gtk_signal_connect(GTK_OBJECT(fwin->win), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       fwin);
    gtk_signal_connect(GTK_OBJECT(fwin->win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), fwin);
#endif
    gtk_widget_realize(fwin->win);
    gtk_widget_add_events(fwin->win, GDK_BUTTON_PRESS_MASK);
    rox_dnd_register_uris(fwin->win, 0, handle_uris, fwin);

    /* Set the icon */
#ifdef GTK2
    if(!icon) {
      fname=rox_resources_find(PROJECT, "freefs.xpm", ROX_RESOURCES_NO_LANG);
      if(fname) {
	GError *err;

	icon=gdk_pixbuf_new_from_file(fname, &err);
	if(err) {
	  dprintf(1, "Failed to load icon %s: %s", fname, err->message);
	}
	g_free(fname);
      }
    }
    if(icon)
      gtk_window_set_icon(GTK_WINDOW(fwin->win), icon);
#else
    if(!pixmap) {
      fname=rox_resources_find(PROJECT, "freefs.xpm", ROX_RESOURCES_NO_LANG);
      if(fname) {      
	style = gtk_widget_get_style(fwin->win);
	pixmap = gdk_pixmap_create_from_xpm(GTK_WIDGET(fwin->win)->window,
					    &mask,
					    &style->bg[GTK_STATE_NORMAL],
					    fname);
	g_free(fname);
      }
    }
    if(pixmap)
      gdk_window_set_icon(GTK_WIDGET(fwin->win)->window, NULL, pixmap, mask);
#endif
    
    gtk_tooltips_set_tip(ttips, fwin->win,
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(fwin->win), vbox);
    gtk_widget_show(vbox);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("Mounted on"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fwin->fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fwin->fs_name), GTK_JUSTIFY_LEFT);
    gtk_widget_set_name(fwin->fs_name, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_name, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_name);
    gtk_tooltips_set_tip(ttips, fwin->fs_name,
			 "This is the name of the directory\n"
			 "the file system is mounted on", TIP_PRIVATE);
  
    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fwin->fs_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_total, FALSE, FALSE, 2);
    gtk_widget_show(fwin->fs_total);
    gtk_tooltips_set_tip(ttips, fwin->fs_total,
			 "This is the total size of the file system",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
    gtk_widget_show(hbox);
    
    fwin->fs_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_used, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_used);
    gtk_tooltips_set_tip(ttips, fwin->fs_used,
			 "This is the space used on the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_used: %s window",
	    GTK_WIDGET_NO_WINDOW(fwin->fs_used)? "no": "has a");
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    fwin->fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fwin->fs_per, "gauge");
#ifdef GTK2
    gtk_widget_set_size_request(fwin->fs_per, 240, 24);
#else
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fwin->fs_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(fwin->fs_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(fwin->fs_per), TRUE);
    gtk_widget_set_usize(fwin->fs_per, 240, 24);
#endif
    gtk_widget_show(fwin->fs_per);
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, fwin->fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_per: %s window",
	    GTK_WIDGET_NO_WINDOW(fwin->fs_per)? "no": "has a");
  
    fwin->fs_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_free, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_free);
    gtk_tooltips_set_tip(ttips, fwin->fs_free,
			 "This is the space available on the file system",
			 TIP_PRIVATE);

    if(!menu)
      menu_create_menu(fwin->win);

    fwin->is_applet=FALSE;
  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;
    int i;

    dprintf(3, "xid=0x%x", xid);
    plug=gtk_plug_new(xid);
#ifdef GTK2
    g_signal_connect(plug, "destroy", 
		       G_CALLBACK(window_gone), 
		       fwin);
    gtk_widget_set_size_request(plug, fwin->options.applet_init_size,
			 fwin->options.applet_init_size);
#else
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       fwin);
    gtk_widget_set_usize(plug, fwin->options.applet_init_size,
			 fwin->options.applet_init_size);
#endif
    dprintf(4, "set_usize %d\n", fwin->options.applet_init_size);
    
#ifdef GTK2
    g_signal_connect(plug, "button_press_event",
		       G_CALLBACK(button_press), fwin);
#else
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), fwin);
#endif
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    gtk_tooltips_set_tip(ttips, plug,
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);

#if APPLET_DND
    dprintf(3, "make drop target for plug");
    rox_dnd_register_uris(plug, 0, handle_uris, NULL);
#endif

    dprintf(4, "vbox new");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    dprintf(4, "alignment new");
    align=gtk_alignment_new(1, 0.5, 1, 0);
    gtk_box_pack_end(GTK_BOX(vbox), align, TRUE, TRUE, 2);
    gtk_widget_show(align);

    dprintf(4, "label new");
    fwin->fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fwin->fs_name), GTK_JUSTIFY_RIGHT);
    gtk_widget_set_name(fwin->fs_name, "text display");
    gtk_container_add(GTK_CONTAINER(align), fwin->fs_name);
    if(default_options.applet_show_dir)
      gtk_widget_show(fwin->fs_name);
    gtk_tooltips_set_tip(ttips, fwin->fs_name,
			 "This shows the dir where the FS is mounted",
			 TIP_PRIVATE);

    fwin->fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fwin->fs_per, "gauge");
    /*gtk_widget_set_usize(fwin->fs_per, -1, 22);*/
#ifdef GTK2
#else
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fwin->fs_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(fwin->fs_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(fwin->fs_per), TRUE);
#endif
    gtk_widget_show(fwin->fs_per);
    gtk_box_pack_end(GTK_BOX(vbox), fwin->fs_per, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, fwin->fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);

    if(!menu)
      menu_create_menu(plug);

    dprintf(5, "show plug");
    gtk_widget_show(plug);
    dprintf(5, "made applet");

    fwin->win=plug;
    fwin->is_applet=TRUE;
  }

  dprintf(3, "update_fs_values(%s)", fwin->df_dir);
  update_fs_values(fwin);

  dprintf(3, "show %p (%ld)", fwin->win, xid);
  if(!xid)
    gtk_widget_show(fwin->win);
  dprintf(2, "timeout %ds, %p, %s", fwin->options.update_sec,
	  (GtkFunction) update_fs_values, fwin->df_dir);
  fwin->update_tag=gtk_timeout_add(fwin->options.update_sec*1000,
			 (GtkFunction) update_fs_values, fwin);
  dprintf(2, "update_sec=%d, update_tag=%u", fwin->options.update_sec,
	  fwin->update_tag);
  
  windows=g_list_append(windows, fwin);
  current_window=fwin;
  gtk_widget_ref(fwin->win);

  return fwin;
}

/* Change the directory/file and therefore FS we are monitoring */
static void set_target(FreeWindow *fwin, const char *ndir)
{
  dprintf(3, "set_target(\"%s\")", ndir);
  
  if(fwin->df_dir)
    g_free(fwin->df_dir);
  fwin->df_dir=g_strdup(ndir);

  gtk_timeout_remove(fwin->update_tag);
  update_fs_values(fwin);
  fwin->update_tag=gtk_timeout_add(fwin->options.update_sec*1000,
			 (GtkFunction) update_fs_values, fwin);
  gtk_window_set_title(GTK_WINDOW(fwin->win), fwin->df_dir);
}

#define MVAL 8ll
static const char *fmt_size(unsigned long long bytes)
{
  static char buf[64];

  if(bytes>MVAL<<30)
    sprintf(buf, "%llu GB", bytes>>30);
  else if(bytes>MVAL<<20)
    sprintf(buf, "%llu MB", bytes>>20);
  else if(bytes>MVAL<<10)
    sprintf(buf, "%llu KB", bytes>>10);
  else
    sprintf(buf, "%llu  B", bytes);

  return buf;
}

static const char *find_mount_point(const char *fname)
{
  static char wnm[PATH_MAX];
  static const char *failed=N_("(unknown)");
  static gchar *prev=NULL;
  static const gchar *prev_ans=NULL;
  
  const char *ans=NULL;
  char name[PATH_MAX];
  char *sl;
  glibtop_mountentry *ents;
  glibtop_mountlist list;
  int i;

#ifdef HAVE_REALPATH
  dprintf(3, "Calling realpath for %s", fname);
  if(!realpath(fname, name))
    return _(failed);
  dprintf(3, "got %s", name);
#else
  strcpy(name, fname);
#endif
  
  if(prev && strcmp(prev, name)==0 && prev_ans) {
    dprintf(3, "return same answer as last time: %s", prev_ans);
    return prev_ans;
  }

  ents=glibtop_get_mountlist(&list, FALSE);
  
  strcpy(wnm, name);
  do {
    for(i=0; i<list.number; i++) {
      dprintf(5, "%s - %s", wnm, ents[i].mountdir);
      if(strcmp(wnm, ents[i].mountdir)==0) {
	ans=wnm;
	break;
      }
    }
    if(ans)
      break;
    sl=strrchr(wnm, '/');
    if(!sl)
      break;
    if(sl==wnm)
      sl[1]=0;
    else
      sl[0]=0;
  } while(!ans);
  
  if(prev)
    g_free(prev);
  prev=g_strdup(name);
  prev_ans=ans;

  free(ents);

  return ans? ans: failed;
}

#define BLOCKSIZE 512

static gboolean update_fs_values(FreeWindow *fwin)
{
  int ok=FALSE;

  dprintf(4, "fwin=%p", fwin);
  dprintf(4, "update_sec=%d, update_tag=%u", fwin->options.update_sec,
	  fwin->update_tag);
  dprintf(3, "update_fs_values(\"%s\")", fwin->df_dir);
  
  if(fwin->df_dir) {
    glibtop_fsusage buf;

    errno=0;
    glibtop_get_fsusage(&buf, fwin->df_dir);
    ok=(errno==0);
    if(!ok)
      perror(fwin->df_dir);
    
    if(ok) {
      unsigned long long total, used, avail;
      int row;
#ifdef GTK2
      gdouble fused;
      char tbuf[32];
#else
      gfloat fused;
#endif
      
      dprintf(5, "%lld %lld %lld", buf.blocks, buf.bfree,
	     buf.bavail);
      
      total=BLOCKSIZE*(unsigned long long) buf.blocks;
      used=BLOCKSIZE*(unsigned long long) (buf.blocks-buf.bfree);
      avail=BLOCKSIZE*(unsigned long long) buf.bavail;
      dprintf(4, "%llu %llu %llu", total, used, avail);

#ifdef GTK2
      if(buf.blocks>0) {
	fused=(100.f*(buf.blocks-buf.bavail))/((gdouble) buf.blocks);
	if(fused>100.)
	  fused=100.;
      } else {
	fused=0.;
      }
#else
      if(buf.blocks>0) {
	fused=(100.f*(buf.blocks-buf.bavail))/((gfloat) buf.blocks);
	if(fused>100.f)
	  fused=100.f;
      } else {
	fused=0.f;
      }
#endif
      dprintf(4, "%2.0f %%", fused);

      if(!fwin->is_applet) {
	dprintf(4, "mount point=%s", find_mount_point(fwin->df_dir));
	gtk_label_set_text(GTK_LABEL(fwin->fs_name),
			   find_mount_point(fwin->df_dir));
	gtk_label_set_text(GTK_LABEL(fwin->fs_total), fmt_size(total));
	gtk_label_set_text(GTK_LABEL(fwin->fs_used), fmt_size(used));
	gtk_label_set_text(GTK_LABEL(fwin->fs_free), fmt_size(avail));

      } else {
	dprintf(5, "set text");
	gtk_label_set_text(GTK_LABEL(fwin->fs_name),
			   g_basename(find_mount_point(fwin->df_dir)));
	/*gtk_label_set_text(GTK_LABEL(fwin->fs_name), find_mount_point(fwin->df_dir));*/
      }
      dprintf(5, "set progress %f", fused);
#ifdef GTK2
      sprintf(tbuf, "%d%%", (int)(fused*100));
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(fwin->fs_per), fused);
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(fwin->fs_per), tbuf);
#else
      gtk_progress_set_value(GTK_PROGRESS(fwin->fs_per), fused);
#endif
    }
  }
  
  dprintf(4, "update_fs_values(%s), ok=%d, update_sec=%d", fwin->df_dir, ok,
	  fwin->options.update_sec);
  
  if(!ok) {
    if(!fwin->is_applet) {
      gtk_label_set_text(GTK_LABEL(fwin->fs_name), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_total), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_used), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_free), "?");
    }

#ifdef GTK2
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(fwin->fs_per), 0.);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(fwin->fs_per), "");
#else
    gtk_progress_set_value(GTK_PROGRESS(fwin->fs_per), 0.);
#endif
  }

  return TRUE;
}

static void do_update(void)
{
  update_fs_values(current_window);
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SHOW_DIR    "AppletShowDir"
#define EXCLUDE_FS  "ExcludeFS"

#if USE_XML
static gboolean read_choices_xml(void)
{
  guchar *fname;

  fname=choices_find_path_load("Config.xml", PROJECT);

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr node, root;
    const xmlChar *string;
    
    doc=xmlParseFile(fname);
    if(!doc) {
      g_free(fname);
      return FALSE;
    }

    root=xmlDocGetRootElement(doc);
    if(!root) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    if(strcmp(root->name, "FreeFS")!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process data here */
      if(strcmp(node->name, UPDATE_RATE)==0) {
 	string=xmlGetProp(node, "value");
	if(!string)
	  continue;
	default_options.update_sec=atoi(string);
	free(string);
	
      } else if(strcmp(node->name, "Applet")==0) {
 	string=xmlGetProp(node, "initial-size");
	if(string) {
	  default_options.applet_init_size=atoi(string);
	  free(string);
	}
 	string=xmlGetProp(node, "show-dir");
	if(string) {
	  default_options.applet_show_dir=atoi(string);
	  free(string);
	}
	
      } else if(strcmp(node->name, EXCLUDE_FS)==0) {
	xmlNodePtr sub;
	
	g_list_foreach(fs_exclude, (GFunc) g_free, NULL);
	g_list_free(fs_exclude);
	fs_exclude=NULL;

	for(sub=node->xmlChildrenNode; sub; sub=sub->next) {
	  if(sub->type!=XML_ELEMENT_NODE)
	    continue;
	  if(strcmp(sub->name, "Name")==0) {
	    string=xmlNodeListGetString(doc, sub->xmlChildrenNode, 1);
	    if(string) {
	      fs_exclude=g_list_append(fs_exclude, g_strdup(string));
	      free(string);
	    }
	  }
	}
     }

    }
    xmlFreeDoc(doc);
    
    g_free(fname);
    return TRUE;
  }

  return FALSE;
}
#endif

static void read_choices(void)
{
  guchar *fname;
  
#if USE_XML
  if(read_choices_xml())
    return;
#endif
  
  fname=choices_find_path_load("Config", "FreeFS");
  if(fname) {
    FILE *in=fopen(fname, "r");
    char buf[256], *line;
    char *end;

    g_free(fname);
    if(!in)
      return;

    do {
      line=fgets(buf, sizeof(buf), in);
      if(line) {
	dprintf(4, "line=%s", line);
	end=strpbrk(line, "\n#");
	if(end)
	  *end=0;
	if(*line) {
	  char *sep;

	  dprintf(4, "line=%s", line);
	  sep=strchr(line, ':');
	  if(sep) {
	    dprintf(3, "%.*s: %s", sep-line, line, sep+1);
	    if(strncmp(line, UPDATE_RATE, sep-line)==0) {
	      default_options.update_sec=atoi(sep+1);
	      if(default_options.update_sec<1)
		default_options.update_sec=1;
	      dprintf(3, "update_sec now %d", default_options.update_sec);
	    } else if(strncmp(line, INIT_SIZE, sep-line)==0) {
	      default_options.applet_init_size=(guint) atoi(sep+1);
	    } else if(strncmp(line, SHOW_DIR, sep-line)==0) {
	      default_options.applet_show_dir=(guint) atoi(sep+1);
	    }
	  }
	}
      }
    } while(line);

    fclose(in);
  } else {
    write_choices(&default_options);
  }
}

#if USE_XML
static void write_choices_xml(const Options *opts)
{
  gchar *fname;
  gboolean ok;

  fname=choices_find_path_save("Config.xml", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr tree;
    FILE *out;
    char buf[80];
    GList *fs;

    doc = xmlNewDoc("1.0");
    doc->children=xmlNewDocNode(doc, NULL, PROJECT, NULL);
    xmlSetProp(doc->children, "version", VERSION);

    /* Insert data here */
    tree=xmlNewChild(doc->children, NULL, UPDATE_RATE, NULL);
    sprintf(buf, "%d", opts->update_sec);
    xmlSetProp(tree, "value", buf);
  
    tree=xmlNewChild(doc->children, NULL, "Applet", NULL);
    sprintf(buf, "%d", opts->applet_init_size);
    xmlSetProp(tree, "initial-size", buf);
    sprintf(buf, "%d", opts->applet_show_dir);
    xmlSetProp(tree, "show-dir", buf);

    if(fs_exclude) {
      tree=xmlNewChild(doc->children, NULL, EXCLUDE_FS, NULL);
      for(fs=fs_exclude; fs; fs=g_list_next(fs))
	(void) xmlNewChild(tree, NULL, "Name", (char *) fs->data);
    }
  
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);

    g_free(fname);
    xmlFreeDoc(doc);
  }
}
#endif

static void write_choices(const Options *opts)
{
#if !USE_XML
  FILE *out;
  gchar *fname;
    
  fname=choices_find_path_save("Config", "FreeFS", TRUE);
  if(!fname)
    return;
  out=fopen(fname, "w");
  g_free(fname);
  if(!out)
    return;
  
  fprintf(out, _("# Config file for FreeFS\n"));
  fprintf(out, "%s: %d\n", UPDATE_RATE, opts->update_sec);
  fprintf(out, "%s: %d\n", INIT_SIZE, opts->applet_init_size);
  fprintf(out, "%s: %d\n", SHOW_DIR, opts->applet_show_dir);
  fclose(out);
#else
  write_choices_xml(opts);
#endif
}

static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    infowin=info_win_new(PROGRAM, PURPOSE, VERSION, AUTHOR, WEBSITE);
  }

  gtk_widget_show(infowin);
}

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

static void hide_window(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(GTK_WIDGET(data));
}

static void cancel_config(GtkWidget *widget, gpointer data)
{
  GtkWidget *confwin=GTK_WIDGET(data);
  
  gtk_widget_hide(confwin);
}

static void set_config(GtkWidget *widget, gpointer data)
{
  OptionWidgets *ow=(OptionWidgets *) data;
  Options *opts;
  gfloat s;

  opts=ow->fwin? &ow->fwin->options: &default_options;

  opts->update_sec=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->update_s));
  opts->applet_init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));
  opts->applet_show_dir=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_dir));

  if(ow->fwin) {
    gtk_timeout_remove(ow->fwin->update_tag);
    ow->fwin->update_tag=gtk_timeout_add(opts->update_sec*1000,
					 (GtkFunction) update_fs_values,
					 ow->fwin);
    if(ow->fwin->is_applet) {
      if(opts->applet_show_dir)
	gtk_widget_show(ow->fwin->fs_name);
      else
	gtk_widget_hide(ow->fwin->fs_name);
    }
  }
  
  gtk_widget_hide(ow->window);
}

static void save_config(GtkWidget *widget, gpointer data)
{
  set_config(widget, data);
  write_choices(&current_window->options);
}

#ifdef GTK2
#define RESPONSE_SAVE 1

static void config_response(GtkWidget *dialog, gint response, gpointer data)
{
  switch(response) {
  case RESPONSE_SAVE:
    save_config(dialog, data);
    break;
  case GTK_RESPONSE_ACCEPT:
    set_config(dialog, data);
    break;
  case GTK_RESPONSE_REJECT:
    cancel_config(dialog, data);
    break;
  case GTK_RESPONSE_NONE:
    gtk_widget_hide(dialog);
    break;
  default:
    dprintf(3, "config_response(%p, %d, %p) %d?", dialog, response, data,
	    response);
    break;
  }
}
#endif

static void show_config_win(void)
{
  static GtkWidget *confwin=NULL;
  static OptionWidgets ow;
  Options *opts;

  opts=current_window? &current_window->options: &default_options;
  ow.fwin=current_window;

  if(!confwin) {
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    GtkWidget *label;
    GtkObject *range;
    GtkWidget *spin;
    GtkWidget *check;
    GtkWidget *frame;

#ifdef GTK2
    confwin=gtk_dialog_new_with_buttons("Configuration",
					ow.fwin? ow.fwin->win: NULL,
					0,
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_SAVE, RESPONSE_SAVE,
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					NULL);
    g_signal_connect(confwin, "response", 
		     G_CALLBACK(dialog_response), 
		     &ow);
    g_signal_connect(confwin, "delete_event", 
		     G_CALLBACK(trap_frame_destroy), 
		     confwin);
#else
    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), "Configuration");
    gtk_window_set_position(GTK_WINDOW(confwin), GTK_WIN_POS_MOUSE);
#endif
    ow.window=confwin;

    vbox=GTK_DIALOG(confwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

    label=gtk_label_new(_("Update rate (s)"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    range=gtk_adjustment_new((gfloat) opts->update_sec,
			     1, 60., 1., 10., 10.);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 0);
    gtk_widget_set_name(spin, "update_s");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 4);
    ow.update_s=spin;

    frame=gtk_frame_new(_("Applet configuration"));
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, FALSE, 6);

    vbox=gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

    label=gtk_label_new(_("Initial size"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    range=gtk_adjustment_new((gfloat) opts->applet_init_size,
			     16, 128, 2, 16, 16);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 0);
    gtk_widget_set_name(spin, "init_size");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 4);
    ow.init_size=spin;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

    label=gtk_label_new(_("Display"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    check=gtk_check_button_new_with_label(_("Show directory"));
    gtk_widget_set_name(check, "show_dir");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 4);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 opts->applet_show_dir);
    ow.show_dir=check;

#ifndef GTK2
    hbox=GTK_DIALOG(confwin)->action_area;

    but=gtk_button_new_with_label(_("Save"));
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(save_config), &ow);

    but=gtk_button_new_with_label(_("Set"));
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       &ow);

    but=gtk_button_new_with_label(_("Cancel"));
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(cancel_config), confwin);
#endif
  } else {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.update_s),
			      (gfloat)(opts->update_sec));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.init_size),
			      (gfloat)(opts->applet_init_size));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_dir),
				 opts->applet_show_dir);
  }

  gtk_widget_show(confwin);  
}

static void do_opendir(gpointer dat, guint action, GtkWidget *wid)
{
  const char *dir=current_window->df_dir;

  if(action)
    dir=find_mount_point(dir);

  rox_filer_open_dir(dir);
}

static void close_window(void)
{
  FreeWindow *fwin=current_window;

  dprintf(1, "close_window %p %p", fwin, fwin->win);

  gtk_widget_hide(fwin->win);
  gtk_widget_unref(fwin->win);
  gtk_widget_destroy(fwin->win);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),	NULL, show_config_win, 0, NULL},
  { N_("/Update Now"),	NULL, do_update, 0, NULL },
  { N_("/Open"),                NULL, NULL, 0, "<Branch>"},
  { N_("/Open/Directory"),	NULL, do_opendir, 0, NULL },
  { N_("/Open/FS root"),	NULL, do_opendir, 1, NULL },
  { N_("/Scan"),                NULL, NULL, 0, "<Branch>"},
  { N_("/Scan/By name"),	NULL, NULL, 0, NULL },
  { N_("/Scan/By mount point"),	NULL, NULL, 1, NULL },
  { N_("/Close"), 	        NULL, close_window, 0, NULL },
  { N_("/sep"), 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Quit"), 	NULL, gtk_main_quit, 0, NULL },
};

static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save("menus", PROJECT, TRUE);
  if (menurc) {
    gboolean	mod = FALSE;

#ifdef GTK2
    gtk_accel_map_save(menurc);
#else
    gtk_item_factory_dump_rc(menurc, NULL, TRUE);    
#endif
    g_free(menurc);
  }
}

static GtkWidget *scan_by_name_menu=NULL;
static GtkWidget *scan_by_mount_menu=NULL;

int compare_mountentry(const void *a, const void *b)
{
  const glibtop_mountentry *ma=(const glibtop_mountentry *) a;
  const glibtop_mountentry *mb=(const glibtop_mountentry *) b;

  return strcmp(ma->mountdir, mb->mountdir);
}

static void menu_set_target(gchar *mount)
{
  dprintf(3, "menu_set_target(%s)", mount? mount: "NULL");

  set_target(current_window, mount);
}

static void menu_item_destroyed(GtkWidget *item, gpointer data)
{
  dprintf(3, "menu_item_destroyed(%p, %p)", item, data);
  g_free(data);
}

static void update_menus(void)
{
  GtkWidget *item;
  gchar *name;
  gchar *mount;
  GtkWidget *name_menu;
  GtkWidget *mount_menu;
  int i;
  
  glibtop_mountlist mount_info;
  glibtop_mountentry *mounts;

  name_menu=gtk_menu_new();
  mount_menu=gtk_menu_new();

  dprintf(3, "name_menu=%p, mount_menu=%p", name_menu, mount_menu);
  mounts=glibtop_get_mountlist(&mount_info, 0);
  if(mounts) {
    qsort(mounts, mount_info.number, sizeof(*mounts), compare_mountentry);
    for(i=0; i<mount_info.number; i++) {
      if(g_list_find_custom(fs_exclude, mounts[i].type, (GCompareFunc) strcmp))
	continue;
      
      mount=g_strdup(mounts[i].mountdir);
      name=mounts[i].devname;
      dprintf(2, "name=%s, mount=%s type=%s", name, mount, mounts[i].type);

      item=gtk_menu_item_new_with_label(name);
      gtk_signal_connect_object(GTK_OBJECT(item), "activate",
				GTK_SIGNAL_FUNC(menu_set_target), mount);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(menu_item_destroyed), mount);
      gtk_menu_shell_append(GTK_MENU_SHELL(name_menu), item);

      item=gtk_menu_item_new_with_label(mount);
      gtk_signal_connect_object(GTK_OBJECT(item), "activate",
				GTK_SIGNAL_FUNC(menu_set_target), mount);
      gtk_menu_shell_append(GTK_MENU_SHELL(mount_menu), item);
    }

    free(mounts);
  }

  gtk_widget_show_all(name_menu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(scan_by_name_menu), name_menu);
  gtk_widget_show_all(mount_menu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(scan_by_mount_menu), mount_menu);
}

static void menu_create_menu(GtkWidget *window)
{
  GtkItemFactory	*item_factory;
  GtkAccelGroup	*accel_group;
  gint 		n_items = sizeof(menu_items) / sizeof(*menu_items);
  gchar *menurc;

  accel_group = gtk_accel_group_new();
  /*gtk_accel_group_lock(accel_group);*/

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<system>", 
				      accel_group);

  gtk_item_factory_create_items(item_factory, n_items, menu_items, NULL);

	/* Attach the new accelerator group to the window. */
  gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
#ifdef GTK2
    gtk_accel_map_load(menurc);
#else
    gtk_item_factory_parse_rc(menurc);
#endif
    g_free(menurc);
  }

  atexit(save_menus);

  menu = gtk_item_factory_get_widget(item_factory, "<system>");

  /*name=g_strdup_printf("%s")*/
  scan_by_name_menu=gtk_item_factory_get_widget(item_factory,
						"/Scan/By name");
  dprintf(3, "scan_by_name_menu=%p", scan_by_name_menu);
  scan_by_mount_menu=gtk_item_factory_get_widget(item_factory,
						"/Scan/By mount point");
  dprintf(3, "scan_by_mount_menu=%p", scan_by_mount_menu);
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer udata)
{
  FreeWindow *fwin=(FreeWindow *) udata;
  
  current_window=fwin;
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==3) {
      if(!menu)
	menu_create_menu(window);
      update_menus();
      
      if(fwin->is_applet) {
	applet_popup_menu(fwin->win, menu, bev);
      } else {
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
      }
      return TRUE;
    } else if(bev->button==1 && fwin->is_applet) {
      FreeWindow *nwin=make_window(0, fwin->df_dir);

      gtk_widget_show(nwin->win);

      return TRUE;
    }
  }

  return FALSE;
}

#if USE_SERVER
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  xmlNodePtr path, parent;
  gchar *str;
  guint32 xid=0;
  gchar *dir;

  dprintf(3, "rpc_Open(%p, \"%s\", %p, %p)", server, action_name, args, udata);

  path=args->data;
  str=xmlNodeGetContent(path);
  dir=g_strdup(str);
  g_free(str);
  
  parent=g_list_next(args)->data;
  if(parent) {

    str=xmlNodeGetContent(parent);
    if(str) {
      xid=(guint32) atol(str);
      g_free(str);
    }
  }

  if(dir && dir[0])
    make_window(xid, dir);
  else {
    rox_error("Invalid remote call, Path not given");
  }
  g_free(dir);

  return NULL;
}

static void open_callback(ROXSOAP *serv, gboolean status, 
				  xmlDocPtr reply, gpointer udata)
{
  gboolean *s=udata;
  
  dprintf(3, "In open_callback(%p, %d, %p, %p)", serv, status, reply,
	 udata);
  *s=status;
  gtk_main_quit();
}

static gboolean open_remote(guint32 xid, const char *path)
{
  ROXSOAP *serv;
  xmlDocPtr doc;
  xmlNodePtr node;
  gboolean sent, ok;
  char buf[32];

  serv=rox_soap_connect(PROJECT);
  dprintf(3, "server for %s is %p", PROJECT, serv);
  if(!serv)
    return FALSE;

  doc=rox_soap_build_xml("Open", FREEFS_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  xmlNewChild(node, NULL, "Path", path);
  if(xid) {
    sprintf(buf, "%lu", xid);
    xmlNewChild(node, NULL, "Parent", buf);
  }

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  dprintf(3, "sent %d", sent);
  /*xmlDocDump(stdout, doc);*/

  xmlFreeDoc(doc);
  if(sent)
    gtk_main();
  rox_soap_close(serv);

  return sent && ok;
}

#endif

static gboolean handle_uris(GtkWidget *widget, GSList *uris,
			    gpointer data, gpointer udata)
{
  FreeWindow *fwin=(FreeWindow *) udata;
  GSList *local;

  local=rox_dnd_filter_local(uris);

  dprintf(2, "URI dropped, local list is %p (%s)", local,
	  local? local->data: "NULL");

  if(local) {
    GSList *tmp;
    
    set_target(fwin, (const char *) local->data);

    rox_dnd_local_free(local);
  }

  return FALSE;
}

/*
 * $Log: freefs.c,v $
 * Revision 1.19  2002/04/29 08:19:39  stephen
 * Added menu of mounted file systems so you can choose which to monitor,
 * otherwise there was no way of changing the FS in the applet version.
 * Fixed memory leak which manifested if you monitored more than one FS.
 * Use replacement applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.18  2002/04/12 10:21:42  stephen
 * Multiple windows, SOAP server
 *
 * Revision 1.17  2002/03/04 11:41:08  stephen
 * Now stable release 1.2.0
 *
 * Revision 1.16  2002/01/30 10:20:22  stephen
 * Add -h and -v options.
 *
 * Revision 1.15  2002/01/10 15:16:20  stephen
 * New applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.14  2001/12/12 10:18:22  stephen
 * Added option to open a viewer for the root of the FS being monitored
 *
 * Revision 1.13  2001/11/12 14:42:10  stephen
 * Change to XML handling: requires 2.4 or later.  Use old style config otherwise.
 *
 * Revision 1.12  2001/11/08 15:12:23  stephen
 * Fix leak in read/write XML config.
 *
 * Revision 1.11  2001/11/06 16:31:53  stephen
 * Pick up window icon from rox_resources_find. Config file now in XML.
 *
 * Revision 1.10  2001/10/23 10:50:06  stephen
 * Another compilation fix: run autoconf if configure script missing.
 * Can now choose what is shown in applet mode.
 *
 * Revision 1.9  2001/08/29 13:46:14  stephen
 * Fixed up the debug output.
 *
 * Revision 1.8  2001/08/20 09:32:15  stephen
 * Switch to using ROX-CLib.  Menu created at start so accelerators are
 * always available.
 *
 * Revision 1.7  2001/07/06 08:30:49  stephen
 * Improved applet appearance for top and bottom panels.  Support menu
 * accelerators.  Add tooltip help.
 *
 * Revision 1.6  2001/05/30 10:32:55  stephen
 * Initial support for i18n
 *
 * Revision 1.5  2001/05/30 09:04:53  stephen
 * include unistd.h, plus some bugfixes and prep for i18n.
 *
 * Revision 1.4  2001/05/25 07:53:44  stephen
 * Expanded configuration, which meant moving it off the menu and into a
 * window.  Applet supports a menu.  Initial size of applet now
 * configurable.
 *
 * Revision 1.3  2001/05/09 10:22:49  stephen
 * Version number now stored in AppInfo.xml
 *
 * Revision 1.2  2001/04/26 13:22:37  stephen
 * Added an applet front end.
 *
 */
