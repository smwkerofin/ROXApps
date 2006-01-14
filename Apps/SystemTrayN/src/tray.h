/*
 * SystemTray, a notification area applet for ROX-Filer
 * By Andy Hanton, <andyhanton@comcast.net>.
 */


#ifndef TRAY_H
#define TRAY_H

typedef struct tray {
  GtkWidget *toplevel;
  GtkWidget *box;
  GtkWidget *spacer;
  GtkSeparator *gliph;
  GtkWidget *manager_window;
  PanelLocation location;
  void *bdata;
} tray;

GtkWidget *tray_new(GtkWidget *top, PanelLocation side);
void tray_destroy(GtkWidget *tray_widget);

#endif

