#ifndef _run_task_h
#define _run_task_h

/*
 * $Id$
 *
 * Runs a task in order to get back textual output.
 */

typedef void (*RunTaskReader)(gchar *line, gpointer udata);

extern guint run_task(const char *cmd, RunTaskReader reader, gpointer udata);

/*
 * $Log$
 */
#endif
