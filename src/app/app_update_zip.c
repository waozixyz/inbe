#include "app_update_zip.h"

#include "kry_archive.h"

int
inbe_update_zip_mkdir_p(const char *path)
{
    return KryArchiveMkdirP(path);
}

int
inbe_update_zip_extract(const char *zip_path, const char *dest_dir)
{
    return KryArchiveExtractZip(zip_path, dest_dir);
}
