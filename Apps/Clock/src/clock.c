/*
 * Clock - a clock for ROX which can run as an applet.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: clock.c,v 1.9 2001/05/18 09:37:27 stephen Exp $
 */
#include "config.h"

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

#include <gtk/gtk.h>
#include "infowin.h"

#if USE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "choices.h"
#include "alarm.h"

#define DEBUG              0
#define SUPPORT_OLD_CONFIG 1
#define APPLET_MENU        1

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
  guint init_size;
} Mode;

static Mode mode={
  formats, MODE_SECONDS, 500, 64
};

#if SUPPORT_OLD_CONFIG
typedef struct old_mode {
  const char *name;
  Mode mode;
} OldMode;

static OldMode old_modes[]={
  {N_("24 hour"), {formats+0, MODE_SECONDS, 500}},
  {N_("24 hour, no seconds"), {formats+1, 0, 30000}},
  {N_("12 hour"), {formats+2, MODE_SECONDS, 500}},
  {N_("12 hour, no seconds"), {formats+3, 0, 30000}},
  {N_("Date"), {formats+4, MODE_SECONDS, 500}},
  {N_("Date, no second hand"), {formats+4, 0, 30000}},
  {N_("Full"), {formats+5, MODE_SECONDS, 500}},
  {N_("Full, no second hand"), {formats+5, 0, 500}},
  {N_("None"), {formats+0, MODE_NO_TEXT|MODE_SECONDS, 500}},
  {N_("None, no second hand"), {formats+1, MODE_NO_TEXT, 30000}},
  {N_("User defined"), {formats+6, MODE_SECONDS, 500}},
  {N_("User defined, no second hand"), {formats+6, 0, 500}},

  {NULL, {NULL, 0, 0}}
};
#endif

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

static guint update_tag;            /* Handle for the timeout */
static time_t config_time=0;        /* Time our config file was last changed */
static gboolean load_alarms=TRUE;   /* Also controls if we show them */
static gboolean save_alarms=TRUE;   

#if EXTRA_FUN
#define FADING 1
#include "extra.h"
static GdkPixbuf *sprite=NULL;
static guint sprite_tag=0;
static enum {
  T_GONE, T_APPEAR, T_SHOW, T_GO
} sprite_state=T_GONE;
#define SHOW_FOR 10000
#define NEXT_SHOW() ((180+(rand()>>7)%120)*1000)
/*#define NEXT_SHOW() ((0+(rand()>>7)%30)*1000)*/
static int sprite_x, sprite_y;
static int sprite_width, sprite_height;
static void setup_sprite(void);
#if FADING
#define NFRAME 3
#define CHANGE_FOR 500
static GdkBitmap *masks[NFRAME];
static int sprite_frame;
#endif
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
static void check_config(void);
#if SUPPORT_OLD_CONFIG
static void read_old_config(void);
#endif

void dprintf(const char *fmt, ...);

int main(int argc, char *argv[])
{
  GtkWidget *win=NULL;
  GtkWidget *vbox;
  GtkStyle *style;
  time_t now;
  char buf[80];
  int i;

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
  /*
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  */
#endif
  
  /* Initialise X/GDK/GTK */
  gtk_init(&argc, &argv);
#if EXTRA_FUN
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  gtk_widget_push_colormap(gdk_rgb_get_cmap());
#endif
  
  /* Init choices and read them in */
  choices_init();
  read_config();
  if(load_alarms)
    alarm_load();

  /*dprintf("argc=%d\n", argc);*/
  if(argc<2 || !atol(argv[1])) {
    /* No arguments, or the first argument was not a (non-zero) number.
       We are not an applet, so create a window */
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "clock");
    gtk_window_set_title(GTK_WINDOW(win), _("Clock"));
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    dprintf("initial size=%d\n", mode.init_size);
    gtk_widget_set_usize(win, mode.init_size, mode.init_size);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_realize(win);

  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;

    /*dprintf("argv[1]=%s\n", argv[1]);*/
    plug=gtk_plug_new(atol(argv[1]));
    if(!plug) {
      fprintf(stderr, "%s: failed to plug into socket %s, not a XID?\n",
	      argv[0], argv[1]);
      exit(1);
    }
    applet_mode=TRUE;
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    dprintf("initial size=%d\n", mode.init_size);
    gtk_widget_set_usize(plug, mode.init_size, mode.init_size);

#if APPLET_MENU
    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), plug);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
#else
    save_alarms=FALSE;
#endif

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

#if EXTRA_FUN
  cmap=gdk_rgb_get_cmap();
#else
  cmap=gdk_colormap_get_system();
#endif
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
  
  gc=gdk_gc_new(canvas->window);

  time(&now);
  strftime(buf, 80, mode.format->fmt, localtime(&now));

  /* This is the text below the clock face */
  digital_out=gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), digital_out, FALSE, FALSE, 2);
  if(!(mode.flags & MODE_NO_TEXT))
    gtk_widget_show(digital_out);

  if(win)
    gtk_widget_show(win);

  /* Make sure we get called periodically */
  update_tag=gtk_timeout_add(mode.interval,
			 (GtkFunction) do_update, digital_out);

#if EXTRA_FUN
  setup_sprite();
#endif

  dprintf("into main.\n");
  gtk_main();

  if(save_alarms)
    alarm_save();

  return 0;
}

void dprintf(const char *fmt, ...)
{
  va_list list;

  va_start(list, fmt);
#if DEBUG
  vfprintf(stderr, fmt, list);
#endif
  va_end(list);  
}

/* Called when the display mode changes */
static void set_mode(Mode *nmode)
{
  /*dprintf("mode now %s\n", nmode->name);*/

  /* Do we need to change the update rate? */
  if(update_tag && mode.interval!=nmode->interval) {
    gtk_timeout_remove(update_tag);
    update_tag=gtk_timeout_add(nmode->interval,
			       (GtkFunction) do_update, digital_out);
    /*dprintf("tag now %u (%d)\n", update_tag, nmode->interval);*/
  }

  /* Change visibility of text line? */
  if(digital_out &&
     (nmode->flags & MODE_NO_TEXT)!=(mode.flags & MODE_NO_TEXT)) {
    if(nmode->flags & MODE_NO_TEXT)
      gtk_widget_hide(digital_out);
    else
      gtk_widget_show(digital_out);
  }
  
  mode=*nmode;
}

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
  int tick, twidth, th;
  GdkFont *font;
  GdkColor *face_fg=CL_FACE_FG;

  /* Has the config changed? If we are an applet the only way to change our
     mode is to run as a full app and save the changed config, which we spot
     and read in
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

  /* Blank out to the background colour */
  gdk_gc_set_foreground(gc, CL_BACKGROUND);
  gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, w, h);

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

  /* Draw the clock face, including the hours */
  gdk_gc_set_foreground(gc, face_fg);
  gdk_draw_arc(pixmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0, 360*64);
  sw-=4;
  sh-=4;
  rad=sw/2;
  gdk_gc_set_foreground(gc, CL_FACE_BG);
  gdk_draw_arc(pixmap, gc, TRUE, x0-rad, y0-rad, sw, sh, 0, 360*64);
  font=canvas->style->font;
  if(mode.flags & MODE_HOURS) {
    int siz;
    
    twidth=gdk_text_width(font, "12", 2);
    siz=sw/twidth;

    if(siz<4)
      th=0;
    else if(siz<10)
      th=1;
    else
      th=2;
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

    if(th && (th==2 || tick%3==0)) {
      char buf[16];
      int l;
      int xt, yt;
      
      gdk_gc_set_foreground(gc, CL_HOUR_TEXT);
      sprintf(buf, "%d", tick==0? 12: tick);
      l=strlen(buf);
      xt=x2-gdk_text_width(font, buf, l)/2;
      yt=y2+gdk_text_height(font, buf, l)*cos(ang);
      gdk_draw_string(pixmap, font, gc, xt, yt, buf);
    }
  }

#if EXTRA_FUN
  if(sprite) {
    GdkGC *lgc;

    /*dprintf("state=%d\n", sprite_state);*/
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

#if FADING
    case T_APPEAR: case T_GO:
      dprintf("state=%d, frame=%d\n", sprite_state, sprite_frame);
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
#endif
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

  if(load_alarms)
    alarm_check();

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

/* Write the config to a file */
static void write_config(void)
{
  gchar *fname;

  fname=choices_find_path_save("options", PROJECT, TRUE);
  /*dprintf("save to %s\n", fname? fname: "NULL");*/

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

      fclose(out);

      config_time=now;
    }

    g_free(fname);
  }
}

/* Read in the config */
static void read_config(void)
{
  guchar *fname;

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
	      
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);

      if(nmode.init_size<16)
	nmode.init_size=16;
      set_mode(&nmode);
    }
#if SUPPORT_OLD_CONFIG
    else {
      read_old_config();
    }
#endif
    
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
  
  fname=choices_find_path_load("options", PROJECT);

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

  /*
  dprintf("format=%s, %s\n", nmode.format->name, nmode.format->fmt);
  dprintf("flags=%d, interval=%d\n", nmode.flags, nmode.interval);
  */
  
  set_mode(&nmode);
  
  if(GPOINTER_TO_INT(data))
    write_config();
  do_update();
  
  gtk_widget_hide(confwin);
}

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

  gtk_widget_show(confwin);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),       	NULL, show_conf_win, 0, NULL },
  { N_("/Alarms..."),		NULL, alarm_show_window, 0, NULL },
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0, NULL },
};

/* Create the pop-up menu */
static void menu_create_menu(GtkWidget *window)
{
  GtkItemFactory	*item_factory;
  GtkAccelGroup	*accel_group;
  gint 		n_items = sizeof(menu_items) / sizeof(*menu_items);

  accel_group = gtk_accel_group_new();
  gtk_accel_group_lock(accel_group);

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<system>", 
				      accel_group);

  gtk_item_factory_create_items(item_factory, n_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  menu = gtk_item_factory_get_widget(item_factory, "<system>");
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
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

#if EXTRA_FUN

static gboolean start_show_sprite(void);

#if FADING
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
#endif

static gboolean end_show_sprite(void)
{
#if FADING
  sprite_state=T_GO;
  sprite_frame=NFRAME-1;
  sprite_tag=gtk_timeout_add(CHANGE_FOR/NFRAME, (GtkFunction) fade_out_sprite,
			     NULL);
#else
  sprite_state=T_GONE;
  sprite_tag=gtk_timeout_add(NEXT_SHOW(), (GtkFunction) start_show_sprite,
			       NULL);
#endif
  
  return FALSE;
}

#if FADING
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
#endif

static gboolean start_show_sprite(void)
{
  int w, h;
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;
  if(w<sprite_width*2 || h<sprite_height*2)
    return TRUE;

  sprite_x=(rand()>>9)%(w-sprite_width);
  sprite_y=(rand()>>9)%(h-sprite_height);
  dprintf("%dx%d (%dx%d) -> %d,%d\n", w, h, sprite_width, sprite_height,
	 sprite_x, sprite_y);
#if FADING
  sprite_state=T_APPEAR;
  sprite_frame=0;
  sprite_tag=gtk_timeout_add(CHANGE_FOR/NFRAME, (GtkFunction) fade_in_sprite,
			     NULL);
#else
  sprite_state=T_SHOW;
  sprite_tag=gtk_timeout_add(SHOW_FOR, (GtkFunction) end_show_sprite,
			       NULL);
#endif
  
  return FALSE;
}

static void setup_sprite(void)
{
  GdkPixmap *pmap;
  time_t now;
#if FADING
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
#endif
  
  sprite=gdk_pixbuf_new_from_xpm_data((const char **) tardis_xpm);
  if(!sprite)
    return;
  
  dprintf("sprite=%p\n", sprite);
  sprite_width=gdk_pixbuf_get_width(sprite);
  sprite_height=gdk_pixbuf_get_height(sprite);
  dprintf("%dx%d\n", sprite_width, sprite_height);

#if FADING
  for(i=0; i<NFRAME; i++) {
    gdk_pixbuf_render_pixmap_and_mask(sprite, &pmap, masks+i, 127);
    dprintf("%d %p %p\n", i, pmap, masks[i]);
    gdk_pixmap_unref(pmap);

    if(masks[i]) {
      GdkGC *bgc=gdk_gc_new(masks[i]);
      
      dprintf("got mask %d, %p (gc=%p)\n", i, masks[i], bgc);
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
#endif
  
  dprintf("set up callback\n");
  time(&now);
  srand((unsigned int) now);
  sprite_tag=gtk_timeout_add(NEXT_SHOW(),
			     (GtkFunction) start_show_sprite, NULL);
			     
}

#endif

#if SUPPORT_OLD_CONFIG
/* Read in the config */
static void read_old_config(void)
{
  guchar *fname;

  fname=choices_find_path_load("config", PROJECT);

  if(fname) {
    FILE *in;

    in=fopen(fname, "r");
    if(in) {
#if defined(HAVE_SYS_STAT_H)
      struct stat statb;
#endif
      char buf[1024], *line;
      char *end;
      gchar *words;

#ifdef HAVE_FSTAT
      if(fstat(fileno(in), &statb)==0) {
	config_time=statb.st_mtime;
      }
#else
#ifdef HAVE_STAT
      if(stat(fname, &statb)==0) {
	config_time=statb.st_mtime;
      }
#else
      time(&config_time);
#endif
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

	    if(strcmp(var, "mode")==0) {
	      int i;

	      for(i=0; old_modes[i].name; i++)
		if(strcmp(val, old_modes[i].name)==0)
		  break;
	      if(old_modes[i].name) 
		set_mode(&old_modes[i].mode);
	      
	    } else if(strcmp(var, "user_format")==0) {
	      strncpy(user_defined, val, sizeof(user_defined));
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);
    }

    g_free(fname);
  }
}

#endif

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
