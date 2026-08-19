#ifndef INBE_APP_UPDATE_ZIP_H
#define INBE_APP_UPDATE_ZIP_H

/*
 * Extract a release zip (miniz) into a directory, rejecting path-traversal
 * entries. Used by the Windows portable self-update; plain portable C so
 * it builds and tests everywhere.
 */

/* mkdir -p. Returns 1 on success (or already exists). */
int inbe_update_zip_mkdir_p(const char *path);

/* Extract every entry of `zip_path` under `dest_dir`. Returns 1 when all
 * entries extracted cleanly, 0 on any error (partial files may remain). */
int inbe_update_zip_extract(const char *zip_path, const char *dest_dir);

#endif
