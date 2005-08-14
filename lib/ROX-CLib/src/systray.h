/*
 * systray.c - Interface to the system tray.
 *
 * $Id: systray.h,v 1.1 2005/06/07 10:22:54 stephen Exp $
 */

/** @file systray.h
 * @brief Interface to the system tray.
 */

#ifndef _rox_systray_h
#define _rox_systray_h

extern void rox_systray_init(void);

extern GtkWidget *rox_systray_new(void);
extern GtkWidget *rox_systray_new_on_screen(GdkScreen *screen);

extern int rox_systray_send_message(GtkWidget *systray,
				     const char *message,
				     unsigned usec);

extern void rox_systray_cancel_message(GtkWidget *systray, int id);

#endif

/*
 * $Log: systray.h,v $
 * Revision 1.1  2005/06/07 10:22:54  stephen
 * Added system tray interface
 *
 *
 */
