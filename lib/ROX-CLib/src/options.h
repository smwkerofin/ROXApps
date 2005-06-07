/*
 * $Id: options.h,v 1.3 2004/10/29 13:36:07 stephen Exp $
 *
 * Options system for ROX-CLib.
 *
 * Derived from code written by Thomas Leonard, <tal197@users.sourceforge.net>.
 */

/** \file options.h
 * \brief Interface to the ROX options system.
 *
 * Manage a programs options in the same way as ROX-Filer does.
 *
 * Derived from code written by Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _options_h
#define _options_h

#include <libxml/parser.h>

/**
 * An option, stored as a string.  It may have an integer value
 */
typedef struct _Option Option;

/** Type of function called when the options change */
typedef void OptionNotify(void);

/** Type of function used to build custom option widgets */
typedef GList * (*OptionBuildFn)(Option *option, xmlNode *node, gchar *label);

/**
 * An option, stored as a string.  It may have an integer value
 */
struct _Option {
  gchar		*value; /**< Current value of the option */
  long		int_value;      /**< Result of atol(value) */
	gboolean	has_changed;  /**< Non-zero if changed */

	gchar		*backup;	/**< Copy of value to Revert to */

	GtkWidget	*widget;		/**< NULL => No UI yet */
  /** Called to set the widget to reflect the options current value
   * @param option the option
   */
	void		(*update_widget)(Option *option);
  /** Called to read the current value from the widget
   * @param option the option
   * @return new value
   */
	gchar * 	(*read_widget)(Option *option);
};

/* Prototypes */

extern void options_init(const char *project);
extern void options_init_with_domain(const char *project, const char *domain);

extern void option_register_widget(char *name, OptionBuildFn builder);
extern void option_check_widget(Option *option);

extern void option_add_int(Option *option, const gchar *key, int value);
extern void option_add_string(Option *option, const gchar *key,
			      const gchar *value);

extern void options_notify(void);
extern void option_add_notify(OptionNotify *callback);
extern void option_add_saver(OptionNotify *callback);

GtkWidget *options_show(void);

#endif

/*
 * $Log: options.h,v $
 * Revision 1.3  2004/10/29 13:36:07  stephen
 * Added rox_choices_load()/save()
 *
 * Revision 1.2  2003/10/18 11:46:56  stephen
 * Make sure xmlNode is defined
 *
 * Revision 1.1  2003/04/16 09:01:19  stephen
 * Added options code from filer
 *
 */

