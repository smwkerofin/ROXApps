/*
 * alarm.c - alarms for the Clock program
 *
 * $Id: alarm.c,v 1.5 2001/06/29 10:42:48 stephen Exp $
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include <gtk/gtk.h>

#include "choices.h"
#include "alarm.h"

extern void dprintf(int level, const char *fmt, ...);
void debug_free(gpointer p, const char *file, int line);
#if 0
#define g_free(p) debug_free((gpointer) p, __FILE__, __LINE__);
#endif

typedef enum repeat_mode {
  REPEAT_NONE,
  REPEAT_HOURLY, REPEAT_DAILY,
  REPEAT_WEEKDAYS, /* Mon-Fri */
  REPEAT_WEEKLY, REPEAT_MONTHLY, REPEAT_YEARLY,
} RepeatMode;

static const char *repeat_text[]={
  N_("None"), N_("Hourly"), N_("Daily"), N_("Weekdays only"), N_("Weekly"),
  N_("Monthly"), N_("Yearly"),
  NULL
};

typedef struct alarm {
  time_t when;
  RepeatMode repeat;
  gchar *message;
} Alarm;

static GList *alarms=NULL;

static time_t alarms_saved=(time_t) 0;

int alarm_have_active(void)
{
  return alarms!=NULL;
}

static Alarm *alarm_new(time_t w, RepeatMode r, const char *m)
{
  Alarm *alarm=g_new(Alarm, 1);

  if(!alarm)
    return NULL;

  alarm->when=w;
  alarm->repeat=r;
  alarm->message=g_strdup(m);

  return alarm;
}

static void alarm_delete(Alarm *alarm)
{
  g_free(alarm->message);
  g_free(alarm);
}

void alarm_load(void)
{
  gchar *fname;
#ifdef HAVE_SYS_STAT_H
    struct stat statb;
#endif

  fname=choices_find_path_load("alarms", PROJECT);

  if(fname) {
    FILE *in;
    
    in=fopen(fname, "r");
    if(in) {
      char buf[1024], *line;
      char *end, *sep;
      
      do {
	line=fgets(buf, sizeof(buf), in);
	if(!line)
	  break;
	if(line[0]=='#')
	  continue;
	end=strchr(line, '\n');
	if(end)
	  *end=0;

	sep=strchr(line, ':');
	if(sep) {
	  char *rep;
	  Alarm *nalarm;
	  
	  *sep=0;
	  rep=sep+1;

	  sep=strchr(rep, ':');

	  if(sep) {
	    *sep=0;
	    
	    nalarm=alarm_new((time_t) atol(line), atoi(rep), sep+1);
	  } else {
	    nalarm=alarm_new((time_t) atol(line), REPEAT_NONE, rep);
	  }
	  dprintf(3, "read alarm: %s at %ld (%d)", nalarm->message,
		  nalarm->when, nalarm->repeat);
	  alarms=g_list_append(alarms, nalarm);
	}
	
      } while(!feof(in));
      
      fclose(in);
#ifdef HAVE_STAT
      if(stat(fname, &statb)==0)
	alarms_saved=statb.st_mtime;
      else
	time(&alarms_saved);
#else
      time(&alarms_saved);
#endif
    }

    g_free(fname);
  }
}

static time_t next_alarm_time(time_t when, RepeatMode mode)
{
  time_t next;
  struct tm *tms;

  if(mode==REPEAT_NONE)
    return (time_t) 0;

  tms=localtime(&when);

  switch(mode) {
  case REPEAT_HOURLY:
    tms->tm_hour++;
    break;

  case REPEAT_DAILY:
    tms->tm_mday++;
    break;

  case REPEAT_WEEKDAYS:
    switch(tms->tm_wday) {
    case 0: case 1: case 2: case 3: case 4:
      tms->tm_mday++;
      break;
    case 5:
      tms->tm_mday+=3;
      break;
    case 6:
      tms->tm_mday+=2;
      break;
    }
    break;

  case REPEAT_WEEKLY:
    tms->tm_mday+=7;
    break;

  case REPEAT_MONTHLY:
    tms->tm_mon++;
    if(tms->tm_mon==12) {
      tms->tm_mon=0;
      tms->tm_year++;
    }
    break;

  case REPEAT_YEARLY:
    tms->tm_year++;
    break;

  default:
    return (time_t) -1;
  }

  next=mktime(tms);

  return next;
}

static gint find_alarm(gconstpointer el, gconstpointer udat)
{
  const Alarm *elem=(const Alarm *) el;
  const Alarm *user=(const Alarm *) udat;

  dprintf(4, "compare: %ld %d %s", elem->when, elem->repeat, elem->message);
  dprintf(4, "     to: %ld %d %s", user->when, user->repeat, user->message);

  if(elem->when==user->when && elem->repeat==user->repeat &&
     strcmp(elem->message, user->message)==0)
    return 0;
  dprintf(5, " nope");

  return -1;
}

static void check_alarms_file(void)
{
  gchar *fname;
  
  fname=choices_find_path_load("alarms", PROJECT);
  dprintf(5, "%p: %s", fname, fname? fname: "NULL");

  if(fname) {
#ifdef HAVE_SYS_STAT_H
    struct stat statb;
#endif

#ifdef HAVE_STAT
    if(stat(fname, &statb)==0) {
      if(statb.st_mtime>alarms_saved) {
	GList *old, *rover;

	dprintf(2, "re-reading alarm file because %ld>%ld",
		statb.st_mtime, alarms_saved);
	old=alarms;
	alarms=NULL;
	alarm_load();
	alarms_saved=statb.st_mtime; /* May have clock sync problems over
				      a networked FS */

	for(rover=old; rover; rover=g_list_next(rover)) {
	  if(g_list_find_custom(alarms, rover->data, find_alarm)) {
	    dprintf(3, "delete old %s", ((Alarm *) rover->data)->message);
	    alarm_delete((Alarm *) rover->data);
	  } else {
	    dprintf(3, "merge in %s", ((Alarm *) rover->data)->message);
	    alarms=g_list_append(alarms, rover->data);
	  }
	}

	g_list_free(old);
      }
    }
#endif
    
    g_free(fname);
  }
}

void alarm_save(void)
{
  gchar *fname;
#ifdef HAVE_SYS_STAT_H
  struct stat statb;
#endif

  fname=choices_find_path_save("alarms", PROJECT, TRUE);
  dprintf(2, "Save alarms to %s", fname? fname: "NULL");

  if(fname) {
    FILE *out;

    out=fopen(fname, "w");
    if(out) {
      time_t now;
      char buf[80];
      GList *rover;
      
      fprintf(out, _("# Alarms file for %s %s (%s)\n"), PROJECT, VERSION,
	      AUTHOR);
      fprintf(out, _("# Latest version at %s\n"), WEBSITE);
      time(&now);
      strftime(buf, 80, "%c", localtime(&now));
      fprintf(out, _("#\n# Written %s\n\n"), buf);
      dprintf(3, "version %s, write at %s", VERSION, buf);

      for(rover=alarms; rover; rover=g_list_next(rover)) {
	Alarm *alarm=(Alarm *) rover->data;
	fprintf(out, "%ld:%d:%s\n", alarm->when, alarm->repeat,
		alarm->message);
	dprintf(3, "wrote %s %ld %d", alarm->message, alarm->when,
		alarm->repeat);
      }

      fclose(out);
#ifdef HAVE_STAT
      if(stat(fname, &statb)==0)
	alarms_saved=statb.st_mtime;
      else
	time(&alarms_saved);
#else
      time(&alarms_saved);
#endif
      dprintf(3, "wrote %s at %ld", fname, alarms_saved);
    }
    g_free(fname);
  }
}

static void dismiss(GtkWidget *wid, gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);

  gtk_widget_hide(win);
}

static void remove_window(GtkWidget *wid, gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);

  gtk_widget_hide(win);
  /*gtk_widget_unref(win);*/
  gtk_widget_destroy(win);
}

static void show_message(const Alarm *alarm)
{
  GtkWidget *win;
  GtkWidget *label;
  GtkWidget *but;
  GtkWidget *vbox;
  GtkWidget *hbox;
  char buf[64];
  struct tm *tms;
  
  win=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(win), _("Alarm"));
  
  vbox=GTK_DIALOG(win)->vbox;

  tms=localtime(&alarm->when);
  strftime(buf, 64, "%c", tms);
  dprintf(3, "show_message(%p): %ld (%d) %s %s", alarm, alarm->when,
	  alarm->repeat, buf, alarm->message);
  
  label=gtk_label_new(buf);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

  label=gtk_label_new(alarm->message);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 2);

  hbox=GTK_DIALOG(win)->action_area;

  but=gtk_button_new_with_label(_("Dismiss"));
  gtk_widget_show(but);
  gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     GTK_SIGNAL_FUNC(remove_window), win);

  gtk_widget_show(win);
  gdk_beep();
}

int alarm_check(void)
{
  GList *rover;
  time_t now;
  int nraise=0;

  check_alarms_file();

  time(&now);
  
  for(rover=alarms; rover; rover=g_list_next(rover)) {
    Alarm *alarm=(Alarm *) rover->data;

    if(now>alarm->when) {
      time_t next;
      
      show_message(alarm);
      nraise++;

      alarms=g_list_remove_link(alarms, rover);
      if(alarm->repeat!=REPEAT_NONE) {
	next=alarm->when;
	do {
	  next=next_alarm_time(next, alarm->repeat);
	  if(next<=0)
	    break;
	} while(next<now);
	if(next>0) {
	  dprintf(3, "re-schedule %s for %ld", alarm->message, next);
	  alarms=g_list_append(alarms, alarm_new(next, alarm->repeat,
						 alarm->message));
	}
      }
      alarm_delete(alarm);
      rover=alarms;
      continue;
    }
  }

  return nraise;
}

/* Make a destroy-frame into a close */
static int trap_frame_destroy(GtkWidget *widget, GdkEvent *event,
			      gpointer data)
{
  /* Change this destroy into a hide */
  gtk_widget_hide(GTK_WIDGET(data));
  return TRUE;
}

/* Widgets on alarms window */
static GtkWidget *day;
static GtkWidget *month;
static GtkWidget *year;
static GtkWidget *hour;
static GtkWidget *minute;
/* Seconds == 0 */
static GtkWidget *message;
static GtkWidget *list;
static GtkWidget *repmode;
static GtkWidget *delalarm;

static void set_alarm(GtkWidget *wid, gpointer data)
{
  GtkWidget *win=GTK_WIDGET(data);
  struct tm tms;
  RepeatMode repeat;
  GtkWidget *menu;
  GtkWidget *item;
  gchar *mess;

  tms.tm_sec=0;
  tms.tm_min=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minute));
  tms.tm_hour=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hour));
  tms.tm_mday=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(day));
  tms.tm_mon=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(month))-1;
  tms.tm_year=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(year))-1900;
  tms.tm_isdst=-1;

  menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(repmode));
  item=gtk_menu_get_active(GTK_MENU(menu));
  repeat=(RepeatMode) gtk_object_get_data(GTK_OBJECT(item), "mode");

  mess=gtk_entry_get_text(GTK_ENTRY(message));

  dprintf(3, "New alarm: %s at %d/%d/%d %d:%d repeat %d", mess,
	  tms.tm_mday, tms.tm_mon+1, tms.tm_year+1900, tms.tm_hour, tms.tm_min,
	  repeat);
	  
  alarms=g_list_append(alarms, alarm_new(mktime(&tms), repeat, mess));

  gtk_widget_hide(win);

  dprintf(3, "Save updated alarm file");
  alarm_save();
}

static void delete_alarm(GtkWidget *wid, gpointer data)
{
  int row;
  Alarm *alarm;
  
  row=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(delalarm), "row"));
  alarm=(Alarm *) gtk_clist_get_row_data(GTK_CLIST(list), row);

  gtk_clist_remove(GTK_CLIST(list), row);
  alarms=g_list_remove(alarms, alarm);
  dprintf(3, "delete alarm: %s at %d repeat %d", alarm->message, alarm->when,
	  alarm->repeat);
  alarm_delete(alarm);

  alarm_save();
}

static void set_the_time(time_t *when)
{
  struct tm *tms;
  
  tms=localtime(when);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(hour), tms->tm_hour);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(minute), tms->tm_min);
  
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(day), tms->tm_mday);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(month), tms->tm_mon+1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(year), tms->tm_year+1900);
}

static void alarm_sel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data)
{
  Alarm *alarm;

  alarm=(Alarm *) gtk_clist_get_row_data(GTK_CLIST(clist), row);

  gtk_entry_set_text(GTK_ENTRY(message), alarm->message);
  set_the_time(&alarm->when);
  gtk_option_menu_set_history(GTK_OPTION_MENU(repmode), alarm->repeat);
  
  gtk_widget_set_sensitive(delalarm, TRUE);
  gtk_object_set_data(GTK_OBJECT(delalarm), "row", GINT_TO_POINTER(row));
}

static void alarm_unsel(GtkWidget *clist, gint row, gint column,
		    GdkEvent *event, gpointer data)
{
  gtk_widget_set_sensitive(delalarm, FALSE);
}

static GtkWidget *make_alarm_window(void)
{
  GtkWidget *win;
  GtkWidget *label;
  GtkWidget *but;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *scrw;
  GtkWidget *menu;
  GtkWidget *item;
  GtkObject *adj;
  GtkRequisition req;
  int mw=0, mh=0;
  int i;

  static gchar *titles[3]={N_("Time"), N_("Repeat"), N_("Message")};
  
  win=gtk_dialog_new();
  gtk_signal_connect(GTK_OBJECT(win), "delete_event", 
		     GTK_SIGNAL_FUNC(trap_frame_destroy), 
		     win);
  gtk_window_set_title(GTK_WINDOW(win), _("Alarms"));
  
  vbox=GTK_DIALOG(win)->vbox;

  scrw=gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrw),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_show(scrw);
  gtk_widget_set_usize(scrw, 320, 100);
  gtk_box_pack_start(GTK_BOX(vbox), scrw, TRUE, TRUE, 2);

  list=gtk_clist_new_with_titles(3, titles);
  gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_SINGLE);
  gtk_clist_set_column_width(GTK_CLIST(list), 0, 100);
  gtk_clist_set_column_width(GTK_CLIST(list), 1, 64);
  gtk_signal_connect(GTK_OBJECT(list), "select_row",
		       GTK_SIGNAL_FUNC(alarm_sel),
		       GINT_TO_POINTER(TRUE));
  gtk_signal_connect(GTK_OBJECT(list), "unselect_row",
		       GTK_SIGNAL_FUNC(alarm_unsel),
		       GINT_TO_POINTER(FALSE));
  gtk_widget_show(list);
  gtk_container_add(GTK_CONTAINER(scrw), list);
  
  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label=gtk_label_new(_("Date (dd/mm/yy)"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  adj=gtk_adjustment_new(1, 1, 31, 1, 1, 1);
  day=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_widget_show(day);
  gtk_box_pack_start(GTK_BOX(hbox), day, FALSE, FALSE, 2);
    
  label=gtk_label_new(_("/"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    
  adj=gtk_adjustment_new(1, 1, 12, 1, 1, 1);
  month=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_widget_show(month);
  gtk_box_pack_start(GTK_BOX(hbox), month, FALSE, FALSE, 2);
    
  label=gtk_label_new(_("/"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    
  adj=gtk_adjustment_new(2001, 2001, 2037, 1, 10, 0);
  year=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_widget_show(year);
  gtk_box_pack_start(GTK_BOX(hbox), year, TRUE, TRUE, 2);
    
  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label=gtk_label_new(_("Time (HH:MM)"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  adj=gtk_adjustment_new(12, 0, 23, 1, 1, 1);
  hour=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_widget_show(hour);
  gtk_box_pack_start(GTK_BOX(hbox), hour, FALSE, FALSE, 2);
    
  label=gtk_label_new(_(":"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
    
  adj=gtk_adjustment_new(0, 0, 59, 1, 10, 10);
  minute=gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_widget_show(minute);
  gtk_box_pack_start(GTK_BOX(hbox), minute, FALSE, FALSE, 2);
  
  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label=gtk_label_new(_("Message"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  message=gtk_entry_new();
  gtk_widget_show(message);
  gtk_box_pack_start(GTK_BOX(hbox), message, TRUE, TRUE, 2);    

  hbox=gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  label=gtk_label_new(_("Repeat alarm"));
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  repmode=gtk_option_menu_new();
  gtk_widget_show(repmode);
  gtk_box_pack_start(GTK_BOX(hbox), repmode, FALSE, FALSE, 2);

  menu=gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(repmode), menu);

  for(i=0; repeat_text[i]; i++) {
    item=gtk_menu_item_new_with_label(_(repeat_text[i]));
    gtk_object_set_data(GTK_OBJECT(item), "mode", GINT_TO_POINTER(i));
    gtk_widget_show(item);
    gtk_widget_size_request(item, &req);
    if(mw<req.width)
      mw=req.width;
    if(mh<req.height)
      mh=req.height;
    gtk_menu_append(GTK_MENU(menu), item);
  }
  gtk_widget_set_usize(repmode, mw+50, mh+4);
  gtk_option_menu_set_history(GTK_OPTION_MENU(repmode), REPEAT_NONE);
    
  hbox=GTK_DIALOG(win)->action_area;

  but=gtk_button_new_with_label(_("Set alarm"));
  gtk_widget_show(but);
  gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     GTK_SIGNAL_FUNC(set_alarm), win);

  but=gtk_button_new_with_label(_("Delete alarm"));
  gtk_widget_show(but);
  gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     GTK_SIGNAL_FUNC(delete_alarm), win);
  delalarm=but;

  but=gtk_button_new_with_label(_("Dismiss"));
  gtk_widget_show(but);
  gtk_box_pack_start(GTK_BOX(hbox), but, FALSE, FALSE, 2);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     GTK_SIGNAL_FUNC(dismiss), win);

  return win;
}

static void update_alarm_window(GtkWidget *win)
{
  time_t now;
  GList *rover;

  time(&now);
  set_the_time(&now);
  gtk_entry_set_text(GTK_ENTRY(message), "");
  gtk_option_menu_set_history(GTK_OPTION_MENU(repmode), REPEAT_NONE);

  g_return_if_fail(GTK_IS_CLIST(list));
  gtk_clist_freeze(GTK_CLIST(list));
  gtk_clist_clear(GTK_CLIST(list));
  for(rover=alarms; rover; rover=g_list_next(rover)) {
    Alarm *alarm=(Alarm *) rover->data;
    struct tm *tms;
    char buf[80];
    gchar *text[3];
    gint row;

    tms=localtime(&alarm->when);
    strftime(buf, 80, "%R %x", tms);

    text[0]=buf;
    text[1]=(gchar *) repeat_text[alarm->repeat];
    text[2]=alarm->message;
    row=gtk_clist_append(GTK_CLIST(list), text);
    gtk_clist_set_row_data(GTK_CLIST(list), row, alarm);
  }
  gtk_clist_thaw(GTK_CLIST(list));

  gtk_widget_set_sensitive(delalarm, FALSE);
}

void alarm_show_window(void)
{
  static GtkWidget *al_win=NULL;

  if(!al_win)
    al_win=make_alarm_window();

  update_alarm_window(al_win);

  gtk_widget_show(al_win);
}


/*
 * $Log: alarm.c,v $
 * Revision 1.5  2001/06/29 10:42:48  stephen
 * Added debugging statements, fixed merging in another alarms file
 * (which was g_free'ing memory twice!) and don't do it so often.
 *
 * Revision 1.4  2001/06/14 12:29:32  stephen
 * return number of alarms raised so that the caller knows something has
 * changed.
 *
 * Revision 1.3  2001/05/24 09:10:04  stephen
 * Fixed bug in repeating alarms
 *
 * Revision 1.2  2001/05/16 11:02:17  stephen
 * Added repeating alarms.
 * Menu supported on applet (compile time option).
 *
 * Revision 1.1  2001/05/10 14:54:27  stephen
 * Added new alarm feature
 *
 */

