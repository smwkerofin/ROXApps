/*
 * $Id: choices.h,v 1.1.1.1 2001/05/29 14:09:58 stephen Exp $
 *
 * Borrowed from ROX-Filer
 *
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 * (Slight changes, mainly const's, by Stephen Watson
 *    <stephen@kerofin.demon.co.uk>)
 */

#ifndef _CHOICES_H
#define _CHOICES_H

extern void 		choices_init	       (void);
extern GPtrArray	*choices_list_dirs     (const char *dir);
extern void		choices_free_list      (GPtrArray *list);
extern gchar 		*choices_find_path_load(const char *leaf,
						const char *dir);
extern gchar	   	*choices_find_path_save(const char *leaf,
						const char *dir,
						gboolean create);

#endif /* _CHOICES_H */
