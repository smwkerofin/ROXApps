/*
 * $Id: rox_filer_action.h,v 1.3 2002/02/27 16:28:09 stephen Exp $
 *
 * rox_filer_action.h - drive the filer via SOAP
 */

#ifndef _rox_filer_action_h
#define _rox_filer_action_h

#define ROX_NAMESPACE_URL "http://rox.sourceforge.net/SOAP/ROX-Filer"

typedef enum panel_side {
  ROXPS_TOP,ROXPS_BOTTOM,ROXPS_LEFT,ROXPS_RIGHT
} ROXPanelSide;

#define ROX_FILER_DEFAULT (-1)  /* Use default setting of option */

extern void rox_filer_open_dir(const char *filename);
extern void rox_filer_close_dir(const char *filename);
extern void rox_filer_examine(const char *filename);
extern void rox_filer_panel(const char *name, ROXPanelSide side);
extern void rox_filer_panel_add(ROXPanelSide side, const char *path,
				int after);
extern void rox_filer_pinboard(const char *name);
extern void rox_filer_pinboard_add(const char *path, int x, int y);
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
extern char *rox_filer_version(void);
extern char *rox_filer_file_type(const char *file);

/* Do not free return value */
extern const char *rox_filer_get_last_error(void);
extern int rox_filer_have_error(void);
extern void rox_filer_clear_error(void);

#endif

/*
 * $Log: rox_filer_action.h,v $
 * Revision 1.3  2002/02/27 16:28:09  stephen
 * Add support for PanelAdd and PinboardAdd (assuming they get into the
 * filer)
 *
 * Revision 1.2  2002/01/07 15:37:59  stephen
 * added rox_filer_version() and rox_filer_have_error()
 *
 * Revision 1.1  2001/12/05 16:46:33  stephen
 * Added rox_soap.c to talk to the filer using SOAP.  Added rox_filer_action.c
 * to use rox_soap to drive the filer.
 * Added test.c to try the above routines.
 *
 */
