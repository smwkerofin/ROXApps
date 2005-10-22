/*
 * $Id: test.c,v 1.11 2005/10/12 11:19:06 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxml/parser.h>

#define DEBUG 1
#include "rox.h"
#include "rox_soap.h"
#include "error.h"
#include "rox_filer_action.h"
#include "basedir.h"
#include "mime.h"
#include "appinfo.h"

#define TEST_FILE "tmp/tmp/rm.me"

static void clock_open_callback(ROXSOAP *clock, gboolean status,
				xmlDocPtr reply, gpointer udata);

static void test_soap(const char *home);
static void test_basedir(const char *home);
static void test_mime(const char *home);
static void test_appinfo(const char *home);

int main(int argc, char *argv[])
{
  char *home=g_getenv("HOME");
  
  rox_init("test", &argc, &argv);
  
  printf("ROX-CLib version %s\n\n", rox_clib_version_string());

  /*test_soap(home);*/
  test_basedir(home);
  test_mime(home);
  test_appinfo(home);

  /*rox_error("This is an error %d", 42);*/
  
  return 0;
}

static void test_soap(const char *home)
{
  char *type, *ver;
  char buf[256];
  ROXSOAP *prog;
  xmlDocPtr doc;
  xmlNodePtr act;
  gboolean state;
  
  rox_soap_set_timeout(NULL, 5000);

  printf("Version()=");
  ver=rox_filer_version();
  printf("%s\n", ver);
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);

  printf("OpenDir(/tmp)\n");
  rox_filer_open_dir("/tmp");
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);
  printf("Examine(/tmp/stephen)\n");
  rox_filer_examine("/tmp/stephen");
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);
  printf("CloseDir(/tmp)\n");
  rox_filer_close_dir("/tmp");
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);

#if 0
  printf("Panel(empty, Top)\n");
  rox_filer_panel("empty", ROXPS_TOP);
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Show(%s, %s)\n", home, "public_html");
  rox_filer_show(home, "public_html");
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);
  
  sprintf(buf, "%s/text/apf-7a.5.3", home);
  printf("Run(%s)\n", buf);
  rox_filer_run(buf);
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Panel(, Top)\n");
  rox_filer_panel("", ROXPS_TOP);
  printf("error=%s\n", rox_filer_get_last_error());
  printf("Waiting..\n");
  sleep(5);

  sprintf(buf, "%s/tmp/tmp", home);
  printf("OpenDir(%s)\n", buf);
  rox_filer_open_dir(buf);
  printf("error=%s\n", rox_filer_get_last_error());
  sprintf(buf, "%s/tmp/guess", home);
  printf("Copy(%s, %s, %s)\n", buf, "/tmp", "rm.me");
  rox_filer_copy(buf, "/tmp",
		 "rm.me", ROX_FILER_DEFAULT);
  printf("error=%s\n", rox_filer_get_last_error());
#endif
  
  sprintf(buf, "%s/%s", home, TEST_FILE);
  printf("FileType(%s)=", buf);
  type=rox_filer_file_type(buf);
  printf("%s\n", type? type: "unknown");
  if(type)
    g_free(type);
  printf("error=%s\n", rox_filer_get_last_error());

  printf("\nConnect to Clock... ");
  prog=rox_soap_connect("Clock");
  printf("%p\n", prog);
  printf("error=%s\n", rox_soap_get_last_error());
  printf("Build action %s\n", "Open");
  doc=rox_soap_build_xml("Open", "http://www.kerofin.demon.co.uk/rox/Clock",
			 &act);
  printf("error=%s\n", rox_soap_get_last_error());
  printf("Send action %s\n", "Open");
  state=rox_soap_send(prog, doc, FALSE, clock_open_callback, NULL);
  printf("state=%d, error=%s\n", state, rox_soap_get_last_error());
  xmlFreeDoc(doc);
  if(state)
    gtk_main();
  rox_soap_close(prog);
}

static void test_basedir(const char *home)
{
  gchar *path;
  GList *paths, *p;

  printf("test basedirs\n");
  
  printf("save_config_path %s %s => ", "ROX-CLib", "dummy");
  path=rox_basedir_save_config_path("ROX-CLib", "dummy");
  printf("%s\n", path? path: "NULL");
  if(path)
    g_free(path);

  printf("load_config_path %s %s => ", "ROX-CLib", "dummy");
  path=rox_basedir_load_config_path("ROX-CLib", "dummy");
  printf("%s\n", path? path: "NULL");
  if(path)
    g_free(path);

  printf("load_data_path %s %s => ", "mime", "globs");
  path=rox_basedir_load_data_path("mime", "globs");
  printf("%s\n", path? path: "NULL");
  if(path)
    g_free(path);

  printf("load_data_paths %s %s => \n", "mime", "globs");
  paths=rox_basedir_load_data_paths("mime", "globs");
  for(p=paths; p; p=g_list_next(p)) {
    printf("  %s\n", (char *) p->data);
    g_free(p->data);
  }
  g_list_free(paths);
}

static void test_mime_file(const char *path)
{
  ROXMIMEType *type=rox_mime_lookup(path);
  char *tname, *comm;

  printf(" %s -> ", path);
  if(!type) {
    printf("UNKNOWN\n");
    return;
  }
  tname=rox_mime_type_name(type);
  comm=rox_mime_type_comment(type);
  printf("%s (%s)\n", tname, comm? comm: "UNKNOWN");
  g_free(tname);
}

static void test_mime(const char *home)
{
  printf("test MIME system\n");
  
  /*mime_init();*/

  test_mime_file("/etc/passwd");
  test_mime_file("/dev/null");
  test_mime_file(home);
  test_mime_file("/bin/ls");
  test_mime_file("/usr/include/stdio.h");

  test_mime_file("Makefile");
  test_mime_file("Makefile.in");
}

static void test_appinfo_2(ROXAppInfo *ai, const char *lbl)
{
  gchar *l, *v;

  l=rox_appinfo_get_about_label(ai, lbl);
  v=rox_appinfo_get_about(ai, lbl);

  printf("%8s: %s=%s\n", lbl, l? l: "NULL", v? v: "NULL");

  g_free(l);
  g_free(v);
}

static void test_appinfo_1(ROXAppInfo *ai)
{
  const char *labs[]={"Purpose", "Version", "Authors", "Author",
		      "License", "Homepage", NULL};
  int i;
  const gchar *lang;
  const gchar *summary;

  lang=rox_appinfo_get_language(ai);
  printf("Language is \"%s\"\n", lang? lang: "NULL");
  summary=rox_appinfo_get_summary(ai);
  printf("Summary: %s\n", summary? summary: "NULL");
  for(i=0; labs[i]; i++)
    test_appinfo_2(ai, labs[i]);
}
  
static void test_appinfo(const char *home)
{
  GObject *obj;
  ROXAppInfo *ai;
  gchar *path;

  printf("Test AppInfo parser\n");

  obj=rox_appinfo_new();
  ai=ROX_APPINFO(obj);

  printf("obj=%p ai=%p ROX_IS_APPINFO(obj)=%d\n", obj, ai,
	 ROX_IS_APPINFO(obj));

  if(ai) {
    test_appinfo_1(ai);
    rox_appinfo_set_language(ai, "it"); 
    test_appinfo_1(ai);

    g_object_unref(obj);
  }

  path=g_strdup_printf("%s/Apps/VideoThumbnail/AppInfo.xml", home);
  obj=rox_appinfo_new_from_path(path);
  if(obj) {
    GList *list, *p;
    ROXMIMEType *type;
    
    ai=ROX_APPINFO(obj);
    test_appinfo_1(ai);
    list=rox_appinfo_get_can_thumbnail_list(ai);
    printf("Thumbnail:\n");
    for(p=list; p; p=g_list_next(p)) {
      char *tname, *comm;
      
      type=(ROXMIMEType *) p->data;
      tname=rox_mime_type_name(type);
      comm=rox_mime_type_comment(type);
      printf("  %s (%s)\n", tname, comm? comm: "UNKNOWN");
      g_free(tname);
    }
    g_object_unref(obj);
  }
}

static void clock_open_callback(ROXSOAP *clock, gboolean status,
				xmlDocPtr reply, gpointer udata)
{
  printf("In clock_open_callback(%p, %d, %p, %p)\n", clock, status, reply,
	 udata);
  gtk_main_quit();
}
