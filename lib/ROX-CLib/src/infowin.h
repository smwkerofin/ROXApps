/*
 * A GTK+ Widget to implement a RISC OS style info window
 *
 * $Id: infowin.h,v 1.3 2003/10/18 11:46:18 stephen Exp $
 */

#ifndef __ROX_INFO_WIN_H__
#define __ROX_INFO_WIN_H__

#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ROX_INFO_WIN(obj)          GTK_CHECK_CAST (obj, rox_info_win_get_type (), ROXInfoWin)
#define ROX_INFO_WIN_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, rox_info_win_get_type (), ROXInfoWinClass)
#define ROX_IS_INFO_WIN(obj)       GTK_CHECK_TYPE (obj, rox_info_win_get_type ())

typedef struct _InfoWin       ROXInfoWin;
typedef struct _InfoWinClass  ROXInfoWinClass;

  /* Internal use */
enum info_slots {
    ROX_INFO_WIN_PROGRAM, ROX_INFO_WIN_PURPOSE, ROX_INFO_WIN_VERSION,
    ROX_INFO_WIN_AUTHOR, ROX_INFO_WIN_WEBSITE,

    ROX_INFO_WIN_NSLOT
};

struct _InfoWin
{
  GtkDialog dialog;

  GtkWidget *table;
  GtkWidget *slots[ROX_INFO_WIN_NSLOT];
  
  gchar *web_site;
  GList *browser_cmds;
};

struct _InfoWinClass
{
  GtkDialogClass parent_class;
};

extern GType    rox_info_win_get_type (void);
extern GtkWidget* rox_info_win_new(const gchar *program, const gchar *purpose,
				const gchar *version, const gchar *author,
				const gchar *website);
/* rox_info_win_new_from_appinfo() is the prefered interface */
extern GtkWidget* rox_info_win_new_from_appinfo(const gchar *program);
extern void rox_info_win_add_browser_command(ROXInfoWin *iw, const gchar *cmd);

/* Backwards compatability */
#define INFO_WIN(obj)          ROX_INFO_WIN(obj)
#define INFO_WIN_CLASS(klass)  ROX_INFO_WIN_CLASS(klass)
#define IS_INFO_WIN(obj)       ROX_IS_INFO_WIN(obj)
 
#define InfoWin ROXInfoWin
#define InfoWinClass ROXInfoWinClass

extern GtkWidget* info_win_new(const gchar *program, const gchar *purpose,
				const gchar *version, const gchar *author,
				const gchar *website);
extern GtkWidget* info_win_new_from_appinfo(const gchar *program);
extern void info_win_add_browser_command(ROXInfoWin *iw, const gchar *cmd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/*
 * $Log: infowin.h,v $
 * Revision 1.3  2003/10/18 11:46:18  stephen
 * Can parse AppInfo.xml file to supply the values for the window.
 *
 * Revision 1.2  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.1.1.1  2001/05/29 14:09:59  stephen
 * Initial version of the library
 *
 */
