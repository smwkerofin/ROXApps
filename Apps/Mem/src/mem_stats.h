/*
 * $Id$
 *
 * mem_stats.h - interface to the functions which get the stats
 */

#ifndef _mem_stats_h
#define _mem_stats_h

#include "config.h"

/* Type to hold values */
#ifdef HAVE_UNSIGNED_LONG_LONG
typedef unsigned long long MemValue;
#else
typedef unsigned long MemValue;
#endif

extern void mem_stats_init(void);

/* Return TRUE if valid values were stored in total and avail */
extern int mem_stats_get_mem(MemValue *total, MemValue *avail);
extern int mem_stats_get_swap(MemValue *total, MemValue *avail);
			     
extern void mem_stats_shutdown(void);

#endif

/*
 * $Log$
 */
