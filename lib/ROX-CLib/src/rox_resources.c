/*
 * $Id: rox_resources.c,v 1.5 2005/09/10 16:16:59 stephen Exp $
 */
/**
 * @file rox_resources.c
 * @brief Find internationalized resource files.
 *
 * @author Stephen Watson
 * @version $Id: rox_resources.c,v 1.5 2005/09/10 16:16:59 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <glib.h>

#define DEBUG 1
#include "rox_debug.h"
#include "choices.h"
#include "rox_resources.h"

static gboolean try_path(const gchar *path)
{
  dprintf(3, "Is %s readable? %d", path, (access(path, R_OK)==0));
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
  } else {
    path=g_strconcat(dir, "/Resources/", leaf, NULL);
    if(try_path(path))
      return path;
    g_free(path);
  }

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
  gchar *answer=NULL;

  answer=try_lang(dirs, app_dir, leaf, lang);
  if(answer)
    return answer;

  dprintf(3, "language=%s", lang? lang: "NULL");
  if(lang && lang[0]) {
    gchar *lang2=NULL, *country=NULL, *charset=NULL;
    gchar *slang;
    gchar *dot, *ul;

    dot=strchr((char *) lang, '.');
    ul=strchr((char *) lang, '_');
    if(ul)
      lang2=g_strndup(lang, ul-lang);
    else
      lang2=g_strndup(lang, 2);
    if(dot)
      charset=g_strdup(dot+1);
    if(dot && ul)
      country=g_strndup(ul+1, dot-ul-1);
    else if(ul)
      country=g_strdup(ul+1);

    dprintf(3, "lang=%s, %s country %s charset %s", lang, lang2,
	    country? country: "NULL", charset? charset: "NULL");

    if(charset) {
      slang=g_strconcat(lang2, ".", charset, NULL);
      answer=try_lang(dirs, app_dir, leaf, slang);
      g_free(slang);
    }
    if(!answer && country) {
      slang=g_strconcat(lang2, "_", country, NULL);
      answer=try_lang(dirs, app_dir, leaf, slang);
      g_free(slang);
    }
    if(!answer)
      answer=try_lang(dirs, app_dir, leaf, lang2);
    if(!answer)
      answer=try_lang(dirs, app_dir, leaf, NULL);

    if(lang2)
      g_free(lang2);
    if(country)
      g_free(country);
    if(charset)
      g_free(charset);
  }

  return answer;
}
  
/**
 * Search CHOICESPATH, then APP_DIR for a directory called Resources
 * which contains the file leaf, whether in a sub-directory lang or
 * directly.
 *
 * @param[in] app_name application name, for seaching choices.
 * @param[in] leaf name of file to seach for.
 * @param[in] lang language code,  may be #ROX_RESOURCES_NO_LANG to not search
 * for sub-directories, or #ROX_RESOURCES_DEFAULT_LANG for the sub-directory
 * appropriate for the selected language
 * @return the full path if found (pass to g_free when done), otherwise
 * @c NULL
 *
 * @deprecated Use rox_resources_find_with_domain() instead.
 */
gchar *rox_resources_find(const gchar *app_name,
				 const gchar *leaf,
				 const gchar *lang)
{
  int i;
  GPtrArray *dirs;
  gchar *app_dir;
  gchar *answer=NULL;

  ROX_CLIB_DEPRECATED("rox_resources_find_with_domain");

  if(!lang)
    lang=g_getenv("LANG");

  dirs=choices_list_dirs(app_name);
  if(!dirs) {
    choices_init();
    dirs=choices_list_dirs(app_name);
  }

  app_dir=g_getenv("APP_DIR");

  dprintf(3, "app_dir=%s leaf=%s lang=%s", app_dir, leaf, lang? lang: "NULL");
  answer=do_tests(dirs, app_dir, leaf, lang);
  
  if(dirs) {
    choices_free_list(dirs);
  }

  return answer;
}

/**
 * Search XDG_CONFIG_DIRS, then APP_DIR for a directory called
 * Resources
 * which contains the file leaf, whether in a sub-directory lang or
 * directly.
 *
 * @param[in] app_name application name, for seaching choices.
 * @param[in] leaf name of file to seach for.
 * @param[in] lang language code,  may be #ROX_RESOURCES_NO_LANG to not search
 * for sub-directories, or #ROX_RESOURCES_DEFAULT_LANG for the sub-directory
 * appropriate for the selected language
 * @param[in] domain name of domain, or @c NULL (see rox_choices_list_dirs())
 *
 * @return the full path if found (pass to g_free when done), otherwise
 * @c NULL
 */
gchar *rox_resources_find_with_domain(const gchar *app_name,
				      const gchar *leaf,
				      const gchar *lang,
				      const gchar *domain)
{
  int i;
  GPtrArray *dirs;
  gchar *app_dir;
  gchar *answer=NULL;

  if(!lang)
    lang=g_getenv("LANG");

  dirs=rox_choices_list_dirs(app_name, domain);

  app_dir=g_getenv("APP_DIR");

  dprintf(3, "app_dir=%s leaf=%s lang=%s", app_dir, leaf, lang? lang: "NULL");
  answer=do_tests(dirs, app_dir, leaf, lang);
  
  if(dirs) {
    rox_choices_free_list(dirs);
  }

  return answer;
}

/*
 * $Log: rox_resources.c,v $
 * Revision 1.5  2005/09/10 16:16:59  stephen
 * Added author and version info to the doxygen output
 *
 * Revision 1.4  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.3  2002/02/13 11:00:38  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 * Revision 1.2  2001/11/29 15:44:09  stephen
 * Fixed bug in rox_resources.
 *
 * Revision 1.1  2001/11/05 14:00:27  stephen
 * Added resources finding function
 *
 */
