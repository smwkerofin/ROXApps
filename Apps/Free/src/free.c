/*
 * %W% %E%
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mnttab.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include "choices.h"

#pragma ident "%W% %E%"

#define MNTTAB "/etc/mnttab"

/* The icon */
#include "../pixmaps/free.xpm"

static GtkWidget *fs_list, *fs_name, *fs_total, *fs_used, *fs_free, *fs_per;
static GtkWidget *tipq, *help_win, *help_text, *status_bar;
static int update_sec=10;
#define FULL_UPDATE_SEC 600
static time_t last_read_mnttab=0;

static GList *exclude_fs=NULL;

static void find_fs(void);
static void status(const char *, ...);
static void report_errno(const char *);
static void read_choices(void);

/* Call backs */
static void help_menu(gpointer dat, guint action, GtkWidget *);
static void change_update(gpointer dat, guint action, GtkWidget *);
static void fs_sel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data);
static int update_all_entries(GtkWidget *);

static GtkItemFactoryEntry menu_items[] = {
  {"/_File", NULL, NULL, 0, "<Branch>"},
  {"/File/_Update list","<control>U",  find_fs, 0, NULL },
  {"/File/E_xit","<control>X",  gtk_main_quit, 0, NULL },
  {"/_View", NULL, NULL, 0, "<Branch>"},
  {"/View/Update FS _more often","<control>M",  change_update, -1, NULL },
  {"/View/Update FS _less often","<control>L",  change_update, +1, NULL },
  {"/_Help", NULL, NULL, 0, "<LastBranch>"},
  {"/Help/Help on item", "Help", help_menu, 1, NULL},
};

static void get_main_menu(GtkWidget *window, GtkWidget **menubar)
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

  accel_group = gtk_accel_group_new ();

  /* This function initializes the item factory.
     Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
              or GTK_TYPE_OPTION_MENU.
     Param 2: The path of the menu.
     Param 3: A pointer to a gtk_accel_group.  The item factory sets up
              the accelerator table while generating menus.
  */
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", 
				       accel_group);
  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach (accel_group, GTK_OBJECT (window));

  if (menubar)
    /* Finally, return the actual menu bar created by the item factory. */ 
    *menubar = gtk_item_factory_get_widget (item_factory, "<main>");
}

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

static void close_window(GtkWidget *but, gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);

  if(win)
    gtk_widget_hide(win);
}

static void show_help_popup(GtkWidget *tips_query, GtkWidget *widget,
			    const gchar *tip_text, const gchar *tip_private,
			    GdkEventButton *event, gpointer func_data)
{
  const gchar *help=tip_private;
  int nch;

  /*
  status("show_help_popup, text=%s private=%s\n", tip_text? tip_text: "(nil)",
	 tip_private? tip_private: "(nil)");
  */
  
  if(!help)
    help=tip_text;
  if(!help)
    return;
  
  gtk_text_freeze(GTK_TEXT(help_text));
  nch=gtk_text_get_length(GTK_TEXT(help_text));
  gtk_text_set_point(GTK_TEXT(help_text), 0);
  gtk_text_forward_delete(GTK_TEXT(help_text), nch);
  gtk_text_insert(GTK_TEXT(help_text), NULL, NULL, NULL, help, strlen(help));
  gtk_text_set_point(GTK_TEXT(help_text), 0);
  gtk_text_thaw(GTK_TEXT(help_text));
  
  gtk_widget_show(help_win);
}

int main(int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *menu;
  GtkWidget *scrw;
  GtkWidget *but;
  GtkAdjustment *adj;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkTooltips *ttips;
  char tbuf[1024], *home;
  gchar *dir;

  gchar *titles[]={"Filing System", "Type", "Mounted on", "Free"};
  
  gtk_init(&argc, &argv);

  dir=g_dirname(argv[0]);
  if(dir) {
    strcpy(tbuf, dir);
    strcat(tbuf, "/free.gtkrc");
    gtk_rc_parse(tbuf);
    g_free(dir);
  }
  home=getenv("HOME");
  if(home) {
    strcpy(tbuf, home);
    strcat(tbuf, "/.free.gtkrc");
    gtk_rc_parse(tbuf);
  }
  gtk_rc_parse(".free.gtkrc");

  read_choices();

  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "disc free");
  gtk_window_set_title(GTK_WINDOW(win), "Disc free");
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");
  gtk_widget_realize(win);
  /* Set the icon */
  style = gtk_widget_get_style(win);
  pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(win)->window,  &mask,
                                           &style->bg[GTK_STATE_NORMAL],
                                           (gchar **)free_xpm );
  gdk_window_set_icon(GTK_WIDGET(win)->window, NULL, pixmap, mask);

  ttips=gtk_tooltips_new();

  vbox=gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);
  
  get_main_menu(win, &menu);
  gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 2);
  gtk_widget_show(menu);

  /*
  hbox=gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label=gtk_label_new("Filing systems");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);
  */
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, 512, 320);
  gtk_box_pack_start(GTK_BOX(vbox), scrw, TRUE, TRUE, 2);
  
  fs_list=gtk_clist_new_with_titles(4, titles);
  gtk_widget_set_name(fs_list, "fs list");
  gtk_clist_set_column_width(GTK_CLIST(fs_list), 0, 160);
  gtk_clist_set_column_width(GTK_CLIST(fs_list), 1, 64);
  gtk_clist_set_column_width(GTK_CLIST(fs_list), 2, 160);
  gtk_clist_set_column_justification(GTK_CLIST(fs_list), 3,
				     GTK_JUSTIFY_RIGHT);
  gtk_clist_set_selection_mode(GTK_CLIST(fs_list), GTK_SELECTION_SINGLE);
  gtk_clist_column_titles_passive(GTK_CLIST(fs_list));
  gtk_signal_connect(GTK_OBJECT(fs_list), "select_row",
		       GTK_SIGNAL_FUNC(fs_sel),
		       GINT_TO_POINTER(TRUE));
  gtk_signal_connect(GTK_OBJECT(fs_list), "unselect_row",
		       GTK_SIGNAL_FUNC(fs_sel),
		       GINT_TO_POINTER(FALSE));
  gtk_widget_show(fs_list);
  gtk_container_add(GTK_CONTAINER(scrw), fs_list);
  
  gtk_tooltips_set_tip(ttips, fs_list,
		       "This is a list of mounted file systems",
		       "This is a list of mounted file systems");

  hbox=gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label=gtk_label_new("Mounted on");
  gtk_widget_set_name(label, "simple label");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
  gtk_widget_show(label);
  gtk_tooltips_set_tip(ttips, label,
		       "Directory where the FS is mounted",
		       NULL);

  fs_name=gtk_label_new("");
  gtk_widget_set_name(fs_name, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_name, FALSE, FALSE, 2);
  gtk_widget_show(fs_name);
  gtk_tooltips_set_tip(ttips, fs_name,
		       "Directory where the FS is mounted",
		       NULL);
  
  hbox=gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  label=gtk_label_new("Total:");
  gtk_widget_set_name(label, "simple label");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 2);
  gtk_widget_show(label);
  gtk_tooltips_set_tip(ttips, label,
		       "Total size of FS",
		       NULL);

  fs_total=gtk_label_new("XXxxx ybytes");
  gtk_widget_set_name(fs_total, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_total, TRUE, FALSE, 2);
  gtk_widget_show(fs_total);
  gtk_tooltips_set_tip(ttips, fs_total,
		       "Total size of FS",
		       NULL);
  
  label=gtk_label_new("Used:");
  gtk_widget_set_name(label, "simple label");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 2);
  gtk_widget_show(label);
  gtk_tooltips_set_tip(ttips, label,
		       "Space used on FS",
		       NULL);

  fs_used=gtk_label_new("XXxxx ybytes");
  gtk_widget_set_name(fs_used, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_used, TRUE, FALSE, 2);
  gtk_widget_show(fs_used);
  gtk_tooltips_set_tip(ttips, fs_used,
		       "Space used on FS",
		       NULL);
  
  label=gtk_label_new("Free:");
  gtk_widget_set_name(label, "simple label");
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 2);
  gtk_widget_show(label);
  gtk_tooltips_set_tip(ttips, label,
		       "Space free on FS",
		       NULL);

  fs_free=gtk_label_new("XXxxx ybytes");
  gtk_widget_set_name(fs_free, "text display");
  gtk_box_pack_start(GTK_BOX(hbox), fs_free, TRUE, FALSE, 2);
  gtk_widget_show(fs_free);
  gtk_tooltips_set_tip(ttips, fs_free,
		       "Space free on FS",
		       NULL);
  
  hbox=gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
  gtk_widget_show(hbox);

  adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 100, 0, 0, 0);
  
  fs_per=gtk_progress_bar_new();
  gtk_widget_set_name(fs_per, "guage");
  gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(fs_per),
				   GTK_PROGRESS_CONTINUOUS);
  gtk_progress_set_format_string(GTK_PROGRESS(fs_per), "%p%%");
  gtk_progress_set_show_text(GTK_PROGRESS(fs_per), TRUE);
  gtk_widget_set_usize(fs_per, 240, 24);
  gtk_widget_show(fs_per);
  gtk_box_pack_start(GTK_BOX(hbox), fs_per, TRUE, TRUE, 2);
  gtk_tooltips_set_tip(ttips, fs_per,
		       "Pecentage used on FS",
		       "This shows the pecentage used on the filing system");

  tipq=gtk_tips_query_new();
  gtk_tips_query_set_caller(GTK_TIPS_QUERY(tipq), menu);
  gtk_box_pack_start(GTK_BOX(vbox), tipq, FALSE, FALSE, 2);
  gtk_widget_show(tipq);
  gtk_signal_connect(GTK_OBJECT (tipq), "widget_selected",
                        GTK_SIGNAL_FUNC(show_help_popup), NULL);
  
  status_bar=gtk_statusbar_new();
  gtk_box_pack_end(GTK_BOX(vbox), status_bar, FALSE, FALSE, 2);
  gtk_widget_show(status_bar);
  
  help_win=gtk_dialog_new();
  gtk_widget_set_name(help_win, "help popup");
  gtk_window_set_position(GTK_WINDOW(help_win), GTK_WIN_POS_MOUSE);
  gtk_window_set_policy(GTK_WINDOW(help_win), FALSE, FALSE, FALSE);
  gtk_window_set_title(GTK_WINDOW(help_win), "Help");
  gtk_signal_connect(GTK_OBJECT(help_win), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     help_win);
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, 400, 150);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_win)->vbox), scrw, FALSE,
		     FALSE, 2);

  help_text=gtk_text_new(NULL, NULL);
  gtk_widget_set_name(help_text, "help_text");
  gtk_text_set_editable(GTK_TEXT(help_text), FALSE);

  gtk_container_add(GTK_CONTAINER(scrw), help_text);
  gtk_widget_show(help_text);

  but=gtk_button_new_with_label("Done");
  gtk_widget_set_name(but, "help done");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_win)->action_area), but,
		     FALSE, FALSE, 2);
  GTK_WIDGET_SET_FLAGS(but, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(but);
  gtk_widget_show(but);
  gtk_signal_connect(GTK_OBJECT(but), "clicked", 
		     GTK_SIGNAL_FUNC(close_window), 
		     help_win);
  
  find_fs();
  
  gtk_widget_show(win);
  (void) gtk_timeout_add(FULL_UPDATE_SEC*1000,
			 (GtkFunction) update_all_entries, fs_list);

  gtk_main();

  return 0;
}

static int find_selected_row(void)
{
  GList *sel=GTK_CLIST(fs_list)->selection;

  if(sel)
    return GPOINTER_TO_INT(sel->data);

  return -1;
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

/* return TRUE if mnttab has changed recently */
static int reread_mnttab(void)
{
  struct stat sb;

  if(stat(MNTTAB, &sb)<0)
    return FALSE;
  if(sb.st_mtime>last_read_mnttab) {
    return TRUE;
  }
  return FALSE;
}

static int update_fs_values(gchar *mntpt)
{
  int ok=FALSE;

  if(reread_mnttab()) {
    find_fs(); /* List is updated */
  }
  if(mntpt) {
    struct statvfs buf;
    
    ok=(statvfs(mntpt, &buf)==0);
    if(ok) {
      unsigned long long total, used, avail;
      gfloat fused;
      int row;
      
      /*printf("%lu %d %d %d\n", buf.f_frsize, buf.f_blocks, buf.f_bfree,
	     buf.f_bavail);*/
      
      total=buf.f_frsize*(unsigned long long) buf.f_blocks;
      used=buf.f_frsize*(unsigned long long) (buf.f_blocks-buf.f_bfree);
      avail=buf.f_frsize*(unsigned long long) buf.f_bavail;
      /*printf("%llu %llu %llu\n", total, used, avail);*/

      if(buf.f_blocks>0) {
	fused=(100.f*(buf.f_blocks-buf.f_bavail))/((gfloat) buf.f_blocks);
	if(fused>100.f)
	  fused=100.f;
      } else {
	fused=0.f;
      }
      /*printf("%2.0f %%\n", fused);*/
      
      gtk_label_set_text(GTK_LABEL(fs_name), mntpt);
      gtk_label_set_text(GTK_LABEL(fs_total), fmt_size(total));
      gtk_label_set_text(GTK_LABEL(fs_used), fmt_size(used));
      gtk_label_set_text(GTK_LABEL(fs_free), fmt_size(avail));
      row=find_selected_row();
      if(row>=0)
	gtk_clist_set_text(GTK_CLIST(fs_list), row, 3, fmt_size(avail));

      gtk_progress_set_value(GTK_PROGRESS(fs_per), fused);

      status("Updated %s", mntpt);
    } else {
      report_errno("Checking file system");
    }
  }
  if(!ok) {
    gtk_label_set_text(GTK_LABEL(fs_name), "");
    gtk_label_set_text(GTK_LABEL(fs_total), "");
    gtk_label_set_text(GTK_LABEL(fs_used), "");
    gtk_label_set_text(GTK_LABEL(fs_free), "");
    
    gtk_progress_set_value(GTK_PROGRESS(fs_per), 0);
  }

  return ok;
}

static int update_all_entries(GtkWidget *clist)
{
  int i, n;

  if(reread_mnttab()) {
    find_fs(); /* List is updated */
    return TRUE;
  }

  n=GTK_CLIST(clist)->rows;
  for(i=0; i<n; i++) {
    struct statvfs buf;
    gchar *mntpt;
    int ok;
    
    gtk_clist_get_text(GTK_CLIST(clist), i, 2, &mntpt);
    
    ok=(statvfs(mntpt, &buf)==0);
    if(ok) {
      unsigned long long avail;
      
      avail=buf.f_frsize*(unsigned long long) buf.f_bavail;
      gtk_clist_set_text(GTK_CLIST(clist), i, 3, fmt_size(avail));
    } else {
      gtk_clist_set_text(GTK_CLIST(clist), i, 3, "?");
    }
  }
  
  return TRUE;
}

static void process_fs_sel(GtkWidget *clist, gint row, int data, int sec)
{
  static guint update_tag=0;
  
  if(update_tag) {
    gtk_timeout_remove(update_tag);
    update_tag=0;
  }
  
  if(data) {
    gchar *mntpt;
    
    gtk_clist_get_text(GTK_CLIST(clist), row, 2, &mntpt);
    update_fs_values(mntpt);
    update_tag=gtk_timeout_add(sec*1000, (GtkFunction) update_fs_values,
			       mntpt);
    
  } else {
    update_fs_values(NULL);
  }
}

static void fs_sel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data)
{
  process_fs_sel(clist, row, GPOINTER_TO_INT(data), update_sec);
}

static void redo_fs_sel(void)
{
  int row=find_selected_row();
  
  if(row>=0)
    process_fs_sel(fs_list, row, TRUE, update_sec);
  else
    process_fs_sel(fs_list, 0, FALSE, update_sec);
}

#if 0
static gint match_string(gconstpointer a, gconstpointer b)
{
  return strcmp(a, b);
}
#endif

static int excluded_fs(const char *fsname)
{
  if(g_list_find_custom(exclude_fs, (gpointer) fsname,
			(GCompareFunc) strcmp))
    return TRUE;
  return FALSE;
}

/* Iterate over mounted FS's and load them into fs_list */
static void find_fs(void)
{
  struct mnttab mnt;
  FILE *mtfile;
  gchar *row[4];
  struct flock lb;
  struct statvfs buf;

  static int recursed=0;

  if(recursed)
    return;
  recursed++;

  mtfile=fopen(MNTTAB, "r");
  if(!mtfile) {
    report_errno("Opening "MNTTAB);
    return;
  }
  
  lb.l_type = F_RDLCK;
  lb.l_whence = 0;
  lb.l_start = 0;
  lb.l_len = 0;

  fcntl(fileno(mtfile), F_SETLKW, &lb);
  
  gtk_clist_freeze(GTK_CLIST(fs_list));
  gtk_clist_clear(GTK_CLIST(fs_list));

  do {
    if(getmntent(mtfile, &mnt)!=0)
      break;

    if(excluded_fs(mnt.mnt_fstype))
      continue;
    if(excluded_fs(mnt.mnt_mountp))
      continue;

    row[0]=mnt.mnt_special;
    row[1]=mnt.mnt_fstype;
    row[2]=mnt.mnt_mountp;

    if(statvfs(row[2], &buf)==0) {
      unsigned long long avail;
      
      avail=buf.f_frsize*(unsigned long long) buf.f_bavail;
      row[3]=(gchar *) fmt_size(avail);
    } else {
      row[3]="?";
    }

    gtk_clist_append(GTK_CLIST(fs_list), row);
  } while(!feof(mtfile));
  
  gtk_clist_thaw(GTK_CLIST(fs_list));
  fclose(mtfile);

  process_fs_sel(fs_list, 0, FALSE, update_sec);
  time(&last_read_mnttab);
  recursed--;
}

static void help_menu(gpointer dat, guint action, GtkWidget *wid)
{
  switch(action) {
  case 1:
    gtk_tips_query_start_query(GTK_TIPS_QUERY(tipq));
    break;
  }
}

static void change_update(gpointer dat, guint action, GtkWidget *wid)
{
  update_sec+=action;
  if(update_sec<1)
    update_sec=1;

  redo_fs_sel();
  status("Update every %d seconds", update_sec);
}

static void status(const char *fmt, ...)
{
  static guint context=0;
  static guint id=0;

  char str[256];
  va_list list;

  if(!context)
    context=gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar), "Status");
  
  if(id) {
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), context);
    id=0;
  }

  va_start(list, fmt);
  vsprintf(str, fmt, list);
  va_end(list);
  
  id=gtk_statusbar_push(GTK_STATUSBAR(status_bar), context, str);
}

static void report_errno(const char *what)
{
  const char *em=strerror(errno);

  status("%s: %s", what, em);
  errno=0;
}

static void read_choices(void)
{
  guchar *fname;
  
  choices_init();

  fname=choices_find_path_load("FS", "Free");
  if(fname) {
    FILE *in=fopen(fname, "r");
    char buf[256], *line;
    char *end;

    if(!in)
      return;

    do {
      line=fgets(buf, sizeof(buf), in);
      if(line) {
	end=strpbrk(line, " \t\n#");
	if(end)
	  *end=0;
	if(*line)
	  exclude_fs=g_list_append(exclude_fs, g_strdup(line));
      }
    } while(line);

    fclose(in);
  } else {
    FILE *out;
    GList *l;
    
    fname=choices_find_path_save("FS", "Free", TRUE);
    if(!fname)
      return;
    out=fopen(fname, "w");
    if(!out)
      return;

    exclude_fs=g_list_append(exclude_fs, "autofs");
    exclude_fs=g_list_append(exclude_fs, "lofs");
    exclude_fs=g_list_append(exclude_fs, "proc");
    exclude_fs=g_list_append(exclude_fs, "fd");

    fprintf(out, "# File systems to exclude from list\n");
    fprintf(out, "# (by FS type or mount point).\n");
    for(l=exclude_fs; l; l=g_list_next(l))
      fprintf(out, "%s\n", (char *) l->data);
    fclose(out);
  }
}
