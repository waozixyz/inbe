#include "flint_icons.h"
#include "flint_color.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

extern const FlintIconAsset flint_icon_assets[];
extern const unsigned int flint_icon_asset_count;

const FlintIconAsset *
flint_icon_asset(FlintIconType type)
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
flint_load_icon_texture(FlintIconType type)
{
    return load_icon_asset_texture(flint_icon_asset(type));
}

Texture2D
flint_load_icon_texture_by_name(const char *name)
{
    return load_icon_asset_texture(flint_icon_asset_by_name(name));
}

void
flint_draw_icon_fallback(FlintIconType type, int x, int y, int size, Color color)
{
    int center = size / 2;
    int thickness = size / 8;
    if(thickness < 1) thickness = 1;

    switch(type) {
    case FLINT_ICON_TYPE_X: {
        int p = thickness / 2;
        DrawLine(x + p, y + p, x + size - p, y + size - p, color);
        DrawLine(x + size - p, y + p, x + p, y + size - p, color);
        break;
    }
    case FLINT_ICON_TYPE_GEAR: {
        int outer = size / 2 - thickness;
        int inner = size / 4;
        DrawCircle(x + center, y + center, outer, color);
        for(int i = 0; i < 6; i++) {
            float angle = i * 60.0f * DEG2RAD;
            int cx = x + center + (int)(cos(angle) * inner);
            int cy = y + center + (int)(sin(angle) * inner);
            DrawLine(x + center, y + center, cx, cy, color);
        }
        break;
    }
    case FLINT_ICON_TYPE_BACKWARD: {
        int p = thickness;
        DrawTriangle(
            (Vector2){x + size - p, y + p},
            (Vector2){x + p, y + center},
            (Vector2){x + size - p, y + size - p},
            color
        );
        break;
    }
    case FLINT_ICON_TYPE_FORWARD: {
        int p = thickness;
        DrawTriangle(
            (Vector2){x + p, y + p},
            (Vector2){x + size - p, y + center},
            (Vector2){x + p, y + size - p},
            color
        );
        break;
    }
    case FLINT_ICON_TYPE_PLAY: {
        int p = thickness;
        DrawTriangle(
            (Vector2){x + p, y + p},
            (Vector2){x + size - p, y + center},
            (Vector2){x + p, y + size - p},
            color
        );
        break;
    }
    case FLINT_ICON_TYPE_PAUSE: {
        int w = size / 3;
        int p = (size - w * 2) / 3;
        DrawRectangle(x + p, y + thickness, w, size - thickness * 2, color);
        DrawRectangle(x + p + w + p, y + thickness, w, size - thickness * 2, color);
        break;
    }
    case FLINT_ICON_TYPE_RETURN:
    case FLINT_ICON_TYPE_HOME: {
        int p = thickness;
        DrawLine(x + center, y + size - p, x + center, y + p, color);
        DrawLine(x + center, y + p, x + p, y + p + (size / 3), color);
        DrawLine(x + center, y + p, x + size - p, y + p + (size / 3), color);
        if(type == FLINT_ICON_TYPE_RETURN) {
            DrawLine(x + p, y + center, x + size - p, y + center, color);
        }
        break;
    }
    case FLINT_ICON_TYPE_TRASH: {
        int p = thickness;
        DrawRectangle(x + p * 2, y + size / 3, size - p * 4, size - p * 3, color);
        DrawRectangle(x + size / 3, y + p, size / 3, p, color);
        DrawLine(x + p, y + size / 4, x + size - p, y + size / 4, color);
        Color dark_color = flint_darken(color, 70);
        DrawLine(x + size / 3, y + size / 3 + p, x + size / 3, y + size - p * 2, dark_color);
        DrawLine(x + size * 2 / 3, y + size / 3 + p, x + size * 2 / 3, y + size - p * 2, dark_color);
        break;
    }
    case FLINT_ICON_TYPE_PENCIL: {
        int p = thickness;
        DrawLineEx((Vector2){x + p * 2, y + size - p * 2},
                   (Vector2){x + size - p * 2, y + p * 2},
                   (float)(thickness + 1), color);
        DrawTriangle((Vector2){x + size - p * 2, y + p * 2},
                     (Vector2){x + size - p, y + p},
                     (Vector2){x + size - p, y + p * 3},
                     color);
        DrawLine(x + p, y + size - p, x + p * 3, y + size - p, color);
        break;
    }
    case FLINT_ICON_TYPE_SAVE: {
        int p = thickness;
        DrawLineEx((Vector2){x + p * 2, y + center},
                   (Vector2){x + center - p, y + size - p * 2},
                   (float)(thickness + 1), color);
        DrawLineEx((Vector2){x + center - p, y + size - p * 2},
                   (Vector2){x + size - p, y + p * 2},
                   (float)(thickness + 1), color);
        break;
    }
    case FLINT_ICON_TYPE_STACK: {
        int p = thickness;
        int layer_h = size / 4;
        int gap = thickness;
        int w = size - p * 2;
        int top_y = y + p * 2;
        DrawRectangleLinesEx((Rectangle){(float)(x + p), (float)top_y, (float)w, (float)layer_h},
                             (float)thickness, color);
        DrawRectangleLinesEx((Rectangle){(float)(x + p), (float)(top_y + layer_h + gap),
                                         (float)w, (float)layer_h},
                             (float)thickness, color);
        DrawRectangleLinesEx((Rectangle){(float)(x + p), (float)(top_y + (layer_h + gap) * 2),
                                         (float)w, (float)layer_h},
                             (float)thickness, color);
        break;
    }
    case FLINT_ICON_TYPE_MANUAL:
    case FLINT_ICON_TYPE_STAT: {
        DrawRectangle(x + thickness, y + thickness, size - thickness * 2, size - thickness * 2, color);
        DrawLine(x + thickness, y + thickness + size / 3, x + size - thickness, y + thickness + size / 3, color);
        DrawLine(x + thickness, y + thickness + size * 2 / 3, x + size - thickness, y + thickness + size * 2 / 3, color);
        break;
    }
    default: {
        DrawCircle(x + center, y + center, size / 3, color);
        break;
    }
    }
}
