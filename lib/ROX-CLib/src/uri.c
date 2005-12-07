/*
 * uri.c - utilities for uri handling and launching
 *
 * $Id: uri.c,v 1.2 2005/09/10 16:17:55 stephen Exp $
 */

/**
 * @file uri.c
 * @brief Utilities for uri handling and launching.
 *
 * @author Stephen Watson
 * @version $Id: uri.c,v 1.2 2005/09/10 16:17:55 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <glib.h>

#include "uri.h"
#include "choices.h"
#define DEBUG 1
#include "rox_debug.h"


/* handle % escapes in DnD */
/**
 * Escape path for future use in URI by replacing problematic characters
 * with a %xx escape sequence.
 *
 * @param[in] path path to be escaped
 * @return escaped path, pass to g_free() when done.
 */
gchar *rox_escape_uri_path(const char *path)
{
  const char *safe = "-_./"; /* Plus isalnum() */
  const guchar *s;
  gchar *ans;
  GString *str;

  str = g_string_sized_new(strlen(path));

  for (s = path; *s; s++) {
    if (!g_ascii_isalnum(*s) && !strchr(safe, *s))
      g_string_append_printf(str, "%%%02x", *s);
    else
      str = g_string_append_c(str, *s);
  }

  ans = str->str;
  g_string_free(str, FALSE);
  
  return ans;
}

/**
 * Return the canonical hostname for use in drag and drop.
 *
 * @return host name
 */
const gchar *rox_hostname(void)
{
  static char host[1025]={0};

#ifdef HAVE_GETHOSTNAME
  if(!host[0])
    gethostname(host, sizeof(host)-1);
#else
  rox_debug_printf(1, "No gethostname, can't determine our hostname.");
#endif

  return host;
}

/**
 * Convert a local path into a file: URI with problematic characters
 * replaced with %xx escapes.
 *
 * @param[in] path to encode
 * @return the converted URI, pass to g_free() when done.
 */
gchar *rox_encode_path_as_uri(const guchar *path)
{
  gchar *tpath = rox_escape_uri_path(path);
  gchar *uri;
  const gchar *host=rox_hostname();

  uri = g_strconcat("file://", host? host: "localhost", tpath, NULL);
  g_free(tpath);

  return uri;
}

/**
 * Convert a URI with %xx escapes into one without
 *
 * @param[in] uri URI to be converted.
 * @return converted URI, pass to g_free() when done.
 */
gchar *rox_unescape_uri(const char *uri)
{
  const gchar *s;
  gchar *d;
  gchar *tmp;
	
  tmp = g_strdup(uri);
  for (s = uri, d=tmp; *s; s++, d++) {
    /*printf("%s\n", s);*/
    if (*s=='%' && g_ascii_isxdigit(s[1]) &&
	g_ascii_isxdigit(s[2])) {
      int c;
      char buf[3];
      buf[0]=s[1];
      buf[1]=s[2];
      buf[2]=0;
      c=(int) strtol(buf, NULL, 16);
      *d=c;
      s+=2;
    } else {
      *d=*s;
    }
  }
  *d=0;

  return tmp;
}

/**
 * Launch a URI.  If there is a handler for the MIME type text/x-uri then
 * that is executed with '-' as a single argument and the URI pass in on
 * standard input.  If there is no such handler then each of the following
 * commands is tried in turn:
 * - gnome-moz-remote --newwin @a uri
 * - mozilla @a uri
 * - netscape @a uri
 *
 * @param[in] uri URI to launch
 * @return exit status of the last attempted command (0 for success).
 */
int rox_uri_launch(const char *uri)
{
  gchar *text_x_uri=choices_find_path_load("text_x-uri", "MIME-types");

  gchar *cmd, *app;
  int s;
    
  rox_debug_printf(2, "text_x-uri=%s", text_x_uri? text_x_uri: "NULL");
  if(text_x_uri) {
    FILE *p;

    app=g_strdup_printf("%s/%s", text_x_uri, "AppRun");
    if(access(app, X_OK)==0)
      cmd=g_strdup_printf("%s -", app);
    else
      cmd=g_strdup_printf("%s -", text_x_uri);
    
    p=popen(cmd, "w");
    fprintf(p, "%s\n", uri);
    s=pclose(p);
    rox_debug_printf(2, "pclose(%s %s)=%d", cmd, uri, s);
    g_free(text_x_uri);
    g_free(cmd);
    g_free(app);
    return s;
  }

  cmd=g_strdup_printf("gnome-moz-remote --newwin \"%s\"", uri);
  s=system(cmd);
  g_free(cmd);
  if(s) {
    cmd=g_strdup_printf("firefox %s", uri);
    s=system(cmd);
    g_free(cmd);
  }
  if(s) {
    cmd=g_strdup_printf("mozilla %s", uri);
    s=system(cmd);
    g_free(cmd);
  }
  if(s) {
    cmd=g_strdup_printf("netscape %s", uri);
    s=system(cmd);
    g_free(cmd);
  }

  return s;
}

/*
 * $Log: uri.c,v $
 * Revision 1.2  2005/09/10 16:17:55  stephen
 * Added doxygen comments
 *
 * Revision 1.1  2004/10/02 13:10:28  stephen
 * Added uri.h and rox_uri_launch() (and moved some stuff from rox_path
 * there) to better handle launching URIs.  ROXInfoWin now uses it.
 *
 */
