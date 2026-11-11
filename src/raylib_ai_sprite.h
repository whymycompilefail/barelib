/**********************************************************************************************
*
*   raylib_ai_sprite - Pixel-art sprite sheets: loading, animation playback, AI observation
*
*   Reads the sheet JSON exported by Pixel Loom (native format) and by Aseprite
*   (JSON hash / JSON array), loads the atlas PNG with TEXTURE_FILTER_POINT and draws
*   frames at integer scale so pixel art stays crisp.
*
*   Playback state is owned by the engine: AISpriteUpdate() advances the timeline and
*   AISpriteDraw() records an AI_DRAW_SPRITE scene element, so the AI can verify which
*   tag/frame is actually on screen (see raylib_ai_adapter.h).
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#ifndef RAYLIB_AI_SPRITE_H
#define RAYLIB_AI_SPRITE_H

#include "raylib.h"

#if defined(__cplusplus)
extern "C" {
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define AI_SPRITE_MAX_FRAMES    256     // Maximum frames per sheet
#define AI_SPRITE_MAX_TAGS      32      // Maximum animation tags per sheet
#define AI_SPRITE_TAG_NAME_LEN  32      // Animation tag name buffer (UTF-8 bytes)
#define AI_SPRITE_PATH_LEN      256     // Sheet/atlas path buffer

// Additive draw effects for AISpriteDrawEx(); bits may be combined.
#define AI_SPRITE_FX_NONE       0
#define AI_SPRITE_FX_FLASH      (1 << 0)   // additive solid white rect over the frame (alpha follows tint.a)
#define AI_SPRITE_FX_TINT_ADD   (1 << 1)   // additive solid tint instead of a multiplicative tint

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Playback direction of a tag, matching Pixel Loom / Aseprite.
typedef enum AISpriteDirection {
    AI_SPRITE_FORWARD = 0,      // start -> end
    AI_SPRITE_REVERSE,          // end -> start
    AI_SPRITE_PINGPONG          // start -> end -> start (endpoints not repeated)
} AISpriteDirection;

// Which point of the frame (x, y) refers to in AISpriteDrawEx().
typedef enum AISpriteAnchor {
    AI_SPRITE_TOPLEFT = 0,      // (x, y) is the top-left corner (legacy AISpriteDraw behaviour)
    AI_SPRITE_CENTER,           // (x, y) is the frame center
    AI_SPRITE_BOTTOM_CENTER     // (x, y) is the bottom-center ("feet"), handy for platformers
} AISpriteAnchor;

// One frame of the atlas: a source rectangle plus its hold duration.
typedef struct AISpriteFrame {
    Rectangle source;           // Source rectangle inside the atlas texture
    int durationMs;             // Hold duration in milliseconds (>= 1)
    int sourceIndex;            // Original timeline frame index (Pixel Loom source_index)
} AISpriteFrame;

// A named animation range.
typedef struct AISpriteTag {
    char name[AI_SPRITE_TAG_NAME_LEN];  // Tag name (UTF-8)
    int start;                          // First frame index in the sheet
    int end;                            // Last frame index in the sheet (inclusive)
    AISpriteDirection direction;        // Playback direction
    bool loop;                          // Loop when the end is reached
} AISpriteTag;

// Loaded sprite sheet plus its playback state.
typedef struct AISprite {
    Texture2D texture;                          // Atlas texture (id == 0 when parsed from memory only)
    AISpriteFrame frames[AI_SPRITE_MAX_FRAMES];
    int frameCount;
    AISpriteTag tags[AI_SPRITE_MAX_TAGS];
    int tagCount;
    int canvasWidth;                            // Single frame width as declared by the sheet
    int canvasHeight;                           // Single frame height as declared by the sheet
    char sheetPath[AI_SPRITE_PATH_LEN];         // Sheet JSON path (scene JSON "sheet")
    char imagePath[AI_SPRITE_PATH_LEN];         // Atlas PNG path (scene JSON "source")

    int order[AI_SPRITE_MAX_FRAMES * 2];        // Expanded playback order for the active tag
    int orderCount;
    int orderPos;                               // Current position inside order[]
    int activeTag;                              // Active tag index, -1 = whole sheet
    bool loop;                                  // Active sequence loops
    float elapsedMs;                            // Time spent on the current frame
    bool playing;
    bool finished;                              // Non-looping sequence reached its last frame
    bool ownsTexture;                           // True only for AILoadSpriteSheet(); clones share the atlas
} AISprite;

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Load a sheet JSON (Pixel Loom native or Aseprite hash/array) and its atlas PNG.
// The PNG path comes from the JSON and is resolved relative to the JSON directory.
// Returns NULL on failure (missing file, invalid JSON, no frames).
RLAPI AISprite *AILoadSpriteSheet(const char *jsonPath);

// Parse sheet JSON already in memory. No texture is loaded (texture.id stays 0),
// which keeps parsing and playback testable without a GPU.
// sheetPathHint is recorded for the scene JSON and used to resolve the PNG path.
RLAPI AISprite *AIParseSpriteSheet(const char *json, int jsonSize, const char *sheetPathHint);

// Unload the atlas texture (when loaded) and free the sprite.
RLAPI void AIUnloadSpriteSheet(AISprite *sprite);

// Start playing a tag from its first frame. NULL or "" plays the whole timeline.
// Returns false when the sprite is empty or the tag does not exist.
RLAPI bool AISpritePlay(AISprite *sprite, const char *tag);

// Pause playback, keeping the current frame.
RLAPI void AISpriteStop(AISprite *sprite);

// Advance playback by dt seconds, honouring per-frame durationMs.
RLAPI void AISpriteUpdate(AISprite *sprite, float dt);

// True while the active sequence is advancing.
RLAPI bool AISpriteIsPlaying(const AISprite *sprite);

// Original timeline index of the frame currently shown, or -1 when empty.
RLAPI int AISpriteCurrentFrame(const AISprite *sprite);

// Number of frames in the sheet.
RLAPI int AISpriteFrameCount(const AISprite *sprite);

// Jump to a position inside the expanded playback order (0 = first step).
RLAPI void AISpriteSeek(AISprite *sprite, int position);

// Clone an instance: shares the atlas texture but owns an INDEPENDENT playback
// clock (frames/tags/canvas/paths/order/elapsed are copied, then diverge).
// Use this for a squad of identical enemies that must not step in lockstep.
// The clone does not own the texture: AIUnloadSpriteSheet(clone) frees only the
// instance, leaving the original and every other clone drawable.
// Returns NULL on allocation failure.
RLAPI AISprite *AISpriteCloneState(const AISprite *src);

// Draw the current frame with an explicit anchor, float scale and additive fx.
// scale may be < 1 (clamped to 0.01); POINT filtering is kept, no mipmaps.
// AI_SPRITE_FX_FLASH adds a solid white rect over the frame (alpha from tint.a)
// — a white TINT cannot brighten dark texels because tint multiplies. TINT_ADD
// uses tint as a solid additive amount instead. Effects are rectangle-level
// (no alpha mask); only ONE AI_DRAW_SPRITE element is registered, and x/y are
// the anchor point.
RLAPI void AISpriteDrawEx(AISprite *sprite, float x, float y, float scale,
                          AISpriteAnchor anchor, bool flipX, Color tint, int fx);

// Legacy wrapper: integer scale (< 1 clamped to 1), top-left anchor, no fx.
// Records exactly one AI_DRAW_SPRITE element. Kept for source compatibility —
// use AISpriteDrawEx() for anchors, sub-1 scales and additive effects.
RLAPI void AISpriteDraw(AISprite *sprite, float x, float y, int scale, bool flipX, Color tint);

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_AI_SPRITE_H
