#include "flint_icons.h"
#include "flint_ui.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

extern const FlintIconAsset flint_icon_assets[];
extern const unsigned int flint_icon_asset_count;

const FlintIconAsset *
flint_icon_asset(UIIconType type)
{
    for(unsigned int i = 0; i < flint_icon_asset_count; i++) {
        if(flint_icon_assets[i].type == type)
            return &flint_icon_assets[i];
    }
    return NULL;
}

const FlintIconAsset *
flint_icon_asset_by_name(const char *name)
{
    if(name == NULL)
        return NULL;

    for(unsigned int i = 0; i < flint_icon_asset_count; i++) {
        if(strcmp(flint_icon_assets[i].name, name) == 0)
            return &flint_icon_assets[i];
    }
    return NULL;
}

static Texture2D
load_icon_asset_texture(const FlintIconAsset *asset)
{
    Texture2D texture = {0};
    Image image;

    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return texture;

    image = LoadImageFromMemory(".png", asset->data, (int)asset->size);
    if(image.data == NULL)
        return texture;

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if(texture.id != 0)
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D
flint_load_icon_texture(UIIconType type)
{
    return load_icon_asset_texture(flint_icon_asset(type));
}

Texture2D
flint_load_icon_texture_by_name(const char *name)
{
    return load_icon_asset_texture(flint_icon_asset_by_name(name));
}

