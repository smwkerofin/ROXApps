/*
 * Mem - Monitor memory and swap usage
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies, see ../Help/COPYING.
 *
 * $Id: mem.c,v 1.13 2003/04/16 09:12:38 stephen Exp $
 */
#include "config.h"

/* Select compilation options */
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
#include <sys/types.h>
#include <sys/wait.h>

#include <gtk/gtk.h>

#include <glibtop.h>
#include <glibtop/mem.h>
#include <glibtop/swap.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#define USE_XML 1

#if TRY_SERVER && ROX_CLIB_VERSION>=201
#define USE_SERVER 1
#else
#define USE_SERVER 0
#endif

#include <rox.h>
#include "applet.h"
#include "options.h"
#if USE_SERVER
#include "rox_soap.h"
#include "rox_soap_server.h"
#endif

#define TIP_PRIVATE "For more information see the help file"

/* Can we support swap?  On Solaris, versions of libgtop up to 1.1.2 don't
   read swap figures correctly */
#define SOLARIS_SWAP_BROKEN_UP_TO 1001002

#if defined(__sparc__) && defined(__svr4__)
#if LIBGTOP_VERSION_CODE>SOLARIS_SWAP_BROKEN_UP_TO
#define SWAP_SUPPORTED_LIBGTOP 1 
#else
#define SWAP_SUPPORTED_LIBGTOP 0
#endif
#else
#define SWAP_SUPPORTED_LIBGTOP 1
#endif

typedef enum applet_display {
  AD_TOTAL, AD_USED, AD_FREE, AD_PER
} AppletDisplay;
#define NUM_DISPLAY 4

/* GTK+ objects */
static GtkWidget *menu=NULL;
typedef struct options {
  guint update_sec;          /* How often to update */
  guint applet_init_size;    /* Initial size of applet */
  guint show_host;
  guint gauge_width;
  AppletDisplay mem_disp;
  AppletDisplay swap_disp;
} Options;

static Options default_options={
  5,
  36,
  FALSE,
  120,
  AD_PER,
  AD_PER
};

static Option o_update_rate;
static Option o_applet_size;
static Option o_show_host;
static Option o_gauge_width;
static Option o_mem_disp;
static Option o_swap_disp;

typedef struct mem_window {
  gboolean is_applet;
  
  GtkWidget *win;
  GtkWidget *host;
  GtkWidget *mem_total, *mem_used, *mem_free, *mem_per;
  GtkWidget *swap_total, *swap_used, *swap_free, *swap_per;
  
  guint update_tag;
  
  /*Options options;*/
} MemWindow;

#if !SWAP_SUPPORTED_LIBGTOP
struct _swap_data {
  uint64_t alloc, reserved, used, avail;
  gboolean valid;
};

static struct _swap_data swap_data={
  0ll, 0ll, 0ll, 0LL,
  FALSE
};
static guint swap_update_tag=0;
static pid_t swap_process=0;
static gint swap_read_tag;
#endif

static GList *windows=NULL;

static MemWindow *current_window=NULL;

#if USE_SERVER
static ROXSOAPServer *server=NULL;
#define MEM_NAMESPACE_URL WEBSITE PROJECT
#endif

/* Call backs & stuff */
static MemWindow *make_window(guint32 socket);
static gboolean update_values(MemWindow *);
static void do_update(void);
static void read_choices(void);
static void setup_options(void);
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);
static void opts_changed(void);

#if !SWAP_SUPPORTED_LIBGTOP
static gboolean update_swap(gpointer);
#endif
#if USE_SERVER
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid);
#endif

static void usage(const char *argv0)
{
#if USE_SERVER
  printf("Usage: %s [X-options] [gtk-options] [-vhnr] [-a XID]\n", argv0);
#else
  printf("Usage: %s [X-options] [gtk-options] [-vh] [-a XID]\n", argv0);
#endif
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
#if USE_SERVER
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
#endif
  printf("  -v\tdisplay version information\n");
  printf("  -a XID\tX window to create applet in\n");
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
  printf("ROX-CLib version %s (built with %d.%d.%d)\n",
	 rox_clib_version_string(),
	 ROX_CLIB_VERSION/10000, (ROX_CLIB_VERSION%10000)/100,
	 ROX_CLIB_VERSION%100);

  printf("\nCompile time options:\n");
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
  printf("  Using XML... ");
  if(USE_XML)
    printf("yes (libxml version %d)\n", LIBXML_VERSION);
  else {
    printf("no (libxml version %d)\n", LIBXML_VERSION);
  }
  printf("  Use libgtop for swap... %s\n",
	 SWAP_SUPPORTED_LIBGTOP? "yes": "no");
  if(!SWAP_SUPPORTED_LIBGTOP) {
#ifdef LIBGTOP_VERSION_CODE
    printf("    libgtop version %d\n", LIBGTOP_VERSION_CODE);
#else
    printf("    libgtop version 1.0.x\n");
#endif
    printf("    Broken on Solaris up to version %d\n",
	   SOLARIS_SWAP_BROKEN_UP_TO);
  }
  printf("  Using SOAP server... ");
  if(USE_SERVER)
    printf("yes\n");
  else {
    printf("no");
    if(!TRY_SERVER)
      printf(", disabled");
    if(ROX_CLIB_VERSION<201)
      printf(", ROX-CLib %d.%d.%d does not support it",
	     ROX_CLIB_VERSION/10000, (ROX_CLIB_VERSION%10000)/100,
	     ROX_CLIB_VERSION%100);
    printf("\n");
  }
}

int main(int argc, char *argv[])
{
  char tbuf[1024], *home;
  guchar *fname;
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

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif

  rox_debug_init(PROJECT);
  
  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  dprintf(5, "%d %s -> %s", argc, argv[1], argv[argc-1]);
  gtk_init(&argc, &argv);
  dprintf(5, "%d %s -> %s", argc, argv[1], argv[argc-1]);

  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, options))!=EOF)
    switch(c) {
    case 'h':
      usage(argv[0]);
      do_exit=TRUE;
      break;
    case 'v':
      do_version();
      do_exit=TRUE;
      break;
    case 'a':
      xid=atol(optarg);
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

  setup_options();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=choices_find_path_load("gtkrc", PROJECT);
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

#if !SWAP_SUPPORTED_LIBGTOP
  swap_update_tag=gtk_timeout_add(10*1000,
			 (GtkFunction) update_swap, NULL);
  (void) update_swap(NULL);
#endif
  
  dprintf(4, "set up glibtop");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_MEM)
#if SWAP_SUPPORTED_LIBGTOP
		 |(1<<GLIBTOP_SYSDEPS_SWAP)
#endif
		 , 0);

#if USE_SERVER
  if(replace_server || !rox_soap_ping(PROJECT)) {
    dprintf(1, "Making SOAP server");
    server=rox_soap_server_new(PROJECT, MEM_NAMESPACE_URL);
    rox_soap_server_add_action(server, "Open", NULL, "Parent",
			       rpc_Open, NULL);
  } else if(!new_run) {
    if(open_remote(xid)) {
      dprintf(1, "success in open_remote(%lu), exiting", xid);
      return 0;
    }
  }
#endif
  make_window(xid);
  
  gtk_main();

#if USE_SERVER
  if(server)
    rox_soap_server_delete(server);
#endif

  return 0;
}

static void setup_options(void)
{
  choices_init();
  options_init(PROJECT);

  read_choices();  /* Read old format config */

  option_add_int(&o_update_rate, "update_rate",
		 1000*default_options.update_sec);
  option_add_int(&o_applet_size, "applet_size",
		  default_options.applet_init_size);
  option_add_int(&o_show_host, "show_host", default_options.show_host);
  option_add_int(&o_gauge_width, "gauge_width", default_options.gauge_width);
  option_add_int(&o_mem_disp, "mem_disp", default_options.mem_disp);
  option_add_int(&o_swap_disp, "swap_disp", default_options.swap_disp);

  option_add_notify(opts_changed);
}

static void remove_window(MemWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);
  /*gtk_widget_hide(win->win);*/

  gtk_timeout_remove(win->update_tag);
  g_free(win);

  dprintf(1, "windows=%p, number of active windows=%d", windows,
	  g_list_length(windows));
  if(g_list_length(windows)<1)
    gtk_main_quit();
}

static void window_gone(GtkWidget *widget, gpointer data)
{
  MemWindow *mw=(MemWindow *) data;

  dprintf(1, "Window gone: %p %p", widget, mw);

  remove_window(mw);
}

static MemWindow *make_window(guint32 xid)
{
  MemWindow *mwin;
  GtkWidget *vbox, *hbox, *vbox2, *frame;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  /*GtkWidget *pixmapwid;*/
  static GdkPixbuf *pixbuf=NULL;
  /*GtkStyle *style;*/
  static GtkTooltips *ttips=NULL;
  char hname[1024];

  dprintf(1, "make_window(%lu)", xid);
  
  if(!ttips)
    ttips=gtk_tooltips_new();
  
  mwin=g_new0(MemWindow, 1);

  if(!xid) {
    /* Full window mode */
    /* Construct our window and bits */
    mwin->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(mwin->win, "mem free");
    gtk_window_set_title(GTK_WINDOW(mwin->win), "System memory");
    gtk_signal_connect(GTK_OBJECT(mwin->win), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       mwin);
    gtk_signal_connect(GTK_OBJECT(mwin->win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), mwin);
    gtk_widget_realize(mwin->win);
    gtk_widget_add_events(mwin->win, GDK_BUTTON_PRESS_MASK);

    /* Set the icon */
    if(!pixbuf) {
      gchar *fname;
      fname=rox_resources_find(PROJECT, "AppIcon.png", ROX_RESOURCES_NO_LANG);
      if(!fname)
	fname=rox_resources_find(PROJECT, "AppIcon.xpm",
				 ROX_RESOURCES_NO_LANG);
      if(fname) {
	GError *error=NULL;
	pixbuf=gdk_pixbuf_new_from_file(fname, &error);
	g_free(fname);
      }
    }
    if(pixbuf) {
      gtk_window_set_icon(GTK_WINDOW(mwin->win), pixbuf);
    }

    gtk_tooltips_set_tip(ttips, mwin->win,
			 "Mem shows the memory and swap usage on a host",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(mwin->win), vbox);
    gtk_widget_show(vbox);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("<b>Host:</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    gethostname(hname, sizeof(hname));
    label=gtk_label_new(hname);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_set_name(label, "text display");
    gtk_widget_show(label);
    mwin->host=hbox;
    if(o_show_host.int_value)
      gtk_widget_show(mwin->host);

    frame=gtk_frame_new(_("Memory"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 2);
    gtk_widget_show(frame);
  
    vbox2=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    gtk_widget_show(vbox2);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("<b>Total:</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    mwin->mem_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->mem_total, FALSE, FALSE, 2);
    gtk_widget_show(mwin->mem_total);
    gtk_tooltips_set_tip(ttips, mwin->mem_total,
			 "This is the total size of memory",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    mwin->mem_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->mem_used, TRUE, FALSE, 2);
    gtk_widget_show(mwin->mem_used);
    gtk_tooltips_set_tip(ttips, mwin->mem_used,
			 "This is the memory used on the host",
			 TIP_PRIVATE);
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    mwin->mem_per=gtk_progress_bar_new();
    gtk_widget_set_name(mwin->mem_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mwin->mem_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mwin->mem_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mwin->mem_per), TRUE);
    gtk_widget_set_usize(mwin->mem_per, o_gauge_width.int_value, -1);
    gtk_widget_show(mwin->mem_per);
    gtk_box_pack_start(GTK_BOX(hbox), mwin->mem_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mwin->mem_per,
			 "This shows the relative usage of memory",
			 TIP_PRIVATE);
  
    mwin->mem_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->mem_free, TRUE, FALSE, 2);
    gtk_widget_show(mwin->mem_free);
    gtk_tooltips_set_tip(ttips, mwin->mem_free,
			 "This is the memory available on the host",
			 TIP_PRIVATE);

    frame=gtk_frame_new(_("Swap"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 2);
    gtk_widget_show(frame);
  
    vbox2=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    gtk_widget_show(vbox2);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("<b>Total:</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    mwin->swap_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->swap_total, FALSE, FALSE, 2);
    gtk_widget_show(mwin->swap_total);
    gtk_tooltips_set_tip(ttips, mwin->swap_total,
			 "This is the total size of swap space",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    mwin->swap_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->swap_used, TRUE, FALSE, 2);
    gtk_widget_show(mwin->swap_used);
    gtk_tooltips_set_tip(ttips, mwin->swap_used,
			 "This is the swap space used on the host",
			 TIP_PRIVATE);
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    mwin->swap_per=gtk_progress_bar_new();
    gtk_widget_set_name(mwin->swap_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mwin->swap_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mwin->swap_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mwin->swap_per), TRUE);
    gtk_widget_set_usize(mwin->swap_per, o_gauge_width.int_value, -1);
    gtk_widget_show(mwin->swap_per);
    gtk_box_pack_start(GTK_BOX(hbox), mwin->swap_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mwin->swap_per,
			 "This shows the relative usage of swap space",
			 TIP_PRIVATE);
  
    mwin->swap_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mwin->swap_free, TRUE, FALSE, 2);
    gtk_widget_show(mwin->swap_free);
    gtk_tooltips_set_tip(ttips, mwin->swap_free,
			 "This is the swap space available on the host",
			 TIP_PRIVATE);

    mwin->is_applet=FALSE;
    
  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;
    int i;
    
    plug=gtk_plug_new(xid);
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       mwin);
    gtk_widget_set_size_request(plug, o_applet_size.int_value,
			 o_applet_size.int_value);
    dprintf(4, "set_usize %d\n", o_applet_size.int_value);
    
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), mwin);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    gtk_tooltips_set_tip(ttips, plug,
			 "Mem shows the memory and swap usage",
			 TIP_PRIVATE);

    dprintf(4, "vbox new");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    mwin->mem_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_total, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->mem_total, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->mem_total,
			 "This is the total size of memory",
			 TIP_PRIVATE);
  
    mwin->mem_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_used, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->mem_used, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->mem_used,
			 "This is the memory used on the host",
			 TIP_PRIVATE);
  
    mwin->mem_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->mem_free, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->mem_free, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->mem_free,
			 "This is the memory available on the host",
			 TIP_PRIVATE);

    mwin->mem_per=gtk_progress_bar_new();
    gtk_widget_set_name(mwin->mem_per, "gauge");
    /*gtk_widget_set_usize(mem_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mwin->mem_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mwin->mem_per), "M %p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mwin->mem_per), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), mwin->mem_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mwin->mem_per,
			 "This shows the relative usage of memory",
			 TIP_PRIVATE);

    switch(o_mem_disp.int_value) {
    case AD_TOTAL:
      gtk_widget_show(mwin->mem_total);
      break;
    case AD_USED:
      gtk_widget_show(mwin->mem_used);
      break;
    case AD_FREE:
      gtk_widget_show(mwin->mem_free);
      break;
    case AD_PER:
      gtk_widget_show(mwin->mem_per);
      break;
    }

    mwin->swap_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_total, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->swap_total, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->swap_total,
			 "This is the total size of swap space",
			 TIP_PRIVATE);
  
    mwin->swap_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_used, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->swap_used, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->swap_used,
			 "This is the swap space used on the host",
			 TIP_PRIVATE);
  
    mwin->swap_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mwin->swap_free, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mwin->swap_free, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mwin->swap_free,
			 "This is the swap space available on the host",
			 TIP_PRIVATE);

    mwin->swap_per=gtk_progress_bar_new();
    gtk_widget_set_name(mwin->swap_per, "gauge");
    /*gtk_widget_set_usize(mwin->swap_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mwin->swap_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mwin->swap_per), "S %p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mwin->swap_per), TRUE);
    gtk_widget_show(mwin->swap_per);
    gtk_box_pack_start(GTK_BOX(vbox), mwin->swap_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mwin->swap_per,
			 "This shows the relative usage of swap space",
			 TIP_PRIVATE);
    
    switch(o_swap_disp.int_value) {
    case AD_TOTAL:
      gtk_widget_show(mwin->swap_total);
      break;
    case AD_USED:
      gtk_widget_show(mwin->swap_used);
      break;
    case AD_FREE:
      gtk_widget_show(mwin->swap_free);
      break;
    case AD_PER:
      gtk_widget_show(mwin->swap_per);
      break;
    }

    dprintf(5, "show plug");
    gtk_widget_show(plug);
    dprintf(5, "made applet");

    mwin->win=plug;
    mwin->is_applet=TRUE;
  }
  if(!menu)
    menu_create_menu(mwin->win);

  /* check now */
  update_values(mwin);

  dprintf(3, "show %p (%ld)", mwin->win, xid);
  if(!xid)
    gtk_widget_show(mwin->win);
  dprintf(2, "timeout %ds, %p, %p", o_update_rate.int_value,
	  (GtkFunction) update_values, mwin);
  mwin->update_tag=gtk_timeout_add(o_update_rate.int_value,
			 (GtkFunction) update_values, mwin);
  dprintf(2, "update_sec=%d, update_tag=%u", o_update_rate.int_value,
	  mwin->update_tag);

  windows=g_list_append(windows, mwin);
  current_window=mwin;
  gtk_widget_ref(mwin->win);

  return mwin;
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

#define MEM_FLAGS ((1<<GLIBTOP_MEM_TOTAL)|(1<<GLIBTOP_MEM_USED)|(1<<GLIBTOP_MEM_FREE))

#if SWAP_SUPPORTED_LIBGTOP
#define SWAP_FLAGS ((1<<GLIBTOP_SWAP_TOTAL)|(1<<GLIBTOP_SWAP_USED)|(1<<GLIBTOP_SWAP_FREE))
#endif

static gboolean update_values(MemWindow *mwin)
{
  int ok=FALSE;
  glibtop_mem mem;
#if SWAP_SUPPORTED_LIBGTOP
  glibtop_swap swap;
#endif
  unsigned long long total, used, avail;
  
  dprintf(4, "update_sec=%d, update_tag=%u", o_update_rate.int_value,
	  mwin->update_tag);
  
  errno=0;
  glibtop_get_mem(&mem);
  ok=(errno==0) && (mem.flags & MEM_FLAGS)==MEM_FLAGS;
    
  if(ok) {
    gfloat fused;
      
    total=mem.total;
    used=mem.total-mem.free;
    avail=mem.free;
      
    fused=(100.f*used)/((gfloat) total);
    if(fused>100.f)
      fused=100.f;

    dprintf(4, "%2.0f %%", fused);

    if(!mwin->is_applet) {
      gtk_label_set_text(GTK_LABEL(mwin->mem_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mwin->mem_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mwin->mem_free), fmt_size(avail));
    } else {
      char tmp[64];

      strcpy(tmp, "Mt ");
      strcat(tmp, fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mwin->mem_total), tmp);

      strcpy(tmp, "Mu ");
      strcat(tmp, fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mwin->mem_used), tmp);

      strcpy(tmp, "Mf ");
      strcat(tmp, fmt_size(avail));
      gtk_label_set_text(GTK_LABEL(mwin->mem_free), tmp);

    }
      
    dprintf(5, "set progress");
    gtk_progress_set_value(GTK_PROGRESS(mwin->mem_per), fused);
    gtk_widget_set_sensitive(GTK_WIDGET(mwin->mem_per), TRUE);

  } else {
    gtk_label_set_text(GTK_LABEL(mwin->mem_total), "Mem?");
    gtk_label_set_text(GTK_LABEL(mwin->mem_used), "Mem?");
    gtk_label_set_text(GTK_LABEL(mwin->mem_free), "Mem?");
    gtk_widget_set_sensitive(GTK_WIDGET(mwin->mem_per), FALSE);
  }

#if SWAP_SUPPORTED_LIBGTOP
  errno=0;
  glibtop_get_swap(&swap);
  ok=(errno==0) && (swap.flags & SWAP_FLAGS)==SWAP_FLAGS;
  total=swap.total;
  used=swap.used;
  avail=swap.free;
  dprintf(3, "%llx: %lld %lld %lld, %lld %lld", swap.flags, swap.total,
	  swap.used, swap.free, swap.pagein, swap.pageout);
#else
  ok=swap_data.valid;
  total=swap_data.used+swap_data.avail;
  used=swap_data.used;
  avail=swap_data.avail;
#endif
  
  if(ok) {
    gfloat fused;
      
    dprintf(2, "swap: %lld %lld %lld", total, used, avail);
    dprintf(2, "swap: %lldK %lldK %lldK", total>>10, used>>10, avail>>10);
    dprintf(2, "swap: %lldM %lldM %lldM", total>>20, used>>20, avail>>20);
      
    fused=(100.f*used)/((gfloat) total);
    if(fused>100.f)
      fused=100.f;

    dprintf(4, "%2.0f %%\n", fused);

    if(!mwin->is_applet) {
      gtk_label_set_text(GTK_LABEL(mwin->swap_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mwin->swap_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mwin->swap_free), fmt_size(avail));
    } else {
      char tmp[64];

      strcpy(tmp, "St ");
      strcat(tmp, fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mwin->swap_total), tmp);

      strcpy(tmp, "Su ");
      strcat(tmp, fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mwin->swap_used), tmp);

      strcpy(tmp, "Sf ");
      strcat(tmp, fmt_size(avail));
      gtk_label_set_text(GTK_LABEL(mwin->swap_free), tmp);

    }
      
    dprintf(5, "set progress");
    gtk_progress_set_value(GTK_PROGRESS(mwin->swap_per), fused);
    gtk_widget_set_sensitive(GTK_WIDGET(mwin->swap_per), TRUE);
    
  } else {
    gtk_label_set_text(GTK_LABEL(mwin->swap_total), "Swap?");
    gtk_label_set_text(GTK_LABEL(mwin->swap_used), "Swap?");
    gtk_label_set_text(GTK_LABEL(mwin->swap_free), "Swap?");

    gtk_widget_set_sensitive(GTK_WIDGET(mwin->swap_per), FALSE);
  }
  
  return TRUE;
}

static void do_update(void)
{
  update_values(current_window);
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SWAP_NUM    "ShowSwapNumbers"
#define SHOW_HOST   "ShowHostName"
#define GAUGE_WIDTH "GaugeWidth"
#define MEM_DISP    "MemoryAppletDisplay"
#define SWAP_DISP   "SwapAppletDisplay"

#if USE_XML
static gboolean read_choices_xml(void)
{
  gchar *fname;

  fname=choices_find_path_load("Config.xml", PROJECT);

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr node, root;
    xmlChar *string;
    
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

    if(strcmp(root->name, PROJECT)!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process data here */
      if(strcmp(node->name, "update")==0) {
 	string=xmlGetProp(node, "rate");
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
 	string=xmlGetProp(node, "mem");
	if(string) {
	  default_options.mem_disp=atoi(string);
	  free(string);
	}
 	string=xmlGetProp(node, "swap");
	if(string) {
	  default_options.swap_disp=atoi(string);
	  free(string);
	}
	
      } else if(strcmp(node->name, "Window")==0) {
 	string=xmlGetProp(node, "show-host");
	if(string) {
	  default_options.show_host=atoi(string);
	  free(string);
	}
 	string=xmlGetProp(node, "gauge-width");
	if(string) {
	  default_options.gauge_width=atoi(string);
	  free(string);
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
  gchar *fname;
  
#if USE_XML
  if(read_choices_xml())
    return;
#endif

  fname=choices_find_path_load("Config", PROJECT);
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
	      
	    } else if(strncmp(line, SWAP_NUM, sep-line)==0) {
	      /* Ignore */
	      dprintf(1, "obselete option %s, ignored", SWAP_NUM);
	      
	    } else if(strncmp(line, SHOW_HOST, sep-line)==0) {
	      default_options.show_host=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, GAUGE_WIDTH, sep-line)==0) {
	      default_options.gauge_width=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, MEM_DISP, sep-line)==0) {
	      default_options.mem_disp=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, SWAP_DISP, sep-line)==0) {
	      default_options.swap_disp=(guint) atoi(sep+1);
	      
	    } else {

	      dprintf(1, "unknown option %s, ignored", sep-line);
	    }
	  }
	}
      }
    } while(line);

    fclose(in);
  }
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

static void opts_changed(void)
{
  gfloat s;
  int i;
  MemWindow *mwin;
  GList *w;

  if(o_update_rate.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;
      
      gtk_timeout_remove(mwin->update_tag);
      mwin->update_tag=gtk_timeout_add(o_update_rate.int_value,
				    (GtkFunction) update_values, mwin);
    }
  }

  if(o_applet_size.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;

      if(mwin->is_applet)
	gtk_widget_set_size_request(mwin->win, o_applet_size.int_value,
				    o_applet_size.int_value);
    }
  }
  
  if(o_show_host.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;

      if(!mwin->is_applet) {
	if(o_show_host.int_value)
	  gtk_widget_show(mwin->host);
	else
	  gtk_widget_hide(mwin->host);
      }
    }
  }

  if(o_gauge_width.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;

      if(!mwin->is_applet) {
	gtk_widget_set_size_request(mwin->mem_per, o_gauge_width.int_value,
				    -1);
	gtk_widget_set_size_request(mwin->swap_per, o_gauge_width.int_value,
				    -1);
      }
    }
  }

  if(o_mem_disp.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;

      if(mwin->is_applet) {
	switch(o_mem_disp.int_value) {
	case AD_TOTAL:
	  gtk_widget_show(mwin->mem_total);
	  gtk_widget_hide(mwin->mem_used);
	  gtk_widget_hide(mwin->mem_free);
	  gtk_widget_hide(mwin->mem_per);
	  break;
      
	case AD_USED:
	  gtk_widget_hide(mwin->mem_total);
	  gtk_widget_show(mwin->mem_used);
	  gtk_widget_hide(mwin->mem_free);
	  gtk_widget_hide(mwin->mem_per);
	  break;
      
	case AD_FREE:
	  gtk_widget_hide(mwin->mem_total);
	  gtk_widget_hide(mwin->mem_used);
	  gtk_widget_show(mwin->mem_free);
	  gtk_widget_hide(mwin->mem_per);
	  break;
      
	case AD_PER:
	  gtk_widget_hide(mwin->mem_total);
	  gtk_widget_hide(mwin->mem_used);
	  gtk_widget_hide(mwin->mem_free);
	  gtk_widget_show(mwin->mem_per);
	  break;
      
	}
      }
    }
  }
  
  if(o_swap_disp.has_changed) {
    for(w=windows; w; w=g_list_next(w)) {
      mwin=(MemWindow *) w->data;

      if(mwin->is_applet) {
	switch(o_swap_disp.int_value) {
	case AD_TOTAL:
	  gtk_widget_show(mwin->swap_total);
	  gtk_widget_hide(mwin->swap_used);
	  gtk_widget_hide(mwin->swap_free);
	  gtk_widget_hide(mwin->swap_per);
	  break;
      
	case AD_USED:
	  gtk_widget_hide(mwin->swap_total);
	  gtk_widget_show(mwin->swap_used);
	  gtk_widget_hide(mwin->swap_free);
	  gtk_widget_hide(mwin->swap_per);
	  break;
      
	case AD_FREE:
	  gtk_widget_hide(mwin->swap_total);
	  gtk_widget_hide(mwin->swap_used);
	  gtk_widget_show(mwin->swap_free);
	  gtk_widget_hide(mwin->swap_per);
	  break;
      
	case AD_PER:
	  gtk_widget_hide(mwin->swap_total);
	  gtk_widget_hide(mwin->swap_used);
	  gtk_widget_hide(mwin->swap_free);
	  gtk_widget_show(mwin->swap_per);
	  break;
      
	}
      }
    }
  }
}

static void show_config_win(void)
{
  options_show();
}

static void close_window(void)
{
  MemWindow *mwin=current_window;

  if(!mwin)
    return;
  
  dprintf(1, "close_window %p %p", mwin, mwin->win);

  gtk_widget_hide(mwin->win);
  gtk_widget_unref(mwin->win);
  gtk_widget_destroy(mwin->win);

  current_window=NULL;
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),	NULL, show_config_win, 0, NULL},
  { N_("/Update Now"),	        NULL, do_update, 0, NULL },
  { N_("/Close"), 	        NULL, close_window, 0, NULL },
  { "/",                        NULL, NULL,         0, "<Separator>"},
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0, NULL },
};

static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save("menus", PROJECT, TRUE);
  if (menurc) {
    gtk_accel_map_save(menurc);
    g_free(menurc);
  }
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
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
    gtk_accel_map_load(menurc);
    g_free(menurc);
  }

  atexit(save_menus);

  menu = gtk_item_factory_get_widget(item_factory, "<system>");
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer udata)
{
  MemWindow *mwin=(MemWindow *) udata;

  current_window=mwin;
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==3) {
      if(!menu)
	menu_create_menu(window);

      if(mwin->is_applet)
	applet_popup_menu(mwin->win, menu, bev);
      else
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		       bev->button, bev->time);
      return TRUE;
    } else if(bev->button==1 && mwin->is_applet) {
      MemWindow *nwin=make_window(0);

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
  xmlNodePtr parent;
  gchar *str;
  guint32 xid=0;

  dprintf(3, "rpc_Open(%p, \"%s\", %p, %p)", server, action_name, args, udata);

  parent=args->data;
  if(parent) {

    str=xmlNodeGetContent(parent);
    if(str) {
      xid=(guint32) atol(str);
      g_free(str);
    }
  }

  make_window(xid);

  return NULL;
}

static void open_callback(ROXSOAP *serv, gboolean status, 
				  xmlDocPtr reply, gpointer udata)
{
  gboolean *s=udata;
  
  dprintf(3, "In open_callback(%p, %d, %p, %p)", clock, status, reply,
	 udata);
  *s=status;
  gtk_main_quit();
}

static gboolean open_remote(guint32 xid)
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

  doc=rox_soap_build_xml("Open", MEM_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sprintf(buf, "%lu", xid);
  xmlNewChild(node, NULL, "Parent", buf);

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

#if !SWAP_SUPPORTED_LIBGTOP
static void read_from_pipe(gpointer data, gint com, GdkInputCondition cond)
{
  char buf[BUFSIZ];
  int nr;

  nr=read(com, buf, BUFSIZ);
  dprintf(3, "read %d from %d", nr, com);
  
  if(nr>0) {
    buf[nr]=0;
    
    if(strncmp(buf, "total: ", 7)==0) {
      long tmp;
      char *start, *ptr;

      start=buf+7;
      tmp=strtol(start, &ptr, 10);
      if(ptr<=start)
	return;
      swap_data.valid=FALSE;
      swap_data.alloc=tmp<<10;

      start=strstr(ptr, " + ");
      if(!start)
	return;
      start+=3;
      tmp=strtol(start, &ptr, 10);
      if(ptr<=start)
	return;
      swap_data.reserved=tmp<<10;
      
      start=strstr(ptr, " = ");
      if(!start)
	return;
      start+=3;
      tmp=strtol(start, &ptr, 10);
      if(ptr<=start)
	return;
      swap_data.used=tmp<<10;
      
      start=strstr(ptr, ", ");
      if(!start)
	return;
      start+=2;
      tmp=strtol(start, &ptr, 10);
      if(ptr<=start)
	return;
      swap_data.avail=tmp<<10;

      swap_data.valid=TRUE;
    }
  } else if(nr==0) {
    int stat;
    
    waitpid(swap_process, &stat, 0);
    dprintf(2, "status %d from %d, closing %d", stat, swap_process, com);
    close(com);
    if(WIFEXITED(stat) && WEXITSTATUS(stat))
      dprintf(1, "%d exited with status %d", swap_process, WEXITSTATUS(stat));
    else if(WIFSIGNALED(stat))
      dprintf(1, "%d killed by signal %d %s", swap_process, WTERMSIG(stat),
	      strsignal(WTERMSIG(stat)));
    if(WIFEXITED(stat) || WIFSIGNALED(stat))
      swap_process=0;
    gdk_input_remove(swap_read_tag);
    
  } else {
    dprintf(1, "error reading data from pipe: %s", strerror(errno));
  }
}

static gboolean update_swap(gpointer unused)
{
  dprintf(2, "in update_swap");
  
  if(swap_process<=0) {
    int com[2];

    pipe(com);
    dprintf(3, "pipes are %d, %d", com[0], com[1]);
    swap_process=fork();

    switch(swap_process) {
    case -1:
      dprintf(0, "failed to fork! %s", strerror(errno));
      close(com[0]);
      close(com[1]);
      return;

    case 0:
      close(com[0]);
      dup2(com[1], 1);
      execl("/usr/sbin/swap", "swap", "-s", NULL);
      _exit(1);

    default:
      dprintf(3, "child is %d, monitor %d", swap_process, com[0]);
      close(com[1]);
      swap_read_tag=gdk_input_add(com[0], GDK_INPUT_READ,
				  read_from_pipe, NULL);
      break;
    }
  }
  
  return TRUE;
}
#endif

/*
 * $Log: mem.c,v $
 * Revision 1.13  2003/04/16 09:12:38  stephen
 * Re-enabled server code
 *
 * Revision 1.12  2003/03/05 15:30:40  stephen
 * First pass at conversion to GTK 2.
 *
 * Revision 1.11  2002/08/24 16:45:56  stephen
 * Fix compilation problem with libxml2.
 * Removed [] from if in configure.in
 *
 * Revision 1.10  2002/04/29 08:58:31  stephen
 * Use replacement applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.9  2002/04/12 10:25:25  stephen
 * Multiple windows.  Acts as a server, using SOAP, so there only needs to be
 * one instance running.
 *
 * Revision 1.8  2002/03/04 11:48:10  stephen
 * Stable release.
 *
 * Revision 1.7  2002/01/30 10:22:05  stephen
 * Use new applet menu positioning code.
 * Add -h and -v options.
 *
 * Revision 1.6  2001/11/30 11:55:31  stephen
 * Use smaller window on start up
 *
 * Revision 1.5  2001/11/12 14:45:27  stephen
 * Use XML for config file, if XML version >=2.4
 *
 * Revision 1.4  2001/09/06 09:53:31  stephen
 * Add some display options.
 *
 * Revision 1.3  2001/08/31 08:35:18  stephen
 * Added support for swap under Solaris, by piping "/usr/sbin/swap -s".
 *
 * Revision 1.2  2001/08/28 14:07:52  stephen
 * Fix compilation bugs on Linux.  Fix bug in setting non-existant label.
 * Show swap option out, compilation control on LIBGTOP_VERSION better.
 *
 * Revision 1.1.1.1  2001/08/24 11:08:30  stephen
 * Monitor memory and swap
 *
 *
 */
