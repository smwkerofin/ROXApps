/*
 * $Id: rox_soap_server.c,v 1.3 2002/07/31 17:17:30 stephen Exp $
 *
 * rox_soap_server.c - Provide ROX-Filer like SOAP server
 *
 * Mostly adapted from ROX-Filer/src/remote.c by Thomas Leonard
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>

#include <libxml/parser.h>

#define DEBUG 1
#include "rox.h"
#include "rox_soap.h"
#include "rox_soap_server.h"

struct rox_soap_server {
  const char *ns_url;
  GdkAtom atom;
  GdkAtom xsoap;
  GtkWidget *ipc_window;
  GHashTable *actions;
};

typedef struct action {
  rox_soap_server_action method;
  gchar **required_args;
  gchar **optional_args;
  gpointer udata;
} Action;

static gboolean client_event(GtkWidget *window,
				 GdkEventClient *event,
				 gpointer data);
static GdkAtom xsoap;

static gboolean done_init=FALSE;
#define check_init() do {if(!done_init)rox_soap_server_init();} while(0)

void rox_soap_server_init(void)
{
  xsoap=gdk_atom_intern("_XSOAP", FALSE);

  done_init=TRUE;
}

ROXSOAPServer *rox_soap_server_new(const char *program_name,
				   const char *ns_url)
{
  ROXSOAPServer *server;
  char *name;
  Window xwindow;

  check_init();

  server=g_new(ROXSOAPServer, 1);
  server->ns_url=g_strdup(ns_url);
  name=rox_soap_atom_name_for_program(program_name);
  server->atom=gdk_atom_intern(name, FALSE);
  server->xsoap=xsoap;
  server->actions=g_hash_table_new(g_str_hash, g_str_equal);

  server->ipc_window = gtk_invisible_new();
  gtk_widget_realize(server->ipc_window);
  xwindow = GDK_WINDOW_XWINDOW(server->ipc_window->window);
  gdk_property_change(server->ipc_window->window, server->atom,
		      gdk_x11_xatom_to_atom(XA_WINDOW), 32,
		      GDK_PROP_MODE_REPLACE,
		      (void *) &xwindow, 1);
  g_signal_connect(server->ipc_window, "client-event",
		     G_CALLBACK(client_event), server);
  gdk_property_change(GDK_ROOT_PARENT(), server->atom,
		      gdk_x11_xatom_to_atom(XA_WINDOW), 32,
		      GDK_PROP_MODE_REPLACE,
		      (void *) &xwindow, 1);
  
  return server;
}

void rox_soap_server_add_action(ROXSOAPServer *server,
				const char *action_name,
				const char *args,
				const char *optional_args,
				rox_soap_server_action action,
				gpointer udata)
{
  Action *act;

  act=g_new(Action, 1);
  act->method=action;
  act->udata=udata;
  act->required_args = args ? g_strsplit(args, ",", 0) : NULL;
  act->optional_args = optional_args ?
    g_strsplit(optional_args, ",", 0) : NULL;

  g_hash_table_insert(server->actions, g_strdup(action_name), act);
}

static void action_table_free(gpointer key, gpointer value, gpointer udata)
{
  Action *action=(Action *) value;
  
  g_free(key);

  g_strfreev(action->required_args);
  g_strfreev(action->optional_args);
  g_free(action);
}

void rox_soap_server_delete(ROXSOAPServer *server)
{
  g_free((gpointer) server->ns_url);
  gdk_property_delete(GDK_ROOT_PARENT(), server->atom);
  gdk_property_delete(server->ipc_window->window, server->atom);
  gtk_widget_destroy(server->ipc_window);
  g_hash_table_foreach(server->actions, action_table_free, NULL);
  g_hash_table_destroy(server->actions);
  g_free(server);
}

/* Return the (first) child of this node with the given name.
 * NULL if not found.
 */
xmlNode *get_subnode(xmlNode *node, const char *namespaceURI, const char *name)
{
  for (node = node->xmlChildrenNode; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE)
      continue;

    if (strcmp(node->name, name))
      continue;

    if (node->ns == NULL || namespaceURI == NULL) {
      if (node->ns == NULL && namespaceURI == NULL)
	return node;
      continue;
    }
		
    if (strcmp(node->ns->href, namespaceURI) == 0)
      return node;
  }

  return NULL;
}

/* Lookup this method and invoke it.
 * Returns the SOAP reply or fault, or NULL if this method
 * doesn't return anything.
 */
static xmlNodePtr soap_invoke(ROXSOAPServer *server, xmlNode *method)
{
  GList *args = NULL;
  Action *call;
  gchar **arg;
  xmlNodePtr retval = NULL;

  call = g_hash_table_lookup(server->actions, method->name);
  if (!call) {
    xmlNodePtr reply;
    gchar *err;

    err = g_strdup_printf(_("Attempt to invoke unknown SOAP "
			    "method '%s'"), method->name);
    reply = xmlNewNode(NULL, "env:Fault");
    xmlNewNs(reply, SOAP_NAMESPACE_URL, "rpc");
    xmlNewNs(reply, ENV_NAMESPACE_URL, "env");
    xmlNewChild(reply, NULL, "faultcode",
		"rpc:ProcedureNotPresent");
    xmlNewChild(reply, NULL, "faultstring", err);
    g_free(err);
    return reply;
  }

  if (call->required_args) {
    for (arg = call->required_args; *arg; arg++) {
      xmlNode *node;

      node = get_subnode(method, server->ns_url, *arg);
      if (!node)
	{
	  g_warning("Missing required argument '%s' "
		    "in call to method '%s'", *arg,
		    method->name);
	  goto out;
	}

      dprintf(3, "Append '%s' to argument list for %s", *arg, method->name);
      
      args = g_list_append(args, node);
    }
  }

  if (call->optional_args) {
    for (arg = call->optional_args; *arg; arg++) {
      xmlNode *node;
      
      node = get_subnode(method, server->ns_url, *arg);
      
      dprintf(3, "Append '%s' to argument list for %s", *arg, method->name);
      
      args = g_list_append(args, node);
    }
  }

  dprintf(2, "Calling method %s for %s", method->name, server->ns_url);
  retval = call->method(server, method->name, args, call->udata);
 out:
  g_list_free(args);

  return retval;
}

/* Executes the RPC call(s) in the given SOAP message and returns
 * the reply.
 */
xmlDocPtr run_soap(ROXSOAPServer *server, xmlDocPtr soap)
{
  xmlNodePtr body, node, rep_body, reply;
  xmlDocPtr rep_doc = NULL;

  g_return_val_if_fail(soap != NULL, NULL);

  node = xmlDocGetRootElement(soap);
  dprintf(3, "node->ns=%s", node->ns? node->ns: "NULL");
  if (!node->ns)
    goto bad_soap;

  dprintf(3, "node->ns->href=%s", node->ns->href);
  if (strcmp(node->ns->href, ENV_NAMESPACE_URL) != 0)
    goto bad_soap;

  body = get_subnode(node, ENV_NAMESPACE_URL, "Body");
  dprintf(3, "body=%p", body);
  if (!body)
    goto bad_soap;

  for (node = body->xmlChildrenNode; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE)
      continue;
    
    if (node->ns == NULL || strcmp(node->ns->href, server->ns_url) != 0) {
      g_warning("Unknown namespace %s (expected %s)",
		node->ns ? node->ns->href
		: (xmlChar *) "(none)", server->ns_url);
      continue;
    }
		
    reply = soap_invoke(server, node);

    if (reply) {
      if (!rep_doc) {
	rep_doc = rox_soap_build_xml("unused", server->ns_url, &rep_body);
	xmlUnlinkNode(rep_body);
	xmlFreeNode(rep_body);
	rep_body=xmlDocGetRootElement(rep_doc)->xmlChildrenNode;
      }
      xmlAddChild(rep_body, reply);
    }
  }

  goto out;

bad_soap:
  g_warning("Bad SOAP message received!");
  /*xmlDocDump(stderr, soap);*/

out:

  return rep_doc;
}

static char *read_property(GdkWindow *window, GdkAtom prop, gint *out_length)
{
  gint	grab_len = 4096;
  gint	length;
  guchar	*data;

  while (TRUE) {
    if (!(gdk_property_get(window, prop,
			   gdk_x11_xatom_to_atom(XA_STRING), 0, grab_len,
			   FALSE, NULL, NULL,
			   &length, &data) && data))
      return NULL;	/* Error? */

    if (length >= grab_len) {
      /* Didn't get all of it - try again */
      grab_len <<= 1;
      g_free(data);
      continue;
    }

    data = g_realloc(data, length + 1);
    data[length] = '\0';	/* libxml seems to need this */
    *out_length = length;

    return data;
  }
}

static gboolean client_event(GtkWidget *window,
				 GdkEventClient *event,
				 gpointer udata)
{
  ROXSOAPServer *server=(ROXSOAPServer *) udata;
  GdkWindow *src_window;
  GdkAtom prop;
  xmlDocPtr reply;
  guchar	*data;
  gint	length;
  xmlDocPtr doc;

  if (event->message_type != xsoap)
    return FALSE;

  dprintf(3, "event->data.l={%p, %p}", event->data.l[0], event->data.l[1]);
  src_window = gdk_window_foreign_new(event->data.l[0]);
  dprintf(3, "src_window=%p", src_window);
  g_return_val_if_fail(src_window != NULL, FALSE);
  prop = gdk_x11_xatom_to_atom(event->data.l[1]);
  dprintf(3, "prop=%p", prop);

  data = read_property(src_window, prop, &length);
  if (!data)
    return TRUE;

  doc = xmlParseMemory(g_strndup(data, length), length);
  g_free(data);

  reply = run_soap(server, doc);
  xmlFreeDoc(doc);

  if (reply) {
    /* Send reply back... */
    xmlChar *mem;
    int	size;

    xmlDocDumpMemory(reply, &mem, &size);
    g_return_val_if_fail(size > 0, TRUE);

    gdk_property_change(src_window, prop,
			gdk_x11_xatom_to_atom(XA_STRING), 8,
			GDK_PROP_MODE_REPLACE, mem, size);
    g_free(mem);
  } else
    gdk_property_delete(src_window, prop);

  return TRUE;
}

