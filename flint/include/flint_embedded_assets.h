#ifndef FLINT_EMBEDDED_ASSETS_H
#define FLINT_EMBEDDED_ASSETS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlintEmbeddedAsset {
    const char *path;
    const char *mime;
    const unsigned char *data;
    unsigned int size;
} FlintEmbeddedAsset;

extern const FlintEmbeddedAsset flint_embedded_assets[];
extern const unsigned int flint_embedded_asset_count;

const FlintEmbeddedAsset *flint_embedded_asset(const char *path);
char *flint_embedded_asset_text(const char *path);
const char *flint_embedded_asset_extension(const char *path);

#ifdef __cplusplus
}
#endif

#endif
