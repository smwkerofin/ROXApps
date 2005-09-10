/*
 * rox_path.c - utilities for path handling, support for drag & drop
 *
 * $Id: rox_path.c,v 1.9 2004/10/23 11:50:45 stephen Exp $
 */
/**
 * @file rox_path.c
 * @brief Utilities for path handling, including support for drag & drop.
 *
 * @author Stephen Watson
 * @version $Id$
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

/**
 * Given a URI return a local path using rox_unescape_uri()
 * to convert the path.  file://thishost/path -> /path
 *
 * @param[in] uri uri to process
 * @return local path (pass to g_free() when done) or @c NULL if it is
 * not a local file path.
 */
char *rox_path_get_local(const char *uri)
{
  char *host;
  char *end;
  const char *orig=uri;
  
  if(strncmp(uri, "file:", 5)==0)
    uri+=5;
  
  if(uri[0]=='/') {
    if(uri[1]!='/')
      return rox_unescape_uri(uri);
    if(uri[2]=='/')
      return rox_unescape_uri(uri+2);

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

/**
 * Determine whether or not a host name identifies the local machine.
 * 
 * @param[in] hname host name to check.
 * @return non-zero if the host name belongs to the local machine
 */
int rox_hostname_is_local(const char *hname)
{
  const char *ahname=rox_hostname();
#ifdef HAVE_GETHOSTBYNAME
  struct hostent *hp;
#endif

  if(strcmp(hname, "localhost")==0)
    return TRUE;
  if(ahname && strcmp(ahname, hname)==0)
    return TRUE;
  
#ifdef HAVE_GETHOSTBYNAME
  if(ahname) {
    hp=gethostbyname(ahname);
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
    }

  }
#endif
    
  return FALSE;
}

/**
 * Given a file: URI, return the host part of the URI.
 * file://host/path -> host
 *
 * @param[in] uri uri to process
 * @return server name (pass to g_free() when done).
 */
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

/**
 * Given a file: URI, return the path part of the URI using rox_unescape_uri()
 * to convert the path.
 * file://host/path -> /path
 *
 * @param[in] uri uri to process
 * @return server name (pass to g_free() when done), or @c NULL if not a
 * valid file: URI.
 */
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
 * Revision 1.9  2004/10/23 11:50:45  stephen
 * More fixes for finding the hostname in drag and drop
 *
 * Revision 1.8  2004/10/02 13:10:22  stephen
 * Added uri.h and rox_uri_launch() (and moved some stuff from rox_path
 * there) to better handle launching URIs.  ROXInfoWin now uses it.
 *
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
