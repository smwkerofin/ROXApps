/*
 * Tail - GTK version of tail -f
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>
/*#include "infowin.h"*/

#include "config.h"
/*#include "choices.h"*/

static GtkWidget *win;
static GtkWidget *text;

static int fd=-1;
static off_t pos;
static off_t size;
static gchar *fname=NULL;
static guint update_tag=0;

static gint check_file(gpointer unused);
static void set_file(const char *);

static void dnd_init(void);
static void make_drop_target(GtkWidget *widget);

static GtkItemFactoryEntry menu_items[] = {
  {"/_File", NULL, NULL, 0, "<Branch>"},
  {"/File/E_xit","<control>X",  gtk_main_quit, 0, NULL },
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

int main(int argc, char *argv[])
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *scr;
  GtkWidget *menu;

  gtk_init(&argc, &argv);
  dnd_init();

  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");
  gtk_window_set_title(GTK_WINDOW(win), "Tail");
  make_drop_target(win);
  
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
  */

  scr=gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_show(scr);
  gtk_widget_set_usize(scr, 640, 480);
  gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 2);
  
  text=gtk_text_new(NULL, NULL);
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scr), text);
  gtk_widget_show(text);

  gtk_widget_show(win);

  if(argv[1])
    set_file(argv[1]);
  update_tag=gtk_timeout_add(500, (GtkFunction) check_file, NULL);

  gtk_main();

  return 0;
}

static void append_text_from_file(int fd)
{
  char buf[BUFSIZ];
  size_t m=sizeof(buf)-1;
  ssize_t nr;
  gint pos;

  do {
    nr=read(fd, buf, m);
    if(nr<1)
      break;
    buf[nr]=0;

    gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buf, nr);
  } while(TRUE);
}

static gint check_file(gpointer unused)
{
  struct stat statb;
  
  if(!fd)
    return TRUE;

  if(fstat(fd, &statb)<0)
    return TRUE;

  if(statb.st_size==size)
    return TRUE;

  if(statb.st_size>size) {
    gtk_text_freeze(GTK_TEXT(text));
    append_text_from_file(fd);
    gtk_text_thaw(GTK_TEXT(text));
    
    pos=lseek(fd, (off_t) 0, SEEK_END);
    size=statb.st_size;
    
  } else {
    set_file(fname);
  }

  return TRUE;
}

static void set_file(const char *name)
{
  struct stat statb;
  gchar *title, *tmp;
  int nfd;
  int npos;

  nfd=open(name, O_RDONLY);
  if(nfd<0)
    return;

  if(fstat(nfd, &statb)<0)
    return;
  
  gtk_text_freeze(GTK_TEXT(text));
  gtk_editable_delete_text(GTK_EDITABLE(text), 0, -1);

  append_text_from_file(nfd);
  npos=lseek(nfd, (off_t) 0, SEEK_END);

  gtk_text_thaw(GTK_TEXT(text));

  title=g_strconcat("Tail: ", name, NULL);
  gtk_window_set_title(GTK_WINDOW(win), title);
  g_free(title);

  if(fd)
    close(fd);
  fd=nfd;
  pos=npos;
  size=statb.st_size;

  tmp=g_strdup(name);
  if(fname)
    g_free(fname);
  fname=tmp;
}

/*
 * Drag and drop code
 */
enum {
	TARGET_RAW,
	TARGET_URI_LIST,
	TARGET_XDS,
	TARGET_STRING,
};

#define MAXURILEN 4096

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
    /*{"XdndDirectSave0", 0, TARGET_XDS},*/
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

    set_file(path);
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
    gtk_drag_finish(context, FALSE, FALSE, time);
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

/*
 * $Log$
 */
