/*
 * uri.c - utilities for uri handling and launching
 *
 * $Id: uri.c,v 1.4 2006/10/06 16:29:38 stephen Exp $
 */

/**
 * @file uri.c
 * @brief Utilities for uri handling and launching.
 *
 * @author Stephen Watson
 * @version $Id: uri.c,v 1.4 2006/10/06 16:29:38 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <gtk/gtk.h>

#include "rox.h"
#include "uri.h"
#include "choices.h"
#include "basedir.h"
#define DEBUG 1
#include "rox_debug.h"


/* handle % escapes in DnD */
/**
 * Escape path for future use in URI by replacing problematic characters
 * with a %%xx escape sequence.
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

  g_return_val_if_fail(path!=NULL, NULL);
  
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
 * replaced with %%xx escapes.
 *
 * @param[in] path to encode
 * @return the converted URI, pass to g_free() when done.
 */
gchar *rox_encode_path_as_uri(const guchar *path)
{
  gchar *tpath;
  gchar *uri;
  const gchar *host=rox_hostname();

  g_return_val_if_fail(path!=NULL, NULL);
  
  tpath = rox_escape_uri_path(path);
  uri = g_strconcat("file://", host? host: "localhost", tpath, NULL);
  g_free(tpath);

  return uri;
}

/**
 * Convert a URI with %%xx escapes into one without
 *
 * @param[in] uri URI to be converted.
 * @return converted URI, pass to g_free() when done.
 */
gchar *rox_unescape_uri(const char *uri)
{
  const gchar *s;
  gchar *d;
  gchar *tmp;
	
  g_return_val_if_fail(uri!=NULL, NULL);
  
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
 *
 * @deprecated use rox_uri_launch_handler() instead.
 */
int rox_uri_launch(const char *uri)
{
  gchar *text_x_uri;

  gchar *cmd, *app;
  int s;
    
  ROX_CLIB_DEPRECATED("rox_uri_launch_handler");
  
  g_return_val_if_fail(uri!=NULL, -1);
  
  text_x_uri=rox_choices_load("text_x-uri", "MIME-types",
				     "rox.sourceforge.net");
  
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

/**
 * Return the handler for URI's of the named scheme (e.g. http, file, ftp,
 * etc.)  The handler for file is always rox, otherwise it obtained from
 *  the configuration directory rox.sourceforge.net/URI.  
 *
 * @param[in] scheme URI scheme to get handler for (do not include trailing ':'
 * character)
 *
 * @return Command to execute. The returned string may contain %%s in which
 * case it should be replaced
 * with the URI, otherwise append the URI (after a space).  Pass the string
 * to g_free() when done.  @c NULL is returned if no handler is defined.
 */
gchar *rox_uri_get_handler(const char *scheme)
{
  gchar *cmd;

  
  g_return_val_if_fail(scheme!=NULL, NULL);
  
  if(strcmp(scheme, "file")==0)
    return g_strdup("rox -U \"%s\"");

  cmd=rox_basedir_load_config_path("rox.sourceforge.net/URI", scheme);

  if(cmd && rox_is_appdir(cmd)) {
    gchar *tmp=g_build_filename(cmd, "AppRun", NULL);
    g_free(cmd);
    cmd=tmp;
  }

  return cmd;
}

/**
 * For a given URI pass it to the appropriate launcher.
 * rox_uri_get_handler() is used to look up the launcher command which is
 * executed.
 *
 * @param[in] uri the URI to launch
 * @param[in] block if @c FALSE then do not wait for command to complete,
 * otherwise block until so.
 * @param[in,out] err location to store error message when launching
 * fails, or @c NULL to discard it
 
 * @return When @a block is true the process id of the command otherwise
 * the exit status of the command, @c -1 if no launcher is defined or
 * @c -2 if an error occured (check @a err).  
 */
int rox_uri_launch_handler(const char *uri, gboolean block, GError **err)
{
  gchar *scheme, *handler, *cmd, *arg;
  const char *t;
  int result;
  gboolean start;
  gchar *args[]={"sh", "-c", NULL, NULL};

  g_return_val_if_fail(uri!=NULL, -1);

  for(t=uri; *t && *t!=':'; t++)
    ;
  if(*t!=':')
    return -1;

  scheme=g_strndup(uri, t-uri);
  handler=rox_uri_get_handler(scheme);
  g_free(scheme);

  if(!handler)
    return -1;

  arg=strstr(handler, "%s");
  if(arg) {
    gchar *tmp=g_strndup(handler, arg-handler);
    cmd=g_strconcat(tmp, uri, arg+2, NULL);
    g_free(tmp);

  } else {
    cmd=g_strconcat(handler, " ", uri, NULL);
  }
  g_free(handler);

  args[2]=cmd;
  if(block) {
    gint status;
    
    start=g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
		       NULL, NULL, &status, err);
    if(!start)
      result=-2;
    else
      result=status;
    
  } else {
    GPid pid;

    start=g_spawn_async(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
			&pid, err);
    if(!start)
      result=-2;
    else
      result=(int) pid;
  }

  g_free(cmd);

  return result;
}

/*
 * $Log: uri.c,v $
 * Revision 1.4  2006/10/06 16:29:38  stephen
 * Marked the old choices_*() functions as deprecated and changed those functions still using them.
 *
 * Revision 1.3  2005/12/07 09:53:37  stephen
 * Support firefox for launching uri
 *
 * Revision 1.2  2005/09/10 16:17:55  stephen
 * Added doxygen comments
 *
 * Revision 1.1  2004/10/02 13:10:28  stephen
 * Added uri.h and rox_uri_launch() (and moved some stuff from rox_path
 * there) to better handle launching URIs.  ROXInfoWin now uses it.
 *
 */
