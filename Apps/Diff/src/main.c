/*
 * Demo - a demonstration of coding a ROX application in C
 *
 * To adapt this as your own program, first replace all occurances of "Demo" or
 * "DEMO" with your app name.
 *
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id: main.c,v 1.4 2002/01/30 10:17:41 stephen Exp $
 */
#include "config.h"

#define DEBUG              1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#include <fcntl.h>

/* These next two are for internationalization */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>
#include "infowin.h"

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
#include "rox_debug.h"
#include "infowin.h"
#include "rox_dnd.h"
#include "error.h"

#define WIN_WIDTH  320
#define WIN_HEIGHT 240

typedef struct diff_window {
  GtkWidget *win;
  GtkWidget *file[2];
  GtkWidget *diffs;
  gchar *fname[2];
  int tag;
  int pid;
} DiffWindow;

static DiffWindow window;

static GdkColormap *cmap;
static GdkColor col_add={0, 0, 0x8000, 0};
static GdkColor col_del={0, 0xffff, 0, 0};
static GdkColor col_chn={0, 0, 0, 0xffff};
static GdkColor col_ctl={0, 0xffff, 0, 0xffff};

typedef struct options {
  gchar *font_name;
} Options;

static Options options={
  "fixed"
};

/* Declare functions in advance */
static void make_window(DiffWindow *win);
static gint button_press(GtkWidget *window, GdkEventButton *bev,
			 gpointer win); /* button press on window */
static void show_info_win(void);        /* Show information box */
static void read_config(void);          /* Read configuration */
static void write_config(void);         /* Write configuration */
#if USE_XML
static gboolean read_config_xml(void);
static void write_config_xml(void);
#endif
static gboolean load_from_uri(GtkWidget *widget, GSList *uris, gpointer data,
			      gpointer udata);
static gboolean load_from_xds(GtkWidget *widget, const char *path,
			      gpointer data, gpointer udata);
static void show_diffs(DiffWindow *win);

static void usage(const char *argv0)
{
  printf("Usage: %s [X-options] [gtk-options] [-vh]\n", argv0);
  printf("where:\n\n");
  printf("  X-options\tstandard Xlib options\n");
  printf("  gtk-options\tstandard GTK+ options\n");
  printf("  -h\tprint this help message\n");
  printf("  -v\tdisplay version information\n");
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

/* Main.  Here we set up the gui and enter the main loop */
int main(int argc, char *argv[])
{
  gchar *app_dir;
#ifdef HAVE_BINDTEXTDOMAIN
  gchar *localedir;
#endif
  int c, do_exit, nerr;

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
  
  /* Check for this argument by itself */
  if(argv[1] && strcmp(argv[1], "-v")==0 && !argv[2]) {
    do_version();
    exit(0);
  }
  
  /* Initialise X/GDK/GTK */
  gtk_init(&argc, &argv);
  gdk_rgb_init();
  gtk_widget_push_visual(gdk_rgb_get_visual());
  cmap=gdk_rgb_get_cmap();
  gtk_widget_push_colormap(cmap);
  
  /* Process remaining arguments */
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
  rox_debug_init(PROJECT);
  choices_init();
  options.font_name=g_strdup(options.font_name);
  read_config();

  rox_dnd_init();

  /* Set up colours */
  gdk_color_alloc(cmap, &col_add);
  gdk_color_alloc(cmap, &col_del);
  gdk_color_alloc(cmap, &col_chn);
  gdk_color_alloc(cmap, &col_ctl);

  /* Make a window */
  make_window(&window);

  /* Show our new window */
  gtk_widget_show(window.win);

  /* Main processing loop */
  gtk_main();

  return 0;
}

static void make_window(DiffWindow *window)
{
  GtkWidget *win=NULL;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *text;
  GtkWidget *scrw;
  
  /* Create window */
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(win, "diff");
  gtk_window_set_title(GTK_WINDOW(win), _("Diff"));
  gtk_signal_connect(GTK_OBJECT(win), "destroy", 
		     GTK_SIGNAL_FUNC(gtk_main_quit), 
		     "WM destroy");

  /* We want to pop up a menu on a button press */
  gtk_signal_connect(GTK_OBJECT(win), "button_press_event",
		     GTK_SIGNAL_FUNC(button_press), win);
  gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);

  window->win=win;

  /* A vbox contains widgets arranged vertically */
  vbox=gtk_vbox_new(FALSE, 4);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  gtk_widget_show(vbox);

  /* An hbox contains widgets arranged horizontally.  We put it inside the
     vbox */
  hbox=gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, WIN_WIDTH, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), scrw, TRUE, TRUE, 2);
  
  text=gtk_text_new(NULL, NULL);
  gtk_widget_set_name(text, "file 1");
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_text_set_word_wrap(GTK_TEXT(text), FALSE);
  gtk_text_set_line_wrap(GTK_TEXT(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);

  rox_dnd_register_full(text, 0, load_from_uri, load_from_xds, window);
  window->file[0]=text;
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, WIN_WIDTH, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(hbox), scrw, TRUE, TRUE, 2);
  
  text=gtk_text_new(NULL, NULL);
  gtk_widget_set_name(text, "file 2");
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_text_set_word_wrap(GTK_TEXT(text), FALSE);
  gtk_text_set_line_wrap(GTK_TEXT(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);

  rox_dnd_register_full(text, 0, load_from_uri, load_from_xds, window);
  window->file[1]=text;
  
  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, -1, WIN_HEIGHT);
  gtk_box_pack_start(GTK_BOX(vbox), scrw, TRUE, TRUE, 2);
  
  text=gtk_text_new(NULL, NULL);
  gtk_widget_set_name(text, "diffs");
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_text_set_word_wrap(GTK_TEXT(text), FALSE);
  gtk_text_set_line_wrap(GTK_TEXT(text), FALSE);
  gtk_container_add(GTK_CONTAINER(scrw), text);
  gtk_widget_show(text);

  window->diffs=text;
}

#if USE_XML
/*
 * Write the config to a file. We have no config to save, but you might...
 * This saves in XML mode
 */
static void write_config_xml(void)
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
    tree=xmlNewChild(doc->children, NULL, "font", options.font_name);
    /*
    tree=xmlNewChild(doc->children, NULL, "flags", NULL);
    sprintf(buf, "%u", (unsigned int) mode.flags);
    xmlSetProp(tree, "value", buf);
    */
  
    ok=(xmlSaveFormatFileEnc(fname, doc, NULL, 1)>=0);

    xmlFreeDoc(doc);
    g_free(fname);
  }
}

#endif

/* Write the config to a file */
static void write_config(void)
{
#if !USE_XML
  gchar *fname;

  fname=choices_find_path_save("options", PROJECT, TRUE);
  dprintf(2, "save to %s", fname? fname: "NULL");

  if(fname) {
    FILE *out;

    out=fopen(fname, "w");
    if(out) {
      time_t now;
      char buf[80];
      
      fprintf(out, _("# Config file for %s %s (%s)\n"), PROJECT, VERSION,
	      AUTHOR);
      fprintf(out, _("# Latest version at %s\n"), WEBSITE);
      time(&now);
      strftime(buf, 80, "%c", localtime(&now));
      fprintf(out, _("#\n# Written %s\n\n"), buf);

      fprintf(out, "font=%s\n", options.font_name);

      fclose(out);
    }

    g_free(fname);
  }
#else
  write_config_xml();
#endif
}

#if USE_XML
/* Read in the config.  Again, nothing defined for this demo  */
static gboolean read_config_xml(void)
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
      xmlChar *string;
      
      if(node->type!=XML_ELEMENT_NODE)
	continue;

      /* Process your elements here */
      if(strcmp(node->name, "font")==0) {
	string=xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if(!string)
	  continue;
	if(options.font_name)
	  g_free(options.font_name);
	options.font_name=g_strdup(string);
	free(string);

	/*
      } else if(strcmp(node->name, "flags")==0) {
 	string=xmlGetProp(node, "value");
	if(!string)
	  continue;
	nmode.flags=atoi(string);
	free(string);
      */

      }
    }
    

    xmlFreeDoc(doc);
    g_free(fname);
  }
}
#endif

/* Read in the config */
static void read_config(void)
{
  guchar *fname;

#if USE_XML
  if(read_config_xml())
    return;
#endif
  
  fname=choices_find_path_load("options", PROJECT);

  if(fname) {
    FILE *in;
    
    in=fopen(fname, "r");
    if(in) {
      char buf[1024], *line;
      char *end;
      gchar *words;

      do {
	line=fgets(buf, sizeof(buf), in);
	if(!line)
	  break;
	end=strchr(line, '\n');
	if(end)
	  *end=0;
	end=strchr(line, '#');  /* everything after # is a comment */
	if(end)
	  *end=0;

	words=g_strstrip(line);
	if(words[0]) {
	  gchar *var, *val;

	  end=strchr(words, '=');
	  if(end) {
	    /* var = val */
	    val=g_strstrip(end+1);
	    *end=0;
	    var=g_strstrip(words);

	    if(strcmp(var, "font")==0) {
	      if(options.font_name)
		g_free(options.font_name);
	      options.font_name=g_strdup(val);
	    }
	  }
	}
	
      } while(!feof(in));

      fclose(in);
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

static gboolean load_window_from(GtkWidget *widget, const char *fname,
				 DiffWindow *win)
{
  GtkText *text;
  gboolean ok=FALSE;
  FILE *in;
  char buf[BUFSIZ];
  int nb;
  GdkFont *font=NULL;
  
  g_return_val_if_fail(widget!=NULL, FALSE);
  g_return_val_if_fail(GTK_IS_TEXT(widget), FALSE);
  g_return_val_if_fail(fname!=NULL, FALSE);
  g_return_val_if_fail(win!=NULL, FALSE);

  dprintf(3, "Load '%s' into %p (data at %p)", fname, widget, win);

  in=fopen(fname, "r");
  if(!in) {
    rox_error("Failed to read %s: %s", fname, g_strerror(errno));
    return FALSE;
  }

  if(options.font_name)
    font=gdk_font_load(options.font_name);
  if(!font) 
    font=widget->style->font;
  gdk_font_ref(font);

  text=GTK_TEXT(widget);
  gtk_text_freeze(text);
  gtk_editable_delete_text(GTK_EDITABLE(text), 0, -1);
  do {
    nb=fread(buf, 1, sizeof(buf), in);
    if(nb>0) {
      gtk_text_insert(text, font, NULL, NULL, buf, nb);
    }
  } while(nb>0);
  if(nb==0)
    ok=TRUE;
  gtk_text_thaw(text);
  fclose(in);
  gdk_font_unref(font);

  return ok;
}

static gboolean load_from_uri(GtkWidget *widget, GSList *uris, gpointer data,
			      gpointer udata)
{
  DiffWindow *win=(DiffWindow *) udata;
  GSList *files=rox_dnd_filter_local(uris);
  gboolean ok=FALSE;

  if(!files)
    return FALSE;

  dprintf(3, "first uri=%s", (char *) uris->data);

  if(files->data) {
    const char *fname=(const char *) files->data;

    ok=load_window_from(widget, fname, win);
  }

  rox_dnd_local_free(files);

  if(ok)
    show_diffs(win);

  return ok;
}

static gboolean load_from_xds(GtkWidget *widget, const char *path,
			      gpointer data, gpointer udata)
{
  DiffWindow *win=(DiffWindow *) udata;

  dprintf(3, "load_from_xds: %s", path);

  if(load_window_from(widget, path, win)) {
    show_diffs(win);
    return TRUE;
  }
  return FALSE;
}

static void clean_up(DiffWindow *win)
{
  int f;
  int stat;

  for(f=0; f<2; f++) {
    if(win->fname[f]) {
      unlink(win->fname[f]);
      g_free(win->fname[f]);
    }
  }

  gtk_input_remove(win->tag);
  waitpid((pid_t) win->pid, &stat, WNOHANG);
  gtk_text_thaw(GTK_TEXT(win->diffs));
}

static void write_window_to(GtkWidget *widget, const char *fname,
			    DiffWindow *win)
{
  GtkText *text;
  FILE *out;
  int nb;
  guint len;
  gint start;
  int begin, size;

  g_return_if_fail(widget!=NULL);
  g_return_if_fail(GTK_IS_TEXT(widget));
  g_return_if_fail(fname!=NULL);
  g_return_if_fail(win!=NULL);

  text=GTK_TEXT(widget);
  len=gtk_text_get_length(text);

  out=fopen(fname, "w");
  if(!out) {
    rox_error("Failed to write to %s: %s", fname, g_strerror(errno));
    return;
  }

  for(start=0; start<len; start+=BUFSIZ) {
    gint end;
    gchar *txt;

    end=start+BUFSIZ;
    if(end>len)
      end=len;

    txt=gtk_editable_get_chars(GTK_EDITABLE(text), start, end);

    do {
      begin=0;
      size=end-start;
      
      nb=fwrite(txt, 1, size-begin, out);
      if(nb<0) {
	rox_error("Failed to write to %s: %s", fname, g_strerror(errno));
	fclose(out);
	g_free(txt);
	return;
      }
      begin+=nb;
    } while(begin<size);

    g_free(txt);
  }
  fclose(out);
}

static void process_diff_line(GtkWidget *text, char *line)
{
  GdkColor *col=NULL;
  GdkFont *font=NULL;

  if(line[0]=='+')
    col=&col_add;
  else if(strncmp(line, "- ", 2)==0)
    col=&col_del;
  else if(line[0]=='!')
    col=&col_chn;
  else if(line[0]=='*' || strncmp(line, "--", 2)==0)
    col=&col_ctl;
  
  if(options.font_name)
    font=gdk_font_load(options.font_name);
  if(!font) 
    font=text->style->font;
  gdk_font_ref(font);

  gtk_text_insert(GTK_TEXT(text), font, col, NULL, line, strlen(line));
  gtk_text_insert(GTK_TEXT(text), font, col, NULL, "\n", 1);
  
  gdk_font_unref(font);
}

static void diff_reader(gpointer data, gint fd, GdkInputCondition condition)
{
  DiffWindow *win=(DiffWindow *) data;
  static char *buf=NULL;
  static int bufsize=0;
  int nbready, nbread;
  char *nl;

  if(ioctl(fd, FIONREAD, &nbready)<0)
    return;

  if(nbready<1) {
    close(fd);

    clean_up(win);
    return;
  }
  if(nbready>bufsize-1) {
    bufsize=nbready+1;
    buf=realloc(buf, bufsize);
  }

  nbread=read(fd, buf, nbready);
  if(nbread>1) {
    buf[nbread]=0;

    nl=buf-1;
    do {
      char *st=nl+1;
      nl=strchr(st, '\n');
      if(nl) {
	*nl=0;
	process_diff_line(win->diffs, st);
      }
    } while(nl);
  }
}

static void show_diffs(DiffWindow *win)
{
  int p[2];
  int pid;
  gchar *cmd;

  win->fname[0]=g_strdup(tmpnam(NULL));
  write_window_to(win->file[0], win->fname[0], win);

  win->fname[1]=g_strdup(tmpnam(NULL));
  write_window_to(win->file[1], win->fname[1], win);

  pipe(p);
  pid=fork();
  if(pid<0) {
    rox_error("Failed to fork: %s", g_strerror(errno));
    return;
  }

  if(!pid) {
    dup2(p[1], 1);

    cmd=g_strdup_printf("diff -c %s %s", win->fname[0], win->fname[1]);
   execlp("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(1);
  }
  win->pid=pid;
  close(p[1]);

  gtk_text_freeze(GTK_TEXT(win->diffs));
  gtk_editable_delete_text(GTK_EDITABLE(win->diffs), 0, -1);
  win->tag=gdk_input_add(p[0], GDK_INPUT_READ, diff_reader,
		    win);
}

/*
 * $Log: main.c,v $
 * Revision 1.4  2002/01/30 10:17:41  stephen
 * Added -h and -v options.
 *
 * Revision 1.3  2001/12/21 09:54:26  stephen
 * Added some debug lines (not ready for new release yet).
 *
 * Revision 1.2  2001/11/29 15:52:33  stephen
 * Test for <sys/filio.h>
 *
 * Revision 1.1.1.1  2001/11/23 13:02:16  stephen
 * Coloured diff
 *
 */
