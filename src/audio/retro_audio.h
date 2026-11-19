#ifndef SWIETA_RETRO_AUDIO_H
#define SWIETA_RETRO_AUDIO_H

#include "sim/world.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_MAX_VOICES 8

typedef struct {
    bool active;
    float phase;
    float freq;
    float freq_slide;
    float time_left;
    float duration;
    float volume;
    int wave; /* 0=square, 1=saw, 2=noise, 3=sine */
} SfxVoice;

typedef struct {
    bool muted;
    float music_phase;
    float bass_phase;
    float song_time;
    uint32_t rng_state;
    SfxVoice voices[AUDIO_MAX_VOICES];
} RetroAudioSynth;

void retro_audio_init(RetroAudioSynth *synth);
void retro_audio_trigger_events(RetroAudioSynth *synth, uint32_t sound_events);
void retro_audio_render_samples(RetroAudioSynth *synth, SceneId scene, int shard_count, float *out_mono, size_t sample_count);

#endif /* SWIETA_RETRO_AUDIO_H */
