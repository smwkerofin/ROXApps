/*
 * rox_debug.c - Access to standard debug output
 *
 * $Id: rox_debug.c,v 1.3 2005/08/21 13:06:38 stephen Exp $
 */

/**
 * @file rox_debug.c
 * @brief Standard debug output.
 *
 * Unless the pre-processer symbol @c DEBUG is defined as non-zero
 * before rox_debug.h is included then the functions here are defined to be
 * no-operations.
 *
 * @author Stephen Watson
 * @version $Id$
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#define DEBUG 1
#include "rox_debug.h"

static gchar *pname=NULL;
static int dlevel;

/**
 * Initialize the debug system.  This is normally called by rox_init() or
 * rox_init_with_domain().  The program name is forced to all upper case and
 * appended with "_DEBUG_LEVEL" to obtain a environment variable name, so
 * that "Clock" becomes "CLOCK_DEBUG_LEVEL".  If that variable exists it is
 * interpreted as an integer value and used as the debug level if it is greater
 * than zero.  Otherwise the debug level is set to zero.
 *
 * @param[in] progname program name
 */
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

/**
 * Format printf-like arugments and send the result to stderr (using g_logv())
 * if the level is less than or equal to the current debug level.
 *
 * @param[in] level the level of the message (0 will always be seen).
 * @param[in] fmt a printf()-like format string
 * @param[in] ... arguments to the format string
 */
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
 * $Log: rox_debug.c,v $
 * Revision 1.3  2005/08/21 13:06:38  stephen
 * Added doxygen comments
 *
 * Revision 1.2  2005/03/04 17:22:18  stephen
 * Use apsymbols.h if available to reduce problems re-using binary on different Linux distros
 *
 * Revision 1.1  2001/07/17 14:44:49  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
