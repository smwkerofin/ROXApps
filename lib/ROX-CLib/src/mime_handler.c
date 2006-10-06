/*
 * Install handlers for MIME types
 *
 * $Id: mime_handler.c,v 1.5 2006/08/12 17:04:56 stephen Exp $
 */

/**
 * @file mime_handler.c
 * @brief Install handlers for MIME types.
 *
 * This provides the facility for a ROX application to install itself as
 * the handler for a set of MIME types in a standardized way.  The user is
 * asked to confirm any changes to their set up.
 *
 * @author Stephen Watson
 * @version $Id: mime_handler.c,v 1.5 2006/08/12 17:04:56 stephen Exp $
 */

#include "rox-clib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include "mime_handler.h"

#define DEBUG 1
#include "rox.h"
#include "rox_debug.h"
#include "mime.h"
#include "appinfo.h"

static void load_types(GtkWidget *win, GList *types);
static void set_active(GtkWidget *win);
static void set_uninstall(GtkWidget *win);
static void install_toggled(GtkCellRendererToggle *cell, gchar *path,
			    gpointer udata);
static void uninstall_toggled(GtkCellRendererToggle *cell, gchar *path,
			    gpointer udata);

#define PYTHON_CODE "import os;import findrox;findrox.version(1,9,13);" \
                    "import rox,rox.mime_handler;" \
                    "rox.mime_handler.install_from_appinfo(os.getenv('APP_DIR'))"

#define ROX_DOMAIN "rox.sourceforge.net"

static void install_from_appinfo_python(void)
{
  gchar *cmd;

  cmd=g_strdup_printf("python -c \"%s\" &", PYTHON_CODE);
  system(cmd);
  g_free(cmd);
}

enum {TNAME, COMMENT, CURRENT, INSTALL, ICON, UNINSTALL, IS_OURS};

static void destroy_win(GtkWidget *win, gpointer udata)
{
  GList *list;

  list=g_object_get_data(G_OBJECT(win), "install-list");
  g_list_free(list);
  list=g_object_get_data(G_OBJECT(win), "uninstall-list");
  g_list_free(list);
}

static void update_lists(GtkWidget *win, gint resp, gpointer udata)
{
  if(resp==GTK_RESPONSE_ACCEPT) {
    set_active(win);
    set_uninstall(win);
  }
}

static GtkWidget *make_install_list(const gchar *app_dir,
				    const gchar *itype,
				    const gchar *dir,
				    GList *types,
				    const gchar *info,
				    gboolean check)
{
  GtkWidget *win;
  gchar *title;
  GtkWidget *vbox;
  GtkWidget *swin;
  GtkListStore *store;
  GtkTreeModel *model;
  GtkWidget *view;
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;
  GValue val={0}, *gv_true;

  title=g_strdup_printf(_("Install %s"), itype);
  win=gtk_dialog_new_with_buttons(title, NULL, GTK_DIALOG_MODAL,
				  GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE,
				  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				  NULL);

  vbox=GTK_DIALOG(win)->vbox;

  swin=gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_size_request(swin, -1, 160);
  gtk_container_set_border_width(GTK_CONTAINER(swin), 4);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				 GTK_POLICY_NEVER,  GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin),
				      GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

  store=gtk_list_store_new(7, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			   G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN,
			   G_TYPE_BOOLEAN);
  model=(GtkTreeModel *) store;
  view=gtk_tree_view_new_with_model(model);
  gtk_container_add(GTK_CONTAINER(swin), view);
  gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), 1);

#if 0
  cell=gtk_cell_renderer_pixbuf_new();
  column=gtk_tree_view_column_new_with_attributes("", cell,
						  "pixbuf", ICON, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
#endif
  
  cell=gtk_cell_renderer_text_new();
  column=gtk_tree_view_column_new_with_attributes(_("Type"), cell,
						  "text", TNAME, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
  gtk_tree_view_column_set_sort_column_id(column, TNAME);

  cell=gtk_cell_renderer_text_new();
  column=gtk_tree_view_column_new_with_attributes(_("Name"), cell,
						  "text", COMMENT, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
  gtk_tree_view_column_set_sort_column_id(column, COMMENT);

  if(check) {
    cell=gtk_cell_renderer_text_new();
    column=gtk_tree_view_column_new_with_attributes(_("Current"), cell,
						    "text", CURRENT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
    gtk_tree_view_column_set_sort_column_id(column, CURRENT);
  }

  gv_true=g_value_init(&val, G_TYPE_BOOLEAN);
  g_value_set_boolean(gv_true, TRUE);

  cell=gtk_cell_renderer_toggle_new();
  g_object_set_property(G_OBJECT(cell), "activatable", gv_true);
  g_signal_connect(cell, "toggled", G_CALLBACK(install_toggled), model);
  column=gtk_tree_view_column_new_with_attributes(_("Install?"), cell,
						  "active", INSTALL, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
  gtk_tree_view_column_set_sort_column_id(column, INSTALL);

  cell=gtk_cell_renderer_toggle_new();
  g_object_set_property(G_OBJECT(cell), "activatable", gv_true);
  g_signal_connect(cell, "toggled", G_CALLBACK(uninstall_toggled), model);
  column=gtk_tree_view_column_new_with_attributes(_("Uninstall?"), cell,
						  "active", UNINSTALL,
						  "activatable", IS_OURS,
						  NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
  gtk_tree_view_column_set_sort_column_id(column, UNINSTALL);

  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
			      GTK_SELECTION_NONE);

  if(info) {
    GtkWidget *hbox, *img, *lbl;

    hbox=gtk_hbox_new(FALSE, 4);
    img=gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
				 GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

    lbl=gtk_label_new(info);
    gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  }

  gtk_widget_show_all(vbox);

  g_object_set_data(G_OBJECT(win), "check", GINT_TO_POINTER(check));
  g_object_set_data(G_OBJECT(win), "dir", (gpointer) dir);
  g_object_set_data(G_OBJECT(win), "app_dir", (gpointer) app_dir);
  g_object_set_data(G_OBJECT(win), "types", types);
  g_object_set_data(G_OBJECT(win), "view", view);
  g_object_set_data(G_OBJECT(win), "model", model);
  g_object_set_data(G_OBJECT(win), "install-list", NULL);
  g_object_set_data(G_OBJECT(win), "uninstall-list", NULL);

  g_signal_connect(win, "destroy", G_CALLBACK(destroy_win), NULL);
  g_signal_connect(win, "response", G_CALLBACK(update_lists), NULL);

  load_types(win, types);

  return win;
}

static void load_types(GtkWidget *win, GList *types)
{
  GtkTreeModel *model;
  gboolean check;
  GList *rover;
  ROXMIMEType *type;
  const gchar *dir;
  const gchar *app_dir;
  GtkTreeIter iter;
  gchar *tname;

  model=g_object_get_data(G_OBJECT(win), "model");
  check=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(win), "check"));
  dir=g_object_get_data(G_OBJECT(win), "dir");
  app_dir=g_object_get_data(G_OBJECT(win), "app_dir");

  gtk_list_store_clear(GTK_LIST_STORE(model));

  for(rover=types; rover; rover=g_list_next(rover)) {
    gboolean dinstall, can_un;
    gchar *oname;
    GdkPixbuf *icon;
    
    type=rover->data;

    if(check) {
      gchar *old, *leaf;

      leaf=g_strdup_printf("%s_%s", type->media, type->subtype);
      old=rox_choices_load(leaf, dir, ROX_DOMAIN);
      g_free(leaf);

      if(old) {
	struct stat buf;

	if(lstat(old, &buf)==0 && S_ISLNK(buf.st_mode)) {
	  gchar *tmp=g_new(gchar, 4096);
	  int n;

	  n=readlink(old, tmp, 4096);
	  tmp[n]=0;
	  g_free(old);
	  old=tmp;
	  oname=g_path_get_basename(old);
	  
	} else {
	  oname=g_strdup("script");
	}

	if(strcmp(old, app_dir)==0) {
	  dinstall=FALSE;
	  can_un=TRUE;
	} else {
	  dinstall=TRUE;
	  can_un=FALSE;
	}

	g_free(old);
      } else {
	dinstall=TRUE;
	can_un=FALSE;
	oname=g_strdup("");
      }
    } else {
      dinstall=TRUE;
      can_un=FALSE;
      oname=g_strdup("");
    }

    icon=NULL;
    tname=rox_mime_type_name(type);

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, TNAME, tname,
		       COMMENT, rox_mime_type_comment(type),
		       INSTALL, dinstall, UNINSTALL, FALSE,
		       IS_OURS, can_un, -1);
    if(check)
      gtk_list_store_set(GTK_LIST_STORE(model), &iter, CURRENT, oname, -1);
    if(icon)
      gtk_list_store_set(GTK_LIST_STORE(model), &iter, ICON, icon, -1);
  }
}

static void set_active(GtkWidget *win)
{
  ROXMIMEType *type;
  GList *types=NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkListStore *store;

  types=g_object_get_data(G_OBJECT(win), "install-list");
  if(types) {
    g_list_free(types);
    types=NULL;
  }

  model=g_object_get_data(G_OBJECT(win), "model");
  store=GTK_LIST_STORE(model);

  if(!gtk_tree_model_get_iter_first(model, &iter)) {
    g_object_set_data(G_OBJECT(win), "install-list", NULL);
    return;
  }

  do {
    GValue ins={0}, name={0};

    gtk_tree_model_get_value(model, &iter, INSTALL, &ins);
    if(g_value_get_boolean(&ins)) {
      gtk_tree_model_get_value(model, &iter, TNAME, &name);
      type=rox_mime_lookup_by_name(g_value_get_string(&name));
      rox_debug_printf(3, " type=%s", g_value_get_string(&name));
      types=g_list_append(types, type);
      g_value_unset(&name);
    }
    g_value_unset(&ins);
    
  } while(gtk_tree_model_iter_next(model, &iter));

  g_object_set_data(G_OBJECT(win), "install-list", types);
}

static void set_uninstall(GtkWidget *win)
{
  ROXMIMEType *type;
  GList *types=NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkListStore *store;

  types=g_object_get_data(G_OBJECT(win), "uninstall-list");
  if(types) {
    g_list_free(types);
    types=NULL;
  }

  model=g_object_get_data(G_OBJECT(win), "model");
  store=GTK_LIST_STORE(model);

  if(!gtk_tree_model_get_iter_first(model, &iter)) {
    g_object_set_data(G_OBJECT(win), "uninstall-list", NULL);
    return;
  }

  do {
    GValue uins={0}, name={0};

    gtk_tree_model_get_value(model, &iter, UNINSTALL, &uins);
    if(g_value_get_boolean(&uins)) {
      gtk_tree_model_get_value(model, &iter, TNAME, &name);
      type=rox_mime_lookup_by_name(g_value_get_string(&name));
      rox_debug_printf(3, " type=%s", g_value_get_string(&name));
      types=g_list_append(types, type);
      g_value_unset(&name);
    }
    g_value_unset(&uins);
    
  } while(gtk_tree_model_iter_next(model, &iter));
	  
  g_object_set_data(G_OBJECT(win), "uninstall-list", types);
}

static void install_toggled(GtkCellRendererToggle *cell, gchar *path,
			    gpointer udata)
{
  GtkTreeIter iter;
  GtkTreeModel *model=GTK_TREE_MODEL(udata);
  int active;

  gtk_tree_model_get_iter_from_string(model, &iter, path);

  active=gtk_cell_renderer_toggle_get_active(cell);
  gtk_list_store_set(GTK_LIST_STORE(model), &iter, INSTALL, !active, -1);
  if(!active)
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, UNINSTALL, 0, -1);
}

static void uninstall_toggled(GtkCellRendererToggle *cell, gchar *path,
			    gpointer udata)
{
  GtkTreeIter iter;
  GtkTreeModel *model=GTK_TREE_MODEL(udata);
  int active;
  GValue avail={0};

  gtk_tree_model_get_iter_from_string(model, &iter, path);

  active=gtk_cell_renderer_toggle_get_active(cell);
  gtk_tree_model_get_value(model, &iter, IS_OURS, &avail);
  if(g_value_get_boolean(&avail)) {
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, UNINSTALL, !active, -1);
    if(!active)
      gtk_list_store_set(GTK_LIST_STORE(model), &iter, INSTALL, 0, -1);
  } else
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, UNINSTALL, 0, -1);

  g_value_unset(&avail);
}

static void install_type_handler(GList *types, const gchar *dir,
				 const gchar *desc, const gchar *app_dir,
				 gboolean overwrite, const gchar *info)
{
  GtkWidget *win;
  int resp;
  GList *atypes, *rover;
  gchar *path, *leaf;
  ROXMIMEType *mtype;

  win=make_install_list(app_dir, desc, dir, types, info, TRUE);

  resp=gtk_dialog_run(GTK_DIALOG(win));
  if(resp!=GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(win);
    return;
  }

  atypes=g_object_get_data(G_OBJECT(win), "install-list");
  for(rover=atypes; rover; rover=g_list_next(rover)) {
    mtype=(ROXMIMEType *) rover->data;
    leaf=g_strdup_printf("%s_%s", mtype->media, mtype->subtype);
    path=rox_choices_save(leaf, dir, ROX_DOMAIN);
    rox_debug_printf(3, "rover=%p leaf=%s path=%s", rover, leaf, path);
    if(path) {
      if(access(path, F_OK)==0)
	if(remove(path)!=0) {
	  rox_error(_("Failed to replace %s"), path);
	  g_free(path);
	  continue;
	}
      if(symlink(app_dir, path)!=0)
	rox_error(_("Failed to install %s as %s"), app_dir, path);
      g_free(path);
    }
    g_free(leaf);
  }
  /*g_list_free(atypes);*/

  atypes=g_object_get_data(G_OBJECT(win), "uninstall-list");
  for(rover=atypes; rover; rover=g_list_next(rover)) {
    mtype=(ROXMIMEType *) rover->data;
    leaf=g_strdup_printf("%s_%s", mtype->media, mtype->subtype);
    path=rox_choices_save(leaf, dir, ROX_DOMAIN);
    rox_debug_printf(3, "u rover=%p leaf=%s path=%s", rover, leaf, path);
    if(path) {
      if(remove(path)!=0)
	rox_error(_("Failed to remove %s"), path);
      g_free(path);
    }
    g_free(leaf);    
  }
  /*g_list_free(atypes);*/

  gtk_widget_destroy(win);
}

static void install_run_action(GList *types, const gchar *app_dir,
			       gboolean overwrite)
{
  install_type_handler(types, "MIME-types", _("run action"),
		       app_dir, overwrite,
		       _("Run actions can be changed by selecting a "
			 "file of the appropriate type in the Filer and "
			 "selecting the menu option 'Set Run Action...'"));
}

static void install_thumbnailer(GList *types, const gchar *app_dir,
			       gboolean overwrite)
{
  install_type_handler(types, "MIME-thumb", _("thumbnail handler"),
		       app_dir, overwrite,
		       _("Thumbnail handlers provide support for creating "
			 "thumbnail images of types of file.  The filer can "
			 "generate thumbnails for most types of image (JPEG, "
			 "PNG, etc.) but relies on helper applications for "
			 "the others."));
}

static void install_send_to_types(GList *types, const gchar *app_dir)
{
  GtkWidget *win;
  int resp;
  GList *atypes, *rover;
  gchar *path, *aname, *dir;
  ROXMIMEType *mtype;
  
  win=make_install_list(app_dir, _("type handler"), "SendTo", types,
			_("The application can handle files of these types.  "
			  "Click on OK to add it to the SendTo menu for the "
			  "type of file, and also the customized File menu."),
			FALSE);

  resp=gtk_dialog_run(GTK_DIALOG(win));
  if(resp!=GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(win);
    return;
  }

  aname=g_path_get_basename(app_dir);
  
  atypes=g_object_get_data(G_OBJECT(win), "install-list");
  for(rover=atypes; rover; rover=g_list_next(rover)) {
    mtype=(ROXMIMEType *) rover->data;
    dir=g_strdup_printf("SendTo/.%s_%s", mtype->media, mtype->subtype);
    path=rox_choices_save(aname, dir, ROX_DOMAIN);
    if(path) {
      if(access(path, F_OK)==0) 
	if(remove(path)!=0) {
	  rox_error(_("Failed to replace %s"), path);
	  g_free(path);
	  continue;
	}
      if(symlink(app_dir, path)!=0)
	rox_error(_("Failed to install %s as %s"), app_dir, path);
    }
    g_free(dir);
  }
  
  g_free(aname);

  gtk_widget_destroy(win);
}

static void install_from_appinfo_c(void)
{
  const gchar *app_dir;
  GList *types;
  ROXAppInfo *ainfo;

  app_dir=rox_get_app_dir();
  ainfo=ROX_APPINFO(rox_appinfo_new());

  types=rox_appinfo_get_can_run_list(ainfo);
  if(types) {
    install_run_action(types, app_dir, TRUE);
    install_send_to_types(types, app_dir);

    g_list_free(types);
  }

  types=rox_appinfo_get_can_thumbnail_list(ainfo);
  if(types) {
    install_thumbnailer(types, app_dir, TRUE);

    g_list_free(types);
  }

  g_object_unref(ainfo);
}

/**
 * Scan the applications AppInfo.xml file for CanRun and CanThumbnail
 * declarations.  If any are found they are reported to the user who is asked
 * whether the application can be set as the current handler for those types.
 *
 * Example AppInfo.xml (from VideoThumbnail):
 * <pre>
 * &lt;AppInfo&gt;
 * ...
 *   &lt;CanThumbnail&gt;
 *     &lt;MimeType type="video/mpeg"/&gt;
 *     &lt;MimeType type="video/quicktime"/&gt;
 *     &lt;MimeType type="video/x-msvideo"/&gt;
 *   &lt;/CanThumbnail&gt;
 * &lt;/AppInfo&gt; 
 * </pre>
 * 
 */
void rox_mime_install_from_appinfo(void)
{
  install_from_appinfo_c();
}

/*
 * $Log: mime_handler.c,v $
 * Revision 1.5  2006/08/12 17:04:56  stephen
 * Fix most compilation warnings.
 *
 * Revision 1.4  2005/10/12 11:00:22  stephen
 * Externally visible symbols have rox_ or ROX prefixes.
 * All still exist under the old names but in general will produce a warning message.
 *
 * Revision 1.3  2005/09/10 16:14:37  stephen
 * Added doxygen comments
 *
 * Revision 1.2  2004/06/21 21:09:29  stephen
 * Added C implementation
 *
 */
