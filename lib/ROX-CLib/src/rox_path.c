/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.c,v 1.7 2004/09/11 14:09:27 stephen Exp $
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

char *rox_path_get_local(const char *uri)
{
  char *host;
  char *end;
  const char *orig=uri;
  
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

    if(strcmp(host, "localhost")==0) {
      g_free(host);
      return rox_unescape_uri(end);
    }

    if(rox_hostname_is_local(host)) {
      g_free(host);
      return rox_unescape_uri(end);
    }
    
    rox_debug_printf(1, "rox_path_get_local(%s) is %s local?",
		     orig, host);

    g_free(host);
  }

  return NULL;
}

int rox_hostname_is_local(const char *hname)
{
  const char *ahname=rox_hostname();
#ifdef HAVE_GETHOSTBYNAME
  struct hostent *hp;
  struct hostent *lhp;
#endif

  if(ahname && strcmp(ahname, hname)==0)
    return TRUE;
  
#ifdef HAVE_GETHOSTBYNAME
    hp=gethostbyname(hname);
    if(hp) {
      char **alias;
      if(strcmp(hp->h_name, hname)==0) {
	return TRUE;
      }
      for(alias=hp->h_aliases; *alias; alias++) {
	rox_debug_printf(3, "compare %s to %s", *alias, hname);
	if(strcmp(*alias, hname)==0) {
	  return TRUE;
	}
      }

      lhp=gethostbyname(hname);
      if(lhp) {
	if(strcmp(hp->h_name, lhp->h_name)==0) {
	  return TRUE;
	}
      }
    }
#endif
    
  return FALSE;
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
      return rox_unescape_uri(uri);
    if(uri[2]=='/')
      return rox_unescape_uri(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    
    return rox_unescape_uri(end);
  }

  return NULL;
}


/*
 * $Log: rox_path.c,v $
 * Revision 1.7  2004/09/11 14:09:27  stephen
 * Fix bug in drag and drop
 *
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
