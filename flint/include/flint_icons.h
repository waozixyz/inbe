#ifndef FLINT_ICONS_H
#define FLINT_ICONS_H

#include "raylib.h"
#include "ui_icon_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlintIconAsset {
    UIIconType type;
    const char *name;
    const unsigned char *data;
    unsigned int size;
} FlintIconAsset;

const FlintIconAsset *flint_icon_asset(UIIconType type);
const FlintIconAsset *flint_icon_asset_by_name(const char *name);
Texture2D flint_load_icon_texture(UIIconType type);
Texture2D flint_load_icon_texture_by_name(const char *name);
void flint_load_all_icons(Texture2D *icons);
void flint_unload_all_icons(Texture2D *icons);

#ifdef __cplusplus
}
#endif

#endif // FLINT_ICONS_H
