/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.c,v 1.1 2001/07/17 14:44:50 stephen Exp $
 */

#include <stdlib.h>
#include <string.h>

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

    if(gethostname(hostn, sizeof(hostn))==0 && strcmp(host, hostn)==0) {
      g_free(host);
      return g_strdup(end);
    }

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
 * Revision 1.1  2001/07/17 14:44:50  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
