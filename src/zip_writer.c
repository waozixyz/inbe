#include "zip_writer.h"
#include <string.h>
#include <time.h>

/* ZIP local file header */
typedef struct {
    unsigned int signature;
    unsigned short version;
    unsigned short flags;
    unsigned short compression;
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int crc32;
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned short filename_len;
    unsigned short extra_len;
} ZIPLocalHeader;

/* ZIP central directory header */
typedef struct {
    unsigned int signature;
    unsigned short version_made;
    unsigned short version_needed;
    unsigned short flags;
    unsigned short compression;
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int crc32;
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned short filename_len;
    unsigned short extra_len;
    unsigned short comment_len;
    unsigned short disk_num;
    unsigned short internal_attrs;
    unsigned int external_attrs;
    unsigned int local_header_offset;
} ZIPCentralHeader;

/* ZIP end of central directory record */
typedef struct {
    unsigned int signature;
    unsigned short disk_num;
    unsigned short central_dir_disk;
    unsigned short disk_entries;
    unsigned short total_entries;
    unsigned int central_dir_size;
    unsigned int central_dir_offset;
    unsigned short comment_len;
} ZIPEndHeader;

#define LOCAL_SIG 0x04034b50
#define CENTRAL_SIG 0x02014b50
#define END_SIG 0x06054b50

/* CRC32 table */
static unsigned int crc_table[256];
static int crc_table_initialized = 0;

static void init_crc_table(void)
{
    unsigned int i, j;
    for(i = 0; i < 256; i++) {
        unsigned int crc = i;
        for(j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc_table[i] = crc;
    }
    crc_table_initialized = 1;
}

static unsigned int compute_crc(const unsigned char *data, int size)
{
    unsigned int crc = 0xFFFFFFFF;
    int i;
    if(!crc_table_initialized)
        init_crc_table();
    for(i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/* Convert time_t to DOS date/time */
static void time_to_dos(time_t t, unsigned short *date, unsigned short *time)
{
    struct tm *tm = localtime(&t);
    if(tm == NULL) {
        *date = 0x0021; /* 1980-01-01 */
        *time = 0;
        return;
    }
    *date = (1980 - 1980) << 9 | (tm->tm_mon + 1) << 5 | tm->tm_mday;
    *time = tm->tm_hour << 11 | tm->tm_min << 5 | tm->tm_sec / 2;
}

ZIPWriter zip_writer_init(FILE *fp)
{
    ZIPWriter writer;
    writer.fp = fp;
    writer.file_count = 0;
    writer.central_dir_offset = 0;
    return writer;
}

int zip_write_file(ZIPWriter *writer, const char *path, const void *data, int size)
{
    ZIPLocalHeader local;
    ZIPCentralHeader central;
    unsigned int crc;
    long long local_start, central_offset;
    int path_len = (int)strlen(path);
    time_t now = time(NULL);
    unsigned short dos_date, dos_time;

    if(writer->fp == NULL || path == NULL)
        return 0;

    time_to_dos(now, &dos_date, &dos_time);
    crc = compute_crc((const unsigned char *)data, size);
    local_start = ftell(writer->fp);

    /* Write local file header */
    memset(&local, 0, sizeof(local));
    local.signature = LOCAL_SIG;
    local.version = 0x000a; /* 1.0 */
    local.flags = 0x0808; /* UTF-8 filename */
    local.compression = 0; /* Store (no compression) */
    local.mod_time = dos_time;
    local.mod_date = dos_date;
    local.crc32 = crc;
    local.compressed_size = size;
    local.uncompressed_size = size;
    local.filename_len = path_len;
    local.extra_len = 0;

    fwrite(&local, 1, sizeof(local), writer->fp);
    fwrite(path, 1, path_len, writer->fp);
    fwrite(data, 1, size, writer->fp);

    /* Write central directory entry immediately after file data */
    central_offset = ftell(writer->fp);

    memset(&central, 0, sizeof(central));
    central.signature = CENTRAL_SIG;
    central.version_made = 0x0014; /* 2.0 */
    central.version_needed = 0x000a; /* 1.0 */
    central.flags = 0x0808;
    central.compression = 0;
    central.mod_time = dos_time;
    central.mod_date = dos_date;
    central.crc32 = crc;
    central.compressed_size = size;
    central.uncompressed_size = size;
    central.filename_len = path_len;
    central.extra_len = 0;
    central.comment_len = 0;
    central.disk_num = 0;
    central.internal_attrs = 0;
    central.external_attrs = 0x81a40000; /* Regular file, read-only */
    central.local_header_offset = local_start;

    fwrite(&central, 1, sizeof(central), writer->fp);
    fwrite(path, 1, path_len, writer->fp);

    /* Record where central directory starts (first entry) */
    if(writer->file_count == 0)
        writer->central_dir_offset = central_offset;

    writer->file_count++;
    return 1;
}

int zip_finalize(ZIPWriter *writer)
{
    ZIPEndHeader end;
    long long central_start, central_end;

    if(writer->fp == NULL)
        return 0;

    central_start = writer->central_dir_offset;
    if(central_start == 0) {
        /* First call - record central dir start */
        central_start = ftell(writer->fp);
    }

    central_end = ftell(writer->fp);

    memset(&end, 0, sizeof(end));
    end.signature = END_SIG;
    end.disk_num = 0;
    end.central_dir_disk = 0;
    end.disk_entries = writer->file_count;
    end.total_entries = writer->file_count;
    end.central_dir_size = central_end - central_start;
    end.central_dir_offset = central_start;
    end.comment_len = 0;

    fwrite(&end, 1, sizeof(end), writer->fp);
    fclose(writer->fp);

    return 1;
}
