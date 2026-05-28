#ifndef ZIP_WRITER_H
#define ZIP_WRITER_H

#include <stdio.h>

/* Minimal ZIP writer for exporting session data
 * Based on RFC 1951 (DEFLATE) and APPNOTE.TXT (ZIP format) */

typedef struct ZIPWriter {
    FILE *fp;
    int file_count;
    long long central_dir_offset;
} ZIPWriter;

/* Initialize ZIP writer
 * fp: Open file handle (will be written to)
 * Returns: ZIPWriter structure */
ZIPWriter zip_writer_init(FILE *fp);

/* Write a file to the ZIP archive
 * writer: ZIP writer instance
 * path: Path inside archive (e.g., "sessions/2025/05/28/inbe-143025")
 * data: File content
 * size: Content size
 * Returns: 1 on success, 0 on failure */
int zip_write_file(ZIPWriter *writer, const char *path, const void *data, int size);

/* Finalize ZIP archive (write central directory and close)
 * writer: ZIP writer instance
 * Returns: 1 on success, 0 on failure */
int zip_finalize(ZIPWriter *writer);

#endif
