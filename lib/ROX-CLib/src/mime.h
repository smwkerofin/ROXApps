/*
 * $Id: mime.h,v 1.1 2004/03/25 13:10:40 stephen Exp $
 *
 * Shared MIME databse functions for ROX-CLib
 */

#ifndef _rox_mime_h
#define _rox_mime_h

typedef struct mime_type{
  const char *media;
  const char *subtype;

  const char *comment;
} MIMEType;

/* Common types, valid after mime_init() called */
extern MIMEType *text_plain;       /* Default for plain file */
extern MIMEType *application_executable;  /* Default for executable file */
extern MIMEType *application_octet_stream;  /* Default for binary file */
extern MIMEType *inode_directory;
extern MIMEType *inode_mountpoint;
extern MIMEType *inode_pipe;
extern MIMEType *inode_socket;
extern MIMEType *inode_block;
extern MIMEType *inode_char;
extern MIMEType *inode_door;
extern MIMEType *inode_unknown;

/* Initialize MIME system.   */
extern void mime_init(void);

extern MIMEType *mime_lookup(const char *path);
extern MIMEType *mime_lookup_by_name(const char *name);
extern char *mime_type_name(const MIMEType *type);
extern const char *mime_type_comment(MIMEType *type);

extern void mime_set_ignore_exec_bit(int ignore);
extern int mime_get_ignore_exec_bit(void);
extern void mime_set_by_content(int content);
extern int mime_get_by_content(void);

#endif

/*
 * $Log: mime.h,v $
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

