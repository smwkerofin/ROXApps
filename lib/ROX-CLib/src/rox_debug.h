/*
 * rox_debug.h - Access to standard debug output
 *
 * Unless DEBUG is define as non-zero all these functions are no-ops
 *
 * $Id: rox_debug.h,v 1.1 2001/07/17 14:44:49 stephen Exp $
 */

#ifndef _rox_debug_h
#define _rox_debug_h

#if !defined(DEBUG) || DEBUG!=0

/* Should be called at start, if not (or progname==NULL) then progname
   is assumed to be "ROX"

   Now called by rox_init()
*/
extern void rox_debug_init(const char *progname);

/* Send debug message to g_logv (no \n needed at end) */
extern void rox_debug_printf(int level, const char *fmt, ...);

#else

#define rox_debug_init(progname)
#define rox_debug_printf (void)

#endif

/* Less typing */
#define dprintf rox_debug_printf

#endif /* !defined(_rox_debug_h) */

/*
 * $Log: rox_debug.h,v $
 * Revision 1.1  2001/07/17 14:44:49  stephen
 * Added DnD stuff (plus path utils and debug util)
 *
 */
