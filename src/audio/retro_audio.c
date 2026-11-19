#include "audio/retro_audio.h"

#include <math.h>
#include <string.h>

void retro_audio_init(RetroAudioSynth *synth) {
    if (!synth) return;
    memset(synth, 0, sizeof(*synth));
    synth->rng_state = 0x20261201u;
}

static void add_voice(RetroAudioSynth *s, float freq, float slide, float dur, float vol, int wave) {
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!s->voices[i].active) {
            SfxVoice *v = &s->voices[i];
            v->active = true;
            v->phase = 0.0f;
            v->freq = freq;
            v->freq_slide = slide;
            v->time_left = dur;
            v->duration = dur;
            v->volume = vol;
            v->wave = wave;
            return;
        }
    }
}

void retro_audio_trigger_events(RetroAudioSynth *synth, uint32_t ev) {
    if (!synth || ev == 0u) return;
    if (ev & SFX_PISTOL)  add_voice(synth, 620.0f, -1800.0f, 0.09f, 0.28f, 2);
    if (ev & SFX_SHOTGUN) add_voice(synth, 240.0f,  -620.0f, 0.24f, 0.42f, 2);
    if (ev & SFX_SHRINK)  add_voice(synth, 1100.0f, -750.0f, 0.28f, 0.24f, 0);
    if (ev & SFX_STOMP)   add_voice(synth, 160.0f,  -120.0f, 0.18f, 0.35f, 1);
    if (ev & SFX_THROW)   add_voice(synth, 320.0f,   240.0f, 0.12f, 0.20f, 3);
    if (ev & SFX_EXPLODE) add_voice(synth, 110.0f,   -85.0f, 0.42f, 0.42f, 2);
    if (ev & SFX_JETPACK) add_voice(synth, 190.0f,    20.0f, 0.07f, 0.10f, 2);
    if (ev & SFX_PICKUP)  add_voice(synth, 523.0f,   680.0f, 0.16f, 0.22f, 0);
    if (ev & SFX_SWITCH)  add_voice(synth, 440.0f,   440.0f, 0.14f, 0.24f, 0);
    if (ev & SFX_PORTAL)  add_voice(synth, 280.0f,   520.0f, 0.35f, 0.26f, 3);
    if (ev & SFX_HURT)    add_voice(synth, 220.0f,  -180.0f, 0.22f, 0.30f, 1);
    if (ev & SFX_SHARD)   add_voice(synth, 587.0f,   880.0f, 0.55f, 0.32f, 3);
    if (ev & SFX_GLASS)   add_voice(synth, 1800.0f, -900.0f, 0.15f, 0.26f, 2);
    if (ev & SFX_DOOR)    add_voice(synth, 140.0f,   110.0f, 0.26f, 0.22f, 1);
    if (ev & SFX_SECRET)  add_voice(synth, 659.0f,   980.0f, 0.40f, 0.28f, 3);
}

static float next_noise(RetroAudioSynth *s) {
    s->rng_state = s->rng_state * 1664525u + 1013904223u;
    return ((float)((s->rng_state >> 16) & 0xFFFFu) / 32768.0f) - 1.0f;
}

void retro_audio_render_samples(RetroAudioSynth *synth, SceneId scene, int shard_count, float *out_mono, size_t sample_count) {
    if (!synth || !out_mono || sample_count == 0) return;
    if (synth->muted) {
        memset(out_mono, 0, sample_count * sizeof(float));
        return;
    }

    /* Cozy festive music-box notes in Hub; punchy 90s action riff in Day 1 */
    static const float kHubNotes[16] = {
        329.6f, 329.6f, 329.6f, 0.0f, 329.6f, 329.6f, 329.6f, 0.0f,
        329.6f, 392.0f, 261.6f, 293.7f, 329.6f, 0.0f, 392.0f, 440.0f
    };
    static const float kDukeBass[16] = {
        110.0f, 110.0f, 130.8f, 110.0f, 146.8f, 110.0f, 130.8f, 98.0f,
        110.0f, 110.0f, 164.8f, 146.8f, 130.8f, 110.0f, 98.0f,  110.0f
    };

    const float dt = 1.0f / (float)AUDIO_SAMPLE_RATE;
    for (size_t i = 0; i < sample_count; ++i) {
        synth->song_time += dt;
        int step = (int)(synth->song_time * (scene == SCENE_DAY01 ? 6.0f : 3.2f)) & 15;
        float step_frac = fmodf(synth->song_time * (scene == SCENE_DAY01 ? 6.0f : 3.2f), 1.0f);
        float env = (step_frac < 0.85f) ? (1.0f - step_frac * 0.9f) : 0.0f;

        float music = 0.0f;
        if (scene == SCENE_DAY01) {
            float bf = kDukeBass[step];
            synth->bass_phase += bf * dt;
            if (synth->bass_phase >= 1.0f) synth->bass_phase -= 1.0f;
            float saw = (synth->bass_phase * 2.0f - 1.0f);
            music = saw * 0.08f * env;
        } else {
            float nf = kHubNotes[step];
            if (nf > 0.0f) {
                synth->music_phase += nf * dt;
                if (synth->music_phase >= 1.0f) synth->music_phase -= 1.0f;
                music = sinf(synth->music_phase * 2.0f * SW_PI) * 0.06f * env;
                if (shard_count > 0) {
                    /* Add bright festive shimmer overtone as shards are collected */
                    music += sinf(synth->music_phase * 4.0f * SW_PI) * 0.025f * env;
                }
            }
        }

        float sfx = 0.0f;
        for (int vi = 0; vi < AUDIO_MAX_VOICES; ++vi) {
            SfxVoice *v = &synth->voices[vi];
            if (!v->active) continue;
            v->time_left -= dt;
            if (v->time_left <= 0.0f) {
                v->active = false;
                continue;
            }
            v->freq += v->freq_slide * dt;
            if (v->freq < 40.0f) v->freq = 40.0f;
            v->phase += v->freq * dt;
            if (v->phase >= 1.0f) v->phase -= floorf(v->phase);

            float gain = v->volume * (v->time_left / v->duration);
            float sample = 0.0f;
            if (v->wave == 0) {
                sample = (v->phase < 0.5f) ? 1.0f : -1.0f;
            } else if (v->wave == 1) {
                sample = v->phase * 2.0f - 1.0f;
            } else if (v->wave == 2) {
                sample = next_noise(synth);
            } else {
                sample = sinf(v->phase * 2.0f * SW_PI);
            }
            sfx += sample * gain;
        }

        out_mono[i] = sw_clampf(music + sfx, -0.95f, 0.95f);
    }
}
