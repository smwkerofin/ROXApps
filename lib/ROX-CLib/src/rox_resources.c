/*
 * rox_resources.c -Find internationalized resource files.
 *
 * $Id: rox_resources.c,v 1.1 2001/11/05 14:00:27 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <glib.h>

#include "choices.h"
#include "rox_resources.h"

static gboolean try_path(const gchar *path)
{
  return (access(path, R_OK)==0);
}

static gchar *try_dir(const gchar *dir, const gchar *leaf,
		      const gchar *lang)
{
  gchar *path;
  
  if(lang && lang[0]) {
    path=g_strconcat(dir, "/Resources/", lang, "/", leaf, NULL);
    if(try_path(path))
      return path;
    g_free(path);
  }

  path=g_strconcat(dir, "/Resources/", leaf, NULL);
  if(try_path(path))
    return path;
  g_free(path);

  return NULL;
}

static gchar *try_lang(GPtrArray *dirs, const gchar *app_dir, 
				 const gchar *leaf,
				 const gchar *lang)
{
  gchar *answer=NULL;
  int i;

  if(dirs) {
    for(i=0; i<dirs->len; i++) {
      answer=try_dir((gchar *) g_ptr_array_index(dirs, i), leaf, lang);
      if(answer)
	return answer;
    }
  }
  
  return app_dir? try_dir(app_dir, leaf, lang): NULL;
}

static gchar *do_tests(GPtrArray *dirs, const gchar *app_dir, 
				 const gchar *leaf,
				 const gchar *lang)
{
  gchar *slang;
  gchar *answer=NULL;

  answer=try_lang(dirs, app_dir, leaf, lang);
  if(answer)
    return answer;

  if(lang && lang[0]) {
    gchar *sep;
    
    sep=strchr((char *) lang, '.');
    if(sep) {
      gchar *slang;

      slang=g_strndup(lang, sep-lang);
      answer=try_lang(dirs, app_dir, leaf, slang);
      g_free(slang);
      if(answer)
	return answer;
    }

    if(strlen(lang)>2) {
      gchar *slang;

      slang=g_strndup(lang, 2);
      answer=try_lang(dirs, app_dir, leaf, slang);
      g_free(slang);
      if(answer)
	return answer;
    }
  }

  return NULL;
}
  
gchar *rox_resources_find(const gchar *app_name,
				 const gchar *leaf,
				 const gchar *lang)
{
  int i;
  GPtrArray *dirs;
  gchar *app_dir;
  gchar *answer=NULL;

  if(!lang)
    lang=g_getenv("LANG");

  dirs=choices_list_dirs(app_name);
  if(!dirs) {
    choices_init();
    dirs=choices_list_dirs(app_name);
  }

  app_dir=g_getenv("APP_DIR");

  answer=do_tests(dirs, app_dir, leaf, lang);
  
  if(dirs) {
    choices_free_list(dirs);
  }

  return answer;
}

/*
 * $Log: rox_resources.c,v $
 * Revision 1.1  2001/11/05 14:00:27  stephen
 * Added resources finding function
 *
 */
