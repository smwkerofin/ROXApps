/*
 * Clock - a clock for ROX which can run as an applet.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: clock.c,v 1.19 2001/11/29 16:16:15 stephen Exp $
 */
#include "config.h"

#define DEBUG              1
#define INLINE_FONT_SEL    0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <errno.h>

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

#if USE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef HAVE_XML
#include <tree.h>
#include <parser.h>
#endif

#if defined(HAVE_XML) && LIBXML_VERSION>=20400
#define USE_XML 1
#else
#define USE_XML 0
#endif

#include "choices.h"
#include "rox_debug.h"
#include "infowin.h"
#include "applet.h"
#include "alarm.h"

static gboolean applet_mode=FALSE;

typedef struct time_format {
  const char *name;   /* User visible name for this format */
  const char *fmt;    /* Format to pass to strftime(3c) */
  guint32 interval;   /* Suitable interval rate */
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

/* Flags for display mode */
#define MODE_SECONDS 1  /* Plot the second hand */
#define MODE_NO_TEXT 2  /* Don't show the text below the clock face */
#define MODE_HOURS   4  /* Show the hours on the clock face */
#define MODE_NFLAGS 3

typedef struct mode {
  TimeFormat *format;  /* Format of text below clock */
  int flags;          /* Bit mask of flags defined above */
  guint32 interval;   /* Interval between updates (in ms) */
  guint init_size;    /* Initial size of applet */
  const char *font_name;   /* Font for drawing clock numbers */
} Mode;

static Mode mode={
  formats, MODE_SECONDS, 500, 64, NULL
};


static GtkWidget *digital_out = NULL; /* Text below clock face */
static GtkWidget *menu=NULL;          /* Popup menu */
static GtkWidget *infowin=NULL;       /* Information window */
static GtkWidget *canvas = NULL;      /* Displays clock face */
static GdkPixmap *pixmap = NULL;      /* Draw face here, copy to canvas */
static GdkGC *gc=NULL;
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

static GtkWidget *confwin=NULL;     /* Window for configuring */
static GtkWidget *mode_sel=NULL;    /* Selects a display format */
static GtkWidget *sel_fmt=NULL;     /* Shows the current format */
static GtkWidget *user_fmt=NULL;    /* Entering the user-defined format */
static GtkWidget *mode_flags[MODE_NFLAGS];
static GtkWidget *interval=NULL;
static GtkWidget *init_size=NULL;
#if INLINE_FONT_SEL
static GtkWidget *font_sel=NULL;
#else
static GtkWidget *font_window;
static GtkWidget *font_name;
#endif

static guint update_tag;            /* Handle for the timeout */
static time_t config_time=0;        /* Time our config file was last changed */
static gboolean load_alarms=TRUE;   /* Also controls if we show them */
static gboolean save_alarms=TRUE;   

#if EXTRA_FUN
#include "extra.h"
static GdkPixbuf *sprite=NULL;
static guint sprite_tag=0;
static enum {
  T_GONE, T_APPEAR, T_SHOW, T_GO
} sprite_state=T_GONE;
#define SHOW_FOR 10000
#define HIDE_FOR 180
#define HIDE_FOR_PLUS 120
#define NEXT_SHOW() ((HIDE_FOR+(rand()>>7)%HIDE_FOR_PLUS)*1000)
/*#define NEXT_SHOW() ((0+(rand()>>7)%30)*1000)*/
static int sprite_x, sprite_y;
static int sprite_width, sprite_height;
static void setup_sprite(void);

#define NFRAME 3
#define CHANGE_FOR 500
static GdkBitmap *masks[NFRAME];
static int sprite_frame;
#endif

static gboolean do_update(void);        /* Update clock face and text out */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event);
                                    /* Window resized */
static gint expose_event(GtkWidget *widget, GdkEventExpose *event);
                                    /* Window needs redrawing */
static void menu_create_menu(GtkWidget *window);
                                    /* create the pop-up menu */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on canvas */
static void show_info_win(void);
static void show_conf_win(void);

static void read_config(void);
static void write_config(void);
#if USE_XML
static gboolean read_config_xml(void);
static void write_config_xml(void);
#endif
static void check_config(void);

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vh] [XID]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
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
#if EXTRA_FUN
  printf("  Time and relative dimensions... in space\n");
  printf("  Show for %d ms, %d frames of %d ms\n",
	 SHOW_FOR, NFRAME, CHANGE_FOR);
  printf("  Hide for %d to %d s\n", HIDE_FOR, HIDE_FOR+HIDE_FOR_PLUS);
#endif
}

int main(int argc, char *argv[])
{
  GtkWidget *win=NULL;
  GtkWidget *vbox;
  GtkStyle *style;
  time_t now;
  char buf[80];
  int i, c, do_exit, nerr;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
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

  rox_debug_init("Clock");
  dprintf(1, "Debug system inited");

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  /* Initialise X/GDK/GTK */
  dprintf(2, "Initialise X/GDK/GTK");
  gtk_init(&argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  gtk_widget_push_colormap(gdk_rgb_get_cmap());
  
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
  dprintf(2, "Initialise Choices");
  choices_init();
  dprintf(2, "Read config");
  read_config();
  dprintf(2, "Read alarms");
  if(load_alarms)
    alarm_load();

  dprintf(4, "argc=%d", argc);
  if(!argv[optind] || !atol(argv[optind])) {
    /* No arguments, or the first argument was not a (non-zero) number.
       We are not an applet, so create a window */
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "clock");
    gtk_window_set_title(GTK_WINDOW(win), _("Clock"));
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    dprintf(3, "initial size=%d\n", mode.init_size);
    gtk_widget_set_usize(win, mode.init_size, mode.init_size);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_realize(win);

  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;

    dprintf(3, "argv[%d]=%s", optind, argv[optind]);
    plug=gtk_plug_new(atol(argv[optind]));
    if(!plug) {
      fprintf(stderr, _("%s: failed to plug into socket %s, not a XID?\n"),
	      argv[0], argv[optind]);
      exit(1);
    }
    applet_mode=TRUE;
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    dprintf(3, "initial size=%d", mode.init_size);
    gtk_widget_set_usize(plug, mode.init_size, mode.init_size);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), plug);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);

    
    /* Isn't object oriented code wonderful.  Now that we have done the
       plug specific code, its just a containing widget.  For the rest of the
       program we just pretend it was a window and it all works
    */
    win=plug;
  }

  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  /* Get the right grey for the background */
  style=gtk_widget_get_style(vbox);
  colours[GREY]=style->bg[GTK_STATE_NORMAL];

  cmap=gdk_rgb_get_cmap();
  for(i=0; i<NUM_COLOUR; i++)
    gdk_color_alloc(cmap, colours+i);

  canvas=gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(canvas), 64, 48);
  /* next line causes trouble??  maybe just as an applet... */
  gtk_widget_set_events (canvas, GDK_EXPOSURE_MASK);
  gtk_box_pack_start (GTK_BOX (vbox), canvas, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (canvas), "expose_event",
		      (GtkSignalFunc) expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT (canvas), "configure_event",
		      (GtkSignalFunc) configure_event, NULL);
  gtk_widget_realize(canvas);
  gtk_widget_show (canvas);
  gtk_widget_set_name(canvas, "clock face");
  
  gc=gdk_gc_new(canvas->window);

  time(&now);
  strftime(buf, 80, mode.format->fmt, localtime(&now));

  /* This is the text below the clock face */
  digital_out=gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), digital_out, FALSE, FALSE, 2);
  if(!(mode.flags & MODE_NO_TEXT))
    gtk_widget_show(digital_out);

  if(applet_mode);
    applet_get_panel_location(win);
  if(win)
    gtk_widget_show(win);

  /* Make sure we get called periodically */
  update_tag=gtk_timeout_add(mode.interval,
			 (GtkFunction) do_update, digital_out);

#if EXTRA_FUN
  setup_sprite();
#endif

  dprintf(2, "into main.\n");
  gtk_main();

  if(save_alarms)
    alarm_save();

  return 0;
}

/* Called when the display mode changes */
static void set_mode(Mode *nmode)
{
  dprintf(3, "mode now %s, %d, %#x, font %s\n", nmode->format->name,
	  nmode->interval, nmode->flags,
	  nmode->font_name? nmode->font_name: "(default)");

  /* Do we need to change the update rate? */
  if(update_tag && mode.interval!=nmode->interval) {
    gtk_timeout_remove(update_tag);
    update_tag=gtk_timeout_add(nmode->interval,
			       (GtkFunction) do_update, digital_out);
    dprintf(4, "tag now %u (%d)\n", update_tag, nmode->interval);
  }

  /* Change visibility of text line? */
  if(digital_out &&
     (nmode->flags & MODE_NO_TEXT)!=(mode.flags & MODE_NO_TEXT)) {
    if(nmode->flags & MODE_NO_TEXT)
      gtk_widget_hide(digital_out);
    else
      gtk_widget_show(digital_out);
  }

  if(mode.font_name && mode.font_name!=nmode->font_name)
    g_free((gpointer) mode.font_name);
  
  mode=*nmode;
}

enum draw_hours {
  DH_NO, DH_QUARTER, DH_ALL
};

/* Redraw clock face and update digital_out */
static gboolean do_update(void)
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
  GdkColor *face_fg=CL_FACE_FG;
  GdkFont *font=NULL;
  GtkStyle *style;

  /*
   * Has the config changed?  Another instance may have saved its config,
   * we should read it in and use that.
   */
  check_config();

  time(&now);
  tms=localtime(&now);
    
  if(digital_out && !(mode.flags & MODE_NO_TEXT)) {
    strftime(buf, 80, mode.format->fmt, tms);
    
    gtk_label_set_text(GTK_LABEL(digital_out), buf);
  }

  if(!gc || !canvas || !pixmap)
    return TRUE;
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;

  if(alarm_have_active())
    face_fg=CL_FACE_FG_ALRM;

  style=gtk_widget_get_style(canvas);
  dprintf(3, "style=%p bg_gc[GTK_STATE_NORMAL]=%p", style,
	  style->bg_gc[GTK_STATE_NORMAL]);
  if(style && style->bg_gc[GTK_STATE_NORMAL]) {
    /*gdk_draw_rectangle(pixmap, style->bg_gc[GTK_STATE_NORMAL], TRUE,
		       0, 0, w, h);*/
    gtk_style_apply_default_background(style, pixmap, TRUE, GTK_STATE_NORMAL,
				       NULL, 0, 0, w, h);
  } else {
    /* Blank out to the background colour */
    gdk_gc_set_foreground(gc, CL_BACKGROUND);
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, w, h);
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

  if(!font && mode.font_name) {
    font=gdk_font_load(mode.font_name);
  }
  if(!font) 
    font=canvas->style->font;
  gdk_font_ref(font);
  
  /* Draw the clock face, including the hours */
  gdk_gc_set_foreground(gc, face_fg);
  gdk_draw_arc(pixmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0, 360*64);
  sw-=4;
  sh-=4;
  rad=sw/2;
  gdk_gc_set_foreground(gc, CL_FACE_BG);
  gdk_draw_arc(pixmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0, 360*64);
  if(mode.flags & MODE_HOURS) {
    int siz;
    
    twidth=gdk_text_width(font, "12", 2);
    siz=sw/twidth;

    if(siz<4)
      th=DH_NO;
    else if(siz<10)
      th=DH_QUARTER;
    else
      th=DH_ALL;
  } else {
    th=0;
  }
  for(tick=0; tick<12; tick++) {
    double ang=tick*2*M_PI/12.;
    double x2=x0+rad*sin(ang)*7/8;
    double y2=y0-rad*cos(ang)*7/8;
    x1=x0+rad*sin(ang);
    y1=y0-rad*cos(ang);
    gdk_gc_set_foreground(gc, face_fg);
    gdk_draw_line(pixmap, gc, x2, y2, x1, y1);

    if(th!=DH_NO && (th==DH_ALL || tick%3==0)) {
      char buf[16];
      int l;
      int xt, yt;
      
      gdk_gc_set_foreground(gc, CL_HOUR_TEXT);
      sprintf(buf, "%d", tick==0? 12: tick);
      l=strlen(buf);
      xt=x2-gdk_text_width(font, buf, l)/2;
      yt=y2+gdk_text_height(font, buf, l)/2;
      gdk_draw_string(pixmap, font, gc, xt, yt, buf);
    }
  }

#if EXTRA_FUN
  if(sprite) {
    GdkGC *lgc;

    dprintf(5, "state=%d\n", sprite_state);
    switch(sprite_state) {
    case T_SHOW:
      gdk_pixbuf_render_to_drawable_alpha(sprite, pixmap,
					  0, 0,
					  sprite_x, sprite_y,
					  sprite_width, sprite_height,
					  GDK_PIXBUF_ALPHA_BILEVEL,
					  127,
					  GDK_RGB_DITHER_NORMAL,
					  0, 0);
      break;

    case T_APPEAR: case T_GO:
      dprintf(3, "state=%d, frame=%d\n", sprite_state, sprite_frame);
      if(sprite_frame>=0 && sprite_frame<NFRAME && masks[sprite_frame]) {
	lgc=gdk_gc_new(canvas->window);

	gdk_gc_set_clip_mask(lgc, masks[sprite_frame]);
	gdk_gc_set_clip_origin(lgc, sprite_x, sprite_y);
	gdk_pixbuf_render_to_drawable(sprite, pixmap, lgc, 0, 0,
				      sprite_x, sprite_y,
				      sprite_width, sprite_height,
				      GDK_RGB_DITHER_NORMAL,
				      0, 0);
	gdk_gc_unref(lgc);
      }
      break;
    }
  }
#endif

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
    gdk_gc_set_foreground(gc, CL_HOUR_HAND);
    gdk_draw_polygon(pixmap, gc, TRUE, points, 4);
    
  } else {
    x1=x0+rad*sin(h_ang)*0.5;
    y1=y0-rad*cos(h_ang)*0.5;
    gdk_gc_set_foreground(gc, CL_HOUR_HAND);
    gdk_gc_set_line_attributes(gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND,
			       GDK_JOIN_MITER);
    gdk_draw_line(pixmap, gc, x0, y0, x1, y1);
    
  }
  
  x1=x0+rad*sin(m_ang)*0.95;
  y1=y0-rad*cos(m_ang)*0.95;
  gdk_gc_set_foreground(gc, CL_MINUTE_HAND);
  gdk_gc_set_line_attributes(gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND,
			     GDK_JOIN_MITER);
  gdk_draw_line(pixmap, gc, x0, y0, x1, y1);
  
  gdk_gc_set_line_attributes(gc, 0, GDK_LINE_SOLID, GDK_CAP_ROUND,
			     GDK_JOIN_MITER);
  if(mode.flags & MODE_SECONDS) {
    int x2, y2;
    
    x1=x0+rad*sin(s_ang)*0.95;
    y1=y0-rad*cos(s_ang)*0.95;
    x2=x0+rad*sin(s_ang+M_PI)*0.05;
    y2=y0-rad*cos(s_ang+M_PI)*0.05;
    gdk_gc_set_foreground(gc, CL_SECOND_HAND);
    gdk_draw_line(pixmap, gc, x2, y2, x1, y1);
  }
  gdk_gc_set_foreground(gc, CL_HOUR_HAND);
  rad=sw/10;
  if(rad>8)
    rad=8;
  gdk_draw_arc(pixmap, gc, TRUE, x0-rad/2, y0-rad/2, rad, rad, 0, 360*64);

  /* Copy the pixmap to the display canvas */
  gdk_draw_pixmap(canvas->window,
		  canvas->style->fg_gc[GTK_WIDGET_STATE (canvas)],
		  pixmap,
		  0, 0,
		  0, 0,
		  w, h);

  if(load_alarms) {
    if(alarm_check())
      if(save_alarms)
	alarm_save();
  }

  gdk_font_unref(font);

  return TRUE;
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event)
{
  int w, h;
  
  if(!gc || !canvas || !pixmap)
    return;
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;

  /* Copy the pixmap to the display canvas */
  gdk_draw_pixmap(canvas->window,
		  canvas->style->fg_gc[GTK_WIDGET_STATE (canvas)],
		  pixmap,
		  0, 0,
		  0, 0,
		  w, h);

  return TRUE;
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event)
{
  if (pixmap)
    gdk_pixmap_unref(pixmap);

  pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height,
			  -1);
  do_update();

  return TRUE;
}

#if USE_XML
static void write_config_xml(void)
{
  gchar *fname;
  gboolean ok;

  fname=choices_find_path_save("options.xml", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr tree;
    FILE *out;
    char buf[80];

    doc = xmlNewDoc("1.0");
    doc->children=xmlNewDocNode(doc, NULL, "Clock", NULL);
    xmlSetProp(doc->children, "version", VERSION);

    tree=xmlNewChild(doc->children, NULL, "format", NULL);
    xmlSetProp(tree, "name", mode.format->name);
    tree=xmlNewChild(doc->children, NULL, "flags", NULL);
    sprintf(buf, "%u", (unsigned int) mode.flags);
    xmlSetProp(tree, "value", buf);
    tree=xmlNewChild(doc->children, NULL, "interval", NULL);
    sprintf(buf, "%ld", (long) mode.interval);
    xmlSetProp(tree, "value", buf);
    tree=xmlNewChild(doc->children, NULL, "user-format", user_defined);
    if(mode.font_name)
      tree=xmlNewChild(doc->children, NULL, "font", mode.font_name);
    tree=xmlNewChild(doc->children, NULL, "applet", NULL);
    sprintf(buf, "%d", (int) mode.init_size);
    xmlSetProp(tree, "initial-size", buf);
  
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);
    if(ok) 
      time(&config_time);

    xmlFreeDoc(doc);
    g_free(fname);
  }
}
#endif

/* Write the config to a file */
static void write_config(void)
{
#if !USE_XML
  gchar *fname;

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

      /* These aren't translated */
      fprintf(out, "time_format=%s\n", mode.format->name);
      fprintf(out, "flags=%d\n", (int) mode.flags);
      fprintf(out, "interval=%ld\n", (long) mode.interval);
      fprintf(out, "user_format=%s\n", user_defined);
      fprintf(out, "init_size=%d\n", (int) mode.init_size);
      if(mode.font_name)
	fprintf(out, "font_name=%s\n", mode.font_name);

      fclose(out);

      config_time=now;
    }

    g_free(fname);
  }
#else
  write_config_xml();
#endif
}

#if USE_XML
static gboolean read_config_xml(void)
{
  guchar *fname;

  fname=choices_find_path_load("options.xml", PROJECT);

  if(fname) {
#if defined(HAVE_SYS_STAT_H)
    struct stat statb;
#endif
    Mode nmode;
    xmlDocPtr doc;
    xmlNodePtr node, root;
    const xmlChar *string;

    nmode=mode;
    
#ifdef HAVE_STAT
    if(stat(fname, &statb)==0) {
      config_time=statb.st_mtime;
    }
#else
    time(&config_time);
#endif

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

    if(strcmp(root->name, "Clock")!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *str;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      if(strcmp(node->name, "format")==0) {
	int i;
	
	str=xmlGetProp(node, "name");
	if(!str)
	  continue;

	for(i=0; formats[i].name; i++)
	  if(strcmp(str, formats[i].name)==0)
	    break;
	if(formats[i].name)
	  nmode.format=formats+i;
	free(str);
	
      } else if(strcmp(node->name, "flags")==0) {
 	str=xmlGetProp(node, "value");
	if(!str)
	  continue;
	nmode.flags=atoi(str);
	free(str);

      } else if(strcmp(node->name, "interval")==0) {
 	str=xmlGetProp(node, "value");
	if(!str)
	  continue;
	nmode.interval=atol(str);
	free(str);

      } else if(strcmp(node->name, "user-format")==0) {
	str=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!str)
	  continue;
	strncpy(user_defined, str, sizeof(user_defined));
	free(str);

      } else if(strcmp(node->name, "font")==0) {
	str=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!str)
	  continue;
	nmode.font_name=g_strdup(str);
	free(str);

      } else if(strcmp(node->name, "applet")==0) {
 	str=xmlGetProp(node, "initial-size");
	if(!str)
	  continue;
	nmode.init_size=atoi(str);
	free(str);

      }
    }
    
    if(nmode.init_size<16)
      nmode.init_size=16;
    set_mode(&nmode);
    
    g_free(fname);
    xmlFreeDoc(doc);
    return TRUE;
  }

  return FALSE;
}
#endif

/* Read in the config */
static void read_config(void)
{
  guchar *fname;

#if USE_XML
  if(read_config_xml())
    return;
#endif
  
  fname=choices_find_path_load("options", PROJECT);

  if(fname) {
#if defined(HAVE_SYS_STAT_H)
    struct stat statb;
#endif
    FILE *in;
    
#ifdef HAVE_STAT
    if(stat(fname, &statb)==0) {
      config_time=statb.st_mtime;
    }
#else
    time(&config_time);
#endif

    in=fopen(fname, "r");
    if(in) {
      char buf[1024], *line;
      char *end;
      gchar *words;
      Mode nmode;

      nmode=mode;

#ifdef HAVE_FSTAT
      if(fstat(fileno(in), &statb)==0) {
	config_time=statb.st_mtime;
      }
#else
#endif

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

	words=g_strstrip(line);
	if(words[0]) {
	  gchar *var, *val;

	  end=strchr(words, '=');
	  if(end) {
	    /* var = val */
	    val=g_strstrip(end+1);
	    *end=0;
	    var=g_strstrip(words);

	    if(strcmp(var, "time_format")==0) {
	      int i;

	      for(i=0; formats[i].name; i++)
		if(strcmp(val, formats[i].name)==0)
		  break;
	      if(formats[i].name)
		nmode.format=formats+i;
	      
	    } else if(strcmp(var, "flags")==0) {
	      nmode.flags=atoi(val);
	      
	    } else if(strcmp(var, "interval")==0) {
	      nmode.interval=(guint32) atol(val);
	      
	    } else if(strcmp(var, "user_format")==0) {
	      strncpy(user_defined, val, sizeof(user_defined));
	      
	    } else if(strcmp(var, "init_size")==0) {
	      nmode.init_size=(guint) atoi(val);
	      
	    } else if(strcmp(var, "font_name")==0) {
	      nmode.font_name=g_strdup(val);
	      
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);

      if(nmode.init_size<16)
	nmode.init_size=16;
      set_mode(&nmode);
    }
    
    g_free(fname);
  }
}

/* Check timestamp of config file against time we last read it, if file
   has changed, call read_config() */
static void check_config(void)
{
  gchar *fname;

  if(config_time==0) {
    read_config();
    return;
  }

#if USE_XML
  fname=choices_find_path_load("options.xml", PROJECT);
  if(!fname)
#endif
    fname=choices_find_path_load("options", PROJECT);
  dprintf(5, "%p: %s", fname, fname? fname: "NULL");

  if(fname) {
#ifdef HAVE_SYS_STAT_H
    struct stat statb;
#endif

#ifdef HAVE_STAT
    if(stat(fname, &statb)==0) {
      if(statb.st_mtime>config_time)
	read_config();
    }
#else
    /* Don't read the config again. */
#endif

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

/* User cancels a change of config */
static void cancel_config(GtkWidget *widget, gpointer data)
{
  int i;
  /* Reset window contents */
  gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel),
			      (int) (mode.format-formats));
  gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);
  for(i=0; i<MODE_NFLAGS; i++)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode_flags[i]),
				 mode.flags & 1<<i);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(interval), mode.interval/1000.);

  /* Hide window */
  gtk_widget_hide(confwin);
}

static void set_config(GtkWidget *widget, gpointer data)
{
  GtkWidget *menu;
  GtkWidget *item;
  gchar *udef;
  Mode nmode;
  gfloat sec;
  int i;
  gchar *text;

  /* get data from window contents */
  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(mode_sel));
  item=gtk_menu_get_active(GTK_MENU(menu));
  nmode.format=(TimeFormat *) gtk_object_get_data(GTK_OBJECT(item), "format");
  nmode.flags=0;
  for(i=0; i<MODE_NFLAGS; i++)
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mode_flags[i])))
      nmode.flags|=1<<i;
  sec=gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(interval));
  nmode.interval=(guint32) (sec*1000.);
  nmode.init_size=
    (guint) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(init_size));
    
  udef=gtk_entry_get_text(GTK_ENTRY(user_fmt));
  strncpy(user_defined, udef, sizeof(user_defined));

#if INLINE_FONT_SEL
  nmode.font_name=
    gtk_font_selection_get_font_name(GTK_FONT_SELECTION(font_sel));
#else
  gtk_label_get(GTK_LABEL(font_name), &text);
  nmode.font_name=g_strdup(text);
#endif

  dprintf(2, "format=%s, %s", nmode.format->name, nmode.format->fmt);
  dprintf(2, "flags=%d, interval=%d", nmode.flags, nmode.interval);
  dprintf(2, "font=%s", nmode.font_name);
  
  set_mode(&nmode);
  
  if(GPOINTER_TO_INT(data))
    write_config();
  do_update();
  
  gtk_widget_hide(confwin);
}

#if !INLINE_FONT_SEL
static void set_font(GtkWidget *widget, gpointer data)
{
  gchar *name;

  name=gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_window));
  if(name) {
    gtk_label_set_text(GTK_LABEL(font_name), name);
    g_free(name);
    gtk_widget_hide(font_window);
  }
}

static void show_font_window(GtkWidget *widget, gpointer data)
{
  gchar *name;

  gtk_label_get(GTK_LABEL(font_name), &name);
  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(font_window),
					  name);

  gtk_widget_show(font_window);
}

static void hide_font_window(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(font_window);
}

#endif

static void sel_format(GtkWidget *widget, gpointer data)
{
  GtkWidget *menu;
  GtkWidget *item;
  TimeFormat *fmt;
  
  /* get data from window contents */
  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(mode_sel));
  item=gtk_menu_get_active(GTK_MENU(menu));
  fmt=(TimeFormat *) gtk_object_get_data(GTK_OBJECT(item), "format");
  
  if(fmt) {
    if(fmt->fmt==user_defined) {
      gtk_label_set_text(GTK_LABEL(sel_fmt),
			 gtk_entry_get_text(GTK_ENTRY(user_fmt)));
    } else
      gtk_label_set_text(GTK_LABEL(sel_fmt), fmt->fmt);
  }
}

/* Show the configure window */
static void show_conf_win(void)
{
  int i;
  
  if(!confwin) {
    /* Need to create it first */
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    GtkWidget *label;
    GtkWidget *menu;
    GtkWidget *item;
    GtkRequisition req;
    GtkObject *adj;
    GtkStyle *style;
    int mw=0, mh=0;
    int set=0;

    static const char *flags[]={
      N_("Second hand"), N_("Hide text"), N_("Numbers on face"),
      NULL
    };

    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), _("Configuration"));
    gtk_window_set_position(GTK_WINDOW(confwin), GTK_WIN_POS_MOUSE);

    vbox=GTK_DIALOG(confwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Time format"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    mode_sel=gtk_option_menu_new();
    gtk_widget_show(mode_sel);
    gtk_box_pack_start(GTK_BOX(hbox), mode_sel, TRUE, TRUE, 2);

    menu=gtk_menu_new();
    gtk_option_menu_set_menu(GTK_OPTION_MENU(mode_sel), menu);

    for(i=0; formats[i].name; i++) {
      item=gtk_menu_item_new_with_label(_(formats[i].name));
      if(mode.format==formats+i)
	set=i;
      gtk_object_set_data(GTK_OBJECT(item), "format", formats+i);
      gtk_widget_show(item);
      gtk_widget_size_request(item, &req);
      if(mw<req.width)
	mw=req.width;
      if(mh<req.height)
	mh=req.height;
      gtk_signal_connect(GTK_OBJECT(item), "activate",
		       GTK_SIGNAL_FUNC(sel_format), mode_sel);
      gtk_menu_append(GTK_MENU(menu), item);
    }
    gtk_widget_set_usize(mode_sel, mw+50, mh+4);
    gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel), set);
    
    sel_fmt=gtk_label_new(mode.format->fmt);
    gtk_widget_show(sel_fmt);
    gtk_box_pack_start(GTK_BOX(hbox), sel_fmt, FALSE, FALSE, 2);

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("User defined format"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    user_fmt=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);
    gtk_widget_show(user_fmt);
    gtk_box_pack_start(GTK_BOX(hbox), user_fmt, TRUE, TRUE, 2);    

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Update rate (s)"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    adj=gtk_adjustment_new((gfloat) (mode.interval/1000.), 0.1, 60.,
			   0.1, 5, 5);
    interval=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1., 2);
    gtk_widget_show(interval);
    gtk_box_pack_start(GTK_BOX(hbox), interval, FALSE, FALSE, 2);

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Options"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    for(i=0; i<MODE_NFLAGS; i++) {
      mode_flags[i]=gtk_check_button_new_with_label(_(flags[i]));
      gtk_widget_show(mode_flags[i]);
      gtk_box_pack_start(GTK_BOX(hbox), mode_flags[i], FALSE, FALSE, 2);
    }

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Display font"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

#if INLINE_FONT_SEL
    font_sel=gtk_font_selection_new();
    gtk_font_selection_set_preview_text(GTK_FONT_SELECTION(font_sel),
					"1 2 3 4 5 6 7 8 9 10 11 12");
    gtk_widget_show(font_sel);
    gtk_box_pack_start(GTK_BOX(hbox), font_sel, FALSE, FALSE, 2);
#else
    font_name=gtk_label_new(mode.font_name? mode.font_name: "");
    gtk_widget_show(font_name);
    gtk_box_pack_start(GTK_BOX(hbox), font_name, FALSE, FALSE, 2);

    font_window=gtk_font_selection_dialog_new("Choose display font");
    gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(font_window),
					"1 2 3 4 5 6 7 8 9 10 11 12");
    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_window)->ok_button), 
		       "clicked", GTK_SIGNAL_FUNC(set_font), NULL);
    gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_window)->cancel_button), 
		       "clicked", GTK_SIGNAL_FUNC(hide_font_window), NULL);

    but=gtk_button_new_with_label("Change");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(show_font_window), NULL);
#endif

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Initial size"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    adj=gtk_adjustment_new((gfloat) mode.init_size, 16, 128,
			   2, 16, 16);
    init_size=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1., 0);
    gtk_widget_show(init_size);
    gtk_box_pack_start(GTK_BOX(hbox), init_size, FALSE, FALSE, 2);

    label=gtk_label_new(_("(only used on start up)"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    hbox=GTK_DIALOG(confwin)->action_area;

    but=gtk_button_new_with_label(_("Set"));
    GTK_WIDGET_SET_FLAGS(but, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(but);
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       GINT_TO_POINTER(FALSE));

    but=gtk_button_new_with_label(_("Save"));
    GTK_WIDGET_SET_FLAGS(but, GTK_CAN_DEFAULT);
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       GINT_TO_POINTER(TRUE));

    but=gtk_button_new_with_label(_("Cancel"));
    GTK_WIDGET_SET_FLAGS(but, GTK_CAN_DEFAULT);
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(cancel_config), confwin);

  }

  /* Initialise values */
  gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel),
			      (int) (mode.format-formats));
  gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);
  for(i=0; i<MODE_NFLAGS; i++)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode_flags[i]),
				 mode.flags & 1<<i);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(interval), mode.interval/1000.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(init_size), mode.init_size);

#if INLINE_FONT_SEL
  if(mode.font_name)
    gtk_font_selection_set_font_name(GTK_FONT_SELECTION(font_sel),
				     mode.font_name);
#else
  if(mode.font_name) {
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(font_window),
				     mode.font_name);
    gtk_label_set_text(GTK_LABEL(font_name), mode.font_name);
  }
#endif

  gtk_widget_show(confwin);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),       	NULL, show_conf_win, 0, NULL },
  { N_("/Alarms..."),		NULL, alarm_show_window, 0, NULL },
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0, NULL },
};

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

/* Button press in canvas */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    if(bev->state & GDK_CONTROL_MASK) /* ctrl-menu */
      return FALSE; /* Let it pass */
    /* Pop up the menu */
    if(!menu) 
      menu_create_menu(GTK_WIDGET(win));

    if(!save_alarms || !load_alarms) {
      GList *children=GTK_MENU_SHELL(menu)->children;
      GtkWidget *alarms=GTK_WIDGET(g_list_nth(children, 2));
      
      gtk_widget_set_sensitive(alarms, FALSE);
    }

    if(applet_mode)
      applet_show_menu(menu, bev);
    else
      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
    return TRUE;
  } else if(bev->button==1 && applet_mode) {
      gchar *cmd;

      cmd=g_strdup_printf("%s/AppRun &", getenv("APP_DIR"));
      system(cmd);
      g_free(cmd);

      return TRUE;
  }

  return FALSE;
}

#if EXTRA_FUN

static gboolean start_show_sprite(void);

static gboolean fade_out_sprite(void)
{
  do_update();

  sprite_frame--;
  if(sprite_frame<0) {
    sprite_state=T_GONE;
    sprite_tag=gtk_timeout_add(NEXT_SHOW(), (GtkFunction) start_show_sprite,
			       NULL);
    return FALSE;
  }
  return TRUE;
}

static gboolean end_show_sprite(void)
{
  sprite_state=T_GO;
  sprite_frame=NFRAME-1;
  sprite_tag=gtk_timeout_add(CHANGE_FOR/NFRAME, (GtkFunction) fade_out_sprite,
			     NULL);
  
  return FALSE;
}

static gboolean fade_in_sprite(void)
{
  do_update();

  sprite_frame++;
  if(sprite_frame>=NFRAME) {
    sprite_state=T_SHOW;
    sprite_tag=gtk_timeout_add(SHOW_FOR, (GtkFunction) end_show_sprite,
			       NULL);
    return FALSE;
  }
  return TRUE;
}

static gboolean start_show_sprite(void)
{
  int w, h;
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;
  if(w<sprite_width*2 || h<sprite_height*2)
    return TRUE;

  sprite_x=(rand()>>3)%(w-sprite_width);
  sprite_y=(rand()>>4)%(h-sprite_height);
  dprintf(3, "%dx%d (%dx%d) -> %d,%d\n", w, h, sprite_width, sprite_height,
	 sprite_x, sprite_y);
  sprite_state=T_APPEAR;
  sprite_frame=0;
  sprite_tag=gtk_timeout_add(CHANGE_FOR/NFRAME, (GtkFunction) fade_in_sprite,
			     NULL);
  
  return FALSE;
}

static void setup_sprite(void)
{
  GdkPixmap *pmap;
  time_t now;
  int i;
  int x, y;
  static struct pattern {
    unsigned row;
    int shift;
  } patterns[NFRAME]={
    {0xedb7edb7, 3},
    {0x5a5a5a5a, 2},
    {0x12481248, 4},
  };
  
  sprite=gdk_pixbuf_new_from_xpm_data((const char **) tardis_xpm);
  if(!sprite)
    return;
  
  dprintf(4, "sprite=%p\n", sprite);
  sprite_width=gdk_pixbuf_get_width(sprite);
  sprite_height=gdk_pixbuf_get_height(sprite);
  dprintf(4, "%dx%d\n", sprite_width, sprite_height);

  for(i=0; i<NFRAME; i++) {
    gdk_pixbuf_render_pixmap_and_mask(sprite, &pmap, masks+i, 127);
    dprintf(5, "%d %p %p\n", i, pmap, masks[i]);
    gdk_pixmap_unref(pmap);

    if(masks[i]) {
      GdkGC *bgc=gdk_gc_new(masks[i]);
      
      dprintf(5, "got mask %d, %p (gc=%p)\n", i, masks[i], bgc);
      gdk_gc_set_foreground(gc, colours+BLACK);
      for(y=0; y<sprite_height; y++) {
	for(x=0; x<sprite_width; x++) {
	  /*
	   * Put holes in the mask based on patterns[i]
	   */
	  
	  if(patterns[i].row & (1<<((x+patterns[i].shift*y)%16)))
	    gdk_draw_point(masks[i], bgc, x, y);
	  
	}
      }
      gdk_gc_unref(bgc);
    }
  }
  
  dprintf(3, "set up callback\n");
  time(&now);
  srand((unsigned int) now);
  sprite_tag=gtk_timeout_add(NEXT_SHOW(),
			     (GtkFunction) start_show_sprite, NULL);
			     
}

#endif /* EXTRA_FUN */


/* Show the info window */
static void show_info_win(void)
{
  if(!infowin) {
    /* Need to make it first */
    infowin=info_win_new(PROJECT, PURPOSE, VERSION, AUTHOR, WEBSITE);
    gtk_signal_connect(GTK_OBJECT(infowin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     infowin);
  }

  gtk_widget_show(infowin);
}

/*
 * $Log: clock.c,v $
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
