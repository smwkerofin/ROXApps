/*
 * %W% %E%
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include <locale.h>

#include <sys/stat.h>

#include <gtk/gtk.h>
#include "infowin.h"

#include "config.h"
#include "choices.h"

/* For SCCS (but ignored by gcc) */
#pragma ident "%W% %E%"

/* Flags for display mode */
#define MODE_SECONDS 1  /* Plot the second hand */
#define MODE_NO_TEXT 2  /* Don't show the text below the clock face */

typedef struct mode {
  const char *name;   /* User visible name for this mode */
  int flags;          /* Bit mask of flags defined above */
  const char *fmt;    /* Format to pass to strftime(3c) */
  guint32 interval;   /* Interval between updates (in ms) */
} Mode;

static char user_defined[256]="%X%n%x"; /* Buffer to hold the user format */

static Mode modes[]={
  {"24 hour", MODE_SECONDS, "%k:%M.%S", 500},
  {"24 hour, no seconds", 0, "%k:%M", 30000},
  {"12 hour", MODE_SECONDS, "%l:%M.%S", 500},
  {"12 hour, no seconds", 0, "%l:%M", 30000},
  {"Date", MODE_SECONDS, "%x", 500},
  {"Date, no second hand", 0, "%x", 30000},
  {"Full", MODE_SECONDS, "%c", 500},
  {"Full, no second hand", 0, "%c", 500},
  {"None", MODE_NO_TEXT|MODE_SECONDS, "", 500},
  {"None, no second hand", MODE_NO_TEXT, "", 30000},
  {"User defined", MODE_SECONDS, user_defined, 500},
  {"User defined, no second hand", 0, user_defined, 500},

  {NULL, 0, NULL, 0}
};
static Mode *mode=modes;

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
  {0, 0x8888, 0x8888, 0x8888}, /* This gets replaced with the gtk background*/
  {0, 0, 0, 0}
};
enum {WHITE, RED, GREEN, BLUE, GREY, BLACK};
#define NUM_COLOUR (BLACK+1)

static GtkWidget *confwin=NULL;     /* Window for configuring */
static GtkWidget *mode_sel=NULL;    /* Selects a display mode */
static GtkWidget *user_fmt=NULL;    /* Entering the user-defined format */

static guint update_tag;            /* Handle for the timeout */
static time_t config_time=0;        /* Time our config file was last changed */

static void do_update(void);        /* Update clock face and text out */
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event);
                                    /* Window resized */
static void menu_create_menu(GtkWidget *window);
                                    /* create the pop-up menu */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on canvas */
static void show_info_win(void);
static void show_conf_win(void);

static void read_config(void);
static void write_config(void);
static void check_config(void);

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
  setlocale(LC_TIME, "");

  /* Initialise X/GDK/GTK */
  gtk_init(&argc, &argv);

  /* Init choices and read them in */
  choices_init();
  read_config();

  /*printf("argc=%d\n", argc);*/
  if(argc<2 || !atol(argv[1])) {
    /* No arguments, or the first argument was not a (non-zero) number.
       We are not an applet, so create a window */
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "clock");
    gtk_window_set_title(GTK_WINDOW(win), "Clock");
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_widget_set_usize(win, 64, 64);

    /* We want to pop up a menu on a button press */
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_realize(win);

  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;

    /*printf("argv[1]=%s\n", argv[1]);*/
    plug=gtk_plug_new(atol(argv[1]));
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_widget_set_usize(plug, 64, 64);

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

  cmap=gdk_colormap_get_system();
  for(i=0; i<NUM_COLOUR; i++)
    gdk_color_alloc(cmap, colours+i);

  canvas=gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(canvas), 64, 48);
  /* next line causes trouble??  maybe just as an applet... */
  gtk_widget_set_events (canvas, GDK_EXPOSURE_MASK);
  gtk_box_pack_start (GTK_BOX (vbox), canvas, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (canvas), "expose_event",
		      (GtkSignalFunc) do_update, NULL);
  gtk_signal_connect (GTK_OBJECT (canvas), "configure_event",
		      (GtkSignalFunc) configure_event, NULL);
  gtk_widget_realize(canvas);
  gtk_widget_show (canvas);
  
  gc=gdk_gc_new(canvas->window);

  time(&now);
  strftime(buf, 80, mode->fmt, localtime(&now));

  /* This is the text below the clock face */
  digital_out=gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), digital_out, FALSE, FALSE, 2);
  if(!(mode->flags & MODE_NO_TEXT))
    gtk_widget_show(digital_out);

  if(win)
    gtk_widget_show(win);

  /* Make sure we get called periodically */
  update_tag=gtk_timeout_add(mode->interval,
			 (GtkFunction) do_update, digital_out);
  
  gtk_main();

  return 0;
}

/* Called when the display mode changes */
static void set_mode(Mode *nmode)
{
  /* Is this a no-op? */
  if(mode==nmode)
    return;

  /*printf("mode now %s\n", nmode->name);*/

  /* Do we need to change the update rate? */
  if(update_tag && mode->interval!=nmode->interval) {
    gtk_timeout_remove(update_tag);
    update_tag=gtk_timeout_add(nmode->interval,
			       (GtkFunction) do_update, digital_out);
    /*printf("tag now %u (%d)\n", update_tag, nmode->interval);*/
  }

  /* Change visibility of text line? */
  if((nmode->flags & MODE_NO_TEXT)!=(mode->flags & MODE_NO_TEXT)) {
    if(nmode->flags & MODE_NO_TEXT)
      gtk_widget_hide(digital_out);
    else
      gtk_widget_show(digital_out);
  }
  
  mode=nmode;
}

/* Redraw clock face and update digital_out */
static void do_update(void)
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
  int tick;

  /* Has the config changed? If we are an applet the only way to change our
     mode is to run as a full app and save the changed config, which we spot
     and read in
  */
  check_config();

  time(&now);
  tms=localtime(&now);
    
  if(digital_out && !(mode->flags & MODE_NO_TEXT)) {
    strftime(buf, 80, mode->fmt, tms);
    
    gtk_label_set_text(GTK_LABEL(digital_out), buf);
  }

  if(!gc || !canvas || !pixmap)
    return;
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;

  /* Blank out to the background colour */
  gdk_gc_set_foreground(gc, colours+GREY);
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
  gdk_gc_set_foreground(gc, colours+WHITE);
  gdk_draw_arc(pixmap, gc, TRUE, x0-sw/2, y0-sh/2, sw, sh, 0, 360*64);
  gdk_gc_set_foreground(gc, colours+BLUE);
  gdk_draw_arc(pixmap, gc, FALSE, x0-sw/2, y0-sh/2, sw, sh, 0, 360*64);
  for(tick=0; tick<12; tick++) {
    double ang=tick*2*M_PI/12.;
    double x2=x0+rad*sin(ang)*7/8;
    double y2=y0-rad*cos(ang)*7/8;
    x1=x0+rad*sin(ang);
    y1=y0-rad*cos(ang);
    gdk_draw_line(pixmap, gc, x2, y2, x1, y1);
  }

  /* Draw the hands */
  gdk_gc_set_foreground(gc, colours+BLACK);
  x1=x0+rad*sin(h_ang)/2;
  y1=y0-rad*cos(h_ang)/2;
  gdk_draw_line(pixmap, gc, x0, y0, x1, y1);
  x1=x0+rad*sin(m_ang);
  y1=y0-rad*cos(m_ang);
  gdk_draw_line(pixmap, gc, x0, y0, x1, y1);
  if(mode->flags & MODE_SECONDS) {
    gdk_gc_set_foreground(gc, colours+RED);
    x1=x0+rad*sin(s_ang);
    y1=y0-rad*cos(s_ang);
    gdk_draw_line(pixmap, gc, x0, y0, x1, y1);
  }

  /* Copy the pixmap to the display canvas */
  gdk_draw_pixmap(canvas->window,
		  canvas->style->fg_gc[GTK_WIDGET_STATE (canvas)],
		  pixmap,
		  0, 0,
		  0, 0,
		  w, h);
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

  fname=choices_find_path_save("config", PROJECT, TRUE);
  /*printf("save to %s\n", fname? fname: "NULL");*/

  if(fname) {
    FILE *out;

    out=fopen(fname, "w");
    if(out) {
      time_t now;
      char buf[80];
      
      fprintf(out, "# Config file for %s %s (%s)\n", PROJECT, VERSION, AUTHOR);
      fprintf(out, "# Latest version at %s\n", WEBSITE);
      time(&now);
      strftime(buf, 80, "%c", localtime(&now));
      fprintf(out, "#\n# Written %s\n\n", buf);

      fprintf(out, "mode=%s # (%#x, \"%s\", %d)\n", mode->name, mode->flags,
	      mode->fmt, mode->interval);
      fprintf(out, "user_format=%s\n", user_defined);

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

  fname=choices_find_path_load("config", PROJECT);

  if(fname) {
    FILE *in;

    in=fopen(fname, "r");
    if(in) {
      struct stat statb;
      char buf[1024], *line;
      char *end;
      gchar *words;

      if(fstat(fileno(in), &statb)==0) {
	config_time=statb.st_mtime;
      }

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

	      for(i=0; modes[i].name; i++)
		if(strcmp(val, modes[i].name)==0)
		  break;
	      if(modes[i].name) 
		set_mode(modes+i);
	      
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

/* Check timestamp of config file against time we last read it, if file
   has changed, call read_config() */
static void check_config(void)
{
  gchar *fname;

  if(config_time==0) {
    read_config();
    return;
  }
  
  fname=choices_find_path_load("config", PROJECT);

  if(fname) {
    struct stat statb;

    if(stat(fname, &statb)==0) {
      if(statb.st_mtime>config_time)
	read_config();
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

/* User cancels a change of config */
static void cancel_config(GtkWidget *widget, gpointer data)
{
  int set;

  /* Reset window contents */
  set=mode-modes;
  gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel), set);
  gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);

  /* Hide window */
  gtk_widget_hide(confwin);
}

static void set_config(GtkWidget *widget, gpointer data)
{
  GtkWidget *menu;
  GtkWidget *item;
  gchar *udef;

  /* get data from window contents */
  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(mode_sel));
  item=gtk_menu_get_active(GTK_MENU(menu));
  set_mode((Mode *) gtk_object_get_data(GTK_OBJECT(item), "mode"));
  udef=gtk_entry_get_text(GTK_ENTRY(user_fmt));
  strncpy(user_defined, udef, sizeof(user_defined));
  
  if(GPOINTER_TO_INT(data))
    write_config();
  do_update();
  
  gtk_widget_hide(confwin);
}

/* Show the configure window */
static void show_conf_win(void)
{
  if(!confwin) {
    /* Need to create it first */
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *but;
    GtkWidget *label;
    GtkWidget *menu;
    GtkWidget *item;
    GtkRequisition req;
    int mw=0, mh=0;
    int i, set=0;

    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), "Configuration");

    vbox=GTK_DIALOG(confwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Mode");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    mode_sel=gtk_option_menu_new();
    gtk_widget_show(mode_sel);
    gtk_box_pack_start(GTK_BOX(hbox), mode_sel, TRUE, TRUE, 2);

    menu=gtk_menu_new();
    gtk_option_menu_set_menu(GTK_OPTION_MENU(mode_sel), menu);

    for(i=0; modes[i].name; i++) {
      item=gtk_menu_item_new_with_label(modes[i].name);
      if(mode==modes+i)
	set=i;
      gtk_object_set_data(GTK_OBJECT(item), "mode", modes+i);
      gtk_widget_show(item);
      gtk_widget_size_request(item, &req);
      if(mw<req.width)
	mw=req.width;
      if(mh<req.height)
	mh=req.height;
      gtk_menu_append(GTK_MENU(menu), item);
    }
    gtk_widget_set_usize(mode_sel, mw+50, mh+4);
    gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel), set);
    
    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("User defined format");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    user_fmt=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);
    gtk_widget_show(user_fmt);
    gtk_box_pack_start(GTK_BOX(hbox), user_fmt, TRUE, TRUE, 2);    

    hbox=GTK_DIALOG(confwin)->action_area;

    but=gtk_button_new_with_label("Cancel");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked",
		       GTK_SIGNAL_FUNC(cancel_config), confwin);

    but=gtk_button_new_with_label("Set");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       GINT_TO_POINTER(FALSE));

    but=gtk_button_new_with_label("Save");
    gtk_widget_show(but);
    gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
    gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(set_config),
		       GINT_TO_POINTER(TRUE));

  }

  /* Initialise values */
  gtk_option_menu_set_history(GTK_OPTION_MENU(mode_sel), (int) (mode-modes));
  gtk_entry_set_text(GTK_ENTRY(user_fmt), user_defined);

  gtk_widget_show(confwin);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { "/Info",		NULL, show_info_win, 0, NULL },
  { "/Configure",		NULL, show_conf_win, 0, NULL },
  { "/Quit",	NULL, gtk_main_quit, 0, NULL },
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
    /* Pop up the menu */
    if(!menu)
      menu_create_menu(GTK_WIDGET(win));

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

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

