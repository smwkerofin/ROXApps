/*
 * A GObject to hold a parsed AppInfo.xml file
 *
 * $Id$
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

static void rox_appinfo_finalize (GObject *object);

static GObjectClass *parent_class=NULL;

static void rox_appinfo_class_init(ROXAppInfoClass *aic)
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
    dprintf(3, "node->name=%s lang=%s", node->name, lang? lang: "NULL");
    
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

void rox_appinfo_set_language(ROXAppInfo *ai, const gchar *lang)
{
  g_return_if_fail(ai!=NULL);

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

const gchar *rox_appinfo_get_language(ROXAppInfo *ai)
{
  g_return_val_if_fail(ai!=NULL, NULL);

  return ai->pref_lang;
}

const gchar *rox_appinfo_get_summary(ROXAppInfo *ai)
{
  g_return_val_if_fail(ai!=NULL, NULL);

  return ai->summary;
}

gchar *get_about_from_node(xmlNodePtr about, const gchar *lbl,
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

gchar *rox_appinfo_get_about(ROXAppInfo *ai, const gchar *lbl)
{
  xmlNodePtr node;
  xmlChar *value;
  gchar *answer=NULL;

  g_return_val_if_fail(ai!=NULL, NULL);
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

gchar *rox_appinfo_get_about_label(ROXAppInfo *ai, const gchar *lbl)
{
  xmlNodePtr node;
  xmlChar *value;
  gchar *answer=NULL;

  g_return_val_if_fail(ai!=NULL, NULL);
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

const xmlNodePtr rox_appinfo_get_element(ROXAppInfo *ai, const gchar *element)
{
  xmlNodePtr node;
  
  g_return_val_if_fail(ai!=NULL, NULL);
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

GList *rox_appinfo_get_mime_type_list(ROXAppInfo *ai, const gchar *element)
{
  GList *list=NULL;
  const xmlNodePtr node=rox_appinfo_get_element(ai, element);
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
      type=mime_lookup_by_name(value);
      xmlFree(value);
      if(type)
	list=g_list_append(list, type);
    }
  }

  return list;
}

void rox_appinfo_free_mime_type_list(GList *list)
{
  g_list_free(list);
}

GList *rox_appinfo_get_can_run_list(ROXAppInfo *ai)
{
  return rox_appinfo_get_mime_type_list(ai, "CanRun");
}

GList *rox_appinfo_get_can_thumbnail_list(ROXAppInfo *ai)
{
  return rox_appinfo_get_mime_type_list(ai, "CanThumbnail");
}

/*
 * $Log$
 */
