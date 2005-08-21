/*
 * $Id: basedir.h,v 1.2 2004/09/13 11:29:30 stephen Exp $
 *
 * XDG base directory functions for ROX-CLib
 */

/**
 * @file basedir.h
 * @brief XDG base directory functions for ROX-CLib
 *
 * @author Stephen Watson
 * @version $Id$
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

extern void basedir_free_paths(GList *paths);

/* These two are primarily for the use of choices */
extern const gchar *basedir_get_config_home(void);
extern GList *basedir_get_config_paths(void); /* Free list, not contents */

#endif

/*
 * $Log: basedir.h,v $
 * Revision 1.2  2004/09/13 11:29:30  stephen
 * Choices system can use XDG base directories
 *
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

