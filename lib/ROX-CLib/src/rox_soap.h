/*
 * $Id: rox_soap.h,v 1.3 2001/12/21 10:01:46 stephen Exp $
 *
 * rox_soap.h - interface to ROX-Filer using the SOAP protocol
 * (Yes, that's protocol twice on the line above.  Your problem?)
 *
 * Now expanded to interface to any program that implements a ROX-Filer like
 * interface.  See rox_soap_server.h
 */

#ifndef _rox_soap_h
#define _rox_soap_h

#include "rox-clib.h"

#ifndef HAVE_XML
#define xmlDocPtr void *
#define xmlNodePtr void *
#endif

#define ENV_NAMESPACE_URL "http://www.w3.org/2001/12/soap-envelope"
#define SOAP_NAMESPACE_URL "http://www.w3.org/2001/12/soap-rpc"

struct rox_soap;
typedef struct rox_soap ROXSOAP;

typedef void (*rox_soap_callback)(ROXSOAP *filer, gboolean status, 
				  xmlDocPtr reply, gpointer udata);

/* Initialize data
 */
extern void rox_soap_init(void);

/*
 * Define how to connect to the named program.
 * name     - name of program as passed to rox_soap_connect
 * atom_fmt - format of atom name to use to connect, %e is effective UID,
 *            %h is FQDN of display
 * command  - command that may be fed the SOAP document on stdin if
 *            connecting via the atom fails, may be NULL
 *
 * An entry for ROX-Filer is pre-defined
 */
extern void rox_soap_define_program(const char *name, const char *atom_fmt,
				    const char *command);

/*
 * Returns the name of the atom which will be used to locate the given program.
 * It uses the format given in the rox_soap_define_program() call, with the
 * defined substitutions.  Pass to g_free() when done.
 */
extern char *rox_soap_atom_name_for_program(const char *name);

/*
 * Check connection to the program
 */
extern gboolean rox_soap_ping(const char *prog);

/*
 * Initialise the connection to the named program
 */
extern ROXSOAP *rox_soap_connect(const char *prog);

/*
 * Initialise the connection to the filer
 */
extern ROXSOAP *rox_soap_connect_to_filer(void);

/* Send the XML document to ROX-Filer using SOAP.  If run_filer is TRUE
 * and there is no filer to talk to, use rox_soap_send_via_pipe()
 * Returns TRUE if comms succeeded, 
 * When complete callback is called with the status and reply.
 */
extern gboolean rox_soap_send(ROXSOAP *filer,
			      xmlDocPtr doc, gboolean run_filer,
			      rox_soap_callback callback, gpointer udata);

/* Set the timeout when contacting ROX-Filer (in ms, defaults to 10000).
 */
extern void rox_soap_set_timeout(ROXSOAP *filer, guint ms);

/* Send the XML document to ROX-Filer using the -R option.
 * When complete callback is called with the status and reply.
 */
extern gboolean rox_soap_send_via_pipe(ROXSOAP *filer, xmlDocPtr doc, 
				   rox_soap_callback callback, gpointer udata);

/*
 * Build part of XML document to send.
 * The action to perform is in name space ns_url.  Add arguments to the node
 * act, then call rox_soap_send with the return value.
 */
extern xmlDocPtr rox_soap_build_xml(const char *action, const char *ns_url,
				    xmlNodePtr *act);

/*
 * Return text of last error raised
 */
extern const char *rox_soap_get_last_error(void);

/* Clear error text
 */
extern void rox_soap_clear_error(void);

/* Close connection
 */
extern void rox_soap_close(ROXSOAP *filer);

#endif

/*
 * $Log: rox_soap.h,v $
 * Revision 1.3  2001/12/21 10:01:46  stephen
 * Updated version number, but not yet ready for new release.
 * Added debug to rox_soap (and protect if no XML)
 *
 * Revision 1.2  2001/12/07 11:25:02  stephen
 * More work on SOAP, mainly to get rox_filer_file_type() working.
 *
 * Revision 1.1  2001/12/05 16:46:34  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */

