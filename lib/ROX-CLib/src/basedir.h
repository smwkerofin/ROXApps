/*
 * $Id: basedir.h,v 1.1 2004/03/25 13:10:40 stephen Exp $
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

/* These two are primarily for the use of choices */
extern const gchar *basedir_get_config_home(void);
extern GList *basedir_get_config_paths(void); /* Free list, not contents */

#endif

/*
 * $Log: basedir.h,v $
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

