/*
 * $Id$
 *
 * XDG base directory functions for ROX-CLib
 */

#ifndef _rox_basedir_h
#define _rox_basedir_h

extern gchar *basedir_save_config_path(const char *resource, const char *leaf);
extern gchar *basedir_save_data_path(const char *resource, const char *leaf);

extern gchar *basedir_load_config_path(const char *resource, const char *leaf);
extern gchar *basedir_load_data_path(const char *resource, const char *leaf);

extern GList *basedir_load_config_paths(const char *resource,
					const char *leaf);
extern GList *basedir_load_data_paths(const char *resource, const char *leaf);

#endif

/*
 * $Log$
 */

