/*
 * $Id: test.c,v 1.5 2002/07/31 17:17:55 stephen Exp $
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
#include "rox_filer_action.h"

#define TEST_FILE "tmp/tmp/rm.me"

static void clock_open_callback(ROXSOAP *clock, gboolean status,
				xmlDocPtr reply, gpointer udata);

int main(int argc, char *argv[])
{
  char *type, *ver;
  char *home=g_getenv("HOME");
  char buf[256];
  ROXSOAP *prog;
  xmlDocPtr doc;
  xmlNodePtr act;
  
  gtk_init(&argc, &argv);
  rox_debug_init("test");
  rox_soap_set_timeout(NULL, 5000);

  printf("ROX-CLib version %s\n\n", rox_clib_version_string());

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
  printf("Examine(/tmp)\n");
  rox_filer_examine("/tmp");
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
  rox_soap_send(prog, doc, FALSE, clock_open_callback, NULL);
  printf("error=%s\n", rox_soap_get_last_error());
  xmlFreeDoc(doc);
  gtk_main();
  rox_soap_close(prog);
  
  return 0;
}

static void clock_open_callback(ROXSOAP *clock, gboolean status,
				xmlDocPtr reply, gpointer udata)
{
  printf("In clock_open_callback(%p, %d, %p, %p)\n", clock, status, reply,
	 udata);
  gtk_main_quit();
}
