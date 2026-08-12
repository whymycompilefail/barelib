/**********************************************************************************************
*
*   raudio_internal.h - Internal audio state and codec abstraction for raylib
*
*   This header defines:
*     1. The AudioData context struct (audio global state, allocated in RaylibContext)
*     2. The abstract codec interface between raudio.c and external audio libraries
*        (miniaudio, dr_wav, dr_mp3, etc.)
*
*   The AudioData struct is an internal implementation detail. External code should
*   only interact with it through the public raylib API (InitAudioDevice, LoadSound, etc.).
*
*   The codec interface creates a seam for swapping dependencies (e.g. replace
*   miniaudio with a different audio backend).
*
*   USAGE:
*       #include "raudio_internal.h"   // after including miniaudio.h
*       AudioData *audio = GetRaylibContext()->audioState;
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as follows, and must not be
*     misrepresented as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef RAUDIO_INTERNAL_H
#define RAUDIO_INTERNAL_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Audio data context
// NOTE: This struct is the audio subsystem's global state. It is allocated in
// InitAudioDevice() and stored in RaylibContext->audioState as an opaque pointer.
// The full definition is exposed here for internal use only.
typedef struct AudioData {
    struct {
        ma_context context;         // miniaudio context data
        ma_device device;           // miniaudio device
        ma_mutex lock;              // miniaudio mutex lock
        bool isReady;               // Check if audio device is ready
        size_t pcmBufferSize;       // Pre-allocated buffer size
        void *pcmBuffer;            // Pre-allocated buffer to read audio data from file/memory
    } System;
    struct {
        struct rAudioBuffer *first; // Pointer to first AudioBuffer in the list
        struct rAudioBuffer *last;  // Pointer to last AudioBuffer in the list
        int defaultSize;            // Default audio buffer size for audio streams
    } Buffer;
    struct rAudioProcessor *mixedProcessor;
} AudioData;

//----------------------------------------------------------------------------------
// Audio Codec Interface
//----------------------------------------------------------------------------------
// These functions wrap external audio loading/decoding libraries.
// To replace a codec backend, provide alternative implementations of these functions.

// Audio device lifecycle (wraps miniaudio)
bool InitAudioDevice_Internal(void);
void CloseAudioDevice_Internal(void);
bool IsAudioDeviceReady_Internal(void);

// Audio stream management
// rAudioBuffer is defined in raylib_core.h
void PlayAudioStream_Internal(struct rAudioBuffer *buffer);
void StopAudioStream_Internal(struct rAudioBuffer *buffer);
void PauseAudioStream_Internal(struct rAudioBuffer *buffer);
void ResumeAudioStream_Internal(struct rAudioBuffer *buffer);
void UpdateAudioStream_Internal(struct rAudioBuffer *buffer, const void *data, int samplesCount);

// Load audio data from memory (wraps dr_wav / dr_mp3 / dr_flac / stb_vorbis)
// Returns decoded PCM data and format info.
float *LoadAudioDataFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int *channels, int *sampleRate, unsigned int *totalFrameCount);

// Audio buffer observer interface (AudioBufferInfo + iterator functions)
// is declared in raudio_observer.h — a miniaudio-free header that external
// modules (e.g., AI adapter) can include without pulling in miniaudio types.

#endif // RAUDIO_INTERNAL_H