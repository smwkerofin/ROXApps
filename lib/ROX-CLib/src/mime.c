/*
 * $Id: mime.c,v 1.1 2004/03/25 13:10:40 stephen Exp $
 *
 * Shared MIME databse functions for ROX-CLib
 */

#include "rox-clib.h"

#include <stdlib.h>
#include <time.h>
#include <sys/param.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <libxml/parser.h>

#include "rox.h"
#include "basedir.h"
#include "mime.h"

#define TYPE_NS "http://www.freedesktop.org/standards/shared-mime-info"

/* Globals */
MIMEType *text_plain;       /* Default for plain file */
MIMEType *application_executable;  /* Default for executable file */
MIMEType *application_octet_stream;  /* Default for binary file */
MIMEType *inode_directory;
MIMEType *inode_mountpoint;
MIMEType *inode_pipe;
MIMEType *inode_socket;
MIMEType *inode_block;
MIMEType *inode_char;
MIMEType *inode_door;
MIMEType *inode_unknown;

#define UNKNOWN text_plain

/* The first pattern in the list which matches is used */
typedef struct pattern {
  gint len;		/* Used for sorting */
  gchar *glob;
  MIMEType *type;
} Pattern;

/* List of all defined types */
static GHashTable *all_types=NULL;

/* Ways to look up types */
static GHashTable *literals=NULL;  /* By full leaf name */
static GHashTable *exten=NULL;     /* .jpg -> image/jpeg */
static GPtrArray *globs=NULL;      /* README* -> text/x-readme */

/* config */
static int by_content=FALSE;   /* Currently ignored */
static int ignore_exec=FALSE;

/* Local functions */
static MIMEType *get_type(const char *name, int can_create);
static void load_mime_types(void);
static void get_comment(MIMEType *type);
static MIMEType *type_by_path(const char *path);

void mime_init(void)
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

MIMEType *mime_lookup(const char *path)
{
  MIMEType *type=NULL;
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

MIMEType *mime_lookup_by_name(const char *name)
{
  return get_type(name, TRUE);
}

char *mime_type_name(const MIMEType *type)
{
  char *s;

  g_return_val_if_fail(type!=NULL, NULL);

  s=g_strdup_printf("%s/%s", type->media, type->subtype);

  return s;
}

static void get_comment_file(const gchar *fname, MIMEType *type,
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
    rox_debug_printf(3, "lang=%s", lang? lang: "NULL");

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
    rox_debug_printf(3, "best=%s", best? best: "NULL");
    
    xmlFree(lang);
    
  }

  if(best) {
    type->comment=g_strdup(best);
    xmlFree(best);
  }
  
  xmlFreeDoc(doc);
}

static void get_comment(MIMEType *type)
{
  GList *files, *p;
  gchar *leaf;

  g_return_if_fail(type!=NULL);

  rox_debug_printf(1, "get_comment %s/%s", type->media, type->subtype);

  leaf=g_strdup_printf("%s/%s.xml", type->media, type->subtype);
  files=basedir_load_data_paths("mime", leaf);
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

const char *mime_type_comment(MIMEType *type)
{
  g_return_val_if_fail(type!=NULL, NULL);

  if(!type->comment)
    get_comment(type);

  return type->comment? type->comment: "unknown";
}

void mime_set_ignore_exec_bit(int ignore)
{
  ignore_exec=ignore;
}

int mime_get_ignore_exec_bit(void)
{
  return ignore_exec;
}

void mime_set_by_content(int content)
{
  by_content=content;
}

int mime_get_by_content(void)
{
  return by_content;
}

static MIMEType *get_type(const char *name, int can_create)
{
  MIMEType *type;
  char *sl;

  type=g_hash_table_lookup(all_types, name);
  if(type || !can_create)
    return type;

  sl=strchr(name, '/');
  g_return_val_if_fail(sl!=NULL, NULL);
  g_return_val_if_fail(strchr(sl+1, '/')==NULL, NULL);

  type=g_new(MIMEType, 1);
  type->media=g_strndup(name, sl-name);
  type->subtype=g_strdup(sl+1);
  type->comment=NULL;

  g_hash_table_insert(all_types, g_strdup(name), type);

  return type;
}

static void add_pattern(MIMEType *type, const char *pattern)
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
  MIMEType *type=NULL;
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

  paths=basedir_load_data_paths("mime", "globs");
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

static MIMEType *type_by_path(const char *path)
{
  const char *ext, *dot, *leafname;
  char *lower;
  MIMEType *type=NULL;
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

  while(dot=strchr(ext, '.')) {
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
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

