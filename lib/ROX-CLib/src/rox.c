/*
 * $Id: rox.c,v 1.3 2003/10/22 17:17:01 stephen Exp $
 *
 * rox.c - General stuff
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <unistd.h>

#include "rox.h"
#include "options.h"

static gchar *program_name=NULL;
static GdkPixbuf *program_icon=NULL;

void rox_init(const char *program, int *argc, char ***argv)
{
  const gchar *app_dir;
  
  gtk_init(argc, argv);

  program_name=g_strdup(program);
  rox_debug_init(program); 

  choices_init();

  app_dir=g_getenv("APP_DIR");
  if(app_dir) {
    gchar *options_file;
    gchar *dir_icon;
    
    options_file=g_build_filename(app_dir, "Options.xml", NULL);
    if(access(options_file, R_OK)==0) {
      options_init(program);
    }
    g_free(options_file);
    
    dir_icon=g_build_filename(app_dir, ".DirIcon", NULL);
    program_icon=gdk_pixbuf_new_from_file(dir_icon, NULL);
    if(program_icon) {
      GList *list=g_list_append(NULL, program_icon);
      
      gtk_window_set_default_icon_list(list);
      g_list_free(list);
    }
    g_free(dir_icon);
  }
  
  rox_dnd_init();
}

const gchar *rox_get_program_name(void)
{
  return program_name;
}

GdkPixbuf *rox_get_program_icon(void)
{
  if(program_icon)
    g_object_ref(program_icon);
  return program_icon;
}

int rox_clib_version_number(void)
{
  int ver=0;
  gchar **words;

  words=g_strsplit(ROXCLIB_VERSION, " ", 2);
  if(words) {
    gchar **els;

    els=g_strsplit(words[0], ".", 3);
    if(els) {

      ver=atoi(els[2]) + 100*atoi(els[1]) + 10000*atoi(els[0]);

      g_strfreev(els);
    }
    g_strfreev(words);
  }

  return ver;
}

const char *rox_clib_version_string(void)
{
  return ROXCLIB_VERSION;
}

int rox_clib_gtk_version_number(void)
{
  return GTK_MAJOR_VERSION*10000+GTK_MINOR_VERSION*100+GTK_MICRO_VERSION;
}

const char *rox_clib_gtk_version_string(void)
{
  static char buf[32];

  sprintf(buf, "%d.%d.%d", GTK_MAJOR_VERSION, GTK_MINOR_VERSION,
	  GTK_MICRO_VERSION);
  
  return buf;
}

/*
 * $Log: rox.c,v $
 * Revision 1.3  2003/10/22 17:17:01  stephen
 * Added rox_init
 *
 * Revision 1.2  2002/04/29 08:17:24  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.1  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 */
