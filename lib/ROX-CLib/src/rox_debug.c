/*
 * rox_debug.h - Access to standard debug output
 *
 * $Id$
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <glib.h>

#define DEBUG 1
#include "rox_debug.h"

static gchar *pname=NULL;
static int dlevel;

void rox_debug_init(const char *progname)
{
  gchar *env;
  gchar *val;

  if(pname)
    g_free(pname);

  if(progname)
    pname=g_strdup(progname);
  else
    pname=g_strdup("ROX");

  env=g_strconcat(pname, "_DEBUG_LEVEL", NULL);
  g_strup(env);
  val=g_getenv(env);
  if(val) {
    dlevel=atoi(val);
    if(dlevel<0)
      dlevel=0;
  } else {
    dlevel=0;
  }
  g_free(env);
}

void rox_debug_printf(int level, const char *fmt, ...)
{
  va_list list;

  if(!pname)
    rox_debug_init(NULL);

  if(level>dlevel)
    return;

  va_start(list, fmt);
  g_logv(pname, G_LOG_LEVEL_DEBUG, fmt, list);
  va_end(list);  
}

/*
 * $Log$
 */
