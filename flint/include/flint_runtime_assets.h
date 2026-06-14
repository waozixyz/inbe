#ifndef FLINT_RUNTIME_ASSETS_H
#define FLINT_RUNTIME_ASSETS_H

#include <stddef.h>

typedef enum FlintRuntimeAssetStatus {
    FLINT_RUNTIME_ASSET_IDLE = 0,
    FLINT_RUNTIME_ASSET_DOWNLOADING,
    FLINT_RUNTIME_ASSET_READY,
    FLINT_RUNTIME_ASSET_ERROR,
    FLINT_RUNTIME_ASSET_UNSUPPORTED
} FlintRuntimeAssetStatus;

typedef struct FlintRuntimeAssetDownload {
    volatile FlintRuntimeAssetStatus status;
    char url[1024];
    char path[512];
    char error[256];
    long http_status;
    size_t bytes;
    void *platform;
} FlintRuntimeAssetDownload;

typedef int (*FlintRuntimeAssetDownloadBackend)(FlintRuntimeAssetDownload *download,
                                                const char *url,
                                                const char *path);

int flint_runtime_assets_init(const char *app_id);
int flint_runtime_asset_cache_root(const char *app_id, char *out, size_t out_size);
int flint_runtime_asset_ensure_dir(const char *path);
void flint_runtime_asset_set_download_backend(FlintRuntimeAssetDownloadBackend backend);
int flint_runtime_asset_download(FlintRuntimeAssetDownload *download,
                                 const char *url,
                                 const char *path);
const char *flint_runtime_asset_status_text(FlintRuntimeAssetStatus status);

#endif
