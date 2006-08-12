/*
 * $Id: basedir.h,v 1.5 2005/10/22 10:47:46 stephen Exp $
 *
 * XDG base directory functions for ROX-CLib
 */

/**
 * @file basedir.h
 * @brief XDG base directory functions for ROX-CLib
 *
 * @author Stephen Watson
 * @version $Id: basedir.h,v 1.5 2005/10/22 10:47:46 stephen Exp $
 */

#ifndef _rox_basedir_h
#define _rox_basedir_h

extern gchar *rox_basedir_save_config_path(const char *resource,
					   const char *leaf);
extern gchar *rox_basedir_save_data_path(const char *resource,
					 const char *leaf);

extern gchar *rox_basedir_load_config_path(const char *resource,
					   const char *leaf);
extern gchar *rox_basedir_load_data_path(const char *resource,
					 const char *leaf);

extern GList *rox_basedir_load_config_paths(const char *resource,
					const char *leaf);
extern GList *rox_basedir_load_data_paths(const char *resource,
					  const char *leaf);

extern void rox_basedir_free_paths(GList *paths);

/* These two are primarily for the use of choices */
extern const gchar *rox_basedir_get_config_home(void);
extern GList *rox_basedir_get_config_paths(void); 

/* Old names for backwards compatability */
extern gchar *basedir_save_config_path(const char *resource, const char *leaf);
extern gchar *basedir_save_data_path(const char *resource, const char *leaf);

extern gchar *basedir_load_config_path(const char *resource, const char *leaf);
extern gchar *basedir_load_data_path(const char *resource, const char *leaf);

extern GList *basedir_load_config_paths(const char *resource,
					const char *leaf);
extern GList *basedir_load_data_paths(const char *resource, const char *leaf);

extern const gchar *basedir_get_config_home(void);
extern GList *basedir_get_config_paths(void); 

#endif

/*
 * $Log: basedir.h,v $
 * Revision 1.5  2005/10/22 10:47:46  stephen
 * Removed basedir_free_paths(), don't need to keep it around as it
 * wasn't in the last release.
 *
 * Revision 1.4  2005/10/22 10:42:28  stephen
 * Renamed basedir functions to rox_basedir.
 * Disabled deprecation warning.
 * This is version 2.1.6
 *
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

