/*
 * A GTK+ Widget to implement a RISC OS style info window
 *
 * $Id: infowin.h,v 1.2 2003/03/05 15:31:23 stephen Exp $
 */

#ifndef __INFO_WIN_H__
#define __INFO_WIN_H__

#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define INFO_WIN(obj)          GTK_CHECK_CAST (obj, info_win_get_type (), InfoWin)
#define INFO_WIN_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, info_win_get_type (), InfoWinClass)
#define IS_INFO_WIN(obj)       GTK_CHECK_TYPE (obj, info_win_get_type ())

typedef struct _InfoWin       InfoWin;
typedef struct _InfoWinClass  InfoWinClass;

  /* Internal use */
enum info_slots {
    INFO_WIN_PROGRAM, INFO_WIN_PURPOSE, INFO_WIN_VERSION, INFO_WIN_AUTHOR,
    INFO_WIN_WEBSITE,

    INFO_WIN_NSLOT
};

struct _InfoWin
{
  GtkDialog dialog;

  GtkWidget *table;
  GtkWidget *slots[INFO_WIN_NSLOT];
  
  gchar *web_site;
  GList *browser_cmds;
};

struct _InfoWinClass
{
  GtkDialogClass parent_class;
};

extern GType    info_win_get_type (void);
extern GtkWidget* info_win_new(const gchar *program, const gchar *purpose,
				const gchar *version, const gchar *author,
				const gchar *website);
extern GtkWidget* info_win_new_from_appinfo(const gchar *program);
extern void info_win_add_browser_command(InfoWin *iw, const gchar *cmd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/*
 * $Log: infowin.h,v $
 * Revision 1.2  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.1.1.1  2001/05/29 14:09:59  stephen
 * Initial version of the library
 *
 */
