/*
 * A GObject to hold a parsed AppInfo.xml file
 *
 * $Id: appinfo.h,v 1.1 2004/05/22 17:03:57 stephen Exp $
 */

#ifndef __ROX_APPINFO_H__
#define __ROX_APPINFO_H__

#include <glib.h>
#include <glib-object.h>
#include <libxml/parser.h>

/**
 * @file appinfo.h
 * @brief Parsing an applications AppInfo.xml file.
 */

/** Cast pointer into a pointer to ROXAppInfo only if it is valid,
 * otherwise return @c NULL
 * @param[in] obj pointer to object
 * @return pointer to ROXAppInfo or @c NULL for error
 */
#define ROX_APPINFO(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, rox_appinfo_get_type (), ROXAppInfo)

/** Cast pointer into a pointer to ROXAppInfo class only if it is valid,
 * otherwise return @c NULL
 * @param[in] klass pointer to object class
 * @return pointer to ROXAppInfo class or @c NULL for error
 */
#define ROX_APPINFO_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, rox_appinfo_get_type (), ROXAppinfoClass)

/** Check a pointer to ROXAppInfo
 * @param[in] obj pointer to object
 * @return non-zero if a pointer to a ROXAppInfo
 */
#define ROX_IS_APPINFO(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, rox_appinfo_get_type ())

typedef struct ROXAppInfo       ROXAppInfo;
typedef struct ROXAppInfoClass  ROXAppInfoClass;

/**
 * @brief Type used to store a parsed AppInfo file
 */
struct ROXAppInfo
{
  GObject object;                /**< We derive from GObject to enable
				  * reference counting */

  xmlDocPtr doc;                 /**< The parsed XML document */

  gchar *pref_lang;              /**< Prefered language code when
				  * retrieving the Summary and About
				  * information */

  /* When pref_lang changes, invalidate these */
  gchar *summary;                /**< Summary for current language */
  xmlNodePtr about;              /**< About node for current language,
				  * access via rox_appinfo_get_about()
				  * or rox_appinfo_get_about_label() */
  xmlNodePtr about_no_lang;      /**< About node for no declared language
				  * (normally english) */
};

/** @internal */
struct ROXAppInfoClass
{
  GObjectClass parent;
};

extern GType    rox_appinfo_get_type (void);
extern GObject *rox_appinfo_new(void); /* Reads $APP_DIR/AppInfo.xml */
extern GObject *rox_appinfo_new_from_path(const char *path);

extern void rox_appinfo_set_language(ROXAppInfo *ai, const gchar *lang);
extern const gchar *rox_appinfo_get_language(ROXAppInfo *ai);

extern const gchar *rox_appinfo_get_summary(ROXAppInfo *ai);
extern gchar *rox_appinfo_get_about(ROXAppInfo *ai, const gchar *lbl);
extern gchar *rox_appinfo_get_about_label(ROXAppInfo *ai, const gchar *lbl);

extern const xmlNodePtr rox_appinfo_get_element(ROXAppInfo *ai,
						const gchar *element);

extern GList *rox_appinfo_get_mime_type_list(ROXAppInfo *ai,
					     const gchar *element);
extern void rox_appinfo_free_mime_type_list(GList *);
extern GList *rox_appinfo_get_can_run_list(ROXAppInfo *ai);
extern GList *rox_appinfo_get_can_thumbnail_list(ROXAppInfo *ai);

#endif
