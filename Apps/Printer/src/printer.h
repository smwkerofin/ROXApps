/*
 * Printer - a printer manager for ROX
 * Stephen Watson <stephen@kerofin.demon.co.uk>
 *
 * GPL applies.
 *
 * $Id$
 */

#ifndef _printer_h
#define _printer_h

enum columns {
  COL_ID, COL_SIZE, COL_USER, COL_START,
  COL_END
};
#define NUM_COLUMN COL_END

typedef struct printer {
  gchar *name;
  gboolean enabled;
  gboolean available;

  GList *jobs;
} Printer;

extern Printer *printer_new(const char *name, gboolean en, gboolean avail);
extern void printer_free(Printer *);
extern Printer *printer_find_by_name(const char *name);

extern GList *printers;
extern Printer *current_printer;
extern Printer *default_printer;

typedef struct job {
  gchar *id;
  guint size;
  gchar *user;
  gchar *started;
  gboolean ours;
  Printer *printer;
} Job;

extern Job *job_new(Printer *print, const char *id, int size,
		    const char *username, const char *start);
extern void job_free(Job *);

#endif


/*
 * $Log$
 */

