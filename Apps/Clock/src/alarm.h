/*
 * alarm.c - alarms for the Clock program
 *
 * $Id: alarm.h,v 1.1 2001/05/10 14:54:28 stephen Exp $
 */

#ifndef _alarm_h
#define _alarm_h

extern void alarm_load(void);
extern void alarm_save(void);

extern void alarm_check(void);
extern void alarm_show_window(void);

extern int alarm_have_active(void);

#endif

/*
 * $Log: alarm.h,v $
 * Revision 1.1  2001/05/10 14:54:28  stephen
 * Added new alarm feature
 *
 */

