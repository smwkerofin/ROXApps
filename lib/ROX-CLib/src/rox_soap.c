/*
 * $Id: rox_soap.c,v 1.1 2001/12/05 16:46:34 stephen Exp $
 *
 * rox_soap.c - interface to ROX-Filer using the SOAP protocol
 * (Yes, that's protocol twice on the line above.  Your problem?)
 *
 * Mostly adapted from ROX-Filer/src/remote.c by Thomas Leonard
 */
#include "rox-clib.h"

#include <stdlib.h>
#include <errno.h>

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

#ifdef HAVE_XML
#include <parser.h>
#endif

#include "rox_soap.h"
#include "error.h"

#define DEBUG 1
#include "rox_debug.h"

#define TIMEOUT 10000

struct rox_soap_filer {
  GdkAtom filer;
  GdkAtom xsoap;
  GdkWindow *existing_ipc_window;
  guint timeout;
};

typedef struct soap_data {
  ROXSOAP *filer;
  rox_soap_callback callback;
  gpointer data;
  GdkAtom prop;
  GtkWidget *widget;
  guint timeout_tag;
} SoapData;

typedef struct soap_pipe_data {
  ROXSOAP *filer;
  rox_soap_callback callback;
  gpointer data;
  pid_t child;
  int fd;
  guint read_tag;
  GString *reply;
} SoapPipeData;

static GdkAtom filer_atom;	/* _ROX_FILER_EUID_HOST */
static GdkAtom xsoap;		/* _XSOAP */

static gboolean done_init=FALSE;
static guint timeout=TIMEOUT;

static char *last_error=NULL;
static char last_error_buffer[1024];

static GdkWindow *get_existing_ipc_window(GdkAtom);

#define rox_soap_return_if_fail(expr) \
do{if(!(expr)){sprintf(last_error_buffer,"assertion '%s' failed",#expr); \
last_error=last_error_buffer;return;}}while(0);

#define rox_soap_return_val_if_fail(expr, val) \
do{if(!(expr)){sprintf(last_error_buffer,"assertion '%s' failed",#expr); \
last_error=last_error_buffer;return val;}}while(0);

#ifdef HAVE_XML
void rox_soap_init(void)
{
  gchar *id;
  char hostn[256];
  struct hostent *ent;

  if(done_init)
    return;

  gethostname(hostn, sizeof(hostn));
  ent=gethostbyname(hostn);
  id=g_strdup_printf("_ROX_FILER_%d_%s", (int) geteuid(),
		     ent? ent->h_name: hostn);
  dprintf(3, "filer_atom=%s", id);
  filer_atom=gdk_atom_intern(id, FALSE);
  g_free(id);

  xsoap=gdk_atom_intern("_XSOAP", FALSE);

  done_init=TRUE;
}

ROXSOAP *rox_soap_connect_to_filer(void)
{
  ROXSOAP *filer=g_new(ROXSOAP, 1);

  if(!done_init)
    rox_soap_init();

  filer->filer=filer_atom;
  filer->xsoap=xsoap;
  filer->timeout=timeout;
  filer->existing_ipc_window=get_existing_ipc_window(filer->filer);

  return filer;
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
		if (format == 32 && length == 4)
		{
			retval = TRUE;
			*r_xid = *((Window *) data);
		}
		g_free(data);
	}

	return retval;
}

/* Get the remote IPC window of the already-running filer if there
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
	if (gdk_window_lookup(xid))
		return NULL;	/* Stale handle which we now own */

	/*gdk_flush();*/
	dprintf(3, "gdk_window_foreign_new %p", xid);
	window = gdk_window_foreign_new(xid);
	if (!window)
		return NULL;

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

  dprintf(2, "soap_done %p vs %p", sdata->prop, event->atom);

  if(sdata->prop != event->atom)
    return;

  dprintf(2, "soap_done");
  gtk_timeout_remove(sdata->timeout_tag);

  if(event->state==GDK_PROPERTY_NEW_VALUE) {
    gint length;
    gpointer dat;

    dat=read_property(event->window, event->atom, &length);
    dprintf(3, "read data %p (length %d)", dat, length);
    if(dat && ((char *)dat)[0]=='<')
      dprintf(3, "data=%s", dat);
    reply=xmlParseMemory(dat, length);
    g_free(dat);
  }

  if(sdata->callback)
    sdata->callback(sdata->filer, TRUE, reply, sdata->data);

  if(reply)
    xmlFreeDoc(reply);
  gtk_widget_unref(sdata->widget);
  g_free(sdata);
}

gboolean too_slow(gpointer data)
{
  SoapData *sdata=(SoapData *) data;

  dprintf(2, "too_slow");
  sprintf(last_error_buffer, "SOAP timed out");
  last_error=last_error_buffer;

  if(sdata->callback)
    sdata->callback(sdata->filer, FALSE, NULL, sdata->data);

  gtk_widget_unref(sdata->widget);
  g_free(sdata);

  return FALSE;
}

gboolean rox_soap_send(ROXSOAP *filer, xmlDocPtr doc, gboolean run_filer,
		       rox_soap_callback callback, gpointer udata)
{
  GtkWidget	*ipc_window;
  Window		xwindow;
  xmlChar *mem;
  int	size;
  GdkEventClient event;
  SoapData *sdata;

  rox_soap_return_val_if_fail(filer!=NULL, FALSE);
  rox_soap_return_val_if_fail(doc!=NULL, FALSE);
  rox_soap_return_val_if_fail(callback!=NULL, FALSE);
  
  if(!done_init)
    rox_soap_init();

  ipc_window = gtk_invisible_new();
  gtk_widget_realize(ipc_window);

  if(!filer->existing_ipc_window)
    filer->existing_ipc_window=get_existing_ipc_window(filer->filer);
  dprintf(3, "existing_ipc_window %p", filer->existing_ipc_window);
  if(!filer->existing_ipc_window && !run_filer) {
    sprintf(last_error_buffer, "No ROX-Filer to target");
    last_error=last_error_buffer;
    return FALSE;
  }

  if(!filer->existing_ipc_window)
    return rox_soap_send_via_pipe(filer, doc, callback, udata);

  xmlDocDumpMemory(doc, &mem, &size);
  if(size<0) {
    sprintf(last_error_buffer, "Failed to dump XML doc to memory");
    last_error=last_error_buffer;
    return FALSE;
  }
  dprintf(3, "doc is %d bytes (%.10s)", size, mem);

  gdk_property_change(ipc_window->window, filer->xsoap,
		      gdk_x11_xatom_to_atom(XA_STRING), 8,
		      GDK_PROP_MODE_REPLACE, mem, size);
  g_free(mem);
  dprintf(3, "set property %p on %p", xsoap, ipc_window->window);

  sdata=g_new(SoapData, 1);
  sdata->filer=filer;
  sdata->callback=callback;
  sdata->data=udata;
  sdata->prop=xsoap;
  sdata->widget=ipc_window;
  
  event.data.l[0] = GDK_WINDOW_XWINDOW(ipc_window->window);
  event.data.l[1] = gdk_x11_atom_to_xatom(filer->xsoap);
  event.data_format = 32;
  event.message_type = filer->xsoap;
	
  gtk_widget_add_events(ipc_window, GDK_PROPERTY_CHANGE_MASK);
  gtk_signal_connect(GTK_OBJECT(ipc_window), "property-notify-event",
		     GTK_SIGNAL_FUNC(soap_done), sdata);

  dprintf(2, "sending message %p to %p", event.message_type,
	  filer->existing_ipc_window);
  gdk_event_send_client_message((GdkEvent *) &event,
		       	GDK_WINDOW_XWINDOW(filer->existing_ipc_window));
  dprintf(3, "sent message to %p",
	  GDK_WINDOW_XWINDOW(filer->existing_ipc_window));

  sdata->timeout_tag=gtk_timeout_add(filer->timeout, too_slow, sdata);
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

gboolean rox_soap_send_via_pipe(ROXSOAP *filer, xmlDocPtr doc,
				rox_soap_callback callback, gpointer udata)
{
  SoapPipeData *sdata;
  int tch[2], fch[2];
  FILE *out;

  rox_soap_return_val_if_fail(filer!=NULL, FALSE);
  rox_soap_return_val_if_fail(doc!=NULL, FALSE);
  rox_soap_return_val_if_fail(callback!=NULL, FALSE);
  
  sdata=g_new(SoapPipeData, 1);
  sdata->filer=filer;
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
    execlp("rox", "rox", "-R", NULL);
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

void rox_soap_set_timeout(ROXSOAP *filer, guint ms)
{
  if(filer)
    filer->timeout=ms;
  else
    timeout=ms;
}
#else /* HAVE_XML */
void rox_soap_init(void)
{
  strcpy(last_error_buffer, "XML not available, SOAP support disabled");
  last_error=last_error_buffer;
}

gboolean rox_soap_send(ROXSOAP *filer, xmlDocPtr doc, gboolean run_filer,
			      rox_soap_callback callback, gpointer udata)
{
  strcpy(last_error_buffer, "XML not available, SOAP support disabled");
  last_error=last_error_buffer;
  return FALSE;
}

void rox_soap_set_timeout(ROXSOAP *filer, guint ms)
{
  strcpy(last_error_buffer, "XML not available, SOAP support disabled");
  last_error=last_error_buffer;
}

gboolean rox_soap_send_via_pipe(ROXSOAP *filer, xmlDocPtr doc, 
				rox_soap_callback callback, gpointer udata)
{
  strcpy(last_error_buffer, "XML not available, SOAP support disabled");
  last_error=last_error_buffer;
  return FALSE;
}

#endif /* HAVE_XML */

const char *rox_soap_get_last_error(void)
{
  if(last_error)
    return last_error;

  return "No error";
}

void rox_soap_clear_error(void)
{
  last_error=NULL;
}

/*
 * $Log: rox_soap.c,v $
 * Revision 1.1  2001/12/05 16:46:34  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */

