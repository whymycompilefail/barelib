/**********************************************************************************************
*
*   ai_audio.c - AI adapter procedural audio subsystem
*
*   M4 code-comprehensibility refactor: split from raylib_ai_adapter.c.
*   Owns the procedural audio slots (AI-generated sound effects): tone/noise synthesis
*   with fade in/out, per-handle control (play/stop/pause/resume/volume/pitch), and the
*   per-frame auto-recycle + shutdown cleanup that AIUpdate/AIClose delegate here.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "ai_adapter_internal.h"
#include "raylib.h"          // Required for: AudioStream, LoadAudioStream(), etc.
#include "config.h"

#include <stdlib.h>          // Required for: rand(), RAND_MAX
#include <string.h>          // (empty for now, kept for symmetry)
#include <math.h>            // Required for: sinf(), fabsf(), floorf()

#ifndef PI
#define PI 3.14159265358979323846f
#endif

//----------------------------------------------------------------------------------
// Module Functions
//----------------------------------------------------------------------------------

// Generate PCM samples (16-bit mono) for a procedural tone.
// PURE computation — no raylib/audio calls, unit-testable without a window.
// Caller owns the returned buffer (RL_FREE it); *outSampleCount gets the length.
// `seed` drives the NOISE waveform via a self-contained LCG (reproducible per
// seed, no global rand() state); ignored for tonal waveforms.
short *GenerateWaveData(int sampleRate, float frequency, float duration, float volume, int waveform, unsigned int seed, int *outSampleCount)
{
    if (sampleRate <= 0 || duration <= 0.0f)
    {
        if (outSampleCount) *outSampleCount = 0;
        return NULL;
    }

    // Clamp volume
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    int sampleCount = (int)(sampleRate * duration);
    // Hard cap (candidate 6): a huge AI-requested duration must not allocate
    // unbounded PCM — clamp, don't reject (tolerant truncation).
    if (sampleCount > AI_MAX_AUDIO_SAMPLES) sampleCount = AI_MAX_AUDIO_SAMPLES;
    if (sampleCount <= 0)
    {
        if (outSampleCount) *outSampleCount = 0;
        return NULL;
    }

    short *pcm = (short *)RL_CALLOC(sampleCount, sizeof(short));
    if (pcm == NULL)
    {
        if (outSampleCount) *outSampleCount = 0;
        return NULL;
    }

    // Fade duration: 10ms or 5% of total, whichever is smaller
    int fadeSamples = (int)(sampleRate * 0.01f);
    if (fadeSamples > sampleCount / 20) fadeSamples = sampleCount / 20;
    if (fadeSamples < 1) fadeSamples = 1;

    for (int i = 0; i < sampleCount; i++)
    {
        float t = (float)i / (float)sampleRate;
        float phase = 2.0f * PI * frequency * t;
        float sample = 0.0f;

        switch (waveform)
        {
            case AI_WAVEFORM_SINE:
                sample = sinf(phase);
                break;
            case AI_WAVEFORM_SQUARE:
                sample = (sinf(phase) >= 0.0f) ? 1.0f : -1.0f;
                break;
            case AI_WAVEFORM_TRIANGLE:
                sample = 2.0f * fabsf(2.0f * (frequency * t - floorf(frequency * t + 0.5f))) - 1.0f;
                break;
            case AI_WAVEFORM_SAWTOOTH:
                sample = 2.0f * (frequency * t - floorf(frequency * t + 0.5f));
                break;
            case AI_WAVEFORM_NOISE:
                // Self-contained LCG (numerical recipes): reproducible per seed,
                // no global rand() state (thread-safe, testable).
                seed = seed * 1103515245u + 12345u;
                sample = ((float)((seed >> 16) & 0x7FFF) / 32767.0f) * 2.0f - 1.0f;
                break;
            default:
                sample = sinf(phase);
                break;
        }

        // Apply fade in/out to avoid clicks
        if (i < fadeSamples)
        {
            sample *= (float)i / (float)fadeSamples;
        }
        else if (i > sampleCount - fadeSamples)
        {
            sample *= (float)(sampleCount - i) / (float)fadeSamples;
        }

        pcm[i] = (short)(sample * volume * 32767.0f);
    }

    if (outSampleCount) *outSampleCount = sampleCount;
    return pcm;
}

// Find a free audio slot (pure allocation state machine, no raylib calls).
// Returns slot index, or -1 when all slots are active.
int AIAllocSlot(AIAdapter *adapter)
{
    if (adapter == NULL) return -1;

    for (int i = 0; i < MAX_AI_AUDIO_SLOTS; i++)
    {
        if (!adapter->audioSlots[i].active) return i;
    }
    return -1;  // No free slots
}

// Play a procedural tone with given waveform
// `seed` drives NOISE reproducibility (ignored for tonal waveforms).
int AIPlayTone(AIAdapter *adapter, float frequency, float duration, float volume, int waveform, unsigned int seed)
{
    if (adapter == NULL || duration <= 0.0f || volume < 0.0f) return 0;

    // Find a free slot (pure state machine)
    int slot = AIAllocSlot(adapter);
    if (slot < 0) return 0;  // No free slots

    // Pure PCM synthesis (sample rate = engine default, candidate 6 constant)
    const int sampleRate = AI_AUDIO_SAMPLE_RATE;
    int sampleCount = 0;
    short *pcm = GenerateWaveData(sampleRate, frequency, duration, volume, waveform, seed, &sampleCount);
    if (pcm == NULL) return 0;

    // Thin raylib glue: load a static Sound (plays once — avoids the streaming
    // refill stall diagnosed earlier) and register the slot.
    Wave wave = { 0 };
    wave.frameCount = (unsigned int)sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm;

    Sound sound = LoadSoundFromWave(wave);
    if (sound.frameCount == 0)
    {
        RL_FREE(pcm);
        return 0;
    }

    SetSoundVolume(sound, volume);
    PlaySound(sound);

    // LoadSoundFromWave copies the PCM data, safe to free our buffer
    RL_FREE(pcm);

    // Record in slot
    adapter->audioSlots[slot].sound = sound;
    adapter->audioSlots[slot].active = true;
    adapter->audioSlots[slot].duration = duration;
    adapter->audioSlots[slot].volume = volume;
    adapter->audioSlots[slot].pitch = 1.0f;
    adapter->audioSlots[slot].waveform = waveform;

    return slot + 1;  // Handle = slot index + 1 (0 = invalid)
}

// Play white noise burst (reproducible per `seed`)
int AIPlayNoise(AIAdapter *adapter, float duration, float volume, unsigned int seed)
{
    return AIPlayTone(adapter, 0.0f, duration, volume, AI_WAVEFORM_NOISE, seed);
}

// Stop a procedural audio source by handle
void AIStopAudio(AIAdapter *adapter, int handle)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return;

    StopSound(adapter->audioSlots[slot].sound);
    UnloadSound(adapter->audioSlots[slot].sound);
    adapter->audioSlots[slot].active = false;
}

// Pause a procedural audio source by handle
void AIPauseAudio(AIAdapter *adapter, int handle)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return;

    PauseSound(adapter->audioSlots[slot].sound);
}

// Resume a paused procedural audio source by handle
void AIResumeAudio(AIAdapter *adapter, int handle)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return;

    ResumeSound(adapter->audioSlots[slot].sound);
}

// Set volume of a procedural audio source by handle
void AISetAudioVolume(AIAdapter *adapter, int handle, float volume)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    SetSoundVolume(adapter->audioSlots[slot].sound, volume);
    adapter->audioSlots[slot].volume = volume;
}

// Set pitch of a procedural audio source by handle
void AISetAudioPitch(AIAdapter *adapter, int handle, float pitch)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return;

    SetSoundPitch(adapter->audioSlots[slot].sound, pitch);
    adapter->audioSlots[slot].pitch = pitch;
}

// Check if a procedural audio source is still playing
bool IsAIAudioPlaying(AIAdapter *adapter, int handle)
{
    if (adapter == NULL || handle <= 0 || handle > MAX_AI_AUDIO_SLOTS) return false;
    int slot = handle - 1;
    if (!adapter->audioSlots[slot].active) return false;

    return IsSoundPlaying(adapter->audioSlots[slot].sound);
}

// Auto-recycle finished procedural audio slots (called from AIUpdate each frame)
void AIAudioUpdate(AIAdapter *adapter)
{
    if (adapter == NULL) return;

    for (int i = 0; i < MAX_AI_AUDIO_SLOTS; i++)
    {
        if (adapter->audioSlots[i].active && !IsSoundPlaying(adapter->audioSlots[i].sound))
        {
            UnloadSound(adapter->audioSlots[i].sound);
            adapter->audioSlots[i].active = false;
        }
    }
}

// Stop and unload all procedural audio slots (called from AIClose)
void AIAudioShutdown(AIAdapter *adapter)
{
    if (adapter == NULL) return;

    for (int i = 0; i < MAX_AI_AUDIO_SLOTS; i++)
    {
        if (adapter->audioSlots[i].active)
        {
            StopSound(adapter->audioSlots[i].sound);
            UnloadSound(adapter->audioSlots[i].sound);
            adapter->audioSlots[i].active = false;
        }
    }
}
