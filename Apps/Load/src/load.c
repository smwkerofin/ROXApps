/*
 * Plot system load average
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: load.c,v 1.5 2001/07/06 08:19:55 stephen Exp $
 *
 * Log at end of file
 */

#include "config.h"

#define APPLET_MENU        1
#define DEBUG              1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include "infowin.h"

#include <glibtop.h>
#include <glibtop/cpu.h>
#include <glibtop/loadavg.h>

/*#include "rox-clib.h"*/
#include "choices.h"
#include "rox_debug.h"

static GtkWidget *menu=NULL;
static GtkWidget *infowin=NULL;
static GtkWidget *canvas = NULL;
static GdkPixmap *pixmap = NULL;
static GdkGC *gc=NULL;
static GdkColormap *cmap = NULL;
static GdkColor colours[]={
  {0, 0xffff,0xffff,0xffff},
  {0, 0xffff,0,0},
  {0, 0, 0xcccc, 0},
  {0, 0,0,0xffff},
  {0, 0x8888, 0x8888, 0x8888},
  {0, 0, 0, 0}
};
enum {COL_WHITE, COL_HIGH, COL_NORMAL, COL_BLUE, COL_BG, COL_FG};
#define NUM_COLOUR (sizeof(colours)/sizeof(colours[0]))
#define BAR_WIDTH 16
#define BAR_GAP    4
#define BOTTOM_MARGIN 12
#define TOP_MARGIN 10
#define MIN_WIDTH 36
#define MIN_HEIGHT MIN_WIDTH
#define MAX_BAR_WIDTH 32

typedef struct options {
  guint32 update_ms;
  gboolean show_max;
  gboolean show_vals;
  gboolean show_host;
  guint init_size;
} Options;

static Options options={
  2000, TRUE, FALSE, FALSE, MIN_WIDTH
};

typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *show_max;
  GtkWidget *show_vals;
  GtkWidget *show_host;
  GtkWidget *init_size;
} OptionWidgets;

static glibtop *server=NULL;
static double max_load=1.0;
static double red_line=1.0;
static int reduce_delay=10;
static guint update=0;
static time_t config_time=0;
static double *history=NULL;
static int nhistory=0;
static int ihistory=0;
static gboolean is_applet;
#ifdef HAVE_GETHOSTNAME
static char hostname[256];
#endif

static void do_update(void);
static gint configure_event(GtkWidget *widget, GdkEventConfigure *event);
static void menu_create_menu(GtkWidget *window);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win);
static void show_info_win(void);

static void read_config(void);
static void write_config(void);
static void check_config(void);

int main(int argc, char *argv[])
{
  GtkWidget *win=NULL;
  GtkWidget *vbox;
  GtkStyle *style;
  int w=MIN_WIDTH, h=MIN_HEIGHT;
  int i;

  rox_debug_init("Load");

#ifdef HAVE_GETHOSTNAME
  if(gethostname(hostname, sizeof(hostname))<0) {
    hostname[0]=0;
    dprintf(1, "couldn't get hostname: %s", strerror(errno));
    errno=0;
  } else {
    dprintf(1, "Running on %s", hostname);
  }
#endif

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
  
  choices_init();
  dprintf(1, "read config");
  read_config();
  w=options.init_size;
  h=options.init_size;

  if(argc<2 || !atol(argv[1])) {
    dprintf(2, "make window");
    is_applet=FALSE;
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "load");
    gtk_window_set_title(GTK_WINDOW(win), "Load");
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    
    dprintf(3, "set size to %d,%d", w, h);
    gtk_widget_set_usize(win, w, h);
    /*gtk_widget_realize(win);*/
    
  } else {
    GtkWidget *plug;

    dprintf(2, "argv[1]=%s", argv[1]);
    plug=gtk_plug_new(atol(argv[1]));
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_widget_set_usize(plug, w, h);
    
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), plug);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    
    win=plug;
    is_applet=TRUE;
  }
  
  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  style=gtk_widget_get_style(vbox);
  colours[COL_BG]=style->bg[GTK_STATE_NORMAL];

  for(i=0; i<NUM_COLOUR; i++) {
    gdk_color_alloc(cmap, colours+i);
    dprintf(3, "colour %d: %4x, %4x, %4x: %ld", i, colours[i].red,
	   colours[i].green, colours[i].blue, colours[i].pixel);
  }

  canvas=gtk_drawing_area_new();
  gtk_drawing_area_size (GTK_DRAWING_AREA(canvas), w-2, h-2);
  gtk_box_pack_start (GTK_BOX (vbox), canvas, TRUE, TRUE, 0);
  gtk_widget_show (canvas);
  gtk_signal_connect (GTK_OBJECT (canvas), "expose_event",
		      (GtkSignalFunc) do_update, NULL);
  gtk_signal_connect (GTK_OBJECT (canvas), "configure_event",
		      (GtkSignalFunc) configure_event, NULL);
  gtk_widget_set_events (canvas, GDK_EXPOSURE_MASK);
  gtk_widget_realize(canvas);
  gc=gdk_gc_new(canvas->window);
  gdk_gc_set_background(gc, colours+COL_BG);

  gtk_widget_show(win);

  update=gtk_timeout_add(options.update_ms, (GtkFunction) do_update, NULL);
  
  gtk_main();

  return 0;
}

static void do_update(void)
{
  int w, h;
  int bw, mbh, bh, bm=BOTTOM_MARGIN, tm=TOP_MARGIN;
  int bx, by, x, sbx;
  int cpu, other;
  double ld;
  GdkFont *font;
  glibtop_loadavg load;
  int i;
  char buf[32];
  int reduce;
  int ndec=1, host_x=0;

  dprintf(4, "gc=%p canvas=%p pixmap=%p", gc, canvas, pixmap);

  if(!gc || !canvas || !pixmap)
    return;

  check_config();
  
  h=canvas->allocation.height;
  w=canvas->allocation.width;
  font=canvas->style->font;
  
  gdk_gc_set_foreground(gc, colours+COL_BG);
  gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, w, h);

  bx=2;
  bw=(w-2-3*BAR_GAP)/3;
  if(options.show_vals) {
    int width, height;

    width=gdk_char_width(font, '0');
    height=gdk_char_height(font, '0');

    ndec=(bw/width)-2;
    if(ndec<1)
      ndec=1;
    if(ndec>3)
      ndec=3;

    tm=height+2;
    bm=height+4;
  }
  by=h-bm;
  if(by<48) 
    by=h-2;
  mbh=by-tm;
  if(mbh<48)
    mbh=by-2;
  if(bw>MAX_BAR_WIDTH) {
    bw=MAX_BAR_WIDTH;
    bx=w-2-3*bw-3*BAR_GAP-1;
  }
  dprintf(4, "w=%d bw=%d", w, bw);
  sbx=bx;

  glibtop_get_loadavg_l(server, &load);
  if(history && nhistory>0)
    history[(ihistory++)%nhistory]=load.loadavg[0];

  reduce=TRUE;
  for(i=0; i<3; i++) {
    if(load.loadavg[i]>max_load/2)
      reduce=FALSE;
    while(max_load<load.loadavg[i]) {
      max_load*=2;
      reduce_delay=10;
    }
  }
  if(reduce && max_load>=2.0 && history)
    for(i=0; i<nhistory && i<ihistory; i++)
      if(history[(ihistory-i-1)%nhistory]>max_load/2)
	reduce=FALSE;
  if(reduce && max_load>=2.0) {
    if(reduce_delay--<1)
      max_load/=2;
  }
  /* Correct for FP inaccuracy, if needed */
  if(max_load<1.0)
    max_load=1.0;

  for(i=0; i<3; i++) {
    ld=load.loadavg[i];
    bh=mbh*ld/max_load;
    gdk_gc_set_foreground(gc, colours+COL_NORMAL);
    gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, bw, bh);
    
    dprintf(4, "load=%f max_load=%f, mbh=%d, by=%d, bh=%d, by-bh=%d",
	   ld, max_load, mbh, by, bh, by-bh);
    dprintf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bh);
    
    
    if(ld>red_line) {
      int bhred=mbh*(ld-red_line)/max_load;
      
      dprintf(5, "bhred=%d by-bhred=%d", bhred, by-bhred);
      dprintf(5, "(%d, %d) by (%d, %d)", bx, by-bh, bw, bhred);
      
      gdk_gc_set_foreground(gc, colours+COL_HIGH);
      gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, bw, bhred);
    }
    gdk_gc_set_foreground(gc, colours+COL_FG);
    gdk_draw_rectangle(pixmap, gc, FALSE, bx, by-bh, bw, bh);

    if(options.show_vals) {
      sprintf(buf, "%3.*f", ndec, ld);
      gdk_draw_string(pixmap, font, gc, bx+2, h-2, buf);
    }

    bx+=bw+BAR_GAP;
  }

  if(history && ihistory>0) {
    bx=sbx-BAR_GAP;
    for(i=0; i<nhistory && i<ihistory && bx>=BAR_GAP; i++, bx-=2) {
      ld=history[(ihistory-i-1)%nhistory];
      bh=mbh*ld/max_load;
      gdk_gc_set_foreground(gc, colours+COL_NORMAL);
      gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, 2, bh);
      if(ld>red_line) {
	int bhred=mbh*(ld-red_line)/max_load;
	gdk_gc_set_foreground(gc, colours+COL_HIGH);
	gdk_draw_rectangle(pixmap, gc, TRUE, bx, by-bh, 2, bhred);
      }
    }
  }

  if(options.show_max) {
    sprintf(buf, "Max %2d", (int) max_load);
    gdk_gc_set_foreground(gc, colours+COL_FG);
    gdk_draw_string(pixmap, font, gc, 2, tm, buf);

    host_x=2+gdk_string_width(font, buf)+4;
  }

  if(options.show_host) {
    const char *host=hostname;
    
    if(server->server_host)
      host=server->server_host;
    
    x=w-gdk_string_width(font, host)-2;
    if(x<host_x)
      x=host_x;
    gdk_gc_set_foreground(gc, colours+COL_FG);
    gdk_draw_string(pixmap, font, gc, x, tm, host);
  }
  
    
  gdk_draw_pixmap(canvas->window,
		  canvas->style->fg_gc[GTK_WIDGET_STATE (canvas)],
		  pixmap,
		  0, 0,
		  0, 0,
		  w, h);
}

static void resize_history(int width)
{
  int nrec;
  double *tmp;
  int i;

  nrec=(width-2-3*MAX_BAR_WIDTH-3*BAR_GAP-2-1)/2;

  if(nrec<=nhistory)
    return;

  if(ihistory<nhistory) {
    tmp=g_realloc(history, nrec*sizeof(double));
    if(!tmp)
      return;

    history=tmp;
    nhistory=nrec;
    
    for(i=ihistory; i<nhistory; i++)
      history[i]=0.;
    
    return;
  }

  if(!nhistory) {
    history=g_new(double, nrec);
    nhistory=nrec;
    ihistory=0;
    return;
  }

  tmp=g_new(double, nrec);
  i=nhistory-(ihistory%nhistory);
  if(i==nhistory)
    i=0;
  g_memmove(tmp, history+(ihistory%nhistory), i*sizeof(double));
  g_memmove(tmp+i, history, (nhistory-i)*sizeof(double));

  g_free(history);
  history=tmp;
  ihistory=nhistory;
  nhistory=nrec;
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
  resize_history(widget->allocation.width);
  do_update();

  return TRUE;
}

static void write_config(void)
{
  gchar *fname;

  fname=choices_find_path_save("config", PROJECT, TRUE);
  dprintf(1, "save to %s", fname? fname: "NULL");

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

      fprintf(out, "update_ms=%d\n", options.update_ms);
      fprintf(out, "show_max=%d\n", options.show_max);
      fprintf(out, "show_vals=%d\n", options.show_vals);
      fprintf(out, "show_host=%d\n", options.show_host);
      fprintf(out, "init_size=%d\n", (int) options.init_size);

      fclose(out);

      config_time=now;
    }

    g_free(fname);
  }
}

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
	      options.update_ms=atoi(val);

	      if(update) {
		gtk_timeout_remove(update);
		update=gtk_timeout_add(options.update_ms,
				       (GtkFunction) do_update, NULL);
	      }
	      
	    } else if(strcmp(var, "show_max")==0) {
	      options.show_max=atoi(val);
	    } else if(strcmp(var, "show_vals")==0) {
	      options.show_vals=atoi(val);
	    } else if(strcmp(var, "show_host")==0) {
	      options.show_host=atoi(val);
	    } else if(strcmp(var, "init_size")==0) {
	      options.init_size=(guint) atoi(val);
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);
    }

    g_free(fname);
  }
}

static void check_config(void)
{
  guchar *fname;

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
  gfloat s;

  s=gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(ow->update_s));
  options.update_ms=(guint) (s*1000);
  options.show_max=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_max));
  options.show_vals=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_vals));
  options.show_host=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_host));
  options.init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));
  
  gtk_timeout_remove(update);
  update=gtk_timeout_add(options.update_ms,
				     (GtkFunction) do_update, NULL);

  gtk_widget_hide(ow->window);
}

static void save_config(GtkWidget *widget, gpointer data)
{
  set_config(widget, data);
  write_config();
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

    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), "Configuration");
    gtk_window_set_position(GTK_WINDOW(confwin), GTK_WIN_POS_MOUSE);
    ow.window=confwin;

    vbox=GTK_DIALOG(confwin)->vbox;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Update rate (s)");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    range=gtk_adjustment_new((gfloat)(options.update_ms/1000.),
			     0.2, 60., 0.1, 1., 1.);
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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), options.show_max);
    ow.show_max=check;

    check=gtk_check_button_new_with_label("Show values");
    gtk_widget_set_name(check, "show_vals");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), options.show_vals);
    ow.show_vals=check;

    check=gtk_check_button_new_with_label("Show hostname");
    gtk_widget_set_name(check, "show_host");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), options.show_host);
    ow.show_host=check;

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new("Initial size");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

    range=gtk_adjustment_new((gfloat) options.init_size,
			     16, 128, 2, 16, 16);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 0);
    gtk_widget_set_name(spin, "init_size");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 2);
    ow.init_size=spin;

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
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.update_s),
			      (gfloat)(options.update_ms/1000.));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_max),
				 options.show_max);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_vals),
				 options.show_vals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_host),
				 options.show_host);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.init_size),
			      (gfloat) options.init_size);
  }
  
  gtk_widget_show(confwin);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { "/Info",	NULL, show_info_win, 0, NULL },
  { "/Configure",	NULL, show_config_win, 0, NULL },
  { "/Quit",	NULL, gtk_main_quit, 0, NULL },
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
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3
#if !APPLET_MENU
     && !is_applet
#endif
     ) {
    if(!menu)
      menu_create_menu(GTK_WIDGET(win));

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  } else if(is_applet && bev->type==GDK_BUTTON_PRESS && bev->button==1) {
    gchar *cmd;

    cmd=g_strdup_printf("%s/AppRun &", getenv("APP_DIR"));
    system(cmd);
    g_free(cmd);
      
    return TRUE;
  }

  return FALSE;
}

static void show_info_win(void)
{
  if(!infowin) {
    infowin=info_win_new(PROGRAM, PURPOSE, VERSION, AUTHOR, WEBSITE);
  }

  gtk_widget_show(infowin);
}

/*
 * $Log: load.c,v $
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
