/*
 * $Id: rox_soap.h,v 1.1 2001/12/05 16:46:34 stephen Exp $
 *
 * rox_soap.h - interface to ROX-Filer using the SOAP protocol
 * (Yes, that's protocol twice on the line above.  Your problem?)
 *
 * WARNING: this is in a state of flux.  It may be that this will implement
 * a generic SOAP interface to any program and a wrapper to contact
 * ROX-Filer.
 *
 * e.g.
 * ROXSOAP *rox_soap_connect(const char *program_id);
 * #define rox_soap_connect_to_filer() rox_soap_connect("_ROX_FILER")
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
 * $Log: rox_soap.h,v $
 * Revision 1.1  2001/12/05 16:46:34  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */

