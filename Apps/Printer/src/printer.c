/*
 * %W% %E%
 *
 * Printer - a printer manager for ROX
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <gtk/gtk.h>
#include "infowin.h"

#include "config.h"
#include "choices.h"

#include "printer.h"

/* The icon */
#include "../AppIcon.xpm"

/* Printer data */
GList *printers=NULL;
Printer *current_printer=NULL;
Printer *default_printer=NULL;

/* GTK+ objects */
static GtkWidget *win;
static GtkWidget *tipq;

static GtkWidget *printer_list;
static GtkWidget *job_list;

static guint printer_list_pipe_tag;
static guint default_printer_pipe_tag;

static void file_menu(gpointer dat, guint action, GtkWidget *);
static void help_menu(gpointer dat, guint action, GtkWidget *);
static void make_drop_target(GtkWidget *widget);
static void dnd_init(void);
static void show_help_popup(GtkWidget *tips_query, GtkWidget *widget,
			    const gchar *tip_text, const gchar *tip_private,
			    GdkEventButton *event, gpointer func_data);
static void job_sel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data);
static void printer_sel(GtkWidget *widget, gpointer data);
static void list_printers(void);

static void print_file(const char *fname);
static void print_file_as(const char *fname, const char *mime_type);

static void set_current_printer(Printer *);

enum {
	TARGET_RAW,
	TARGET_URI_LIST,
	TARGET_XDS,
	TARGET_STRING,
};

#define MAXURILEN 4096

static GtkItemFactoryEntry menu_items[] = {
  {"/_File", NULL, NULL, 0, "<Branch>"},
  {"/File/Check _Printer list","<control>P",  list_printers, 0, NULL },
  {"/File/sep",     NULL,         NULL, 0, "<Separator>" },
  {"/File/E_xit","<control>X",  gtk_main_quit, 0, NULL },
  /*
  {"/_View", NULL, NULL, 0, "<Branch>"},
  {"/View/Change _log", "<control>L", show_change_log, 0, NULL},
  {"/_Edit", NULL, NULL, 0, "<Branch>"},
  {"/Edit/Edit co_nfiguration", "<control>N", show_var_win, 0, NULL},
  */
  {"/_Help", NULL, NULL, 0, "<LastBranch>"},
  {"/Help/_About", "<control>A", help_menu, 0, NULL},
  
  {"/Help/sep",     NULL,         NULL, 0, "<Separator>" },
  {"/Help/Help on item", NULL, help_menu, 1, NULL},
  
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

static void append_option_menu_item(GtkWidget *base, GtkWidget *optmen,
				    const char *label, int index,
				    GtkSignalFunc func,
				    int *mw, int *mh)
{
  GtkWidget *optitem;
  GtkRequisition req;
  
  optitem=gtk_menu_item_new_with_label(label);
  gtk_object_set_data(GTK_OBJECT(optitem), "index", GINT_TO_POINTER(index));
  gtk_widget_show(optitem);
  gtk_widget_size_request(optitem, &req);
  if(*mw<req.width)
    *mw=req.width;
  if(*mh<req.height)
    *mh=req.height;
  gtk_object_set_data(GTK_OBJECT(base), "signal-func", (gpointer) func);
  if(func)
    gtk_signal_connect(GTK_OBJECT(optitem), "activate",
		       GTK_SIGNAL_FUNC(func), base);
  gtk_menu_append(GTK_MENU(optmen), optitem);
}

static void usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s [X options]\n", argv0);
  fprintf(stderr, "   or: %s filename...\n", argv0);
  fprintf(stderr, "     : %s -m mime-type filename\n", argv0);

  exit(1);
}

int main(int argc, char *argv[])
{
  GtkWidget *menu;
  GtkWidget *status_bar;
  GtkWidget *but;
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *pixmapwid;
  GtkWidget *scrw;
  GtkWidget *optmenu;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GList *list;
  GtkTooltips *ttips;
  int mx, my;

  /* Init X */
  gtk_init(&argc, &argv);

  /* Create our main window */
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "printer window");

  gtk_window_set_title(GTK_WINDOW(win), "FEMAP Main Control");
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");
  gtk_widget_realize(win);
  /* Set the icon */
  style = gtk_widget_get_style( win );
  pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(win)->window,  &mask,
                                           &style->bg[GTK_STATE_NORMAL],
                                           (gchar **)AppIcon_xpm );
  gdk_window_set_icon(GTK_WIDGET(win)->window, NULL, pixmap, mask);

  dnd_init();
  make_drop_target(win);

  ttips=gtk_tooltips_new();

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);
  
  get_main_menu(win, &menu);
  gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 2);
  gtk_widget_show(menu);

  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label=gtk_label_new("Printers");
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  printer_list=gtk_option_menu_new();
  gtk_widget_show(printer_list);
  gtk_box_pack_start(GTK_BOX(hbox), printer_list, FALSE, FALSE, 2);
  gtk_tooltips_set_tip(ttips, printer_list,
		       "List of defined printers to select from",
		       NULL);
  
  optmenu=gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(printer_list), optmenu);

  /*
  mw=0;
  mh=0;
  append_option_menu_item(cprog, optmenu, "Femap", 0,
			  GTK_SIGNAL_FUNC(sel_prog), &mw, &mh);
			  */

  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

  label=gtk_label_new("Jobs");
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrw),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, 24*12, 6*12);
  gtk_box_pack_start(GTK_BOX(hbox), scrw, TRUE, TRUE, 2);
  
  job_list=gtk_clist_new(NUM_COLUMN);
  gtk_clist_set_selection_mode(GTK_CLIST(job_list), GTK_SELECTION_SINGLE);
  gtk_signal_connect(GTK_OBJECT(job_list), "select_row",
		       GTK_SIGNAL_FUNC(job_sel),
		       GINT_TO_POINTER(TRUE));
  gtk_signal_connect(GTK_OBJECT(job_list), "unselect_row",
		       GTK_SIGNAL_FUNC(job_sel),
		       GINT_TO_POINTER(FALSE));
  gtk_widget_show(job_list);
  gtk_container_add(GTK_CONTAINER(scrw), job_list);
  gtk_tooltips_set_tip(ttips, job_list,
		       "This is the list of jobs for this printer",
		       NULL);
  
  tipq=gtk_tips_query_new();
  gtk_tips_query_set_caller(GTK_TIPS_QUERY(tipq), menu);
  gtk_box_pack_start(GTK_BOX(vbox), tipq, FALSE, FALSE, 2);
  gtk_widget_show(tipq);
  gtk_signal_connect(GTK_OBJECT (tipq), "widget_selected",
                        GTK_SIGNAL_FUNC(show_help_popup), NULL);
  
  status_bar=gtk_statusbar_new();
  gtk_box_pack_end(GTK_BOX(vbox), status_bar, FALSE, FALSE, 2);
  gtk_widget_show(status_bar);
  
  gtk_widget_show(win);

  list_printers();

  gtk_main();

  return 0;
}

static void show_help_popup(GtkWidget *tips_query, GtkWidget *widget,
			    const gchar *tip_text, const gchar *tip_private,
			    GdkEventButton *event, gpointer func_data)
{
}

static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    infowin=info_win_new(PROJECT, PURPOSE, VERSION, AUTHOR, WEBSITE);
  }

  gtk_widget_show(infowin);
}

static void help_menu(gpointer dat, guint action, GtkWidget *wid)
{
  switch(action) {
  case 0:
    show_info_win();
    break;
  case 1:
    gtk_tips_query_start_query(GTK_TIPS_QUERY(tipq));
    break;
  }
}

static void set_current_printer(Printer *p)
{
  int changed=(current_printer!=p);
  
  current_printer=p;

  if(!current_printer) {
    gtk_clist_clear(GTK_CLIST(job_list));
    return;
  }

  if(changed) {
  }
}

static void printer_sel(GtkWidget *widget, gpointer data)
{
  GtkWidget *opt=GTK_WIDGET(data);
}

static void job_sel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data)
{
}

static guint shell_output(const char *cmd, GdkInputFunction func,
			  gpointer data)
{
  guint tag;
  int i;
  int pid;
  int out[2];
  
  extern int getdtablesize(void);
  
  pipe(out);

  pid=fork();
  if(!pid) {
    dup2(out[1], 1);
    for(i=getdtablesize(); i>2; i--) /* Close all but standard files */
      (void) close(i);

    execl("/bin/sh", "sh", "-c", "lpstat -p", NULL);
    _exit(12);
    
  } else if(pid<0) {
    return 0;
    
  } else {
    close(out[1]);

    tag=gdk_input_add(out[0], GDK_INPUT_READ, func, data);
  }
  
  return tag;
}

static void end_default_printer(gint fd)
{
  int stat;
  
  gdk_input_remove(default_printer_pipe_tag);
  default_printer_pipe_tag=0;
  close(fd);
  waitpid((pid_t) -1, &stat, WNOHANG);
}

static void read_default_printer(gpointer data, gint fd,
			      GdkInputCondition condition)
{
  int nready, nr, actual;
  char buf[1024];
  char *ptr;
  
  if(!(condition&GDK_INPUT_READ) || ioctl(fd, FIONREAD, &nready)<0)
    return;

  if(nready<=0) {
    end_default_printer(fd);
    return;
  }

  nr=nready>sizeof(buf)? sizeof(buf): nready;
  actual=read(fd, buf, nr);

  if(actual<1) {
    end_default_printer(fd);
    return;
  }
  buf[actual]=0;

  ptr=strchr(buf, '\n');
  if(ptr)
    *ptr=0;
  ptr=strchr(buf, ':');
  if(ptr) {
    ptr+=2;

    default_printer=printer_find_by_name(ptr);
    if(!current_printer) {
      set_current_printer(default_printer);
    }
  }
}

static void get_default_printer(void)
{
  default_printer_pipe_tag=shell_output("lpstat -d", read_default_printer,
					NULL);
}

static void end_printer_list(gint fd)
{
  int stat;
  
  gdk_input_remove(printer_list_pipe_tag);
  printer_list_pipe_tag=0;
  close(fd);

  waitpid((pid_t) -1, &stat, WNOHANG);

  get_default_printer();
}

static void read_printer_list_line(GtkWidget *optitem, char *line)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkRequisition req;
  Printer *p;
  int mw, mh, aw, ah;
  gchar **sects, **words;
  gchar *name;
  gboolean en=FALSE, avail=TRUE;

  if(strncmp(line, "printer ", 8)!=0)
    return;
  
  sects=g_strsplit(line, ".", 3);
  if(!sects[0]) {
    g_strfreev(sects);
    return;
  }
  words=g_strsplit(sects[0], " ", 2);
  name=g_strdup(words[1]);
  g_strfreev(words);

  words=g_strsplit(sects[1], " ", 2);
  if(strcmp(words[0], "enabled")==0)
    en=TRUE;
  g_strfreev(words);
  
  if(sects[2] && strcmp(sects[2], "available")==0)
    avail=0;
    
  printf("append %s\n", name);
  p=printer_new(name, en, avail);
  g_free(name);
  printers=g_list_append(printers, p);

  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(optitem));

  item=gtk_menu_item_new_with_label(p->name);
  gtk_widget_show(item);
  gtk_widget_size_request(item, &req);
  mw=req.width;
  mh=req.height;
  gtk_widget_size_request(optitem, &req);
  aw=req.width;
  ah=req.height;
  aw=(mw+50>aw)? mw+50: -1;
  ah=(mh+4>ah)? mh+4: -1;
  gtk_widget_set_usize(optitem, aw, ah);
  gtk_signal_connect(GTK_OBJECT(item), "activate",
		       GTK_SIGNAL_FUNC(printer_sel), optitem);
  gtk_menu_append(GTK_MENU(menu), item);

  g_strfreev(sects);
}

static void read_printer_list(gpointer data, gint fd,
			      GdkInputCondition condition)
{
  GtkWidget *optitem=GTK_WIDGET(data);
  int nready, nr, actual;
  char buf[1024];
  gchar **lines;
  int i;
  
  if(!(condition&GDK_INPUT_READ) || ioctl(fd, FIONREAD, &nready)<0)
    return;

  if(nready<=0) {
    end_printer_list(fd);
    return;
  }

  nr=nready>sizeof(buf)? sizeof(buf): nready;
  actual=read(fd, buf, nr);

  if(actual<1) {
    end_printer_list(fd);
    return;
  }
  buf[actual]=0;

  lines=g_strsplit(buf, "\n", 1024);
  for(i=0; lines[i]; i++)
    read_printer_list_line(optitem, lines[i]);
  g_strfreev(lines);
}

static void list_printers(void)
{
  GtkWidget *nmenu;
  GList *p;

  current_printer=NULL;
  default_printer=NULL;
  for(p=printers; p; p=g_list_next(p))
    printer_free((Printer *) p->data);

  g_list_free(printers);
  printers=NULL;

  gtk_option_menu_remove_menu(GTK_OPTION_MENU(printer_list));
  nmenu=gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(printer_list), nmenu);
  
  printer_list_pipe_tag=shell_output("lpstat -p", read_printer_list, printer_list);

}

/* In print mode */
static void print_file(const char *fname)
{
  gchar *cmd;

  if(!current_printer)
    current_printer=default_printer;

  if(current_printer)
    cmd=g_strdup_printf("lp -d %s %s", current_printer->name, fname);
  else
    cmd=g_strdup_printf("lp %s", fname);

  execl("/bin/sh", "sh", "-c", cmd, NULL);
  _exit(11);
}

static gchar *find_filter(const char *mime_type)
{
  return NULL;
}

static void print_file_as(const char *fname, const char *mime_type)
{
  gchar *lpcmd;
  gchar *filter;
  gchar *cmd;
  int pid;

  if(!current_printer)
    current_printer=default_printer;

  if(current_printer)
    lpcmd=g_strdup_printf("lp -d %s", current_printer->name);
  else
    lpcmd="lp";

  filter=find_filter(mime_type);
  if(!filter)
    print_file(fname);

  cmd=g_strdup_printf("cat %s | %s | %s", fname, filter, lpcmd);
  execl("/bin/sh", "sh", "-c", cmd, NULL);
  _exit(12);
}

/* Printer objects */
Printer *printer_new(const char *name, gboolean en, gboolean avail)
{
  Printer *p=g_new(Printer, 1);

  p->name=g_strdup(name);
  p->enabled=en;
  p->available=avail;
  p->jobs=NULL;

  return p;
}

void printer_free(Printer *p)
{
  GList *jobs;
  
  g_free(p->name);

  for(jobs=p->jobs; jobs; jobs=g_list_next(jobs))
    job_free((Job *) jobs->data);
  g_list_free(p->jobs);
  
  g_free(p);
}

Printer *printer_find_by_name(const char *name)
{
  GList *rover;

  for(rover=printers; rover; rover=g_list_next(rover)) {
    Printer *p=(Printer *) rover->data;
    if(strcmp(name, p->name)==0)
      return p;
  }

  return NULL;
}

Job *job_new(Printer *print, const char *id, int size, const char *username,
	     const char *start)
{
  Job *job=g_new(Job, 1);

  job->printer=print;
  job->id=g_strdup(id);
  job->size=size;
  job->user=g_strdup(username);
  job->started=g_strdup(start);

  job->ours=FALSE;

  return job;
}

void job_free(Job *job)
{
  g_free(job->id);
  g_free(job->user);
  g_free(job->started);
}


/* Drag and drop */
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

    printf("new target is %s\n", path);
    
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

