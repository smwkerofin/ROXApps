/*
 * Install handlers for MIME types
 *
 * $Id$
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib.h>
#include "mime_handler.h"

#define DEBUG 1
#include "rox.h"
#include "rox_debug.h"
#include "mime.h"
#include "appinfo.h"

#define PYTHON_CODE "import findrox;findrox.version(1,9,13);" \
                    "import rox,rox.mime_handler;" \
                    "rox.mime_handler.install_from_appinfo()"

static void install_from_appinfo_python(void)
{
  gchar *cmd;

  cmd=g_strdup_printf("python -c \"%s\" &", PYTHON_CODE);
  system(cmd);
  g_free(cmd);
}

void rox_mime_install_from_appinfo(void)
{
  install_from_appinfo_python();
}
