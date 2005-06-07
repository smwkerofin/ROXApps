/*
 * $Id: rox.h,v 1.7 2004/10/29 13:36:07 stephen Exp $
 *
 * rox.h - Top level header for ROX-CLib
 */

#ifndef _rox_h
#define _rox_h

/** \file rox.h
 * \brief Top level header for ROX-CLib.
 *
 * This is the top level header file for ROX-CLib.  It provides the usual
 * facilities required by most ROX programs.
 */

/*
 * Useful headers.  Most programs will need all of these.
 */
#include "rox_debug.h"
#include "choices.h"
#include "options.h"
#include "error.h"
#include "infowin.h"
#include "rox_resources.h"

/* Initialize GTK+ and bits of ROX-CLib */
extern void rox_init(const char *program, int *argc, char ***argv);
extern void rox_init_with_domain(const char *program, const char *domain,
				 int *argc, char ***argv);

/* Return name of program as passed to rox_init.
   Returns NULL if rox_init not called. */
extern const gchar *rox_get_program_name(void);

/* Return icon of program (.DirIcon).  Pass to g_object_ref when done.
   Returns NULL if not available. */
extern GdkPixbuf *rox_get_program_icon(void);

/* Returns $APP_DIR, or NULL if not set */
extern const gchar *rox_get_app_dir(void);

/* Version number of ROX-CLib where x.y.z is encoded as xxyyzz */
extern int rox_clib_version_number(void);
extern const char *rox_clib_version_string(void);

/* And again for the version of GTK+ we work with */
extern int rox_clib_gtk_version_number(void);
extern const char *rox_clib_gtk_version_string(void);

/* Window counting */
extern void rox_add_window(GtkWidget *window);
extern int rox_get_n_windows(void);

extern void rox_main_loop(void);
extern void rox_main_quit(void);

#endif

/*
 * $Log: rox.h,v $
 * Revision 1.7  2004/10/29 13:36:07  stephen
 * Added rox_choices_load()/save()
 *
 * Revision 1.6  2004/10/23 11:50:12  stephen
 * Added window counting
 *
 * Revision 1.5  2004/05/22 17:03:57  stephen
 * Added AppInfo parser
 *
 * Revision 1.4  2004/03/10 22:40:35  stephen
 * Get the .DirIcon in rox_init and set it as default icon for windows
 *
 * Revision 1.3  2003/10/22 17:17:01  stephen
 * Added rox_init
 *
 * Revision 1.2  2002/04/29 08:17:25  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.1  2002/02/13 11:00:37  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 */
