/*
 * $Id$
 *
 * rox_filer_action.h - drive the filer via SOAP
 */

#ifndef _rox_filer_action_h
#define _rox_filer_action_h

typedef enum panel_side {
  ROXPS_TOP,ROXPS_BOTTOM,ROXPS_LEFT,ROXPS_RIGHT
} ROXPanelSide;

#define ROX_FILER_DEFAULT (-1)  /* Use default setting of option */

extern void rox_filer_open_dir(const char *filename);
extern void rox_filer_close_dir(const char *filename);
extern void rox_filer_examine(const char *filename);
extern void rox_filer_panel(const char *name, ROXPanelSide side);
extern void rox_filer_pinboard(const char *name);
extern void rox_filer_run(const char *filename);
extern void rox_filer_show(const char *directory, const char *leafname);

extern void rox_filer_copy(const char *from, const char *to,
			   const char *leafname, int quiet);
extern void rox_filer_move(const char *from, const char *to,
			   const char *leafname, int quiet);
extern void rox_filer_link(const char *from, const char *to,
			   const char *leafname);
extern void rox_filer_mount(const char *mountpoint,
			    int quiet, int opendir);

/* Return value should be passed to g_free when done */
extern char *rox_filer_file_type(const char *file);

extern const char *rox_filer_get_last_error(void);
extern void rox_filer_clear_error(void);

#endif

/*
 * $Log$
 */
