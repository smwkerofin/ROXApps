/*
 * $Id$
 *
 * rox_soap.h - interface to ROX-Filer using the SOAP protocol
 * (Yes, that's protocol twice on the line above.  Your problem?)
 */

#ifndef _rox_soap_h
#define _rox_soap_h

struct rox_soap_filer;
typedef struct rox_soap_filer ROXSOAP;

typedef void (*rox_soap_callback)(ROXSOAP *filer, gboolean status, 
				  xmlDocPtr reply, gpointer udata);

/* Initialize data
 */
extern void rox_soap_init(void);

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
 * $Log$
 */

