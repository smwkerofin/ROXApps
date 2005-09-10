/*
 * $Id: rox_resources.h,v 1.2 2005/08/14 16:07:00 stephen Exp $
 */
/**
 * @file rox_resources.h
 * @brief Find internationalized resource files.
 *
 *
 * @author Stephen Watson
 * @version $Id$
 */

#ifndef _rox_resources_h
#define _rox_resources_h

#define ROX_RESOURCES_NO_LANG ""         /**< Indicates that the resource
					  * is in no language and does not
					  * need translating */
#define ROX_RESOURCES_DEFAULT_LANG NULL  /**< Accept the default language */

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
extern gchar *rox_resources_find_with_domain(const gchar *app_name,
					     const gchar *leaf,
					     const gchar *lang,
					     const gchar *domain);

#endif

/*
 * $Log: rox_resources.h,v $
 * Revision 1.2  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.1  2001/11/05 14:00:27  stephen
 * Added resources finding function
 *
 */
