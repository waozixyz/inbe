#include "kryon_plan9_libc.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

typedef struct Plan9SqliteFile {
    sqlite3_file base;
    int fd;
    int lock;
    int delete_on_close;
    char path[512];
} Plan9SqliteFile;

static int
p9sqlite_close(sqlite3_file *file)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;

    if(p->fd >= 0) {
        close(p->fd);
        p->fd = -1;
    }
    if(p->delete_on_close && p->path[0] != '\0')
        remove(p->path);
    return SQLITE_OK;
}

static int
p9sqlite_read(sqlite3_file *file, void *buf, int amount, sqlite3_int64 offset)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;
    char *dst = (char *)buf;
    int got;

    if(seek(p->fd, (vlong)offset, 0) < 0)
        return SQLITE_IOERR_READ;
    got = read(p->fd, dst, amount);
    if(got == amount)
        return SQLITE_OK;
    if(got < 0)
        return SQLITE_IOERR_READ;
    memset(dst + got, 0, amount - got);
    return SQLITE_IOERR_SHORT_READ;
}

static int
p9sqlite_write(sqlite3_file *file, const void *buf, int amount, sqlite3_int64 offset)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;
    const char *src = (const char *)buf;
    int done = 0;

    if(seek(p->fd, (vlong)offset, 0) < 0)
        return SQLITE_IOERR_WRITE;
    while(done < amount) {
        int n = write(p->fd, src + done, amount - done);
        if(n <= 0)
            return SQLITE_IOERR_WRITE;
        done += n;
    }
    return SQLITE_OK;
}

static int
p9sqlite_truncate(sqlite3_file *file, sqlite3_int64 size)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;
    Dir d;

    nulldir(&d);
    d.length = (vlong)size;
    return dirfwstat(p->fd, &d) < 0 ? SQLITE_IOERR_TRUNCATE : SQLITE_OK;
}

static int
p9sqlite_sync(sqlite3_file *file, int flags)
{
    (void)file;
    (void)flags;
    return SQLITE_OK;
}

static int
p9sqlite_filesize(sqlite3_file *file, sqlite3_int64 *size)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;
    vlong end = seek(p->fd, 0, 2);

    if(end < 0)
        return SQLITE_IOERR_FSTAT;
    *size = (sqlite3_int64)end;
    return SQLITE_OK;
}

static int
p9sqlite_lock(sqlite3_file *file, int lock)
{
    ((Plan9SqliteFile *)file)->lock = lock;
    return SQLITE_OK;
}

static int
p9sqlite_unlock(sqlite3_file *file, int lock)
{
    ((Plan9SqliteFile *)file)->lock = lock;
    return SQLITE_OK;
}

static int
p9sqlite_reserved(sqlite3_file *file, int *reserved)
{
    (void)file;
    *reserved = 0;
    return SQLITE_OK;
}

static int
p9sqlite_filecontrol(sqlite3_file *file, int op, void *arg)
{
    switch(op) {
    case SQLITE_FCNTL_LOCKSTATE:
        *(int *)arg = ((Plan9SqliteFile *)file)->lock;
        return SQLITE_OK;
    case SQLITE_FCNTL_SIZE_HINT:
    case SQLITE_FCNTL_SYNC:
    case SQLITE_FCNTL_COMMIT_PHASETWO:
        return SQLITE_OK;
    default:
        return SQLITE_NOTFOUND;
    }
}

static int
p9sqlite_sector(sqlite3_file *file)
{
    (void)file;
    return 512;
}

static int
p9sqlite_device(sqlite3_file *file)
{
    (void)file;
    return 0;
}

static const sqlite3_io_methods p9sqlite_io = {
    1,
    p9sqlite_close,
    p9sqlite_read,
    p9sqlite_write,
    p9sqlite_truncate,
    p9sqlite_sync,
    p9sqlite_filesize,
    p9sqlite_lock,
    p9sqlite_unlock,
    p9sqlite_reserved,
    p9sqlite_filecontrol,
    p9sqlite_sector,
    p9sqlite_device
};

static void
p9sqlite_tempname(char *dst, int dst_size)
{
    snprintf(dst, dst_size, "/tmp/inbe-sqlite-%ld-%ld", (long)getpid(),
             (long)time(0));
}

static int
p9sqlite_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *file,
              int flags, int *out_flags)
{
    Plan9SqliteFile *p = (Plan9SqliteFile *)file;
    char temp[512];
    const char *path = name;
    int fd = -1;
    int mode;

    (void)vfs;
    memset(p, 0, sizeof(*p));
    p->fd = -1;

    if(path == 0 || path[0] == '\0') {
        p9sqlite_tempname(temp, sizeof(temp));
        path = temp;
        flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_DELETEONCLOSE;
    }

    mode = (flags & SQLITE_OPEN_READONLY) ? OREAD : ORDWR;
    if((flags & SQLITE_OPEN_CREATE) != 0)
        fd = open((char *)path, mode);
    if(fd < 0 && (flags & SQLITE_OPEN_CREATE) != 0)
        fd = create((char *)path, mode, 0666);
    if(fd < 0 && (flags & SQLITE_OPEN_READONLY) == 0)
        fd = open((char *)path, ORDWR);
    if(fd < 0)
        fd = open((char *)path, OREAD);
    if(fd < 0)
        return SQLITE_CANTOPEN;

    p->fd = fd;
    p->delete_on_close = (flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
    snprintf(p->path, sizeof(p->path), "%s", path);
    p->base.pMethods = &p9sqlite_io;
    if(out_flags != 0)
        *out_flags = flags;
    return SQLITE_OK;
}

static int
p9sqlite_delete(sqlite3_vfs *vfs, const char *name, int sync_dir)
{
    (void)vfs;
    (void)sync_dir;
    if(name == 0)
        return SQLITE_OK;
    if(dirstat((char *)name) == nil)
        return SQLITE_OK;
    return remove((char *)name) < 0 ? SQLITE_IOERR_DELETE : SQLITE_OK;
}

static int
p9sqlite_access(sqlite3_vfs *vfs, const char *name, int flags, int *out)
{
    Dir *d;

    (void)vfs;
    (void)flags;
    d = dirstat((char *)name);
    *out = d != nil;
    if(d != nil)
        free(d);
    return SQLITE_OK;
}

static int
p9sqlite_fullpath(sqlite3_vfs *vfs, const char *name, int out_size, char *out)
{
    (void)vfs;
    if(name == 0)
        name = "";
    snprintf(out, out_size, "%s", name);
    return SQLITE_OK;
}

static void *
p9sqlite_dlopen(sqlite3_vfs *vfs, const char *name)
{
    (void)vfs;
    (void)name;
    return 0;
}

static void
p9sqlite_dlerror(sqlite3_vfs *vfs, int n, char *msg)
{
    (void)vfs;
    if(n > 0 && msg != 0)
        msg[0] = '\0';
}

static void
(*p9sqlite_dlsym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void)
{
    (void)vfs;
    (void)handle;
    (void)symbol;
    return 0;
}

static void
p9sqlite_dlclose(sqlite3_vfs *vfs, void *handle)
{
    (void)vfs;
    (void)handle;
}

static int
p9sqlite_random(sqlite3_vfs *vfs, int n, char *out)
{
    int fd;
    int got = 0;
    int i;

    (void)vfs;
    fd = open("/dev/random", OREAD);
    if(fd >= 0) {
        got = read(fd, out, n);
        close(fd);
        if(got < 0)
            got = 0;
    }
    for(i = got; i < n; i++)
        out[i] = (char)(time(0) + getpid() + i * 37);
    return n;
}

static int
p9sqlite_sleep(sqlite3_vfs *vfs, int microseconds)
{
    (void)vfs;
    sleep(microseconds / 1000);
    return microseconds;
}

static int
p9sqlite_time64(sqlite3_vfs *vfs, sqlite3_int64 *now)
{
    (void)vfs;
    *now = ((sqlite3_int64)time(0)) * 1000 + (sqlite3_int64)210866760000000LL;
    return SQLITE_OK;
}

static int
p9sqlite_time(sqlite3_vfs *vfs, double *now)
{
    sqlite3_int64 i;
    int rc = p9sqlite_time64(vfs, &i);
    *now = (double)i / 86400000.0;
    return rc;
}

static int
p9sqlite_lasterror(sqlite3_vfs *vfs, int n, char *msg)
{
    (void)vfs;
    if(n > 0 && msg != 0)
        msg[0] = '\0';
    return 0;
}

static sqlite3_vfs p9sqlite_vfs = {
    2,
    sizeof(Plan9SqliteFile),
    512,
    0,
    "plan9",
    0,
    p9sqlite_open,
    p9sqlite_delete,
    p9sqlite_access,
    p9sqlite_fullpath,
    p9sqlite_dlopen,
    p9sqlite_dlerror,
    p9sqlite_dlsym,
    p9sqlite_dlclose,
    p9sqlite_random,
    p9sqlite_sleep,
    p9sqlite_time,
    p9sqlite_lasterror,
    p9sqlite_time64
};

int
sqlite3_os_init(void)
{
    return sqlite3_vfs_register(&p9sqlite_vfs, 1);
}

int
sqlite3_os_end(void)
{
    return SQLITE_OK;
}
