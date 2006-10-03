/*
 * Extended file attribute support, particularly for MIME types.
 *
 * Originally ROX-Filer/src/xtypes.h
 */

/**
 * @file xattr.h
 * @brief Extended file attribute support, particularly for MIME types.
 *
 * This provides an abstract interface to the supported extended attribute
 * API.
 *
 * @version $Id$
 * @author Stephen Watson, Thomas Leonard
 */

#ifndef _rox_xattr_h
#define _rox_xattr_h

/* Known attribute names */
#define ROX_XATTR_MIME_TYPE "user.mime_type"  /**< User defined MIME type of
					       * file */
#define ROX_XATTR_HIDDEN    "user.hidden"     /**< User set hidden attribute
					       * on file */

/* Prototypes */

/**
 * Initialize the extended attrubute system.
 */
extern void rox_xattr_init(void);

/* path may be NULL to test for support in libc */
/**
 * Check for extended attribute support on the system or a specific file.
 *
 * @param[in] path If @c NULL then check for support on the system, otherwise
 * check for xattr support on the named file name.
 * @return @c TRUE if extended attributes are supported.
 */
extern int rox_xattr_supported(const char *path);

/**
 * Check for the presence of extended attributes on a file.
 *
 * @param[in] path file to check
 * @return @c TRUE if extended attributes are present, @c FALSE if not or
 * if an error occurred (check errno for ENOENT etc.).
 */
extern int rox_xattr_have(const char *path);

/**
 * Get the value of an extended attribute.  The returned value should be
 * freed with g_free() when done.
 *
 * @param[in] path file to get extended attribute from.
 * @param[in] attr name of the attribute
 * @param[out] len if not @c NULL then the length of the data returned is
 * stored here, but if the function returns @c NULL then the contents of
 * @a len are not changed.
 * @return the value of the extended attribute @a attr (pass to g_free()
 * when done, or @c NULL if
 * @a attr is not present or if an error occurred (check errno).
 */
extern gchar *rox_xattr_get(const char *path, const char *attr, int *len);

/**
 * Set the value of an extended attribute.
 *
 * @param[in] path file to set extended attribute on.
 * @param[in] attr name of the attribute
 * @param[in] value value of the attribute to set
 * @param[in] value_len length of data in @a value, or @c -1 if @a value
 * is a string in which case strlen(value) will be used.
 * @return @c 0 for success, or non-zero for an error (check errno).
 */
extern int rox_xattr_set(const char *path, const char *attr,
	      const char *value, int value_len);

/**
 * Return a list of extended attributes set on a file.
 *
 * @param[in] path name of file to check
 * @return List of attribute names, pass to rox_basedir_free_paths() when
 * done, or @c NULL if no attributes found or an error occurred (check errno).
 */
extern GList *rox_xattr_list(const char *path);

/**
 * Remove an extended attributes from a file.
 *
 * @param[in] path name of file to check
 * @param[in] attr name of attribute to check
 * @return @c 0 for success, or non-zero for an error (check errno).
 * If the attribute was not set then this returns non-zero but sets errno to 0.
 */
extern int rox_xattr_delete(const char *path, const char *attr);

/**
 * Check validity of an extended attribute name.
 *
 * Some implementations of extended attributes enforce restrictions on
 * valid attribute names.  This checks the name against the current rules.
 *
 * @param[in] attr name of attribute to check
 * @return @c TRUE if the name is valid
 */
extern gboolean rox_xattr_name_valid(const char *attr);

/**
 * @return @c TRUE if the value of an extended attribute can include the ASCII
 * '\\0' character.
 */
extern gboolean rox_xattr_binary_value_supported(void);

/**
 * Convinience function to check a file for the #ROX_XATTR_MIME_TYPE
 * attribute and return the appropriate ROXMIMEType.
 *
 * @param[in] path file to check
 * @return MIME type, or @c NULL if not set or an error occurred (check errno).
 */
extern ROXMIMEType *rox_xattr_type_get(const char *path);

/**
 * Convinience function to set the #ROX_XATTR_MIME_TYPE attribute on a file.
 *
 * @param[in] path file to set MIME type of
 * @param[in] type type to set.
 * @return @c 0 for success, or non-zero for an error (check errno).
 */
extern int rox_xattr_type_set(const char *path, const ROXMIMEType *type);

#endif
