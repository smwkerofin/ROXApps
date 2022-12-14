/*
 * $Id: test.c,v 1.13 2006/10/03 20:45:58 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
#include "xattr.h"
#include "uri.h"

#define TEST_FILE "tmp/tmp/rm.me"

static void clock_open_callback(ROXSOAP *clock, gboolean status,
				xmlDocPtr reply, gpointer udata);

static void test_soap(const char *home);
static void test_basedir(const char *home);
static void test_mime(const char *home);
static void test_appinfo(const char *home);
static void test_xattr(const char *home);
static void test_uri(const char *home);

struct tests {
  char *name;
  void (*func)(const char *);
};

static struct tests tests[]={
  {"soap", test_soap},
  {"basedir", test_basedir},
  {"mime", test_mime},
  {"appinfo", test_appinfo},
  {"xattr", test_xattr},
  {"uri", test_uri},

  {NULL, NULL}
};

#define TESTS "soap,basedir,mime,appinfo,xattr,uri"

int main(int argc, char *argv[])
{
  char *home=g_getenv("HOME");
  char *to_run=g_strdup(TESTS);
  char *running;
  
  rox_init("test", &argc, &argv);
  
  printf("ROX-CLib version %s\n\n", rox_clib_version_string());

  if(argv[1] && strcmp(argv[1], "all")!=0)
    to_run=argv[1];

  for(running=strtok(to_run, ","); running; running=strtok(NULL, ",")) {
    int i;

    for(i=0; tests[i].name; i++)
      if(strcmp(tests[i].name, running)==0) {
	printf("Run test %s\n", running);
	tests[i].func(home);
	break;
      }
    if(!tests[i].name)
      printf("Test \"%s\" not known\n", running);
  }

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
  if(!ver) {
    printf("not responding, tests cancelled\n");
    return;
  }
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

static void test_xattr(const char *home)
{
  gchar *path;
  gchar *value;
  GList *names;
  ROXMIMEType *type;

  printf("Extended attributes\n");

  rox_xattr_init();

  if(!rox_xattr_supported(NULL)) {
    printf("No xattr support\n");
    return;
  }
  
  path=g_build_filename(home, "tmp", "thing", NULL);

  errno=0;
  printf("supported(%s)=%d\n", path, rox_xattr_supported(path));
  if(errno)
    perror(path);

  errno=0;
  printf("have(%s)=%d\n", path, rox_xattr_have(path));
  if(errno)
    perror(path);

  errno=0;
  value=rox_xattr_get(path, ROX_XATTR_MIME_TYPE, NULL);
  printf("get(%s, %s)=%s\n", path, ROX_XATTR_MIME_TYPE,
	 value? value: "NULL");
  if(value)
    g_free(value);
  if(errno)
    perror(path);

  errno=0;
  names=rox_xattr_list(path);
  if(names) {
    GList *l;
    printf("Attrs on %s:\n", path);
    for(l=names; l; l=g_list_next(l))
      printf("\t%s\n", ((char *) l->data));
    rox_basedir_free_paths(names);
  } else
    printf("No attrs on %s\n", path);
  if(errno)
    perror(path);

  printf("name_valid(%s)=%d\n", "test", rox_xattr_name_valid("test"));
  printf("name_valid(%s)=%d\n", "user.test",
	 rox_xattr_name_valid("user.test"));

  printf("binary_value_supported=%d\n", rox_xattr_binary_value_supported());

  errno=0;
  printf("set(%s, %s, %s)=%d\n", path, "user.test", "this is a test",
	 rox_xattr_set(path, "user.test", "this is a test", -1));
  if(errno)
    perror(path);

  errno=0;
  names=rox_xattr_list(path);
  if(names) {
    GList *l;
    printf("Attrs on %s:\n", path);
    for(l=names; l; l=g_list_next(l))
      printf("\t%s\n", ((char *) l->data));
    rox_basedir_free_paths(names);
  } else
    printf("No attrs on %s\n", path);
  if(errno)
    perror(path);

  errno=0;
  value=rox_xattr_get(path, "user.test", NULL);
  printf("get(%s, %s)=%s\n", path, "user.test",
	 value? value: "NULL");
  if(value)
    g_free(value);
  if(errno)
    perror(path);

  errno=0;
  printf("delete(%s, %s)=%d\n", path, "user.test",
	 rox_xattr_delete(path, "user.test"));
  if(errno)
    perror(path);

  errno=0;
  names=rox_xattr_list(path);
  if(names) {
    GList *l;
    printf("Attrs on %s:\n", path);
    for(l=names; l; l=g_list_next(l))
      printf("\t%s\n", ((char *) l->data));
    rox_basedir_free_paths(names);
  } else
    printf("No attrs on %s\n", path);
  if(errno)
    perror(path);

  errno=0;
  type=rox_xattr_type_get(path);
  printf("type_get(%s)=%s\n", path, type? rox_mime_type_comment(type): "NULL");
  if(errno)
    perror(path);

  errno=0;
  type=rox_xattr_type_get("/tmp");
  printf("type_get(%s)=%s\n", "/tmp",
	 type? rox_mime_type_comment(type): "NULL");
  if(errno)
    perror("rox_xattr_type_get");

  errno=0;
  type=rox_xattr_type_get("/wibble");
  printf("type_get(%s)=%s\n", "/wibble",
	 type? rox_mime_type_comment(type): "NULL");
  if(errno)
    perror("rox_xattr_type_get");

  g_free(path);
}

static void test_uri(const char *home)
{
  static const char *opath="/tmp/this does not exist!";
  gchar *uri1, *path2, *path3;
  static const char *schemes[]={"file", "http", "https", "mailto",
				"unknown", NULL};
  int i;
  static const char *uris[]={"http://rox.sf.net/", "unknown:dummy", NULL};

  printf("Name to use for drag and drop: %s\n", rox_hostname());

  printf("\nStart with path: %s\n", opath);
  path2=rox_escape_uri_path(opath);
  printf("rox_escape_uri_path: %s\n", path2);
  uri1=rox_encode_path_as_uri(opath);
  printf("rox_encode_path_as_uri: %s\n", uri1);
  path3=rox_unescape_uri(path2);
  printf("rox_unescape_uri: %s\n", path3);
  g_free(path2);
  g_free(uri1);
  g_free(path3);

  printf("\nScheme handlers:\n");
  for(i=0; schemes[i]; i++) {
    gchar *handler=rox_uri_get_handler(schemes[i]);

    printf("\t%s = %s\n", schemes[i], handler? handler: "(not defined)");
    if(handler)
      g_free(handler);
  }

  printf("\nLaunching:\n");
  for(i=0; uris[i]; i++) {
    int res;
    GError *err;

    printf("\t%s (non-blocking)", uris[i]);
    err=NULL;
    res=rox_uri_launch_handler(uris[i], FALSE, &err);
    if(res>=0) 
      printf(" success, PID %d\n", res);
    else if(res==-1)
      printf(" failed, no handler\n");
    else
      printf(" failed: %s\n", err->message);
    if(err)
      g_error_free(err);

    printf("\t%s (blocking)", uris[i]);
    err=NULL;
    res=rox_uri_launch_handler(uris[i], TRUE, &err);
    if(res>=0) 
      printf(" success, exit code %d\n", res);
    else if(res==-1)
      printf(" failed, no handler\n");
    else
      printf(" failed: %s\n", err->message);
    if(err)
      g_error_free(err);
  }
}
