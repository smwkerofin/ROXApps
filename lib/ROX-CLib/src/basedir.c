/*
 * $Id$
 *
 * XDG base directory functions for ROX-CLib
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

gchar *basedir_save_config_path(const char *resource, const char *leaf)
{
  if(!xdg_config_home)
    init();

  return save_path(xdg_config_home, resource, leaf);
}

gchar *basedir_save_data_path(const char *resource, const char *leaf)
{
  if(!xdg_data_home)
    init();

  return save_path(xdg_data_home, resource, leaf);
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

gchar *basedir_load_config_path(const char *resource, const char *leaf)
{
  if(!xdg_config_dirs)
    init();

  return load_path(xdg_config_dirs, resource, leaf);
}

gchar *basedir_load_data_path(const char *resource, const char *leaf)
{
  if(!xdg_data_dirs)
    init();

  return load_path(xdg_data_dirs, resource, leaf);
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

GList *basedir_load_config_paths(const char *resource, const char *leaf)
{
  if(!xdg_config_dirs)
    init();

  return load_paths(xdg_config_dirs, resource, leaf);
}

GList *basedir_load_data_paths(const char *resource, const char *leaf)
{
  if(!xdg_data_dirs)
    init();

  return load_paths(xdg_data_dirs, resource, leaf);
}

/*
 * $Log$
 */
