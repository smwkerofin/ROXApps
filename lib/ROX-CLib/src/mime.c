/*
 * $Id: mime.c,v 1.8 2006/08/12 17:04:56 stephen Exp $
 *
 * Shared MIME databse functions for ROX-CLib
 */

/**
 * @file mime.c
 * @brief Shared MIME database functions for ROX-CLib
 *
 * @author Thomas Leonard, Stephen Watson
 * @version $Id: mime.c,v 1.8 2006/08/12 17:04:56 stephen Exp $
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <sys/param.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <libxml/parser.h>

#include "rox.h"
#include "basedir.h"
#include "mime.h"

/**
 * XML name space of the MIME definitions.
 */
#define TYPE_NS "http://www.freedesktop.org/standards/shared-mime-info"

#ifdef ROX_CLIB_NO_MIME_TYPE_EXPORT
#define MIMEType_export static
#else
#define MIMEType_export
#endif

/* Globals */
MIMEType_export ROXMIMEType *text_plain;              /* Default for plain
						       * file */
MIMEType_export ROXMIMEType *application_executable;  /* Default for
						       * executable file */
MIMEType_export ROXMIMEType *application_octet_stream;/* Default for binary
						       * file */
MIMEType_export ROXMIMEType *inode_directory;
MIMEType_export ROXMIMEType *inode_mountpoint;
MIMEType_export ROXMIMEType *inode_pipe;
MIMEType_export ROXMIMEType *inode_socket;
MIMEType_export ROXMIMEType *inode_block;
MIMEType_export ROXMIMEType *inode_char;
MIMEType_export ROXMIMEType *inode_door;
MIMEType_export ROXMIMEType *inode_unknown;

/** Normal files of undefined type default to text/plain */
#define UNKNOWN text_plain

/** The first pattern in the list which matches is used */
typedef struct pattern {
  gint len;		/**< Used for sorting the list, longest pattern
			 * first */
  gchar *glob;          /**< glob pattern, such as *.txt */
  ROXMIMEType *type;    /**< Type for this pattern */
} Pattern;

/** List of all defined types */
static GHashTable *all_types=NULL;

/* Ways to look up types */
static GHashTable *literals=NULL;  /**< Type by full leaf name */
static GHashTable *exten=NULL;     /**< Type by extension,
				    * .jpg -> image/jpeg */
static GPtrArray *globs=NULL;      /**< Type by pattern
				    * README* -> text/x-readme */

/* config */
static int by_content=FALSE;   /**< If non-zero check the file contents to
				* identify the type, currently ignored */
static int ignore_exec=FALSE;  /**< If zero all files with an execute
				* bit set are returned as
				* application/executable */

/* Local functions */
static ROXMIMEType *get_type(const char *name, int can_create);
static void load_mime_types(void);
static void get_comment(ROXMIMEType *type);
static ROXMIMEType *type_by_path(const char *path);

/**
 * Initialize the MIME type system.  This must be called before the
 * other functions.
 */
void rox_mime_init(void)
{
  all_types=g_hash_table_new(g_str_hash, g_str_equal);
  literals=g_hash_table_new(g_str_hash, g_str_equal);
  exten=g_hash_table_new(g_str_hash, g_str_equal);
  globs=g_ptr_array_new();

  text_plain=get_type("text/plain", TRUE);
  application_executable=get_type("application/x-executable", TRUE);
  application_octet_stream=get_type("application/octet-stream", TRUE);
  inode_directory=get_type("inode/directory", TRUE);
  inode_mountpoint=get_type("inode/mountpoint", TRUE);
  inode_pipe=get_type("inode/fifo", TRUE);
  inode_socket=get_type("inode/socket", TRUE);
  inode_block=get_type("inode/blockdevice", TRUE);
  inode_char=get_type("inode/chardevice", TRUE);
  inode_door=get_type("inode/door", TRUE);
  inode_unknown=get_type("inode/unknown", TRUE);

  load_mime_types();
}

/**
 * Parse a MIME type name and return the corresponding ROXMIMEType
 * data.
 *
 * @param[in] name name of the type, as media/sub-type
 * @param[in] can_create if @c TRUE and @a name is not a currently known
 * type then create a new one and return it, otherwise return @c NULL
 * @return the type, or @c NULL if @a is not known and @a can_create
 * @a false, or if @a is malformed.
 */
ROXMIMEType *rox_mime_get_type(const char *name, gboolean can_create)
{
  if(!strchr(name, '/'))
    return NULL;
  if(name[0]=='/')
    return NULL;
  
  return get_type(name, can_create);
}

/**
 * Initialize the MIME type system.
 * @deprecated Use rox_mime_init() instead.
 */
void mime_init(void)
{
  ROX_CLIB_DEPRECATED("rox_mime_init");
  rox_mime_init();
}

/**
 * Return the MIME type of the named object.
 *
 * If the object does not exist, or
 * is in an unsearchable directory, then the type returned is based purely
 * on the name.
 *
 * If the object does exist then the type of file is checked.  If it is a
 * non-file object (such as a directory)  then the appropriate inode/subtype
 * type is returned.  If it is a file and is executable and the
 * mime_get_ignore_exec_bit() value is @c FALSE then application/executable
 * is returned.
 *
 * If mime_get_by_content() is not @c FALSE and content checking is
 * implemented (it currently is not) and a type is identified it is returned.
 *
 * If the type can be identified by the name then that is returned.
 *
 * Finally if the execute bit is set application/executable is returned,
 * otherwise #UNKNOWN.
 *
 * @param[in] path path to object to check, or a name of a non-existant file
 * @return the identified type.
 */
ROXMIMEType *rox_mime_lookup(const char *path)
{
  ROXMIMEType *type=NULL;
  struct stat st;
  int exec=FALSE;

  if(stat(path, &st)==0) {

    if(S_ISDIR(st.st_mode)) {
      /* Need to check for mount points */
      
      return inode_directory;
    } else if(S_ISCHR(st.st_mode))
      return inode_char;
    else if(S_ISCHR(st.st_mode))
      return inode_block;
    else if(S_ISFIFO(st.st_mode))
      return inode_pipe;
    else if(S_ISSOCK(st.st_mode))
      return inode_socket;
#ifdef S_ISDOOR
    else if(S_ISDOOR(st.st_mode))
      return inode_door;
#endif
    else if(!S_ISREG(st.st_mode))
      return inode_unknown;
    
   exec=(st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
  }
  if(exec && !ignore_exec)
    return application_executable;

  type=type_by_path(path);
  if(type)
    return type;

  return exec? application_executable: UNKNOWN;
}
 
/**
 * Return the MIME type of the named object.
 *
 * @deprecated Use rox_mime_lookup().
 */
ROXMIMEType *mime_lookup(const char *path)
{
  ROX_CLIB_DEPRECATED("rox_mime_lookup");
  return rox_mime_lookup(path);
}

/**
 * Return the type of a file based purely on the name.  The file need not
 * exist or be readable.
 *
 * @param[in] name name of file
 * @return the identified type.
 */
ROXMIMEType *rox_mime_lookup_by_name(const char *name)
{
  return get_type(name, TRUE);
}

/**
 * Return the type of a file based purely on the name.  The file need not
 * exist or be readable.
 *
 * @deprecated use rox_mime_lookup_by_name()
 */
ROXMIMEType *mime_lookup_by_name(const char *name)
{
  ROX_CLIB_DEPRECATED("rox_mime_lookup_by_name");
  return rox_mime_lookup_by_name(name);
}

/**
 * Return the human readable name of the type, in the form media/sub-type
 *
 * @param[in] type type to return name of.
 * @return the name, pass to g_free() when done.
 */
char *rox_mime_type_name(const ROXMIMEType *type)
{
  char *s;

  g_return_val_if_fail(type!=NULL, NULL);

  s=g_strdup_printf("%s/%s", type->media, type->subtype);

  return s;
}

/**
 * Return the human readable name of the type, in the form media/sub-type
 *
 * @deprecated use rox_mime_type_name().
 */
char *mime_type_name(const ROXMIMEType *type)
{
  ROX_CLIB_DEPRECATED("rox_mime_type_name");
  return rox_mime_type_name(type);
}

static void get_comment_file(const gchar *fname, ROXMIMEType *type,
			     const char *req_lang)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  xmlChar *best=NULL;

  rox_debug_printf(2, "fname=%s %s/%s %s", fname, type->media, type->subtype,
	  req_lang? req_lang: "no lang");

  doc=xmlParseFile(fname);
  if(!doc) {
    rox_debug_printf(2, "failed to parse %s", fname);
    return;
  }

  rox_debug_printf(2, "scan doc");
  for(node=xmlDocGetRootElement(doc)->xmlChildrenNode; node;
      node=node->next) {
    xmlChar *lang;
    
    if(node->type!=XML_ELEMENT_NODE)
      continue;

    rox_debug_printf(3, " node->name=%s", node->name);
    if(strcmp(node->name, "comment")!=0)
      continue;

    lang=xmlNodeGetLang(node);
    rox_debug_printf(3, "lang=%s", lang? ((char *) lang): "NULL");

    if(!lang && !req_lang) {
      best=xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
      break;
    }

    if(req_lang && !lang) {
      best=xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
      continue;
    }

    if(req_lang && lang && strcmp(lang, req_lang)==0) {
      best=xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
      xmlFree(lang);
      break;
    }
    rox_debug_printf(3, "best=%s", best? ((char *) best): "NULL");
    
    xmlFree(lang);
    
  }

  if(best) {
    type->comment=g_strdup(best);
    xmlFree(best);
  }
  
  xmlFreeDoc(doc);
}

static void get_comment(ROXMIMEType *type)
{
  GList *files, *p;
  gchar *leaf;

  g_return_if_fail(type!=NULL);

  rox_debug_printf(1, "get_comment %s/%s", type->media, type->subtype);

  leaf=g_strdup_printf("%s/%s.xml", type->media, type->subtype);
  files=rox_basedir_load_data_paths("mime", leaf);
  g_free(leaf);

  for(p=files; p; p=g_list_next(p)) {
    get_comment_file((gchar *) p->data, type, NULL);

    if(type->comment)
      break;
  }

  for(p=files; p; p=g_list_next(p)) {
    g_free(p->data);
  }

  g_list_free(files);
}

/**
 * Return the comment of a MIME type (the long form of a name, e.g. "
 * Text document")
 *
 * @param[in] type the type
 * @return the comment
 */
const char *rox_mime_type_comment(ROXMIMEType *type)
{
  g_return_val_if_fail(type!=NULL, NULL);

  if(!type->comment)
    get_comment(type);

  return type->comment? type->comment: "unknown";
}

/**
 * Return the comment of a MIME type (the long form of a name, e.g. "
 * Text document")
 *
 * @deprecated use rox_mime_type_comment().
 */
const char *mime_type_comment(ROXMIMEType *type)
{
  ROX_CLIB_DEPRECATED("rox_mime_type_comment");
  return rox_mime_type_comment(type);
}

/**
 * Set how the MIME system treats executable files.  If @a ignore is @c FALSE
 * then all executable files will be identified as application/executable
 *
 * @param[in] ignore the new value of the ignore_exec flag.
 */
void rox_mime_set_ignore_exec_bit(int ignore)
{
  ignore_exec=ignore;
}

/**
 * Set how the MIME system treats executable files.  If @a ignore is @c FALSE
 * then all executable files will be identified as application/executable
 *
 * @deprecated Use rox_mime_set_ignore_exec_bit().
 */
void mime_set_ignore_exec_bit(int ignore)
{
  ROX_CLIB_DEPRECATED("rox_mime_set_ignore_exec_bit");
  ignore_exec=ignore;
}

/**
 * Return how the MIME system treats executable files, see
 * mime_set_ignore_exec_bit().
 *
 * @return the value of the ignore_exec flag.
 */
int rox_mime_get_ignore_exec_bit(void)
{
  return ignore_exec;
}

/**
 * Return how the MIME system treats executable files, see
 * mime_set_ignore_exec_bit().
 *
 * @deprecated Use rox_mime_get_ignore_exec_bit().
 */
int mime_get_ignore_exec_bit(void)
{
  ROX_CLIB_DEPRECATED("rox_mime_get_ignore_exec_bit");
  return ignore_exec;
}

/**
 * Set how the MIME system deals with the content of files.  If @a content is
 * @c FALSE then the content of the files will never be read to identify the
 * type.
 *
 * Currently this flag has no effect as identifying files by content is not
 * yet supported.
 *
 * @param[in] content the new value of the by_cotnent flag.
 */
void rox_mime_set_by_content(int content)
{
  by_content=content;
}

/**
 * Set how the MIME system deals with the content of files.  If @a content is
 * @c FALSE then the content of the files will never be read to identify the
 * type.
 *
 * @deprecated Use rox_mime_set_by_content()
 */
void mime_set_by_content(int content)
{
  ROX_CLIB_DEPRECATED("rox_mime_set_by_content");
  by_content=content;
}

/**
 * Return how the MIME system deals with the content of files, see
 * mime_set_by_content().
 *
 * Currently this flag has no effect as identifying files by content is not
 * yet supported.
 *
 * @return the value of the by_content flag.
 */
int rox_mime_get_by_content(void)
{
  return by_content;
}

/**
 * Return how the MIME system deals with the content of files, see
 * mime_set_by_content().
 *
 *
 * @deprecated Use rox_mime_get_by_content()
 */
int mime_get_by_content(void)
{
  ROX_CLIB_DEPRECATED("rox_mime_get_by_content");
  return by_content;
}

static GdkPixbuf *try_load(const char *path, int msize)
{
  if(!path)
    return NULL;

  rox_debug_printf(1, "try loading %s at %d", path, msize);

  return gdk_pixbuf_new_from_file_at_size(path, msize, msize, NULL);
}

static GdkPixbuf *theme_try_load(const char *root, int dot, const char *theme,
				 const char *name, int msize)
{
  gchar *path;
  gchar *tname;
  GdkPixbuf *icon;

  tname=g_strconcat(name, ".svg", NULL);
  path=g_build_filename(root, dot? ".icons" : "icons",
			theme, "MIME", tname, NULL);
  g_free(tname);
  icon=try_load(path, msize);
  g_free(path);
  if(icon)
    return icon;

  tname=g_strconcat(name, ".png", NULL);
  path=g_build_filename(root, dot? ".icons": "icons",
			theme, "MIME", tname, NULL);
  g_free(tname);
  icon=try_load(path, msize);
  g_free(path);
 
  return icon;
}

/**
 * Return the icon for the specified MIME type, scaled appropriately.
 * This function checks the override icons for media/subtype, then the
 * ROX theme icon for the same type.  If neither works it tries again for the
 * generic media icon in the same places.  If nothing works, @c NULL is
 * returned.
 *
 * In the future this will detect the icon theme in use, but until then
 * only the ROX theme is checked.
 *
 * @param[in] type MIME type to look up
 * @param[in] msize maximum size in pixels of the icon to return.  If 0 or less
 * the default size of 48 is used.
 * @return the icon, or @c NULL if not found.
 */
GdkPixbuf *rox_mime_get_icon(const ROXMIMEType *type, int msize)
{
  gchar *iname, *path;
  const gchar *home;
  GdkPixbuf *icon=NULL;

  if(msize<=0)
    msize=48;

  iname=g_strdup_printf("%s_%s.png", type->media, type->subtype);
  path=rox_choices_load(iname, "MIME-icons", "rox.sourceforge.net");
  if(!path)
    path=rox_choices_load(iname, "MIME-icons", NULL);
  g_free(iname);
  if(path) {
    icon=try_load(path, msize);
    g_free(path);
  }
  if(icon)
    return icon;

  home=g_get_home_dir();

  iname=g_strdup_printf("mime-%s:%s", type->media, type->subtype);
  icon=theme_try_load(home, TRUE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  icon=theme_try_load("/usr/local/share", FALSE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  icon=theme_try_load("/usr/share", FALSE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  g_free(iname);
  
 iname=g_strdup_printf("%s.png", type->media);
  path=rox_choices_load(iname, "MIME-icons", "rox.sourceforge.net");
  if(!path)
    path=rox_choices_load(iname, "MIME-icons", NULL);
  g_free(iname);
  if(path) {
    icon=try_load(path, msize);
    g_free(path);
  }
  if(icon)
    return icon;

  home=g_get_home_dir();

  iname=g_strdup_printf("mime-%s", type->media);
  icon=theme_try_load(home, TRUE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  icon=theme_try_load("/usr/local/share", FALSE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  icon=theme_try_load("/usr/share", FALSE, "ROX", iname, msize);
  if(icon) {
    g_free(iname);
    return icon;
  }
  g_free(iname);

  return NULL;
}

static ROXMIMEType *get_type(const char *name, int can_create)
{
  ROXMIMEType *type;
  char *sl;

  type=g_hash_table_lookup(all_types, name);
  if(type || !can_create)
    return type;

  sl=strchr(name, '/');
  g_return_val_if_fail(sl!=NULL, NULL);
  g_return_val_if_fail(strchr(sl+1, '/')==NULL, NULL);

  type=g_new(ROXMIMEType, 1);
  type->media=g_strndup(name, sl-name);
  type->subtype=g_strdup(sl+1);
  type->comment=NULL;

  g_hash_table_insert(all_types, g_strdup(name), type);

  return type;
}

static void add_pattern(ROXMIMEType *type, const char *pattern)
{
  if(pattern[0]=='*' && pattern[1]=='.' &&
     strpbrk(pattern + 2, "*?[")==NULL) {
    g_hash_table_insert(exten, g_strdup(pattern+2), type);
    
  } else if(strpbrk(pattern, "*?[")==NULL) {
    g_hash_table_insert(literals, g_strdup(pattern), type);
    
  } else {
    Pattern *pat=g_new(Pattern, 1);

    pat->glob=g_strdup(pattern);
    pat->type=type;
    pat->len=strlen(pat->glob);

    g_ptr_array_add(globs, pat);
  }
}

static gint sort_by_strlen(gconstpointer a, gconstpointer b)
{
  const Pattern *pa = *(const Pattern **) a;
  const Pattern *pb = *(const Pattern **) b;

  if(pa->len > pb->len)
    return -1;
  else if(pa->len == pb->len)
    return 0;
  return 1;
}

static void load_glob(const char *path)
{
  ROXMIMEType *type=NULL;
  GError *error=NULL;
  gchar *data, *line;
  
  if(!g_file_get_contents(path, &data, NULL, &error)) {
    rox_error(_("Error loading MIME database %s: %s"), path,
	      error->message);
    g_error_free(error);
    return;
  }
	
  line=data;
  while(line && *line) {
    char *eol;

    eol=strchr(line, '\n');
    if(!eol)
      break;
    *eol=0;

    if(line[0]!='#') {
      const gchar *colon=strchr(line, ':');
      gchar *name;

      if(!colon) {
	rox_error(_("MIME glob file %s in bad format"), path);
	break;
      }

      name=g_strndup(line, colon-line);
      type=get_type(name, TRUE);
      g_free(name);
      if(type)
	add_pattern(type, colon+1);
    }
    line=eol+1;
  }

  g_free(data);
}

static void load_mime_types(void)
{
  GList *paths, *p;

  paths=rox_basedir_load_data_paths("mime", "globs");
  for(p=g_list_last(paths); p; p=g_list_previous(p)) {
    load_glob((char *) p->data);

    g_free(p->data);
  }
  g_list_free(paths);
  
  if(globs->len)
    g_ptr_array_sort(globs, sort_by_strlen);

  if(g_hash_table_size(exten)==0) {
    rox_error(_("No MIME types found.  You may need to install "
		"the Shared MIME type database (version 0.9 or "
		"later.  It may be found at "
		"http://www.freedesktop.org/software/shared-mime-info"));
  }
}

static ROXMIMEType *type_by_path(const char *path)
{
  const char *ext, *dot, *leafname;
  char *lower;
  ROXMIMEType *type=NULL;
  int i;

  leafname=g_basename(path);

  /* Literal names */
  type=g_hash_table_lookup(literals, leafname);
  if(type)
    return type;

  lower=g_utf8_strdown(leafname, -1);
  type=g_hash_table_lookup(literals, lower);
  if(type) {
    g_free(lower);
    return type;
  }

  /* Extensions */
  ext=leafname;

  while((dot=strchr(ext, '.'))) {
    ext=dot+1;
    
    type=g_hash_table_lookup(exten, ext);
    if(type) 
      break;
    
    type=g_hash_table_lookup(exten, lower+(ext-leafname));
  }
  if(type) {
    g_free(lower);
    return type;
  }

  /* Glob patterns */
  for (i=0; i< globs->len; i++) {
    Pattern *p=globs->pdata[i];

    if(fnmatch(p->glob, leafname, 0)==0 || fnmatch(p->glob, lower, 0)==0) {
      type = p->type;
      break;
    }
  }

  g_free(lower);

  return type;
}

/*
 * $Log: mime.c,v $
 * Revision 1.8  2006/08/12 17:04:56  stephen
 * Fix most compilation warnings.
 *
 * Revision 1.7  2005/12/07 11:44:09  stephen
 * Can suppress export of MIMEType objects
 *
 * Revision 1.6  2005/10/22 10:42:28  stephen
 * Renamed basedir functions to rox_basedir.
 * Disabled deprecation warning.
 * This is version 2.1.6
 *
 * Revision 1.5  2005/10/15 10:47:28  stephen
 * Added rox_mime_get_icon()
 *
 * Revision 1.4  2005/10/12 11:00:22  stephen
 * Externally visible symbols have rox_ or ROX prefixes.
 * All still exist under the old names but in general will produce a warning message.
 *
 * Revision 1.3  2005/09/10 16:14:19  stephen
 * Added doxygen comments
 *
 * Revision 1.2  2004/05/22 17:03:57  stephen
 * Added AppInfo parser
 *
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

