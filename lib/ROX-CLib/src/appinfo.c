/*
 * A GObject to hold a parsed AppInfo.xml file
 *
 * $Id: appinfo.c,v 1.3 2005/10/12 11:00:22 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <gtk/gtk.h>
#include "appinfo.h"

#define DEBUG 1
#include "rox.h"
#include "rox_debug.h"
#include "mime.h"

/**
 * @file appinfo.c
 * @brief Parsing an applications AppInfo.xml file.
 */

static void rox_appinfo_finalize (GObject *object);

static GObjectClass *parent_class=NULL;

static void rox_appinfo_class_init(gpointer aic)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) aic;

  parent_class = g_type_class_peek_parent(aic);

  /* Set up signals here... */

  object_class->finalize=rox_appinfo_finalize;
}

static void rox_appinfo_init(GTypeInstance *object, gpointer gclass)
{
  ROXAppInfo *appinfo=(ROXAppInfo *) object;

  appinfo->doc=NULL;
  appinfo->pref_lang=NULL;
  appinfo->summary=NULL;
  appinfo->about=NULL;
  appinfo->about_no_lang=NULL;
}

/**
 * @return GType code for the #ROXAppInfo type.
 */
GType rox_appinfo_get_type(void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
		{
			sizeof (ROXAppInfoClass),
			NULL,			/* base_init */
			NULL,			/* base_finalise */
			rox_appinfo_class_init,
			NULL,	                /* class_finalise */
			NULL,			/* class_data */
			sizeof(ROXAppInfo),
			0,			/* n_preallocs */
			rox_appinfo_init
		};

		type = g_type_register_static(G_TYPE_OBJECT, "ROXAppInfo",
					      &info, 0);
	}

	return type;
}

/**
 * Construct a new #ROXAppInfo object for the current application.
 * @return pointer to new #ROXAppInfo object, or @c NULL if the application
 * does not have an AppInfo.xml file.
 */
GObject *rox_appinfo_new(void)
{
  const gchar *app_dir=rox_get_app_dir();
  gchar *path;
  GObject *obj;

  rox_debug_printf(2, "app_dir=%s", app_dir? app_dir: "NULL");
  if(!app_dir)
    return NULL;

  path=g_strconcat(app_dir, "/AppInfo.xml", NULL);

  obj=rox_appinfo_new_from_path(path);
  g_free(path);
  
  return obj;
}

/**
 * Construct a new #ROXAppInfo object for specified AppInfo file.
 * @param[in] path path to AppInfo.xml file
 * @return pointer to new #ROXAppInfo object, or @c NULL if the application
 * does not have an AppInfo.xml file.
 */
GObject *rox_appinfo_new_from_path(const gchar *path)
{
  GObject *obj;
  ROXAppInfo *appinfo;

  obj=g_object_new(rox_appinfo_get_type(), NULL);
  appinfo=ROX_APPINFO(obj);

  appinfo->doc=xmlParseFile(path);

  rox_appinfo_set_language(appinfo, NULL);

  return obj;
}

static void rox_appinfo_finalize(GObject *object)
{
  ROXAppInfo *appinfo=ROX_APPINFO(object);

  if(appinfo->doc)
    xmlFreeDoc(appinfo->doc);

  if(appinfo->pref_lang)
    g_free(appinfo->pref_lang);
}

static gboolean lang_matches(const gchar *lang, const gchar *plang,
			     void *current)
{
  if(!lang && !plang)
    return TRUE;
  if(lang && plang && strcmp(lang, plang)==0)
    return TRUE;
  if(!lang && plang && !current)
    return TRUE;
  return FALSE;
}

static void scan_appinfo(ROXAppInfo *ai)
{
  xmlNodePtr node;
  xmlChar *lang, *value;

  for(node=ai->doc->children->children; node; node=node->next) {
    if(node->type!=XML_ELEMENT_NODE)
      continue;
    
    lang=xmlNodeGetLang(node);
    dprintf(3, "node->name=%s lang=%s", node->name,
	    lang? ((char *) lang): "NULL");
    
    if(strcmp(node->name, "About")==0) {
      if(!lang)
	ai->about_no_lang=node;
      if(lang_matches(lang, ai->pref_lang, ai->about))
	ai->about=node;
      
    } else if(strcmp(node->name, "Summary")==0) {
      value=xmlNodeGetContent(node);
      if(lang_matches(lang, ai->pref_lang, ai->summary)) {
	if(ai->summary)
	  g_free(ai->summary);
	ai->summary=g_strdup(value);
      }
      xmlFree(value);
    }
  }
  
}

/**
 * Set the language used for scanning the AppInfo document.  This updates
 * #ROXAppInfo.pref_lang, #ROXAppInfo.summary, #ROXAppInfo.about
 * and #ROXAppInfo.about_no_lang
 *
 * @param[in,out] ai parsed AppInfo file
 * @param[in] lang language code, e.g. en_GB
 */
void rox_appinfo_set_language(ROXAppInfo *ai, const gchar *lang)
{
  g_return_if_fail(ROX_IS_APPINFO(ai));

  /*if(!lang)
    lang=g_getenv("LANG");*/

  if(lang && ai->pref_lang && strcmp(lang, ai->pref_lang)==0)
    return;

  if(ai->pref_lang)
    g_free(ai->pref_lang);
  ai->pref_lang=g_strdup(lang);

  if(ai->summary)
    g_free(ai->summary);
  ai->summary=NULL;
  ai->about=NULL;
  ai->about_no_lang=NULL;

  scan_appinfo(ai);
}

/**
 * Get the language used for scanning the AppInfo document.
 * @param[in,out] ai parsed AppInfo file
 * @return the language code, e.g. en_GB
 */
const gchar *rox_appinfo_get_language(ROXAppInfo *ai)
{
  g_return_val_if_fail(ROX_IS_APPINFO(ai), NULL);

  return ai->pref_lang;
}

/**
 * Get the contents of the Summary element in the currently set language.
 * @param[in] ai parsed AppInfo file
 * @return the summary
 */
const gchar *rox_appinfo_get_summary(ROXAppInfo *ai)
{
  g_return_val_if_fail(ROX_IS_APPINFO(ai), NULL);

  return ai->summary;
}

static gchar *get_about_from_node(xmlNodePtr about, const gchar *lbl,
			   gboolean want_label)
{
  xmlNodePtr node;
  xmlChar *value;
  gchar *answer=NULL;
  
  for(node=about->children; node; node=node->next) {
    if(node->type!=XML_ELEMENT_NODE)
      continue;
    
    dprintf(3, "node->name=%s", node->name);
    
    if(strcmp(node->name, lbl)==0) {
      if(want_label)
	value=xmlGetProp(node, "label");
      else
	value=xmlNodeGetContent(node);
      
      answer=g_strdup(value);
      xmlFree(value);
      
      break;
    }
  }

  return answer;
}

/**
 * Get the contents of the named element from the About node in the currently
 * set language, or the neutral language (#ROXAppInfo.about_no_lang) if that
 * fails.
 * @param[in] ai parsed AppInfo file
 * @param[in] lbl name of element in About to fetch
 * @return contents of the element, pass to g_free() when done.
 */
gchar *rox_appinfo_get_about(ROXAppInfo *ai, const gchar *lbl)
{
  gchar *answer=NULL;

  g_return_val_if_fail(ROX_IS_APPINFO(ai), NULL);
  g_return_val_if_fail(lbl!=NULL, NULL);
  
  if(ai->about)
    answer=get_about_from_node(ai->about, lbl, FALSE);
  if((!answer||!answer[0]) && ai->about_no_lang) {
    if(answer)
      g_free(answer);
    answer=get_about_from_node(ai->about_no_lang, lbl, FALSE);
  }

  return answer;
}

/**
 * Get the label for the named element from the About node in the currently
 * set language, or the neutral language (#ROXAppInfo.about_no_lang) if that
 * fails.
 * @param[in] ai parsed AppInfo file
 * @param[in] lbl name of element in About to fetch
 * @return label for the element, pass to g_free() when done.
 */
gchar *rox_appinfo_get_about_label(ROXAppInfo *ai, const gchar *lbl)
{
  gchar *answer=NULL;

  g_return_val_if_fail(ROX_IS_APPINFO(ai), NULL);
  g_return_val_if_fail(lbl!=NULL, NULL);

  if(ai->about)
    answer=get_about_from_node(ai->about, lbl, TRUE);
  if((!answer||!answer[0]) && ai->about_no_lang) {
    if(answer)
      g_free(answer);
    answer=get_about_from_node(ai->about_no_lang, lbl, TRUE);
  }

  if(!answer)
    answer=g_strdup(lbl);

  return answer;  
}

/**
 * Get the named element from the AppInfo file
 * @param[in] ai parsed AppInfo file
 * @param[in] element name of element to fetch
 * @return the elment
 */
const xmlNode *rox_appinfo_get_element(ROXAppInfo *ai, const gchar *element)
{
  xmlNodePtr node;
  
  g_return_val_if_fail(ROX_IS_APPINFO(ai), NULL);
  g_return_val_if_fail(element!=NULL, NULL);
  
  for(node=ai->doc->children->children; node; node=node->next) {
    if(node->type!=XML_ELEMENT_NODE)
      continue;
    
    if(strcmp(node->name, element)==0) {
      return node;
    }
  }

  return NULL;
}

/**
 * From the named element from the AppInfo file return a list of MIME types
 * @param[in] ai parsed AppInfo file
 * @param[in] element name of element to fetch
 * @return list of #MIMEType items, pass to
 * rox_appinfo_free_mime_type_list() when done
 */
GList *rox_appinfo_get_mime_type_list(ROXAppInfo *ai, const gchar *element)
{
  GList *list=NULL;
  const xmlNode *node=rox_appinfo_get_element(ai, element);
  xmlNodePtr sub;
  xmlChar *value;
  MIMEType *type;
  
  if(!node)
    return NULL;

  for(sub=node->children; sub; sub=sub->next) {
    if(sub->type!=XML_ELEMENT_NODE)
      continue;
    
    if(strcmp(sub->name, "MimeType")==0) {
      value=xmlGetProp(sub, "type");
      type=rox_mime_lookup_by_name(value);
      xmlFree(value);
      if(type)
	list=g_list_append(list, type);
    }
  }

  return list;
}

/**
 * Free MIME type list.
 * @param[in,out] list MIME type list.
 */
void rox_appinfo_free_mime_type_list(GList *list)
{
  g_list_free(list);
}

/**
 * Return list of types that the application has declared it can run.
 * Equivalent to rox_appinfo_get_mime_type_list(<var>ai</var>, "CanRun").
 * @param[in] ai parsed AppInfo file
 * @return list of #MIMEType items, pass to
 * rox_appinfo_free_mime_type_list() when done
 */
GList *rox_appinfo_get_can_run_list(ROXAppInfo *ai)
{
  return rox_appinfo_get_mime_type_list(ai, "CanRun");
}

/**
 * Return list of types that the application has declared it can generate
 * thumbnails images for..
 * Equivalent to rox_appinfo_get_mime_type_list(<var>ai</var>, "CanThumbnail").
 * @param[in] ai parsed AppInfo file
 * @return list of #MIMEType items, pass to
 * rox_appinfo_free_mime_type_list() when done
 */
GList *rox_appinfo_get_can_thumbnail_list(ROXAppInfo *ai)
{
  return rox_appinfo_get_mime_type_list(ai, "CanThumbnail");
}

/*
 * $Log: appinfo.c,v $
 * Revision 1.3  2005/10/12 11:00:22  stephen
 * Externally visible symbols have rox_ or ROX prefixes.
 * All still exist under the old names but in general will produce a warning message.
 *
 * Revision 1.2  2005/08/21 13:06:16  stephen
 * Added doxygen comments.
 * Renamed struct to ROXAppInfo
 *
 * Revision 1.1  2004/05/22 17:03:57  stephen
 * Added AppInfo parser
 *
 */
