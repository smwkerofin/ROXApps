/*
 * $Id: mime.h,v 1.3 2005/09/10 16:14:19 stephen Exp $
 *
 * Shared MIME database functions for ROX-CLib
 */

/**
 * @file mime.h
 * @brief Shared MIME database functions for ROX-CLib
 *
 * @author Thomas Leonard, Stephen Watson
 * @version $Id: mime.h,v 1.3 2005/09/10 16:14:19 stephen Exp $
 */

#ifndef _rox_mime_h
#define _rox_mime_h

/**
 * Type holding information on a MIME-type
 */
typedef struct rox_mime_type{
  const char *media;            /**< Media of type, e.g. text or audio */
  const char *subtype;          /**< Sub-type, e.g. html in text/html */

  const char *comment;          /**< Description of type.  Do not access this
				 * directly, use mime_type_comment().*/
} ROXMIMEType;
#define MIMEType ROXMIMEType    /**< Alias for ROXMIMEType */

extern ROXMIMEType *text_plain;               /**< Default for plain file */
extern ROXMIMEType *application_executable;   /**< Default for executable
					       * file */
extern ROXMIMEType *application_octet_stream; /**< Default for binary file */
extern ROXMIMEType *inode_directory;          /**< A directory */
extern ROXMIMEType *inode_mountpoint;         /**< A directory used as a mount
					       * point*/
extern ROXMIMEType *inode_pipe;               /**< A named pipe */
extern ROXMIMEType *inode_socket;             /**< UNIX domain socket */
extern ROXMIMEType *inode_block;              /**< Block device file */
extern ROXMIMEType *inode_char;               /**< Character device file */
extern ROXMIMEType *inode_door;               /**< Door file, similar to
					       * sockets, not available on all
					       * operating systems */
extern ROXMIMEType *inode_unknown;            /**< Unknown type, probably
					       * broken symbolic link */

/* Initialize MIME system.   */
extern void rox_mime_init(void);

extern ROXMIMEType *rox_mime_lookup(const char *path);
extern ROXMIMEType *rox_mime_lookup_by_name(const char *name);
extern char *rox_mime_type_name(const ROXMIMEType *type);
extern const char *rox_mime_type_comment(ROXMIMEType *type);

extern void rox_mime_set_ignore_exec_bit(int ignore);
extern int rox_mime_get_ignore_exec_bit(void);
extern void rox_mime_set_by_content(int content);
extern int rox_mime_get_by_content(void);

/* Old names, to be removed before 2.2 */
extern void mime_init(void);

extern ROXMIMEType *mime_lookup(const char *path);
extern ROXMIMEType *mime_lookup_by_name(const char *name);
extern char *mime_type_name(const ROXMIMEType *type);
extern const char *mime_type_comment(ROXMIMEType *type);

extern void mime_set_ignore_exec_bit(int ignore);
extern int mime_get_ignore_exec_bit(void);
extern void mime_set_by_content(int content);
extern int mime_get_by_content(void);

#endif

/*
 * $Log: mime.h,v $
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

