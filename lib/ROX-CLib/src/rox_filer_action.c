/*
 * $Id: rox_filer_action.c,v 1.1 2001/12/05 16:46:33 stephen Exp $
 *
 * rox_filer_action.c - drive the filer via SOAP
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#ifdef HAVE_XML
#include <parser.h>
#endif

#include "rox_soap.h"
#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#include "rox_filer_action.h"

#define ENV_NAMESPACE_URL "http://www.w3.org/2001/06/soap-envelope"
#define SOAP_NAMESPACE_URL "http://www.w3.org/2001/09/soap-rpc"
#define ROX_NAMESPACE_URL "http://rox.sourceforge.net/SOAP/ROX-Filer"

static gboolean doneinit=FALSE;
static ROXSOAP *filer=NULL;
static char *last_error=NULL;
static char last_error_buffer[1024];

#define check_init() do{if(!doneinit){rox_filer_action_init();}}while(0)

void rox_filer_action_init(void)
{
  rox_soap_init();

  filer=rox_soap_connect_to_filer();
  
  doneinit=TRUE;
}

static void make_soap(const char *action, xmlDocPtr *rpc, xmlNodePtr *act)
{
  xmlDocPtr doc;
  xmlNodePtr tree;
  xmlNsPtr env_ns=NULL;
  xmlNsPtr rox_ns=NULL;

  doc=xmlNewDoc("1.0");
  
  doc->children=xmlNewDocNode(doc, env_ns, "Envelope", NULL);
  env_ns=xmlNewNs(doc->children, ENV_NAMESPACE_URL, "env");
  xmlSetNs(doc->children, env_ns);
  
  tree=xmlNewChild(doc->children, env_ns, "Body", NULL);
  rox_ns=xmlNewNs(tree, ROX_NAMESPACE_URL, NULL);
  *act=xmlNewChild(tree, rox_ns, action, NULL);

  *rpc=doc;
}

static void expect_no_reply(ROXSOAP *filer, gboolean status, xmlDocPtr reply,
			    gpointer udata)
{
  gboolean *stat=(gboolean *) udata;

  dprintf(3, "expect_no_reply(%d, %p, %p)", status, reply, udata);

  *stat=status;
  if(!status) {
    strcpy(last_error_buffer, rox_soap_get_last_error());
    last_error=last_error_buffer;
  }

  gtk_main_quit();
}

static void send_soap(xmlDocPtr rpc, rox_soap_callback callback)
{
  gboolean status=FALSE;
  
  rox_soap_send(filer, rpc, TRUE, callback, &status);
  dprintf(3, "status=%d", status);

  gtk_main();
  dprintf(3, "status=%d", status);
  if(!status) {
    rox_soap_send_via_pipe(filer, rpc, callback, &status);
    gtk_main();
    dprintf(3, "status=%d", status);
  }
}

static void simple_call(const char *action, const char *argname,
			const char *arg)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  check_init();

  make_soap(action, &rpc, &tree);
  (void) xmlNewChild(tree, NULL, argname, arg);

  send_soap(rpc, expect_no_reply);
  
  xmlFreeDoc(rpc);
}

void rox_filer_open_dir(const char *filename)
{
  simple_call("OpenDir", "Filename", filename);
}

void rox_filer_close_dir(const char *filename)
{
  simple_call("CloseDir", "Filename", filename);
}

void rox_filer_examine(const char *filename)
{
  simple_call("Examine", "Filename", filename);
}

void rox_filer_panel(const char *name, ROXPanelSide side)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  const char *sidename;

  switch(side) {
  case ROXPS_TOP: sidename="Top"; break;
  case ROXPS_BOTTOM: sidename="Bottom"; break;
  case ROXPS_LEFT: sidename="Left"; break;
  case ROXPS_RIGHT: sidename="Right"; break;
  default:
    dprintf(0, "Illegal panel side: %d", side);
    sprintf(last_error_buffer, "rox_filer_panel: Illegal panel side: %d",
	    side);
    last_error=last_error_buffer;
    return;
  }

  check_init();

  make_soap("Panel", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Name", name);
  (void) xmlNewChild(tree, NULL, "Side", sidename);

  send_soap(rpc, expect_no_reply);
  
  xmlFreeDoc(rpc);
}

void rox_filer_pinboard(const char *name)
{
  simple_call("Pinboard", "Nmae", name);
}
     
void rox_filer_run(const char *filename)
{
  simple_call("Run", "Filename", filename);
}

void rox_filer_show(const char *directory, const char *leafname)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  check_init();

  make_soap("Show", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Directory", directory);
  (void) xmlNewChild(tree, NULL, "Leafname", leafname);

  send_soap(rpc, expect_no_reply);
  
  xmlFreeDoc(rpc);
}

static void do_transfer_op(const char *action, const char *from,
			   const char *to,
			   const char *leafname, int quiet)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  check_init();

  make_soap(action, &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "From", from);
  (void) xmlNewChild(tree, NULL, "To", to);
  if(leafname)
    (void) xmlNewChild(tree, NULL, "Leafname", leafname);
  if(quiet!=ROX_FILER_DEFAULT) {
    const char *arg=quiet? "yes": "no";

    (void) xmlNewChild(tree, NULL, "Quiet", arg);
  }

  send_soap(rpc, expect_no_reply);

  xmlFreeDoc(rpc);
}

void rox_filer_copy(const char *from, const char *to,
			   const char *leafname, int quiet)
{
  do_transfer_op("Copy", from, to, leafname, quiet);
}
  
void rox_filer_move(const char *from, const char *to,
			   const char *leafname, int quiet)
{
  do_transfer_op("Copy", from, to, leafname, quiet);
}
  
void rox_filer_link(const char *from, const char *to,
			   const char *leafname)
{
  do_transfer_op("Copy", from, to, leafname, ROX_FILER_DEFAULT);
}


void rox_filer_mount(const char *mountpoint, int quiet, int opendir)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  check_init();

  make_soap("Mount", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "MountPoint", mountpoint);
  if(quiet!=ROX_FILER_DEFAULT) {
    const char *arg=quiet? "yes": "no";

    (void) xmlNewChild(tree, NULL, "Quiet", arg);
  }
  if(opendir!=ROX_FILER_DEFAULT) {
    const char *arg=opendir? "yes": "no";

    (void) xmlNewChild(tree, NULL, "OpenDir", arg);
  }

  send_soap(rpc, expect_no_reply);

  xmlFreeDoc(rpc);
}

static xmlNode *get_subnode(xmlNode *node, const char *namespaceURI,
			    const char *name)
{
	for (node = node->xmlChildrenNode; node; node = node->next)
	{
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp(node->name, name))
			continue;

		dprintf(3, "node->ns=%s namespaceURI=%s",
			node->ns? node->ns->href: "NULL",
			namespaceURI? namespaceURI: "NULL");
		if (node->ns == NULL || namespaceURI == NULL)
		{
			if (node->ns == NULL && namespaceURI == NULL)
				return node;
			continue;
		}
		
		if (strcmp(node->ns->href, namespaceURI) == 0)
			return node;
	}

	return NULL;
}


static char *file_type_reply_str;

static void file_type_reply(ROXSOAP *filer, gboolean status, xmlDocPtr reply,
			    gpointer udata)
{
  gboolean *stat=(gboolean *) udata;

  dprintf(3, "file_type_reply(%d, %p, %p)", status, reply, udata);

  *stat=status;

  if(status && reply) {
    xmlNodePtr root;
    
    root = xmlDocGetRootElement(reply);
    if(root && (root->ns && strcmp(root->ns->href, ENV_NAMESPACE_URL)==0)) {
      xmlNodePtr node, body, sub;
      
      body = get_subnode(root, ENV_NAMESPACE_URL, "Body");
      if (body) {
	for(node = body->xmlChildrenNode; node; node = node->next) {
	  if (node->type != XML_ELEMENT_NODE)
	    continue;

	  if(node->ns && strcmp(node->ns->href, ROX_NAMESPACE_URL)!=0) {
	    /*g_warning("Unknown namespace %s",
		      node->ns ? node->ns->href: (xmlChar *) "(none)");*/
	    sprintf(last_error_buffer, "Unknown namespace %s for %s",
		      node->ns ? node->ns->href: (xmlChar *) "(none)",
		    node->name);
	    last_error=last_error_buffer;
	    continue;
	  }
	  if(strcmp(node->name, "FileTypeResponse")!=0)
	    continue;

	  sub=get_subnode(node, SOAP_NAMESPACE_URL, "result");
	  if(sub) {
	    char *type;
	    type=xmlNodeGetContent(sub);
	    file_type_reply_str=g_strdup(type);
	    g_free(type);
	    break;
	  } else {
	    last_error="No valid MIMEType node found";
	  }
	}
      } else {
	sprintf(last_error_buffer, "No Body element found in SOAP reply");
	last_error=last_error_buffer;
      }
    } else {
      sprintf(last_error_buffer, "Root element has wrong namespace (%s)",
	      root->ns ? root->ns->href: (xmlChar *) "(none)");
      last_error=last_error_buffer;
    }
  } else if(status) {
    sprintf(last_error_buffer, "No reply from filer: %s",
	    rox_soap_get_last_error());
    last_error=last_error_buffer;
  }

  gtk_main_quit();
}

char *rox_filer_file_type(const char *file)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  gchar *ans;

  check_init();

  make_soap("FileType", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Filename", file);

  file_type_reply_str=NULL;
  send_soap(rpc, file_type_reply);

  xmlFreeDoc(rpc);

  ans=file_type_reply_str? g_strdup(file_type_reply_str): NULL;
  g_free(file_type_reply_str);

  return ans;
}

const char *rox_filer_get_last_error(void)
{
  if(last_error)
    return last_error;

  return "No error";
}

void rox_filer_clear_error(void)
{
  last_error=NULL;
}

/*
 * $Log: rox_filer_action.c,v $
 * Revision 1.1  2001/12/05 16:46:33  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */
