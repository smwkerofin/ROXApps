/*
 * Mem - Monitor memory and swap usage
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies, see ../Help/COPYING.
 *
 * $Id: mem.c,v 1.4 2001/09/06 09:53:31 stephen Exp $
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
#include <sys/types.h>
#include <sys/wait.h>

#include <gtk/gtk.h>

#include <glibtop.h>
#include <glibtop/mem.h>
#include <glibtop/swap.h>

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
static GtkWidget *win=NULL;
static GtkWidget *host=NULL;
static GtkWidget *mem_total=NULL, *mem_used=NULL, *mem_free=NULL,
  *mem_per=NULL;
static GtkWidget *swap_total=NULL, *swap_used=NULL, *swap_free=NULL,
  *swap_per=NULL;
static GtkWidget *menu=NULL;
static guint update_tag=0;
typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *init_size;
  GtkWidget *show_host;

  GtkWidget *mem_disp[NUM_DISPLAY];
  GtkWidget *swap_disp[NUM_DISPLAY];
} OptionWidgets;

typedef struct options {
  guint update_sec;          /* How often to update */
  guint applet_init_size;    /* Initial size of applet */
  guint show_host;
  AppletDisplay mem_disp;
  AppletDisplay swap_disp;
} Options;

static Options options={
  5,
  36,
  FALSE,
  AD_PER,
  AD_PER
};

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

/* Call backs & stuff */
static gboolean update_values(gpointer);
static void do_update(void);
static void read_choices(void);
static void write_choices(void);
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);

#if !SWAP_SUPPORTED_LIBGTOP
static gboolean update_swap(gpointer);
#endif

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
  char hname[1024];

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif

  rox_debug_init(PROJECT);
  
  dprintf(5, "%d %s -> %s", argc, argv[1], argv[argc-1]);
  gtk_init(&argc, &argv);
  dprintf(5, "%d %s -> %s", argc, argv[1], argv[argc-1]);
  ttips=gtk_tooltips_new();

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=choices_find_path_load("gtkrc", PROJECT);
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  while((c=getopt(argc, argv, "a:"))!=EOF) {
    dprintf(5, " %2d -%c %s", c, c, optarg? optarg: "NULL");
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
			 "Mem shows the memory and swap usage on a host",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show(vbox);

    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    label=gtk_label_new(_("Host:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    gethostname(hname, sizeof(hname));
    label=gtk_label_new(hname);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_set_name(label, "text display");
    gtk_widget_show(label);
    host=hbox;
    if(options.show_host)
      gtk_widget_show(host);

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

    dprintf(4, "vbox new");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    mem_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_total, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mem_total, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mem_total,
			 "This is the total size of memory",
			 TIP_PRIVATE);
  
    mem_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_used, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mem_used, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mem_used,
			 "This is the memory used on the host",
			 TIP_PRIVATE);
  
    mem_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(mem_free, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), mem_free, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, mem_free,
			 "This is the memory available on the host",
			 TIP_PRIVATE);

    mem_per=gtk_progress_bar_new();
    gtk_widget_set_name(mem_per, "gauge");
    /*gtk_widget_set_usize(mem_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(mem_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(mem_per), "M %p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(mem_per), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), mem_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, mem_per,
			 "This shows the relative usage of memory",
			 TIP_PRIVATE);

    switch(options.mem_disp) {
    case AD_TOTAL:
      gtk_widget_show(mem_total);
      break;
    case AD_USED:
      gtk_widget_show(mem_used);
      break;
    case AD_FREE:
      gtk_widget_show(mem_free);
      break;
    case AD_PER:
      gtk_widget_show(mem_per);
      break;
    }

    swap_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_total, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), swap_total, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, swap_total,
			 "This is the total size of swap space",
			 TIP_PRIVATE);
  
    swap_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_used, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), swap_used, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, swap_used,
			 "This is the swap space used on the host",
			 TIP_PRIVATE);
  
    swap_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(swap_free, "text display");
    gtk_box_pack_start(GTK_BOX(vbox), swap_free, TRUE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, swap_free,
			 "This is the swap space available on the host",
			 TIP_PRIVATE);

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
    
    switch(options.swap_disp) {
    case AD_TOTAL:
      gtk_widget_show(swap_total);
      break;
    case AD_USED:
      gtk_widget_show(swap_used);
      break;
    case AD_FREE:
      gtk_widget_show(swap_free);
      break;
    case AD_PER:
      gtk_widget_show(swap_per);
      break;
    }

    menu_create_menu(plug);

    dprintf(5, "show plug");
    gtk_widget_show(plug);
    dprintf(5, "made applet");
  }

  /* Set up glibtop and check now */
  dprintf(4, "set up glibtop");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_MEM)
#if SWAP_SUPPORTED_LIBGTOP
		 |(1<<GLIBTOP_SYSDEPS_SWAP)
#endif
		 , 0);
  update_values(NULL);

  dprintf(3, "show %p (%ld)", win, xid);
  if(!xid)
    gtk_widget_show(win);
  dprintf(2, "timeout %ds, %p, %p", options.update_sec,
	  (GtkFunction) update_values, NULL);
  update_tag=gtk_timeout_add(options.update_sec*1000,
			 (GtkFunction) update_values, NULL);
  dprintf(2, "update_sec=%d, update_tag=%u", options.update_sec,
	  update_tag);

#if !SWAP_SUPPORTED_LIBGTOP
  swap_update_tag=gtk_timeout_add(10*1000,
			 (GtkFunction) update_swap, NULL);
  (void) update_swap(NULL);
#endif
  
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

#if SWAP_SUPPORTED_LIBGTOP
#define SWAP_FLAGS ((1<<GLIBTOP_SWAP_TOTAL)|(1<<GLIBTOP_SWAP_USED)|(1<<GLIBTOP_SWAP_FREE))
#endif

static gboolean update_values(gpointer unused)
{
  int ok=FALSE;
  glibtop_mem mem;
#if SWAP_SUPPORTED_LIBGTOP
  glibtop_swap swap;
#endif
  unsigned long long total, used, avail;
  
  dprintf(4, "update_sec=%d, update_tag=%u", options.update_sec,
	  update_tag);
  
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

    if(win) {
      gtk_label_set_text(GTK_LABEL(mem_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mem_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mem_free), fmt_size(avail));
    } else {
      char tmp[64];

      strcpy(tmp, "Mt ");
      strcat(tmp, fmt_size(total));
      gtk_label_set_text(GTK_LABEL(mem_total), tmp);

      strcpy(tmp, "Mu ");
      strcat(tmp, fmt_size(used));
      gtk_label_set_text(GTK_LABEL(mem_used), tmp);

      strcpy(tmp, "Mf ");
      strcat(tmp, fmt_size(avail));
      gtk_label_set_text(GTK_LABEL(mem_free), tmp);

    }
      
    dprintf(5, "set progress");
    gtk_progress_set_value(GTK_PROGRESS(mem_per), fused);
    gtk_widget_set_sensitive(GTK_WIDGET(mem_per), TRUE);

  } else {
    gtk_label_set_text(GTK_LABEL(mem_total), "Mem?");
    gtk_label_set_text(GTK_LABEL(mem_used), "Mem?");
    gtk_label_set_text(GTK_LABEL(mem_free), "Mem?");
    gtk_widget_set_sensitive(GTK_WIDGET(mem_per), FALSE);
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

    if(win) {
      gtk_label_set_text(GTK_LABEL(swap_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(swap_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(swap_free), fmt_size(avail));
    } else {
      char tmp[64];

      strcpy(tmp, "St ");
      strcat(tmp, fmt_size(total));
      gtk_label_set_text(GTK_LABEL(swap_total), tmp);

      strcpy(tmp, "Su ");
      strcat(tmp, fmt_size(used));
      gtk_label_set_text(GTK_LABEL(swap_used), tmp);

      strcpy(tmp, "Sf ");
      strcat(tmp, fmt_size(avail));
      gtk_label_set_text(GTK_LABEL(swap_free), tmp);

    }
      
    dprintf(5, "set progress");
    gtk_progress_set_value(GTK_PROGRESS(swap_per), fused);
    gtk_widget_set_sensitive(GTK_WIDGET(swap_per), TRUE);
    
  } else {
    gtk_label_set_text(GTK_LABEL(swap_total), "Swap?");
    gtk_label_set_text(GTK_LABEL(swap_used), "Swap?");
    gtk_label_set_text(GTK_LABEL(swap_free), "Swap?");

    gtk_widget_set_sensitive(GTK_WIDGET(swap_per), FALSE);
  }
  
  return TRUE;
}

static void do_update(void)
{
  update_values(NULL);
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SWAP_NUM    "ShowSwapNumbers"
#define SHOW_HOST   "ShowHostName"
#define MEM_DISP    "MemoryAppletDisplay"
#define SWAP_DISP   "SwapAppletDisplay"

#if USE_XML
static gboolean read_choices_xml()
{
  guchar *fname;

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
	options.update_sec=atoi(string);
	free(string);

      } else if(strcmp(node->name, "Applet")==0) {
 	string=xmlGetProp(node, "initial-size");
	if(string) {
	  options.applet_init_size=atoi(string);
	  free(string);
	}
 	string=xmlGetProp(node, "mem");
	if(string) {
	  options.mem_disp=atoi(string);
	  free(string);
	}
 	string=xmlGetProp(node, "swap");
	if(string) {
	  options.swap_disp=atoi(string);
	  free(string);
	}
	
      } else if(strcmp(node->name, "Window")==0) {
 	string=xmlGetProp(node, "show-host");
	if(string) {
	  options.show_host=atoi(string);
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
  guchar *fname;
  
  choices_init();

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
	      options.update_sec=atoi(sep+1);
	      if(options.update_sec<1)
		options.update_sec=1;
	      dprintf(3, "update_sec now %d", options.update_sec);
	    } else if(strncmp(line, INIT_SIZE, sep-line)==0) {
	      options.applet_init_size=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, SWAP_NUM, sep-line)==0) {
	      /* Ignore */
	      dprintf(1, "obselete option %s, ignored", SWAP_NUM);
	      
	    } else if(strncmp(line, SHOW_HOST, sep-line)==0) {
	      options.show_host=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, MEM_DISP, sep-line)==0) {
	      options.mem_disp=(guint) atoi(sep+1);
	      
	    } else if(strncmp(line, SWAP_DISP, sep-line)==0) {
	      options.swap_disp=(guint) atoi(sep+1);
	      
	    } else {

	      dprintf(1, "unknown option %s, ignored", sep-line);
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

#if USE_XML
static void write_choices_xml(void)
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

    doc = xmlNewDoc("1.0");
    doc->children=xmlNewDocNode(doc, NULL, PROJECT, NULL);
    xmlSetProp(doc->children, "version", VERSION);

    /* Insert data here */
    tree=xmlNewChild(doc->children, NULL, "update", NULL);
    sprintf(buf, "%d", options.update_sec);
    xmlSetProp(tree, "rate", buf);
  
    tree=xmlNewChild(doc->children, NULL, "Applet", NULL);
    sprintf(buf, "%d", options.applet_init_size);
    xmlSetProp(tree, "initial-size", buf);
    sprintf(buf, "%d", options.mem_disp);
    xmlSetProp(tree, "mem", buf);
    sprintf(buf, "%d", options.swap_disp);
    xmlSetProp(tree, "swap", buf);
  
    tree=xmlNewChild(doc->children, NULL, "Window", NULL);
    sprintf(buf, "%d", options.show_host);
    xmlSetProp(tree, "show-host", buf);

    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);

    xmlFreeDoc(doc);
    g_free(fname);
  }
}
#endif

static void write_choices(void)
{
#if USE_XML
  write_choices_xml();
#else
  FILE *out;
  gchar *fname;
    
  fname=choices_find_path_save("Config", PROJECT, TRUE);
  if(!fname)
    return;
  out=fopen(fname, "w");
  g_free(fname);
  if(!out)
    return;
  
  fprintf(out, _("# Config file for Mem\n"));
  fprintf(out, "%s: %d\n", UPDATE_RATE, options.update_sec);
  fprintf(out, "%s: %d\n", INIT_SIZE, options.applet_init_size);
  fprintf(out, "%s: %d\n", SHOW_HOST, options.show_host);
  fprintf(out, "%s: %d\n", MEM_DISP, options.mem_disp);
  fprintf(out, "%s: %d\n", SWAP_DISP, options.swap_disp);
  fclose(out);
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
  gfloat s;
  int i;

  options.update_sec=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->update_s));
  options.applet_init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));
  options.show_host=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_host));

  options.mem_disp=AD_PER;
  for(i=0; i<NUM_DISPLAY; i++)
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->mem_disp[i]))) {
      options.mem_disp=i;
      break;
    }
  options.swap_disp=AD_PER;
  for(i=0; i<NUM_DISPLAY; i++)
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->swap_disp[i]))) {
      options.swap_disp=i;
      break;
    }
 
  gtk_timeout_remove(update_tag);
  update_tag=gtk_timeout_add(options.update_sec*1000,
				     (GtkFunction) update_values, NULL);

  if(host) {
    if(options.show_host)
      gtk_widget_show(host);
    else
      gtk_widget_hide(host);
  }

  if(!win) {
    switch(options.mem_disp) {
    case AD_TOTAL:
      gtk_widget_show(mem_total);
      gtk_widget_hide(mem_used);
      gtk_widget_hide(mem_free);
      gtk_widget_hide(mem_per);
      break;
      
    case AD_USED:
      gtk_widget_hide(mem_total);
      gtk_widget_show(mem_used);
      gtk_widget_hide(mem_free);
      gtk_widget_hide(mem_per);
      break;
      
    case AD_FREE:
      gtk_widget_hide(mem_total);
      gtk_widget_hide(mem_used);
      gtk_widget_show(mem_free);
      gtk_widget_hide(mem_per);
      break;
      
    case AD_PER:
      gtk_widget_hide(mem_total);
      gtk_widget_hide(mem_used);
      gtk_widget_hide(mem_free);
      gtk_widget_show(mem_per);
      break;
      
    }
    switch(options.swap_disp) {
    case AD_TOTAL:
      gtk_widget_show(swap_total);
      gtk_widget_hide(swap_used);
      gtk_widget_hide(swap_free);
      gtk_widget_hide(swap_per);
      break;
      
    case AD_USED:
      gtk_widget_hide(swap_total);
      gtk_widget_show(swap_used);
      gtk_widget_hide(swap_free);
      gtk_widget_hide(swap_per);
      break;
      
    case AD_FREE:
      gtk_widget_hide(swap_total);
      gtk_widget_hide(swap_used);
      gtk_widget_show(swap_free);
      gtk_widget_hide(swap_per);
      break;
      
    case AD_PER:
      gtk_widget_hide(swap_total);
      gtk_widget_hide(swap_used);
      gtk_widget_hide(swap_free);
      gtk_widget_show(swap_per);
      break;
      
    }
  }

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
    GtkWidget *radio;

    confwin=gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(confwin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     confwin);
    gtk_window_set_title(GTK_WINDOW(confwin), _("Mem Configuration"));
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

    frame=gtk_frame_new(_("Applet display"));
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

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

    label=gtk_label_new(_("For memory show"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    radio=gtk_radio_button_new_with_label(NULL, _("Total"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.mem_disp[AD_TOTAL]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Used"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.mem_disp[AD_USED]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Free"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.mem_disp[AD_FREE]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Percentage"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.mem_disp[AD_PER]=radio;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.mem_disp[options.mem_disp]),
				 TRUE);
    
    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

    label=gtk_label_new(_("For swap space show"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    radio=gtk_radio_button_new_with_label(NULL, _("Total"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.swap_disp[AD_TOTAL]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Used"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.swap_disp[AD_USED]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Free"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.swap_disp[AD_FREE]=radio;

    radio=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
						      _("Percentage"));
    gtk_widget_show(radio);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 4);
    ow.swap_disp[AD_PER]=radio;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.swap_disp[options.swap_disp]),
				 TRUE);
    
    vbox=GTK_DIALOG(confwin)->vbox;

    frame=gtk_frame_new(_("Window display"));
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, FALSE, 6);

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(frame), hbox);

    check=gtk_check_button_new_with_label(_("Show host name"));
    gtk_widget_set_name(check, "show_host");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 4);
    ow.show_host=check;

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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_host),
				 options.show_host);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.mem_disp[options.mem_disp]),
				 TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.swap_disp[options.swap_disp]),
				 TRUE);
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
  
  if(!swap_process) {
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
