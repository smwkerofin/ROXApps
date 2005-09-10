/*
 * $Id: rox_soap_server.h,v 1.2 2004/04/07 19:26:59 stephen Exp $
 *
 * rox_soap_server.h - Provide ROX-Filer like SOAP server
 */

/**
 * @file rox_soap_server.h
 * @brief Provide ROX-Filer like SOAP server.
 *
 * SOAP allows you to communicate with server programs via the X server.
 * You may implement your applications to function from a single instance
 * no matter how many times they are started, much as ROX-Filer does itself.
 *
 * @author Stephen Watson
 * @version $Id$
 */

#ifndef _rox_soap_server_h
#define _rox_soap_server_h

/** Opaque type defining a SOAP server */
typedef struct rox_soap_server ROXSOAPServer;

/**
 * Type of function called when a client request an action via SOAP.
 *
 * @param[in] server object identifing the server
 * @param[in] action_name name of action
 * @param[in] args list of arguments in string form, in the order
 * defined in rox_soap_server_add_action() or #ROXSOAPServerActions.  The
 * required arguments are first, then the optional arguments with @c NULL
 * given for those ommitted.
 * @param[in] udata addtional data given in rox_soap_server_add_action() or
 * #ROXSOAPServerActions.
 * @return a valid XML SOAP reply node, or @c NULL for no reply
 */
typedef xmlNodePtr (*rox_soap_server_action)(ROXSOAPServer *server,
				       const char *action_name,
				       GList *args, gpointer udata);

extern void rox_soap_server_init(void);

extern ROXSOAPServer *rox_soap_server_new(const char *program_name,
					  const char *ns_url);

extern void rox_soap_server_add_action(ROXSOAPServer *server,
				       const char *action_name,
				       const char *args,
				       const char *optional_args,
				       rox_soap_server_action action,
				       gpointer udata);

/**
 * Type defining an action.
 */
typedef struct rox_soap_server_actions {
  const char *action_name;       /**< Name of an action */
  const char *args;              /**< Comma seperated list of required
				  * arguments, in the order they will be
				  * presented to the #rox_soap_server_action
				  * function. */
  const char *optional_args;     /**< Comma seperated list of optional
				  * arguments, in the order they will be
				  * presented to the #rox_soap_server_action
				  * function. */
  rox_soap_server_action action; /**< Function to call to process the action */
  gpointer udata;                /**< Addtional data to pass to @a action */
} ROXSOAPServerActions;

extern void rox_soap_server_add_actions(ROXSOAPServer *server,
					ROXSOAPServerActions *actions);

extern void rox_soap_server_delete(ROXSOAPServer *server);

#endif

/*
 * $Log: rox_soap_server.h,v $
 * Revision 1.2  2004/04/07 19:26:59  stephen
 * Added call to add multiple SOAP actions.
 *
 * Revision 1.1  2002/03/19 08:29:23  stephen
 * Added SOAP server (rox_soap_server.h).  SOAP client can connect to programs
 * other than ROX-Filer.
 *
 */
