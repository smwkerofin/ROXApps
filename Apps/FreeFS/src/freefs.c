/*
 * freefs - Monitor free space on a single file system.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: freefs.c,v 1.27 2004/05/10 18:36:22 stephen Exp $
 */
#include "config.h"

/* Select compilation options */
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

#if defined(HAVE_MNTENT_H)
#include <mntent.h>
#define CHECK_MOUNTS
#elif defined(HAVE_SYS_MNTTAB_H)
#include <sys/mnttab.h>
#define CHECK_MOUNTS
#else
#undef CHECK_MOUNTS
#endif

#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_STATFS
#include <sys/vfs.h>
#endif
#ifdef HAVE_STATVFS
#include <sys/statvfs.h>
#endif

#include <gtk/gtk.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <rox/rox.h>
#include <rox/rox_dnd.h>
#include <rox/rox_resources.h>
#include <rox/rox_filer_action.h>
#include <rox/applet.h>
#include <rox/rox_soap.h>
#include <rox/rox_soap_server.h>

#define TIP_PRIVATE "For more information see the help file"

/* GTK+ objects */
static GtkWidget *menu=NULL;

static Option opt_update_sec;
static Option opt_applet_size;
static Option opt_applet_show_dir;

typedef struct free_window {
  GtkWidget *win;
  GtkWidget *fs_name, *fs_total, *fs_used, *fs_free, *fs_per;
  
  gboolean is_applet;
  gboolean minimal;
  guint update_tag;
  
  char *df_dir;
  char *real_path;
  gchar *fs_mount;

  gchar *id;
  
} FreeWindow;

typedef struct mount_info {
  gchar *mount_point;
  gchar *device;
  gchar *type;
} MountInfo;

static GList *windows=NULL;

static FreeWindow *current_window=NULL;

static ROXSOAPServer *server=NULL;
#define FREEFS_NAMESPACE_URL WEBSITE PROJECT

static GList *fs_exclude=NULL; /* File system types not to offer on menu */

static GList *mount_points=NULL;

/* Call backs & stuff */
static FreeWindow *make_window(guint32 socket, const char *dir,
			       gboolean mini, const gchar *id);
static FreeWindow *find_window(const gchar *id);
static gboolean update_fs_values(FreeWindow *);
static void do_update(void);
static void init_options(void);
static void show_config_win(int ignored);
static void menu_create_menu(GtkWidget *);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);
static gboolean popup_menu(GtkWidget *window, gpointer udata);
static gboolean handle_uris(GtkWidget *widget, GSList *uris, gpointer data,
			   gpointer udata);

static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean open_remote(guint32 xid, const char *path, gboolean mini);
static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
static gboolean options_remote(void);
static xmlNodePtr rpc_Change(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata);
/*static gboolean change_remote(const char *id, const char *path);*/

static void get_mount_points(void);

static ROXSOAPServerActions actions[]={
  {"Open", "Path", "Parent,Minimal,ID", rpc_Open, NULL},
  {"Options", NULL, NULL, rpc_Options, NULL},
  {"Change", "Path,ID", NULL, rpc_Change, NULL},

  {NULL},
};

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-ovhnr] [-a XID] [-m XID] [dir]\n",
	 argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
  printf("  -o\topen options window\n");
  printf("  -a XID\tX id of window to use in applet mode\n");
  printf("  -m XID\tX id of window to use in minimal applet mode\n");
  printf("  -n\tdon't attempt to contact existing server\n");
  printf("  -r\treplace existing server\n");
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
  printf("ROX-CLib version %s for GTK+ %s (built with %d.%d.%d)\n",
	 rox_clib_version_string(), rox_clib_gtk_version_string(),
	 ROX_CLIB_VERSION/10000, (ROX_CLIB_VERSION%10000)/100,
	 ROX_CLIB_VERSION%100);

  printf("\nCompile time options:\n");
  printf("  GTK+... version 2 (%d.%d.%d)\n", gtk_major_version,
	 gtk_minor_version,
	 gtk_micro_version);
  printf("  Debug output... %s\n", DEBUG? "yes": "no");
  printf("  Support drag & drop to applet... %s\n", APPLET_DND? "yes": "no");
  printf("  Using XML... libxml version %d)\n", LIBXML_VERSION);
  
}

int main(int argc, char *argv[])
{
  gchar *df_dir;
  gchar *fname;
  guint xid=0;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  int c, do_exit, nerr;
  const char *options="vha:nrom:";
  gboolean minimal=FALSE;
  gboolean new_run=FALSE;
  gboolean replace_server=FALSE;
  gboolean show_options=FALSE;

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }

  rox_init_with_domain(PROJECT, DOMAIN, &argc, &argv);

  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  localedir=rox_resources_find(PROGRAM, "Messages", ROX_RESOURCES_NO_LANG);
  bindtextdomain(PROGRAM, localedir);
  textdomain(PROGRAM);
  g_free(localedir);
#endif
  
  /* Process remaining arguments */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, options))!=EOF)
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
    case 'n':
      new_run=TRUE;
      break;
    case 'r':
      replace_server=TRUE;
      break;
    case 'o':
      show_options=TRUE;
      break;
    case 'm':
      minimal=TRUE;
      xid=atol(optarg);
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

  init_options();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=rox_resources_find(PROJECT, "gtkrc", ROX_RESOURCES_DEFAULT_LANG);
  if(!fname)
    fname=rox_choices_load("gtkrc", PROGRAM, DOMAIN);
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
  
  if(replace_server || !rox_soap_ping(PROJECT)) {
    dprintf(1, "Making SOAP server");
    server=rox_soap_server_new(PROJECT, FREEFS_NAMESPACE_URL);
    rox_soap_server_add_actions(server, actions);
    
  } else if(!new_run) {
    if(show_options) {
      if(options_remote()) {
	return 0;
      } else {
	rox_error("Could not connect to server, exiting");
	exit(22);
      }
    }
    if(open_remote(xid, df_dir, minimal)) {
      dprintf(1, "success in open_remote(%lu), exiting", xid);
      return 0;
    } else {
      rox_error("Could not connect to server, running standalone");
    }
  }

  if(show_options) 
    show_config_win(0);

  (void) make_window(xid, df_dir, minimal, NULL);
  g_free(df_dir);
  
  rox_main_loop();

  dprintf(2, "server=%p", server);
  if(server)
    rox_soap_server_delete(server);

  return 0;
}

static void remove_window(FreeWindow *win)
{
  if(win==current_window)
    current_window=NULL;
  
  windows=g_list_remove(windows, win);
  /*gtk_widget_hide(win->win);*/

  gtk_timeout_remove(win->update_tag);
  g_free(win->df_dir);
  g_free(win->real_path);
  g_free(win->fs_mount);
  g_free(win);

}

static void window_gone(GtkWidget *widget, gpointer data)
{
  FreeWindow *fw=(FreeWindow *) data;

  dprintf(1, "Window gone: %p %p", widget, fw);

  remove_window(fw);
}

static FreeWindow *make_window(guint32 xid, const char *dir,
			       gboolean mini, const gchar *id)
{
  FreeWindow *fwin;
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
  static GtkTooltips *ttips=NULL;
  char tbuf[1024], *home;
  gchar *fname;

  dprintf(1, "make_window(%lu, %s)", xid, dir? dir: "NULL");
  
  if(!ttips)
    ttips=gtk_tooltips_new();
  
  fwin=g_new0(FreeWindow, 1);

  fwin->df_dir=g_strdup(dir);
  fwin->real_path=NULL;
  fwin->fs_mount=NULL;
  fwin->minimal=mini;

  if(!xid) {
    /* Full window mode */

    /* Construct our window and bits */
    fwin->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(fwin->win, "fs free");
    gtk_window_set_title(GTK_WINDOW(fwin->win), dir);
    g_signal_connect(fwin->win, "destroy", 
		       G_CALLBACK(window_gone), 
		       fwin);
    g_signal_connect(fwin->win, "button_press_event",
		       G_CALLBACK(button_press), fwin);
    gtk_widget_realize(fwin->win);
    gtk_widget_add_events(fwin->win, GDK_BUTTON_PRESS_MASK);
    rox_dnd_register_uris(fwin->win, 0, handle_uris, fwin);

    gtk_tooltips_set_tip(ttips, fwin->win,
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);
  
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(fwin->win), vbox);
    gtk_widget_show(vbox);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show(hbox);

    label=gtk_label_new(_("Mounted on"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fwin->fs_name=gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(fwin->fs_name), GTK_JUSTIFY_LEFT);
    gtk_widget_set_name(fwin->fs_name, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_name, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_name);
    gtk_tooltips_set_tip(ttips, fwin->fs_name,
			 "This is the name of the directory\n"
			 "the file system is mounted on", TIP_PRIVATE);
  
    label=gtk_label_new(_("Total:"));
    gtk_widget_set_name(label, "simple label");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show(label);

    fwin->fs_total=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_total, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_total, FALSE, FALSE, 2);
    gtk_widget_show(fwin->fs_total);
    gtk_tooltips_set_tip(ttips, fwin->fs_total,
			 "This is the total size of the file system",
			 TIP_PRIVATE);
  
    hbox=gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);
    gtk_widget_show(hbox);
    
    fwin->fs_used=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_used, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_used, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_used);
    gtk_tooltips_set_tip(ttips, fwin->fs_used,
			 "This is the space used on the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_used: %s window",
	    GTK_WIDGET_NO_WINDOW(fwin->fs_used)? "no": "has a");
  
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
    fwin->fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fwin->fs_per, "gauge");
    gtk_widget_set_size_request(fwin->fs_per, 240, 24);
    gtk_widget_show(fwin->fs_per);
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_per, TRUE, TRUE, 2);
    gtk_tooltips_set_tip(ttips, fwin->fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);
    dprintf(3, "fs_per: %s window",
	    GTK_WIDGET_NO_WINDOW(fwin->fs_per)? "no": "has a");
  
    fwin->fs_free=gtk_label_new("XXxxx ybytes");
    gtk_widget_set_name(fwin->fs_free, "text display");
    gtk_box_pack_start(GTK_BOX(hbox), fwin->fs_free, TRUE, FALSE, 2);
    gtk_widget_show(fwin->fs_free);
    gtk_tooltips_set_tip(ttips, fwin->fs_free,
			 "This is the space available on the file system",
			 TIP_PRIVATE);

    fwin->is_applet=FALSE;
  } else {
    /* We are an applet, plug ourselves in */
    GtkWidget *plug;
    int i;

    dprintf(3, "xid=0x%x", xid);
    plug=gtk_plug_new(xid);
    g_signal_connect(plug, "destroy", 
		       G_CALLBACK(window_gone), 
		       fwin);
    if(!mini)
      gtk_widget_set_size_request(plug, opt_applet_size.int_value,
				  opt_applet_size.int_value);
    dprintf(4, "set_usize %d\n", opt_applet_size.int_value);
    
    g_signal_connect(plug, "button_press_event",
		       G_CALLBACK(button_press), fwin);
    gtk_widget_add_events(plug, GDK_BUTTON_PRESS_MASK);
    gtk_tooltips_set_tip(ttips, plug,
			 "FreeFS shows the space usage on a single "
			 "file system",
			 TIP_PRIVATE);

#if APPLET_DND
    dprintf(3, "make drop target for plug");
    rox_dnd_register_uris(plug, 0, handle_uris, NULL);
#endif

    dprintf(4, "vbox new");
    vbox=gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(plug), vbox);
    gtk_widget_show(vbox);

    if(!mini) {
      dprintf(4, "alignment new");
      align=gtk_alignment_new(1, 0.5, 1, 0);
      gtk_box_pack_end(GTK_BOX(vbox), align, TRUE, TRUE, 2);
      gtk_widget_show(align);

      dprintf(4, "label new");
      fwin->fs_name=gtk_label_new("");
      gtk_label_set_justify(GTK_LABEL(fwin->fs_name), GTK_JUSTIFY_RIGHT);
      gtk_widget_set_name(fwin->fs_name, "text display");
      gtk_container_add(GTK_CONTAINER(align), fwin->fs_name);
      if(opt_applet_show_dir.int_value)
	gtk_widget_show(fwin->fs_name);
      gtk_tooltips_set_tip(ttips, fwin->fs_name,
			   "This shows the dir where the FS is mounted",
			   TIP_PRIVATE);
    } else {
      fwin->fs_name=NULL;
    }

    fwin->fs_per=gtk_progress_bar_new();
    gtk_widget_set_name(fwin->fs_per, "gauge");
    gtk_widget_set_size_request(fwin->fs_per, -1, 6);
    gtk_widget_show(fwin->fs_per);
    gtk_box_pack_end(GTK_BOX(vbox), fwin->fs_per, FALSE, FALSE, 2);
    gtk_tooltips_set_tip(ttips, fwin->fs_per,
			 "This shows the relative usage of the file system",
			 TIP_PRIVATE);

    dprintf(5, "show plug");
    gtk_widget_show(plug);
    dprintf(5, "made applet");

    fwin->win=plug;
    fwin->is_applet=TRUE;
  }
  if(!menu)
    menu_create_menu(fwin->win);


  gtk_signal_connect(GTK_OBJECT(fwin->win), "popup-menu",
		     GTK_SIGNAL_FUNC(popup_menu), fwin);
  
  dprintf(3, "update_fs_values(%s)", fwin->df_dir);
  update_fs_values(fwin);

  dprintf(3, "show %p (%ld)", fwin->win, xid);
  if(!xid)
    gtk_widget_show(fwin->win);
  dprintf(2, "timeout %ds, %p, %s", opt_update_sec.int_value,
	  (GtkFunction) update_fs_values, fwin->df_dir);
  fwin->update_tag=gtk_timeout_add(opt_update_sec.int_value*1000,
			 (GtkFunction) update_fs_values, fwin);
  dprintf(2, "update_sec=%d, update_tag=%u", opt_update_sec.int_value,
	  fwin->update_tag);

  if(id)
    fwin->id=g_strdup(id);
  else
    fwin->id=NULL;
  
  windows=g_list_append(windows, fwin);
  current_window=fwin;
  gtk_widget_ref(fwin->win);
  rox_add_window(fwin->win);

  return fwin;
}

static FreeWindow *find_window(const gchar *id)
{
  GList *p;

  for(p=windows; p; p=g_list_next(p)) {
    FreeWindow *f=(FreeWindow *) p->data;

    if(f->id && strcmp(f->id, id)==0) 
      return f;
  }

  return NULL;
}

/* Change the directory/file and therefore FS we are monitoring */
static void set_target(FreeWindow *fwin, const char *ndir)
{
  dprintf(3, "set_target(\"%s\")", ndir);
  
  if(fwin->df_dir)
    g_free(fwin->df_dir);
  fwin->df_dir=g_strdup(ndir);

  gtk_timeout_remove(fwin->update_tag);
  update_fs_values(fwin);
  fwin->update_tag=gtk_timeout_add(opt_update_sec.int_value*1000,
			 (GtkFunction) update_fs_values, fwin);
  gtk_window_set_title(GTK_WINDOW(fwin->win), fwin->df_dir);
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

int compare_mount_info(const void *a, const void *b)
{
  const MountInfo *ma=(const MountInfo *) a;
  const MountInfo *mb=(const MountInfo *) b;

  return strcmp(ma->mount_point, mb->mount_point);
}

static MountInfo *next_mount_point(FILE *table)
{
  MountInfo *entry=NULL;
  
#if defined(HAVE_MNTENT_H)
  struct mntent *ment;
#elif defined(HAVE_SYS_MNTTAB_H)
  struct mnttab ment;
#endif
  
#if defined(HAVE_MNTENT_H)
  ment=getmntent(table);
  if(!ment)
    return NULL;

  rox_debug_printf(3, "ment=%p %s %s %s", ment, ment->mnt_dir,
		   ment->mnt_fsname, ment->mnt_type);
  entry=g_new(MountInfo, 1);
  entry->mount_point=g_strdup(ment->mnt_dir);
  entry->device=g_strdup(ment->mnt_fsname);
  entry->type=g_strdup(ment->mnt_type);
  
#elif defined(HAVE_SYS_MNTTAB_H)
  if(getmntent(table, &ment)!=0)
    return NULL;
  
  entry=g_new(MountInfo, 1);
  entry->mount_point=g_strdup(ment.mnt_mountp);
  entry->device=g_strdup(ment.mnt_special);
  entry->type=g_strdup(ment.mnt_fstype);

#endif
  if(entry)
    rox_debug_printf(3, "%s %s %s", entry->mount_point,
		     entry->device, entry->type);

  return entry;
}

static void get_mount_points(void)
{
  GList *lp;
  MountInfo *entry;
  FILE *f;

  if(mount_points) {
    for(lp=mount_points; lp; lp=g_list_next(lp)) {
      entry=(MountInfo *) lp->data;
      g_free(entry->mount_point);
      g_free(entry->device);
      g_free(entry->type);
      g_free(entry);
    }
    g_list_free(mount_points);
    mount_points=NULL;
  }

#ifdef HAVE_SETMNTENT
  f=setmntent(MTAB, "r");
#else
  f=fopen(MTAB, "r");
#endif
  if(!f)
    return;

  for(entry=next_mount_point(f); entry; entry=next_mount_point(f))
    mount_points=g_list_append(mount_points, entry);
  
#ifdef HAVE_SETMNTENT
  endmntent(f);
#else
  fclose(f);
#endif

  g_list_sort(mount_points, compare_mount_info);
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
  int i;
  GList *lp;
  MountInfo *entry;

#ifdef HAVE_REALPATH
  dprintf(3, "Calling realpath for %s", fname);
  if(!realpath(fname, name))
    return _(failed);
  dprintf(3, "got %s", name);
#else
  strcpy(name, fname);
#endif
  
  if(prev && strcmp(prev, name)==0 && prev_ans) {
    dprintf(3, "return same answer as last time: %s", prev_ans);
    return prev_ans;
  }

  if(!mount_points)
    get_mount_points();
  
  strcpy(wnm, name);
  do {
    for(lp=mount_points; lp; lp=g_list_next(lp)) {
      entry=(MountInfo *) lp->data;
      dprintf(5, "%s - %s", wnm, entry->mount_point);
      if(strcmp(wnm, entry->mount_point)==0) {
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

static gboolean update_fs_values(FreeWindow *fwin)
{
  int ok=FALSE;

  dprintf(4, "fwin=%p", fwin);
  dprintf(4, "update_sec=%d, update_tag=%u", opt_update_sec.int_value,
	  fwin->update_tag);
  dprintf(3, "update_fs_values(\"%s\")", fwin->df_dir);
  
  if(fwin->df_dir) {
#if defined(HAVE_STATVFS)
    struct statvfs buf;
#elif defined(HAVE_STATFS)
    struct statfs buf;
#else
#error No way to get FS stats
#endif

    errno=0;
#if defined(HAVE_STATVFS)
    ok=statvfs(fwin->df_dir, &buf)==0;
#else
    ok=statfs(fwin->df_dir, &buf)==0;
#endif
    if(!ok)
      perror(fwin->df_dir);
    
    if(ok) {
      unsigned long long total, used, avail;
      int row;
      gdouble fused;
      char tbuf[32];
      
#if defined(HAVE_STATVFS)
      rox_debug_printf(5, "%lu %lu %lu %lu", buf.f_frsize, buf.f_blocks,
		       buf.f_bfree, buf.f_bavail);

      total=buf.f_frsize*(unsigned long long) buf.f_blocks;
      used=buf.f_frsize*(unsigned long long) (buf.f_blocks-buf.f_bfree);
      avail=buf.f_frsize*(unsigned long long) buf.f_bavail;
      dprintf(4, "%llu %llu %llu", total, used, avail);

      if(buf.f_blocks>0) {
	fused=(100.f*(buf.f_blocks-buf.f_bavail))/((gdouble) buf.f_blocks);
	if(fused>100.)
	  fused=100.;
      } else {
	fused=0.;
      }
#else
      dprintf(5, "%ld %ld %ld %ld", buf.f_bsize, buf.f_blocks, buf.f_bfree,
	     buf.f_bavail);

      total=buf.f_bsize*(unsigned long long) buf.f_blocks;
      used=buf.f_bsize*(unsigned long long) (buf.f_blocks-buf.f_bfree);
      avail=buf.f_bsize*(unsigned long long) buf.f_bavail;
      dprintf(4, "%llu %llu %llu", total, used, avail);

      if(buf.f_blocks>0) {
	fused=(100.f*(buf.f_blocks-buf.f_bavail))/((gdouble) buf.f_blocks);
	if(fused>100.)
	  fused=100.;
      } else {
	fused=0.;
      }
#endif
      dprintf(4, "%2.0f %%", fused);

      if(!fwin->is_applet) {
	dprintf(4, "mount point=%s", find_mount_point(fwin->df_dir));
	gtk_label_set_text(GTK_LABEL(fwin->fs_name),
			   find_mount_point(fwin->df_dir));
	gtk_label_set_text(GTK_LABEL(fwin->fs_total), fmt_size(total));
	gtk_label_set_text(GTK_LABEL(fwin->fs_used), fmt_size(used));
	gtk_label_set_text(GTK_LABEL(fwin->fs_free), fmt_size(avail));

      } else if(fwin->fs_name) {
	const char *mpt;
	
	dprintf(5, "set text");
	mpt=find_mount_point(fwin->df_dir);
	if(strcmp(mpt, "/")==0)
	  gtk_label_set_text(GTK_LABEL(fwin->fs_name), "/");
	else
	  gtk_label_set_text(GTK_LABEL(fwin->fs_name), g_basename(mpt));
      }
      dprintf(5, "set progress %f", fused);
      sprintf(tbuf, "%d%%", (int)(fused));
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(fwin->fs_per),
				    fused/100.);
      if(!fwin->minimal)
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(fwin->fs_per), tbuf);
    }
  }
  
  dprintf(4, "update_fs_values(%s), ok=%d, update_sec=%d", fwin->df_dir, ok,
	  opt_update_sec.int_value);
  
  if(!ok) {
    if(!fwin->is_applet) {
      gtk_label_set_text(GTK_LABEL(fwin->fs_name), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_total), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_used), "?");
      gtk_label_set_text(GTK_LABEL(fwin->fs_free), "?");
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(fwin->fs_per), 0.);
    if(!fwin->minimal)
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(fwin->fs_per), "");
  }

  return TRUE;
}

static void do_update(void)
{
  get_mount_points();
  update_fs_values(current_window);
}

static void opts_changed(void)
{
  if(opt_applet_size.has_changed) {
    GList *p;

    for(p=windows; p; p=g_list_next(p)) {
      FreeWindow *fw=(FreeWindow *) p->data;
      if(fw && fw->is_applet && !fw->minimal) {
	gtk_widget_set_size_request(fw->win, opt_applet_size.int_value,
				    opt_applet_size.int_value);
      }
    }
  }
}

#define UPDATE_RATE "UpdateRate"
#define INIT_SIZE   "AppletInitSize"
#define SHOW_DIR    "AppletShowDir"
#define EXCLUDE_FS  "ExcludeFS"

static void init_options(void)
{
  guint update_sec=5;           /* How often to update */
  guint applet_init_size=32;    /* Initial size of applet */
  gboolean applet_show_dir=TRUE;/* Print name of directory on applet version */

  option_add_int(&opt_update_sec, "update_rate", update_sec);
  option_add_int(&opt_applet_size, "applet_size", applet_init_size);
  option_add_int(&opt_applet_show_dir, "show_dir", applet_show_dir);

  option_add_notify(opts_changed);
}


static void show_info_win(void)
{
  GtkWidget *infowin;

  infowin=rox_info_win_new_from_appinfo(PROGRAM);
  rox_add_window(infowin);

  gtk_widget_show(infowin);
}

static void do_opendir(gpointer dat, guint action, GtkWidget *wid)
{
  const char *dir=current_window->df_dir;

  rox_debug_printf(2, "do_opendir %p %d %s", current_window, action,
		   dir);

  if(action)
    dir=find_mount_point(dir);

  rox_debug_printf(3, "open %s", dir);
  rox_filer_open_dir(dir);
}

static void close_window(void)
{
  FreeWindow *fwin=current_window;

  dprintf(1, "close_window %p %p", fwin, fwin->win);

  gtk_widget_hide(fwin->win);
  gtk_widget_unref(fwin->win);
  gtk_widget_destroy(fwin->win);
}

static void show_config_win(int ignored)
{
  options_show();
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0,
                                "<StockItem>", GTK_STOCK_DIALOG_INFO},
  { N_("/sep"), 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Configure..."),	NULL, show_config_win, 0, 
                                "<StockItem>", GTK_STOCK_PREFERENCES},
  { N_("/Update Now"),	        NULL, do_update, 0,   
                                "<StockItem>", GTK_STOCK_REFRESH},
  { N_("/Open"),                NULL, NULL, 0, "<Branch>"},
  { N_("/Open/Directory"),	NULL, do_opendir, 0,     
                                "<StockItem>", GTK_STOCK_OPEN},
  { N_("/Open/FS root"),	NULL, do_opendir, 1,       
                                "<StockItem>", GTK_STOCK_OPEN},
  { N_("/Scan"),                NULL, NULL, 0, NULL},
  { N_("/sep"), 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Close"), 	        NULL, close_window, 0,      
                                "<StockItem>", GTK_STOCK_CLOSE},
  { N_("/sep"), 	        NULL, NULL, 0, "<Separator>" },
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0,      
                                "<StockItem>", GTK_STOCK_QUIT},
};

static void save_menus(void)
{
  char	*menurc;
	
  menurc = rox_choices_save("menus", PROJECT, DOMAIN);
  if (menurc) {
    gtk_accel_map_save(menurc);
    g_free(menurc);
  }
}

static GtkWidget *scan_menu=NULL;

static void menu_set_target(gchar *mount)
{
  dprintf(3, "menu_set_target(%s)", mount? mount: "NULL");

  set_target(current_window, mount);
}

static void menu_item_destroyed(GtkWidget *item, gpointer data)
{
  dprintf(3, "menu_item_destroyed(%p, %p)", item, data);
  g_free(data);
}

static void update_menus(void)
{
  GtkWidget *item;
  gchar *mount;
  gchar *lbl;
  GtkWidget *mount_menu, *l;
  int i;
  
  mount_menu=gtk_menu_new();

  dprintf(3, "mount_menu=%p", mount_menu);
  if(!mount_points)
    get_mount_points();
  if(mount_points) {
    GList *lp;
    MountInfo *entry;
    
    for(lp=mount_points; lp; lp=g_list_next(lp)) {
      entry=(MountInfo *) lp->data;
      
      if(g_list_find_custom(fs_exclude, entry->type, (GCompareFunc) strcmp))
	continue;

      mount=g_strdup(entry->mount_point);
      lbl=g_strdup_printf("<b>%s</b> (%s)", entry->mount_point, entry->device);

      dprintf(2, "mount=%s type=%s", mount, entry->type);

      /*item=gtk_menu_item_new_with_label(lbl);*/
      item=gtk_menu_item_new();
      l=gtk_label_new("");
      gtk_misc_set_alignment(GTK_MISC(l), 0.0, 0.5);
      gtk_label_set_markup(GTK_LABEL(l), lbl);
      gtk_container_add(GTK_CONTAINER(item), l);
      gtk_signal_connect_object(GTK_OBJECT(item), "activate",
				GTK_SIGNAL_FUNC(menu_set_target), mount);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(menu_item_destroyed), mount);
      gtk_menu_shell_append(GTK_MENU_SHELL(mount_menu), item);

      g_free(lbl);
    }
  }

  gtk_widget_show_all(mount_menu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(scan_menu), mount_menu);
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
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  menurc=rox_choices_load("menus", PROJECT, DOMAIN);
  if(menurc) {
    gtk_accel_map_load(menurc);
    g_free(menurc);
  }

  atexit(save_menus);

  menu = gtk_item_factory_get_widget(item_factory, "<system>");

  /*name=g_strdup_printf("%s")*/
  scan_menu=gtk_item_factory_get_widget(item_factory,
						"/Scan");
  dprintf(3, "scan_menu=%p", scan_menu);
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer udata)
{
  FreeWindow *fwin=(FreeWindow *) udata;
  
  current_window=fwin;

  rox_debug_printf(3, "in button_press %d", bev->button);
  if(bev->type==GDK_BUTTON_PRESS) {
    if(bev->button==3) {
      rox_debug_printf(3, "menu=%p", menu);
      if(!menu)
	menu_create_menu(window);
      update_menus();
      rox_debug_printf(3, "menu=%p", menu);
      
      if(fwin->is_applet) {
	applet_popup_menu(fwin->win, menu, bev);
      } else {
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		     bev->button, bev->time);
      }
      return TRUE;
    } else if(bev->button==1 && fwin->is_applet) {
      FreeWindow *nwin=make_window(0, fwin->df_dir, FALSE);

      gtk_widget_show(nwin->win);

      return TRUE;
    }
  }

  return FALSE;
}

static gboolean popup_menu(GtkWidget *window, gpointer udata)
{
  FreeWindow *fwin=(FreeWindow *) udata;

  if(!menu) 
    menu_create_menu(GTK_WIDGET(fwin->win));
  update_menus();

  if(fwin->is_applet)
    applet_popup_menu(fwin->win, menu, NULL);
  else
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		   0, gtk_get_current_event_time());

  return TRUE;
  
}

static xmlNodePtr rpc_Open(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  xmlNodePtr path, parent=NULL, mini=NULL;
  gchar *str;
  guint32 xid=0;
  gboolean minimal=FALSE;
  gchar *dir;

  dprintf(3, "rpc_Open(%p, \"%s\", %p, %p)", server, action_name, args, udata);

  path=args->data;
  str=xmlNodeGetContent(path);
  dir=g_strdup(str);
  g_free(str);

  args=g_list_next(args);
  if(args)
    parent=args->data;
  if(parent) {

    str=xmlNodeGetContent(parent);
    if(str) {
      xid=(guint32) atol(str);
      g_free(str);
    }
  }

  if(args)
    args=g_list_next(args);
  if(args)
    mini=args->data;
  if(mini) {

    str=xmlNodeGetContent(mini);
    if(str) {
      minimal=(strcmp(str, "yes")==0);
      g_free(str);
    }
  }

  if(dir && dir[0])
    make_window(xid, dir, minimal);
  else {
    rox_error("Invalid remote call, Path not given");
  }
  g_free(dir);

  dprintf(3, "rpc_Open complete");

  return NULL;
}

static xmlNodePtr rpc_Change(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  xmlNodePtr path, id;
  gchar *str;
  gchar *dir, *ids;
  FreeWindow *fwin;

  dprintf(3, "rpc_Change(%p, \"%s\", %p, %p)", server, action_name,
	  args, udata);

  path=args->data;
  str=xmlNodeGetContent(path);
  dir=g_strdup(str);
  g_free(str);

  args=g_list_next(args);
  id=args->data;
  str=xmlNodeGetContent(id);
  ids=g_strdup(str);
  g_free(str);

  fwin=find_window(ids);
  if(fwin) {
    set_target(fwin, dir);
  }

  g_free(ids);
  g_free(dir);
}

static xmlNodePtr rpc_Options(ROXSOAPServer *server, const char *action_name,
			   GList *args, gpointer udata)
{
  dprintf(3, "rpc_Options(%p, \"%s\", %p, %p)", server, action_name,
	  args, udata);

  show_config_win(0);

  dprintf(3, "rpc_Options complete");

  return NULL;
}

static void open_callback(ROXSOAP *serv, gboolean status, 
				  xmlDocPtr reply, gpointer udata)
{
  gboolean *s=udata;
  
  dprintf(3, "In open_callback(%p, %d, %p, %p)", serv, status, reply,
	 udata);
  *s=status;
  gtk_main_quit();
}

static gboolean open_remote(guint32 xid, const char *path, gboolean mini)
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

  doc=rox_soap_build_xml("Open", FREEFS_NAMESPACE_URL, &node);
  if(!doc) {
    dprintf(3, "Failed to build XML doc");
    rox_soap_close(serv);
    return FALSE;
  }

  xmlNewChild(node, NULL, "Path", path);
  if(xid) {
    sprintf(buf, "%lu", xid);
    xmlNewChild(node, NULL, "Parent", buf);
  }
  xmlNewChild(node, NULL, "Minimal", mini? "yes": "no");

  sent=rox_soap_send(serv, doc, FALSE, open_callback, &ok);
  dprintf(3, "sent %d", sent);
  /*xmlDocDump(stdout, doc);*/

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
  dprintf(3, "server for %s is %p", PROJECT, serv);
  if(!serv)
    return FALSE;

  doc=rox_soap_build_xml("Options", FREEFS_NAMESPACE_URL, &node);
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

static gboolean handle_uris(GtkWidget *widget, GSList *uris,
			    gpointer data, gpointer udata)
{
  FreeWindow *fwin=(FreeWindow *) udata;
  GSList *local;

  local=rox_dnd_filter_local(uris);

  dprintf(2, "URI dropped, local list is %p (%s)", local,
	  local? local->data: "NULL");

  if(local) {
    GSList *tmp;
    
    set_target(fwin, (const char *) local->data);

    rox_dnd_local_free(local);
  }

  return FALSE;
}

/*
 * $Log: freefs.c,v $
 * Revision 1.27  2004/05/10 18:36:22  stephen
 * Eliminate libgtop
 *
 * Revision 1.26  2004/05/05 19:20:33  stephen
 * Extra debug
 *
 * Revision 1.25  2004/04/12 13:33:56  stephen
 * Remove dead code.  Open options dialog from command line or SOAP message.  Stock items in menus.
 *
 * Revision 1.24  2004/02/14 13:49:26  stephen
 * Use rox_init().  Fix load of icon.
 *
 * Revision 1.23  2003/06/22 09:06:08  stephen
 * Use new options system. New icon provided by Geoff Youngs.
 *
 * Revision 1.22  2003/03/05 15:30:40  stephen
 * First pass at conversion to GTK 2.
 *
 * Revision 1.21  2002/10/19 14:32:05  stephen
 * Fixed bug when applet monitors FS mounted as /
 *
 * Revision 1.20  2002/08/24 16:40:31  stephen
 * Fix compilation problem with libxml2.
 * Remove [] from libgtop test in configure.in
 *
 * Revision 1.19  2002/04/29 08:19:39  stephen
 * Added menu of mounted file systems so you can choose which to monitor,
 * otherwise there was no way of changing the FS in the applet version.
 * Fixed memory leak which manifested if you monitored more than one FS.
 * Use replacement applet menu positioning code (from ROX-CLib).
 *
 * Revision 1.18  2002/04/12 10:21:42  stephen
 * Multiple windows, SOAP server
 *
 * Revision 1.17  2002/03/04 11:41:08  stephen
 * Now stable release 1.2.0
 *
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
