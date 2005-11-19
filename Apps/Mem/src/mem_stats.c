/*
 * $Id: mem_stats.c,v 1.1 2004/09/25 11:29:40 stephen Exp $
 *
 * mem_stats.c - the functions which get the stats
 */

#include "mem_stats.h"

/* Select compilation options */
#define DEBUG              1     /* Set to 1 for debug messages */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#ifdef HAVE_LIBGTOP
#include <glibtop.h>
#include <glibtop/mem.h>
#include <glibtop/swap.h>

#define MEM_FLAGS ((1<<GLIBTOP_MEM_TOTAL)|(1<<GLIBTOP_MEM_USED)|(1<<GLIBTOP_MEM_FREE))
#define SWAP_FLAGS ((1<<GLIBTOP_SWAP_TOTAL)|(1<<GLIBTOP_SWAP_USED)|(1<<GLIBTOP_SWAP_FREE))
#endif

#ifdef HAVE_KSTAT_H
#include <kstat.h>
#endif

#ifdef HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif

#include <rox/rox.h>

static int pagesize=0;

/* For caching of reading /proc/meminfo file */
static time_t last_proc_meminfo=0;

/* For caching of /proc/meminfo data */
static MemValue last_mem_total=0;
static MemValue last_mem_avail=0;
static MemValue last_mem_buffers=0;
static MemValue last_mem_cached=0;
static MemValue last_swap_total=0;
static MemValue last_swap_avail=0;

static int get_mem_libgtop(MemValue *total, MemValue *avail);
static int get_swap_libgtop(MemValue *total, MemValue *avail);
static int get_mem_kstat(MemValue *total, MemValue *avail);
static int get_swap_swapctl(MemValue *total, MemValue *avail);
static int get_mem_proc(MemValue *total, MemValue *avail);
static int get_swap_proc(MemValue *total, MemValue *avail);

void mem_stats_init(void)
{
  pagesize=getpagesize();

#ifdef HAVE_LIBGTOP
  dprintf(4, "set up glibtop");
  glibtop_init_r(&glibtop_global_server,
		 (1<<GLIBTOP_SYSDEPS_MEM)|(1<<GLIBTOP_SYSDEPS_SWAP), 0);
#endif
}

/* Return TRUE if valid values were stored in total and avail */
int mem_stats_get_mem(MemValue *total, MemValue *avail)
{
  int valid=FALSE;

  if(!valid)
    valid=get_mem_libgtop(total, avail);
  if(!valid)
    valid=get_mem_kstat(total, avail);
  if(!valid)
    valid=get_mem_proc(total, avail);

  return valid;
}

int mem_stats_get_swap(MemValue *total, MemValue *avail)
{
  int valid=FALSE;

  if(!valid)
    valid=get_swap_swapctl(total, avail);
  if(!valid)
    valid=get_swap_libgtop(total, avail);
  if(!valid)
    valid=get_swap_proc(total, avail);

  return valid;
}
			     
void mem_stats_shutdown(void)
{
  /* Not needed yet */
}

static int get_mem_libgtop(MemValue *total, MemValue *avail)
{
  int ok=FALSE;
  
#ifdef HAVE_LIBGTOP
  glibtop_mem mem;
  
  rox_debug_printf(3, "in get_mem_libgtop");
    errno=0;

  glibtop_get_mem(&mem);
  ok=(errno==0) && (mem.flags & MEM_FLAGS)==MEM_FLAGS;

  if(ok) {
    *total=(MemValue) mem.total;
    *avail=(MemValue) mem.free;
  }
#endif
  
  return ok;
}

static int get_swap_libgtop(MemValue *total, MemValue *avail)
{
  int ok=FALSE;
  
#ifdef HAVE_LIBGTOP
  glibtop_swap swap;
  
  rox_debug_printf(3, "in get_swap_libgtop");
  
  errno=0;
  glibtop_get_swap(&swap);
  ok=(errno==0) && (swap.flags & SWAP_FLAGS)==SWAP_FLAGS;
  
  if(ok) {
    *total=(MemValue) swap.total;
    *avail=(MemValue) swap.free;
  }
#endif
  
  return ok;
}

static int get_mem_kstat(MemValue *total, MemValue *avail)
{
  int ok=FALSE;
  
#if defined(HAVE_KSTAT_H) && defined(HAVE_KSTAT_OPEN)
  static kstat_ctl_t *kc=NULL;
  kid_t id;
  kstat_t *stat;
  kstat_named_t *kn;
  unsigned long long v;

  *total=(MemValue) sysconf(_SC_PHYS_PAGES)*pagesize;

  if(!kc) {
    kc=kstat_open();
  } else {
    kstat_chain_update(kc);
  }
  if(!kc)
    return FALSE;

  stat=kstat_lookup(kc, "unix", -1, "system_pages");
  if(!stat)
    return FALSE;
  kstat_read(kc, stat, NULL);
  kn=kstat_data_lookup(stat, "pagesfree");
  if(!kn)
    return FALSE;

  switch(kn->data_type) {
  case KSTAT_DATA_INT32:  v=kn->value.i32*pagesize;  break;
  case KSTAT_DATA_UINT32: v=kn->value.ui32*pagesize; break;
  case KSTAT_DATA_INT64:  v=kn->value.i64*pagesize;  break;
  case KSTAT_DATA_UINT64: v=kn->value.ui64*pagesize; break;
    
  default:
    return FALSE;
  }

  *avail=(MemValue) v;

  ok=TRUE;

#endif

  return ok;
}

static int get_swap_swapctl(MemValue *total, MemValue *avail)
{
  int ok=FALSE;

#ifdef HAVE_SWAPCTL
#define MAXSTRSIZE 128

  int nswp, n;
  swaptbl_t *table;
  int i;
  char *str;
  MemValue t=0, f=0;

  nswp=swapctl(SC_GETNSWP, NULL);
  if(nswp<=0) {
    *total=0;
    *avail=0;
    return FALSE;
  }

  table=malloc(nswp*sizeof(swapent_t) +sizeof(struct swaptable));
  if(!table) {
    *total=0;
    *avail=0;
    return FALSE;
  }

  str=malloc((nswp+1)*MAXSTRSIZE);
  if(!str) {
    free(table);
    *total=0;
    *avail=0;
    return FALSE;
  }
  for(i=0; i<nswp+1; i++)
    table->swt_ent[i].ste_path=str+i*MAXSTRSIZE;
  table->swt_n=nswp+1;

  n=swapctl(SC_LIST, table);
  /*printf("%d %d\n", nswp, n);*/
  for(i=0; i<n && i<nswp; i++) {
    /*printf("%s %ld %ld\n", table->swt_ent[i].ste_path,
	   table->swt_ent[i].ste_pages, table->swt_ent[i].ste_free);*/
    t+=table->swt_ent[i].ste_pages;
    f+=table->swt_ent[i].ste_free;
  }
  /*printf("%lld %lld\n", t, f);*/

  free(table);
  free(str);

  *total=t*pagesize;
  *avail=f*pagesize;

  ok=TRUE;
#endif

  return ok;
}

static MemValue decode_line(const char *line)
{
  const char *p;
  MemValue v;

  for(p=line; *p && *p!=':'; p++)
    ;
  if(!*p)
    return 0;

  p++;
  v=(MemValue) strtoul(p, NULL, 0);

  rox_debug_printf(3, "%s -> %lld K", line, v);
    
  return v<<10;
}

#define starts(s, w) (strncmp(s, w, strlen(w))==0)

static void read_proc_meminfo(void)
{
  FILE *f;
  char buf[1024], *line;

  f=fopen("/proc/meminfo", "r");
  if(f) {
    rox_debug_printf(2, "Opened %s", "/proc/meminfo");
    do {
      line=fgets(buf, sizeof(buf), f);
      if(!line)
	break;

      if(starts(line, "MemTotal:"))
	last_mem_total=decode_line(line);
      else if(starts(line, "MemFree:"))
	last_mem_avail=decode_line(line);
      else if(starts(line, "Buffers:"))
	last_mem_buffers=decode_line(line);
      else if(starts(line, "Cached:"))
	last_mem_cached=decode_line(line);
      else if(starts(line, "SwapTotal:"))
	last_swap_total=decode_line(line);
      else if(starts(line, "SwapFree:"))
	last_swap_avail=decode_line(line);

    } while(line);

    fclose(f);
    time(&last_proc_meminfo);
  }
}

static int get_mem_proc(MemValue *total, MemValue *avail)
{
  int ok=FALSE;
  time_t now;

  time(&now);
  if(now-last_proc_meminfo>1)
    read_proc_meminfo();

  if(now-last_proc_meminfo<=1) {
    ok=TRUE;
    *total=last_mem_total;
    *avail=last_mem_avail+last_mem_cached+last_mem_buffers;
  } else {
    ok=FALSE;
  }

  return ok;
}

static int get_swap_proc(MemValue *total, MemValue *avail)
{
  int ok=FALSE;
  time_t now;

  time(&now);
  if(now-last_proc_meminfo>1)
    read_proc_meminfo();

  if(now-last_proc_meminfo<=1) {
    ok=TRUE;
    *total=last_swap_total;
    *avail=last_swap_avail;
  } else {
    ok=FALSE;
  }

  return ok;
}

/*
 * $Log: mem_stats.c,v $
 * Revision 1.1  2004/09/25 11:29:40  stephen
 * Functions to get the Mem stats moved here
 *
 */
