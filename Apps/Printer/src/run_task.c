/*
 * $Id$
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <gtk/gtk.h>

#include "run_task.h"

struct task {
  guint timeout_tag;
  guint input_tag;
  pid_t child;
  int fd;
  RunTaskReader reader;
  gpointer udata;
  gchar *tail;
};
typedef struct task Task;

static void close_pipe(Task *task)
{
  if(task->tail)
    task->reader(task->tail, task->udata);
    
  gdk_input_remove(task->input_tag);
  
  if(task->reader)
    task->reader(NULL, task->udata);
  task->reader=NULL;
  close(task->fd);
  task->fd=-1;
}

static void pipe_reader(gpointer data, gint fd,
			      GdkInputCondition condition)
{
  Task *task=(Task *) data;
  int nready;
  int nr, actual;
  gchar *buf;
  gchar *line, *end;

  if(task->fd!=fd)
    return;

  if(!(condition&GDK_INPUT_READ) || ioctl(fd, FIONREAD, &nready)<0)
    return;

  if(nready<=0) {
    close_pipe(task);
    return;
  }

  buf=g_new(char, nready+1);
  actual=read(fd, buf, nready);
  if(actual<1) {
    close_pipe(task);
    return;
  }
  
  buf[actual]=0;

  if(task->tail) {
    gchar *tmp=g_strconcat(task->tail, buf, NULL);
    g_free(buf);
    buf=tmp;
    g_free(task->tail);
    task->tail=NULL;
  }

  for(line=buf; line<buf+actual; line=end+1) {
    gchar *tmp;
    
    end=strchr(line, '\n');
    if(!end) {
      task->tail=g_strdup(line);
      break;
    }

    tmp=g_strndup(line, end-line+1);
    task->reader(tmp, task->udata);
    g_free(tmp);
  }

  g_free(buf);
}

static gint child_monitor(gpointer data)
{
  Task *task=(Task *) data;
  int stat, pid;

  if(!task)
    return FALSE;
  
  pid=waitpid(task->child, &stat, WNOHANG);
  if(pid==task->child && (WIFEXITED(stat) || WIFSIGNALED(stat))) {
    if(task->reader)
      task->reader(NULL, task->udata);
    if(task->fd>=0)
      close(task->fd);
    g_free(task);
    return FALSE;
  }

  return TRUE;
}

guint run_task(const char *cmd, RunTaskReader reader, gpointer udata)
{
  int i;
  int out[2];
  Task *task=g_new(Task, 1);

  task->reader=reader;
  task->udata=udata;
  task->tail=NULL;
  
  pipe(out);

  task->child=fork();
  if(!task->child) {
    dup2(out[1], 1);
    for(i=getdtablesize(); i>2; i--) /* Close all but standard files */
      (void) close(i);

    execl("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(12);
    
  } else if(task->child<0) {
    g_free(task);
    return NULL;
    
  } else {
    close(out[1]);

    task->input_tag=gdk_input_add(out[0], GDK_INPUT_READ, pipe_reader, task);
    task->fd=out[0];

    task->timeout_tag=gtk_timeout_add(1000, child_monitor, task);
  }

  return (guint) task;
}


/*
 * $Log$
 */
