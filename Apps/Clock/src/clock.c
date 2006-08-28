/*
 * Clock - a clock for ROX which can run as an applet.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: clock.c,v 1.41 2006/08/12 17:12:54 stephen Exp $
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

#ifdef HAVE_UNISTD_H
/* Sometimes getopt is in stdlib.h, sometimes in here.  And sometimes both
   and stdio.h */
#include <unistd.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>

#include <libxml/xmlversion.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include <rox/rox.h>
#include <rox/rox_debug.h>
#include <rox/applet.h>
#include <rox/rox_soap.h>
#include <rox/rox_soap_server.h>
#include <rox/options.h>

#include "alarm.h"

typedef struct time_format {
  const char *name;   /* User visible name for this format (unused?) */
  const char *fmt;    /* Format to pass to strftime(3c) */
  guint32 interval;   /* Suitable interval rate (unused?) */
} TimeFormat;

static char user_defined[256]="%X%n%x"; /* Buffer to hold the user format */
  
static TimeFormat formats[]={
  {N_("24 hour"), "%k:%M.%S", 500},
  {N_("24 hour, no seconds"), "%k:%M", 30000},
  {N_("12 hour"), "%l:%M.%S", 500},
  {N_("12 hour, no seconds"), "%l:%M", 30000},
  {N_("Date"), "%x", 30000},
  {N_("Full"), "%c", 500},
  {N_("User defined"), user_defined, 500},

  {NULL, NULL, 0}
};

#define NUM_FORMAT 7
#define USER_FORMAT 6

/* Flags for display mode */
#define MODE_SECONDS 1  /* Plot the second hand */
#define MODE_NO_TEXT 2  /* Don't show the text below the clock face */
#define MODE_HOURS   4  /* Show the hours on the clock face */
#define MODE_NFLAGS 3

typedef struct mode {
  int format;  /* Format of text below clock */
  int seconds, show_text, hours;
  /*int flags;  */        /* Bit mask of flags defined above */
  guint32 interval;   /* Interval between updates (in ms) */
  guint init_size;    /* Initial size of applet */
  const char *font_name;   /* Font for drawing clock numbers */
} Mode;

static Mode default_mode={
  0, 1, 1, 0, 500, 32, "Serif 12"
};

static GdkColormap *cmap = NULL;
static GdkColor colours[]={
  /* If you change these, change the enums below */
  {0, 0xffff,0xffff,0xffff},
  {0, 0xffff,0,0},
  {0, 0, 0x8000, 0},
  {0, 0,0,0xffff},
  {0, 0xf4f4, 0x8d8d, 0x0e0e},
  {0, 0x8888, 0x8888, 0x8888}, /* This gets replaced with the gtk background*/
  {0, 0, 0, 0}
};
enum {WHITE, RED, GREEN, BLUE, ORANGE, GREY, BLACK};
#define NUM_COLOUR (BLACK+1)
#define CL_FACE_BG      (colours+WHITE)
#define CL_FACE_FG      (colours+BLUE)
#define CL_FACE_FG_ALRM (colours+ORANGE)
#define CL_HOUR_TEXT    (colours+BLACK)
#define CL_HOUR_HAND    (colours+BLACK)
#define CL_MINUTE_HAND  (colours+BLACK)
#define CL_SECOND_HAND  (colours+RED)
#define CL_BACKGROUND   (colours+GREY)

#define CLOCK_SIZE 256
static GdkPixbuf *clock_face=NULL;

static ROXOption o_format;
static ROXOption o_user_format;
static ROXOption o_seconds;
static ROXOption o_show_text;
static ROXOption o_hours;
static ROXOption o_interval;
static ROXOption o_size;
static ROXOption o_font;
static ROXOption o_no_face;

static gboolean load_alarms=TRUE;   /* Also controls if we show them */
static gboolean save_alarms=TRUE;

typedef struct clock_window {
  GtkWidget *win;
  gboolean applet_mode;
  GtkWidget *digital_out; /* Text below clock face */
  GtkWidget *canvas;      /* Displays clock face */
  guint update_tag;            /* Handle for the timeout */
  GdkGC *gc;
  GdkPixbuf *clock_face;
  GtkWidget *menu;        /* Popup menu */
} ClockWindow;

static GList *windows=NULL;

static ClockWindow *current_window=NULL;

static ROXSOAPServer *server=NULL;
#define CLOCK_NAMESPACE_URL WEBSITE PROJECT

static ClockWindow *make_window(guint32 socket);
static void remove_window(ClockWindow *win);
static const char *get_format(void);
static gboolean do_update(ClockWindow *win);/* Update clock face and text */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event,
			    gpointer data);
                                    /* Window resized */
static gint expose_event(GtkWidget *widget, GdkEventExpose *event,
			    gpointer data);
                                    /* Window needs redrawing */
static void check_clock_face(ClockWindow *cwin);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on canvas */
static gboolean popup_menu(GtkWidget *window, gpointer udata);
static void show_info_win(void);
static void show_conf_win(void);
static void close_window(void);
static gboolean check_alarms(gpointer data);
static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid);
static gboolean options_remote(void);

static void setup_options(void);
static void read_config(void);
static gboolean read_config_xml(void);
static void setup_config(void);

static ROXSOAPServerActions actions[]={
  {"Open", NULL, "Parent", rpc_Open, NULL},
  {"Options", NULL, NULL, rpc_Options, NULL},

  {NULL}
};

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, "<StockItem>",
                                GTK_STOCK_DIALOG_INFO},
  { N_("/Configure..."),       	NULL, show_conf_win, 0, "<StockItem>",
                                GTK_STOCK_PROPERTIES},
  { N_("/Alarms..."),		NULL, alarm_show_window, 0, NULL },
  { N_("/Close"), 	        NULL, close_window, 0, "<StockItem>",
                                GTK_STOCK_CLOSE},
  { N_("/sep"), 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Quit"), 	        NULL, rox_main_quit, 0, "<StockItem>",
                                GTK_STOCK_QUIT},
};

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-onrvh] [XID]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -o\tshow options dialog\n");
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  XID\tthe X id of the window to use for applet mode\n");
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
  printf("  Using XML... yes (libxml version %d)\n", LIBXML_VERSION);
  printf("  Using SOAP server... yes\n");
}

int main(int argc, char *argv[])
{
  ClockWindow *cwin;
  int i, c, do_exit, nerr;
  const gchar *app_dir;
  gchar *aicon;
  guint32 xid;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  const char *options="vhnro";
  gboolean new_run=FALSE;
  gboolean replace_server=FALSE;
  gboolean show_options=FALSE;

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  app_dir=rox_get_app_dir();

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  rox_init_with_domain("Clock", AUTHOR_DOMAIN, &argc, &argv);
  dprintf(1, "ROX system inited");

  
  /* Initialise X/GDK/GTK */
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  gtk_widget_push_colormap(gdk_rgb_get_cmap());
  
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

  setup_options();
  if(load_alarms)
    alarm_load();

  dprintf(4, "argc=%d", argc);
  xid=(!argv[optind] || !atol(argv[optind]))? 0: atol(argv[optind]);
  
  if(replace_server || !rox_soap_ping(PROJECT)) {
    dprintf(1, "Making SOAP server");
    server=rox_soap_server_new(PROJECT, CLOCK_NAMESPACE_URL);
    rox_soap_server_add_actions(server, actions);

  } else if(!new_run) {
    if(show_options && options_remote()) {
      if(!xid)
	return 0;
    }
    if(open_remote(xid)) {
      dprintf(1, "success in open_remote(%lu), exiting", xid);
      if(xid)
	sleep(3);
      return 0;
    }
  }

  aicon=rox_resources_find_with_domain(PROJECT, "alarm.png",
				       ROX_RESOURCES_NO_LANG, AUTHOR_DOMAIN);
  if(aicon) {
    GError *err=NULL;
    
    alarm_icon=gdk_pixbuf_new_from_file(aicon, &err);

    g_free(aicon);
  }

  if(!show_options) {
    cwin=make_window(xid);

    gtk_widget_show(cwin->win);

    g_timeout_add(10*1000, check_alarms, NULL);
  } else {
    show_conf_win();
  }
  
  dprintf(2, "into main, %d windows.", rox_get_n_windows());
  rox_main_loop();
  dprintf(2, "out of main, %d windows.", rox_get_n_windows());

  if(save_alarms)
    alarm_save();

  if(server)
    rox_soap_server_delete(server);

  return 0;
}

static gboolean check_alarms(gpointer data)
{
  if(load_alarms) {
    if(alarm_check())
      if(save_alarms)
	alarm_save();
  }

  return TRUE;
}

static void remove_window(ClockWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);

  g_source_remove(win->update_tag);
  gdk_gc_unref(win->gc);
  g_free(win);

  rox_debug_printf(1, "rox_get_n_windows()=%d", rox_get_n_windows());
}

static void window_gone(GtkWidget *widget, gpointer data)
{
  ClockWindow *cw=(ClockWindow *) data;

  dprintf(1, "Window gone: %p %p", widget, cw);

  remove_window(cw);
}

static ClockWindow *make_window(guint32 socket)
{
  ClockWindow *cwin;
  GtkWidget *vbox;
  GtkStyle *style;
  time_t now;
  char buf[80];
  int i;

  cwin=g_new0(ClockWindow, 1);
  cwin->clock_face=NULL;

  cmap=gdk_rgb_get_cmap();
  
  if(socket) {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;

    plug=gtk_plug_new(socket);
    if(!plug) {
      rox_error(_("%s: failed to plug into socket %ld, not a XID?\n"),
	      "Clock", socket);
      exit(1);
    }
    cwin->applet_mode=TRUE;
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       cwin);
    dprintf(3, "initial size=%d", o_size.value);
    gtk_widget_set_usize(plug, o_size.int_value, o_size.int_value);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), cwin);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);

    
    /* Isn't object oriented code wonderful.  Now that we have done the
       plug specific code, its just a containing widget.  For the rest of the
       program we just pretend it was a window and it all works
    */
    cwin->win=plug;
  } else {
    /*  We are not an applet, so create a window */
    
    cwin->applet_mode=FALSE;
    cwin->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(cwin->win, "clock");
    gtk_window_set_title(GTK_WINDOW(cwin->win), _("Clock"));
    gtk_signal_connect(GTK_OBJECT(cwin->win), "destroy", 
		       GTK_SIGNAL_FUNC(window_gone), 
		       cwin);
    dprintf(3, "initial size=%d", o_size.value);
    gtk_widget_set_usize(cwin->win, o_size.int_value,
			 o_size.int_value);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(cwin->win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), cwin);
    gtk_widget_add_events(cwin->win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_realize(cwin->win);

  }

  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(cwin->win), vbox);
  gtk_widget_show(vbox);

  /* Get the right grey for the background */
  style=gtk_widget_get_style(vbox);
  colours[GREY]=style->bg[GTK_STATE_NORMAL];

  for(i=0; i<NUM_COLOUR; i++)
    gdk_color_alloc(cmap, colours+i);

  cwin->canvas=gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(cwin->canvas), 64, 48);
  /* next line causes trouble??  maybe just as an applet... */
  gtk_widget_set_events (cwin->canvas, GDK_EXPOSURE_MASK);
  gtk_box_pack_start (GTK_BOX (vbox), cwin->canvas, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (cwin->canvas), "expose_event",
		      G_CALLBACK(expose_event), cwin);
  g_signal_connect (G_OBJECT (cwin->canvas), "configure_event",
		      G_CALLBACK(configure_event), cwin);
  gtk_widget_realize(cwin->canvas);
  if(!o_no_face.int_value)
    gtk_widget_show (cwin->canvas);
  gtk_widget_set_name(cwin->canvas, "clock face");
  
  time(&now);
  strftime(buf, 80, get_format(), localtime(&now));

  /* This is the text below the clock face */
  cwin->digital_out=gtk_label_new(buf);
  gtk_widget_set_name(cwin->digital_out, "clock digital");
  gtk_box_pack_start(GTK_BOX(vbox), cwin->digital_out, FALSE, FALSE, 2);
  if(o_show_text.int_value)
    gtk_widget_show(cwin->digital_out);

  cwin->menu=rox_menu_build(cwin->win,
			    menu_items, sizeof(menu_items)/sizeof(*menu_items),
			    "<system>", "menus");
  if(cwin->applet_mode)
    rox_menu_attach_to_applet(cwin->menu, cwin->win, NULL, cwin);
  else
    rox_menu_attach(cwin->menu, cwin->win, TRUE, NULL, cwin);

  if(cwin->win)
    gtk_widget_show(cwin->win);

  /* Make sure we get called periodically */
  cwin->update_tag=g_timeout_add(o_interval.int_value,
			 (GtkFunction) do_update, cwin);

  windows=g_list_append(windows, cwin);
  current_window=cwin;
  gtk_widget_ref(cwin->win);

  rox_add_window(cwin->win);
  rox_debug_printf(2, "%d windows", rox_get_n_windows());

  return cwin;
}

static const char *get_format(void)
{
  int i=o_format.int_value;

  if(i<0 || i>=NUM_FORMAT)
    i=0;

  if(i==USER_FORMAT)
    return o_user_format.value;

  return formats[i].fmt;
}

static void opts_changed(void)
{
  if(o_show_text.has_changed && current_window)
    if(o_show_text.int_value) {
      gtk_widget_show(current_window->digital_out);
    } else {
      gtk_widget_hide(current_window->digital_out);
    }   
   
  if(o_size.has_changed && current_window) {
    gtk_widget_set_usize(current_window->win,
			 o_size.int_value, o_size.int_value);
  }

  if(o_interval.has_changed) {
    if(current_window) {
      g_source_remove(current_window->update_tag);
      current_window->update_tag=g_timeout_add(o_interval.int_value,
			       (GtkFunction) do_update, current_window);
    }
  }

  if(o_no_face.has_changed && current_window) {
    if(!o_no_face.int_value) {
      gtk_widget_show(current_window->canvas);
    } else {
      gtk_widget_hide(current_window->canvas);
    }   
  }

  if(o_font.has_changed) {
    GList *l;

    dprintf(1, "font changed, now %s\n", o_font.value);

    for(l=windows; l; l=g_list_next(l)) {
      ClockWindow *c=(ClockWindow *) l->data;
      dprintf(1, " %p %p\n", c, c->clock_face);
      if(c->clock_face) {
	gdk_pixbuf_unref(c->clock_face);
	c->clock_face=NULL;
      }
    }
    dprintf(1, " %p\n", clock_face);
    if(clock_face) {
      gdk_pixbuf_unref(clock_face);
      clock_face=NULL;
    }
  }
}

static void setup_options(void)
{
  read_config();
  
  rox_option_add_int(&o_format, "format", default_mode.format);
  rox_option_add_string(&o_user_format, "user_format", user_defined);
  rox_option_add_int(&o_seconds, "seconds", default_mode.seconds);
  rox_option_add_int(&o_show_text, "text", default_mode.show_text);
  rox_option_add_int(&o_hours, "hours", default_mode.hours);
  rox_option_add_int(&o_interval, "interval", default_mode.interval);
  rox_option_add_int(&o_size, "size", default_mode.init_size);
  rox_option_add_string(&o_font, "font", default_mode.font_name);
  rox_option_add_int(&o_no_face, "no_face", 0);

  rox_option_add_notify(opts_changed);
}

enum draw_hours {
  DH_NO, DH_QUARTER, DH_ALL
};

static GdkPixbuf *make_clock_face(int w, int h, GtkWidget *widget,
				  GdkGC *gc)
{
  GdkPixbuf *face;
  int sw, sh, rad;
  int x0, y0, x1, y1;
  int tick, twidth;
  enum draw_hours th;
  GdkColor *face_fg=CL_FACE_FG;
  GdkFont *font=NULL;
  GtkStyle *style;
  PangoLayout *layout;
  PangoFontDescription *pfd;
  GdkPixmap *pmap;

  pmap=gdk_pixmap_new(widget->window, w, h, -1);

  style=gtk_widget_get_style(widget);
  dprintf(3, "style=%p bg_gc[GTK_STATE_NORMAL]=%p", style,
	  style->bg_gc[GTK_STATE_NORMAL]);
  if(style && style->bg_gc[GTK_STATE_NORMAL]) {
    gtk_style_apply_default_background(style, pmap, TRUE,
				       GTK_STATE_NORMAL,
				       NULL, 0, 0, w, h);
  } else {
    /* Blank out to the background colour */
    gdk_gc_set_foreground(gc, CL_BACKGROUND);
    gdk_draw_rectangle(pmap, gc, TRUE, 0, 0, w, h);
  }

  /* we want a diameter that can fit in our canvas */
  if(h<w)
    sw=sh=h;
  else
    sw=sh=w;
  if(!(sw%2)) {
    sw--;
    sh--;
  }

  rad=sw/2;

  x0=w/2;
  y0=h/2;

  layout=gtk_widget_create_pango_layout(widget, "");
  dprintf(3, "font is %s", o_font.value? o_font.value: "");
  pfd=pango_font_description_from_string(o_font.value);
  pango_layout_set_font_description(layout, pfd);
  
  /* Draw the clock face, including the hours */
  gdk_gc_set_foreground(gc, face_fg);
  gdk_draw_arc(pmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0,
	       360*64);
  sw-=12;
  sh-=12;
  rad=sw/2;
  gdk_gc_set_foreground(gc, CL_FACE_BG);
  gdk_draw_arc(pmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0,
	       360*64);
  if(o_hours.int_value) {
    int siz;
    
    pango_layout_set_text(layout, "12", -1);
    pango_layout_get_pixel_size(layout, &twidth, NULL);
    siz=sw/twidth;

    if(siz<4)
      th=DH_NO;
    else if(siz<10)
      th=DH_QUARTER;
    else {
      gint psize;
      
      th=DH_ALL;
      psize=pango_font_description_get_size(pfd);
      psize*=siz/10.;
      pango_font_description_set_size(pfd, psize);
      pango_layout_set_font_description(layout, pfd);
    }
  } else {
    th=0;
  }
  pango_font_description_free(pfd);
  
  for(tick=0; tick<12; tick++) {
    double ang=tick*2*M_PI/12.;
    double x2=x0+rad*sin(ang)*7/8;
    double y2=y0-rad*cos(ang)*7/8;
    x1=x0+rad*sin(ang);
    y1=y0-rad*cos(ang);
    gdk_gc_set_foreground(gc, face_fg);
    gdk_draw_line(pmap, gc, x2, y2, x1, y1);

    if(th!=DH_NO && (th==DH_ALL || tick%3==0)) {
      char buf[16];
      int l;
      int xt, yt;
      
      gdk_gc_set_foreground(gc, CL_HOUR_TEXT);
      sprintf(buf, "%d", tick==0? 12: tick);
      l=strlen(buf);
      pango_layout_set_text(layout, buf, -1);
      pango_layout_get_pixel_size(layout, &xt, &yt);
      xt=x2-xt/2;
      yt=y2-yt/2;
      gdk_draw_layout(pmap, gc, xt, yt, layout);
    }
  }

  face=gdk_pixbuf_get_from_drawable(NULL, pmap, NULL, 0, 0, 0, 0, w, h);
  gdk_pixmap_unref(pmap);

  g_object_unref(G_OBJECT(layout));

  return face;
}

/* Redraw clock face and update digital_out */
static gboolean do_update(ClockWindow *cwin)
{
  time_t now;
  char buf[80];
  struct tm *tms;
  int w, h;
  int sw, sh, rad;
  int s_since_12;
  double h_ang, m_ang, s_ang;
  int hour, min, sec;
  int x0, y0, x1, y1;
  int tick, twidth;
  enum draw_hours th;
  GdkFont *font=NULL;
  PangoLayout *layout;
  PangoFontDescription *pfd;

  time(&now);
  tms=localtime(&now);
    
  if(cwin->digital_out && o_show_text.int_value) {
    strftime(buf, 80, get_format(), tms);
    
    gtk_label_set_text(GTK_LABEL(cwin->digital_out), buf);
  }

  if(o_no_face.int_value)
    return;

  if(!cwin->gc || !cwin->canvas)
    return TRUE;
  
  h=cwin->canvas->allocation.height;
  w=cwin->canvas->allocation.width;

  if(!cwin->clock_face) {
    check_clock_face(cwin);
  }

  /* we want a diameter that can fit in our canvas */
  if(h<w)
    sw=sh=h;
  else
    sw=sh=w;
  if(!(sw%2)) {
    sw--;
    sh--;
  }

  rad=sw/2;

  /* Work out the angle of the hands */
  s_since_12=(tms->tm_hour%12)*3600+tms->tm_min*60+tms->tm_sec;
  sec=s_since_12%60;
  min=(s_since_12/60)%60;
  hour=tms->tm_hour%12;
  s_ang=sec*2*M_PI/60.;
  m_ang=min*2*M_PI/60.;
  h_ang=(hour+min/60.)*2*M_PI/12.;

  x0=w/2;
  y0=h/2;

  /* Draw the clock face*/
  gdk_pixbuf_render_to_drawable(cwin->clock_face,
				cwin->canvas->window, cwin->gc,
				0, 0, x0-rad-1, y0-rad-1, -1, -1,
				GDK_RGB_DITHER_NONE,
				0, 0);
  
  if(alarm_have_active()) {
    if(alarm_icon) {
      gint iwid=gdk_pixbuf_get_width(alarm_icon);
      gint ihei=gdk_pixbuf_get_height(alarm_icon);
      GdkPixbuf *icon;

      if(iwid>w/4) {
	iwid=w/4;
	if(iwid<4)
	  iwid=4;
	ihei=iwid;

	icon=gdk_pixbuf_scale_simple(alarm_icon, iwid, ihei,
				     GDK_INTERP_BILINEAR);
      } else {
	icon=alarm_icon;
	g_object_ref(icon);
      }
      if(icon) {
	gdk_pixbuf_render_to_drawable(icon,
				      cwin->canvas->window, cwin->gc,
				      0, 0, w-iwid, h-ihei, -1, -1,
				      GDK_RGB_DITHER_NONE,
				      0, 0);
	g_object_unref(icon);
      }
    }
  }

  sw-=4;
  sh-=4;
  rad=sw/2;

  /* Draw the hands */
  if(rad>100) {
    GdkPoint points[4];
    
    points[0].x=(gint16)(x0+rad*sin(h_ang)*0.5);
    points[0].y=(gint16)(y0-rad*cos(h_ang)*0.5);
    points[1].x=(gint16)(x0+rad*sin(h_ang+M_PI/2)*0.02);
    points[1].y=(gint16)(y0-rad*cos(h_ang+M_PI/2)*0.02);
    points[2].x=(gint16)(x0+rad*sin(h_ang-M_PI)*0.02);
    points[2].y=(gint16)(y0-rad*cos(h_ang-M_PI)*0.02);
    points[3].x=(gint16)(x0+rad*sin(h_ang-M_PI/2)*0.02);
    points[3].y=(gint16)(y0-rad*cos(h_ang-M_PI/2)*0.02);
    gdk_gc_set_foreground(cwin->gc, CL_HOUR_HAND);
    gdk_draw_polygon(cwin->canvas->window, cwin->gc, TRUE, points, 4);
    
  } else {
    x1=x0+rad*sin(h_ang)*0.5;
    y1=y0-rad*cos(h_ang)*0.5;
    gdk_gc_set_foreground(cwin->gc, CL_HOUR_HAND);
    gdk_gc_set_line_attributes(cwin->gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND,
			       GDK_JOIN_MITER);
    gdk_draw_line(cwin->canvas->window, cwin->gc, x0, y0, x1, y1);
    
  }
  
  x1=x0+rad*sin(m_ang)*0.95;
  y1=y0-rad*cos(m_ang)*0.95;
  gdk_gc_set_foreground(cwin->gc, CL_MINUTE_HAND);
  gdk_gc_set_line_attributes(cwin->gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND,
			     GDK_JOIN_MITER);
  gdk_draw_line(cwin->canvas->window, cwin->gc, x0, y0, x1, y1);
  
  gdk_gc_set_line_attributes(cwin->gc, 0, GDK_LINE_SOLID, GDK_CAP_ROUND,
			     GDK_JOIN_MITER);
  if(o_seconds.int_value) {
    int x2, y2;
    
    x1=x0+rad*sin(s_ang)*0.95;
    y1=y0-rad*cos(s_ang)*0.95;
    x2=x0+rad*sin(s_ang+M_PI)*0.05;
    y2=y0-rad*cos(s_ang+M_PI)*0.05;
    gdk_gc_set_foreground(cwin->gc, CL_SECOND_HAND);
    gdk_draw_line(cwin->canvas->window, cwin->gc, x2, y2, x1, y1);
  }
  gdk_gc_set_foreground(cwin->gc, CL_HOUR_HAND);
  rad=sw/10;
  if(rad>8)
    rad=8;
  gdk_draw_arc(cwin->canvas->window, cwin->gc, TRUE,
	       x0-rad/2, y0-rad/2, rad, rad, 0, 360*64);


  return TRUE;
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event,
			 gpointer data)
{
  int w, h;
  ClockWindow *cwin=(ClockWindow *) data;

  do_update(cwin);

  return TRUE;
}

static void check_clock_face(ClockWindow *cwin)
{
  int w, h;
  
  if(!cwin->gc)
    cwin->gc=gdk_gc_new(cwin->canvas->window);

  if(!clock_face)
    clock_face=make_clock_face(CLOCK_SIZE, CLOCK_SIZE, cwin->canvas, cwin->gc);

  if(cwin->clock_face)
    gdk_pixbuf_unref(cwin->clock_face);
  w=cwin->canvas->allocation.width;
  h=cwin->canvas->allocation.height;
  if(w>h)
    w=h;
  else if(h>w)
    h=w;
  if(w<4)
    w=h=4;

  if(w>CLOCK_SIZE) {
    cwin->clock_face=make_clock_face(w, h, cwin->canvas, cwin->gc);
  } else if(w>4) {
    cwin->clock_face=gdk_pixbuf_scale_simple(clock_face, w, h,
					     GDK_INTERP_BILINEAR);
  } else {
    cwin->clock_face=gdk_pixbuf_scale_simple(clock_face, w, h,
					     GDK_INTERP_NEAREST);
  }
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event,
			 gpointer data)
{
  ClockWindow *cwin=(ClockWindow *) data;
  int w, h;
  
  /*do_update(cwin);*/
  dprintf(1, "configure_event");

  if(!cwin->canvas || !cwin->canvas->window)
    return TRUE;

  check_clock_face(cwin);

  return TRUE;
}

static gboolean read_config_xml(void)
{
  gchar *fname;

  fname=rox_choices_load("options.xml", PROJECT, AUTHOR_DOMAIN);

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

    if(strcmp((char *) root->name, "Clock")!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *str;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      if(strcmp((char *) node->name, "format")==0) {
	int i;
	
	str=xmlGetProp(node, (xmlChar *) "name");
	if(!str)
	  continue;

	for(i=0; formats[i].name; i++)
	  if(strcmp((char *) str, formats[i].name)==0)
	    break;
	if(formats[i].name)
	  default_mode.format=i;
	free(str);
	
      } else if(strcmp((char *) node->name, "flags")==0) {
	int flags;
 	str=xmlGetProp(node, (xmlChar *) "value");
	if(!str)
	  continue;
	flags=atoi((char *) str);
	free(str);

	default_mode.seconds=!!(flags & MODE_SECONDS);
	default_mode.show_text=!(flags & MODE_NO_TEXT);
	default_mode.hours=!!(flags & MODE_HOURS);

      } else if(strcmp((char *) node->name, "interval")==0) {
 	str=xmlGetProp(node, (xmlChar *) "value");
	if(!str)
	  continue;
	default_mode.interval=atol((char *) str);
	free(str);

      } else if(strcmp((char *) node->name, "user-format")==0) {
	str=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!str)
	  continue;
	strncpy(user_defined, (char *) str, sizeof(user_defined));
	free(str);

      } else if(strcmp((char *) node->name, "font")==0) {
	str=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!str)
	  continue;
	default_mode.font_name=g_strdup((char *) str);
	free(str);

      } else if(strcmp((char *) node->name, "applet")==0) {
 	str=xmlGetProp(node, (xmlChar *) "initial-size");
	if(!str)
	  continue;
	default_mode.init_size=atoi((char *) str);
	free(str);

      }
    }
    
    if(default_mode.init_size<16)
      default_mode.init_size=16;
    
    g_free(fname);
    xmlFreeDoc(doc);
    return TRUE;
  }

  return FALSE;
}

/* Read in the config */
static void read_config(void)
{
  read_config_xml();
}

/* Show the configure window */
static void show_conf_win(void)
{
  rox_options_show();
}

static void close_window(void)
{
  ClockWindow *cw=current_window;

  dprintf(1, "close_window %p %p", cw, cw->win);

  gtk_widget_hide(cw->win);
  gtk_widget_unref(cw->win);
  gtk_widget_destroy(cw->win);

  current_window=NULL;
}

/* Button press in canvas */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  ClockWindow *cwin=(ClockWindow *) win;

  current_window=cwin;
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==1 && cwin->applet_mode) {
      ClockWindow *nwin=make_window(0);

      gtk_widget_show(nwin->win);

      return TRUE;
    }
  }

  return FALSE;
}

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
      g_free(str);
    }
  }

  make_window(xid);

  return NULL;
}

static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  show_conf_win();

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

  doc=rox_soap_build_xml("Open", CLOCK_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sprintf(buf, "%lu", xid);
  xmlNewChild(node, NULL, (xmlChar *) "Parent", (xmlChar *) buf);

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  dprintf(3, "sent %d", sent);

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
  char buf[32];

  serv=rox_soap_connect(PROJECT);
  dprintf(3, "server for %s is %p", PROJECT, serv);
  if(!serv)
    return FALSE;

  doc=rox_soap_build_xml("Options", CLOCK_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  dprintf(3, "sent %d", sent);

  xmlFreeDoc(doc);
  if(sent)
    gtk_main();
  rox_soap_close(serv);

  return sent && ok;
}

/* Show the info window */
static void show_info_win(void)
{
  GtkWidget *infowin=rox_info_win_new_from_appinfo(PROJECT);

  rox_add_window(infowin);
  gtk_widget_show(infowin);
}

/*
 * $Log: clock.c,v $
 * Revision 1.41  2006/08/12 17:12:54  stephen
 * Use new menu API
 *
 * Revision 1.40  2006/03/18 14:50:05  stephen
 * Cast strings passed to/from libxml to avoid compiler warnings.
 * New release.
 *
 * Revision 1.39  2006/01/17 11:43:24  stephen
 * Fix problem with drawing alarm symbol far too small when panel initializing.
 *
 * Revision 1.38  2005/12/30 16:46:21  stephen
 * Add i18n support
 *
 * Revision 1.37  2005/10/16 11:56:51  stephen
 * Update for ROX-CLib changes, many externally visible symbols
 * (functions and types) now have rox_ or ROX prefixes.
 * Can get ROX-CLib via 0launch.
 * Alarms are show in the system tray.
 *
 * Revision 1.36  2005/05/27 10:14:07  stephen
 * Fix for creating applets in remote mode, need to give the filer long enough
 * to notice the widget was created.
 * Use apsymbols for Linux portability.
 *
 * Revision 1.35  2005/05/07 10:50:38  stephen
 * Faster creation of small face displays
 *
 * Revision 1.34  2004/11/21 13:03:57  stephen
 * Use ROXInfoWin
 *
 * Revision 1.33  2004/10/29 13:36:48  stephen
 * Use rox_choices_load()/save() and  window counting
 *
 * Revision 1.32  2004/10/23 11:32:05  stephen
 * digital readout named for theming
 * Use new window counting stuff in ROX-CLib
 *
 * Revision 1.31  2004/08/19 19:26:14  stephen
 * React to change of font. Icon to indicate if alarms are set.
 *
 * Revision 1.30  2004/08/05 17:20:36  stephen
 * Pre-draw the clock face.
 * Reduce flicker
 * Scale font on clock face.
 *
 * Revision 1.29  2004/04/10 12:07:33  stephen
 * Remove dead code.  Open options dialog from command line or SOAP message.
 *
 * Revision 1.28  2004/03/26 15:25:38  stephen
 * Use ROX-CLib 2.1.0
 *
 * Revision 1.27  2003/06/21 13:09:10  stephen
 * Convert to new options system.  Use pango for fonts.
 * New option for no-face.
 *
 * Revision 1.26  2003/03/25 14:32:28  stephen
 * Re-enabled SOAP server, to test new ROX-CLib code.
 *
 * Revision 1.25  2003/03/05 15:30:39  stephen
 * First pass at conversion to GTK 2.
 *
 * Revision 1.24  2002/08/24 16:39:52  stephen
 * Fix compilation problem with libxml2.
 *
 * Revision 1.23  2002/04/29 08:33:30  stephen
 * Use replacement applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.22  2002/03/25 10:34:11  stephen
 * Use ROX-CLib's SOAP server code to have one instance run multiple windows.
 * Set an icon for the window. (Doesn't quite work properly...)
 *
 * Revision 1.21  2002/02/25 09:48:38  stephen
 * Fix compilation problems.
 *
 * Revision 1.20  2002/01/29 15:24:34  stephen
 * Use new applet menu positioning code.  Added -h and -v options.
 *
 * Revision 1.19  2001/11/29 16:16:15  stephen
 * Use font selection dialog instead of widget.
 * Added a monday-thursday repeat.
 * Test for altzone in <time.h>
 *
 * Revision 1.18  2001/11/12 14:40:39  stephen
 * Change to XML handling: requires 2.4 or later.  Use old style config otherwise.
 *
 * Revision 1.17  2001/11/08 15:10:57  stephen
 * Fix memory leak in xml routines
 *
 * Revision 1.16  2001/11/08 14:10:04  stephen
 * Can set font for the hour numbers. Can use a tiled background if you
 * set up your gtkrc. Fixed bug in new XML format config (interval was
 * incorrect).
 *
 * Revision 1.15  2001/11/06 12:23:05  stephen
 * Use XML for alarms and config file
 *
 * Revision 1.14  2001/08/20 15:18:13  stephen
 * Switch to using ROX-CLib.
 *
 * Revision 1.13  2001/07/18 10:42:29  stephen
 * Improved debug output
 *
 * Revision 1.12  2001/06/29 10:41:25  stephen
 * Fix use of colourmap when we are running under a theme.
 * Add (saved) menu acellerators
 *
 * Revision 1.11  2001/06/14 12:28:28  stephen
 * Save alarms file after one is raised. Added some (untested) i18n
 * support. Dropped old config format.
 *
 * Revision 1.10  2001/05/25 07:58:14  stephen
 * Can set initial size of applet.
 *
 * Revision 1.9  2001/05/18 09:37:27  stephen
 * Fixed bug in update (needed to return TRUE!), display tweaks
 *
 * Revision 1.8  2001/05/16 11:22:06  stephen
 * Fix bug in loading config.
 *
 * Revision 1.7  2001/05/16 11:02:17  stephen
 * Added repeating alarms.
 * Menu supported on applet (compile time option).
 *
 * Revision 1.6  2001/05/10 14:54:28  stephen
 * Added new alarm feature
 *
 * Revision 1.5  2001/05/09 10:04:54  stephen
 * New AppInfo data.
 * Turn off debug output.
 *
 * Revision 1.4  2001/04/24 07:51:47  stephen
 * Better config system.  Many display improvements
 *
 */
