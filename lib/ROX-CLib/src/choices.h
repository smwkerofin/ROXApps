/*
 * $Id: choices.h,v 1.5 2005/08/14 16:07:00 stephen Exp $
 *
 * Borrowed from ROX-Filer
 *
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 * (Slight changes, mainly const's, by Stephen Watson
 *    <stephen@kerofin.demon.co.uk>)
 */

/** \file choices.h
 * \brief Interface to the ROX choices system
 *
 * This declares both the original ROX choices system controlled by
 * $CHOICESPATH and the new XDG based system.  The original was from
 * ROX-Filer and was by
 * Thomas Leonard <tal197@users.sourceforge.net>.
 *
 * These functions are deprecated and should not be used in new code:
 * - choices_init()
 * - choices_list_dirs()
 * - choices_free_list()
 * - choices_find_path_load()
 * - choices_find_path_save()
 *
 * @version $Id$
 * @author Thomas Leonard, Stephen Watson.
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

/*
 * These use XDG (see basedir.c), falling back on choices for loading only.
 * domain is optional and may be NULL.  If given it should be a domain name
 * identifying the software, i.e. I use kerofin.demon.co.uk.  If you don't
 * have a domain name, use an email address, e.g. me@my-isp.org
 */
extern gchar *rox_choices_load(const char *leaf, const char *dir,
			       const char *domain);
extern gchar *rox_choices_save(const char *leaf, const char *dir,
			       const char *domain);
extern GPtrArray *rox_choices_list_dirs(const char *dir,
			       const char *domain);
extern void rox_choices_free_list(GPtrArray *list);

#endif /* _CHOICES_H */
