/*
 * Plot system load average
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: load.c,v 1.13 2002/03/25 11:39:19 stephen Exp $
 *
 * Log at end of file
 */

#include "config.h"

#define DEBUG              1
#define INLINE_FONT_SEL    0
#define TRY_SERVER         1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>
#include "infowin.h"

#include <glibtop.h>
#include <glibtop/cpu.h>
#include <glibtop/loadavg.h>

#ifdef HAVE_XML
#include <tree.h>
#include <parser.h>
#endif

#if defined(HAVE_XML) && LIBXML_VERSION>=20400
#define USE_XML 1
#else
#define USE_XML 0
#endif

#if USE_XML && TRY_SERVER && ROX_CLIB_VERSION>=201
#define USE_SERVER 1
#else
#define USE_SERVER 0
#endif

#include "rox.h"
#include "applet.h"
#if USE_SERVER
#include "rox_soap.h"
#include "rox_soap_server.h"
#endif

static GtkWidget *menu=NULL;
static GtkWidget *infowin=NULL;
static GdkColormap *cmap = NULL;
static GdkColor colours[]={
  {0, 0xffff,0xffff,0xffff},
  {0, 0xffff,0,0},
  {0, 0xdddd,0,0},
  {0, 0xbbbb,0,0},
  {0, 0, 0xcccc, 0},
  {0, 0, 0xaaaa, 0},
  {0, 0, 0x8888, 0},
  {0, 0xd6d6, 0xd6d6, 0xd6d6},
  {0, 0, 0, 0}
};
enum {
  COL_WHITE,
  COL_HIGH0, COL_HIGH1, COL_HIGH2, COL_NORMAL0, COL_NORMAL1, COL_NORMAL2,
  COL_BG, COL_FG
};
#define COL_HIGH(i) (COL_HIGH0+(i))
#define COL_NORMAL(i) (COL_NORMAL0+(i))
#define NUM_COLOUR (sizeof(colours)/sizeof(colours[0]))

typedef struct colour_info {
  int colour;
  gdouble rgb[4];
  const char *use;
  const char *vname;
} ColourInfo;

static ColourInfo colour_info[]={
  {COL_NORMAL0, {0}, "Normal, 1 minute",   "NormalColour1"},
  {COL_NORMAL1, {0}, "Normal, 5 minutes",  "NormalColour5"},
  {COL_NORMAL2, {0}, "Normal, 15 minutes", "NormalColour15"},
  {COL_HIGH0,   {0}, "High, 1 minute",     "HighColour1"},
  {COL_HIGH1,   {0}, "High, 5 minutes",    "HighColour5"},
  {COL_HIGH2,   {0}, "High, 15 minutes",   "HighColour15"},
  {COL_FG,      {0}, "Foreground",         "ForegroundColour"},
  {COL_BG,      {0}, "Background",         "BackgroundColour"},
  
  {-1, {0}, NULL}
};

#define BAR_WIDTH     16          /* Width of bar */
#define BAR_GAP        4          /* Gap between bars */
#define BOTTOM_MARGIN 12          /* Margin at bottom of window */
#define TOP_MARGIN    10          /* Margin at top of window */
#define MIN_WIDTH     36          /* Minimum size of window */
#define MIN_HEIGHT    MIN_WIDTH
#define MAX_BAR_WIDTH 32          /* Maximum size of bars */
#define CHART_RES      2          /* Pixels per second for history */

typedef struct options {
  guint32 update_ms;
  gboolean show_max;
  gboolean show_vals;
  gboolean show_host;
  guint init_size;
  gchar *font_name;
  gboolean multiple_chart;
} Options;

static Options default_options={
  2000, TRUE, FALSE, FALSE, MIN_WIDTH,
  NULL, TRUE
};

typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *show_max;
  GtkWidget *show_vals;
  GtkWidget *show_host;
  GtkWidget *multiple_chart;
  GtkWidget *init_size;
  GtkWidget *colour_menu;
  GtkWidget *colour_show;
  GtkWidget *colour_dialog;
#if INLINE_FONT_SEL
  GtkWidget *font_sel;
#else
  GtkWidget *font_window;
  GtkWidget *font_name;
#endif
} OptionWidgets;

typedef struct history_data {
  double load[3];
  time_t when;
} History;

static glibtop *server=NULL;
static glibtop_loadavg load;
static guint data_update_tag=0;
static int max_load=1;
static double red_line=1.0;
static int reduce_delay=10;
static History *history=NULL;
static int nhistory=0;
static int ihistory=0;
#ifdef HAVE_GETHOSTNAME
static char hostname[256];
#endif

typedef struct load_window {
  GtkWidget *win;
  GtkWidget *canvas;
  GdkPixmap *pixmap;
  GdkGC *gc;
  
  guint update;
  gboolean is_applet;
  int hsize; /* Requested history size */
  
  Options options;
} LoadWindow;

#if USE_SERVER
static ROXSOAPServer *sserver=NULL;
#define LOAD_NAMESPACE_URL WEBSITE PROJECT
#endif

static GList *windows=NULL;
static LoadWindow *current_window=NULL;

static LoadWindow *make_window(guint32 socket);
static void remove_window(LoadWindow *win);
static gboolean window_update(LoadWindow *win);
static gboolean data_update(gpointer unused);
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event,
			    gpointer data);
static gint expose_event(GtkWidget *widget, GdkEventExpose *event,
			    gpointer data);
static void menu_create_menu(GtkWidget *window);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win);
static void show_info_win(void);
#if USE_SERVER
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid);
#endif

static void read_config(void);
static void write_config(const Options *);
#if USE_XML
static gboolean read_config_xml(void);
static void write_config_xml(const Options *);
#endif

static void usage(const char *argv0)
{
#if USE_SERVER
  printf("Usage: %s [X-options] [gtk-options] [-nrvh] [XID]\n", argv0);
#else
  printf("Usage: %s [X-options] [gtk-options] [-vh] [XID]\n", argv0);
#endif
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
#if USE_SERVER
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
#endif
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  XID\tX window to display applet in\n");
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
  printf("  Inline font selection... %s\n", INLINE_FONT_SEL? "yes": "no");
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
    if(ROX_CLIB_VERSION<201)
      printf(", no ROX-CLIB support");
    if(!USE_XML)
      printf(", XML not available");
    printf("\n");
  }
}

int main(int argc, char *argv[])
{
  LoadWindow *lwin=NULL;
  const char *app_dir;
  int c, do_exit, nerr;
  guint32 xid;
  int i;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
#if USE_SERVER
  const char *options="vhnr";
  gboolean new_run=FALSE;
  gboolean replace_server=FALSE;
#else
  const char *options="vh";
#endif

  rox_debug_init("Load");

#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif

#ifdef HAVE_GETHOSTNAME
  if(gethostname(hostname, sizeof(hostname))<0) {
    hostname[0]=0;
    dprintf(1, "couldn't get hostname: %s", strerror(errno));
    errno=0;
  } else {
    dprintf(1, "Running on %s", hostname);
  }
#endif

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  server=glibtop_init_r(&glibtop_global_server,
			(1<<GLIBTOP_SYSDEPS_CPU)|(1<<GLIBTOP_SYSDEPS_LOADAVG),
			0);
  if(server->ncpu>0)
    red_line=(double) server->ncpu;
  dprintf(2, "ncpu=%d %f", server->ncpu, red_line);

  gtk_init(&argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  cmap=gdk_rgb_get_cmap();
  gtk_widget_push_colormap(cmap);
  
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

  choices_init();
  dprintf(1, "read config");
  read_config();

  for(i=0; i<NUM_COLOUR; i++) {
    gdk_color_alloc(cmap, colours+i);
    dprintf(3, "colour %d: %4x, %4x, %4x: %ld", i, colours[i].red,
	   colours[i].green, colours[i].blue, colours[i].pixel);
  }

  data_update_tag=gtk_timeout_add(default_options.update_ms,
			       (GtkFunction) data_update, NULL);
  data_update(NULL);
  
  if(optind>=argc || !atol(argv[optind])) {
    xid=0;
  } else {
    dprintf(2, "argv[%d]=%s", optind, argv[optind]);
    xid=atol(argv[optind]);
  }

#if USE_SERVER
  if(replace_server || !rox_soap_ping(PROJECT)) {
    dprintf(1, "Making SOAP server");
    sserver=rox_soap_server_new(PROJECT, LOAD_NAMESPACE_URL);
    rox_soap_server_add_action(sserver, "Open", NULL, "Parent",
			       rpc_Open, NULL);
  } else if(!new_run) {
    if(open_remote(xid)) {
      dprintf(1, "success in open_remote(%lu), exiting", xid);
      return 0;
    }
  }
#endif
  lwin=make_window(xid);
  
  gtk_main();

#if USE_SERVER
  if(sserver)
    rox_soap_server_delete(sserver);
#endif

  return 0;
}

static void remove_window(LoadWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);
  /*gtk_widget_hide(win->win);*/

  gtk_timeout_remove(win->update);
  gdk_pixmap_unref(win->pixmap);
  gdk_gc_unref(win->gc);
  if(win->options.font_name)
    g_free((void *) win->options.font_name);
  g_free(win);

  dprintf(1, "windows=%p, number of active windows=%d", windows,
	  g_list_length(windows));
  if(g_list_length(windows)<1)
    gtk_main_quit();
}

static void window_gone(GtkWidget *widget, gpointer data)
{
  LoadWindow *lw=(LoadWindow *) data;

  dprintf(1, "Window gone: %p %p", widget, lw);

  remove_window(lw);
}

static LoadWindow *make_window(guint32 xid)
{
  LoadWindow *lwin;
  GtkWidget *vbox;
  int w=MIN_WIDTH, h=MIN_HEIGHT;
  int i;
  static GdkPixmap *pixmap=NULL;
  static GdkBitmap *mask=NULL;
  
  lwin=g_new0(LoadWindow, 1);

  lwin->options=default_options;
  if(lwin->options.font_name)
    lwin->options.font_name=g_strdup(lwin->options.font_name);
  lwin->hsize=0;

  w=lwin->options.init_size;
  h=lwin->options.init_size;
  if(!xid) {
    dprintf(2, "make window");
    lwin->is_applet=FALSE;
    lwin->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(lwin->win, "load");
    gtk_window_set_title(GTK_WINDOW(lwin->win), "Load");
    gtk_signal_connect(GTK_OBJECT(lwin->win), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       lwin);
    gtk_signal_connect(GTK_OBJECT(lwin->win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), lwin);
    gtk_widget_add_events(lwin->win, GDK_BUTTON_PRESS_MASK);
    gtk_window_set_wmclass(GTK_WINDOW(lwin->win), "Load", PROJECT);
    
    dprintf(3, "set size to %d,%d", w, h);
    gtk_widget_set_usize(lwin->win, w, h);

  } else {
    GtkWidget *plug;

    dprintf(2, "make plug");
    plug=gtk_plug_new(xid);
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       lwin);
    gtk_widget_set_usize(plug, w, h);
    
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), lwin);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    
    lwin->win=plug;
    lwin->is_applet=TRUE;
  }
  if(!menu)
    menu_create_menu(GTK_WIDGET(lwin->win));
  
  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(lwin->win), vbox);
  gtk_widget_show(vbox);

  lwin->canvas=gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(lwin->canvas), w-2, h-2);
  gtk_box_pack_start (GTK_BOX (vbox), lwin->canvas, TRUE, TRUE, 0);
  gtk_widget_show (lwin->canvas);
  gtk_signal_connect (GTK_OBJECT (lwin->canvas), "expose_event",
		      (GtkSignalFunc) expose_event, lwin);
  gtk_signal_connect (GTK_OBJECT (lwin->canvas), "configure_event",
		      (GtkSignalFunc) configure_event, lwin);
  gtk_widget_set_events (lwin->canvas, GDK_EXPOSURE_MASK);
  gtk_widget_realize(lwin->canvas);
  gtk_widget_set_name(lwin->canvas, "load display");
  lwin->gc=gdk_gc_new(lwin->canvas->window);
  gdk_gc_set_background(lwin->gc, colours+COL_BG);

  gtk_widget_realize(lwin->win);

  if(!pixmap) {
    gchar *fname=rox_resources_find(PROJECT, "AppIcon.xpm",
				    ROX_RESOURCES_NO_LANG);
    if(fname) {
      GtkStyle *style;
      style = gtk_widget_get_style(lwin->win);
      pixmap = gdk_pixmap_create_from_xpm(GTK_WIDGET(lwin->win)->window,
					  &mask, 
					  &style->bg[GTK_STATE_NORMAL],
					  fname);

      g_free(fname);
    }
  }
  if(pixmap)
    gdk_window_set_icon(GTK_WIDGET(lwin->win)->window, NULL, pixmap, mask);
    
  if(lwin->is_applet)
    applet_get_panel_location(lwin->win);
  gtk_widget_show(lwin->win);

  lwin->update=gtk_timeout_add(lwin->options.update_ms,
			       (GtkFunction) window_update, lwin);
  dprintf(3, "update tag is %u", lwin->update);
  
  windows=g_list_append(windows, lwin);
  current_window=lwin;
  gtk_widget_ref(lwin->win);

}

/* Return history data for the given index, either in the rolling window,
   or negative where  -1 is last,
   -2 for second last etc.  Return NULL if index not valid
*/
static History *get_history(int ind)
{
  dprintf(4, "request for history %d (%d, %d)", ind, ihistory, nhistory);
  
  if(ind<0) {
    if(ind<-nhistory || ind <-ihistory)
      return NULL;

    dprintf(4, "index %d", (ihistory+ind)%nhistory);
    return history+((ihistory+ind)%nhistory);
    
  } else {
    if(ind>=ihistory || ind<ihistory-nhistory)
      return NULL;

    dprintf(4, "index %d", ind%nhistory);
    return history+ind%nhistory;
  }

  return NULL;
}

static void append_history(time_t when, const double loadavg[3])
{
  int i, j;

  dprintf(4, "append history for 0x%lx", when);

  if(history && nhistory>0) {
    if(history[ihistory%nhistory].when==when)
      return;
    i=(ihistory++)%nhistory;
    for(j=0; j<3; j++)
      history[i].load[j]=loadavg[j];
    history[i].when=when;
  }  
}

static gboolean data_update(gpointer unused)
{
  time_t now;
  
  glibtop_get_loadavg_l(server, &load);
  time(&now);
  append_history(now, load.loadavg);

  return TRUE;
}

static gboolean window_update(LoadWindow *lwin)
{
  int w, h;
  int bw, mbh, bh, bm=BOTTOM_MARGIN, tm=TOP_MARGIN, l2=0;
  int bx, by, x, sbx;
  int cpu, other;
  double ld;
  GdkFont *font=NULL;
  int i, j;
  char buf[32];
  int reduce;
  int ndec=1, host_x=0;
  time_t now;
  History *hist;
  GtkStyle *style;
  static const char *bar_labels[]={
    "1m", "5m", "15m"
  };

  dprintf(4, "gc=%p canvas=%p pixmap=%p", lwin->gc, lwin->canvas,
	  lwin->pixmap);

  if(!lwin->gc || !lwin->canvas || !lwin->pixmap)
    return;

  h=lwin->canvas->allocation.height;
  w=lwin->canvas->allocation.width;
  
  if(!font && lwin->options.font_name) {
    font=gdk_font_load(lwin->options.font_name);
  }
  if(!font) 
    font=lwin->canvas->style->font;
  gdk_font_ref(font);
  
  style=gtk_widget_get_style(lwin->canvas);
  dprintf(3, "style=%p bg_gc[GTK_STATE_NORMAL]=%p", style,
	  style->bg_gc[GTK_STATE_NORMAL]);
  if(style && style->bg_gc[GTK_STATE_NORMAL]) {
    gtk_style_apply_default_background(style, lwin->pixmap, TRUE,
				       GTK_STATE_NORMAL,
				       NULL, 0, 0, w, h);
  } else {
    /* Blank out to the background colour */
    gdk_gc_set_foreground(lwin->gc, colours+COL_BG);
    gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, 0, 0, w, h);
  }

  bx=2;
  bw=(w-2-3*BAR_GAP)/3;
  if(bw>MAX_BAR_WIDTH) {
    bw=MAX_BAR_WIDTH;
    bx=w-2-3*bw-3*BAR_GAP-1;
  }
  if(lwin->options.show_vals) {
    int width, height;

    width=gdk_char_width(font, '0');
    height=gdk_char_height(font, '0');

    ndec=(bw/width)-2;
    if(ndec<1)
      ndec=1;
    if(ndec>3)
      ndec=3;

    tm=height+2;
    if(h-2*(height+4)-tm>2*bm)
      bm=2*(height+4);
    else
      bm=height+4;
    l2=bm-(height+4);
  }
  by=h-bm;
  if(by<48) 
    by=h-2;
  mbh=by-tm;
  if(mbh<48)
    mbh=by-2;
  dprintf(4, "w=%d bw=%d", w, bw);
  sbx=bx;

  time(&now);
  
  reduce=TRUE;
  for(i=0; i<3; i++) {
    if(load.loadavg[i]>max_load-1)
      reduce=FALSE;
    if(max_load<load.loadavg[i]) {
      max_load=((int) load.loadavg[i])+1;
      reduce_delay=10;
    }
  }
  if(reduce && max_load>=2.0 && history)
    for(i=-1; i>-nhistory-1; i--) {
      hist=get_history(i);
      if(!hist)
	break;
      for(j=0; j<3; j++)
	if(hist->load[j]>max_load/2)
	  reduce=FALSE;
    }
  if(reduce && max_load>1) {
    if(reduce_delay--<1)
      max_load--;
  }
  for(i=0; i<3; i++) {
    ld=load.loadavg[i];
    bh=mbh*ld/(double) max_load;
    gdk_gc_set_foreground(lwin->gc, colours+COL_NORMAL(i));
    gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh, bw, bh);
    
    dprintf(4, "load=%f max_load=%d, mbh=%d, by=%d, bh=%d, by-bh=%d",
	   ld, max_load, mbh, by, bh, by-bh);
    dprintf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bh);
    
    
    if(ld>red_line) {
      int bhred=mbh*(ld-red_line)/(double) max_load;
      int byred=mbh*red_line/(double) max_load;
      
      dprintf(5, "bhred=%d by-bhred=%d", bhred, by-bhred);
      dprintf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bhred);
      
      gdk_gc_set_foreground(lwin->gc, colours+COL_HIGH(i));
      /*gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, bw, bhred);*/
      gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			 bw, bh-byred);
    }
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_rectangle(lwin->pixmap, lwin->gc, FALSE, bx, by-bh, bw, bh);

    if(lwin->options.show_vals) {
      sprintf(buf, "%3.*f", ndec, ld);
      gdk_draw_string(lwin->pixmap, font, lwin->gc, bx+2, h-2-l2, buf);
      if(l2>0) {
	int xl=bx+2;
	int lw;

	lw=gdk_string_width(font, bar_labels[i]);
	xl=bx+bw/2-lw/2;
	gdk_draw_string(lwin->pixmap, font, lwin->gc, xl, h-2, bar_labels[i]);
      }
    }

    bx+=bw+BAR_GAP;
  }

  if(history && ihistory>0) {
    History *prev;
    int gwidth=CHART_RES;
    int gorg=sbx-BAR_GAP;

    dprintf(2, "draw history");
    bx=sbx-BAR_GAP;
    for(i=-1; bx>BAR_GAP; i--) {
      hist=get_history(i);
      dprintf(3, "get_history(%d)->%p", i, hist);
      if(!hist)
	break;
      prev=get_history(i-1);
      if(prev) {
	dprintf(4, "tdiff %d, org=%d", (int)(now-prev->when), gorg);
	bx=gorg-CHART_RES*(int)(now-prev->when);
	gwidth=CHART_RES*(int)(hist->when-prev->when);

	dprintf(4, " 0x%lx: 0x%lx, 0x%lx -> %d, %d", now, hist->when,
		prev->when, bx, gwidth);
      } else {
	bx=gorg-CHART_RES*(int)(now-hist->when-1)-CHART_RES;
	gwidth=CHART_RES;
	dprintf(3, "get_history(%d)==NULL", i-1);
      }
      if(bx<BAR_GAP) {
	gwidth-=(BAR_GAP-bx);
	bx=BAR_GAP;
      }
      if(gwidth<1)
	continue;

      for(j=lwin->options.multiple_chart? 2: 0; j>=0; j--) {
	ld=hist->load[j];
	bh=mbh*ld/(double) max_load;
	gdk_gc_set_foreground(lwin->gc, colours+COL_NORMAL(j));
	dprintf(4, "(%d, %d) size (%d, %d)", bx, by-bh, gwidth, bh);
	gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			   gwidth, bh);
	if(ld>red_line) {
	  int bhred=mbh*(ld-red_line)/max_load;
	  int byred=mbh*red_line/max_load;
	  gdk_gc_set_foreground(lwin->gc, colours+COL_HIGH(j));
	  /*gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, gwidth, bhred);*/
	  gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			     gwidth, bh-byred);
	}
      }
    }

    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_line(lwin->pixmap, lwin->gc, BAR_GAP, by, gorg, by);
    for(i=0, bx=gorg-2*i; bx>=BAR_GAP; i+=5, bx=gorg-CHART_RES*i)
      if(i%6==0) {
	gdk_draw_line(lwin->pixmap, lwin->gc, bx, by, bx, by+bm-l2);
	if(lwin->options.show_vals) {
	  sprintf(buf, "%d", i);
	  gdk_draw_string(lwin->pixmap, font, lwin->gc,
			  bx-gdk_string_width(font, buf)/2, h-2, buf);
	}
      } else {
	gdk_draw_line(lwin->pixmap, lwin->gc, bx, by, bx, by+(bm-l2)/2);
      }
  }

  if(lwin->options.show_max) {
    sprintf(buf, "Max %2d", max_load);
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_string(lwin->pixmap, font, lwin->gc, 2, tm, buf);

    host_x=2+gdk_string_width(font, buf)+4;
  }

  if(lwin->options.show_host) {
    const char *host=hostname;
    
    if(server->server_host)
      host=server->server_host;
    
    x=w-gdk_string_width(font, host)-2;
    if(x<host_x)
      x=host_x;
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_string(lwin->pixmap, font, lwin->gc, x, tm, host);
  }
  
  gtk_widget_queue_draw(lwin->canvas);    

  gdk_font_unref(font);

  return TRUE;
}

static void resize_history(LoadWindow *lwin, int width)
{
  int nrec;
  History *tmp;
  int i;

  dprintf(3, "width=%d CHART_RES=%d", width, CHART_RES);
  nrec=(width-2-3*MAX_BAR_WIDTH-3*BAR_GAP-1)/CHART_RES;
  if(nrec<0)
    nrec=0;
  lwin->hsize=nrec;

  dprintf(3, "nrec=%d, nhistory=%d, ihistory=%d", nrec, nhistory, ihistory);
  if(nrec<=nhistory)
    return;

  dprintf(2, "expand history from %d to %d", nhistory, nrec);

  if(ihistory<nhistory) {
    tmp=g_realloc(history, nrec*sizeof(History));
    if(!tmp)
      return;

    history=tmp;
    nhistory=nrec;
    
    for(i=ihistory; i<nhistory; i++)
      history[i].when=(time_t) 0;

    dprintf(3, "reset list");
    
    return;
  }

  if(!nhistory) {
    history=g_new(History, nrec);
    nhistory=nrec;
    ihistory=0;
    dprintf(3, "made list");
    return;
  }

  tmp=g_new(History, nrec);
  i=nhistory-(ihistory%nhistory);
  if(i==nhistory)
    i=0;
  dprintf(3, "split list: %d,%d %d,%d", (ihistory%nhistory), i,
	  0, nhistory-i);
  g_memmove(tmp, history+(ihistory%nhistory), i*sizeof(History));
  g_memmove(tmp+i, history, (nhistory-i)*sizeof(History));

  g_free(history);
  history=tmp;
  ihistory=nhistory;
  nhistory=nrec;
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event,
			 gpointer data)
{
  int w, h;
  LoadWindow *lwin=(LoadWindow *) data;
  
  if(!lwin->gc || !lwin->canvas || !lwin->pixmap)
    return;
  
  h=lwin->canvas->allocation.height;
  w=lwin->canvas->allocation.width;

  /* Copy the pixmap to the display canvas */
  gdk_draw_pixmap(lwin->canvas->window,
		  lwin->canvas->style->fg_gc[GTK_WIDGET_STATE (lwin->canvas)],
		  lwin->pixmap,
		  0, 0,
		  0, 0,
		  w, h);

  return TRUE;
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event,
			    gpointer data)
{
  LoadWindow *lwin=(LoadWindow *) data;

  dprintf(3, "configure_event(%p, %p, %p)", widget, event, data);
  dprintf(3, "pixmap=%p canvas=%p hsize=%d", lwin->pixmap, lwin->canvas,
	  lwin->hsize);
  
  if (lwin->pixmap)
    gdk_pixmap_unref(lwin->pixmap);

  lwin->pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height,
			  -1);
  resize_history(lwin, widget->allocation.width);
  dprintf(3, "pixmap=%p canvas=%p hsize=%d", lwin->pixmap, lwin->canvas,
	  lwin->hsize);
  window_update(lwin);

  return TRUE;
}

#if USE_XML
static void write_config_xml(const Options *opt)
{
  gchar *fname;
  gboolean ok;

  fname=choices_find_path_save("config.xml", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr tree, sub;
    FILE *out;
    int i;
    char buf[80];

    doc = xmlNewDoc("1.0");
    doc->children=xmlNewDocNode(doc, NULL, PROJECT, NULL);
    xmlSetProp(doc->children, "version", VERSION);

    tree=xmlNewChild(doc->children, NULL, "update", NULL);
    sprintf(buf, "%d", opt->update_ms);
    xmlSetProp(tree, "value", buf);
    
    tree=xmlNewChild(doc->children, NULL, "show", NULL);
    sprintf(buf, "%d", opt->show_max);
    xmlSetProp(tree, "max", buf);
    sprintf(buf, "%d", opt->show_vals);
    xmlSetProp(tree, "vals", buf);
    sprintf(buf, "%d", opt->show_host);
    xmlSetProp(tree, "host", buf);
    sprintf(buf, "%d", opt->multiple_chart);
    xmlSetProp(tree, "multiple-charts", buf);
    
    if(opt->font_name)
      tree=xmlNewChild(doc->children, NULL, "font", opt->font_name);
    
    tree=xmlNewChild(doc->children, NULL, "applet", NULL);
    sprintf(buf, "%d", opt->init_size);
    xmlSetProp(tree, "initial-size", buf);
    
    tree=xmlNewChild(doc->children, NULL, "colours", NULL);
    for(i=0;  colour_info[i].colour>=0; i++) {
      GdkColor *col=colours+colour_info[i].colour;

      sub=xmlNewChild(tree,  NULL, "colour", colour_info[i].use);
      xmlSetProp(sub, "name", colour_info[i].vname);
      sprintf(buf, "0x%04x", col->red);
      xmlSetProp(sub, "red", buf);
      sprintf(buf, "0x%04x", col->green);
      xmlSetProp(sub, "green", buf);
      sprintf(buf, "0x%04x", col->blue);
      xmlSetProp(sub, "blue", buf);
    }
  
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);

    xmlFreeDoc(doc);
    g_free(fname);
  }
}
#endif

static void write_config(const Options *opt)
{
#if !USE_XML
  gchar *fname;

  fname=choices_find_path_save("config", PROJECT, TRUE);
  dprintf(1, "save to %s", fname? fname: "NULL");

  if(fname) {
    FILE *out;

    out=fopen(fname, "w");
    if(out) {
      time_t now;
      char buf[80];
      int i;
      
      fprintf(out, "# Config file for %s %s (%s)\n", PROJECT, VERSION, AUTHOR);
      fprintf(out, "# Latest version at %s\n", WEBSITE);
      time(&now);
      strftime(buf, 80, "%c", localtime(&now));
      fprintf(out, "#\n# Written %s\n\n", buf);

      fprintf(out, "update_ms=%d\n", opt->update_ms);
      fprintf(out, "show_max=%d\n", opt->show_max);
      fprintf(out, "show_vals=%d\n", opt->show_vals);
      fprintf(out, "show_host=%d\n", opt->show_host);
      fprintf(out, "multiple_chart=%d\n", opt->multiple_chart);
      fprintf(out, "init_size=%d\n", (int) opt->init_size);
      if(opt->font_name)
	fprintf(out, "font_name=%s\n", opt->font_name);

      for(i=0; colour_info[i].colour>=0; i++) {
	GdkColor *col=colours+colour_info[i].colour;
	
	fprintf(out, "%s=0x%04x 0x%04x 0x%04x\n", colour_info[i].vname,
		col->red, col->green, col->blue);
      }

      fclose(out);
    }

    g_free(fname);
  }
#else
  write_config_xml(opt);
#endif
}

#if USE_XML
static gboolean read_config_xml(void)
{
  guchar *fname;

  fname=choices_find_path_load("config.xml", PROJECT);

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

    if(strcmp(root->name, PROJECT)!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *str;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      if(strcmp(node->name, "update")==0) {
 	str=xmlGetProp(node, "value");
	if(!str)
	  continue;
	default_options.update_ms=atol(str);
	free(str);

      } else if(strcmp(node->name, "show")==0) {
	str=xmlGetProp(node, "max");
	if(str) {
	  default_options.show_max=atoi(str);
	  free(str);
	}
	str=xmlGetProp(node, "vals");
	if(str) {
	  default_options.show_vals=atoi(str);
	  free(str);
	}
	str=xmlGetProp(node, "host");
	if(str) {
	  default_options.show_host=atoi(str);
	  free(str);
	}
	str=xmlGetProp(node, "multiple-charts");
	if(str) {
	  default_options.multiple_chart=atoi(str);
	  free(str);
	}

      } else if(strcmp(node->name, "font")==0) {
	str=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!str)
	  continue;
	if(default_options.font_name)
	  g_free(default_options.font_name);
	default_options.font_name=g_strdup(str);
	free(str);

      } else if(strcmp(node->name, "applet")==0) {
 	str=xmlGetProp(node, "initial-size");
	if(!str)
	  continue;
	default_options.init_size=atoi(str);
	free(str);

      } else if(strcmp(node->name, "colours")==0) {
	xmlNodePtr sub;

	for(sub=node->xmlChildrenNode; sub; sub=sub->next) {
	  if(sub->type!=XML_ELEMENT_NODE)
	    continue;

	  if(strcmp(sub->name, "colour")==0) {
	    int i;
	    GdkColor *col=NULL;
	    
	    str=xmlGetProp(sub, "name");
	    if(!str)
	      continue;
	    for(i=0; colour_info[i].colour>=0; i++) {
	      if(strcmp(str, colour_info[i].vname)==0) {
		col=colours+colour_info[i].colour;
		break;
	      }
	    }
	    g_free(str);
	    if(col) {
	      unsigned long r=col->red, g=col->green, b=col->blue;

	      str=xmlGetProp(sub, "red");
	      if(str) {
		r=strtoul(str, NULL, 16);
		g_free(str);
	      }
	      str=xmlGetProp(sub, "green");
	      if(str) {
		g=strtoul(str, NULL, 16);
		g_free(str);
	      }
	      str=xmlGetProp(sub, "blue");
	      if(str) {
		b=strtoul(str, NULL, 16);
		g_free(str);
	      }

	      col->red=r;
	      col->green=g;
	      col->blue=b;

	      dprintf(2, "colour %s=(0x%x, 0x%x, 0x%x)", colour_info[i].vname,
		      col->red, col->green, col->blue);
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

static void read_config(void)
{
  guchar *fname;

#if USE_XML
  if(read_config_xml())
    return;
#endif

  fname=choices_find_path_load("config", PROJECT);

  if(fname) {
    FILE *in;

    in=fopen(fname, "r");
    if(in) {
      char buf[1024], *line;
      char *end;
      gchar *words;

      do {
	line=fgets(buf, sizeof(buf), in);
	if(!line)
	  break;
	end=strchr(line, '\n');
	if(end)
	  *end=0;
	end=strchr(line, '#');
	if(end)
	  *end=0;

	words=g_strstrip(line);
	if(words[0]) {
	  gchar *var, *val;

	  end=strchr(words, '=');
	  if(end) {
	    val=g_strstrip(end+1);
	    *end=0;
	    var=g_strstrip(words);

	    if(strcmp(var, "update_ms")==0) {
	      default_options.update_ms=atoi(val);

	    } else if(strcmp(var, "show_max")==0) {
	      default_options.show_max=atoi(val);
	    } else if(strcmp(var, "show_vals")==0) {
	      default_options.show_vals=atoi(val);
	    } else if(strcmp(var, "show_host")==0) {
	      default_options.show_host=atoi(val);
	    } else if(strcmp(var, "multiple_chart")==0) {
	      default_options.multiple_chart=atoi(val);
	    } else if(strcmp(var, "init_size")==0) {
	      default_options.init_size=(guint) atoi(val);
	    } else if(strcmp(var, "font_name")==0) {
	      if(default_options.font_name)
		g_free(default_options.font_name);
	      default_options.font_name=g_strdup(val);
	    } else {
	      int i;

	      dprintf(3, "%s = %s", var, val);
	      for(i=0; colour_info[i].colour>=0; i++) {
		if(strcmp(var, colour_info[i].vname)==0) {
		  GdkColor col;
		  char *end;
		  long lval;

		  dprintf(3, "matched colour %d %s", colour_info[i].colour,
			  colour_info[i].use);

		  lval=strtol(val, &end, 16);
		  if(end) {
		    col.red=(gushort) lval;

		    lval=strtol(end, &end, 16);
		  }
		  if(end) {
		    col.green=(gushort) lval;

		    lval=strtol(end, &end, 16);
		  }
		  if(end) {
		    col.blue=(gushort) lval;

		    memcpy(colours+colour_info[i].colour, &col,
			   sizeof(GdkColor));

		    dprintf(3, "set colour %x %x %x", col.red, col.green,
			    col.blue);
		  }
		  break;
		}
	      }
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);
    }

    g_free(fname);
  }
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
  int i;
  
  for(i=0; colour_info[i].colour>=0; i++) {
    GdkColor *col=colours+colour_info[i].colour;

    colour_info[i].rgb[0]=((double) col->red)/65535.;
    colour_info[i].rgb[1]=((double) col->green)/65535.;
    colour_info[i].rgb[2]=((double) col->blue)/65535.;
    colour_info[i].rgb[3]=0;
  }
  
  gtk_widget_hide(confwin);
}

static void set_config(GtkWidget *widget, gpointer data)
{
  OptionWidgets *ow=(OptionWidgets *) data;
  gfloat s;
  int i;
  GdkColormap *cmap;
  gchar *text;
  LoadWindow *lwin;
  Options *opts;

  lwin=gtk_object_get_data(GTK_OBJECT(ow->window), "LoadWindow");
  opts=lwin? &lwin->options: &default_options;

  s=gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(ow->update_s));
  opts->update_ms=(guint) (s*1000);
  opts->show_max=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_max));
  opts->show_vals=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_vals));
  opts->show_host=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_host));
  opts->multiple_chart=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->multiple_chart));
  opts->init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));

  if(lwin) {
    gtk_timeout_remove(lwin->update);
    lwin->update=gtk_timeout_add(opts->update_ms, (GtkFunction) window_update,
				 lwin);
  }
  gtk_timeout_remove(data_update_tag);
  data_update_tag=gtk_timeout_add(opts->update_ms, (GtkFunction) data_update,
				 NULL);

  if(opts->font_name)
    g_free(opts->font_name);
#if INLINE_FONT_SEL
  opts->font_name=
    gtk_font_selection_get_font_name(GTK_FONT_SELECTION(ow->font_sel));
#else
  gtk_label_get(GTK_LABEL(ow->font_name), &text);
  opts->font_name=g_strdup(text);
#endif
  
  cmap=gdk_rgb_get_cmap();
  for(i=0; colour_info[i].colour>=0; i++) {
    GdkColor *col=colours+colour_info[i].colour;

    col->red=(gushort)(colour_info[i].rgb[0]*65535);
    col->green=(gushort)(colour_info[i].rgb[1]*65535);
    col->blue=(gushort)(colour_info[i].rgb[2]*65535);

    gdk_color_alloc(cmap, col);    
  }

  gtk_widget_hide(ow->window);
}

static void save_config(GtkWidget *widget, gpointer data)
{
  set_config(widget, data);
  write_config(current_window? &current_window->options: &default_options);
}

#if !INLINE_FONT_SEL
static void set_font(GtkWidget *widget, gpointer data)
{
  OptionWidgets *ow=(OptionWidgets *) data;
  gchar *name;

  name=gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(ow->font_window));
  if(name) {
    gtk_label_set_text(GTK_LABEL(ow->font_name), name);
    g_free(name);
    gtk_widget_hide(ow->font_window);
  }
}

static void show_font_window(GtkWidget *widget, gpointer data)
{
  OptionWidgets *ow=(OptionWidgets *) data;
  gchar *name;

  gtk_label_get(GTK_LABEL(ow->font_name), &name);
  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(ow->font_window),
					  name);

  gtk_widget_show(ow->font_window);
}

static void hide_font_window(GtkWidget *widget, gpointer data)
{
  OptionWidgets *ow=(OptionWidgets *) data;
  
  gtk_widget_hide(ow->font_window);
}

#endif

static void append_option_menu_item(GtkWidget *base, GtkWidget *optmen,
				    const char *label, int index,
				    GtkSignalFunc func,
				    int *mw, int *mh)
{
  GtkWidget *optitem;
  GtkRequisition req;
  
  optitem=gtk_menu_item_new_with_label(label);
  dprintf(3, "menu item %p has label \"%s\"", optitem, label);
  dprintf(3, " child=%p", GTK_BIN(optitem)->child);
  gtk_object_set_data(GTK_OBJECT(optitem), "index", GINT_TO_POINTER(index));
  gtk_object_set_data(GTK_OBJECT(optitem), "label", g_strdup(label));
  gtk_widget_show(optitem);
  gtk_widget_size_request(optitem, &req);
  if(*mw<req.width)
    *mw=req.width;
  if(*mh<req.height)
    *mh=req.height;
  gtk_object_set_data(GTK_OBJECT(base), "signal-func", (gpointer) func);
  if(func)
    gtk_signal_connect(GTK_OBJECT(optitem), "activate",
		       GTK_SIGNAL_FUNC(func), base);
  gtk_menu_append(GTK_MENU(optmen), optitem);
  dprintf(3, "appended %p(%p) to %p", optitem, GTK_BIN(optitem)->child,
	  optmen);
}

static GtkWidget *make_option_menu_item(const char *lbl, GtkWidget *hbox,
					GList *items,
					GtkSignalFunc func, int def)
{
  GtkWidget *label;
  GtkWidget *optmenu;
  GtkWidget *optitem;
  int mw, mh, i;

  if(lbl && hbox) {
    label=gtk_label_new(lbl);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  }

  optitem=gtk_option_menu_new();
  gtk_widget_show(optitem);
  if(hbox)
    gtk_box_pack_start(GTK_BOX(hbox), optitem, FALSE, FALSE, 2);

  optmenu=gtk_menu_new();
  dprintf(2, "menu for option menu \"%s\" is at %p", lbl? lbl: "NULL",
	optmenu);

  mw=0;
  mh=0;
  for(i=0; items; items=g_list_next(items), i++) 
    append_option_menu_item(optitem, optmenu, (const char *) items->data, i,
			    func, &mw, &mh);
  
  gtk_option_menu_set_menu(GTK_OPTION_MENU(optitem), optmenu);
  dprintf(3, "Set %p as menu for %p", optmenu, optitem);

  gtk_widget_set_usize(optitem, mw+50, mh+4);
  gtk_option_menu_set_history(GTK_OPTION_MENU(optitem), def);

  if(func)
    gtk_object_set_data(GTK_OBJECT(optitem), "signal-func", func);

  return optitem;
}

static void show_colour(GtkWidget *item, gdouble rgb[3])
{
  GtkStyle *style;
  GdkColormap *cmap;
  
  style=gtk_style_copy(gtk_widget_get_style(item));
  cmap=gdk_rgb_get_cmap();
  gdk_colormap_ref(cmap);
  
  style->bg[GTK_STATE_NORMAL].red=(gushort)(rgb[0]*65535);
  style->bg[GTK_STATE_NORMAL].green=(gushort)(rgb[1]*65535);
  style->bg[GTK_STATE_NORMAL].blue=(gushort)(rgb[2]*65535);
  style->bg[GTK_STATE_PRELIGHT].red=(gushort)(rgb[0]*65535) | 0x80ff;
  style->bg[GTK_STATE_PRELIGHT].green=(gushort)(rgb[1]*65535) | 0x80ff;
  style->bg[GTK_STATE_PRELIGHT].blue=(gushort)(rgb[2]*65535) | 0x80ff;
  
  gdk_color_alloc(cmap, &style->bg[GTK_STATE_NORMAL]);
  
  gtk_widget_set_style(item, style);

  gdk_colormap_unref(cmap);
}

static void colour_select(GtkMenuItem *item, gpointer udata)
{
  gint ind;
  OptionWidgets *ow;
  GtkWidget *optmenu;

  ind=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item), "index"));
  optmenu=GTK_WIDGET(udata);
  ow=(OptionWidgets *) gtk_object_get_data(GTK_OBJECT(optmenu), "options");
  
  show_colour(ow->colour_show, colour_info[ind].rgb);
}

static void show_colour_dialog(GtkWidget *button, gpointer udata)
{
  OptionWidgets *ow=(OptionWidgets *) udata;
  gdouble rgb[4];
  gint ind;
  GtkWidget *menu, *item;
  GtkWidget *sel;

  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(ow->colour_menu));
  item=gtk_menu_get_active(GTK_MENU(menu));
  ind=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item), "index"));

  gtk_window_set_title(GTK_WINDOW(ow->colour_dialog), colour_info[ind].use);
  sel=GTK_COLOR_SELECTION_DIALOG(ow->colour_dialog)->colorsel;
  gtk_color_selection_set_color(GTK_COLOR_SELECTION(sel),
				colour_info[ind].rgb);
  gtk_widget_show(ow->colour_dialog);
}

static void colour_dialog_set(GtkWidget *button, gpointer udata)
{
  OptionWidgets *ow=(OptionWidgets *) udata;
  gdouble rgb[4];
  GtkWidget *menu, *item;
  GtkWidget *sel;
  gint ind;

  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(ow->colour_menu));
  item=gtk_menu_get_active(GTK_MENU(menu));
  ind=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item), "index"));

  sel=GTK_COLOR_SELECTION_DIALOG(ow->colour_dialog)->colorsel;
  gtk_color_selection_get_color(GTK_COLOR_SELECTION(sel),
				colour_info[ind].rgb);

  show_colour(ow->colour_show, colour_info[ind].rgb);
    
  gtk_widget_hide(ow->colour_dialog);
}

static void colour_dialog_cancel(GtkWidget *button, gpointer udata)
{
  OptionWidgets *ow=(OptionWidgets *) udata;

  gtk_widget_hide(ow->colour_dialog);
}

static void show_config_win(void)
{
  static GtkWidget *confwin=NULL;
  static OptionWidgets ow;

  if(!confwin) {
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    GtkWidget *label;
    GtkObject *range;
    GtkWidget *spin;
    GtkWidget *check;
    GtkStyle *style;
    
    GList *cols;
    int i;

    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), "Configuration");
    gtk_window_set_position(GTK_WINDOW(confwin), GTK_WIN_POS_MOUSE);
    gtk_window_set_wmclass(GTK_WINDOW(confwin), "Config", PROJECT);
    ow.window=confwin;

    vbox=GTK_DIALOG(confwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Update rate (s)");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    range=gtk_adjustment_new((gfloat)(current_window->options.update_ms/1000.),
			     1.0, 60., 0.1, 1., 1.);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 5);
    gtk_widget_set_name(spin, "update_s");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 2);
    ow.update_s=spin;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Display");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    check=gtk_check_button_new_with_label("Show maximum");
    gtk_widget_set_name(check, "show_max");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 current_window->options.show_max);
    ow.show_max=check;

    check=gtk_check_button_new_with_label("Show values");
    gtk_widget_set_name(check, "show_vals");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 current_window->options.show_vals);
    ow.show_vals=check;

    check=gtk_check_button_new_with_label("Show hostname");
    gtk_widget_set_name(check, "show_host");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 current_window->options.show_host);
    ow.show_host=check;

    check=gtk_check_button_new_with_label("Multiple charts");
    gtk_widget_set_name(check, "multiple_chart");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 current_window->options.multiple_chart);
    ow.multiple_chart=check;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Initial size");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    range=gtk_adjustment_new((gfloat) current_window->options.init_size,
			     16, 128, 2, 16, 16);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 0);
    gtk_widget_set_name(spin, "init_size");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 2);
    ow.init_size=spin;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    cols=NULL;
    for(i=0; colour_info[i].colour>=0; i++) {
      GdkColor *col=colours+colour_info[i].colour;
      
      colour_info[i].rgb[0]=((double) col->red)/65535.;
      colour_info[i].rgb[1]=((double) col->green)/65535.;
      colour_info[i].rgb[2]=((double) col->blue)/65535.;
      colour_info[i].rgb[3]=1.0;
      
      cols=g_list_append(cols, (gpointer) colour_info[i].use);
    }
    ow.colour_menu=make_option_menu_item("Colour", hbox, cols,
					 colour_select, 0);
    gtk_widget_show(ow.colour_menu);
    gtk_object_set_data(GTK_OBJECT(ow.colour_menu), "options", (gpointer) &ow);

    ow.colour_dialog=gtk_color_selection_dialog_new("Select colour");
    gtk_signal_connect(GTK_OBJECT(ow.colour_dialog), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     ow.colour_dialog);
    gtk_widget_hide(GTK_COLOR_SELECTION_DIALOG(ow.colour_dialog)->help_button);
    gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(ow.colour_dialog)->ok_button),
		       "clicked", GTK_SIGNAL_FUNC(colour_dialog_set),
		       &ow);
    gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(ow.colour_dialog)->cancel_button),
		       "clicked", GTK_SIGNAL_FUNC(colour_dialog_cancel),
		       &ow);
    gtk_window_set_wmclass(GTK_WINDOW(ow.colour_dialog), "Colour", PROJECT);

    but=gtk_button_new_with_label("Change");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);    
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(show_colour_dialog), &ow);
    ow.colour_show=but;
    show_colour(ow.colour_show, colour_info[0].rgb);

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Display font");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

#if INLINE_FONT_SEL
    ow.font_sel=gtk_font_selection_new();
    gtk_font_selection_set_preview_text(GTK_FONT_SELECTION(ow.font_sel),
					"0.123 Max 2 localhost");
    gtk_widget_show(ow.font_sel);
    gtk_box_pack_start(GTK_BOX(hbox), ow.font_sel, FALSE, FALSE, 2);
#else
    ow.font_name=gtk_label_new(current_window->options.font_name?
			       current_window->options.font_name: "");
    gtk_widget_show(ow.font_name);
    gtk_box_pack_start(GTK_BOX(hbox), ow.font_name, FALSE, FALSE, 2);

    ow.font_window=gtk_font_selection_dialog_new("Choose display font");
    gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(ow.font_window),
					"0.123 Max 2 localhost");
    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(ow.font_window)->ok_button), 
		       "clicked", GTK_SIGNAL_FUNC(set_font), &ow);

    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(ow.font_window)->cancel_button), 
		       "clicked", GTK_SIGNAL_FUNC(hide_font_window), &ow);
    but=gtk_button_new_with_label("Change");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(show_font_window), &ow);

#endif
    
    hbox=GTK_DIALOG(confwin)->action_area;

    but=gtk_button_new_with_label("Save");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(save_config), &ow);

    but=gtk_button_new_with_label("Set");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       &ow);

    but=gtk_button_new_with_label("Cancel");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(cancel_config), confwin);

  } else {
    GtkStyle *style;
    
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.update_s),
			    (gfloat)(current_window->options.update_ms/1000.));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_max),
				 current_window->options.show_max);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_vals),
				 current_window->options.show_vals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_host),
				 current_window->options.show_host);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.multiple_chart),
				 current_window->options.multiple_chart);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.init_size),
			      (gfloat)current_window-> options.init_size);

    gtk_option_menu_set_history(GTK_OPTION_MENU(ow.colour_menu), 0);
    show_colour(ow.colour_show, colour_info[0].rgb);

    gtk_widget_hide(ow.colour_dialog);
  }
#if INLINE_FONT_SEL
  if(current_window->options.font_name)
    gtk_font_selection_set_font_name(GTK_FONT_SELECTION(ow.font_sel),
				     current_window->options.font_name);
#else
  if(current_window->options.font_name) {
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(ow.font_window),
				     current_window->options.font_name);
    gtk_label_set_text(GTK_LABEL(ow.font_name),
		       current_window->options.font_name);
  }
#endif
  gtk_widget_show(confwin);
}

static void close_window(void)
{
  LoadWindow *lw=current_window;

  dprintf(1, "close_window %p %p", lw, lw->win);

  gtk_widget_hide(lw->win);
  gtk_widget_unref(lw->win);
  gtk_widget_destroy(lw->win);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),	NULL, show_info_win, 0, NULL },
  { N_("/Configure"),	NULL, show_config_win, 0, NULL },
  { N_("/Close"), 	NULL, close_window, 0, NULL },
  { N_("/Quit"),	NULL, gtk_main_quit, 0, NULL },
};

static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save("menus", PROJECT, TRUE);
  if (menurc) {
    gboolean	mod = FALSE;

    gtk_item_factory_dump_rc(menurc, NULL, TRUE);
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
  gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  menu = gtk_item_factory_get_widget(item_factory, "<system>");

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
    gtk_item_factory_parse_rc(menurc);
    g_free(menurc);
  }

  atexit(save_menus);
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  LoadWindow *lwin=(LoadWindow *) win;

  current_window=lwin;

  dprintf(2, "Button press on %p %p", window, lwin);
  
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    if(!menu)
      menu_create_menu(GTK_WIDGET(win));

    dprintf(3, "show menu for %s", lwin->is_applet? "applet": "window");
    if(lwin->is_applet)
      applet_show_menu(menu, bev);
    else
      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
    return TRUE;
  } else if(/*lwin->is_applet && */bev->type==GDK_BUTTON_PRESS && bev->button==1) {
    make_window(0);
      
    return TRUE;
  }

  return FALSE;
}

#if USE_SERVER
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  xmlNodePtr parent;
  guint32 xid=0;

  parent=args->data;
  if(parent) {
    gchar *str;

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

  doc=rox_soap_build_xml("Open", LOAD_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sprintf(buf, "%lu", xid);
  xmlNewChild(node, NULL, "Parent", buf);

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  dprintf(3, "sent %d", sent);

  xmlFreeDoc(doc);
  if(sent)
    gtk_main();
  rox_soap_close(serv);

  return sent && ok;
}

#endif

static void show_info_win(void)
{
  if(!infowin) {
    infowin=info_win_new(PROGRAM, PURPOSE, VERSION, AUTHOR, WEBSITE);
    gtk_window_set_wmclass(GTK_WINDOW(infowin), "Info", PROJECT);
  }

  gtk_window_set_position(GTK_WINDOW(infowin), GTK_WIN_POS_MOUSE);
  gtk_widget_show(infowin);
}

/*
 * $Log: load.c,v $
 * Revision 1.13  2002/03/25 11:39:19  stephen
 * Use ROX-CLib's SOAP server code to have one instance run multiple windows.
 * Added an icon.
 *
 * Revision 1.12  2002/03/04 11:48:53  stephen
 * Stable release.
 *
 * Revision 1.11  2002/01/30 10:23:34  stephen
 * Use new applet menu positioning code. Add -h and -v options.
 *
 * Revision 1.10  2001/11/30 11:53:15  stephen
 * Can show multiple charts (1 min in front, 15min at back)
 *
 * Revision 1.9  2001/11/12 14:43:47  stephen
 * Uses XML for config, if XML 2.4 or later.
 * Can select a font for display.
 *
 * Revision 1.8  2001/10/04 13:47:47  stephen
 * Can change foreground & background colours as well.
 *
 * Revision 1.7  2001/09/25 14:59:22  stephen
 * Create menu early to enable accelerators.
 * Time index the history so we can plot it right when the sample rate varies.
 * Bars now have slightly different shades.  Colours can be changed.
 * No longer check for changed config, not needed because of the applet menu.
 *
 * Revision 1.6  2001/08/20 09:31:19  stephen
 * Switch to using ROX-CLib.  Option to display hostname.
 *
 * Revision 1.5  2001/07/06 08:19:55  stephen
 * Don't scale down if the history is still big.  Support menu accelerators.
 * Run full screen version when applet clicked on.  Better debug output (uses
 * g_logv).
 *
 * Revision 1.4  2001/05/25 07:56:25  stephen
 * Added support for menu on applet, can configure initial applet size.
 *
 * Revision 1.3  2001/05/10 14:56:18  stephen
 * Added strip chart if window is wide enough.
 *
 * Revision 1.2  2001/04/24 07:52:33  stephen
 * Display tweaks
 *
 */
