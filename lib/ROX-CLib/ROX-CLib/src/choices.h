/*
 * $Id: choices.h,v 1.1.1.1 2001/04/10 12:30:26 stephen Exp $
 *
 * Borrowed from ROX-Filer
 *
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _CHOICES_H
#define _CHOICES_H

void 		choices_init	       (void);
GPtrArray	*choices_list_dirs     (char *dir);
void		choices_free_list      (GPtrArray *list);
gchar 		*choices_find_path_load(char *leaf, char *dir);
gchar	   	*choices_find_path_save(char *leaf, char *dir, gboolean create);

#endif /* _CHOICES_H */
