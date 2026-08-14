#include "app.h"
#include "app_internal.h"
#include "practices/meditation/meditation_practice.h"
#include <stdio.h>
#include <string.h>

static volatile int g_audio_meter_peak_milli;
static void SafeUnloadSound(Sound sound);

static Sound
load_sound_asset(const char *name)
{
    char path[96];
    const EmbeddedAsset *asset;
    Wave wave;
    Sound sound = {0};

    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    asset = GetEmbeddedAsset(path);
    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Missing embedded sound asset: %s", path);
        return sound;
    }

    wave = LoadWaveFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(wave.data == NULL) {
        TraceLog(LOG_ERROR, "AUDIO: Failed to decode embedded sound asset: %s", path);
        return sound;
    }

    WaveFormat(&wave, 44100, 16, 2);
    sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if(sound.frameCount == 0)
        TraceLog(LOG_ERROR, "AUDIO: Failed to create sound from embedded asset: %s", path);
    else
        TraceLog(LOG_INFO, "AUDIO: Loaded sound asset %s (%u frames)", path, sound.frameCount);
    return sound;
}

static float
app_sound_volume_scale(InbeApp *app, float scale)
{
    float volume;

    if(app == NULL || app->sound_volume <= 0)
        return 0.0f;

    volume = ((float)app->sound_volume / 100.0f) * scale;
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;
    return volume;
}

void
audio_mixed_meter(void *bufferData, unsigned int frames)
{
    float *samples = bufferData;
    float peak = 0.0f;
    int peak_milli;
    unsigned int count;

    if(samples == NULL || frames == 0)
        return;

    count = frames * 2;
    for(unsigned int i = 0; i < count; i++) {
        float sample = samples[i];
        if(sample < 0.0f)
            sample = -sample;
        if(sample > peak)
            peak = sample;
    }
    if(peak > 1.0f)
        peak = 1.0f;
    peak_milli = (int)(peak * 1000.0f);
    if(peak_milli > g_audio_meter_peak_milli)
        g_audio_meter_peak_milli = peak_milli;
}

static float
app_breath_cue_pitch(InbeApp *app)
{
    int default_ticks = breath_half_ticks_for_speed(DefaultSpeedLevel);
    int half_ticks;

    if(app == NULL)
        return 1.0f;
    half_ticks = app->inbe.breath_half_ticks;
    if(half_ticks <= 0)
        half_ticks = breath_half_ticks_for_speed(app->inbe.speed_level);
    if(half_ticks <= 0)
        half_ticks = default_ticks;
    if(default_ticks <= 0)
        return 1.0f;

    return (float)default_ticks / (float)half_ticks;
}

static void
app_play_sound_pitch(InbeApp *app, Sound sound, float scale, float pitch)
{
    float volume;

    if(app == NULL)
        return;
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because audio device is not ready");
        return;
    }
    if(sound.frameCount == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because sound is not loaded");
        return;
    }
    volume = app_sound_volume_scale(app, scale);
    if(volume <= 0.0f)
        return;
    if(pitch < 0.75f)
        pitch = 0.75f;
    if(pitch > 1.45f)
        pitch = 1.45f;

    StopSound(sound);
    SetSoundVolume(sound, volume);
    SetSoundPitch(sound, pitch);
    PlaySound(sound);
    if(!IsSoundPlaying(sound))
        TraceLog(LOG_ERROR, "AUDIO: PlaySound returned but sound is not playing");
}

void
app_play_sound(InbeApp *app, Sound sound, float scale)
{
    app_play_sound_pitch(app, sound, scale, 1.0f);
}

void
app_play_breath_cue(InbeApp *app, int dir)
{
    Sound sound;
    float scale = 1.25f;

    if(app == NULL)
        return;

    sound = dir == 0 ? app->breath_in_sound : app->breath_out_sound;
    app_play_sound_pitch(app, sound, scale, app_breath_cue_pitch(app));
}

void
app_play_bell_cue(InbeApp *app, float scale)
{
    if(app == NULL)
        return;
    app_play_sound(app, app->bell_sound, scale);
}

int
app_bell_cue_playing(InbeApp *app)
{
    if(app == NULL || !app->audio_ready || app->bell_sound.frameCount == 0)
        return 0;
    return IsSoundPlaying(app->bell_sound) ? 1 : 0;
}

float
app_audio_output_level(InbeApp *app)
{
    float latest;

    if(app == NULL || !app->audio_ready)
        return 0.0f;

    latest = (float)g_audio_meter_peak_milli / 1000.0f;
    g_audio_meter_peak_milli = 0;
    if(latest > app->audio_meter_level)
        app->audio_meter_level = latest;
    else
        app->audio_meter_level *= 0.86f;

    if(app->audio_meter_level < 0.01f)
        app->audio_meter_level = 0.0f;
    if(app->audio_meter_level > 1.0f)
        app->audio_meter_level = 1.0f;
    return app->audio_meter_level;
}

void
unload_cue_sounds(InbeApp *app)
{
    if(app == NULL)
        return;
    SafeUnloadSound(app->breath_in_sound);
    SafeUnloadSound(app->breath_out_sound);
    SafeUnloadSound(app->bell_sound);
    app->breath_in_sound = (Sound){0};
    app->breath_out_sound = (Sound){0};
    app->bell_sound = (Sound){0};
}

static Sound
load_cue_sound(InbeApp *app, int cue)
{
    char path[FS_PATH_MAX];
    Wave wave;
    Sound sound = {0};

    if(app == NULL)
        return sound;
    if(app_audio_cue_path(app, cue, path, sizeof(path))) {
        wave = LoadWave(path);
        if(wave.data != NULL) {
            WaveFormat(&wave, 44100, 16, 2);
            sound = LoadSoundFromWave(wave);
            UnloadWave(wave);
            if(sound.frameCount != 0)
                return sound;
        }
    }
    return load_sound_asset(app_audio_cue_default_asset(cue));
}

void
app_audio_reload_cue_sounds(InbeApp *app)
{
    if(app == NULL || !app->audio_ready)
        return;
    unload_cue_sounds(app);
    app->breath_in_sound = load_cue_sound(app, INBE_AUDIO_CUE_BREATH_IN);
    app->breath_out_sound = load_cue_sound(app, INBE_AUDIO_CUE_BREATH_OUT);
    app->bell_sound = load_cue_sound(app, INBE_AUDIO_CUE_BELL);
}

void
init_audio(InbeApp *app)
{
    if(app == NULL || app->audio_ready)
        return;

    InitAudioDevice();
    app->audio_ready = IsAudioDeviceReady();
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Audio device failed to initialize");
        return;
    }
    TraceLog(LOG_INFO, "AUDIO: Audio device initialized");

    app_audio_reload_cue_sounds(app);
    AttachAudioMixedProcessor(audio_mixed_meter);
    app->audio_meter_attached = 1;
}

int
app_audio_reinitialize(InbeApp *app)
{
    if(app == NULL)
        return 0;

    meditation_music_unload(app);
    if(app->audio_ready && app->audio_meter_attached) {
        DetachAudioMixedProcessor(audio_mixed_meter);
        app->audio_meter_attached = 0;
    }
    unload_cue_sounds(app);
    if(app->audio_ready) {
        CloseAudioDevice();
        app->audio_ready = 0;
    }
    app->audio_meter_level = 0.0f;
    g_audio_meter_peak_milli = 0;

    if(!app_running_in_kryon_preview())
        init_audio(app);
    return app->audio_ready ? 1 : 0;
}

static void SafeUnloadSound(Sound sound) {
    if (sound.frameCount != 0) {
        UnloadSound(sound);
    }
}
