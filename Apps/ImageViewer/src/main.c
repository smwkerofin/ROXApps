/*
 * ImageViewer - view an image.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id$
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>

/* These next two are for internationalization */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Libxml */
#include <libxml/tree.h>
#include <libxml/parser.h>

#define DEBUG              1   /* Allow debug output */
#include <rox.h>
#include <rox_dnd.h>

/* Declare functions in advance */
static void show_image_file(const char *fname, GtkWidget *canvas);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on window */
static void redraw(GtkWidget *widget, GdkEventExpose *event,
		   gpointer udata);/* Redraw window*/
static void show_info_win(void);        /* Show information box */
static void read_config(void);          /* Read configuration */
static void write_config(void);         /* Write configuration */
static gboolean drop_uri(GtkWidget *widget, GSList *uris,
			 gpointer data, gpointer udata);

static GdkPixbuf *image=NULL;

/* Print out the command line usage */
static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vh]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
}

/* Print our version number and other version related stuff */
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
  printf("  Using XML version %d\n", LIBXML_VERSION);

  /* Describe other compile time stuff here */
}

/* Main.  Here we set up the gui and enter the main loop */
int main(int argc, char *argv[])
{
  GtkWidget *win=NULL;
  GtkWidget *canvas;
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
 int c, nerr, do_exit;

  /* First things first, set the locale information for time, so that
     strftime will give us a sensible date format... */
#ifdef HAVE_SETLOCALE
  setlocale(LC_TIME, "");
  setlocale (LC_ALL, "");
#endif
  /* What is the directory where our resources are? (set by AppRun) */
  app_dir=g_getenv("APP_DIR");
#ifdef HAVE_BINDTEXTDOMAIN
  /* More (untested) i18n support */
  localedir=g_strconcat(app_dir, "/Messages", NULL);
  bindtextdomain(PROJECT, localedir);
  textdomain(PROJECT);
  g_free(localedir);
#endif
  
  rox_debug_init(PROJECT);
  dprintf(1, "Debug system inited");

  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
    
  /* Initialise X/GDK/GTK */
  gtk_init(&argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  gtk_widget_push_colormap(gdk_rgb_get_cmap());
  
  /* Process remaining arguments.  Test here for any arguments you
   are interested in */
  nerr=0;
  do_exit=FALSE;
  while((c=getopt(argc, argv, "vh"))!=EOF)
    switch(c) {
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

  /* Init choices and read them in */
  choices_init();
  read_config();

  /* Create window */
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "iamge viewer");
  gtk_window_set_title(GTK_WINDOW(win), _("ImageViewer"));
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");

  /* We want to pop up a menu on a button press */
  gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		     GTK_SIGNAL_FUNC(button_press), win);
  gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);

  /*
  canvas=gtk_drawing_area_new();
  gtk_signal_connect (GTK_OBJECT (canvas), "expose_event",
		      (GtkSignalFunc) redraw, canvas);
  gtk_widget_set_events (canvas, GDK_EXPOSURE_MASK
			 | GDK_BUTTON_PRESS_MASK);
  gtk_container_add(GTK_CONTAINER(win), canvas);
  gtk_widget_show (canvas);
  */

  if(optind<argc)
    show_image_file(argv[optind], win);
  
  /* Drag and drop */
  rox_dnd_init();
  rox_dnd_register_uris(win, 0, drop_uri, win);
  
  /* Show our new window */
  gtk_widget_show(win);

  /* Main processing loop */
  gtk_main();

  /* We are done, save out config incase it changed */
  write_config();

  return 0;
}


/*
 * Write the config to a file. We have no config to save, but you might...
 * This saves in XML mode
 */
static void write_config(void)
{
  gchar *fname;

  /* Use the choices system to get the name to save to */
  fname=choices_find_path_save("options.xml", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr tree;
    FILE *out;
    char buf[80];
    gboolean ok;

    doc = xmlNewDoc("1.0");
    doc->children=xmlNewDocNode(doc, NULL, PROJECT, NULL);
    xmlSetProp(doc->children, "version", VERSION);

    /* Insert data here, e.g. */
    /*
    tree=xmlNewChild(doc->children, NULL, "format", NULL);
    xmlSetProp(tree, "name", mode.format->name);
    tree=xmlNewChild(doc->children, NULL, "flags", NULL);
    sprintf(buf, "%u", (unsigned int) mode.flags);
    xmlSetProp(tree, "value", buf);
    */
  
#if LIBXML_VERSION > 20400
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);
#else
    out=fopen(fname, "w");
    if(out) {
      xmlDocDump(out, doc);
            
      fclose(out);
      ok=TRUE;
    } else {
      ok=FALSE;
    }
#endif

    g_free(fname);
  }
}

/* Read in the config.  Again, nothing defined for this demo  */
static void read_config(void)
{
  guchar *fname;

  /* Use the choices system to locate the file to read */
  fname=choices_find_path_load("options.xml", PROJECT);

  if(fname) {
    xmlDocPtr doc;
    xmlNodePtr node, root;
    const xmlChar *string;

    doc=xmlParseFile(fname);
    if(!doc) {
      g_free(fname);
      return;
    }

    root=xmlDocGetRootElement(doc);
    if(!root) {
      g_free(fname);
      xmlFreeDoc(doc);
      return;
    }

    if(strcmp(root->name, PROJECT)!=0) {
      g_free(fname);
      xmlFreeDoc(doc);
      return;
    }

    for(node=root->xmlChildrenNode; node; node=node->next) {
      const xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process your elements here, e.g.
      if(strcmp(node->name, "format")==0) {
	int i;
	
	string=xmlGetProp(node, "name");
	if(!string)
	  continue;

	for(i=0; formats[i].name; i++)
	  if(strcmp(string, formats[i].name)==0)
	    break;
	if(formats[i].name)
	  nmode.format=formats+i;
	free(string);
	
      } else if(strcmp(node->name, "flags")==0) {
 	string=xmlGetProp(node, "value");
	if(!string)
	  continue;
	nmode.flags=atoi(string);
	free(string);

      }
      */
    }
    

    g_free(fname);
  }
}

/*
 * Pop-up menu
 * Just two entries, one shows our information window, the other quits the
 * applet
 */
static GtkItemFactoryEntry menu_items[] = {
  { N_("/Info"),		NULL, show_info_win, 0, NULL },
  { N_("/Quit"), 	        NULL, gtk_main_quit, 0, NULL },
};

/* Save user-defined menu accelerators */
static void save_menus(void)
{
  char	*menurc;
	
  menurc = choices_find_path_save("menus", PROJECT, TRUE);
  if (menurc) {
    gtk_item_factory_dump_rc(menurc, NULL, TRUE);
    g_free(menurc);
  }
}

/* Create the pop-up menu */
static GtkWidget *menu_create_menu(GtkWidget *window)
{
  GtkWidget *menu;
  GtkItemFactory	*item_factory;
  GtkAccelGroup	*accel_group;
  gint 		n_items = sizeof(menu_items) / sizeof(*menu_items);
  gchar *menurc;

  accel_group = gtk_accel_group_new();

  item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<system>", 
				      accel_group);

  gtk_item_factory_create_items(item_factory, n_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

  /* Load any user-defined menu accelerators */
  menu = gtk_item_factory_get_widget(item_factory, "<system>");

  menurc=choices_find_path_load("menus", PROJECT);
  if(menurc) {
    gtk_item_factory_parse_rc(menurc);
    g_free(menurc);
  }

  /* Save updated accelerators when we exit */
  atexit(save_menus);

  return menu;
}

/* Button press on our window */
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win)
{
  static GtkWidget *menu=NULL;
  
  if(bev->type==GDK_BUTTON_PRESS && bev->button==3) {
    /* Pop up the menu */
    if(!menu) 
      menu=menu_create_menu(GTK_WIDGET(win));
    
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
				bev->button, bev->time);
    return TRUE;
  }

  return FALSE;
}

/* Make a destroy-frame into a close, allowing us to re-use the window */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

/* Show the info window */
static void show_info_win(void)
{
  static GtkWidget *infowin=NULL;
  
  if(!infowin) {
    /* Need to make it first.  The arguments are macros defined
     in config.h.in  */
    infowin=info_win_new(PROJECT, PURPOSE, VERSION, AUTHOR, WEBSITE);
    gtk_signal_connect(GTK_OBJECT(infowin), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     infowin);
  }

  gtk_widget_show(infowin);
}

static void show_image_file(const char *fname, GtkWidget *win)
{
  GdkPixbuf *im;
  GdkPixmap *pmap;
  GdkBitmap *mask;
  GtkWidget *wd;
  
  /* Load the image specified as the first argument */
  im=gdk_pixbuf_new_from_file(fname);
  if(!im) {
    rox_error("Failed to load %s", fname);
    return;
  }

  if(image)
    gdk_pixbuf_unref(image);
  image=im;

  /*gtk_drawing_area_size(GTK_DRAWING_AREA(canvas), gdk_pixbuf_get_width(im),
			gdk_pixbuf_get_height(im));*/
  /*gtk_widget_queue_draw(canvas);*/

  gdk_pixbuf_render_pixmap_and_mask(image, &pmap, &mask, 50);
  wd=gtk_pixmap_new(pmap, mask);
  if(GTK_BIN(win)->child)
    gtk_container_remove(GTK_CONTAINER(win), GTK_BIN(win)->child);
  gtk_container_add(GTK_CONTAINER(win), wd);
  gtk_widget_show(wd);
}

static gboolean drop_uri(GtkWidget *widget, GSList *uris,
			 gpointer data, gpointer udata)
{
  GtkWidget *canvas=GTK_WIDGET(udata);
  GSList *locals=rox_dnd_filter_local(uris);
  
  if(locals) {
    show_image_file((const char *) locals->data, canvas);
    rox_dnd_local_free(locals);
    return TRUE;
  }

  return FALSE;
}

/*
static void redraw(GtkWidget *widget, GdkEventExpose *event,
		   gpointer udata)
{
  static GdkGC *gc=NULL;
  GtkWidget *canvas=GTK_WIDGET(udata);
  
  if(!image)
    return;
  if(!gc) {
    gc=gdk_gc_new(canvas->window);
  }
  /* Copy map image to window * /
  gdk_pixbuf_render_to_drawable(image, canvas->window, gc, 0, 0, 0, 0,
				gdk_pixbuf_get_width(image),
				gdk_pixbuf_get_height(image),
				GDK_RGB_DITHER_NONE, 0, 0);
}
*/

/*
 * $Log$
 */
