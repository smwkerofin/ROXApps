/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.c,v 1.3 2001/08/29 13:35:01 stephen Exp $
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
  char hostn[256];
  const char *orig=uri;
#ifdef HAVE_GETHOSTNAME
  struct hostent *hp;
  struct hostent *lhp;
#endif
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    host=g_strndup(host, end-host);

    if(strcmp(host, "localhost")==0) {
      g_free(host);
      return g_strdup(end);
    }

    if(gethostname(hostn, sizeof(hostn))!=0) {
      rox_debug_printf(2, "rox_path_get_local(%s) couldn't get hostname!",
		       orig);
      g_free(host);
      return NULL;
    }

    if(strcmp(host, hostn)==0) {
      g_free(host);
      return g_strdup(end);
    }

#ifdef HAVE_GETHOSTNAME
    hp=gethostbyname(host);
    if(hp) {
      char **alias;
      if(strcmp(hp->h_name, hostn)==0) {
	g_free(host);
	return g_strdup(end);
      }
      for(alias=hp->h_aliases; *alias; alias**)
	if(strcmp(*alias, hostn)==0) {
	  g_free(host);
	  return g_strdup(end);
	}

      lhp=gethostbyname(hostn);
      if(lhp) {
	if(strcmp(hp->h_name, lhp->h_name)==0) {
	  g_free(host);
	  return g_strdup(end);
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
      return g_strdup(uri);
    if(uri[2]=='/')
      return g_strdup(uri+2);

    host=(char *) uri+2;
    end=strchr(host, '/');
    
    return g_strdup(end);
  }

  return NULL;
}



/*
 * $Log: rox_path.c,v $
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
