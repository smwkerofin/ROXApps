/*
 * A GObject to hold a parsed AppInfo.xml file
 *
 * $Id$
 */

#ifndef __ROX_APPINFO_H__
#define __ROX_APPINFO_H__

#include <glib.h>
#include <glib-object.h>
#include <libxml/parser.h>

#define ROX_APPINFO(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, rox_appinfo_get_type (), ROXAppInfo)
#define ROX_APPINFO_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, rox_appinfo_get_type (), ROXAppinfoClass)
#define ROX_IS_APPINFO(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, rox_appinfo_get_type ())

typedef struct _ROXAppInfo       ROXAppInfo;
typedef struct _ROXAppInfoClass  ROXAppInfoClass;

struct _ROXAppInfo
{
  GObject object;

  xmlDocPtr doc;

  gchar *pref_lang;

  /* When pref_lang changes, invalidate these */
  gchar *summary;
  xmlNodePtr about;
  xmlNodePtr about_no_lang;
};

struct _ROXAppInfoClass
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
