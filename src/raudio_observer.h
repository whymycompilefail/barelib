/**********************************************************************************************
*
*   raudio_observer.h - Miniaudio-free observation interface for the audio backend
*
*   This header allows external modules (e.g., AI adapter) to query audio buffer
*   state without pulling in raudio_internal.h, which depends on miniaudio types.
*
*   AudioData and rAudioBuffer are used only as opaque pointers here; their full
*   definitions live in raudio_internal.h and raudio.c respectively.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty.
*
**********************************************************************************************/

#ifndef RAUDIO_OBSERVER_H
#define RAUDIO_OBSERVER_H

#include "raylib.h"  // for bool

//----------------------------------------------------------------------------------
// Forward declarations (opaque pointers — full definitions in raudio_internal.h / raudio.c)
//----------------------------------------------------------------------------------
struct rAudioBuffer;
struct AudioData;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Snapshot of an audio buffer's observable state.
// Exposes only what observers need, without revealing rAudioBuffer internals.
typedef struct AudioBufferInfo {
    int usage;          // 0=STATIC (Sound), 1=STREAM (Music/AudioStream)
    bool playing;
    bool paused;
    bool looping;
    float volume;
    float pitch;
    float pan;
    float progress;     // frameCursorPos / sizeInFrames (0.0-1.0)
} AudioBufferInfo;

//----------------------------------------------------------------------------------
// Audio Buffer Iterator Functions
//----------------------------------------------------------------------------------

// Get first audio buffer in the active list (returns NULL if list is empty)
struct rAudioBuffer *GetFirstAudioBuffer(struct AudioData *audio);

// Get next audio buffer in the list (returns NULL at end)
struct rAudioBuffer *GetNextAudioBuffer(struct rAudioBuffer *buf);

// Extract observable info from an audio buffer. Returns false if buf is NULL.
bool GetAudioBufferInfo(struct rAudioBuffer *buf, AudioBufferInfo *info);

#endif // RAUDIO_OBSERVER_H
