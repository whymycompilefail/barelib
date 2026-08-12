/**********************************************************************************************
*
*   raylib_context.h - Explicit global state context for raylib
*
*   This header defines a context struct that groups all of raylib's global state into a
*   single explicitly-managed object. Currently raylib uses module-level static/global
*   variables (CoreData CORE, rlgl internal state, audio state, etc.). This context
*   struct creates a seam toward future multi-instance support.
*
*   USAGE:
*       RaylibContext *ctx = GetRaylibContext();
*       ctx->core.Window.ready = true;
*
*   For backward compatibility, the CORE macro expands to GetRaylibContext()->core,
*   so existing code using CORE.Window... continues to work.
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
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef RAYLIB_CONTEXT_H
#define RAYLIB_CONTEXT_H

#include "raylib.h"
#include "config.h"
#include "platform.h"
#include "raylib_observability.h"   // Required for: AIDrawObserver owner tracking (single-adapter premise)

// Forward declarations for opaque state pointers (full type definitions are in the .c files)
// Defined in rlgl.c - only pointer types are used here.
typedef struct rlglData rlglData;
// Defined in raudio_internal.h - only pointer types are used here.
typedef struct AudioData AudioData;

// Forward declaration for AIAdapter (used in optional AI adapter state)
// Defined in raylib_ai_adapter.h - only pointer types are used here.
typedef struct AIAdapter AIAdapter;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Maximum number of concurrent AI adapters
#ifndef MAX_AI_ADAPTERS
    #define MAX_AI_ADAPTERS 4
#endif

// Raylib global state context
// Groups all module-level state into a single context object.
// This is the first step toward making raylib's state explicit and testable.
typedef struct RaylibContext {
    CoreData core;              // Platform/windowing/input state (from platform.h)

    // rlgl internal state, allocated in rlglInit()
    rlglData *rlglState;

    // Audio internal state, allocated in InitAudioDevice()
    AudioData *audioState;

    // Optional AI adapter state (only used when SUPPORT_MODULE_AI_ADAPTER is enabled)
    #if SUPPORT_MODULE_AI_ADAPTER
    AIAdapter *aiAdapters[MAX_AI_ADAPTERS]; // Registered AI adapter instances (isolation tests)
    int aiAdapterCount;                     // Number of registered adapters
    // Single-adapter premise (candidate 2): only the FIRST registered adapter
    // serves the AIDrawXXX wrapper calls. Explicit pointer, no [0] hardcode —
    // the registration list above stays for multi-instance memory isolation.
    AIAdapter *aiActiveAdapter;             // Adapter served by AIDrawXXX wrappers (NULL = none)

    // Draw observer ownership (spec M3, single-adapter premise)
    // The engine notifies exactly one observer; the adapter that owns it is
    // tracked so a second adapter cannot silently steal/disable it (AI_ERR_BUSY).
    AIDrawObserver aiDrawObserver;          // Registered observer (NULL = none)
    void *aiDrawObserverOwner;              // Owner pointer (NULL = no owner, e.g. game code)
    TraceLogCallback aiSavedLogCallback;    // Saved log callback (for restoration)
    #endif
} RaylibContext;

//----------------------------------------------------------------------------------
// Context Accessor Functions
//----------------------------------------------------------------------------------

// Get the current raylib context (singleton for now)
// Returns a pointer to the global context instance.
RaylibContext *GetRaylibContext(void);

// Set the current raylib context (pointer-based, no shallow copy)
// The caller is responsible for allocating and freeing the context.
// Pass NULL to reset to the default internal context.
void SetRaylibContext(RaylibContext *context);

//----------------------------------------------------------------------------------
// Backward Compatibility Macro
//----------------------------------------------------------------------------------
// Existing code uses CORE.Window.xxx directly. This macro keeps that working
// while the state now lives inside a RaylibContext.
#ifndef CORE
    #define CORE (GetRaylibContext()->core)
#endif

#endif // RAYLIB_CONTEXT_H