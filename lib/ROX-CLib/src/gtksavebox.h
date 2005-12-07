/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Now based on the ROX-Filer 1.3.5 version:
 * gtksavebox.h,v 1.7 2002/05/02 14:46:07 tal197
 */

/**
 * @file gtksavebox.h
 * @brief Drag and drop saving widget for GTK+
 *
 * @author Thomas Leonard
 * @version $Id: gtksavebox.h,v 1.3 2005/08/21 13:06:38 stephen Exp $
 */

#ifndef __GTK_SAVEBOX_H__
#define __GTK_SAVEBOX_H__


#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkselection.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** This is for the 'info' value of the GtkTargetList.
 * It's for the XdndDirectSave0 target - ignore requests for this target
 * because they're handled internally by the widget. Don't use this
 * value for anything else!
 */
#define GTK_TARGET_XDS 0x584453

/** @return type code for GtkSavebox */
#define GTK_TYPE_SAVEBOX		(gtk_savebox_get_type ())

/** Cast pointer into a pointer to GtkSavebox only if it is valid,
 * otherwise return @c NULL
 * @param[in] obj pointer to object
 * @return pointer to GtkSavebox or @c NULL for error
 */
#define GTK_SAVEBOX(obj)		\
	(GTK_CHECK_CAST ((obj), GTK_TYPE_SAVEBOX, GtkSavebox))

/** Cast pointer into a pointer to GtkSavebox class only if it is valid,
 * otherwise return @c NULL
 * @param[in] klass pointer to object class
 * @return pointer to GtkSavebox class or @c NULL for error
 */
#define GTK_SAVEBOX_CLASS(klass)	\
	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SAVEBOX, GtkSaveboxClass))

/** Check a pointer to GtkSavebox
 * @param[in] obj pointer to object
 * @return non-zero if a pointer to a GtkSavebox
 */
#define GTK_IS_SAVEBOX(obj)	(GTK_CHECK_TYPE ((obj), GTK_TYPE_SAVEBOX))

/** Check a pointer to a GtkSavebox class
 * @param[in] klass pointer to object class
 * @return non-zero if a pointer to a GtkSavebox class
 */
#define GTK_IS_SAVEBOX_CLASS(klass)	\
	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SAVEBOX))


typedef struct _GtkSavebox        GtkSavebox;
typedef struct _GtkSaveboxClass   GtkSaveboxClass;
typedef struct _GtkSaveboxButton  GtkSaveboxButton;

enum {
  GTK_XDS_SAVED,		/**< Done the save - no problem */
  GTK_XDS_SAVE_ERROR,		/**< Error during save - reported */
  GTK_XDS_NO_HANDLER,		/**< Used internally (sanity checking) */
};

/**
 * GTK+ widget used for drag and drop saving.
 */
struct _GtkSavebox
{
  GtkDialog dialog;		/**< Instance of parent class */

  GtkWidget *discard_area;	/**< Normally hidden */
  GtkWidget *drag_box;		/**< Event box - contains pixmap, or @c NULL */
  GtkWidget *icon;		/**< The pixmap widget */
  GtkWidget *entry;		/**< Where the pathname goes */
  GtkWidget *extend;            /**< Extension area, or @c NULL */

  GtkTargetList *targets;	/**< Formats that we can save in */
  gboolean  using_xds;		/**< Have we sent XDS reply 'S' or 'F' yet? */
  gboolean  data_sent;		/**< Did we send any data at all this drag? */
};

/**
 * @internal Class of GTK+ widget used for drag and drop saving.
 */
struct _GtkSaveboxClass
{
  GtkDialogClass parent_class;   /**< Parent class */

  gint (*save_to_file)	(GtkSavebox *savebox, guchar *pathname);
  void (*saved_to_uri)	(GtkSavebox *savebox, guchar *uri);
};


GType	   gtk_savebox_get_type 	(void);
GtkWidget* gtk_savebox_new		(const gchar *action);
void	   gtk_savebox_set_icon		(GtkSavebox *savebox,
					 GdkPixbuf *pixbuf);
void	   gtk_savebox_set_pathname	(GtkSavebox *savebox,
					 const gchar *pathname);
void	   gtk_savebox_set_has_discard	(GtkSavebox *savebox, gboolean setting);
GtkWidget* gtk_savebox_get_extension_area(GtkSavebox *savebox);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SAVEBOX_H__ */

/*
 * $Log: gtksavebox.h,v $
 * Revision 1.3  2005/08/21 13:06:38  stephen
 * Added doxygen comments
 *
 */
  
