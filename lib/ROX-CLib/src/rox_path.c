/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.c,v 1.6 2003/12/13 19:26:05 stephen Exp $
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_GETHOSTNAME
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <glib.h>

#include "rox_path.h"
#define DEBUG 1
#include "rox_debug.h"

static char hostn[256]={0};

char *rox_path_get_local(const char *uri)
{
  char *host;
  char *end;
  const char *orig=uri;
#ifdef HAVE_GETHOSTNAME
  struct hostent *hp;
#endif
  
  rox_debug_printf(3, "rox_path_get_local(%s) %s", uri, hostn);
    
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return unescape_uri(uri);
    if(uri[2]=='/')
      return unescape_uri(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    host=g_strndup(host, end-host);

    rox_debug_printf(3, "rox_path_get_local(%s) %s", orig, host);
    
    if(strcmp(host, "localhost")==0) {
      g_free(host);
      return unescape_uri(end);
    }

#ifdef HAVE_GETHOSTNAME
    if(!hostn[0]) {
      if(gethostname(hostn, sizeof(hostn))!=0) {
	rox_debug_printf(2, "rox_path_get_local(%s) couldn't get hostname!",
			 orig);
	g_free(host);
	return NULL;
      }
    }
#endif
    
    rox_debug_printf(3, "rox_path_get_local(%s) %s hostname is %s", orig,
		     host, hostn);

    if(strcmp(host, hostn)==0) {
      g_free(host);
      return unescape_uri(end);
    }

#ifdef HAVE_GETHOSTBYNAME
    hp=gethostbyname(host);
    if(hp) {
      char **alias;
      
	rox_debug_printf(3, "rox_path_get_local(%s) %s byname %s", orig,
		     host, hp->h_name);
      if(strcmp(hp->h_name, host)==0) {
	g_free(host);
	return unescape_uri(end);
      }
      for(alias=hp->h_aliases; *alias; alias++) {
	rox_debug_printf(3, "rox_path_get_local(%s) %s alias %s", orig,
		     host, *alias);
	if(strcmp(*alias, host)==0) {
	  g_free(host);
	  return unescape_uri(end);
	}
      }

    }
#endif
    
    rox_debug_printf(1, "rox_path_get_local(%s) is %s local? (%s)",
		     orig, host, hostn);

    g_free(host);
  }

  return NULL;
}

char *rox_path_get_server(const char *uri)
{
  char *host, *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup("localhost");
    
    host=(char *) uri+2;
    end=strchr(host, '/');
    return g_strndup(host, end-host);
  }

  return g_strdup("localhost");
}

char *rox_path_get_path(const char *uri)
{
  char *host;
  char *end;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return unescape_uri(uri);
    if(uri[2]=='/')
      return unescape_uri(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    
    return unescape_uri(end);
  }

  return NULL;
}


/* handle % escapes in DnD */
/* Escape path for future use in URI */
gchar *escape_uri_path(const char *path)
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

static const gchar *our_host_name(void)
{
  static char host[1025]={0};

  if(!host[0])
    gethostname(host, sizeof(host));

  return host;
}

gchar *encode_path_as_uri(const guchar *path)
{
  gchar *tpath = escape_uri_path(path);
  gchar *uri;

  uri = g_strconcat("file://", our_host_name(), tpath, NULL);
  g_free(tpath);

  return uri;
}

gchar *unescape_uri(const char *uri)
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


/*
 * $Log: rox_path.c,v $
 * Revision 1.6  2003/12/13 19:26:05  stephen
 * Exposed functions to escape and unescape uri's.
 * rox_path_get_local() and rox_path_get_path() now unescape uri's.
 *
 * Revision 1.5  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.4  2001/12/05 16:45:02  stephen
 * More tests to see if a URI is a local path
 *
 * Revision 1.3  2001/08/29 13:35:01  stephen
 * Added extra test for local host name using gethostbyname
 *
 * Revision 1.2  2001/08/20 15:27:46  stephen
 * Improved matching of hostname
 *
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
