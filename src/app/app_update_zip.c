#include "app_update_zip.h"

#include "miniz.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR_ONE(p) _mkdir(p)
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#define MKDIR_ONE(p) mkdir((p), 0755)
#define PATH_SEP '/'
#endif

int
inbe_update_zip_mkdir_p(const char *path)
{
    char partial[1024];
    size_t len;
    size_t i;

    if(path == NULL || path[0] == '\0')
        return 0;
    len = strlen(path);
    if(len >= sizeof(partial))
        return 0;
    memcpy(partial, path, len + 1);
    for(i = 1; i < len; i++) {
        if(partial[i] != '/' && partial[i] != '\\')
            continue;
        partial[i] = '\0';
        if(partial[0] != '\0' && MKDIR_ONE(partial) != 0 && errno != EEXIST)
            return 0;
        partial[i] = path[i];
    }
    if(MKDIR_ONE(partial) != 0 && errno != EEXIST)
        return 0;
    return 1;
}

/* An entry name is safe when it has no drive letter, no leading separator,
 * and no ".." component — anything else could escape dest_dir. */
static int
entry_name_safe(const char *name)
{
    const char *p = name;

    if(name == NULL || name[0] == '\0')
        return 0;
    if(name[0] == '/' || name[0] == '\\')
        return 0;
    if(((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')) &&
       name[1] == ':')
        return 0;
    while(*p != '\0') {
        if(p[0] == '.' && p[1] == '.' &&
           (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
            /* a leading or component ".." — reject; "..." or "..foo" is fine */
            if(p == name || p[-1] == '/' || p[-1] == '\\')
                return 0;
        }
        p++;
    }
    return 1;
}

int
inbe_update_zip_extract(const char *zip_path, const char *dest_dir)
{
    mz_zip_archive zip;
    mz_uint count;
    mz_uint i;
    int ok = 1;

    if(zip_path == NULL || dest_dir == NULL || dest_dir[0] == '\0')
        return 0;
    if(!inbe_update_zip_mkdir_p(dest_dir))
        return 0;
    memset(&zip, 0, sizeof(zip));
    if(!mz_zip_reader_init_file(&zip, zip_path, 0))
        return 0;
    count = mz_zip_reader_get_num_files(&zip);
    for(i = 0; i < count; i++) {
        char name[512];
        mz_zip_archive_file_stat st;
        char dest[1024];

        if(!mz_zip_reader_file_stat(&zip, i, &st)) {
            ok = 0;
            break;
        }
        if(!mz_zip_reader_is_file_a_directory(&zip, i)) {
            if(!entry_name_safe(st.m_filename)) {
                ok = 0;
                break;
            }
            snprintf(name, sizeof(name), "%s", st.m_filename);
            if(snprintf(dest, sizeof(dest), "%s/%s", dest_dir, name) >=
               (int)sizeof(dest)) {
                ok = 0;
                break;
            }
            /* create parent dirs for entries like "bin/inbe.exe" */
            {
                char *last = strrchr(dest, '/');

                if(last != NULL) {
                    *last = '\0';
                    if(!inbe_update_zip_mkdir_p(dest)) {
                        ok = 0;
                        break;
                    }
                    *last = '/';
                }
            }
            if(!mz_zip_reader_extract_to_file(&zip, i, dest, 0)) {
                ok = 0;
                break;
            }
        }
    }
    mz_zip_reader_end(&zip);
    return ok;
}
