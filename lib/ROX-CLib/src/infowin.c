/*
 * A GTK+ Widget to implement a RISC OS style info window
 *
 * $Id: infowin.c,v 1.8 2003/12/13 19:25:39 stephen Exp $
 */
#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <gtk/gtk.h>
#include "infowin.h"

#define DEBUG 1
#include <rox.h>
#include "rox_debug.h"
#include "rox_filer_action.h"

static void info_win_finalize (GObject *object);

static GtkDialogClass *parent_class=NULL;

static void info_win_class_init(InfoWinClass *iwc)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) iwc;

  parent_class = gtk_type_class (gtk_dialog_get_type ());

  /* Set up signals here... */

  object_class->finalize=info_win_finalize;
}

/* Make a destroy-frame into a close */
static int trap_client_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(widget);
  return TRUE;
}

static void dismiss(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(GTK_WIDGET(data));
}

static void goto_website(GtkWidget *widget, gpointer data)
{
  InfoWin *iw;
  GList *cmds;
  int pid;
  char cpath[1024];
  char *path, *dir, *file;
  FILE *out;
  char tfname[256];
  time_t now;

  g_return_if_fail(IS_INFO_WIN(data));

  iw=INFO_WIN(data);

  if(!iw->web_site)
    return;

  time(&now);
  sprintf(tfname, "/tmp/infowin.%d.%ld.uri", (int) geteuid(), (long) now);
  out=fopen(tfname, "w");
  if(out) {
    fprintf(out, "URL=%s\n", iw->web_site);
    fclose(out);
    dprintf(2, "access %s via %s", iw->web_site, tfname);
    rox_filer_run(tfname);
    if(!rox_filer_have_error())
      return;
  }

  pid=fork();

  if(pid<0) {
    gdk_beep();
    return;
  } else if(pid>0) {
    return;
  }

  /* This is the child process */
  path=getenv("PATH");
  if(!path)
    path="/usr/local/bin:/usr/bin";
  for(cmds=iw->browser_cmds; cmds; cmds=g_list_next(cmds)) {
    const char *cmd=(const char *) cmds->data;
    
    if(cmd[0]=='/') {
      dprintf(3, "%s %s", cmd, iw->web_site);
      execl(cmd, cmd, iw->web_site, NULL);
    } else {
      strcpy(cpath, path);
      for(dir=strtok(cpath, ":"); dir; dir=strtok(NULL, ":")) {
	file=g_strconcat(dir, "/", cmd, NULL);
	dprintf(3, "%s %s", file, iw->web_site);
	execl(file, cmd, iw->web_site, NULL);
	g_free(file);
      }
    }
  }

  _exit(1);
}

static GtkWidget *get_app_icon(void)
{
  GtkWidget *image;
  GdkPixbuf *pixbuf;
  
  pixbuf=rox_get_program_icon();

  if(!pixbuf) {
    const gchar *app_dir=g_getenv("APP_DIR");
    if(app_dir) {
      gchar *dir_icon;
    
      dir_icon=g_build_filename(app_dir, ".DirIcon", NULL);
      pixbuf=gdk_pixbuf_new_from_file(dir_icon, NULL);
      g_free(dir_icon);
    }
  }
  if(pixbuf) {
    image=gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    return image;
  }
  
  return NULL;
}
  
static void info_win_init(InfoWin *iw)
{
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *icon;
  
  g_signal_connect(G_OBJECT(iw), "delete_event", 
		     G_CALLBACK(trap_client_destroy), 
		     "WM destroy");
  gtk_window_set_position(GTK_WINDOW(iw), GTK_WIN_POS_MOUSE);
  
  iw->web_site=NULL;
  iw->browser_cmds=g_list_append(NULL, "netscape");

  hbox=gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(iw)->vbox), hbox, TRUE, TRUE, 2);
  gtk_widget_show(hbox);

  icon=get_app_icon();
  if(icon) {
    gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 2);
    gtk_widget_show(icon);
  }

  iw->table=gtk_table_new(INFO_WIN_NSLOT, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), iw->table, TRUE, TRUE, 2);
  gtk_widget_show(iw->table);

  label=gtk_label_new(_("Program"));
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_PROGRAM, INFO_WIN_PROGRAM+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_PROGRAM, INFO_WIN_PROGRAM+1);

  iw->slots[INFO_WIN_PROGRAM]=frame;

  label=gtk_label_new(_("Purpose"));
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_PURPOSE, INFO_WIN_PURPOSE+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_PURPOSE, INFO_WIN_PURPOSE+1);

  iw->slots[INFO_WIN_PURPOSE]=frame;

  label=gtk_label_new(_("Version"));
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_VERSION, INFO_WIN_VERSION+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_VERSION, INFO_WIN_VERSION+1);

  iw->slots[INFO_WIN_VERSION]=frame;

  label=gtk_label_new(_("Author"));
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_AUTHOR, INFO_WIN_AUTHOR+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_AUTHOR, INFO_WIN_AUTHOR+1);

  iw->slots[INFO_WIN_AUTHOR]=frame;

  label=gtk_label_new(_("Web site"));
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_WEBSITE, INFO_WIN_WEBSITE+1);

  button=gtk_button_new();
  gtk_widget_show(button);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), button, 1, 2,
			    INFO_WIN_WEBSITE, INFO_WIN_WEBSITE+1);
  g_signal_connect(G_OBJECT (button), "clicked",
                        G_CALLBACK(goto_website), iw);

  iw->slots[INFO_WIN_WEBSITE]=button;

  hbox=GTK_DIALOG(iw)->action_area;

  button=gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_widget_show(button);
  g_signal_connect(G_OBJECT (button), "clicked",
                        G_CALLBACK(dismiss), iw);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);
}

GType info_win_get_type(void)
{
  static GType iw_type = 0;

  if (!iw_type) {
      static const GTypeInfo iw_info = {
	sizeof (InfoWinClass),
	NULL,			/* base_init */
	NULL,			/* base_finalise */
	(GClassInitFunc) info_win_class_init,
	NULL,			/* class_finalise */
	NULL,			/* class_data */
	sizeof(InfoWinClass),
	0,			/* n_preallocs */
	(GtkObjectInitFunc) info_win_init,
      };

      iw_type = g_type_register_static(GTK_TYPE_DIALOG, "InfoWin", &iw_info,
				       0);
    }

  return iw_type;
}

GtkWidget* info_win_new(const gchar *program, const gchar *purpose,
				const gchar *version, const gchar *author,
				const gchar *website)
{
  GtkWidget *widget=GTK_WIDGET(gtk_type_new(info_win_get_type()));
  GtkWidget *label;
  InfoWin *iw=INFO_WIN(widget);

  label=gtk_label_new(program);
  gtk_widget_show(label);
  gtk_container_add(GTK_CONTAINER(iw->slots[INFO_WIN_PROGRAM]), label);

  label=gtk_label_new(purpose);
  gtk_widget_show(label);
  gtk_container_add(GTK_CONTAINER(iw->slots[INFO_WIN_PURPOSE]), label);

  label=gtk_label_new(version);
  gtk_widget_show(label);
  gtk_container_add(GTK_CONTAINER(iw->slots[INFO_WIN_VERSION]), label);

  label=gtk_label_new(author);
  gtk_widget_show(label);
  gtk_container_add(GTK_CONTAINER(iw->slots[INFO_WIN_AUTHOR]), label);

  iw->web_site=g_strdup(website);
  label=gtk_label_new(website);
  gtk_widget_show(label);
  gtk_container_add(GTK_CONTAINER(iw->slots[INFO_WIN_WEBSITE]), label);

  return widget;
}

void info_win_add_browser_command(InfoWin *iw, const gchar *cmd)
{
  g_return_if_fail(iw!=NULL);
  g_return_if_fail(IS_INFO_WIN(iw));
  g_return_if_fail(cmd!=NULL);

  iw->browser_cmds=g_list_prepend(iw->browser_cmds, (void *) cmd);
}

static void info_win_finalize (GObject *object)
{
  InfoWin *iw;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (IS_INFO_WIN (object));
  
  iw = INFO_WIN (object);

  g_free(iw->web_site);
  g_list_free(iw->browser_cmds);
  
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* AppInfo scanning */
#include <libxml/parser.h>

/* Based on ROX-Lib's i18n.py, could be moved to rox_i18n.c ? */
#define	COMPONENT_CODESET   (1 << 0)
#define	COMPONENT_TERRITORY (1 << 1)
#define	COMPONENT_MODIFIER  (1 << 2)
static GList *_expand_lang(const char *locale)
{
  GList *langs=NULL;
  gchar *modifier=NULL, *codeset=NULL, *territory=NULL, *language=NULL;
  gchar *tmp, *pos;
  unsigned mask=0;
  int i;

  dprintf(3, "expand %s", locale);

  tmp=g_strdup(locale);
  pos=strchr(tmp, '@');
  if(pos) {
    modifier=pos+1;
    *pos=0;
    mask|=COMPONENT_MODIFIER;
  }
  pos=strchr(tmp, '.');
  if(pos) {
    codeset=pos+1;
    *pos=0;
    mask|=COMPONENT_CODESET;
  }
  pos=strchr(tmp, '_');
  if(pos) {
    territory=pos+1;
    *pos=0;
    mask|=COMPONENT_TERRITORY;
  }
  language=tmp;

  for(i=0; i<mask+1; i++)
    if(!(i & ~mask)) {
      GString *val;

      val=g_string_new(language);
      if(i & COMPONENT_TERRITORY) {
	val=g_string_append(val, "_");
	val=g_string_append(val, territory);
      }
      if(i & COMPONENT_CODESET) {
	val=g_string_append(val, ".");
	val=g_string_append(val, codeset);
      }
      if(i & COMPONENT_MODIFIER) {
	val=g_string_append(val, "@");
	val=g_string_append(val, modifier);
      }
      
      langs=g_list_prepend(langs, val->str);
      dprintf(3, "lang=%s", val->str);
      g_string_free(val, FALSE);
      
    }

  g_free(tmp);

  return langs;
}

static GList *expand_languages(GList *languages)
{
  GList *nelangs=NULL;
  int i;
  GList *rover;
  gboolean we_made=FALSE;

  if(!languages) {
    static const char *envs[]={
      "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG",
      NULL
    };
    const gchar *env;

    for(i=0; envs[i]; i++) {
      env=g_getenv(envs[i]);
      if(env)
	languages=g_list_append(languages, g_strdup(env));
    }
    we_made=TRUE;
  }
  
  for(rover=languages; rover; rover=g_list_next(rover))
    if(strcmp((char *) rover->data, "C")==0)
      break;
  if(!rover)
    languages=g_list_append(languages, g_strdup("C"));

  for(rover=languages; rover; rover=g_list_next(rover)) {
    GList *langs=_expand_lang((char *) rover->data);
    GList *l;
    for(l=langs; l; l=g_list_next(l)) {
      nelangs=g_list_append(nelangs, l->data);
    }
    g_list_free(langs);
  }

  if(we_made) {
    for(rover=languages; rover; rover=g_list_next(rover))
      g_free(rover->data);
    g_list_free(languages);
  }

  return nelangs;
}

GtkWidget *info_win_new_from_appinfo(const char *program)
{
  gchar *purpose=NULL, *version=NULL, *author=NULL, *website=NULL;
  const gchar *appdir;
  gchar *path;
  GtkWidget *window;

  dprintf(3, "info_win_new_from_appinfo(%s)", program);

  appdir=g_getenv("APP_DIR");
  if(appdir) { 
    xmlDocPtr doc;
    
    path=g_build_filename(appdir, "AppInfo.xml", NULL);
    dprintf(3, "read from %s", path);
    doc=xmlParseFile(path);
    if(doc) {
      GList *langs=expand_languages(NULL);
      GList *l;
      xmlNodePtr node, sub;
      xmlChar *lang, *value;
      gboolean use_this;

      for(node=doc->children->children; node; node=node->next) {
	if(node->type!=XML_ELEMENT_NODE)
	  continue;

	dprintf(3, "node->name=%s", node->name);
	if(strcmp(node->name, "About")!=0)
	  continue;

	lang=xmlNodeGetLang(node);
	dprintf(2, "About node with lang %s", lang? lang: "NULL");

	if(lang) {
	  use_this=FALSE;
	  for(l=langs; l; l=g_list_next(l)) {
	    if(strcmp((char *) l->data, lang)==0) {
	      use_this=TRUE;
	      break;
	    }
	  }
	} else {
	  use_this=TRUE;
	}

	if(use_this) {
	  for(sub=node->children; sub; sub=sub->next) {
	    if(sub->type!=XML_ELEMENT_NODE)
	      continue;

	    if(strcmp(sub->name, "Purpose")==0 && (!purpose || lang)) {
	      value=xmlNodeGetContent(sub);
	      dprintf(3, "Purpose=%s", value);
	      if(purpose)
		g_free(purpose);
	      purpose=g_strdup(value);
	      free(value);
	    } else if(strcmp(sub->name, "Version")==0 && (!version || lang)) {
	      value=xmlNodeGetContent(sub);
	      if(version)
		g_free(version);
	      version=g_strdup(value);
	      free(value);
	    } else if(strcmp(sub->name, "Authors")==0 && (!author || lang)) {
	      value=xmlNodeGetContent(sub);
	      if(author)
		g_free(author);
	      author=g_strdup(value);
	      free(value);
	    } else if(strcmp(sub->name, "Homepage")==0 && (!website || lang)) {
	      value=xmlNodeGetContent(sub);
	      if(website)
		g_free(website);
	      website=g_strdup(value);
	      free(value);
	    }
	  }
	}
	if(lang)
	  free(lang);
      }
      
      xmlFreeDoc(doc);
      for(l=langs; l; l=g_list_next(l))
	g_free(l->data);
      g_list_free(l);
    }

    g_free(path);
  }

  window=info_win_new(program, purpose, version, author, website);

  if(purpose)
    g_free(purpose);
  if(version)
    g_free(version);
  if(author)
    g_free(author);
  if(website)
    g_free(website);

  return window;
}

/*
 * $Log: infowin.c,v $
 * Revision 1.8  2003/12/13 19:25:39  stephen
 * InfoWin dismiss button is now a stock close.
 *
 * Revision 1.7  2003/10/18 11:46:18  stephen
 * Can parse AppInfo.xml file to supply the values for the window.
 *
 * Revision 1.6  2003/08/28 18:53:58  stephen
 * Add apps icon to window
 * Avoid SEGV if window closed by window manager
 *
 * Revision 1.5  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.4  2002/04/29 08:17:24  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.3  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 * Revision 1.2  2001/11/05 13:59:18  stephen
 * Changed printf to dprintf
 *
 * Revision 1.1.1.1  2001/05/29 14:09:59  stephen
 * Initial version of the library
 *
 */
