/*
 * $Id: test.c,v 1.1 2001/12/05 16:46:34 stephen Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#define DEBUG 1
#include "rox-clib.h"
#include "rox_debug.h"
#include "rox_filer_action.h"

#define TEST_FILE "tmp/tmp/rm.me"

int main(int argc, char *argv[])
{
  char *type;
  char *home=g_getenv("HOME");
  char buf[256];
  
  gtk_init(&argc, &argv);
  rox_debug_init("test");
  rox_soap_set_timeout(NULL, 5000);

#if 0
  rox_filer_open_dir("/tmp");
  printf("error=%s\n", rox_filer_get_last_error());
  sleep(5);
  rox_filer_examine("/tmp");
  printf("error=%s\n", rox_filer_get_last_error());
  sleep(5);
  rox_filer_close_dir("/tmp");
  printf("error=%s\n", rox_filer_get_last_error());

  rox_filer_panel("empty", ROXPS_TOP);
  printf("error=%s\n", rox_filer_get_last_error());
  rox_filer_show(home, "public_html");
  printf("error=%s\n", rox_filer_get_last_error());

  sleep(5);
  sprintf(buf, "%s/text/apf-7a.5.3", home);
  rox_filer_run(buf);
  printf("error=%s\n", rox_filer_get_last_error());
  rox_filer_panel("", ROXPS_TOP);
  printf("error=%s\n", rox_filer_get_last_error());

  sprintf(buf, "%s/tmp/tmp", home);
  rox_filer_open_dir(buf);
  printf("error=%s\n", rox_filer_get_last_error());
  sprintf(buf, "%s/tmp/guess", home);
  rox_filer_copy(buf, "/tmp",
		 "rm.me", ROX_FILER_DEFAULT);
  printf("error=%s\n", rox_filer_get_last_error());
#endif
  
  sprintf(buf, "%s/%s", home, TEST_FILE);
  type=rox_filer_file_type(buf);
  printf("type of %s is %s\n", buf, type? type: "unknown");
  if(type)
    g_free(type);
  printf("error=%s\n", rox_filer_get_last_error());
  
  return 0;
}
