/*
 * $Id$
 *
 * rox.c - General stuff
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <glib.h>

#include "rox.h"

int rox_clib_version_number(void)
{
  int ver=0;
  gchar **words;

  words=g_strsplit(ROXCLIB_VERSION, " ", 2);
  if(words) {
    gchar **els;

    els=g_strsplit(words[0], ".", 3);
    if(els) {

      ver=atoi(els[2]) + 100*atoi(els[1]) + 10000*atoi(els[0]);

      g_strfreev(els);
    }
    g_strfreev(words);
  }

  return ver;
}

const char *rox_clib_version_string(void)
{
  return ROXCLIB_VERSION;
}

/*
 * $Log$
 */
