/*
 * $Id: rox_soap_server.h,v 1.1 2002/03/19 08:29:23 stephen Exp $
 *
 * rox_soap_server.h - Provide ROX-Filer like SOAP server
 */

#ifndef _rox_soap_server_h
#define _rox_soap_server_h

struct rox_soap_server;

typedef struct rox_soap_server ROXSOAPServer;

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

typedef struct rox_soap_server_actions {
  const char *action_name;
  const char *args;
  const char *optional_args;
  rox_soap_server_action action;
  gpointer udata;
} ROXSOAPServerActions;

extern void rox_soap_server_add_actions(ROXSOAPServer *server,
					ROXSOAPServerActions *actions);

extern void rox_soap_server_delete(ROXSOAPServer *server);

#endif

/*
 * $Log: rox_soap_server.h,v $
 * Revision 1.1  2002/03/19 08:29:23  stephen
 * Added SOAP server (rox_soap_server.h).  SOAP client can connect to programs
 * other than ROX-Filer.
 *
 */
