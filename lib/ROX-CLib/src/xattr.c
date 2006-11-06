/**
 * @file xattr.c
 * @brief Extended file attribute support, particularly for MIME types.
 *
 * This provides an abstract interface to the supported extended attribute
 * API.  Both getxattr() and attropen() are supported, as is a fallback
 * no-op implementation.
 *
 * @version $Id: xattr.c,v 1.1 2006/10/03 20:45:58 stephen Exp $
 * @author Stephen Watson, Thomas Leonard
 */


#include "rox-clib.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "rox.h"
#include "mime.h"
#include "xattr.h"

#if defined(HAVE_GETXATTR)
/* Linux implementation */

#include <dlfcn.h>

static int (*dyn_setxattr)(const char *path, const char *name,
		     const void *value, size_t size, int flags) = NULL;
static ssize_t (*dyn_getxattr)(const char *path, const char *name,
			 void *value, size_t size) = NULL;
static ssize_t (*dyn_listxattr)(const char *path, char *list,
			 size_t size) = NULL;
static int (*dyn_removexattr)(const char *path, const char *name) = NULL;

void rox_xattr_init(void)
{
  void *libc;
  
  libc = dlopen("libc.so.6", RTLD_LAZY | RTLD_NOLOAD);
  if (!libc) {
    /* Try a different name for uClib support */
    libc = dlopen("libc.so", RTLD_LAZY | RTLD_NOLOAD);
  }

  if (!libc)
    return;	/* Give up on xattr support */

  dyn_setxattr = (void *) dlsym(libc, "setxattr");
  dyn_getxattr = (void *) dlsym(libc, "getxattr");
  dyn_listxattr = (void *) dlsym(libc, "listxattr");
  dyn_removexattr = (void *) dlsym(libc, "removexattr");
}

int rox_xattr_supported(const char *path)
{
  char buf[1];
  ssize_t nent;
	
  if (!dyn_getxattr)
    return FALSE;

  if(path) {
    errno=0;
    nent=dyn_getxattr(path, ROX_XATTR_MIME_TYPE, buf, sizeof(buf));

    if(nent<0 && errno==ENOTSUP)
      return FALSE;
    errno=0;
  }

  return TRUE;
}

int rox_xattr_have(const char *path)
{
  char buf[1];
  ssize_t nent;
	
  if (!dyn_listxattr) {
    errno = ENOSYS;
    return FALSE;
  }

  errno=0;
  nent=dyn_listxattr(path, buf, sizeof(buf));

  if(nent<0 && errno==ERANGE) {
    errno=0;
    return TRUE;
  }
	
  return (nent>0);
}

gchar *rox_xattr_get(const char *path, const char *attr, int *len)
{
  ssize_t size;
  gchar *buf;

  if (!dyn_getxattr)
    return NULL;
  
  size = dyn_getxattr(path, attr, "", 0);
  if (size > 0) {
    int new_size;

    buf = g_new(gchar, size + 1);
    new_size = dyn_getxattr(path, attr, buf, size);
    
    if(size == new_size) {
      buf[size] = '\0';
      
      if(len)
	*len=(int) size;

      return buf;
    }
    
    g_free(buf);
  }

  return NULL;
}

/* 0 on success */
int rox_xattr_set(const char *path, const char *attr,
	      const char *value, int value_len)
{
  if (!dyn_setxattr) {
    errno = ENOSYS;
    return 1; /* Set attr failed */
  }

  if(value && value_len<0)
    value_len = strlen(value);

  return dyn_setxattr(path, attr, value, value_len, 0);
}

GList *rox_xattr_list(const char *path)
{
  GList *names=NULL;
  gchar *buf, *p;
  ssize_t len;

  if(!dyn_listxattr) {
    errno = ENOSYS;
    return NULL;
  }

  len=dyn_listxattr(path, NULL, 0);
  if(len<0)
    return NULL;

  buf=g_new(gchar, len);
  len=dyn_listxattr(path, buf, len);
  if(len<0) {
    g_free(buf);
    return NULL;
  }

  for(p=buf; p-buf<len; p++) {
    int l=strlen(p);
    names=g_list_append(names, g_strdup(p));
    p+=l;
  }

  return names;
}

int rox_xattr_delete(const char *path, const char *attr)
{
  if(!dyn_removexattr) {
    errno = ENOSYS;
    return 1;
  }

  return dyn_removexattr(path, attr);
}

gboolean rox_xattr_name_valid(const char *attr)
{
  if(!strchr(attr, '.'))
    return FALSE;
  if(attr[0]=='.')
    return FALSE;
  return TRUE;
}

gboolean rox_xattr_binary_value_supported(void)
{
  return FALSE;
}


#elif defined(HAVE_ATTROPEN)

#include <dirent.h>

/* Solaris 9 implementation */

void rox_xattr_init(void)
{	
  /* Nothing to initialize */
}

int rox_xattr_supported(const char *path)
{
#ifdef _PC_XATTR_ENABLED
  if(!path)
    return TRUE;
	
  return pathconf(path, _PC_XATTR_ENABLED);
#else
  return FALSE;
#endif
}

int rox_xattr_have(const char *path)
{
#ifdef _PC_XATTR_EXISTS
  return pathconf(path, _PC_XATTR_EXISTS)>0;
#else
  errno = ENOSYS;
  return FALSE;
#endif
}

#define MAX_ATTR_SIZE BUFSIZ
gchar *rox_xattr_get(const char *path, const char *attr, int *len)
{
  int fd;
  char *buf=NULL;
  int nb;

#ifdef _PC_XATTR_EXISTS
  if(!pathconf(path, _PC_XATTR_EXISTS))
    return NULL;
#endif

  fd=attropen(path, attr, O_RDONLY);
  
  if(fd>=0) {
    buf = g_new(gchar, MAX_ATTR_SIZE);
    nb=read(fd, buf, MAX_ATTR_SIZE);
    if(nb>0) {
      buf[nb]=0;
    }
    close(fd);

    if(len)
      *len=nb;
  } else if(errno==ENOENT) {
    if(access(path, F_OK)==0)
      errno=0;
  }

  return buf;
}

int rox_xattr_set(const char *path, const char *attr,
	      const char *value, int value_len)
{
  int fd;
  int nb;

  if(value && value_len<0)
    value_len = strlen(value);

  fd=attropen(path, attr, O_WRONLY|O_CREAT, 0644);
  if(fd>0) {
    
    nb=write(fd, value, value_len);
    if(nb==value_len)
      ftruncate(fd, (off_t) nb);

    close(fd);

    if(nb>0)
      return 0;
  }
  
  return 1; /* Set attr failed */
}

GList *rox_xattr_list(const char *path)
{
  GList *names=NULL;
  int fd;
  
  if(!rox_xattr_have(path))
    return FALSE;
  
  fd=attropen(path, ".", O_RDONLY);
  
  if(fd>=0) {
    DIR *dir=fdopendir(fd);
    struct dirent ent;

    while(readdir_r(dir, &ent)) {
      if(strcmp(ent.d_name, ".")==0 || strcmp(ent.d_name, "..")==0)
	continue;

      names=g_list_append(names, g_strdup(ent.d_name));
    }

    closedir(dir);
  }
  
  return names;
}

int rox_xattr_delete(const char *path, const char *attr)
{
  int fd, state;
  
  if(!rox_xattr_have(path)) {
    errno=0;
    return 1;
  }
  
  fd=attropen(path, ".", O_RDONLY);

  state=0;
  if(fd>=0) {
    state=unlinkat(fd, attr, 0);
    close(fd);
  } else {
    state=1;
    
  }
  
  return state;
}

gboolean rox_xattr_name_valid(const char *attr)
{
  if(strchr(attr, '/'))
    return FALSE;
  return TRUE;
}

gboolean rox_xattr_binary_value_supported(void)
{
  return TRUE;
}

#else
/* No extended attributes available */

void rox_xattr_init(void)
{
}

int rox_xattr_supported(const char *path)
{
}

int rox_xattr_have(const char *path)
{
  errno = ENOSYS;
  return FALSE;
}

gchar *rox_xattr_get(const char *path, const char *attr, int *len)
{
  return NULL;
}

int rox_xattr_set(const char *path, const char *attr,
	      const char *value, int value_len)
{
  errno = ENOSYS;
  return 1; /* Set type failed */
}

GList *rox_xattr_list(const char *path)
{
  errno = ENOSYS;
  return NULL;
}

int rox_xattr_delete(const char *path, const char *attr)
{
  errno=ENOSYS;
  return 1;
}

gboolean rox_xattr_name_valid(const char *attr)
{
  return FALSE;
}

gboolean rox_xattr_binary_value_supported(void)
{
  return FALSE;
}

#endif

ROXMIMEType *rox_xattr_type_get(const char *path)
{
  ROXMIMEType *type = NULL;
  gchar *buf;
  char *nl;

  buf = rox_xattr_get(path, ROX_XATTR_MIME_TYPE, NULL);

  if(buf) {
    nl = strchr(buf, '\n');
    if(nl)
      *nl = 0;
    type = rox_mime_get_type(buf, TRUE);
    g_free(buf);
  }
  return type;
}

int rox_xattr_type_set(const char *path, const ROXMIMEType *type)
{
  int res;
  gchar *ttext;

  ttext = g_strdup_printf("%s/%s", type->media, type->subtype);
  res = rox_xattr_set(path, ROX_XATTR_MIME_TYPE, ttext, -1);
  g_free(ttext);

  return res;
}

