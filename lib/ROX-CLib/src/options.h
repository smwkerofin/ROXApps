/*
 * $Id$
 *
 * Options system for ROX-CLib.
 *
 * Derived from code written by Thomas Leonard, <tal197@users.sourceforge.net>.
 */

#ifndef _options_h
#define _options_h

struct _Option;
typedef struct _Option Option;

typedef void OptionNotify(void);
typedef GList * (*OptionBuildFn)(Option *option, xmlNode *node, gchar *label);

struct _Option {
	gchar		*value;
	long		int_value;
	gboolean	has_changed;

	gchar		*backup;	/* Copy of value to Revert to */

	GtkWidget	*widget;		/* NULL => No UI yet */
	void		(*update_widget)(Option *option);
	gchar * 	(*read_widget)(Option *option);
};

/* Prototypes */

extern void options_init(const char *project);

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
 * $Log$
 */

