/*
 * Mem - Monitor memory and swap usage
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: mem.c,v 1.1.1.1 2001/08/24 11:08:30 stephen Exp $
 */
#include "config.h"

/* Select compilation options */
#define DEBUG              1     /* Set to 1 for debug messages */

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
#include <glibtop/mem.h>
#include <glibtop/swap.h>

#include "choices.h"
#include "infowin.h"
#include "rox_debug.h"

/* The icon */
#include "../AppIcon.xpm"

#define TIP_PRIVATE "For more information see the help file"

/* Can we support swap?  On Solaris, versions of libgtop up to 1.1.2 don't
   read swap figures correctly */
#define SOLARIS_SWAP_BROKEN_UP_TO 1001002

#if defined(__sparc__) && defined(__svr4__)
#if LIBGTOP_VERSION_CODE>SOLARIS_SWAP_BROKEN_UP_TO
#define SWAP_SUPPORTED 1 
#else
#define SWAP_SUPPORTED 0
#endif
#else
#define SWAP_SUPPORTED 1
#endif

/* GTK+ objects */
static GtkWidget *win=NULL;
static GtkWidget *mem_total, *mem_used, *mem_free=NULL, *mem_per;
#if SWAP_SUPPORTED
static GtkWidget *swap_total, *swap_used, *swap_free, *swap_per,
  *swap_total_label;
#endif
static GtkWidget *menu=NULL;
static guint update_tag=0;
typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *init_size;
} OptionWidgets;

typedef struct options {
  guint update_sec;          /* How often to update */
  guint applet_init_size;    /* Initial size of applet */
  guint swap_nums;           /* Show the swap numbers - IGNORED */
} Options;

static Options options={
  5,
  36,
  TRUE
};

/* Call backs & stuff */
static gboolean update_values(gpointer);
static void do_update(void);
static void read_choices(void);
static void write_choices(void);
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);

int main(int argc, char *argv[])
{
  GtkWidget *vbox, *hbox, *vbox2, *frame;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkTooltips *ttips;
  char tbuf[1024], *home;
  guchar *fname;
  int c;
  guint xid=0;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif

  rox_debug_init(PROJECT);
  
  dprintf(5, "%d %s -> %s\n", argc, argv[1], argv[argc-1]);
  gtk_init(&argc, &argv);
  dprintf(5, "%d %s -> %s\n", argc, argv[1], argv[argc-1]);
  ttips=gtk_tooltips_new();

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=choices_find_path_load("gtkrc", PROJECT);
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  while((c=getopt(argc, argv, "a:"))!=EOF) {
    dprintf(5, " %2d -%c %s\n", c, c, optarg? optarg: "NULL");
    switch(c) {
    case 'a':
      xid=atol(optarg);
      break;
    }
  }

  if(!xid) {
    /* Full window mode */
    /* Construct our window and bits */
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "mem free");
    gtk_window_set_title(GTK_WINDOW(win), "System memory");
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), NULL);
    gtk_widget_realize(win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);

    /* Set the icon */
    style = gtk_widget_get_style(win);
    pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(win)->window,  &mask,
					  &style->bg[GTK_STATE_NORMAL],
					  (gchar **)AppIcon_xpm );
    gdk_window_set_icon(GTK_WIDGET(win)->window, NULL, pixmap, mask);

    gtk_tooltips_set_tip(ttips, win,
			 "Mem shows the memory"
#if SWAP_SUPPORTED
			 " and swap usage"
#endif
			 " on a host",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show(vbox);

    frame=gtk_frame_new(_("Memory"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 2);
    gtk_widget_show(frame);
  
    vbox2=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    gtk_widget_show(vbox2);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    mem_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mem_total, FALSE, FALSE, 2);
    gtk_widget_show(mem_total);
    gtk_tooltips_set_tip(ttips, mem_total,
			 "This is the total size of memory",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    mem_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mem_used, TRUE, FALSE, 2);
    gtk_widget_show(mem_used);
    gtk_tooltips_set_tip(ttips, mem_used,
			 "This is the memory used on the host",
			 TIP_PRIVATE);
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    mem_per=gtk_progress_bar_new();
    gtk_widget_set_name(mem_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mem_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mem_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mem_per), TRUE);
    gtk_widget_set_usize(mem_per, 240, 24);
    gtk_widget_show(mem_per);
    gtk_box_pack_start(GTK_BOX(hbox), mem_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mem_per,
			 "This shows the relative usage of memory",
			 TIP_PRIVATE);
  
    mem_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), mem_free, TRUE, FALSE, 2);
    gtk_widget_show(mem_free);
    gtk_tooltips_set_tip(ttips, mem_free,
			 "This is the memory available on the host",
			 TIP_PRIVATE);

#if SWAP_SUPPORTED
    frame=gtk_frame_new(_("Swap"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 2);
    gtk_widget_show(frame);
  
    vbox2=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    gtk_widget_show(vbox2);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);
    swap_total_label=label;

    swap_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), swap_total, FALSE, FALSE, 2);
    gtk_widget_show(swap_total);
    gtk_tooltips_set_tip(ttips, swap_total,
			 "This is the total size of swap space",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    swap_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), swap_used, TRUE, FALSE, 2);
    gtk_widget_show(swap_used);
    gtk_tooltips_set_tip(ttips, swap_used,
			 "This is the swap space used on the host",
			 TIP_PRIVATE);
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    swap_per=gtk_progress_bar_new();
    gtk_widget_set_name(swap_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(swap_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(swap_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(swap_per), TRUE);
    gtk_widget_set_usize(swap_per, 240, 24);
    gtk_widget_show(swap_per);
    gtk_box_pack_start(GTK_BOX(hbox), swap_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, swap_per,
			 "This shows the relative usage of swap space",
			 TIP_PRIVATE);
  
    swap_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), swap_free, TRUE, FALSE, 2);
    gtk_widget_show(swap_free);
    gtk_tooltips_set_tip(ttips, swap_free,
			 "This is the swap space available on the host",
			 TIP_PRIVATE);

#endif
    
    menu_create_menu(win);
  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;
    int i;
    
    plug=gtk_plug_new(xid);
    gtk_signal_connect(GTK_OBJECT(plug), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_widget_set_usize(plug, options.applet_init_size,
			 options.applet_init_size);
    dprintf(4, "set_usize %d\n", options.applet_init_size);
    
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), NULL);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    gtk_tooltips_set_tip(ttips, plug,
			 "Mem shows the memory and swap usage",
			 TIP_PRIVATE);

    dprintf(4, "vbox new\n");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    mem_per=gtk_progress_bar_new();
    gtk_widget_set_name(mem_per, "gauge");
    /*gtk_widget_set_usize(mem_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mem_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mem_per), "M %p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mem_per), TRUE);
    gtk_widget_show(mem_per);
    gtk_box_pack_start(GTK_BOX(vbox), mem_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mem_per,
			 "This shows the relative usage of memory",
			 TIP_PRIVATE);

#if SWAP_SUPPORTED
    swap_per=gtk_progress_bar_new();
    gtk_widget_set_name(swap_per, "gauge");
    /*gtk_widget_set_usize(swap_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(swap_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(swap_per), "S %p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(swap_per), TRUE);
    gtk_widget_show(swap_per);
    gtk_box_pack_start(GTK_BOX(vbox), swap_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, swap_per,
			 "This shows the relative usage of swap space",
			 TIP_PRIVATE);
#else
    mem_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_free, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mem_free, FALSE, FALSE, 2);
    gtk_widget_show(mem_free);
    gtk_tooltips_set_tip(ttips, mem_free,
			 "This is the memory available on the host",
			 TIP_PRIVATE);

#endif
    
    menu_create_menu(plug);

    dprintf(5, "show plug\n");
    gtk_widget_show(plug);
    dprintf(5, "made applet\n");
  }

  /* Set up glibtop and check now */
  dprintf(4, "set up glibtop\n");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_MEM)
#if SWAP_SUPPORTED
		 |(1<<GLIBTOP_SYSDEPS_SWAP)
#endif
		 , 0);
  update_values(NULL);

  dprintf(3, "show %p (%ld)\n", win, xid);
  if(!xid)
    gtk_widget_show(win);
  dprintf(2, "timeout %ds, %p, %p\n", options.update_sec,
	  (GtkFunction) update_values, NULL);
  update_tag=gtk_timeout_add(options.update_sec*1000,
			 (GtkFunction) update_values, NULL);
  dprintf(2, "update_sec=%d, update_tag=%u\n", options.update_sec,
	  update_tag);
  
  gtk_main();

  return 0;
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

#if SWAP_SUPPORTED
#define SWAP_FLAGS ((1<<GLIBTOP_SWAP_TOTAL)|(1<<GLIBTOP_SWAP_USED)|(1<<GLIBTOP_SWAP_FREE))
#define SWAP_FIX 1
#endif

static gboolean update_values(gpointer unused)
{
  int ok=FALSE;
  glibtop_mem mem;
#if SWAP_SUPPORTED
  glibtop_swap swap;
#endif
  
  dprintf(4, "update_sec=%d, update_tag=%u\n", options.update_sec,
	  update_tag);
  
  errno=0;
  glibtop_get_mem(&mem);
  ok=(errno==0) && (mem.flags & MEM_FLAGS)==MEM_FLAGS;
    
  if(ok) {
    unsigned long long total, used, avail;
    gfloat fused;
      
    total=mem.total;
    used=mem.total-mem.free;
    avail=mem.free;
      
    fused=(100.f*used)/((gfloat) total);
    if(fused>100.f)
      fused=100.f;

    dprintf(4, "%2.0f %%\n", fused);

    if(win) {
      gtk_label_set_text(GTK_LABEL(mem_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mem_used), fmt_size(used));
      
    }
    if(mem_free)
      gtk_label_set_text(GTK_LABEL(mem_free), fmt_size(avail));
    dprintf(5, "set progress\n");
    gtk_progress_set_value(GTK_PROGRESS(mem_per), fused);
    
  } else {
    if(win) {
      gtk_label_set_text(GTK_LABEL(mem_total), "?");
      gtk_label_set_text(GTK_LABEL(mem_used), "?");
    }
    if(mem_free)
      gtk_label_set_text(GTK_LABEL(mem_free), "Free?");
    gtk_progress_set_value(GTK_PROGRESS(mem_per), 0.);
  }

#if SWAP_SUPPORTED
  errno=0;
  glibtop_get_swap(&swap);
  ok=(errno==0) && (swap.flags & SWAP_FLAGS)==SWAP_FLAGS;
    
  if(ok) {
    unsigned long long total, used, avail;
    gfloat fused;
      
    total=swap.total*SWAP_FIX;
    used=swap.used*SWAP_FIX;
    avail=swap.free*SWAP_FIX;
    dprintf(3, "%llx: %lld %lld %lld, %lld %lld", swap.flags, swap.total,
	    swap.used, swap.free, swap.pagein, swap.pageout);
    dprintf(2, "swap: %lld %lld %lld", total, used, avail);
    dprintf(2, "swap: %lldK %lldK %lldK", total>>10, used>>10, avail>>10);
    dprintf(2, "swap: %lldM %lldM %lldM", total>>20, used>>20, avail>>20);
      
    fused=(100.f*used)/((gfloat) total);
    if(fused>100.f)
      fused=100.f;

    dprintf(4, "%2.0f %%\n", fused);

    if(win) {
      gtk_label_set_text(GTK_LABEL(swap_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(swap_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(swap_free), fmt_size(avail));
      
    }
    dprintf(5, "set progress\n");
    gtk_progress_set_value(GTK_PROGRESS(swap_per), fused);
    
  } else {
    if(win) {
      gtk_label_set_text(GTK_LABEL(swap_total), "?");
      gtk_label_set_text(GTK_LABEL(swap_used), "?");
      gtk_label_set_text(GTK_LABEL(swap_free), "?");
    }
    gtk_progress_set_value(GTK_PROGRESS(swap_per), 0.);
  }
#endif
  
  return TRUE;
}

static void do_update(void)
{
  update_values(NULL);
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SWAP_NUM    "ShowSwapNumbers"

static void read_choices(void)
{
  guchar *fname;
  
  choices_init();

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
	dprintf(4, "line=%s\n", line);
	end=strpbrk(line, "\n#");
	if(end)
	  *end=0;
	if(*line) {
	  char *sep;

	  dprintf(4, "line=%s\n", line);
	  sep=strchr(line, ':');
	  if(sep) {
	    dprintf(3, "%.*s: %s\n", sep-line, line, sep+1);
	    if(strncmp(line, UPDATE_RATE, sep-line)==0) {
	      options.update_sec=atoi(sep+1);
	      if(options.update_sec<1)
		options.update_sec=1;
	      dprintf(3, "update_sec now %d\n", options.update_sec);
	    } else if(strncmp(line, INIT_SIZE, sep-line)==0) {
	      options.applet_init_size=(guint) atoi(sep+1);
	    } else if(strncmp(line, SWAP_NUM, sep-line)==0) {
	      options.swap_nums=(guint) atoi(sep+1);
	    }
	  }
	}
      }
    } while(line);

    fclose(in);
  } else {
    write_choices();
  }
}

static void write_choices(void)
{
  FILE *out;
  gchar *fname;
    
  fname=choices_find_path_save("Config", PROJECT, TRUE);
  if(!fname)
    return;
  out=fopen(fname, "w");
  g_free(fname);
  if(!out)
    return;
  
  fprintf(out, _("# Config file for FreeFS\n"));
  fprintf(out, "%s: %d\n", UPDATE_RATE, options.update_sec);
  fprintf(out, "%s: %d\n", INIT_SIZE, options.applet_init_size);
  fprintf(out, "%s: %d\n", SWAP_NUM, options.swap_nums);
  fclose(out);
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
  gfloat s;

  options.update_sec=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->update_s));
  options.applet_init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));
 
  gtk_timeout_remove(update_tag);
  update_tag=gtk_timeout_add(options.update_sec*1000,
				     (GtkFunction) update_values, NULL);

  gtk_widget_hide(ow->window);
}

static void save_config(GtkWidget *widget, gpointer data)
{
  set_config(widget, data);
  write_choices();
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
    GtkWidget *frame;

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
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

    label=gtk_label_new(_("Update rate (s)"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    range=gtk_adjustment_new((gfloat) options.update_sec,
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

    range=gtk_adjustment_new((gfloat) options.applet_init_size,
			     16, 128, 2, 16, 16);
    spin=gtk_spin_button_new(GTK_ADJUSTMENT(range), 1, 0);
    gtk_widget_set_name(spin, "init_size");
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 4);
    ow.init_size=spin;

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

  } else {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.update_s),
			      (gfloat)(options.update_sec));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow.init_size),
			      (gfloat)(options.applet_init_size));
  }

  gtk_widget_show(confwin);  
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),	NULL, show_config_win, 0, NULL},
  { N_("/Update Now"),	NULL, do_update, 0, NULL },
  { N_("/Quit"), 	NULL, gtk_main_quit, 0, NULL },
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

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
    gtk_item_factory_parse_rc(menurc);
    g_free(menurc);
  }

  atexit(save_menus);

  menu = gtk_item_factory_get_widget(item_factory, "<system>");
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused)
{
  gboolean are_applet=(win==0);
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==3) {
      if(!menu)
	menu_create_menu(window);

      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
      return TRUE;
    } else if(bev->button==1 && are_applet) {
      gchar *cmd;

      cmd=g_strdup_printf("%s/AppRun &", getenv("APP_DIR"));
      system(cmd);
      g_free(cmd);

      return TRUE;
    }
  }

  return FALSE;
}

/*
 * $Log: mem.c,v $
 * Revision 1.1.1.1  2001/08/24 11:08:30  stephen
 * Monitor memory and swap
 *
 *
 */
