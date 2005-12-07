/*
 * error.h - display error message
 *
 * $Id: error.h,v 1.3 2005/08/14 16:07:00 stephen Exp $
 */

/**
 * @file error.h
 * @brief Display error message.
 *
 * Display error message, or queue for later handling.
 */

#ifndef _error_h
#define _error_h

/**
 * Quark used by ROX-CLib in creating GErrors.  This is initialized by
 * rox_error_init().
 */
extern GQuark rox_error_quark;

extern void rox_error_init(void);

extern void rox_error(const char *fmt, ...);
extern void rox_error_report_gerror(GError *err, int del);

extern void rox_error_queue(GError *err);
extern gboolean rox_error_queue_empty(void);
extern GError *rox_error_queue_peek(void);
extern GError *rox_error_queue_fetch(void);
extern GError *rox_error_queue_peek_last(void);
extern GError *rox_error_queue_fetch_last(void);
extern void rox_error_queue_flush(void);
extern void rox_error_queue_report(void);

#endif

/*
 * $Log: error.h,v $
 * Revision 1.3  2005/08/14 16:07:00  stephen
 * Added rox_resources_find_with_domain().
 * More doxygen additions.
 *
 * Revision 1.2  2001/07/17 14:42:59  stephen
 * Changed name of call
 *
 * Revision 1.1.1.1  2001/05/29 14:09:59  stephen
 * Initial version of the library
 *
 */
