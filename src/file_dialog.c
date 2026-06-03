#include "file_dialog.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(_WIN32)
/* Windows headers must be included first, before any raylib includes */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

/* Simple DirectoryExists implementation to avoid pulling in raylib.h */
/* Only used on Linux */
#if !defined(_WIN32) && !defined(__APPLE__)
static int DirectoryExists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}
#endif

void file_dialog_init(FileDialog *dlg)
{
    if(dlg == NULL)
        return;
    dlg->active = 0;
    dlg->mode = 0;
    dlg->path[0] = '\0';
    dlg->filename[0] = '\0';
    dlg->cursor = 0;
}

static void copy_text_bounded(char *dst, size_t dst_size, const char *src)
{
    size_t len = 0;

    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";

    while(len + 1 < dst_size && src[len] != '\0')
        len++;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void join_path_bounded(char *dst, size_t dst_size, const char *base, const char *name, char sep)
{
    size_t len = 0;

    if(dst == NULL || dst_size == 0)
        return;

    dst[0] = '\0';
    if(base == NULL || base[0] == '\0') {
        copy_text_bounded(dst, dst_size, name);
        return;
    }

    while(len + 1 < dst_size && base[len] != '\0') {
        dst[len] = base[len];
        len++;
    }
    if(len + 1 < dst_size && len > 0 && dst[len - 1] != '/' && dst[len - 1] != '\\')
        dst[len++] = sep;
    for(size_t i = 0; len + 1 < dst_size && name != NULL && name[i] != '\0'; i++)
        dst[len++] = name[i];
    dst[len] = '\0';
}

static void get_default_export_path(char *path, size_t size, const char *filename)
{
    (void)filename; /* Will use in the Windows path */

#if defined(_WIN32)
    char home[MAX_PATH];
    /* CSIDL_DOWNLOADS = 0x0006 */
    if(SHGetFolderPathA(NULL, 0x0006, NULL, 0, home) == S_OK) {
        join_path_bounded(path, size, home, filename, '\\');
    } else {
        copy_text_bounded(path, size, filename);
    }
#elif defined(__APPLE__)
    const char *base = getenv("HOME");
    if(base != NULL) {
        char downloads[512];
        join_path_bounded(downloads, sizeof(downloads), base, "Downloads", '/');
        join_path_bounded(path, size, downloads, filename, '/');
    } else {
        copy_text_bounded(path, size, filename);
    }
#else
    /* Linux and other Unix-like systems */
    const char *base = getenv("HOME");
    if(base != NULL) {
        /* Try Downloads first, fall back to home */
        char downloads[512];
        join_path_bounded(downloads, sizeof(downloads), base, "Downloads", '/');
        if(DirectoryExists(downloads)) {
            join_path_bounded(path, size, downloads, filename, '/');
        } else {
            join_path_bounded(path, size, base, filename, '/');
        }
    } else {
        /* Fallback to current directory */
        struct passwd *pw = getpwuid(getuid());
        if(pw != NULL && pw->pw_dir != NULL) {
            join_path_bounded(path, size, pw->pw_dir, filename, '/');
        } else {
            copy_text_bounded(path, size, filename);
        }
    }
#endif
}

int file_dialog_save(FileDialog *dlg, const char *title, const char *default_filename)
{
    char full_path[512];
    time_t now;
    struct tm *tm;
    char date_str[32];
    char generated_filename[128];

    (void)title; /* Not used in simple implementation */
    (void)default_filename; /* Will generate our own */

    if(dlg == NULL)
        return 0;

    /* Generate filename with timestamp */
    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        snprintf(generated_filename, sizeof(generated_filename),
                 "inbe-export-%s.zip", date_str);
    } else {
        strcpy(generated_filename, "inbe-export.zip");
    }

    /* Get default export path */
    get_default_export_path(full_path, sizeof(full_path), generated_filename);

    /* Store the path */
    strncpy(dlg->path, full_path, sizeof(dlg->path) - 1);
    dlg->path[sizeof(dlg->path) - 1] = '\0';

    /* Extract filename for display */
    const char *slash = strrchr(full_path, '/');
    if(slash == NULL)
        slash = strrchr(full_path, '\\');
    if(slash != NULL) {
        strncpy(dlg->filename, slash + 1, sizeof(dlg->filename) - 1);
    } else {
        strncpy(dlg->filename, generated_filename, sizeof(dlg->filename) - 1);
    }
    dlg->filename[sizeof(dlg->filename) - 1] = '\0';

    /* Auto-confirm for simple implementation */
    return 1;
}

const char *file_dialog_get_path(FileDialog *dlg)
{
    if(dlg == NULL)
        return NULL;
    return dlg->path;
}
