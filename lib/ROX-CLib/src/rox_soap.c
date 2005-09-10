/*
 * $Id: rox_soap.c,v 1.14 2005/03/04 17:22:18 stephen Exp $
 *
 * rox_soap.c - interface to ROX-Filer using the SOAP protocol
 * (Yes, that's protocol twice on the line above.  Your problem?)
 *
 * Mostly adapted from ROX-Filer/src/remote.c by Thomas Leonard
 */
/**
 * @file rox_soap.c
 * @brief Interface to server programs using the SOAP protocol.
 *
 * SOAP allows you to communicate with server programs via the X server.
 * You may implement your applications to function from a single instance
 * no matter how many times they are started, much as ROX-Filer does itself.
 *
 * @author Thomas Leonard, Stephen Watson
 * @version $Id$
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

#include "rox_soap.h"
#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#define TIMEOUT 10000

/**
 * Definition of how we contact a program.
 */
typedef struct program { 
  const char *name;         /**< Name of program */
  const char *atom_format;  /**< Format of atom used for communication,
			     * see rox_soap_define_program() */
  const char *command;      /**< Command to execute if a running instance
			     * cannot be contacted. */
} Program;

/**
 * @internal connection to program
 */
struct rox_soap {
  Program *prog;
  GdkAtom atom;
  GdkAtom xsoap;
  GdkWindow *existing_ipc_window;
  guint timeout;
};

/**
 * @internal record of a single message via IPC
 */
typedef struct soap_data {
  ROXSOAP *filer;
  rox_soap_callback callback;
  gpointer data;
  GdkAtom prop;
  GtkWidget *widget;
  guint timeout_tag;
  gulong signal_tag;
  gint done_called;
} SoapData;

/**
 * @internal record of a single message via a pipe
 */
typedef struct soap_pipe_data {
  ROXSOAP *filer;
  rox_soap_callback callback;
  gpointer data;
  pid_t child;
  int fd;
  guint read_tag;
  GString *reply;
} SoapPipeData;

static Program rox_filer={
  "ROX-Filer", "_ROX_FILER_%e_%h", "rox -R"
};

static GdkAtom filer_atom;	/* _ROX_FILER_EUID_HOST */
static GdkAtom xsoap;		/* _XSOAP */

static GHashTable *programs=NULL;

static gboolean done_init=FALSE;
static guint timeout=TIMEOUT;
static gchar *host_name;

static char *last_error=NULL;
static char last_error_buffer[1024];

static GSList *dead_windows=NULL;

static GdkWindow *get_existing_ipc_window(GdkAtom);

#define rox_soap_return_if_fail(expr) \
do{if(!(expr)){sprintf(last_error_buffer,"assertion '%s' failed",#expr); \
last_error=last_error_buffer;return;}}while(0);

#define rox_soap_return_val_if_fail(expr, val) \
do{if(!(expr)){sprintf(last_error_buffer,"assertion '%s' failed",#expr); \
last_error=last_error_buffer;return val;}}while(0);

/**
 * Initialize the SOAP system.
 */
void rox_soap_init(void)
{
  gchar *id;
  char hostn[256];
  struct hostent *ent;

  if(done_init)
    return;

  gethostname(hostn, sizeof(hostn));
  ent=gethostbyname(hostn);
  host_name=g_strdup(ent? ent->h_name: hostn);
  id=g_strdup_printf("_ROX_FILER_%d_%s", (int) geteuid(), host_name);
  dprintf(3, "filer_atom=%s", id);
  filer_atom=gdk_atom_intern(id, FALSE);
  g_free(id);

  xsoap=gdk_atom_intern("_XSOAP", FALSE);

  programs=g_hash_table_new(g_str_hash, g_str_equal);
  g_hash_table_insert(programs, (gpointer) rox_filer.name, &rox_filer);

  done_init=TRUE;
}

/**
 * Open connection to ROX-Filer.
 *
 * @return connection to ROX-Filer.
 */
ROXSOAP *rox_soap_connect_to_filer(void)
{
  ROXSOAP *filer=g_new(ROXSOAP, 1);

  if(!done_init)
    rox_soap_init();

  filer->prog=&rox_filer;
  filer->atom=filer_atom;
  filer->xsoap=xsoap;
  filer->timeout=timeout;
  filer->existing_ipc_window=get_existing_ipc_window(filer->atom);

  return filer;
}

/**
 * Close connection to a SOAP server.
 */
void rox_soap_close(ROXSOAP *con)
{
  g_free(con);
}

static Program *find_program(const char *name)
{
  gpointer data=g_hash_table_lookup(programs, name);
  Program *nprog;
  gchar *fmt;
  int i, l;
  
  if(data)
    return (Program *) data;

  fmt=g_strdup(name);
  l=strlen(fmt);
  for(i=0; i<l; i++) {
    if(g_ascii_islower(fmt[i]))
      fmt[i]=g_ascii_toupper(fmt[i]);
    else if(!g_ascii_isalnum(fmt[i]))
      fmt[i]='_';
  }

  nprog=g_new(Program, 1);

  nprog->name=g_strdup(name);
  nprog->atom_format=g_strconcat("_", fmt, "_%e_%h", NULL);
  nprog->command=NULL;

  g_hash_table_insert(programs, (gpointer) name, nprog);

  return nprog;
}

/** 
 * Define how to connect to the named program. An entry for ROX-Filer is
 * pre-defined.
 *
 * @param[in] name name of program as passed to rox_soap_connect().
 * @param[in] atom_format format of atom name to use to connect, %e is
 * effective UID, %h is fully qualified domain name (FQDN) of display
 * @param[in] command command that may be fed the SOAP document on stdin if
 *            connecting via the atom fails, may be @c NULL
 */
void rox_soap_define_program(const char *name, const char *atom_format,
			     const char *command)
{
  Program *prog=g_hash_table_lookup(programs, name);

  if(prog) {
    if(prog!=&rox_filer) {
      g_free((gpointer) prog->atom_format);
      g_free((gpointer) prog->command);
    }
  } else {
    prog=g_new(Program, 1);
    prog->name=g_strdup(name);
  }
  prog->atom_format=g_strdup(atom_format);
  prog->command=command? g_strdup(command): command;
}

static char *make_atom_name(Program *prog)
{
  GString *atom;
  gchar *tmp;
  const gchar *start, *end;

  atom=g_string_new("");
  start=prog->atom_format;
  end=start;
  while(*end) {
    if(*end=='%') {
      tmp=g_strndup(start, end-start);
      g_string_append(atom, tmp);
      g_free(tmp);

      end++;
      switch(*end) {
      case 'e':
	g_string_sprintfa(atom, "%d", (int) geteuid());
	break;
      case 'h':
	g_string_append(atom, host_name);
	break;
      default:
	g_string_append_c(atom, *end);
      }
      start=++end;
    } else {
      end++;
    }
  }
  if(end>start)
    g_string_append(atom, start);

  tmp=atom->str;
  g_string_free(atom, FALSE);

  return tmp;
}

/**
 * Returns the name of the atom which will be used to locate the given program.
 * It uses the format given in the rox_soap_define_program() call, with the
 * defined substitutions.
 *
 * @param[in] name name of program
 * @return atom name to use (pass to g_free() when done) or @c NULL if
 * the program is not known.
 */
char *rox_soap_atom_name_for_program(const char *name)
{
  Program *prog=find_program(name);

  return prog? make_atom_name(prog): NULL;
}

/*
 * Open the connection to the named program
 *
 * @return connection to program.
 */
ROXSOAP *rox_soap_connect(const char *name)
{
  ROXSOAP *soap;
  Program *prog;
  gchar *atom;

  if(!done_init)
    rox_soap_init();
  
  prog=find_program(name);
  atom=make_atom_name(prog);

  soap=g_new(ROXSOAP, 1);

  soap->prog=prog;
  soap->atom=gdk_atom_intern(atom, FALSE);
  soap->xsoap=xsoap;
  soap->timeout=timeout;
  soap->existing_ipc_window=get_existing_ipc_window(soap->atom);

  dprintf(2, "Connect to %s via atom %s %p", name, atom,
	  soap->existing_ipc_window);

  g_free(atom);

  return soap;
}

/*
 * Check connection to the named program.
 *
 * @param[in] prog name of program to ping
 * @return @c TRUE if the program is contactable via SOAP, @c FALSE otherwise.
 */
gboolean rox_soap_ping(const char *prog)
{
  Program *program;
  gchar *atom_name;
  GdkAtom atom;

  if(!done_init)
    rox_soap_init();
  
  program=find_program(prog);
  atom_name=make_atom_name(program);
  atom=gdk_atom_intern(atom_name, FALSE);
  g_free(atom_name);

  return get_existing_ipc_window(atom)!=NULL;
}

/**
 * Build part of XML document to send.
 * The action to perform is in name space @a ns_url.  Add arguments to the node
 * @a act, then call rox_soap_send() with the return value.
 *
 * @param[in] action Action to perform, the name of the element in the
 * SOAP body.
 * @param[in] ns_url name space of the program
 * @param[out] act XML node, with name @a action, where arguments may
 * be added
 * @return XML document defining the SOAP message to send.
 */
xmlDocPtr rox_soap_build_xml(const char *action, const char *ns_url,
			     xmlNodePtr *act)
{
  xmlDocPtr doc;
  xmlNodePtr tree;
  xmlNsPtr env_ns=NULL;
  xmlNsPtr used_ns=NULL;

  doc=xmlNewDoc("1.0");
  if(!doc) {
    last_error="XML document creation failed";
    return;
  }
  
  doc->children=xmlNewDocNode(doc, env_ns, "Envelope", NULL);
  env_ns=xmlNewNs(doc->children, ENV_NAMESPACE_URL, "env");
  xmlSetNs(doc->children, env_ns);
  
  tree=xmlNewChild(doc->children, env_ns, "Body", NULL);
  used_ns=xmlNewNs(tree, ns_url, NULL);
  *act=xmlNewChild(tree, used_ns, action, NULL);

  return doc;
}

/* Returns the 'rox_atom' property of 'window' */
static gboolean get_ipc_property(GdkWindow *window, GdkAtom atom,
				 Window *r_xid)
{
	guchar		*data;
	gint		format, length;
	gboolean	retval = FALSE;
	
	if (gdk_property_get(window, atom,
			gdk_x11_xatom_to_atom(XA_WINDOW), 0, 4,
			FALSE, NULL, &format, &length, &data) && data)
	{
	  dprintf(3, "format=%d length=%d", format, length);
		if (format == 32 && length == 4)
		{
			retval = TRUE;
			*r_xid = *((Window *) data);
			dprintf(3, "xid=%p", *r_xid);
		}
		g_free(data);
	}

	return retval;
}

/* Get the remote IPC window of the already-running server if there
 * is one.
 */
static GdkWindow *get_existing_ipc_window(GdkAtom atom)
{
  Window		xid, xid_confirm;
  GdkWindow	*window;

  dprintf(3, "get_ipc_property %p", atom);
  if (!get_ipc_property(GDK_ROOT_PARENT(), atom, &xid))
    return NULL;

  dprintf(3, "gdk_window_lookup %p", xid);
  window=gdk_window_lookup(xid);

  if(!window) {  
    /*gdk_flush();*/
    dprintf(3, "gdk_window_foreign_new %p", xid);
    window = gdk_window_foreign_new(xid);
    if (!window)
      return NULL;
  }

  dprintf(3, "get_ipc_property %p %p %p", window, atom, xid);
  if (!get_ipc_property(window, atom, &xid_confirm) || xid_confirm!=xid)
    return NULL;
  
  return window;
}

static char *read_property(GdkWindow *window, GdkAtom prop, gint *out_length)
{
	gint	grab_len = 4096;
	gint	length;
	guchar	*data;

	while (1)
	{
		if (!(gdk_property_get(window, prop,
				gdk_x11_xatom_to_atom(XA_STRING), 0, grab_len,
				FALSE, NULL, NULL,
				&length, &data) && data))
			return NULL;	/* Error? */

		if (length >= grab_len)
		{
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

static void soap_done(GtkWidget *widget, GdkEventProperty *event,
		      gpointer data)
{
  SoapData *sdata=(SoapData *) data;
  xmlDocPtr reply=NULL;

  dprintf(3, "soap_done(%p, %p, %p)", widget, event, data);

  dprintf(2, "soap_done %p vs %p", sdata->prop, event->atom);

  if(sdata->prop != event->atom)
    return;

  sdata->done_called++;
  dprintf(3, "soap_done, done_called %d times", sdata->done_called);
  if(sdata->done_called<2)
    return;

  dprintf(3, "soap_done, remove %u", sdata->timeout_tag);
  g_source_remove(sdata->timeout_tag);
  /* Clear the timeout tag, serves as a flag to indicate we were succesful */
  sdata->timeout_tag=0;

  dprintf(3, "%d==%d?", event->state, GDK_PROPERTY_NEW_VALUE);
  if(event->state==GDK_PROPERTY_NEW_VALUE) {
    gint length;
    gpointer dat;

    dat=read_property(event->window, event->atom, &length);
    dprintf(3, "read data %p (length %d)", dat, length);
    if(dat && ((char *)dat)[0]=='<')
      dprintf(3, "data=%s", dat);
    reply=xmlParseMemory(dat, length);
    g_free(dat);

    /* Delete the data */
    dprintf(3, "delete %d", event->atom);
    gdk_property_delete(event->window, event->atom);
  }

  dprintf(3, "sdata->callback=%p: %p(%p, %d, %p, %p)",
	  sdata->callback, sdata->callback, sdata->filer, TRUE, reply,
	  sdata->data);
  if(sdata->callback)
    sdata->callback(sdata->filer, TRUE, reply, sdata->data);

  if(reply)
    xmlFreeDoc(reply);
  
  if(sdata->signal_tag) {
    g_signal_handler_disconnect(sdata->widget, sdata->signal_tag);
    sdata->signal_tag=0;
  }
  /*dprintf(3, "unref %p", sdata->widget);
  gtk_widget_unref(sdata->widget);
  dprintf(3, "done unref");*/
  
  dead_windows=g_slist_prepend(dead_windows, sdata->widget);
  g_free(sdata);
}

gboolean too_slow(gpointer data)
{
  SoapData *sdata=(SoapData *) data;

  if(sdata->timeout_tag) {
    /* Handler wasn't called */
    dprintf(2, "too_slow");
    sprintf(last_error_buffer, "SOAP timed out");
    last_error=last_error_buffer;

    if(sdata->callback)
      sdata->callback(sdata->filer, FALSE, NULL, sdata->data);
  }

  if(sdata->signal_tag) {
    g_signal_handler_disconnect(sdata->widget, sdata->signal_tag);
    sdata->signal_tag=0;
  }
  
  /*gtk_widget_unref(sdata->widget);*/
  dead_windows=g_slist_prepend(dead_windows, sdata->widget);

  return FALSE;
}

static void destroy_ipc_window(GtkWidget *ipc_window, SoapData *sdata)
{
  dprintf(3, "destroy_ipc_window(%p, %p)", ipc_window, sdata);
  g_free(sdata);
}

/**
 * Send the XML document to a program using SOAP.  If @a run_prog is @c TRUE
 * and there is no program to talk to, use rox_soap_send_via_pipe().
 * When complete callback is called with the status and reply.
 *
 * @param[in] prog connection to program
 * @param[in] doc XML document containg SOAP message
 * @param[in] run_prog if @c FALSE and the program could not be contacted,
 * then return failure.  Otherwise if a command has been defined (see
 * rox_soap_define_program()) execute that and send @a doc to its standard
 * input and read back the reply from its standard output.
 * @param[in] callback function to call after contact has been made and
 * the server returns success or failure.
 * @param[in] udata addtional data to pass to @a callback.
 * @return @c FALSE if the call failed, see rox_soap_get_last_error().
 */
gboolean rox_soap_send(ROXSOAP *prog, xmlDocPtr doc, gboolean run_prog,
		       rox_soap_callback callback, gpointer udata)
{
  GtkWidget	*ipc_window;
  Window		xwindow;
  xmlChar *mem;
  int	size;
  GdkEventClient event;
  SoapData *sdata;

  rox_soap_return_val_if_fail(prog!=NULL, FALSE);
  rox_soap_return_val_if_fail(doc!=NULL, FALSE);
  rox_soap_return_val_if_fail(callback!=NULL, FALSE);
  
  if(!done_init)
    rox_soap_init();

  if(!prog->existing_ipc_window)
    prog->existing_ipc_window=get_existing_ipc_window(prog->atom);
  dprintf(3, "existing_ipc_window %p", prog->existing_ipc_window);
  if(!prog->existing_ipc_window && (!run_prog || !prog->prog->command)) {
    sprintf(last_error_buffer, "No %s to target", prog->prog->name);
    last_error=last_error_buffer;
    return FALSE;
  }

  if(!prog->existing_ipc_window) {
    return rox_soap_send_via_pipe(prog, doc, callback, udata);
  }

  xmlDocDumpMemory(doc, &mem, &size);
  if(size<0) {
    sprintf(last_error_buffer, "Failed to dump XML doc to memory");
    last_error=last_error_buffer;
    return FALSE;
  }
  dprintf(3, "doc is %d bytes (%.10s)", size, mem);

  ipc_window = gtk_invisible_new();
  dprintf(2, "ipc_window=%p ref=%d", ipc_window,
	  G_OBJECT(ipc_window)->ref_count);

  gdk_property_change(ipc_window->window, prog->xsoap,
		      gdk_x11_xatom_to_atom(XA_STRING), 8,
		      GDK_PROP_MODE_REPLACE, mem, size);
  g_free(mem);
  dprintf(3, "set property %p on %p", xsoap, ipc_window->window);

  sdata=g_new(SoapData, 1);
  sdata->filer=prog;
  sdata->callback=callback;
  sdata->data=udata;
  sdata->prop=xsoap;
  sdata->widget=ipc_window;
  sdata->done_called=0;
  
  event.data.l[0] = GDK_WINDOW_XWINDOW(ipc_window->window);
  event.data.l[1] = gdk_x11_atom_to_xatom(prog->xsoap);
  event.data_format = 32;
  event.message_type = prog->xsoap;
  dprintf(3, "event->data.l={%p, %p}", event.data.l[0], event.data.l[1]);
	
  gtk_widget_add_events(ipc_window, GDK_PROPERTY_CHANGE_MASK);
  sdata->signal_tag=g_signal_connect(ipc_window, "property-notify-event",
				     G_CALLBACK(soap_done), sdata);
  
  dprintf(2, "sending message %p to %p", event.message_type,
	  prog->existing_ipc_window);
  gdk_event_send_client_message((GdkEvent *) &event,
		       	GDK_WINDOW_XWINDOW(prog->existing_ipc_window));
  dprintf(3, "sent message to %p",
	  GDK_WINDOW_XWINDOW(prog->existing_ipc_window));

  /*g_signal_connect(ipc_window, "destroy",
    G_CALLBACK(destroy_ipc_window), sdata);*/
  sdata->timeout_tag=g_timeout_add(prog->timeout, too_slow, sdata);

  return TRUE;
}

static void close_pipe(SoapPipeData *sdata, gboolean ok)
{
  int stat;
  
  waitpid(sdata->child, &stat, 0);
  dprintf(2, "status 0x%x from %d, closing %d", stat, sdata->child, sdata->fd);
  close(sdata->fd);
  if(WIFSIGNALED(stat)) {
    dprintf(1, "%d killed by signal %d %s", sdata->child, WTERMSIG(stat),
	      strsignal(WTERMSIG(stat)));
    sprintf(last_error_buffer , "%d killed by signal %d %s", sdata->child,
	    WTERMSIG(stat), strsignal(WTERMSIG(stat)));
    last_error=last_error_buffer;
  } else if(WIFEXITED(stat) && WEXITSTATUS(stat)) {
    dprintf(1, "%d exited with status %d", sdata->child, WEXITSTATUS(stat));
    sprintf(last_error_buffer, "child %d exited with status %d",
	    sdata->child, WEXITSTATUS(stat));
    last_error=last_error_buffer;
  }
  gdk_input_remove(sdata->read_tag);

  if(sdata->callback) {
    if(ok) {
      xmlDocPtr reply;

      reply=xmlParseMemory(sdata->reply->str, sdata->reply->len);
      sdata->callback(sdata->filer, TRUE, reply, sdata->data);
      xmlFreeDoc(reply);
    } else
      sdata->callback(sdata->filer, FALSE, NULL, sdata->data);
  }

  g_string_free(sdata->reply, TRUE);
  g_free(sdata);
}

static void read_from_pipe(gpointer data, gint com, GdkInputCondition cond)
{
  SoapPipeData *sdata=(SoapPipeData *) data;
  char buf[BUFSIZ];
  int nr;

  nr=read(com, buf, BUFSIZ);
  dprintf(3, "read %d from %d", nr, com);
  
  if(nr>0) {
    buf[nr]=0;
    g_string_append(sdata->reply, buf);
  } else if(nr==0) {
    close_pipe(sdata, TRUE);
  } else {
    dprintf(1, "problem reading data from %d,%d: %s", sdata->child,
	    sdata->fd, strerror(errno));
    sprintf(last_error_buffer, "problem reading data from %d,%d: %s", 
	    sdata->child, sdata->fd, strerror(errno));
    last_error=last_error_buffer;
    close_pipe(sdata, FALSE);
  }
}

/**
 * Send the XML document to a program using SOAP by executing the defined
 * program, sending @a doc to its standard
 * input and reading back the reply from its standard output.  If no
 * command has been defined for the program then the call fails.
 *
 * When complete callback is called with the status and reply.
 *
 * @param[in] prog connection to program
 * @param[in] doc XML document containg SOAP message
 * @param[in] callback function to call after contact has been made and
 * the server returns success or failure.
 * @param[in] udata addtional data to pass to @a callback.
 * @return @c FALSE if the call failed, see rox_soap_get_last_error().
 */
gboolean rox_soap_send_via_pipe(ROXSOAP *prog, xmlDocPtr doc,
				rox_soap_callback callback, gpointer udata)
{
  SoapPipeData *sdata;
  int tch[2], fch[2];
  FILE *out;

  rox_soap_return_val_if_fail(prog!=NULL, FALSE);
  rox_soap_return_val_if_fail(doc!=NULL, FALSE);
  rox_soap_return_val_if_fail(callback!=NULL, FALSE);

  if(!prog->prog->command) {
    sprintf(last_error_buffer, "%s has no defined command", prog->prog->name);
    last_error=last_error_buffer;
  }
  
  sdata=g_new(SoapPipeData, 1);
  sdata->filer=prog;
  sdata->callback=callback;
  sdata->data=udata;
  sdata->reply=g_string_new("");

  pipe(tch);
  pipe(fch);
  sdata->child=fork();
  switch(sdata->child) {
  case -1:
    dprintf(1, "failed to fork! %s", strerror(errno));
    sprintf(last_error_buffer, "failed to fork! %s", strerror(errno));
    last_error=last_error_buffer;
    close(tch[0]);
    close(tch[1]);
    close(fch[0]);
    close(fch[1]);
    g_free(sdata);
    return FALSE;

  case 0:
    close(tch[1]);
    close(fch[0]);
    dup2(tch[0], 0);
    dup2(fch[1], 1);
    execlp("sh", "sh", "-c", prog->prog->command, NULL);
    _exit(1);

  default:
    break;
  }
  dprintf(3, "child is %d, monitor %d", sdata->child, fch[0]);
  close(fch[1]);
  close(tch[0]);
  sdata->fd=fch[0];
  sdata->read_tag=gdk_input_add(sdata->fd, GDK_INPUT_READ,
				read_from_pipe, sdata);

  /* Write doc to the child */
  out=fdopen(tch[1], "w");
  xmlDocDump(out, doc);
  fclose(out);

  return TRUE;
}

/**
 * Set the time to wait for a reply from a SOAP call.
 *
 * @param[in,out] prog program to set time out for, or @c NULL to set default
 * time out for all subsequent programs.
 * @param[in] ms time out in milliseconds
 */
void rox_soap_set_timeout(ROXSOAP *prog, guint ms)
{
  if(prog)
    prog->timeout=ms;
  else
    timeout=ms;
}

/**
 * @return text message of last error
 */
const char *rox_soap_get_last_error(void)
{
  if(last_error)
    return last_error;

  return "No error";
}

/**
 * Clear last error
 */
void rox_soap_clear_error(void)
{
  last_error=NULL;
}

/*
 * $Log: rox_soap.c,v $
 * Revision 1.14  2005/03/04 17:22:18  stephen
 * Use apsymbols.h if available to reduce problems re-using binary on different Linux distros
 *
 * Revision 1.13  2004/07/31 12:25:14  stephen
 * gtk_timeout now g_timeout
 *
 * Revision 1.12  2004/05/12 18:25:11  stephen
 * Don't create the ipc_window until we are sure we need it
 *
 * Revision 1.11  2004/05/05 19:24:18  stephen
 * Extra debug (problem when target for SOAP doesn't exist)
 *
 * Revision 1.10  2003/10/22 17:15:59  stephen
 * Fix race condition in SOAP code.  Might finally have it working properly!
 *
 * Revision 1.9  2003/08/20 20:57:16  stephen
 * Removed a dead subroutine
 *
 * Revision 1.8  2003/03/25 14:31:34  stephen
 * New attempt at working SOAP code.
 *
 * Revision 1.7  2003/03/05 15:31:23  stephen
 * First pass a conversion to GTK 2
 * Known problems in SOAP code.
 *
 * Revision 1.6  2002/07/31 17:17:44  stephen
 * Use approved method of including libxml headers.
 *
 * Revision 1.5  2002/04/29 08:17:25  stephen
 * Fixed applet menu positioning (didn't work if program was managing more than
 * one applet window)
 * Some work for GTK+ 2
 *
 * Revision 1.4  2002/03/19 08:29:20  stephen
 * Added SOAP server (rox_soap_server.h).  SOAP client can connect to programs
 * other than ROX-Filer.
 *
 * Revision 1.3  2001/12/21 10:01:46  stephen
 * Updated version number, but not yet ready for new release.
 * Added debug to rox_soap (and protect if no XML)
 *
 * Revision 1.2  2001/12/07 11:25:01  stephen
 * More work on SOAP, mainly to get rox_filer_file_type() working.
 *
 * Revision 1.1  2001/12/05 16:46:34  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */

