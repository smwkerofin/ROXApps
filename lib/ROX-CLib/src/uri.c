/*
 * uri.c - utilities for uri handling and launching
 *
 * $Id$
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
/* Escape path for future use in URI */
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

gchar *rox_encode_path_as_uri(const guchar *path)
{
  gchar *tpath = rox_escape_uri_path(path);
  gchar *uri;
  const gchar *host=rox_hostname();

  uri = g_strconcat("file://", host? host: "localhost", tpath, NULL);
  g_free(tpath);

  return uri;
}

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
 * $Log$
 */
