/*
 * $Id: choices.c,v 1.4 2002/02/13 11:00:36 stephen Exp $
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
/* choices.c - code for handling loading and saving of user choices */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>

#include "choices.h"
#include "basedir.h"

/* change to TRUE when we switch to XDG */
#define ROX_USE_XDG_DIRS_DEFAULT FALSE  

static gboolean saving_disabled = TRUE;
static gchar **dir_list = NULL;

/* Static prototypes */
static gboolean exists(char *path);
static void init_basedir(void);
static void init_choices(void);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/


/* Reads in CHOICESPATH and constructs the directory list table.
 * You must call this before using any other choices_* functions.
 *
 * If CHOICESPATH does not exist then a suitable default is used.
 */
void choices_init(void)
{
	char	*evar;
	int      use_xdg=ROX_USE_XDG_DIRS_DEFAULT;

	g_return_if_fail(dir_list == NULL);

	evar=getenv("ROX_USE_XDG_DIRS");

	if(evar) {
	  if(g_ascii_strcasecmp(evar, "yes")==0 ||
	     g_ascii_strcasecmp(evar, "true")==0 || atoi(evar))
	    use_xdg=TRUE;
	  else if(g_ascii_strcasecmp(evar, "no")==0 ||
		  g_ascii_strcasecmp(evar, "false")==0)
	    use_xdg=FALSE;
	} 

	if(use_xdg)
	  init_basedir();
	else
	  init_choices();

}

/* Returns an array of the directories in CHOICESPATH which contain
 * a subdirectory called 'dir'.
 *
 * Lower-indexed results should override higher-indexed ones.
 *
 * Free the list using choices_free_list().
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

void choices_free_list(GPtrArray *list)
{
	guint	i;

	g_return_if_fail(list != NULL);

	for (i = 0; i < list->len; i++)
		g_free(g_ptr_array_index(list, i));

	g_ptr_array_free(list, TRUE);
}

/* Get the pathname of a choices file to load. Eg:
 *
 * choices_find_path_load("menus", "ROX-Filer")
 *		 		-> "/usr/local/share/Choices/ROX-Filer/menus".
 *
 * The return values may be NULL - use built-in defaults - otherwise
 * g_free() the result.
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

/* Returns the pathname of a file to save to, or NULL if saving is
 * disabled. If 'create' is TRUE then intermediate directories will
 * be created (set this to FALSE if you just want to find out where
 * a saved file would go without actually altering the filesystem).
 *
 * g_free() the result.
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

/* Initialize using new XDG system */
static void init_basedir(void)
{
  const gchar *home;
  GList *paths, *p;
  int i, n;

  home=basedir_get_config_home();
  saving_disabled=(home!=NULL);

  paths=basedir_get_config_paths();
  n=g_list_length(paths);

  dir_list=g_new(gchar *, n+1);
  for(i=0, p=paths; p; p=g_list_next(p), i++)
    dir_list[i]=g_strdup((const gchar *) p->data);
  dir_list[n]=NULL;

  g_list_free(paths);
}
