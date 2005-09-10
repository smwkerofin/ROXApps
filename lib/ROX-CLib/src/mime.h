/*
 * $Id: mime.h,v 1.2 2004/05/22 17:03:57 stephen Exp $
 *
 * Shared MIME database functions for ROX-CLib
 */

/**
 * @file mime.h
 * @brief Shared MIME database functions for ROX-CLib
 *
 * @author Thomas Leonard, Stephen Watson
 * @version $Id$
 */

#ifndef _rox_mime_h
#define _rox_mime_h

/**
 * Type holding information on a MIME-type
 */
typedef struct mime_type{
  const char *media;            /**< Media of type, e.g. text or audio */
  const char *subtype;          /**< Sub-type, e.g. html in text/html */

  const char *comment;          /**< Description of type.  Do not access this
				 * directly, use mime_type_comment().*/
} MIMEType; 

/* Common types, valid after mime_init() called */
extern MIMEType *text_plain;               /**< Default for plain file */
extern MIMEType *application_executable;   /**< Default for executable file */
extern MIMEType *application_octet_stream; /**< Default for binary file */
extern MIMEType *inode_directory;          /**< A directory */
extern MIMEType *inode_mountpoint;         /**< A directory used as a mount
					    * point*/
extern MIMEType *inode_pipe;               /**< A named pipe */
extern MIMEType *inode_socket;             /**< UNIX domain socket */
extern MIMEType *inode_block;              /**< Block device file */
extern MIMEType *inode_char;               /**< Character device file */
extern MIMEType *inode_door;               /**< Door file, similar to sockets,
					    * not available on all operating
					    * systems */
extern MIMEType *inode_unknown;            /**< Unknown type, probably broken
					    * symbolic link */

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
 * Revision 1.2  2004/05/22 17:03:57  stephen
 * Added AppInfo parser
 *
 * Revision 1.1  2004/03/25 13:10:40  stephen
 * Added basedir and mime
 *
 */

