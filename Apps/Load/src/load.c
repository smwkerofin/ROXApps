/*
 * Plot system load average
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: load.c,v 1.28 2006/03/07 19:23:51 stephen Exp $
 *
 * Log at end of file
 */

#include "config.h"

#define DEBUG              1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/stat.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#define USE_SERVER 1

#include <rox/rox.h>
#include <rox/applet.h>
#include <rox/rox_soap.h>
#include <rox/rox_soap_server.h>
#include <rox/options.h>
#include <rox/menu.h>

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

#define BAR_WIDTH     16          /* Width of bar */
#define BAR_GAP        4          /* Gap between bars */
#define BOTTOM_MARGIN 12          /* Margin at bottom of window */
#define TOP_MARGIN    10          /* Margin at top of window */
#define MIN_WIDTH     36          /* Minimum size of window */
#define MIN_HEIGHT    MIN_WIDTH
#define MAX_BAR_WIDTH 32          /* Maximum size of bars */
#define CHART_RES      2          /* Pixels per second for history */
#define TEXT_MARGIN    2

static double load[3];     /* Latest result */

typedef struct history_data {
  double load[3];
  time_t when;
} History;

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

static ROXOption opt_update_rate;
static ROXOption opt_show_max;
static ROXOption opt_show_vals;
static ROXOption opt_show_host;
static ROXOption opt_multiple;
static ROXOption opt_applet_size;
static ROXOption opt_col_norm1;
static ROXOption opt_col_norm5;
static ROXOption opt_col_norm15;
static ROXOption opt_col_high1;
static ROXOption opt_col_high5;
static ROXOption opt_col_high15;
static ROXOption opt_col_fg;
static ROXOption opt_col_bg;
static ROXOption opt_font;

typedef struct load_window {
  GtkWidget *win;
  GtkWidget *canvas;
  GtkWidget *menu;
  GdkPixmap *pixmap;
  GdkGC *gc;
  
  guint update;
  gboolean is_applet;
  int hsize; /* Requested history size */
} LoadWindow;

static ROXSOAPServer *sserver=NULL;
#define LOAD_NAMESPACE_URL WEBSITE PROJECT

static GList *windows=NULL;
static LoadWindow *current_window=NULL;

static int get_num_cpu(void);
static LoadWindow *make_window(guint32 socket);
static void remove_window(LoadWindow *win);
static gboolean window_update(LoadWindow *win);
static gboolean data_update(gpointer unused);
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event,
			    gpointer data);
static gint expose_event(GtkWidget *widget, GdkEventExpose *event,
			    gpointer data);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win);
static void show_info_win(void);
static void close_window(void);

static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid);
static gboolean options_remote(void);
static void show_config_win(void);

static void setup_config(void);
static void opts_changed(void);

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),	NULL, show_info_win, 0, "<StockItem>",
                                                GTK_STOCK_DIALOG_INFO},
  { N_("/Configure"),	NULL, show_config_win, 0,   "<StockItem>",
                                                GTK_STOCK_PREFERENCES},
  { N_("/Close"), 	NULL, close_window, 0,   "<StockItem>",
                                                GTK_STOCK_CLOSE},
  { "/sep", 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Quit"),	NULL, rox_main_quit, 0,   "<StockItem>",
                                                GTK_STOCK_QUIT},
};

static ROXSOAPServerActions actions[]={
  {"Open", NULL, "Parent",rpc_Open, NULL},
  {"Options", NULL, NULL,rpc_Options, NULL},

  {NULL}
};

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-onrvh] [XID]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -o\topen options dialog\n");
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
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
  printf("ROX-CLib version %s for GTK+ %s (compiled for %d.%d.%d)\n",
	 rox_clib_version_string(),
	 rox_clib_gtk_version_string(),
         ROX_CLIB_VERSION/10000, (ROX_CLIB_VERSION%10000)/100,
         ROX_CLIB_VERSION%100);

  printf("\nCompile time options:\n");
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
  printf("  Using XML version %d\n", LIBXML_VERSION);
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
  const char *options="vhnro";
  gboolean new_run=FALSE;
  gboolean replace_server=FALSE;
  gboolean show_options=FALSE;
  int ncpu;

  rox_init_with_domain("Load", "kerofin.demon.co.uk", &argc, &argv);

#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  app_dir=rox_get_app_dir();
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif

#ifdef HAVE_GETHOSTNAME
  if(gethostname(hostname, sizeof(hostname))<0) {
    hostname[0]=0;
    rox_debug_printf(1, "couldn't get hostname: %s", strerror(errno));
    errno=0;
  } else {
    rox_debug_printf(1, "Running on %s", hostname);
  }
#endif

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }

  ncpu=get_num_cpu();
  if(ncpu>0)
    red_line=(double) ncpu;
  rox_debug_printf(2, "ncpu=%d %f", ncpu, red_line);

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
    case 'n':
      new_run=TRUE;
      break;
    case 'r':
      replace_server=TRUE;
      break;
    case 'o':
      show_options=TRUE;
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

  rox_debug_printf(1, "setup config");
  setup_config();

  for(i=0; i<NUM_COLOUR; i++) {
    gdk_color_alloc(cmap, colours+i);
    rox_debug_printf(3, "colour %d: %4x, %4x, %4x: %ld", i, colours[i].red,
	   colours[i].green, colours[i].blue, colours[i].pixel);
  }

  data_update_tag=g_timeout_add(opt_update_rate.int_value*1000,
			       (GtkFunction) data_update, NULL);
  data_update(NULL);
  
  if(optind>=argc || !atol(argv[optind])) {
    xid=0;
  } else {
    rox_debug_printf(2, "argv[%d]=%s", optind, argv[optind]);
    xid=atol(argv[optind]);
  }

  if(replace_server || !rox_soap_ping(PROJECT)) {
    rox_debug_printf(1, "Making SOAP server");
    sserver=rox_soap_server_new(PROJECT, LOAD_NAMESPACE_URL);
    rox_soap_server_add_actions(sserver, actions);
    
  } else if(!new_run) {
    if(show_options && options_remote()) {
      if(!xid)
	return 0;
    }
    if(open_remote(xid)) {
      rox_debug_printf(1, "success in open_remote(%lu), exiting", xid);
      if(xid)
	sleep(3);
      return 0;
    }
  }
  lwin=make_window(xid);
  if(show_options)
    show_config_win();
  
  rox_main_loop();

  if(sserver)
    rox_soap_server_delete(sserver);

  return 0;
}

static void setup_config(void)
{
  rox_option_add_int(&opt_update_rate, "update_rate", 2);
  rox_option_add_int(&opt_show_max, "show_max", TRUE);
  rox_option_add_int(&opt_show_vals, "show_vals", FALSE);
  rox_option_add_int(&opt_show_host, "show_host", FALSE);
  rox_option_add_int(&opt_multiple, "multiple", TRUE);
  rox_option_add_int(&opt_applet_size, "applet_size", MIN_WIDTH);
  rox_option_add_string(&opt_col_norm1, "normal1", "#00cc00");
  rox_option_add_string(&opt_col_norm5, "normal5", "#00aa00");
  rox_option_add_string(&opt_col_norm15, "normal15", "#008800");
  rox_option_add_string(&opt_col_high1, "high1", "#ff0000");
  rox_option_add_string(&opt_col_high5, "high5", "#dd0000");
  rox_option_add_string(&opt_col_high15, "high15", "#bb0000");
  rox_option_add_string(&opt_col_fg, "foreground", "#000000");
  rox_option_add_string(&opt_col_bg, "background", "#d6d6d6");
  rox_option_add_string(&opt_font, "font", "Serif 6");

  rox_option_add_notify(opts_changed);
}

/* Work out number of CPU's here.  0 means we could not work it out and
 1 should be assumed */
static int get_num_cpu(void)
{
#ifdef _SC_NPROCESSORS_ONLN
  return (int) sysconf(_SC_NPROCESSORS_ONLN);
#else
  FILE *pstat;
  char buffer[BUFSIZ];
  int ncpu=0;

  pstat=fopen("/proc/stat", "r");
  if(pstat) {
    while(fgets(buffer, BUFSIZ, pstat)) {
      rox_debug_printf(4, "line=%s", buffer);
      if(strncmp(buffer, "cpu", 3)==0 && isdigit(buffer[3])) {
	int t=atoi(buffer+3);
	if(t>=ncpu)
	  ncpu=t+1;
      }
      rox_debug_printf(3, "ncpu=%d", ncpu);
    }

    fclose(pstat);
  }

  return ncpu;
#endif
}

static void remove_window(LoadWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);
  /*gtk_widget_hide(win->win);*/

  g_source_remove(win->update);
  gdk_pixmap_unref(win->pixmap);
  gdk_gc_unref(win->gc);
  gtk_widget_destroy(win->menu);
  g_free(win);

  rox_debug_printf(1, "rox_get_n_windows()=%d", rox_get_n_windows());
}

static void window_gone(GtkWidget *widget, gpointer data)
{
  LoadWindow *lw=(LoadWindow *) data;

  rox_debug_printf(1, "Window gone: %p %p", widget, lw);

  remove_window(lw);
}

static LoadWindow *make_window(guint32 xid)
{
  LoadWindow *lwin;
  GtkWidget *vbox;
  int w=MIN_WIDTH, h=MIN_HEIGHT;
  
  lwin=g_new0(LoadWindow, 1);

  lwin->hsize=0;

  w=opt_applet_size.int_value;
  h=w;
  if(!xid) {
    rox_debug_printf(2, "make window");
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
    
    rox_debug_printf(3, "set size to %d,%d", w, h);
    gtk_widget_set_size_request(lwin->win, w, h);

  } else {
    GtkWidget *plug;

    rox_debug_printf(2, "make plug");
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

  lwin->menu=rox_menu_build(lwin->win,
			    menu_items, sizeof(menu_items)/sizeof(*menu_items),
			    "<system>", "menus");
  if(lwin->is_applet)
    rox_menu_attach_to_applet(lwin->menu, lwin->win, NULL, lwin);
  else
    rox_menu_attach(lwin->menu, lwin->win, TRUE, NULL, lwin);  

  gtk_widget_show(lwin->win);

  lwin->update=g_timeout_add(opt_update_rate.int_value*1000,
			       (GtkFunction) window_update, lwin);
  rox_debug_printf(3, "update tag is %u", lwin->update);
  
  windows=g_list_append(windows, lwin);
  current_window=lwin;
  gtk_widget_ref(lwin->win);
  rox_add_window(lwin->win);

  return lwin;
}

/* Return history data for the given index, either in the rolling window,
   or negative where  -1 is last,
   -2 for second last etc.  Return NULL if index not valid
*/
static History *get_history(int ind)
{
  rox_debug_printf(4, "request for history %d (%d, %d)", ind, ihistory, nhistory);
  
  if(ind<0) {
    if(ind<-nhistory || ind <-ihistory)
      return NULL;

    rox_debug_printf(4, "index %d", (ihistory+ind)%nhistory);
    return history+((ihistory+ind)%nhistory);
    
  } else {
    if(ind>=ihistory || ind<ihistory-nhistory)
      return NULL;

    rox_debug_printf(4, "index %d", ind%nhistory);
    return history+ind%nhistory;
  }

  return NULL;
}

static void append_history(time_t when, const double loadavg[3])
{
  int i, j;

  rox_debug_printf(4, "append history for 0x%lx", when);

  if(history && nhistory>0) {
    if(history[ihistory%nhistory].when==when)
      return;
    i=(ihistory++)%nhistory;
    for(j=0; j<3; j++)
      history[i].load[j]=loadavg[j];
    history[i].when=when;
  }  
}

/* Updates the global load result and adds it to the history */
static gboolean data_update(gpointer unused)
{
  time_t now;
  
  if(getloadavg(load, 3)!=-1) {
    time(&now);
    append_history(now, load);
  }

  return TRUE;
}

static gboolean window_update(LoadWindow *lwin)
{
  int w, h;
  int bw, mbh, bh, bm=BOTTOM_MARGIN, tm=TOP_MARGIN, l1=BOTTOM_MARGIN, l2=0;
  int theight;
  int bx, by, x, y, sbx;
  double ld;
  int i, j;
  char buf[32];
  int reduce;
  int ndec=1, host_x=0;
  time_t now;
  History *hist;
  GtkStyle *style;
  PangoLayout *layout;
  PangoFontDescription *pfd;
  static const char *bar_labels[]={
    "1m", "5m", "15m"
  };

  rox_debug_printf(4, "gc=%p canvas=%p pixmap=%p", lwin->gc, lwin->canvas,
	  lwin->pixmap);

  if(!lwin->gc || !lwin->canvas || !lwin->pixmap)
    return TRUE;

  h=lwin->canvas->allocation.height;
  w=lwin->canvas->allocation.width;

  layout=gtk_widget_create_pango_layout(lwin->canvas, "");
  rox_debug_printf(3, "font is %s", opt_font.value? opt_font.value: "");
  pfd=pango_font_description_from_string(opt_font.value);
  pango_layout_set_font_description(layout, pfd);
  pango_font_description_free(pfd);
  
  style=gtk_widget_get_style(lwin->canvas);
  rox_debug_printf(3, "style=%p bg_gc[GTK_STATE_NORMAL]=%p", style,
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
  if(opt_show_vals.int_value) {
    int width;

    pango_layout_set_text(layout, "0", -1);
    pango_layout_get_pixel_size(layout, &width, &theight);
    rox_debug_printf(3, "%d,%d", width, theight);

    ndec=(bw/width)-2;
    if(ndec<1)
      ndec=1;
    if(ndec>3)
      ndec=3;

    tm=TEXT_MARGIN;
    if(h-2*(theight+TEXT_MARGIN)-tm>2*bm)
      bm=2*(theight+TEXT_MARGIN);
    else
      bm=theight+TEXT_MARGIN;
    l1=h-bm;
    l2=l1+theight+TEXT_MARGIN;
  }
  by=h-bm;
  if(by<48) 
    by=h-2;
  mbh=by-tm;
  if(mbh<48)
    mbh=by-2;
  rox_debug_printf(4, "w=%d bw=%d", w, bw);
  sbx=bx;

  time(&now);
  
  reduce=TRUE;
  for(i=0; i<3; i++) {
    if(load[i]>max_load-1)
      reduce=FALSE;
    if(max_load<load[i]) {
      max_load=((int) load[i])+1;
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
    ld=load[i];
    bh=mbh*ld/(double) max_load;
    gdk_gc_set_foreground(lwin->gc, colours+COL_NORMAL(i));
    gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh, bw, bh);
    
    rox_debug_printf(4, "load=%f max_load=%d, mbh=%d, by=%d, bh=%d, by-bh=%d",
	   ld, max_load, mbh, by, bh, by-bh);
    rox_debug_printf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bh);
    
    
    if(ld>red_line) {
      int bhred=mbh*(ld-red_line)/(double) max_load;
      int byred=mbh*red_line/(double) max_load;
      
      rox_debug_printf(5, "bhred=%d by-bhred=%d", bhred, by-bhred);
      rox_debug_printf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bhred);
      
      gdk_gc_set_foreground(lwin->gc, colours+COL_HIGH(i));
      /*gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, bw, bhred);*/
      gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			 bw, bh-byred);
    }
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_rectangle(lwin->pixmap, lwin->gc, FALSE, bx, by-bh, bw, bh);

    for(j=1; j<ld; j++) {
      y=mbh*j/(double) max_load;
      gdk_draw_line(lwin->pixmap, lwin->gc, bx, by-y, bx+bw, by-y);
    }

    if(opt_show_vals.int_value) {
      sprintf(buf, "%3.*f", ndec, ld);
      pango_layout_set_text(layout, buf, -1);
      gdk_draw_layout(lwin->pixmap, lwin->gc, bx+2, l1, layout);
      if(l2>0) {
	int xl=bx+2;
	int lw;

	pango_layout_set_text(layout, bar_labels[i], -1);
	pango_layout_get_pixel_size(layout, &lw, NULL);
	xl=bx+bw/2-lw/2;
	gdk_draw_layout(lwin->pixmap, lwin->gc, xl, l2, layout);
      }
    }

    bx+=bw+BAR_GAP;
  }

  if(history && ihistory>0) {
    History *prev;
    int gwidth=CHART_RES;
    int gorg=sbx-BAR_GAP;

    rox_debug_printf(2, "draw history");
    bx=sbx-BAR_GAP;
    for(i=-1; bx>BAR_GAP; i--) {
      hist=get_history(i);
      rox_debug_printf(3, "get_history(%d)->%p", i, hist);
      if(!hist)
	break;
      prev=get_history(i-1);
      if(prev) {
	rox_debug_printf(4, "tdiff %d, org=%d", (int)(now-prev->when), gorg);
	bx=gorg-CHART_RES*(int)(now-prev->when);
	gwidth=CHART_RES*(int)(hist->when-prev->when);

	rox_debug_printf(4, " 0x%lx: 0x%lx, 0x%lx -> %d, %d", now, hist->when,
		prev->when, bx, gwidth);
      } else {
	bx=gorg-CHART_RES*(int)(now-hist->when-1)-CHART_RES;
	gwidth=CHART_RES;
	rox_debug_printf(3, "get_history(%d)==NULL", i-1);
      }
      if(bx<BAR_GAP) {
	gwidth-=(BAR_GAP-bx);
	bx=BAR_GAP;
      }
      if(gwidth<1)
	continue;

      for(j=opt_multiple.int_value? 2: 0; j>=0; j--) {
	ld=hist->load[j];
	bh=mbh*ld/(double) max_load;
	gdk_gc_set_foreground(lwin->gc, colours+COL_NORMAL(j));
	rox_debug_printf(4, "(%d, %d) size (%d, %d)", bx, by-bh, gwidth, bh);
	gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			   gwidth, bh);
	if(ld>red_line) {
	  int byred=mbh*red_line/max_load;
	  gdk_gc_set_foreground(lwin->gc, colours+COL_HIGH(j));
	  /*gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, gwidth, bhred);*/
	  gdk_draw_rectangle(lwin->pixmap, lwin->gc, TRUE, bx, by-bh,
			     gwidth, bh-byred);
	}
      }
    }
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    for(j=1; j<max_load; j++) {
      y=mbh*j/(double) max_load;
      gdk_draw_line(lwin->pixmap, lwin->gc, sbx-BAR_GAP, by-y,
		    TEXT_MARGIN, by-y);
    }

    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_line(lwin->pixmap, lwin->gc, BAR_GAP, by, gorg, by);
    for(i=0, bx=gorg-2*i; bx>=BAR_GAP; i+=5, bx=gorg-CHART_RES*i)
      if(i%6==0) {
	gdk_draw_line(lwin->pixmap, lwin->gc, bx, by, bx, by+bm-theight);
	if(opt_show_vals.int_value) {
	  int lw;
	  sprintf(buf, "%d", i);
	  pango_layout_set_text(layout, buf, -1);
	  pango_layout_get_pixel_size(layout, &lw, NULL);
	  gdk_draw_layout(lwin->pixmap, lwin->gc,
			  bx-lw/2, l2, layout);
	}
      } else {
	gdk_draw_line(lwin->pixmap, lwin->gc, bx, by, bx, by+(bm-theight)/2);
      }
  }

  if(opt_show_max.int_value) {
    int lw;
    sprintf(buf, "Max %2d", max_load);
    pango_layout_set_text(layout, buf, -1);
    pango_layout_get_pixel_size(layout, &lw, NULL);
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_layout(lwin->pixmap, lwin->gc, 2, tm, layout);

    host_x=2+lw+4;
  }

  if(opt_show_host.int_value) {
    const char *host=hostname;
    int lw;
    
    pango_layout_set_text(layout, host, -1);
    pango_layout_get_pixel_size(layout, &lw, NULL);
    x=w-lw-2;
    if(x<host_x)
      x=host_x;
    gdk_gc_set_foreground(lwin->gc, colours+COL_FG);
    gdk_draw_layout(lwin->pixmap, lwin->gc, x, tm, layout);
  }
  
  gtk_widget_queue_draw(lwin->canvas);
  g_object_unref(G_OBJECT(layout));

  return TRUE;
}

static void resize_history(LoadWindow *lwin, int width)
{
  int nrec;
  History *tmp;
  int i;

  rox_debug_printf(3, "width=%d CHART_RES=%d", width, CHART_RES);
  nrec=(width-2-3*MAX_BAR_WIDTH-3*BAR_GAP-1)/CHART_RES;
  if(nrec<0)
    nrec=0;
  lwin->hsize=nrec;

  rox_debug_printf(3, "nrec=%d, nhistory=%d, ihistory=%d", nrec, nhistory, ihistory);
  if(nrec<=nhistory)
    return;

  rox_debug_printf(2, "expand history from %d to %d", nhistory, nrec);

  if(ihistory<nhistory) {
    tmp=g_realloc(history, nrec*sizeof(History));
    if(!tmp)
      return;

    history=tmp;
    nhistory=nrec;
    
    for(i=ihistory; i<nhistory; i++)
      history[i].when=(time_t) 0;

    rox_debug_printf(3, "reset list");
    
    return;
  }

  if(!nhistory) {
    history=g_new(History, nrec);
    nhistory=nrec;
    ihistory=0;
    rox_debug_printf(3, "made list");
    return;
  }

  tmp=g_new(History, nrec);
  i=nhistory-(ihistory%nhistory);
  if(i==nhistory)
    i=0;
  rox_debug_printf(3, "split list: %d,%d %d,%d", (ihistory%nhistory), i,
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
    return TRUE;
  
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

  rox_debug_printf(3, "configure_event(%p, %p, %p)", widget, event, data);
  rox_debug_printf(3, "pixmap=%p canvas=%p hsize=%d", lwin->pixmap, lwin->canvas,
	  lwin->hsize);
  
  if (lwin->pixmap)
    gdk_pixmap_unref(lwin->pixmap);

  lwin->pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height,
			  -1);
  resize_history(lwin, widget->allocation.width);
  rox_debug_printf(3, "pixmap=%p canvas=%p hsize=%d", lwin->pixmap, lwin->canvas,
	  lwin->hsize);
  window_update(lwin);

  return TRUE;
}

static void opts_changed(void)
{
  cmap=gdk_rgb_get_cmap();
  if(opt_col_norm1.has_changed) {
    gdk_color_parse(opt_col_norm1.value, colours+COL_NORMAL0);
    gdk_colormap_alloc_color(cmap, colours+COL_NORMAL0, FALSE, TRUE);
  }
  if(opt_col_norm5.has_changed) {
    gdk_color_parse(opt_col_norm5.value, colours+COL_NORMAL1);
    gdk_colormap_alloc_color(cmap, colours+COL_NORMAL1, FALSE, TRUE);
  }
  if(opt_col_norm15.has_changed) {
    gdk_color_parse(opt_col_norm15.value, colours+COL_NORMAL2);
    gdk_colormap_alloc_color(cmap, colours+COL_NORMAL2, FALSE, TRUE);
  }
  if(opt_col_high1.has_changed) {
    gdk_color_parse(opt_col_high1.value, colours+COL_HIGH0);
    gdk_colormap_alloc_color(cmap, colours+COL_HIGH0, FALSE, TRUE);
  }
  if(opt_col_high5.has_changed) {
    gdk_color_parse(opt_col_high5.value, colours+COL_HIGH1);
    gdk_colormap_alloc_color(cmap, colours+COL_HIGH1, FALSE, TRUE);
  }
  if(opt_col_high15.has_changed) {
    gdk_color_parse(opt_col_high15.value, colours+COL_HIGH2);
    gdk_colormap_alloc_color(cmap, colours+COL_HIGH2, FALSE, TRUE);
  }
  if(opt_col_fg.has_changed) {
    gdk_color_parse(opt_col_fg.value, colours+COL_FG);
    gdk_colormap_alloc_color(cmap, colours+COL_FG, FALSE, TRUE);
  }
  if(opt_col_bg.has_changed) {
    gdk_color_parse(opt_col_bg.value, colours+COL_BG);
    gdk_colormap_alloc_color(cmap, colours+COL_BG, FALSE, TRUE);
  }
  if(opt_update_rate.has_changed) {
    g_source_remove(data_update_tag);
    data_update_tag=g_timeout_add(opt_update_rate.int_value*1000,
				    (GtkFunction) data_update, NULL);
  }
  if(opt_applet_size.has_changed) {
    GList *p;

    for(p=windows; p; p=g_list_next(p)) {
      LoadWindow *lw=(LoadWindow *) p->data;
      if(lw && lw->is_applet) {
	gtk_widget_set_size_request(lw->win, opt_applet_size.int_value,
				    opt_applet_size.int_value);
      }
    }
  }
}

static void show_config_win(void)
{
  rox_options_show();
}

static void close_window(void)
{
  LoadWindow *lw=current_window;

  rox_debug_printf(1, "close_window %p %p", lw, lw->win);

  gtk_widget_hide(lw->win);
  gtk_widget_unref(lw->win);
  gtk_widget_destroy(lw->win);
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  LoadWindow *lwin=(LoadWindow *) win;

  current_window=lwin;

  rox_debug_printf(2, "Button press on %p %p", window, lwin);
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==1 && lwin->is_applet) {
      make_window(0);
      
      return TRUE;
    }
  }

  return FALSE;
}

/* SOAP stuff now */
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  xmlNodePtr parent;
  guint32 xid=0;

  parent=args->data;
  if(parent) {
    xmlChar *str;

    str=xmlNodeGetContent(parent);
    if(str) {
      xid=(guint32) atol((char *) str);
      xmlFree(str);
    }
  }

  make_window(xid);

  return NULL;
}

static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  show_config_win();

  return NULL;
}

static void open_callback(ROXSOAP *serv, gboolean status, 
				  xmlDocPtr reply, gpointer udata)
{
  gboolean *s=udata;
  
  rox_debug_printf(3, "In open_callback(%p, %d, %p, %p)", serv, status, reply,
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
  rox_debug_printf(3, "server for %s is %p", PROJECT, serv);
  if(!serv)
    return FALSE;

  doc=rox_soap_build_xml("Open", LOAD_NAMESPACE_URL, &node);
  if(!doc) {
    rox_debug_printf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sprintf(buf, "%lu", (unsigned long) xid);
  xmlNewChild(node, NULL, "Parent", buf);

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  rox_debug_printf(3, "sent %d", sent);

  xmlFreeDoc(doc);
  if(sent)
    gtk_main();
  rox_soap_close(serv);

  return sent && ok;
}

static gboolean options_remote(void)
{
  ROXSOAP *serv;
  xmlDocPtr doc;
  xmlNodePtr node;
  gboolean sent, ok;

  serv=rox_soap_connect(PROJECT);
  rox_debug_printf(3, "server for %s is %p", PROJECT, serv);
  if(!serv)
    return FALSE;

  doc=rox_soap_build_xml("Options", LOAD_NAMESPACE_URL, &node);
  if(!doc) {
    rox_debug_printf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  rox_debug_printf(3, "sent %d", sent);

  xmlFreeDoc(doc);
  if(sent)
    gtk_main();
  rox_soap_close(serv);

  return sent && ok;
}

static void show_info_win(void)
{
  GtkWidget *infowin;
  
  infowin=rox_info_win_new_from_appinfo(PROGRAM);
  rox_add_window(infowin);

  gtk_window_set_position(GTK_WINDOW(infowin), GTK_WIN_POS_MOUSE);
  gtk_widget_show(infowin);
}

/*
 * $Log: load.c,v $
 * Revision 1.28  2006/03/07 19:23:51  stephen
 * Added i18n support (with ROX-CLib 2.1.8)
 *
 * Revision 1.27  2005/10/16 11:59:50  stephen
 * Update for ROX-CLib changes, many externally visible symbols
 * (functions and types) now have rox_ or ROX prefixes.
 * Can get ROX-CLib via 0launch.
 *
 * Revision 1.26  2005/05/27 10:21:11  stephen
 * Fix for creating applets in remote mode, need to give the filer long enough
 * to notice the widget was created.
 * Use apsymbols for Linux portability.
 *
 * Revision 1.25  2004/11/21 13:21:56  stephen
 * Use new ROX-CLib features
 *
 * Revision 1.24  2004/08/19 19:27:44  stephen
 * Draw lines on the graph to indicate the levels.
 *
 * Revision 1.23  2004/08/18 17:24:55  stephen
 * Eliminate use of libgtop
 *
 * Revision 1.22  2004/04/10 12:11:42  stephen
 * Remove dead code.  Open options dialog from command line or SOAP message.  Stock items in menus.
 *
 * Revision 1.21  2004/02/14 13:45:19  stephen
 * Fix debug line
 *
 * Revision 1.20  2003/06/27 16:27:58  stephen
 * Fixed text positioning.  Fix bug in displaying hostname.  Work on memory leak. New icon provided by Geoff Youngs.
 *
 * Revision 1.19  2003/04/16 09:11:15  stephen
 * Added options system from ROX-CLib
 *
 * Revision 1.18  2003/03/05 15:30:40  stephen
 * First pass at conversion to GTK 2.
 *
 * Revision 1.17  2002/10/19 14:33:36  stephen
 * Fixed missing return statement when creating window.
 *
 * Revision 1.16  2002/08/24 16:45:04  stephen
 * Fix compilation problem with libxml2.
 *
 * Revision 1.15  2002/04/29 08:37:35  stephen
 * Use replacement applet menu positioning code (from ROX-CLib).
 * Fix problem setting options for a window.
 *
 * Revision 1.14  2002/04/12 10:24:30  stephen
 * Moved colour allocation to after read_choices().  Fixed bug in strip chart
 * if running multiple windows.
 *
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
