/*
 * freefs - Monitor free space on a single file system.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: freefs.c,v 1.16 2002/01/30 10:20:22 stephen Exp $
 */
#include "config.h"

/* Select compilation options */
#define TRY_DND            1     /* Enable the drag and drop stuff */
#define APPLET_DND         0     /* Support drag & drop to the applet */
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
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

#ifdef HAVE_XML
#include <tree.h>
#include <parser.h>
#endif

#if defined(HAVE_XML) && LIBXML_VERSION>=20400
#define USE_XML 1
#else
#define USE_XML 0
#endif

#include "rox.h"
#include "choices.h"
#include "infowin.h"
#include "rox_debug.h"
#if TRY_DND
#include "rox_dnd.h"
#endif
#include "rox_resources.h"
#include "rox_filer_action.h"
#include "applet.h"

#define TIP_PRIVATE "For more information see the help file"

/* GTK+ objects */
static GtkWidget *win=NULL;
static GtkWidget *plug=NULL;
static GtkWidget *fs_name=NULL, *fs_total, *fs_used, *fs_free, *fs_per;
static GtkWidget *menu=NULL;
static guint update_tag=0;
typedef struct option_widgets {
  GtkWidget *window;
  GtkWidget *update_s;
  GtkWidget *init_size;
  GtkWidget *show_dir;
} OptionWidgets;

static char *df_dir=NULL;  /* Directory to monitor */

typedef struct options {
  guint update_sec;          /* How often to update */
  guint applet_init_size;    /* Initial size of applet */
  gboolean applet_show_dir;  /* Print name of directory on applet version */
} Options;

static Options options={
  5,
  36,
  TRUE
};

/* Call backs & stuff */
static gboolean update_fs_values(gchar *mntpt);
static void do_update(void);
static void read_choices(void);
static void write_choices(void);
#if USE_XML
static gboolean read_choices_xml(void);
static void write_choices_xml(void);
#endif
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);
static gboolean handle_uris(GtkWidget *widget, GSList *uris, gpointer data,
			   gpointer udata);

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vh] [-a XID] [dir]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  -a XID\tX id of window to use in applet mode\n");
  printf("  dir\tdirectory on file sustem to monitor\n");
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
  printf("  Support drag & drop to change FS... %s\n", TRY_DND? "yes": "no");
  printf("  Support drag & drop to applet... %s\n", APPLET_DND? "yes": "no");
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
}

int main(int argc, char *argv[])
{
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkTooltips *ttips;
  char tbuf[1024], *home;
  gchar *fname;
  guint xid=0;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  int c, do_exit, nerr;

  rox_debug_init(PROJECT);

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif
  
  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  dprintf(5, "%d %s -> %s", argc, argv[1]? argv[1]: "NULL", argv[argc-1]);
  gtk_init(&argc, &argv);
  dprintf(5, "%d %s -> %s", argc, argv[1]? argv[1]: "NULL", argv[argc-1]);
  ttips=gtk_tooltips_new();

  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, "vha:"))!=EOF)
    switch(c) {
    case 'a':
      xid=atol(optarg);
      break;
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

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=rox_resources_find(PROJECT, "gtkrc", ROX_RESOURCES_DEFAULT_LANG);
  if(!fname)
    fname=choices_find_path_load("gtkrc", PROGRAM);
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  /* Figure out which directory we should be monitoring */
  if(optind<argc && argv[optind])
    df_dir=g_strdup(argv[optind]);
  else
    df_dir=g_dirname(argv[0]);
  if(!g_path_is_absolute(df_dir)) {
    gchar *tmp=df_dir;
    gchar *dir=g_get_current_dir();
    df_dir=g_strconcat(dir, "/", df_dir, NULL);
    g_free(dir);
    g_free(tmp);
  }

  if(!xid) {
    /* Full window mode */
#if TRY_DND
    rox_dnd_init();
#endif

    /* Construct our window and bits */
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "fs free");
    gtk_window_set_title(GTK_WINDOW(win), df_dir);
    gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		       GTK_SIGNAL_FUNC(gtk_main_quit), 
		       "WM destroy");
    gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), NULL);
    gtk_widget_realize(win);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
#if TRY_DND
    rox_dnd_register_uris(win, 0, handle_uris, NULL);
#endif
    /* Set the icon */
    style = gtk_widget_get_style(win);
    fname=rox_resources_find(PROJECT, "freefs.xpm", ROX_RESOURCES_NO_LANG);
    if(fname) {
      pixmap = gdk_pixmap_create_from_xpm(GTK_WIDGET(win)->window,  &mask,
					  &style->bg[GTK_STATE_NORMAL],
					  fname);
      gdk_window_set_icon(GTK_WIDGET(win)->window, NULL, pixmap, mask);
      g_free(fname);
    }
    gtk_tooltips_set_tip(ttips, win,
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show(vbox);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("Mounted on"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fs_name), GTK_JUSTIFY_LEFT);
    gtk_widget_set_name(fs_name, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_name, TRUE, FALSE, 2);
    gtk_widget_show(fs_name);
    gtk_tooltips_set_tip(ttips, fs_name,
			 "This is the name of the directory\n"
			 "the file system is mounted on", TIP_PRIVATE);
  
    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fs_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_total, FALSE, FALSE, 2);
    gtk_widget_show(fs_total);
    gtk_tooltips_set_tip(ttips, fs_total,
			 "This is the total size of the file system",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
    gtk_widget_show(hbox);
    
    fs_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_used, TRUE, FALSE, 2);
    gtk_widget_show(fs_used);
    gtk_tooltips_set_tip(ttips, fs_used,
			 "This is the space used on the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_used: %s window",
	    GTK_WIDGET_NO_WINDOW(fs_used)? "no": "has a");
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fs_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fs_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(fs_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(fs_per), TRUE);
    gtk_widget_set_usize(fs_per, 240, 24);
    gtk_widget_show(fs_per);
    gtk_box_pack_start(GTK_BOX(hbox), fs_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_per: %s window",
	    GTK_WIDGET_NO_WINDOW(fs_per)? "no": "has a");
  
    fs_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_free, TRUE, FALSE, 2);
    gtk_widget_show(fs_free);
    gtk_tooltips_set_tip(ttips, fs_free,
			 "This is the space available on the file system",
			 TIP_PRIVATE);

    menu_create_menu(win);
  } else {
    /* We are an applet, plug ourselves in */
    int i;

    dprintf(3, "xid=0x%x", xid);
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
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);

#if TRY_DND && APPLET_DND
    dprintf(3, "make drop target for plug");
    rox_dnd_register_uris(plug, 0, handle_uris, NULL);
#endif

    dprintf(4, "vbox new");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    dprintf(4, "alignment new");
    align=gtk_alignment_new(1, 0.5, 1, 0);
    gtk_box_pack_end(GTK_BOX(vbox), align, TRUE, TRUE, 2);
    gtk_widget_show(align);

    dprintf(4, "label new");
    fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fs_name), GTK_JUSTIFY_RIGHT);
    gtk_widget_set_name(fs_name, "text display");
    gtk_container_add(GTK_CONTAINER(align), fs_name);
    if(options.applet_show_dir)
      gtk_widget_show(fs_name);
    gtk_tooltips_set_tip(ttips, fs_name,
			 "This shows the dir where the FS is mounted",
			 TIP_PRIVATE);

    fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fs_per, "gauge");
    /*gtk_widget_set_usize(fs_per, -1, 22);*/
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fs_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(fs_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(fs_per), TRUE);
    gtk_widget_show(fs_per);
    gtk_box_pack_end(GTK_BOX(vbox), fs_per, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);

    menu_create_menu(plug);

    dprintf(5, "show plug");
    gtk_widget_show(plug);
    (void) applet_get_panel_location(plug);
    dprintf(5, "made applet");
  }

  /* Set up glibtop and check now */
  dprintf(4, "set up glibtop");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_FSUSAGE)|(1<<GLIBTOP_SYSDEPS_MOUNTLIST),
		 0);
  dprintf(3, "update_fs_values(%s)", df_dir);
  update_fs_values(df_dir);

  dprintf(3, "show %p (%ld)", win, xid);
  if(!xid)
    gtk_widget_show(win);
  dprintf(2, "timeout %ds, %p, %s", options.update_sec,
	  (GtkFunction) update_fs_values, df_dir);
  update_tag=gtk_timeout_add(options.update_sec*1000,
			 (GtkFunction) update_fs_values, df_dir);
  dprintf(2, "update_sec=%d, update_tag=%u", options.update_sec,
	  update_tag);
  
  gtk_main();

  return 0;
}

/* Change the directory/file and therefore FS we are monitoring */
static void set_target(const char *ndir)
{
  dprintf(3, "set_target(\"%s\")", ndir);
  
  if(df_dir)
    g_free(df_dir);
  df_dir=g_strdup(ndir);

  gtk_timeout_remove(update_tag);
  update_fs_values(df_dir);
  update_tag=gtk_timeout_add(options.update_sec*1000,
			 (GtkFunction) update_fs_values, df_dir);
  gtk_window_set_title(GTK_WINDOW(win), df_dir);
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

static const char *find_mount_point(const char *fname)
{
  static char wnm[PATH_MAX];
  static const char *failed=N_("(unknown)");
  static gchar *prev=NULL;
  static const gchar *prev_ans=NULL;
  
  const char *ans=NULL;
  char name[PATH_MAX];
  char *sl;
  glibtop_mountentry *ents;
  glibtop_mountlist list;
  int i;

#ifdef HAVE_REALPATH
  if(!realpath(fname, name))
    return _(failed);
#endif
  
  if(prev && strcmp(prev, name)==0 && prev_ans)
    return prev_ans;

  ents=glibtop_get_mountlist(&list, FALSE);
  
  strcpy(wnm, name);
  do {
    for(i=0; i<list.number; i++) {
      dprintf(4, "%s - %s\n", wnm, ents[i].mountdir);
      if(strcmp(wnm, ents[i].mountdir)==0) {
	ans=wnm;
	break;
      }
    }
    if(ans)
      break;
    sl=strrchr(wnm, '/');
    if(!sl)
      break;
    if(sl==wnm)
      sl[1]=0;
    else
      sl[0]=0;
  } while(!ans);
  
  if(prev)
    g_free(prev);
  prev=g_strdup(name);
  prev_ans=ans;

  return ans? ans: failed;
}

#define BLOCKSIZE 512

static gboolean update_fs_values(gchar *mntpt)
{
  int ok=FALSE;

  dprintf(4, "update_sec=%d, update_tag=%u", options.update_sec,
	  update_tag);
  dprintf(4, "update_fs_values(\"%s\")", mntpt);
  
  if(mntpt) {
    glibtop_fsusage buf;

    errno=0;
    glibtop_get_fsusage(&buf, mntpt);
    ok=(errno==0);
    if(!ok)
      perror(mntpt);
    
    if(ok) {
      unsigned long long total, used, avail;
      gfloat fused;
      int row;
      
      dprintf(5, "%lld %lld %lld", buf.blocks, buf.bfree,
	     buf.bavail);
      
      total=BLOCKSIZE*(unsigned long long) buf.blocks;
      used=BLOCKSIZE*(unsigned long long) (buf.blocks-buf.bfree);
      avail=BLOCKSIZE*(unsigned long long) buf.bavail;
      dprintf(4, "%llu %llu %llu", total, used, avail);

      if(buf.blocks>0) {
	fused=(100.f*(buf.blocks-buf.bavail))/((gfloat) buf.blocks);
	if(fused>100.f)
	  fused=100.f;
      } else {
	fused=0.f;
      }
      dprintf(4, "%2.0f %%", fused);

      if(win) {
	dprintf(4, "mount point=%s", find_mount_point(mntpt));
	gtk_label_set_text(GTK_LABEL(fs_name), find_mount_point(mntpt));
	gtk_label_set_text(GTK_LABEL(fs_total), fmt_size(total));
	gtk_label_set_text(GTK_LABEL(fs_used), fmt_size(used));
	gtk_label_set_text(GTK_LABEL(fs_free), fmt_size(avail));

      } else {
	dprintf(5, "set text");
	gtk_label_set_text(GTK_LABEL(fs_name),
			   g_basename(find_mount_point(mntpt)));
	/*gtk_label_set_text(GTK_LABEL(fs_name), find_mount_point(mntpt));*/
      }
      dprintf(5, "set progress");
      gtk_progress_set_value(GTK_PROGRESS(fs_per), fused);

    }
  }
  
  dprintf(4, "update_fs_values(%s), ok=%d, update_sec=%d", mntpt, ok,
	  options.update_sec);
  
  if(!ok && win) {
    gtk_label_set_text(GTK_LABEL(fs_name), "?");
    gtk_label_set_text(GTK_LABEL(fs_total), "?");
    gtk_label_set_text(GTK_LABEL(fs_used), "?");
    gtk_label_set_text(GTK_LABEL(fs_free), "?");

    gtk_progress_set_value(GTK_PROGRESS(fs_per), 0.);
  }

  return TRUE;
}

static void do_update(void)
{
  update_fs_values(df_dir);
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SHOW_DIR    "AppletShowDir"

#if USE_XML
static gboolean read_choices_xml(void)
{
  guchar *fname;

  fname=choices_find_path_load("Config.xml", PROJECT);

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

    if(strcmp(root->name, "FreeFS")!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return FALSE;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process data here */
      if(strcmp(node->name, UPDATE_RATE)==0) {
 	string=xmlGetProp(node, "value");
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
 	string=xmlGetProp(node, "show-dir");
	if(string) {
	  options.applet_show_dir=atoi(string);
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
  
  fname=choices_find_path_load("Config", "FreeFS");
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
	    } else if(strncmp(line, SHOW_DIR, sep-line)==0) {
	      options.applet_show_dir=(guint) atoi(sep+1);
	      if(fs_name) {
		if(options.applet_show_dir)
		  gtk_widget_show(fs_name);
		else
		  gtk_widget_hide(fs_name);
	      }
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
    tree=xmlNewChild(doc->children, NULL, UPDATE_RATE, NULL);
    sprintf(buf, "%d", options.update_sec);
    xmlSetProp(tree, "value", buf);
  
    tree=xmlNewChild(doc->children, NULL, "Applet", NULL);
    sprintf(buf, "%d", options.applet_init_size);
    xmlSetProp(tree, "initial-size", buf);
    sprintf(buf, "%d", options.applet_show_dir);
    xmlSetProp(tree, "show-dir", buf);
  
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);

    g_free(fname);
    xmlFreeDoc(doc);
  }
}
#endif

static void write_choices(void)
{
#if !USE_XML
  FILE *out;
  gchar *fname;
    
  fname=choices_find_path_save("Config", "FreeFS", TRUE);
  if(!fname)
    return;
  out=fopen(fname, "w");
  g_free(fname);
  if(!out)
    return;
  
  fprintf(out, _("# Config file for FreeFS\n"));
  fprintf(out, "%s: %d\n", UPDATE_RATE, options.update_sec);
  fprintf(out, "%s: %d\n", INIT_SIZE, options.applet_init_size);
  fprintf(out, "%s: %d\n", SHOW_DIR, options.applet_show_dir);
  fclose(out);
#else
  write_choices_xml();
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

  options.update_sec=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->update_s));
  options.applet_init_size=
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ow->init_size));
  options.applet_show_dir=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ow->show_dir));
  
  gtk_timeout_remove(update_tag);
  update_tag=gtk_timeout_add(options.update_sec*1000,
				     (GtkFunction) update_fs_values, df_dir);
  if(!win) {
    if(options.applet_show_dir)
      gtk_widget_show(fs_name);
    else
      gtk_widget_hide(fs_name);
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

    hbox=gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

    label=gtk_label_new(_("Display"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

    check=gtk_check_button_new_with_label(_("Show directory"));
    gtk_widget_set_name(check, "show_dir");
    gtk_widget_show(check);
    gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 4);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				 options.applet_show_dir);
    ow.show_dir=check;

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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow.show_dir),
				 options.applet_show_dir);
  }

  gtk_widget_show(confwin);  
}

static void do_opendir(gpointer dat, guint action, GtkWidget *wid)
{
  const char *dir=df_dir;

  if(action)
    dir=find_mount_point(dir);

  rox_filer_open_dir(dir);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),	NULL, show_config_win, 0, NULL},
  { N_("/Update Now"),	NULL, do_update, 0, NULL },
  { N_("/Open"),                NULL, NULL, 0, "<Branch>"},
  { N_("/Open/Directory"),	NULL, do_opendir, 0, NULL },
  { N_("/Open/FS root"),	NULL, do_opendir, 1, NULL },
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

      if(are_applet) {
	applet_show_menu(menu, bev);
      } else {
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
      }
      return TRUE;
    } else if(bev->button==1 && are_applet) {
      gchar *cmd;

      cmd=g_strdup_printf("%s/AppRun %s &", getenv("APP_DIR"),
			  df_dir);
      system(cmd);
      g_free(cmd);

      return TRUE;
    }
  }

  return FALSE;
}

#if TRY_DND

static gboolean handle_uris(GtkWidget *widget, GSList *uris,
			    gpointer data, gpointer udata)
{
  GSList *local;

  local=rox_dnd_filter_local(uris);

  dprintf(2, "URI dropped, local list is %p (%s)", local,
	  local? local->data: "NULL");

  if(local) {
    GSList *tmp;
    
    set_target((const char *) local->data);

    rox_dnd_local_free(local);
  }

  return FALSE;
}
#endif

/*
 * $Log: freefs.c,v $
 * Revision 1.16  2002/01/30 10:20:22  stephen
 * Add -h and -v options.
 *
 * Revision 1.15  2002/01/10 15:16:20  stephen
 * New applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.14  2001/12/12 10:18:22  stephen
 * Added option to open a viewer for the root of the FS being monitored
 *
 * Revision 1.13  2001/11/12 14:42:10  stephen
 * Change to XML handling: requires 2.4 or later.  Use old style config otherwise.
 *
 * Revision 1.12  2001/11/08 15:12:23  stephen
 * Fix leak in read/write XML config.
 *
 * Revision 1.11  2001/11/06 16:31:53  stephen
 * Pick up window icon from rox_resources_find. Config file now in XML.
 *
 * Revision 1.10  2001/10/23 10:50:06  stephen
 * Another compilation fix: run autoconf if configure script missing.
 * Can now choose what is shown in applet mode.
 *
 * Revision 1.9  2001/08/29 13:46:14  stephen
 * Fixed up the debug output.
 *
 * Revision 1.8  2001/08/20 09:32:15  stephen
 * Switch to using ROX-CLib.  Menu created at start so accelerators are
 * always available.
 *
 * Revision 1.7  2001/07/06 08:30:49  stephen
 * Improved applet appearance for top and bottom panels.  Support menu
 * accelerators.  Add tooltip help.
 *
 * Revision 1.6  2001/05/30 10:32:55  stephen
 * Initial support for i18n
 *
 * Revision 1.5  2001/05/30 09:04:53  stephen
 * include unistd.h, plus some bugfixes and prep for i18n.
 *
 * Revision 1.4  2001/05/25 07:53:44  stephen
 * Expanded configuration, which meant moving it off the menu and into a
 * window.  Applet supports a menu.  Initial size of applet now
 * configurable.
 *
 * Revision 1.3  2001/05/09 10:22:49  stephen
 * Version number now stored in AppInfo.xml
 *
 * Revision 1.2  2001/04/26 13:22:37  stephen
 * Added an applet front end.
 *
 */
