/*
 * SystemTray, a notification area applet for ROX-Filer
 * By Andy Hanton, <andyhanton@comcast.net>.
 */


#ifndef BALLOON_H
#define BALLOON_H
#include "tray.h"

typedef struct balloon {
  char *text;
  Window window;
  int length, id, timeout;
  char *window_name;
  guint timeout_id;
  GtkWidget *balloon_window;
  tray *t;
  int use_count;
} balloon;

typedef struct balloon_data {
  GSList *partial_messages;
  GSList *message_queue;
  balloon *current_balloon;
} balloon_data;

#include "tray.h"

void balloon_init(tray *t);
void balloon_cleanup(tray *t);
void new_balloon(tray *t, XClientMessageEvent *event);
void cancel_balloon(tray *t, XClientMessageEvent *event);
void add_balloon_data(tray *t, XClientMessageEvent *event);

#endif
