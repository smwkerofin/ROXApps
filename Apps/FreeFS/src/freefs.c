/*
 * %W% %E%
 *
 * freefs - Monitor free space on a single file system.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <fcntl.h>
/*#include <sys/mnttab.h>*/ /* Replaced by glibtop */
#include <sys/statvfs.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include "infowin.h"

#include <glibtop.h>
#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

#include "config.h"
#include "choices.h"

/* For SCCS (but ignored by gcc) */
#pragma ident "%W% %E%"

/* The icon */
#include "../pixmaps/freefs.xpm"

/*#define MNTTAB "/etc/mnttab"*/ /* Replaced by glibtop */

/* Enable the drag and drop stuff */
#define TRY_DND 1

/* GTK+ objects */
static GtkWidget *win;
static GtkWidget *fs_name, *fs_total, *fs_used, *fs_free, *fs_per;
static GtkWidget *menu=NULL;
static guint update_tag=0;

static int update_sec=5;   /* How often to update */
static char *df_dir=NULL;  /* Directory to monitor */

/* Call backs & stuff */
static gboolean update_fs_values(gchar *mntpt);
static void do_update(void);
static void set_rate(GtkWidget *, gint sec);
static void read_choices(void);
static void menu_create_menu(void);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused);
#ifdef TRY_DND
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
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  char tbuf[1024], *home;
  guchar *fname;
  
  gtk_init(&argc, &argv);

  read_choices();

  /* Pick the gtkrc file up from CHOICESPATH */
  fname=choices_find_path_load("gtkrc", "FreeFS");
  if(fname) {
    gtk_rc_parse(fname);
    g_free(fname);
  }

  /* Figure out which directory we should be monitoring */
  if(argv[1])
    df_dir=g_strdup(argv[1]);
  else
    df_dir=g_dirname(argv[0]);
  if(!g_path_is_absolute(df_dir)) {
    gchar *tmp=df_dir;
    gchar *dir=g_get_current_dir();
    df_dir=g_strconcat(dir, "/", df_dir, NULL);
    g_free(dir);
    g_free(tmp);
  }

#ifdef TRY_DND
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
#ifdef TRY_DND
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

  label=gtk_label_new("Mounted on");
  gtk_widget_set_name(label, "simple label");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  fs_name=gtk_label_new("");
  gtk_widget_set_name(fs_name, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_name, TRUE, TRUE, 2);
  gtk_widget_show(fs_name);
#ifdef TRY_DND
  /*make_drop_target(fs_name);*/
#endif
  
  label=gtk_label_new("Total:");
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
#ifdef TRY_DND
  /*make_drop_target(fs_per);*/
#endif
  
  fs_free=gtk_label_new("XXxxx ybytes");
  gtk_widget_set_name(fs_free, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_free, TRUE, FALSE, 2);
  gtk_widget_show(fs_free);

  /* Set up glibtop and check now */
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_FSUSAGE)|(1<<GLIBTOP_SYSDEPS_MOUNTLIST),
		 0);
  update_fs_values(df_dir);
  
  gtk_widget_show(win);
  update_tag=gtk_timeout_add(update_sec*1000,
			 (GtkFunction) update_fs_values, df_dir);
  /*printf("update_sec=%d, update_tag=%u\n", update_sec, update_tag);*/

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
  update_tag=gtk_timeout_add(update_sec*1000,
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
  static const char *failed="(unknown)";
  static gchar *prev=NULL;
  static const gchar *prev_ans=NULL;
  
  const char *ans=NULL;
  char name[PATH_MAX];
  char *sl;
  glibtop_mountentry *ents;
  glibtop_mountlist list;
  int i;

  if(!realpath(fname, name))
    return failed;

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
  printf("update_sec=%d, update_tag=%u\n", update_sec, update_tag);
  printf("update_fs_values(\"%s\")\n", mntpt);
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
      /*printf("%2.0f %%\n", fused);*/

      /*printf("mount point=%s\n", find_mount_point(mntpt));*/
      gtk_label_set_text(GTK_LABEL(fs_name), find_mount_point(mntpt));
      gtk_label_set_text(GTK_LABEL(fs_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(fs_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(fs_free), fmt_size(avail));

      gtk_progress_set_value(GTK_PROGRESS(fs_per), fused);

    }
  }
  /*
  printf("update_fs_values(%s), ok=%d, update_sec=%d\n", mntpt, ok, update_sec);
  */
  if(!ok) {
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
	      update_sec=atoi(sep+1);
	      if(update_sec<1)
		update_sec=1;
	      /*printf("update_sec now %d\n", update_sec);*/
	    }
	  }
	}
      }
    } while(line);

    fclose(in);
  } else {
    FILE *out;
    
    fname=choices_find_path_save("Config", "FreeFS", TRUE);
    if(!fname)
      return;
    out=fopen(fname, "w");
    g_free(fname);
    if(!out)
      return;

    fprintf(out, "# Config file for FreeFS\n");
    fprintf(out, "%s: %d\n", UPDATE_RATE, update_sec);
    fclose(out);
  }
}

static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    infowin=info_win_new(PROGRAM, PURPOSE, VERSION, AUTHOR, WEBSITE);
  }

  gtk_widget_show(infowin);
}

/* Pop-up menu */
static GtkItemFactoryEntry menu_items[] = {
  { "/Info",		NULL, show_info_win, 0, NULL },
  { "/Update",		NULL, NULL, 0, "<Branch>" },
  { "/Update/Now",	NULL, do_update, 0, NULL },
  { "/Update/",	NULL, NULL, 0, "<Separator>" },
  { "/Update/Every second",	NULL, set_rate, 0, "<RadioItem>" },
  { "/Update/Every 5s",	NULL, set_rate, 1, "/Update/Every second" },
  { "/Update/Every 10s",	NULL, set_rate, 2, "/Update/Every second" },
  { "/Update/Every 30s",	NULL, set_rate, 3, "/Update/Every second" },
  { "/Quit",	NULL, gtk_main_quit, 0, NULL },
};

static GtkWidget *update_menu;

/* Mark a radio item on the menu as selected */
static void menu_select_radio_item(GtkWidget *menu, gint i)
{
  GList	*items, *item;

  items=gtk_container_children(GTK_CONTAINER(menu));

  item=g_list_nth(items, i);

  /*printf("toggle %p\n", item->data);*/
  gtk_check_menu_item_toggled(GTK_CHECK_MENU_ITEM(item->data));
}

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
  update_menu=gtk_item_factory_get_widget(item_factory, "<system>/Update");
  
  menu_select_radio_item(update_menu, 1+2);
}

/* Call back from menu to set update rate */
static void set_rate(GtkWidget *widget, gint rate)
{
  int secs[]={1, 5, 10, 30};

  /*printf("set_rate(%p, %d)\n", widget, rate);*/

  if(rate<0 || rate>=4)
    return;
  
  gtk_timeout_remove(update_tag);

  update_sec=secs[rate];
  update_tag=gtk_timeout_add(update_sec*1000,
			 (GtkFunction) update_fs_values, df_dir);
  /*printf("set rate to %d\n", update_sec);*/
  /*printf("update_sec=%d, update_tag=%u\n", update_sec, update_tag);*/

  menu_select_radio_item(update_menu, rate+2);
}

static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer unused)
{
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    if(!menu)
      menu_create_menu();

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

#ifdef TRY_DND

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
      fprintf(stderr, "uri_list_to_gslist: Incorrect or missing line "
		      "break in text/uri-list data\n");
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
    error = "No URIs in the text/uri-list (nothing to do!)";
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
    fprintf(stderr, "drag_data_received: unknown target\n");
    gtk_drag_finish(context, FALSE, FALSE, time);
    break;
  }
}

#endif
