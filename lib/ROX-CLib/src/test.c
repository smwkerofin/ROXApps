/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#define DEBUG 1
#include "rox-clib.h"
#include "rox_debug.h"
#include "rox_filer_action.h"

#define TEST_FILE "/home/stephen/tmp/tmp/rm.me"

int main(int argc, char *argv[])
{
  char *type;
  
  gtk_init(&argc, &argv);
  rox_debug_init("test");

  rox_soap_set_timeout(NULL, 5000);
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
  rox_filer_show("/home/stephen", "public_html");
  printf("error=%s\n", rox_filer_get_last_error());

  sleep(5);
  rox_filer_run("/home/stephen/text/apf-7a.5.3");
  printf("error=%s\n", rox_filer_get_last_error());
  rox_filer_panel("", ROXPS_TOP);
  printf("error=%s\n", rox_filer_get_last_error());

  rox_filer_open_dir("/home/stephen/tmp/tmp");
  printf("error=%s\n", rox_filer_get_last_error());
  rox_filer_copy("/home/stephen/tmp/guess", "/home/stephen/tmp/tmp",
		 "rm.me", ROX_FILER_DEFAULT);
  printf("error=%s\n", rox_filer_get_last_error());
  
  type=rox_filer_file_type(TEST_FILE);
  printf("type of %s is %s\n", TEST_FILE, type? type: "unknown");
  if(type)
    g_free(type);
  printf("error=%s\n", rox_filer_get_last_error());
  
  return 0;
}
