/*
 * $Id: choices.c,v 1.10 2005/10/22 10:42:28 stephen Exp $
 *
 * Borrowed from:
 * ROX-Filer, filer for the ROX desktop project
 * Copyright (C) 2000, Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */
/** @file choices.c
 * @brief Code for handling loading and saving of user choices.
 *
 * The current versions use a parameter "domain" to ensure that two
 * programs from different sources but with the same name do not clash.
 * domain is optional and may be NULL.  If given it should be a domain name
 * identifying the software, i.e. I use kerofin.demon.co.uk.  If you don't
 * have a domain name, use an email address, e.g. me@my-isp.org
 *
 *
 * These functions are deprecated and should not be used in new code:
 * - choices_init()
 * - choices_list_dirs()
 * - choices_free_list()
 * - choices_find_path_load()
 * - choices_find_path_save()
 *
 * @version $Id: choices.c,v 1.10 2005/10/22 10:42:28 stephen Exp $
 * @author Thomas Leonard, Stephen Watson.
 * Borrowed from:
 * ROX-Filer, filer for the ROX desktop project
 * Copyright (C) 2000, Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * @version $Id: choices.c,v 1.10 2005/10/22 10:42:28 stephen Exp $
 * @author Thomas Leonard, Stephen Watson.
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>

#define DEBUG 1
#include "rox_debug.h"
#include "choices.h"
#include "basedir.h"

static gboolean saving_disabled = TRUE;
static gchar **dir_list = NULL;

/* Static prototypes */
static gboolean exists(char *path);
static void init_choices(void);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/


/** Checks $CHOICESPATH to construct the directory list table for the old choices_system.
 * You must call this before using any other choices_* 
 * functions.
 *
 * If the environment variable does not exist then the defaults are used.
 *
 * @deprecated This is called by rox_init_with_domain() and you should call
 * that in preference to this.
 */
void choices_init(void)
{
	g_return_if_fail(dir_list == NULL);

	init_choices();
}

/** Returns an array of the directories in CHOICESPATH which contain
 * a subdirectory called 'dir'.
 *
 * Lower-indexed results should override higher-indexed ones.
 *
 * Free the list using choices_free_list().
 *
 * @param[in] dir directory to search for.
 * @return pointer array of results.
 *
 * @deprecated use rox_choices_list_dirs() instead.
 */
GPtrArray *choices_list_dirs(const char *dir)
{
	GPtrArray	*list;
	gchar		**cdir = dir_list;

	g_return_val_if_fail(dir_list != NULL, NULL);

	list = g_ptr_array_new();

	while (*cdir)
	{
		gchar	*path;

		path = g_strconcat(*cdir, "/", dir, NULL);
		if (exists(path))
			g_ptr_array_add(list, path);
		else
			g_free(path);

		cdir++;
	}

	return list;
}

/** Free the data returned by choices_list_dirs().
 *
 * @param[in,out] list Return value from choices_list_dirs().
 * 
 * @deprecated use rox_choices_list_dirs() instead.
 */
void choices_free_list(GPtrArray *list)
{
	guint	i;

	g_return_if_fail(list != NULL);

	for (i = 0; i < list->len; i++)
		g_free(g_ptr_array_index(list, i));

	g_ptr_array_free(list, TRUE);
}

/** Get the pathname of a choices file to load. Eg:
 *
 * choices_find_path_load("menus", "ROX-Filer")
 *		 		-> "/usr/local/share/Choices/ROX-Filer/menus".
 *
 * The return values may be NULL - use built-in defaults - otherwise
 * g_free() the result.
 *
 * @deprecated Use rox_choices_load() instead.
 *
 * @param[in] leaf last part of file name (may include a relative directory part)
 * @param[in] dir directory to locate in $CHOICESPATH, normally the name of the program
 * @return path to file (pass to g_free() when done) or NULL
 */
gchar *choices_find_path_load(const char *leaf, const char *dir)
{
	gchar		**cdir = dir_list;

	g_return_val_if_fail(dir_list != NULL, NULL);

	while (*cdir)
	{
		gchar	*path;

		path = g_strconcat(*cdir, "/", dir, "/", leaf, NULL);

		if (exists(path))
			return path;

		g_free(path);

		cdir++;
	}

	return NULL;
}

/** Returns the pathname of a file to save to, or NULL if saving is
 * disabled. If 'create' is TRUE then intermediate directories will
 * be created (set this to FALSE if you just want to find out where
 * a saved file would go without actually altering the filesystem).
 *
 * g_free() the result.
 *
 * @deprecated Use rox_choices_save() instead
 *
 * @param[in] leaf last part of file name (may include a relative directory part)
 * @param[in] dir directory to locate in $CHOICESPATH, normally the name of the program
 * @param[in] create if non-zero create missing directories
 * 
 * @return path to file (pass to g_free() when done) or NULL
 */
gchar *choices_find_path_save(const char *leaf, const char *dir,
			      gboolean create)
{
	gchar	*path, *retval;
	
	g_return_val_if_fail(dir_list != NULL, NULL);

	if (saving_disabled)
		return NULL;

	if (create && !exists(dir_list[0]))
	{
		if (mkdir(dir_list[0], 0777))
			g_warning("mkdir(%s): %s\n", dir_list[0],
					g_strerror(errno));
	}

	path = g_strconcat(dir_list[0], "/", dir, NULL);
	if (create && !exists(path))
	{
		if (mkdir(path, 0777))
			g_warning("mkdir(%s): %s\n", path, g_strerror(errno));
	}

	retval = g_strconcat(path, "/", leaf, NULL);
	g_free(path);

	return retval;
}

/** Returns the pathname of a file to save to, or NULL if saving is
 * disabled. 
 *
 * g_free() the result.
 *
 * @param[in] leaf last part of file name (may include a relative directory part)
 * @param[in] dir directory to locate in $CHOICESPATH, normally the name of the program
 * @param[in] domain domain of the programs author, or email address.  Used to
 * uniquly identify programs with similar names.
 * 
 * @return path to file (pass to g_free() when done) or NULL
 */
gchar *rox_choices_save(const char *leaf, const char *dir,
			       const char *domain)
{
  gchar *path;

  rox_debug_printf(2, "rox_choices_save(%s, %s, %s)", leaf? leaf: "NULL",
		   dir, domain? domain: "NULL");
  
  if(domain) {
    gchar *resource;
    gchar *tmp;

    tmp=rox_basedir_save_config_path(domain, dir);
    rox_debug_printf(3, "tmp=%s", tmp? tmp: "NULL");
    g_free(tmp);

    resource=g_strconcat(domain, "/", dir, NULL);
    path=rox_basedir_save_config_path(resource, leaf);
    g_free(resource);
  } else {
    path=rox_basedir_save_config_path(dir, leaf);
  }
  rox_debug_printf(2, "=%s", path? path: "NULL");

  return path;
}

/** Get the pathname of a choices file to load. Eg:
 *
 * <code>
 * rox_choices_load("menus", "ROX-Filer", "rox.sourceforge.net")
 *		 	    -> "/etc/xdg/rox.sourceforge.net/ROX-Filer/menus".
 * </code>
 *
 * The return values may be NULL - use built-in defaults - otherwise
 * g_free() the result.
 *
 * This uses rox_basedir_load_config_path(), falling back on choices_find_path_load() if that does not return a result.
 *
 * @param[in] leaf last part of file name (may include a relative directory part)
 * @param[in] dir directory to locate in $CHOICESPATH, normally the name of the program
 * @param[in] domain domain of the programs author, or email address.  Used to
 * uniquely identify programs with similar names.
 * @return path to file (pass to g_free() when done) or NULL
 */
gchar *rox_choices_load(const char *leaf, const char *dir,
			       const char *domain)
{
  gchar *path;
  
  if(domain) {
    gchar *resource;

    resource=g_strconcat(domain, "/", dir, NULL);
    path=rox_basedir_load_config_path(resource, leaf);
    g_free(resource);
  } else {
    path=rox_basedir_load_config_path(dir, leaf);
  }

  if(!path)
    path=choices_find_path_load(leaf, dir);

  return path;
}

/** Returns an array of the directories in XDG_CONFIG_DIRS which contain
 * a subdirectory called 'domain/dir'.
 *
 * Lower-indexed results should override higher-indexed ones.
 *
 * Free the list using rox_choices_free_list().
 *
 * @param[in] dir directory to search for.
 * @param[in] domain domain of the programs author, or email address.  Used to
 * uniquely identify programs with similar names.
 * @return pointer array of results.
 */
GPtrArray *rox_choices_list_dirs(const char *domain, const char *dir)
{
	GPtrArray	*list;
	GList *dirs, *d;
	gchar *tmp;

	g_return_val_if_fail(dir_list != NULL, NULL);

	if(domain)
	  tmp=g_strconcat(domain, "/", dir, NULL);
	else
	  tmp=g_strdup(dir);
	
	list = g_ptr_array_new();
	dirs=rox_basedir_get_config_paths();

	for(d=dirs; d; d=g_list_next(d)) {
		gchar	*path;

		path = g_strconcat((gchar *) d->data, "/", tmp, NULL);
		if (exists(path))
			g_ptr_array_add(list, path);
		else
			g_free(path);

	}
	g_list_free(dirs);

	return list;
}

/** Free the data returned by rox_choices_list_dirs().
 *
 * @param[in,out] list Return value from rox_choices_list_dirs().
 */
void rox_choices_free_list(GPtrArray *list)
{
	guint	i;

	g_return_if_fail(list != NULL);

	for (i = 0; i < list->len; i++)
		g_free(g_ptr_array_index(list, i));

	g_ptr_array_free(list, TRUE);
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/


/* Returns TRUE if the object exists, FALSE if it doesn't */
static gboolean exists(char *path)
{
	struct stat info;

	return stat(path, &info) == 0;
}

/* Initialize using old choices system */
/* Reads in CHOICESPATH and constructs the directory list table.
 * You must call this before using any other choices_* functions.
 *
 * If CHOICESPATH does not exist then a suitable default is used.
 */
static void init_choices(void)
{
	char	*choices;

	g_return_if_fail(dir_list == NULL);

	choices = getenv("CHOICESPATH");
	
	if (choices)
	{
		if (*choices != ':' && *choices != '\0')
			saving_disabled = FALSE;

		while (*choices == ':')
			choices++;

		if (*choices == '\0')
		{
			dir_list = g_new(char *, 1);
			dir_list[0] = NULL;
		}
		else
			dir_list = g_strsplit(choices, ":", 0);
	}
	else
	{
		saving_disabled = FALSE;

		dir_list = g_new(gchar *, 4);
		dir_list[0] = g_strconcat(getenv("HOME"), "/Choices", NULL);
		dir_list[1] = g_strdup("/usr/local/share/Choices");
		dir_list[2] = g_strdup("/usr/share/Choices");
		dir_list[3] = NULL;
	}

}

/*
 * $Log: choices.c,v $
 * Revision 1.10  2005/10/22 10:42:28  stephen
 * Renamed basedir functions to rox_basedir.
 * Disabled deprecation warning.
 * This is version 2.1.6
 *
 * Revision 1.9  2005/10/12 10:56:12  stephen
 * Mark the old choices_*() functions as deprecated.
 *
 * Revision 1.8  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.7  2005/06/07 10:24:52  stephen
 * Using doxygen to generate documentation
 *
 * Revision 1.6  2004/10/29 13:36:07  stephen
 * Added rox_choices_load()/save()
 *
 */
