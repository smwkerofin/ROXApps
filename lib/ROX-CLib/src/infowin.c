/*
 * A GTK+ Widget to implement a RISC OS style info window
 *
 * $Id: infowin.c,v 1.4 2002/04/29 08:17:24 stephen Exp $
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
  gtk_widget_hide(GTK_WIDGET(data));
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
  
static void info_win_init(InfoWin *iw)
{
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *hbox;
  
  g_signal_connect(G_OBJECT(iw), "delete_event", 
		     G_CALLBACK(trap_client_destroy), 
		     "WM destroy");
  gtk_window_set_position(GTK_WINDOW(iw), GTK_WIN_POS_MOUSE);
  
  iw->web_site=NULL;
  iw->browser_cmds=g_list_append(NULL, "netscape");

  iw->table=gtk_table_new(INFO_WIN_NSLOT, 2, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(iw)->vbox), iw->table,TRUE, TRUE, 2);
  gtk_widget_show(iw->table);

  label=gtk_label_new("Program");
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_PROGRAM, INFO_WIN_PROGRAM+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_PROGRAM, INFO_WIN_PROGRAM+1);

  iw->slots[INFO_WIN_PROGRAM]=frame;

  label=gtk_label_new("Purpose");
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_PURPOSE, INFO_WIN_PURPOSE+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_PURPOSE, INFO_WIN_PURPOSE+1);

  iw->slots[INFO_WIN_PURPOSE]=frame;

  label=gtk_label_new("Version");
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_VERSION, INFO_WIN_VERSION+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_VERSION, INFO_WIN_VERSION+1);

  iw->slots[INFO_WIN_VERSION]=frame;

  label=gtk_label_new("Author");
  gtk_widget_show(label);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), label, 0, 1,
			    INFO_WIN_AUTHOR, INFO_WIN_AUTHOR+1);

  frame=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show(frame);
  gtk_table_attach_defaults(GTK_TABLE(iw->table), frame, 1, 2,
			    INFO_WIN_AUTHOR, INFO_WIN_AUTHOR+1);

  iw->slots[INFO_WIN_AUTHOR]=frame;

  label=gtk_label_new("Web site");
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

  button=gtk_button_new_with_label("Dismiss");
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


/*
 * $Log: infowin.c,v $
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
