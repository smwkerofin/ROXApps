/*
 * freefs - Monitor free space on a single file system.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: freefs.c,v 1.5 2001/05/30 09:04:53 stephen Exp $
 */
#include "config.h"

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
#include "infowin.h"

#include <glibtop.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

#include "choices.h"

/* The icon */
#include "../pixmaps/freefs.xpm"

/* Select compilation options */
#define TRY_DND            1     /* Enable the drag and drop stuff */
#define APPLET_MENU        1     /* Use a menu for the applet version */
#define APPLET_DND         0     /* Support drag & drop to the applet */

/* GTK+ objects */
static GtkWidget *win=NULL;
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
static void menu_create_menu(void);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);

#if TRY_DND
static void make_drop_target(GtkWidget *widget);
static void dnd_init(void);

enum {
	TARGET_RAW,
	TARGET_URI_LIST,
	TARGET_XDS,
	TARGET_STRING,
};

#define MAXURILEN 4096
#endif

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
  localedir=g_strconcat(app_dir, "%s/Messages", NULL);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif
  
  /*fprintf(stderr, "%d %s -> %s\n", argc, argv[1], argv[argc-1]);*/
  gtk_init(&argc, &argv);
  /*fprintf(stderr, "%d %s -> %s\n", argc, argv[1], argv[argc-1]);*/

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=choices_find_path_load("gtkrc", "FreeFS");
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  while((c=getopt(argc, argv, "a:"))!=EOF) {
    /*fprintf(stderr, " %2d -%c %s\n", c, c, optarg? optarg: "NULL");*/
    switch(c) {
    case 'a':
      xid=atol(optarg);
      break;
    }
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
    dnd_init();
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
    make_drop_target(win);
#endif
    /* Set the icon */
    style = gtk_widget_get_style(win);
    pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(win)->window,  &mask,
					  &style->bg[GTK_STATE_NORMAL],
					  (gchar **)freefs_xpm );
    gdk_window_set_icon(GTK_WIDGET(win)->window, NULL, pixmap, mask);
  
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
    gtk_box_pack_start(GTK_BOX(hbox), fs_name, TRUE, TRUE, 2);
    gtk_widget_show(fs_name);
  
    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fs_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_total, FALSE, FALSE, 2);
    gtk_widget_show(fs_total);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
    gtk_widget_show(hbox);
    
    fs_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_used, TRUE, FALSE, 2);
    gtk_widget_show(fs_used);
  
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
  
    fs_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fs_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fs_free, TRUE, FALSE, 2);
    gtk_widget_show(fs_free);
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
    /*fprintf(stderr, "set_usize %d\n", options.applet_init_size);*/
    
    gtk_signal_connect(GTK_OBJECT(plug), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press), NULL);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);

#if TRY_DND && APPLET_DND
    /*fprintf(stderr, "make drop target for plug\n");*/
    make_drop_target(plug);
#endif

    /*fprintf(stderr, "vbox new\n");*/
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);
  
    /*fprintf(stderr, "progress bar new\n");*/
    fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fs_per, "gauge");
    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fs_per),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(fs_per), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(fs_per), TRUE);
    gtk_widget_show(fs_per);
    gtk_box_pack_start(GTK_BOX(vbox), fs_per, TRUE, TRUE, 2);

    /*fprintf(stderr, "alignment new\n");*/
    align=gtk_alignment_new(1, 0.5, 1, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, FALSE, FALSE, 2);
    gtk_widget_show(align);

    /* fprintf(stderr, "label new\n");*/
    fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fs_name), GTK_JUSTIFY_RIGHT);
    gtk_widget_set_name(fs_name, "text display");
    gtk_container_add(GTK_CONTAINER(align), fs_name);
    if(options.applet_show_dir)
      gtk_widget_show(fs_name);

    /*fprintf(stderr, "show plug\n");*/
    gtk_widget_show(plug);
    /*fprintf(stderr, "made applet\n");*/
  }

  /* Set up glibtop and check now */
  /*fprintf(stderr, "set up glibtop\n");*/
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_FSUSAGE)|(1<<GLIBTOP_SYSDEPS_MOUNTLIST),
		 0);
  /*fprintf(stderr, "update_fs_values(%s)\n", df_dir);*/
  update_fs_values(df_dir);

  /*fprintf(stderr, "show %p (%ld)\n", win, xid);*/
  if(!xid)
    gtk_widget_show(win);
  /*fprintf(stderr, "timeout %ds, %p, %s\n", options.update_sec,
	  (GtkFunction) update_fs_values, df_dir);*/
  update_tag=gtk_timeout_add(options.update_sec*1000,
			 (GtkFunction) update_fs_values, df_dir);
  /*fprintf(stderr, "update_sec=%d, update_tag=%u\n", options.update_sec,
	  update_tag);*/

  gtk_main();

  return 0;
}

/* Change the directory/file and therefore FS we are monitoring */
static void set_target(const char *ndir)
{
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
      /*printf("%s - %s\n", wnm, ents[i].mountdir);*/
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

  /*
  fprintf(stderr, "update_sec=%d, update_tag=%u\n", options.update_sec,
	  update_tag);
  fprintf(stderr, "update_fs_values(\"%s\")\n", mntpt);
  */
  
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
      
      /*printf("%lld %lld %lld\n", buf.f_blocks, buf.f_bfree,
	     buf.f_bavail);*/
      
      total=BLOCKSIZE*(unsigned long long) buf.blocks;
      used=BLOCKSIZE*(unsigned long long) (buf.blocks-buf.bfree);
      avail=BLOCKSIZE*(unsigned long long) buf.bavail;
      /*printf("%llu %llu %llu\n", total, used, avail);*/

      if(buf.blocks>0) {
	fused=(100.f*(buf.blocks-buf.bavail))/((gfloat) buf.blocks);
	if(fused>100.f)
	  fused=100.f;
      } else {
	fused=0.f;
      }
      /*fprintf(stderr, "%2.0f %%\n", fused);*/

      if(win) {
	/*printf("mount point=%s\n", find_mount_point(mntpt));*/
	gtk_label_set_text(GTK_LABEL(fs_name), find_mount_point(mntpt));
	gtk_label_set_text(GTK_LABEL(fs_total), fmt_size(total));
	gtk_label_set_text(GTK_LABEL(fs_used), fmt_size(used));
	gtk_label_set_text(GTK_LABEL(fs_free), fmt_size(avail));

      } else {
	/*fprintf(stderr, "set text\n");*/
	gtk_label_set_text(GTK_LABEL(fs_name),
			   g_basename(find_mount_point(mntpt)));
	/*gtk_label_set_text(GTK_LABEL(fs_name), find_mount_point(mntpt));*/
      }
      /*fprintf(stderr, "set progress\n");*/
      gtk_progress_set_value(GTK_PROGRESS(fs_per), fused);

    }
  }
  
  /*fprintf(stderr, "update_fs_values(%s), ok=%d, update_sec=%d\n", mntpt, ok,
	  options.update_sec);*/
  
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

static void read_choices(void)
{
  guchar *fname;
  
  choices_init();

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
	/*printf("line=%s\n", line);*/
	end=strpbrk(line, "\n#");
	if(end)
	  *end=0;
	if(*line) {
	  char *sep;

	  /*printf("line=%s\n", line);*/
	  sep=strchr(line, ':');
	  if(sep) {
	    /*printf("%.*s: %s\n", sep-line, line, sep+1);*/
	    if(strncmp(line, UPDATE_RATE, sep-line)==0) {
	      options.update_sec=atoi(sep+1);
	      if(options.update_sec<1)
		options.update_sec=1;
	      /*printf("update_sec now %d\n", update_sec);*/
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

static void write_choices(void)
{
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

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Configure..."),	NULL, show_config_win, 0, NULL},
  { N_("/Update Now"),	NULL, do_update, 0, NULL },
  { N_("/Quit"), 	NULL, gtk_main_quit, 0, NULL },
};


static void menu_create_menu(void)
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
	//gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  menu = gtk_item_factory_get_widget(item_factory, "<system>");
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused)
{
  gboolean are_applet=(win==0);
  
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==3
#if !APPLET_MENU
       && !are_applet
#endif
       ) {
      if(!menu)
	menu_create_menu();

      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
      return TRUE;
    } else if(bev->button==1 && are_applet) {
      gchar *cmd;

      cmd=g_strdup_printf("%s/AppRun %s", getenv("APP_DIR"),
			  df_dir);
      system(cmd);
      g_free(cmd);

      return TRUE;
    }
  }

  return FALSE;
}

#if TRY_DND

/* Implement drag & drop to change directory monitored.
   "Borrowed" from ROX-Filer.
*/

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context,
			  gint x, gint y, guint time, gpointer data);
static void drag_data_received(GtkWidget *widget, GdkDragContext *context,
			       gint x, gint y,
			       GtkSelectionData *selection_data,
			       guint info, guint32 time, gpointer user_data);

static GdkAtom XdndDirectSave0;
static GdkAtom xa_text_plain;
static GdkAtom text_uri_list;
static GdkAtom application_octet_stream;

static void dnd_init(void)
{
	XdndDirectSave0 = gdk_atom_intern("XdndDirectSave0", FALSE);
	xa_text_plain = gdk_atom_intern("text/plain", FALSE);
	text_uri_list = gdk_atom_intern("text/uri-list", FALSE);
	application_octet_stream = gdk_atom_intern("application/octet-stream",
			FALSE);
}

static void make_drop_target(GtkWidget *widget)
{
  static GtkTargetEntry target_table[]={
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"XdndDirectSave0", 0, TARGET_XDS},
  };
  static const int ntarget=sizeof(target_table)/sizeof(target_table[0]);
  
  gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, target_table, ntarget,
		    GDK_ACTION_COPY|GDK_ACTION_PRIVATE);

  gtk_signal_connect(GTK_OBJECT(widget), "drag_drop",
		     GTK_SIGNAL_FUNC(drag_drop), NULL);
  gtk_signal_connect(GTK_OBJECT(widget), "drag_data_received",
		     GTK_SIGNAL_FUNC(drag_data_received), NULL);

  /*printf("made %p a drop target\n", widget);*/
}

static void print_context(GQuark key_id, gpointer data, gpointer user_data)
{
  printf("%p (%s) = %p (%s)\n", key_id, g_quark_to_string(key_id),
	 data, data? data: "NULL");
}

/* Is the sender willing to supply this target type? */
gboolean provides(GdkDragContext *context, GdkAtom target)
{
  GList	    *targets = context->targets;

  while (targets && ((GdkAtom) targets->data != target))
    targets = targets->next;
  
  return targets != NULL;
}

static char *get_xds_prop(GdkDragContext *context)
{
  guchar	*prop_text;
  gint	length;

  if (gdk_property_get(context->source_window,
		       XdndDirectSave0,
		       xa_text_plain,
		       0, MAXURILEN,
		       FALSE,
		       NULL, NULL,
		       &length, &prop_text) && prop_text) {
    /* Terminate the string */
    prop_text = g_realloc(prop_text, length + 1);
    prop_text[length] = '\0';
    return prop_text;
  }

  return NULL;
}

/* Convert a list of URIs into a list of strings.
 * Lines beginning with # are skipped.
 * The text block passed in is zero terminated (after the final CRLF)
 */
GSList *uri_list_to_gslist(char *uri_list)
{
  GSList   *list = NULL;

  while (*uri_list) {
    char	*linebreak;
    char	*uri;
    int	length;

    linebreak = strchr(uri_list, 13);

    if (!linebreak || linebreak[1] != 10) {
      fprintf(stderr, _("uri_list_to_gslist: Incorrect or missing line "
		      "break in text/uri-list data\n"));
      return list;
    }
    
    length = linebreak - uri_list;

    if (length && uri_list[0] != '#') {
	uri = g_malloc(sizeof(char) * (length + 1));
	strncpy(uri, uri_list, length);
	uri[length] = 0;
	list = g_slist_append(list, uri);
      }

    uri_list = linebreak + 2;
  }

  return list;
}

/* User has tried to drop some data on us. Decide what format we would
 * like the data in.
 */
static gboolean drag_drop(GtkWidget 	  *widget,
			  GdkDragContext  *context,
			  gint            x,
			  gint            y,
			  guint           time,
			  gpointer	  data)
{
  char *leafname=NULL;
  char *path=NULL;
  GdkAtom target = GDK_NONE;

  /*
  printf("in drag_drop(%p, %p, %d, %d, %u, %p)\n", widget, context, x, y,
	 time, data);
  */
  
  /*g_dataset_foreach(context, print_context, NULL);*/

  if(provides(context, XdndDirectSave0)) {
    leafname = get_xds_prop(context);
    if (leafname) {
      /*printf("leaf is %s\n", leafname);*/
      target = XdndDirectSave0;
      g_dataset_set_data_full(context, "leafname", leafname, g_free);
    }
  } else if(provides(context, text_uri_list))
    target = text_uri_list;

  gtk_drag_get_data(widget, context, target, time);

  return TRUE;
}

char *get_local_path(const char *uri)
{
  char *host;
  char *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    host=g_strndup(host, end-host);

    if(strcmp(host, "localhost")==0) {
      g_free(host);
      return g_strdup(end);
    }

    g_free(host);
  }

  return NULL;
}

char *get_server(const char *uri)
{
  char *host, *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup("localhost");
    
    host=(char *) uri+2;
    end=strchr(host, '/');
    return g_strndup(host, end-host);
  }

  return g_strdup("localhost");
}

char *get_path(const char *uri)
{
  char *host;
  char *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    
    return g_strdup(end);
  }

  return NULL;
}

/* We've got a list of URIs from somewhere (probably a filer window).
 * If the files are on the local machine then use the name,
 * otherwise, try /net/server/path.
 */
static void got_uri_list(GtkWidget 		*widget,
			 GdkDragContext 	*context,
			 GtkSelectionData 	*selection_data,
			 guint32             	time)
{
  GSList		*uri_list;
  char		*error = NULL;
  GSList		*next_uri;
  gboolean	send_reply = TRUE;
  char		*path=NULL, *server=NULL;
  const char		*uri;

  uri_list = uri_list_to_gslist(selection_data->data);

  if (!uri_list)
    error = _("No URIs in the text/uri-list (nothing to do!)");
  else {
    /*
    for(next_uri=uri_list; next_uri; next_uri=next_uri->next)
      printf("%s\n", next_uri->data);
      */

    uri=uri_list->data;

    path=get_local_path(uri);
    if(!path) {
      char *tpath=get_path(uri);
      server=get_server(uri);
      path=g_strconcat("/net/", server, tpath, NULL);
      g_free(server);
      g_free(tpath);
    }
      
  }

  if (error) {
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    fprintf(stderr, "%s: %s\n", "got_uri_list", error);
  } else {
    gtk_drag_finish(context, TRUE, FALSE, time);    /* Success! */

    /*printf("new target is %s\n", path);*/
    set_target(path);
  }
  
  next_uri = uri_list;
  while (next_uri) {
      g_free(next_uri->data);
      next_uri = next_uri->next;
  }
  g_slist_free(uri_list);

  if(path)
    g_free(path);
}

/* Called when some data arrives from the remote app (which we asked for
 * in drag_drop).
 */
static void drag_data_received(GtkWidget      	*widget,
			       GdkDragContext  	*context,
			       gint            	x,
			       gint            	y,
			       GtkSelectionData *selection_data,
			       guint            info,
			       guint32          time,
			       gpointer		user_data)
{
  /*printf("%p in drag_data_received\n", widget);*/
  
  if (!selection_data->data) {
    /* Timeout? */
    gtk_drag_finish(context, FALSE, FALSE, time);	/* Failure */
    return;
  }

  switch(info) {
  case TARGET_XDS:
    /*printf("XDS\n");*/
    break;
  case TARGET_URI_LIST:
    /*printf("URI list\n");*/
    got_uri_list(widget, context, selection_data, time);
    break;
  default:
    fprintf(stderr, _("drag_data_received: unknown target\n"));
    gtk_drag_finish(context, FALSE, FALSE, time);
    break;
  }
}

#endif

/*
 * $Log: freefs.c,v $
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
