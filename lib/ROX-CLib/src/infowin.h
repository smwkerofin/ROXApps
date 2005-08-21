/**
 * @file infowin.h
 * @brief A GTK+ Widget to implement a RISC OS style info window
 *
 */
/*
 * $Id: infowin.h,v 1.6 2005/08/14 16:07:00 stephen Exp $
 */

#ifndef __ROX_INFO_WIN_H__
#define __ROX_INFO_WIN_H__

#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Cast object pointer to #ROXInfoWin * or @c NULL if not a
 *  valid #ROXInfoWin *
 * @param[in] obj pointer to object
 * @return pointer to #ROXInfoWin or @c NULL
 */
#define ROX_INFO_WIN(obj)          GTK_CHECK_CAST (obj, rox_info_win_get_type (), ROXInfoWin)
  
/** Cast class pointer to #ROXInfoWinClass * or @c NULL if not a
 *  valid #ROXInfoWinClass *
 * @param[in] klass pointer to class
 * @return pointer to #ROXInfoWinClass or @c NULL
 */
#define ROX_INFO_WIN_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, rox_info_win_get_type (), ROXInfoWinClass)
  
/** Test if object is a #ROXInfoWin
 * @param[in] obj pointer to object
 * @return non-zero if argument points to a #ROXInfoWin
 */
#define ROX_IS_INFO_WIN(obj)       GTK_CHECK_TYPE (obj, rox_info_win_get_type ())

typedef struct ROXInfoWin       ROXInfoWin;
typedef struct ROXInfoWinClass  ROXInfoWinClass;

  /** @internal */
enum info_slots {
    ROX_INFO_WIN_PROGRAM, ROX_INFO_WIN_PURPOSE, ROX_INFO_WIN_VERSION,
    ROX_INFO_WIN_AUTHOR, ROX_INFO_WIN_WEBSITE,

    ROX_INFO_WIN_NSLOT
};

/**
 * @brief Definition of a ROXInfoWin widget.
 *
 * ROXInfoWin manages a standard window for displaying information about
 * a ROX application.
 */
struct ROXInfoWin
{
  GtkDialog dialog;                     /**< Instance of parent class */

  GtkWidget *table;                     /**< Used to layout window */
  GtkWidget *slots[ROX_INFO_WIN_NSLOT]; /**< @internal */
  GtkWidget *extend;                    /**< Extension area, see
					 * rox_info_win_get_extension_area()
					 */
  
  gchar *web_site;                      /**< URL of program's home page */
  GList *browser_cmds;                  /**< Ways to launch browser to
					 * view program's home page.
					 * @deprecated
					 */
};

/**
 * Definition of ROXInfoWin class. @internal
 */
struct ROXInfoWinClass
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
extern GtkWidget *rox_info_win_get_extension_area(ROXInfoWin *iw);

/* Backwards compatability */
#define INFO_WIN(obj)          ROX_INFO_WIN(obj)
#define INFO_WIN_CLASS(klass)  ROX_INFO_WIN_CLASS(klass)
#define IS_INFO_WIN(obj)       ROX_IS_INFO_WIN(obj)
 
/** @deprecated Use ROXInfoWin */
#define InfoWin ROXInfoWin
/** @deprecated Use ROXInfoWinClass */
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
 * Revision 1.6  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.5  2004/05/31 10:47:06  stephen
 * Added mime_handler support (needs testing)
 *
 * Revision 1.4  2004/05/22 15:54:02  stephen
 * InfoWin is now ROXInfoWin
 *
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
