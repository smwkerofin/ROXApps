/*
 * SystemTray, a notification area applet for ROX-Filer
 * By Andy Hanton, <andyhanton@comcast.net>.
 */


#ifndef TRAY_H
#define TRAY_H

typedef struct tray {
  GtkWidget *toplevel;
  GtkWidget *box;
  GtkSeparator *gliph;
  GtkWidget *manager_window;
  ROXPanelLocation location;
  void *bdata;
  gboolean ishoriz;
  int icon_count;
} tray;

extern void tray_init(void);
extern GtkWidget *tray_new(GtkWidget *top, ROXPanelLocation side);
extern void tray_destroy(GtkWidget *tray_widget);

#endif

