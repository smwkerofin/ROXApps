/*
 * resources.h - Find internationalized resource files.
 *
 * $Id$
 */

#ifndef _rox_resources_h
#define _rox_resources_h

#define ROX_RESOURCES_NO_LANG ""
#define ROX_RESOURCES_DEFAULT_LANG NULL

/*
 * Search CHOICESPATH, then APP_DIR for a directory called Resources
 * which contains the file leaf, whether in a sub-directory lang or
 * directly.  Return the full path if found (pass to g_free when done)
 * or NULL if not.  lang may be ROX_RESOURCES_NO_LANG to not search
 * for sub-directories, or ROX_RESOURCES_DEFAULT_LANG for the sub-directory
 * appropriate for the selected language
 */
extern gchar *rox_resources_find(const gchar *app_name,
				 const gchar *leaf,
				 const gchar *lang);

#endif

/*
 * $Log$
 */
