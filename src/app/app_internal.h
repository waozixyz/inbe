#ifndef INBE_APP_INTERNAL_H
#define INBE_APP_INTERNAL_H

/*
 * Prototypes for app-module-private functions that cross translation-unit
 * boundaries within src/app/ (app.c <-> app_audio.c / app_fonts.c /
 * app_web_bridge.c). These are intentionally not in the public app.h: they are
 * implementation details of the app layer.
 */

#include "app.h"

/* app_fonts.c */
int  app_running_in_kryon_preview(void);
int  load_locale_font(InbeApp *app);
void unload_locale_font(InbeApp *app);
void discard_locale_font_cpu(InbeApp *app);

/* app_audio.c */
void audio_mixed_meter(void *bufferData, unsigned int frames);
void unload_cue_sounds(InbeApp *app);
void init_audio(InbeApp *app);

#endif /* INBE_APP_INTERNAL_H */
