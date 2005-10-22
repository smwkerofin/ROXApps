/*
 * $Id: basedir.c,v 1.3 2005/08/21 13:05:09 stephen Exp $
 *
 * XDG base directory functions for ROX-CLib
 */

/**
 * @file basedir.c
 * @brief XDG base directory functions for ROX-CLib
 *
 * @author Stephen Watson
 * @version $Id: basedir.c,v 1.3 2005/08/21 13:05:09 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>

#include <glib.h>

#include "basedir.h"

static gchar **xdg_data_dirs=NULL;
static gchar **xdg_config_dirs=NULL;
static const gchar *xdg_data_home=NULL;
static const gchar *xdg_config_home=NULL;

static gchar **split_path(const gchar *first, const gchar *pstring)
{
  gchar **spl;
  gchar **final;
  int i, n;

  spl=g_strsplit(pstring, ":", -1);

  if(spl) {
    for(n=0; spl[n]; n++)
      ;

    final=g_new(gchar *, n+2);
    final[0]=(gchar *) first;
    for(i=0; i<n; i++)
      final[i+1]=spl[i];
    final[i+1]=NULL;

    g_free(spl);
    
  } else {
    final=g_new(gchar *, 2);
    final[0]=first;
    final[1]=NULL;
  }

  return final;
}

static void init(void)
{
  const gchar *home=g_getenv("HOME");
  const gchar *tmp;

  if(!home)
    home="/";

  xdg_data_home=g_getenv("XDG_DATA_HOME");
  if(!xdg_data_home || !xdg_data_home[0])
    xdg_data_home=g_build_filename(home, ".local", "share", NULL);

  xdg_config_home=g_getenv("XDG_CONFIG_HOME");
  if(!xdg_config_home || !xdg_config_home[0])
    xdg_config_home=g_build_filename(home, ".config", NULL);

  tmp=g_getenv("XDG_DATA_DIRS");
  if(!tmp || !tmp[0])
    tmp="/usr/local/share:/usr/share";
  xdg_data_dirs=split_path(xdg_data_home, tmp);

  tmp=g_getenv("XDG_CONFIG_DIRS");
  if(!tmp || !tmp[0])
    tmp="/etc/xdg";
  xdg_config_dirs=split_path(xdg_config_home, tmp);
}

/**
 * @return The directory where the user may write configuration files.
 */
const gchar *rox_basedir_get_config_home(void)
{
  if(!xdg_config_home)
    init();

  return xdg_config_home;
}

/**
 * @return The directory where the user may write configuration files.
 *
 * @deprecated use rox_basedir_get_config_home()
 */
const gchar *basedir_get_config_home(void)
{
  ROX_CLIB_DEPRECATED("rox_basedir_get_config_home");

  return rox_basedir_get_config_home();
}

/**
 * @return A list of directories to be searched for configuration files.  Free
 * the list but not the directories.
 */
GList *rox_basedir_get_config_paths(void)
{
  GList *paths=NULL;
  int i;
  
  if(!xdg_config_home)
    init();

  for(i=0; xdg_config_dirs[i]; i++) {
    paths=g_list_append(paths, xdg_config_dirs[i]);
  }
  
  return paths;
}

/**
 * @return A list of directories to be searched for configuration files.  Free
 * the list but not the directories.
 *
 * @deprecated use rox_basedir_get_config_home()
 */
GList *basedir_get_config_paths(void)
{
  ROX_CLIB_DEPRECATED("rox_basedir_get_config_paths");
  
  return rox_basedir_get_config_paths();
}

static gchar *save_path(const gchar *base, const char *resource,
			const char *leaf)
{
  gchar *path;
  
  if(!g_file_test(base, G_FILE_TEST_EXISTS)) {
    if(mkdir(base, 0755)) {
      return NULL;
    }
  }
  if(!g_file_test(base, G_FILE_TEST_IS_DIR)) {
    return NULL;
  }
  
  path=g_build_filename(base, resource, NULL);

  if(!g_file_test(path, G_FILE_TEST_EXISTS)) {
    if(mkdir(path, 0755)) {
      g_free(path);
      return NULL;
    }
  }
  if(!g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_free(path);
    return NULL;
  }
  if(leaf) {
    g_free(path);
    path=g_build_filename(base, resource, leaf, NULL);
  }

  return path;
}

/**
 * Return the full path to a file where configuration may be saved.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if saving has been disabled
 */
gchar *rox_basedir_save_config_path(const char *resource, const char *leaf)
{
  if(!xdg_config_home)
    init();

  return save_path(xdg_config_home, resource, leaf);
}

/**
 * Return the full path to a file where configuration may be saved.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if saving has been disabled.
 *
 * @deprecated use rox_basedir_save_config_path().
 */
gchar *basedir_save_config_path(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_save_config_path");

  return rox_basedir_save_config_path(resource, leaf);
}

/**
 * Return the full path to a file where data may be saved.  Normally this
 * is not required.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL if
 * saving has been disabled
 */
gchar *rox_basedir_save_data_path(const char *resource, const char *leaf)
{
  if(!xdg_data_home)
    init();

  return save_path(xdg_data_home, resource, leaf);
}

/**
 * Return the full path to a file where data may be saved.  Normally this
 * is not required.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL if
 * saving has been disabled.
 *
 * @deprecated use rox_basedir_save_data_path().
 */
gchar *basedir_save_data_path(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_save_data_path");

  return rox_basedir_save_data_path(resource, leaf);
}

static gchar *load_path(gchar **dirs, const char *resource, const char *leaf)
{
  int i;
  gchar *path;

  for(i=0; dirs[i]; i++) {
    path=g_build_filename(dirs[i], resource, leaf, NULL);
    if(g_file_test(path, G_FILE_TEST_EXISTS))
      return path;
    g_free(path);
  }

  return NULL;
}

/**
 * Return the full path to a file from which configuration may be loaded.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if no such file found.
 */
gchar *rox_basedir_load_config_path(const char *resource, const char *leaf)
{
  if(!xdg_config_dirs)
    init();

  return load_path(xdg_config_dirs, resource, leaf);
}

/**
 * Return the full path to a file from which configuration may be loaded.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if no such file found.
 *
 * @deprecated use rox_basedir_load_config_path().
 */
gchar *basedir_load_config_path(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_load_config_path");

  return rox_basedir_load_config_path(resource, leaf);
}

/**
 * Return the full path to a file from which data may be loaded.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if no such file found.
 */
gchar *rox_basedir_load_data_path(const char *resource, const char *leaf)
{
  if(!xdg_data_dirs)
    init();

  return load_path(xdg_data_dirs, resource, leaf);
}

/**
 * Return the full path to a file from which data may be loaded.
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return Full path to the file, pass to g_free() when done, or @c NULL
 * if no such file found.
 */
gchar *basedir_load_data_path(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_load_data_path");

  return rox_basedir_load_data_path(resource, leaf);
}

static GList *load_paths(gchar **dirs, const char *resource, const char *leaf)
{
  int i;
  gchar *path;
  GList *paths=NULL;

  for(i=0; dirs[i]; i++) {
    path=g_build_filename(dirs[i], resource, leaf, NULL);
    if(g_file_test(path, G_FILE_TEST_EXISTS))
      paths=g_list_append(paths, path);
    else
      g_free(path);
  }

  return paths;
}

/**
 * Return a list of full paths to a file from which configuration may be
 * loaded in each diretory where it exists.  Useful for merging the
 * conents of multiple files..
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return List of full path to files, pass to rox_basedir_free_paths() when done,
 * or @c NULL if no such file found.
 */
GList *rox_basedir_load_config_paths(const char *resource, const char *leaf)
{
  if(!xdg_config_dirs)
    init();

  return load_paths(xdg_config_dirs, resource, leaf);
}

/**
 * Return a list of full paths to a file from which configuration may be
 * loaded in each diretory where it exists.  Useful for merging the
 * conents of multiple files..
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return List of full path to files, pass to rox_basedir_free_paths() when done,
 * or @c NULL if no such file found.
 *
 * @deprecated use rox_basedir_load_config_paths().
 */
GList *basedir_load_config_paths(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_load_config_paths");

  return rox_basedir_load_config_paths(resource, leaf);
}

/**
 * Return a list of full paths to a file from which data may be
 * loaded in each diretory where it exists.  Useful for merging the
 * conents of multiple files..
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return List of full path to files, pass to rox_basedir_free_paths() when done,
 * or @c NULL if no such file found.
 */
GList *rox_basedir_load_data_paths(const char *resource, const char *leaf)
{
  if(!xdg_data_dirs)
    init();

  return load_paths(xdg_data_dirs, resource, leaf);
}

/**
 * Return a list of full paths to a file from which data may be
 * loaded in each diretory where it exists.  Useful for merging the
 * conents of multiple files..
 *
 * @param[in] resource Either the name of the program ("Clock") or
 * the programmers domain and the name of the program as
 * "kerofin.demon.co.uk/Clock"
 * @param[in] leaf Last part of file name, such as "alarms.xml".
 * @return List of full path to files, pass to rox_basedir_free_paths() when done,
 * or @c NULL if no such file found.
 *
 * @deprecated use rox_basedir_load_data_paths().
 */
GList *basedir_load_data_paths(const char *resource, const char *leaf)
{
  ROX_CLIB_DEPRECATED("rox_basedir_load_data_paths");

  return rox_basedir_load_data_paths(resource, leaf);
}

/** Free a list returned by basedir_load_config_paths() or
 * basedir_load_data_paths().
 *
 * @param[in,out] paths list of paths.
 */
void rox_basedir_free_paths(GList *paths)
{
  GList *p;

  for(p=paths; p; p=g_list_next(p))
    g_free(p);
  g_list_free(paths);
}

/*
 * $Log: basedir.c,v $
 * Revision 1.3  2005/08/21 13:05:09  stephen
 * Added doxygen comments.
 * Added basedir_free_paths
 *
 * Revision 1.2  2004/09/13 11:29:30  stephen
 * Choices system can use XDG base directories
 *
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */
