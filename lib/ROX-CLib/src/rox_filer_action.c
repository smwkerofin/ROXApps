/*
 * $Id: rox_filer_action.c,v 1.13 2005/12/07 11:45:02 stephen Exp $
 *
 * rox_filer_action.c - drive the filer via SOAP
 */
/**
 * @file rox_filer_action.c
 * @brief  Drive the filer via the SOAP calls.
 *
 * Uses the filer's remote protocol to drive the filer.  See Appendix C in
 * ROX-Filer/Help/Manual.html
 *
 * @author Stephen Watson
 * @version $Id: rox_filer_action.c,v 1.13 2005/12/07 11:45:02 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

#include "rox.h"
#include "rox_soap.h"
#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#include "rox_filer_action.h"

static gboolean doneinit=FALSE;
static ROXSOAP *filer=NULL;

struct fa_data {
  gboolean status;
  gpointer udata;
};

#define check_init() do{if(!doneinit){rox_filer_action_init();}}while(0)

/**
 * Initialize the filer action system.  Calling this function is optional,
 * the other file action functions will call this automatically if they
 * detect that initialization has not been performed.
 */
void rox_filer_action_init(void)
{
  rox_soap_init();

  filer=rox_soap_connect_to_filer();
  
  doneinit=TRUE;
}

static void queue_error(int code, const char *mess)
{
  GError *err=g_error_new_literal(rox_error_quark,
				  code | ROX_FILER_ACTION_ERROR, mess);

  rox_error_queue(err);
}

static void make_soap(const char *action, xmlDocPtr *rpc, xmlNodePtr *act)
{
  *rpc=rox_soap_build_xml(action, ROX_NAMESPACE_URL, act);
}

static void expect_no_reply(ROXSOAP *filer, gboolean status, xmlDocPtr reply,
			    gpointer udata)
{
  struct fa_data *data=(struct fa_data *) udata;

  dprintf(3, "expect_no_reply(%d, %p, %p)", status, reply, udata);

  data->status=status;
  if(!status) {
    queue_error(7, rox_soap_get_last_error());
  }

  rox_main_quit();
}

static void send_soap(xmlDocPtr rpc, rox_soap_callback callback,
		      gpointer udata)
{
  static struct fa_data data;
  
  data.status=FALSE;
  data.udata=udata;

  rox_debug_printf(2, "calling rox_soap_send()"); 
  rox_soap_send(filer, rpc, TRUE, callback, &data);
  rox_debug_printf(3, "status=%d", data.status);

  rox_debug_printf(2, "Entering gtk_main() for rox_soap_send");
  rox_main_loop();
  dprintf(3, "status=%d", data.status);
  if(!data.status) {
    rox_soap_send_via_pipe(filer, rpc, callback, &data);
    rox_debug_printf(2, "Entering gtk_main() for rox_soap_send_via_pipe");
    rox_main_loop();
    rox_debug_printf(3, "status=%d", data.status);
  }
}

static void simple_call(const char *action, const char *argname,
			const char *arg)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  rox_debug_printf(2, "In simple_call %s %s %s", action,
		   argname? argname: "NULL", arg? arg: "NULL");

  check_init();

  make_soap(action, &rpc, &tree);
  (void) xmlNewChild(tree, NULL, argname, arg);

  send_soap(rpc, expect_no_reply, NULL);
  
  xmlFreeDoc(rpc);
}

/**
 * Call the OpenDir method
 *
 * @param[in] filename name of directory to open.
 */
void rox_filer_open_dir(const char *filename)
{
  simple_call("OpenDir", "Filename", filename);
}

/**
 * Call the CloseDir method
 *
 * @param[in] filename name of directory to close.
 */
void rox_filer_close_dir(const char *filename)
{
  simple_call("CloseDir", "Filename", filename);
}

/**
 * Call the Examine method
 *
 * @param[in] filename name of file to examine.
 */
void rox_filer_examine(const char *filename)
{
  simple_call("Examine", "Filename", filename);
}

static const char *panel_side(ROXPanelSide side)
{
  const char *sidename;
  gchar *mess;

  switch(side) {
  case ROXPS_TOP: sidename="Top"; break;
  case ROXPS_BOTTOM: sidename="Bottom"; break;
  case ROXPS_LEFT: sidename="Left"; break;
  case ROXPS_RIGHT: sidename="Right"; break;
  default:
    dprintf(0, "Illegal panel side: %d", side);
    mess=g_strdup_printf(_("Illegal panel side: %d"), side);
    queue_error(1, mess);
    g_free(mess);
    return NULL;
  }

  return sidename;
}

/**
 * Call the Panel method to set the panel on a side.
 *
 * @param[in] name name of the panel
 * @param[in] side side of the screen to place the panel
 */
void rox_filer_panel(const char *name, ROXPanelSide side)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  const char *sidename=panel_side(side);

  if(!sidename)
    return;
  
  check_init();

  make_soap("Panel", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Name", name);
  (void) xmlNewChild(tree, NULL, "Side", sidename);

  send_soap(rpc, expect_no_reply, NULL);
  
  xmlFreeDoc(rpc);
}

/**
 * Add an object to a panel.
 *
 * @param[in] side the panel to add to
 * @param[in] path path name of object to be added.
 * @param[in] after if @c FALSE the object is added to the end of the group of
 * icons at the start, otherwise is is added to the start of the icons at the
 * end.
 */
void rox_filer_panel_add(ROXPanelSide side, const char *path, int after)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  const char *sidename=panel_side(side);

  if(!sidename)
    return;
  
  check_init();

  make_soap("PanelAdd", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Side", sidename);
  (void) xmlNewChild(tree, NULL, "Path", path);
  if(after!=ROX_FILER_DEFAULT) {
    char tmp[32];
    sprintf(tmp, "%d", !!after);
    (void) xmlNewChild(tree, NULL, "After", tmp);
  } 

  send_soap(rpc, expect_no_reply, NULL);
  
  xmlFreeDoc(rpc);
}

/**
 * Set the name of the pinboard.
 *
 * @param[in] name name of the panel.
 */
void rox_filer_pinboard(const char *name)
{
  simple_call("Pinboard", "Name", name);
}
     
/**
 * Add an object to the pinboard.
 *
 * @param[in] path path name of object to be added.
 * @param[in] x x coordinate of position on pinboard
 * @param[in] y y coordinate of position on pinboard
 */
void rox_filer_pinboard_add(const char *path, int x, int y)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  char tmp[32];

  check_init();

  make_soap("PinboardAdd", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Path", path);
  sprintf(tmp, "%d", x);
  (void) xmlNewChild(tree, NULL, "X", tmp);
  sprintf(tmp, "%d", y);
  (void) xmlNewChild(tree, NULL, "Y", tmp);

  send_soap(rpc, expect_no_reply, NULL);
  
  xmlFreeDoc(rpc);
}

/**
 * Call the Run method to run a file.
 *
 * @param[in] filename name of object to run.
 */
void rox_filer_run(const char *filename)
{
  simple_call("Run", "Filename", filename);
}

/**
 * Show a directory viewer with the named file highlighted.
 *
 * @param[in] directory name of directory to show
 * @param[in] leafname name of file in directory to highlight.
 */
void rox_filer_show(const char *directory, const char *leafname)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;

  check_init();

  make_soap("Show", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Directory", directory);
  (void) xmlNewChild(tree, NULL, "Leafname", leafname);

  send_soap(rpc, expect_no_reply, NULL);
  
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

  send_soap(rpc, expect_no_reply, NULL);

  xmlFreeDoc(rpc);
}

/**
 * Copy a file.  If @a leafname is @c NULL then copy object @a from to
 * destination @a to, otherwise copy @a leafname in directory @a from to
 * directory @a to.
 *
 * @param[in] from object to copy or directory file is in
 * @param[in] to path to copy object to, or destination directory.
 * @param[in] leafname if not @c NULL the file in @a from to be copied
 * to directory @a to.
 * @param[in] quiet if @c TRUE then the operation is quiet (no confirmation
 * required), if @c FALSE then confirmation is needed.  If #ROX_FILER_DEFAULT
 * then the setting is taken from the filer's current options.
 */
void rox_filer_copy(const char *from, const char *to,
			   const char *leafname, int quiet)
{
  do_transfer_op("Copy", from, to, leafname, quiet);
}
  
/**
 * Move a file.  If @a leafname is @c NULL then move object @a from to
 * destination @a to, otherwise move @a leafname in directory @a from to
 * directory @a to.
 *
 * @param[in] from object to move or directory file is in
 * @param[in] to path to move object to, or destination directory.
 * @param[in] leafname if not @c NULL the file in @a from to be moved
 * to directory @a to.
 * @param[in] quiet if @c TRUE then the operation is quiet (no confirmation
 * required), if @c FALSE then confirmation is needed.  If #ROX_FILER_DEFAULT
 * then the setting is taken from the filer's current options.
 */
void rox_filer_move(const char *from, const char *to,
			   const char *leafname, int quiet)
{
  do_transfer_op("Copy", from, to, leafname, quiet);
}
  
/**
 * Create a symbolic link.  If @a leafname is @c NULL then link object @a from to
 * destination @a to, otherwise link @a leafname in directory @a from to
 * directory @a to.
 *
 * @param[in] from object to link or directory file is in
 * @param[in] to path to create link, or destination directory.
 * @param[in] leafname if not @c NULL the file in @a from to be linked
 * to directory @a to.
 */
void rox_filer_link(const char *from, const char *to,
			   const char *leafname)
{
  do_transfer_op("Copy", from, to, leafname, ROX_FILER_DEFAULT);
}

/**
 * Mount a known mountpoint (defined in /etc/fstab or /etc/vfstab).
 *
 * @param[in] mountpoint path to mount point defined in system file
 * @param[in] quiet if @c TRUE then the operation is quiet (no confirmation
 * required), if @c FALSE then confirmation is needed.  If #ROX_FILER_DEFAULT
 * then the setting is taken from the filer's current options.
 * @param[in] opendir if @c TRUE then a new filer window is opened for the
 * mounted directory.  If #ROX_FILER_DEFAULT
 * then the setting is taken from the filer's current options.
 */
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

  send_soap(rpc, expect_no_reply, NULL);

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
			node->ns? (char *) node->ns->href: "NULL",
			namespaceURI? namespaceURI: (const char *) "NULL");
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


static char *filer_reply_str=NULL;

static void string_reply(ROXSOAP *filer, gboolean status, xmlDocPtr reply,
			    gpointer udata)
{
  struct fa_data *data=(struct fa_data *) udata;
  static gchar *resp=NULL;
  gchar *mess;

  g_return_if_fail(data->udata!=NULL);
  
  if(resp)
    g_free(resp);
  resp=g_strconcat((gchar *) data->udata, "Response", NULL);
  
  dprintf(3, "string_reply(%d, %p, %p (%p: %s))", status, reply, udata,
	  data->udata, resp);

  data->status=status;

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
	    
	    mess=g_strdup_printf(_("Unknown namespace %s for %s"),
		      node->ns ? node->ns->href: (xmlChar *) "(none)",
		    node->name);
	    queue_error(2, mess);
	    g_free(mess);
	    continue;
	  }
	  if(strcmp(node->name, resp)!=0)
	    continue;

	  sub=get_subnode(node, SOAP_NAMESPACE_URL, "result");
	  if(sub) {
	    char *type;
	    type=xmlNodeGetContent(sub);
	    dprintf(3, "result is %s", type);
	    filer_reply_str=g_strdup(type);
	    g_free(type);
	    break;
	  } else {
	    mess=g_strdup_printf(_("No valid %s result node found"),
		    (gchar *) data->udata);
	    queue_error(3, mess);
	    g_free(mess);
	  }
	}
      } else {
	queue_error(4, _("No Body element found in SOAP reply"));
      }
    } else {
      mess=g_strdup_printf(_("Root element has wrong namespace (%s)"),
			   
	      root->ns ? root->ns->href: (xmlChar *) "(none)");
      queue_error(5, mess);
      g_free(mess);
    }
  } else if(status) {
    mess=g_strdup_printf(_("No reply from filer: %s"),
	    rox_soap_get_last_error());
    queue_error(6, mess);
    g_free(mess);
  }

  dprintf(2, "calling rox_main_quit() from string_reply");
  rox_main_quit();
  dprintf(2, "returning from string_reply");
}

/**
 * Determine the MIME type of a file by asking ROX-Filer.
 *
 * @param[in] file name of file to identify
 * @return MIME type of file, in form "media/sub-type" (pass to g_free() when
 * done), or @c NULL if failed
 */
char *rox_filer_file_type(const char *file)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  gchar *ans;

  check_init();

  make_soap("FileType", &rpc, &tree);
  (void) xmlNewChild(tree, NULL, "Filename", file);

  filer_reply_str=NULL;
  send_soap(rpc, string_reply, "FileType");

  xmlFreeDoc(rpc);

  ans=filer_reply_str? g_strdup(filer_reply_str): NULL;
  g_free(filer_reply_str);

  return ans;
}


/**
 * Get ROX-Filer's version string.
 *
 * @return version string (pass to g_free() when done), or @c NULL if failed
 */
char *rox_filer_version(void)
{
  xmlDocPtr rpc;
  xmlNodePtr tree;
  gchar *ans;

  check_init();

  make_soap("Version", &rpc, &tree);

  filer_reply_str=NULL;
  send_soap(rpc, string_reply, "Version");

  xmlFreeDoc(rpc);

  ans=filer_reply_str? g_strdup(filer_reply_str): NULL;
  g_free(filer_reply_str);

  return ans;
}

/**
 * Check for error in talking to ROX-Filer.  Errors are not normally
 * reported back to the caller, but are recorded.  Only the most recent error
 * is returned.
 *
 * @deprecated use rox_error_queue_empty() instead.
 *
 * @return non-zero if an error is recorded.
 */
int rox_filer_have_error(void)
{
  return !rox_error_queue_empty();
}

/**
 * @deprecated use rox_error_queue_fetch() instead.
 * @return text string containing last known error, do not free
 */
const char *rox_filer_get_last_error(void)
{
  GError *err=rox_error_queue_peek_last();
  if(err)
    return err->message;

  return _("No error");
}

/**
 * Clear any recorded error.
 * @deprecated use rox_error_queue_flush() instead.
 */
void rox_filer_clear_error(void)
{
  GError *err=rox_error_queue_fetch_last();
  g_error_free(err);
}

/*
 * $Log: rox_filer_action.c,v $
 * Revision 1.13  2005/12/07 11:45:02  stephen
 * Adding an error handling framework
 *
 * Revision 1.12  2005/09/10 16:16:14  stephen
 * Added doxygen comments
 *
 * Revision 1.11  2004/05/05 19:24:18  stephen
 * Extra debug (problem when target for SOAP doesn't exist)
 *
 * Revision 1.10  2003/03/25 14:31:34  stephen
 * New attempt at working SOAP code.
 *
 * Revision 1.9  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.8  2002/07/31 17:17:21  stephen
 * Use approved method of including libxml headers.
 *
 * Revision 1.7  2002/03/19 08:29:18  stephen
 * Added SOAP server (rox_soap_server.h).  SOAP client can connect to programs
 * other than ROX-Filer.
 *
 * Revision 1.6  2002/02/27 16:28:16  stephen
 * Add support for PanelAdd and PinboardAdd (assuming they get into the
 * filer)
 *
 * Revision 1.5  2002/02/21 11:56:48  stephen
 * Fix typo which causes rox_filer_pinboard() to fail.
 *
 * Revision 1.4  2002/02/13 11:00:38  stephen
 * Better way of accessing web site (by running a URI file).  Improvement to
 * language checking in rox_resources.c.  Visit by the const fairy in choices.h.
 * Updated pkg program to check for libxml2.  Added rox.c to access ROX-CLib's
 * version number.
 *
 * Revision 1.3  2002/01/07 15:39:01  stephen
 * Updated SOAP namespaces, added rox_filer_version() and improved
 * send_soap() to allow some methods to specify the expected reply.
 *
 * Revision 1.2  2001/12/07 11:25:01  stephen
 * More work on SOAP, mainly to get rox_filer_file_type() working.
 *
 * Revision 1.1  2001/12/05 16:46:33  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */
