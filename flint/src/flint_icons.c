#include "flint_icons.h"
#include "flint_ui.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
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

void
flint_load_all_icons(Texture2D *icons)
{
    if(icons == NULL)
        return;

    static const char *icon_names[UI_ICON_TYPE_COUNT] = {
        NULL,                 // UI_ICON_TYPE_NONE (no icon)

        // Core UI icons
        "gear.png",            // UI_ICON_TYPE_GEAR
        "x.png",               // UI_ICON_TYPE_X
        "x-red.png",           // UI_ICON_TYPE_X_RED
        "manual.png",          // UI_ICON_TYPE_MANUAL
        "return.png",          // UI_ICON_TYPE_RETURN
        "backward.png",        // UI_ICON_TYPE_BACKWARD
        "forward.png",         // UI_ICON_TYPE_FORWARD
        "play.png",            // UI_ICON_TYPE_PLAY
        "pause.png",           // UI_ICON_TYPE_PAUSE
        "stat.png",            // UI_ICON_TYPE_STAT
        "home.png",            // UI_ICON_TYPE_HOME
        "trash.png",           // UI_ICON_TYPE_TRASH
        "edit.png",            // UI_ICON_TYPE_PENCIL
        "save.png",            // UI_ICON_TYPE_SAVE
        "wrench.png",          // UI_ICON_TYPE_WRENCH
        "plus.png",            // UI_ICON_TYPE_PLUS
        "stack.png",           // UI_ICON_TYPE_STACK
        "calendar.png",        // UI_ICON_TYPE_CALENDAR

        // Social & payment icons
        "discord.png",         // UI_ICON_TYPE_DISCORD
        "telegram.png",        // UI_ICON_TYPE_TELEGRAM
        "github.png",          // UI_ICON_TYPE_GITHUB
        "globe.png",           // UI_ICON_TYPE_GLOBE
        "stripe.png",          // UI_ICON_TYPE_STRIPE
        "btc.png",             // UI_ICON_TYPE_BTC
        "monero.png",          // UI_ICON_TYPE_MONERO

        // Sound icons
        "sound.png",           // UI_ICON_TYPE_SOUND
        "sound0.png",          // UI_ICON_TYPE_SOUND0
        "sound1.png",          // UI_ICON_TYPE_SOUND1
        "sound2.png",          // UI_ICON_TYPE_SOUND2
        "sound3.png",          // UI_ICON_TYPE_SOUND3
        "mute.png",            // UI_ICON_TYPE_MUTE
        "music.png",           // UI_ICON_TYPE_MUSIC

        // Habit & practice icons
        "habit.png",            // UI_ICON_TYPE_HABIT
        "amen.png",            // UI_ICON_TYPE_AMEN
        "weekly.png",          // UI_ICON_TYPE_WEEKLY
        "inbe.png",            // UI_ICON_TYPE_INBE

        // Meditation & theme icons
        "droid.png",           // UI_ICON_TYPE_DROID
        "fdroid.png",          // UI_ICON_TYPE_FDROID
        "lighton.png",         // UI_ICON_TYPE_LIGHTON
        "lightoff.png",        // UI_ICON_TYPE_LIGHTOFF
        "moon.png",            // UI_ICON_TYPE_MOON
        "sun.png",             // UI_ICON_TYPE_SUN
        "jupiter.png",         // UI_ICON_TYPE_JUPITER
        "mars.png",            // UI_ICON_TYPE_MARS
        "mercury.png",         // UI_ICON_TYPE_MERCURY
        "venus.png",           // UI_ICON_TYPE_VENUS
        "saturn.png",          // UI_ICON_TYPE_SATURN

        // Navigation & utility icons
        "link.png",            // UI_ICON_TYPE_LINK
        "edit.png",            // UI_ICON_TYPE_EDIT
        "eye-closed.png",      // UI_ICON_TYPE_EYE_CLOSED
        "check.png",           // UI_ICON_TYPE_CHECK
        "quest.png",           // UI_ICON_TYPE_QUEST
        "routine.png",         // UI_ICON_TYPE_ROUTINE
        "timeline.png",        // UI_ICON_TYPE_TIMELINE
        "todos.png",           // UI_ICON_TYPE_TODOS
        "tile.png",            // UI_ICON_TYPE_TILE
        "tile2.png",           // UI_ICON_TYPE_TILE2
        "tile3.png",           // UI_ICON_TYPE_TILE3
        "tile4.png",           // UI_ICON_TYPE_TILE4
        "text.png",            // UI_ICON_TYPE_TEXT

        // Platform & store icons
        "itch.png",            // UI_ICON_TYPE_ITCH
        "playstore.png",       // UI_ICON_TYPE_PLAYSTORE
        "tux.png",             // UI_ICON_TYPE_TUX
        "win.png",             // UI_ICON_TYPE_WIN
        "uxn.png",             // UI_ICON_TYPE_UXN
        "wasm.png",            // UI_ICON_TYPE_WASM
        "wasm4.png",           // UI_ICON_TYPE_WASM4
        "ray.png",             // UI_ICON_TYPE_RAY
        "rocket.png",          // UI_ICON_TYPE_ROCKET
        "srht.png",            // UI_ICON_TYPE_SRHT
        "tcl.png",             // UI_ICON_TYPE_TCL
    };

    for(int i = 1; i < UI_ICON_TYPE_COUNT; i++) {
        if(icon_names[i] != NULL && icons[i].id == 0) {
            char icon_name[64];
            char *ext;
            snprintf(icon_name, sizeof(icon_name), "%s", icon_names[i]);
            ext = strrchr(icon_name, '.');
            if(ext != NULL && strcmp(ext, ".png") == 0)
                *ext = '\0';
            icons[i] = flint_load_icon_texture_by_name(icon_name);
        }
    }
}

void
flint_unload_all_icons(Texture2D *icons)
{
    if(icons == NULL)
        return;

    for(int i = 0; i < UI_ICON_TYPE_COUNT; i++) {
        if(icons[i].id != 0) {
            UnloadTexture(icons[i]);
            icons[i].id = 0;
        }
    }
}
