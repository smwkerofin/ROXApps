/*
 * $Id$
 *
 * rox_pinboard: send a message to a running rox to change the pinboard
 *
 * With no arguments turn pinboard off, otherwise a single argument to
 * use as the pinboard file.
 * Equivalent to
 *   rox -p arg
 * but with less overhead
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#define DEBUG 1
#include "rox-clib.h"
#include "rox_debug.h"
#include "rox_filer_action.h"
#include "rox_soap.h"

static void usage(const char *argv0)
{
  fprintf(stderr, "%s: invalid arguments\n", argv0);
  fprintf(stderr, "Usage: %s [pinboard-file]\n", argv0);
  exit(1);
}

int main(int argc, char *argv[])
{
  if(argc>2)
    usage(argv[0]);

  gtk_init(&argc, &argv);
  rox_debug_init("rox_pinboard");
  rox_soap_set_timeout(NULL, 5000);

  rox_filer_pinboard(argv[1]);
  if(rox_filer_have_error()) {
    fprintf(stderr, "%s: cannot set pinboard \"%s\": %s", argv[0],
	    argv[1]? argv[1]: "", rox_filer_get_last_error());
    exit(2);
  }

  return 0;
}
