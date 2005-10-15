/*
 * $Id: options.h,v 1.7 2005/09/10 16:15:06 stephen Exp $
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
 *
 * How it works:
 *
 * On startup:
 *
 * - The &lt;Choices&gt;/PROJECT/Options file is read in, which contains a list of
 *   name/value pairs, and these are stored in the 'loading' hash table.
 *
 * - Each part of the filer then calls option_add_int(), or a related function,
 *   supplying the name for each option and a default value. Once an option is
 *   registered, it is removed from the loading table.
 *
 * - If things need to happen when values change, modules register with
 *   rox_option_add_notify().
 *
 * - rox_option_register_widget() can be used during initialisation (any time
 *   before the ROXOptions box is displayed) to tell the system how to render a
 *   particular type of option.
 *
 * - Finally, all notify callbacks are called. Use the #ROXOption.has_changed
 *   field to work out what has changed from the defaults.
 *
 * When the user opens the Options box:
 *
 * - The Options.xml file is read and used to create the Options dialog box.
 *   Each element in the file has a key corresponding to an option named
 *   above.
 *
 * - For each widget in the box, the current value of the option is used to
 *   set the widget's state.
 *
 * - All current values are saved for a possible Revert later.
 *
 * When the user changes an option or clicks on Revert:
 *
 * - The option values are updated.
 *
 * - All notify callbacks are called. Use the #ROXOption.has_changed field
 *   to see what changed.
 *
 * When OK is clicked:
 *
 * - If anything changed then:
 *   - All the options are written to the filesystem
 *   - The saver_callbacks are called.
 *
 * @author Thomas Leonard, Stephen Watson
 * @version $Id: options.h,v 1.7 2005/09/10 16:15:06 stephen Exp $
 */

#ifndef _options_h
#define _options_h

#include <libxml/parser.h>

/**
 * An option, stored as a string.  It may have an integer value
 */
typedef struct ROXOption ROXOption;
/** @deprecated use #ROXOption */
typedef struct ROXOption Option;

/** Type of function called when the options change */
typedef void ROXOptionNotify(void);
/** @deprecated use #ROXOptionNotify */
typedef ROXOptionNotify OptionNotify;;

/** Type of function used to build custom option widgets
 *
 * @param[in,out] option option being constructed
 * @param[in] node element of the Options.xml file to use in construction
 * @param[in] label label to attach to the widget
 */
typedef GList * (*ROXOptionBuildFn)(Option *option, xmlNode *node,
				    gchar *label);
/** @deprecated use #ROXOptionBuildFn */
typedef ROXOptionBuildFn OptionBuildFn;

/**
 * An option, stored as a string.  It may have an integer value
 */
struct ROXOption {
  gchar		*value;       /**< Current value of the option */
  long		int_value;    /**< Result of atol(value) */
  gboolean	has_changed;  /**< Non-zero if changed */
  
  gchar		*backup;      /**< Copy of value to Revert to */

  GtkWidget	*widget;      /**< @c NULL => No UI yet */

  /** Called to set the widget to reflect the options current value
   * @param[in,out] option the option
   */
	void		(*update_widget)(ROXOption *option);
  /** Called to read the current value from the widget
   * @param[in] option the option
   * @return new value
   */
	gchar * 	(*read_widget)(ROXOption *option);
};

/* Prototypes */

extern void rox_options_init(const char *project);
extern void rox_options_init_with_domain(const char *project, const char *domain);

extern void rox_option_register_widget(char *name, ROXOptionBuildFn builder);
extern void rox_option_check_widget(ROXOption *option);

extern void rox_option_add_int(ROXOption *option, const gchar *key, int value);
extern void rox_option_add_string(ROXOption *option, const gchar *key,
			      const gchar *value);

extern void rox_options_notify(void);
extern void rox_option_add_notify(ROXOptionNotify *callback);
extern void rox_option_add_saver(ROXOptionNotify *callback);

GtkWidget *rox_options_show(void);

/* Backward compatability */
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
 * Revision 1.7  2005/09/10 16:15:06  stephen
 * Added author and version info to the doxygen output
 *
 * Revision 1.6  2005/08/21 13:05:40  stephen
 * Renamed _Options to Options to aid in doxygenning
 *
 * Revision 1.5  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.4  2005/06/07 10:24:52  stephen
 * Using doxygen to generate documentation
 *
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

