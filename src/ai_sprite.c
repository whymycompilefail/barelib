/**********************************************************************************************
*
*   ai_sprite - Pixel-art sprite sheets for the AI adapter
*
*   FEATURES:
*       - Reads Pixel Loom native sheet JSON (image/frames/tags/canvas)
*       - Reads Aseprite JSON hash and JSON array exports (frames/meta.frameTags)
*       - Loads the atlas with TEXTURE_FILTER_POINT and draws at integer scale
*       - Owns playback state (tag, frame, elapsed) so AIDrawXXX-style observation
*         always reports the frame that is actually on screen
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "ai_adapter_internal.h"    // AIAdapter, AIAddElement(), AIGetActiveAdapterInternal()
#include "raylib_ai_sprite.h"       // Public sprite types and API
#include "raylib.h"                 // Texture2D, LoadTexture(), DrawTexturePro(), TraceLog()
#include "rlgl.h"                   // RL_MALLOC / RL_CALLOC / RL_FREE

#include "external/jsmn.h"          // Tokenizer (MIT)

#include <stdlib.h>                 // Required for: strtol()
#include <string.h>                 // Required for: strlen(), strcmp(), memcpy(), memset()
#include <stdio.h>                  // Required for: fopen(), fread(), fseek()
#include <ctype.h>                  // Required for: isalpha()

//----------------------------------------------------------------------------------
// Local types
//----------------------------------------------------------------------------------
typedef struct AISpriteJSON {
    const char *text;       // JSON source
    jsmntok_t *tokens;      // Token array
    int count;              // Token count
} AISpriteJSON;

//----------------------------------------------------------------------------------
// JSON helpers (jsmn token navigation)
//----------------------------------------------------------------------------------
// jsmn stores tokens in document order and records byte ranges, so the token
// following a subtree is the first one starting at or after this token's end.
static int AISpriteSubtreeEnd(const AISpriteJSON *doc, int index)
{
    const jsmntok_t *token = &doc->tokens[index];
    int next = index + 1;
    while ((next < doc->count) && (doc->tokens[next].start < token->end)) next++;
    return next;
}

static bool AISpriteKeyIs(const AISpriteJSON *doc, const jsmntok_t *token, const char *key)
{
    int len = (int)strlen(key);
    if (token->type != JSMN_STRING) return false;
    if ((token->end - token->start) != len) return false;
    return strncmp(doc->text + token->start, key, (size_t)len) == 0;
}

// Returns the index of the value for `key` inside an object, or -1.
static int AISpriteObjectGet(const AISpriteJSON *doc, int objectIndex, const char *key)
{
    if ((objectIndex < 0) || (objectIndex >= doc->count)) return -1;

    const jsmntok_t *object = &doc->tokens[objectIndex];
    if (object->type != JSMN_OBJECT) return -1;

    int index = objectIndex + 1;
    for (int i = 0; (i < object->size) && (index + 1 < doc->count); i++)
    {
        int valueIndex = index + 1;
        if (AISpriteKeyIs(doc, &doc->tokens[index], key)) return valueIndex;
        index = AISpriteSubtreeEnd(doc, valueIndex);
    }

    return -1;
}

static int AISpriteInt(const AISpriteJSON *doc, int index, int fallback)
{
    if ((index < 0) || (index >= doc->count)) return fallback;

    const jsmntok_t *token = &doc->tokens[index];
    if ((token->type != JSMN_PRIMITIVE) || (token->start >= token->end)) return fallback;

    char buffer[32] = { 0 };
    int len = token->end - token->start;
    if (len >= (int)sizeof(buffer)) return fallback;
    memcpy(buffer, doc->text + token->start, (size_t)len);

    return (int)strtol(buffer, NULL, 10);
}

static int AISpriteHex4(const char *text)
{
    int value = 0;
    for (int i = 0; i < 4; i++)
    {
        char c = text[i];
        int digit = -1;
        if ((c >= '0') && (c <= '9')) digit = c - '0';
        else if ((c >= 'a') && (c <= 'f')) digit = c - 'a' + 10;
        else if ((c >= 'A') && (c <= 'F')) digit = c - 'A' + 10;
        if (digit < 0) return -1;
        value = value*16 + digit;
    }
    return value;
}

// Decode a JSON string body (escapes included) into out. Always NUL-terminates.
static void AISpriteDecodeString(const char *src, int len, char *out, int outSize)
{
    int written = 0;
    for (int i = 0; (i < len) && (written < outSize - 1); i++)
    {
        char c = src[i];
        if ((c != '\\') || (i + 1 >= len))
        {
            out[written++] = c;
            continue;
        }

        char escaped = src[++i];
        switch (escaped)
        {
            case 'n': out[written++] = '\n'; break;
            case 't': out[written++] = '\t'; break;
            case 'r': out[written++] = '\r'; break;
            case 'b': out[written++] = '\b'; break;
            case 'f': out[written++] = '\f'; break;
            case '/': out[written++] = '/'; break;
            case '\\': out[written++] = '\\'; break;
            case '"': out[written++] = '"'; break;
            case 'u':
            {
                if (i + 4 >= len) break;
                int codepoint = AISpriteHex4(src + i + 1);
                if (codepoint < 0) break;
                i += 4;

                // Combine a UTF-16 surrogate pair when present
                if ((codepoint >= 0xD800) && (codepoint <= 0xDBFF) &&
                    (i + 6 < len) && (src[i + 1] == '\\') && (src[i + 2] == 'u'))
                {
                    int low = AISpriteHex4(src + i + 3);
                    if ((low >= 0xDC00) && (low <= 0xDFFF))
                    {
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                        i += 6;
                    }
                }

                if (codepoint < 0x80) out[written++] = (char)codepoint;
                else if (codepoint < 0x800)
                {
                    if (written + 2 <= outSize - 1)
                    {
                        out[written++] = (char)(0xC0 | (codepoint >> 6));
                        out[written++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                }
                else if (codepoint < 0x10000)
                {
                    if (written + 3 <= outSize - 1)
                    {
                        out[written++] = (char)(0xE0 | (codepoint >> 12));
                        out[written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        out[written++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                }
                else
                {
                    if (written + 4 <= outSize - 1)
                    {
                        out[written++] = (char)(0xF0 | (codepoint >> 18));
                        out[written++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                        out[written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        out[written++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                }
            } break;
            default: out[written++] = escaped; break;
        }
    }

    out[written] = '\0';
}

static bool AISpriteString(const AISpriteJSON *doc, int index, char *out, int outSize)
{
    if ((index < 0) || (index >= doc->count) || (outSize <= 0)) return false;

    const jsmntok_t *token = &doc->tokens[index];
    if (token->type != JSMN_STRING) return false;

    AISpriteDecodeString(doc->text + token->start, token->end - token->start, out, outSize);
    return true;
}

static AISpriteDirection AISpriteDirectionFromString(const char *value)
{
    if (strcmp(value, "reverse") == 0) return AI_SPRITE_REVERSE;
    if (strcmp(value, "pingpong") == 0) return AI_SPRITE_PINGPONG;
    return AI_SPRITE_FORWARD;
}

//----------------------------------------------------------------------------------
// Path and file helpers
//----------------------------------------------------------------------------------
static char *AISpriteReadFile(const char *path, int *sizeOut)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(file);
        return NULL;
    }

    char *data = (char *)RL_MALLOC((size_t)size + 1);
    if (data == NULL)
    {
        fclose(file);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[read] = '\0';
    *sizeOut = (int)read;

    return data;
}

// Resolve `image` against the directory of `sheetPath` (Windows separators accepted).
static void AISpriteResolveImagePath(const char *sheetPath, const char *image, char *out, int outSize)
{
    if ((image == NULL) || (image[0] == '\0'))
    {
        out[0] = '\0';
        return;
    }

    bool absolute = (image[0] == '/') || (image[0] == '\\') ||
                    (isalpha((unsigned char)image[0]) && (image[1] == ':'));
    if (absolute || (sheetPath == NULL))
    {
        snprintf(out, (size_t)outSize, "%s", image);
        return;
    }

    const char *lastSlash = NULL;
    for (const char *p = sheetPath; *p != '\0'; p++)
    {
        if ((*p == '/') || (*p == '\\')) lastSlash = p;
    }

    if (lastSlash == NULL)
    {
        snprintf(out, (size_t)outSize, "%s", image);
        return;
    }

    int dirLen = (int)(lastSlash - sheetPath) + 1;
    if (dirLen >= outSize) dirLen = outSize - 1;
    memcpy(out, sheetPath, (size_t)dirLen);
    snprintf(out + dirLen, (size_t)(outSize - dirLen), "%s", image);
}

//----------------------------------------------------------------------------------
// Sheet parsing
//----------------------------------------------------------------------------------
static bool AISpriteParseFrame(const AISpriteJSON *doc, int objectIndex, AISpriteFrame *frame, int fallbackSourceIndex)
{
    int inner = AISpriteObjectGet(doc, objectIndex, "frame");
    bool aseprite = (inner >= 0) && (doc->tokens[inner].type == JSMN_OBJECT);

    if (aseprite)
    {
        frame->source.x = (float)AISpriteInt(doc, AISpriteObjectGet(doc, inner, "x"), 0);
        frame->source.y = (float)AISpriteInt(doc, AISpriteObjectGet(doc, inner, "y"), 0);
        frame->source.width = (float)AISpriteInt(doc, AISpriteObjectGet(doc, inner, "w"), 0);
        frame->source.height = (float)AISpriteInt(doc, AISpriteObjectGet(doc, inner, "h"), 0);
        frame->durationMs = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "duration"), 100);
        frame->sourceIndex = fallbackSourceIndex;
    }
    else
    {
        frame->source.x = (float)AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "x"), 0);
        frame->source.y = (float)AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "y"), 0);
        frame->source.width = (float)AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "width"), 0);
        frame->source.height = (float)AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "height"), 0);
        frame->durationMs = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "duration_ms"), 100);
        frame->sourceIndex = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "source_index"), fallbackSourceIndex);
    }

    if ((frame->source.width <= 0.0f) || (frame->source.height <= 0.0f)) return false;
    if (frame->durationMs < 1) frame->durationMs = 1;
    if (frame->durationMs > 60000) frame->durationMs = 60000;

    return true;
}

static void AISpriteParseTag(const AISpriteJSON *doc, int objectIndex, AISpriteTag *tag, bool aseprite)
{
    char name[AI_SPRITE_TAG_NAME_LEN] = { 0 };
    AISpriteString(doc, AISpriteObjectGet(doc, objectIndex, "name"), name, sizeof(name));
    snprintf(tag->name, sizeof(tag->name), "%s", name);

    if (aseprite)
    {
        tag->start = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "from"), 0);
        tag->end = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "to"), 0);
        // Pixel Loom writes Aseprite's repeat field only when the tag must not loop
        tag->loop = (AISpriteObjectGet(doc, objectIndex, "repeat") < 0);
    }
    else
    {
        tag->start = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "start"), 0);
        tag->end = AISpriteInt(doc, AISpriteObjectGet(doc, objectIndex, "end"), 0);
        tag->loop = true;
    }

    char direction[16] = "forward";
    AISpriteString(doc, AISpriteObjectGet(doc, objectIndex, "direction"), direction, sizeof(direction));
    tag->direction = AISpriteDirectionFromString(direction);
}

static void AISpriteBuildOrder(AISprite *sprite, int start, int end, AISpriteDirection direction)
{
    sprite->orderCount = 0;
    sprite->orderPos = 0;
    sprite->elapsedMs = 0.0f;

    if ((start < 0) || (end < start) || (start >= sprite->frameCount)) return;
    if (end >= sprite->frameCount) end = sprite->frameCount - 1;

    int count = end - start + 1;
    int maxOrder = AI_SPRITE_MAX_FRAMES*2;

    for (int i = 0; i < count && sprite->orderCount < maxOrder; i++)
    {
        sprite->order[sprite->orderCount++] = start + i;
    }

    if (direction == AI_SPRITE_REVERSE)
    {
        for (int i = 0; i < sprite->orderCount/2; i++)
        {
            int tmp = sprite->order[i];
            sprite->order[i] = sprite->order[sprite->orderCount - 1 - i];
            sprite->order[sprite->orderCount - 1 - i] = tmp;
        }
    }
    else if ((direction == AI_SPRITE_PINGPONG) && (count > 2))
    {
        // Pixel Loom expands pingpong as start..end then end-1..start+1
        for (int i = count - 2; (i >= 1) && (sprite->orderCount < maxOrder); i--)
        {
            sprite->order[sprite->orderCount++] = start + i;
        }
    }
}

AISprite *AIParseSpriteSheet(const char *json, int jsonSize, const char *sheetPathHint)
{
    if ((json == NULL) || (jsonSize <= 0)) return NULL;

    jsmn_parser parser = { 0 };
    jsmntok_t *tokens = NULL;
    int capacity = 512;
    int tokenCount = 0;

    for (;;)
    {
        tokens = (jsmntok_t *)RL_MALLOC((size_t)capacity*sizeof(jsmntok_t));
        if (tokens == NULL) return NULL;

        jsmn_init(&parser);
        tokenCount = jsmn_parse(&parser, json, (size_t)jsonSize, tokens, (unsigned int)capacity);
        if ((tokenCount != JSMN_ERROR_NOMEM) || (capacity >= 65536)) break;

        RL_FREE(tokens);
        tokens = NULL;
        capacity *= 2;
    }

    if ((tokenCount <= 0) || (tokens[0].type != JSMN_OBJECT))
    {
        RL_FREE(tokens);
        return NULL;
    }

    AISpriteJSON doc = { json, tokens, tokenCount };
    AISprite *sprite = (AISprite *)RL_CALLOC(1, sizeof(AISprite));
    if (sprite == NULL)
    {
        RL_FREE(tokens);
        return NULL;
    }
    sprite->activeTag = -1;
    sprite->loop = true;

    if (sheetPathHint != NULL) snprintf(sprite->sheetPath, sizeof(sprite->sheetPath), "%s", sheetPathHint);

    int framesIndex = AISpriteObjectGet(&doc, 0, "frames");
    int metaIndex = AISpriteObjectGet(&doc, 0, "meta");
    bool aseprite = (metaIndex >= 0) && (tokens[metaIndex].type == JSMN_OBJECT);

    // --- Frames ---
    if ((framesIndex >= 0) && (tokens[framesIndex].type == JSMN_ARRAY))
    {
        int arrayEnd = AISpriteSubtreeEnd(&doc, framesIndex);
        int index = framesIndex + 1;
        while ((index < arrayEnd) && (sprite->frameCount < AI_SPRITE_MAX_FRAMES))
        {
            if (tokens[index].type == JSMN_OBJECT)
            {
                AISpriteFrame frame = { 0 };
                if (AISpriteParseFrame(&doc, index, &frame, sprite->frameCount))
                {
                    sprite->frames[sprite->frameCount++] = frame;
                }
            }
            index = AISpriteSubtreeEnd(&doc, index);
        }
    }
    else if ((framesIndex >= 0) && (tokens[framesIndex].type == JSMN_OBJECT))
    {
        // Aseprite JSON hash: { "name.png": { ... }, ... }
        const jsmntok_t *object = &tokens[framesIndex];
        int index = framesIndex + 1;
        for (int i = 0; (i < object->size) && (sprite->frameCount < AI_SPRITE_MAX_FRAMES); i++)
        {
            if (index + 1 >= tokenCount) break;
            int valueIndex = index + 1;

            if (tokens[valueIndex].type == JSMN_OBJECT)
            {
                AISpriteFrame frame = { 0 };
                if (AISpriteParseFrame(&doc, valueIndex, &frame, sprite->frameCount))
                {
                    sprite->frames[sprite->frameCount++] = frame;
                }
            }
            index = AISpriteSubtreeEnd(&doc, valueIndex);
        }
    }

    if (sprite->frameCount == 0)
    {
        RL_FREE(tokens);
        RL_FREE(sprite);
        TraceLog(LOG_WARNING, "SPRITE: sheet has no usable frames");
        return NULL;
    }

    // --- Canvas size and atlas image name ---
    char image[AI_SPRITE_PATH_LEN] = { 0 };

    if (aseprite)
    {
        AISpriteString(&doc, AISpriteObjectGet(&doc, metaIndex, "image"), image, sizeof(image));

        int sizeIndex = AISpriteObjectGet(&doc, metaIndex, "size");
        sprite->canvasWidth = AISpriteInt(&doc, AISpriteObjectGet(&doc, sizeIndex, "w"), 0);
        sprite->canvasHeight = AISpriteInt(&doc, AISpriteObjectGet(&doc, sizeIndex, "h"), 0);
    }
    else
    {
        AISpriteString(&doc, AISpriteObjectGet(&doc, 0, "image"), image, sizeof(image));

        int canvasIndex = AISpriteObjectGet(&doc, 0, "canvas");
        sprite->canvasWidth = AISpriteInt(&doc, AISpriteObjectGet(&doc, canvasIndex, "width"), 0);
        sprite->canvasHeight = AISpriteInt(&doc, AISpriteObjectGet(&doc, canvasIndex, "height"), 0);
    }

    if ((sprite->canvasWidth <= 0) || (sprite->canvasHeight <= 0))
    {
        sprite->canvasWidth = (int)sprite->frames[0].source.width;
        sprite->canvasHeight = (int)sprite->frames[0].source.height;
    }

    AISpriteResolveImagePath(sheetPathHint, image, sprite->imagePath, sizeof(sprite->imagePath));

    // --- Tags ---
    int tagsIndex = aseprite ? AISpriteObjectGet(&doc, metaIndex, "frameTags")
                             : AISpriteObjectGet(&doc, 0, "tags");

    if ((tagsIndex >= 0) && (tokens[tagsIndex].type == JSMN_ARRAY))
    {
        int arrayEnd = AISpriteSubtreeEnd(&doc, tagsIndex);
        int index = tagsIndex + 1;
        while ((index < arrayEnd) && (sprite->tagCount < AI_SPRITE_MAX_TAGS))
        {
            if (tokens[index].type == JSMN_OBJECT)
            {
                AISpriteTag tag = { 0 };
                AISpriteParseTag(&doc, index, &tag, aseprite);

                if (tag.start < 0) tag.start = 0;
                if (tag.end >= sprite->frameCount) tag.end = sprite->frameCount - 1;
                if ((tag.name[0] != '\0') && (tag.end >= tag.start))
                {
                    sprite->tags[sprite->tagCount++] = tag;
                }
            }
            index = AISpriteSubtreeEnd(&doc, index);
        }
    }

    AISpriteBuildOrder(sprite, 0, sprite->frameCount - 1, AI_SPRITE_FORWARD);
    sprite->playing = false;

    RL_FREE(tokens);
    return sprite;
}

AISprite *AILoadSpriteSheet(const char *jsonPath)
{
    if (jsonPath == NULL) return NULL;

    int jsonSize = 0;
    char *json = AISpriteReadFile(jsonPath, &jsonSize);
    if (json == NULL)
    {
        TraceLog(LOG_WARNING, "SPRITE: could not read sheet JSON: %s", jsonPath);
        return NULL;
    }

    AISprite *sprite = AIParseSpriteSheet(json, jsonSize, jsonPath);
    RL_FREE(json);
    if (sprite == NULL) return NULL;

    if (sprite->imagePath[0] == '\0')
    {
        TraceLog(LOG_WARNING, "SPRITE: sheet has no atlas image: %s", jsonPath);
        AIUnloadSpriteSheet(sprite);
        return NULL;
    }

    Texture2D texture = LoadTexture(sprite->imagePath);
    if (texture.id == 0)
    {
        TraceLog(LOG_WARNING, "SPRITE: could not load atlas: %s", sprite->imagePath);
        AIUnloadSpriteSheet(sprite);
        return NULL;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    sprite->texture = texture;
    sprite->ownsTexture = true;      // only the loader owns the GPU texture

    return sprite;
}

void AIUnloadSpriteSheet(AISprite *sprite)
{
    if (sprite == NULL) return;

    // Clones and memory-only parses do not own the atlas: unloading one must
    // not invalidate the original or any sibling instance.
    if (sprite->ownsTexture && (sprite->texture.id != 0)) UnloadTexture(sprite->texture);
    RL_FREE(sprite);
}

AISprite *AISpriteCloneState(const AISprite *src)
{
    if (src == NULL) return NULL;

    // Full value copy: frames/tags/canvas/paths/order plus the current playback
    // position are per-instance state; the texture handle is shared. NOTE: this
    // is sizeof(AISprite) bytes per clone, so clones are for on-screen actors,
    // not for thousands of particles.
    AISprite *copy = (AISprite *)RL_MALLOC(sizeof(AISprite));
    if (copy == NULL) return NULL;

    *copy = *src;
    copy->ownsTexture = false;
    return copy;
}

//----------------------------------------------------------------------------------
// Playback
//----------------------------------------------------------------------------------
bool AISpritePlay(AISprite *sprite, const char *tag)
{
    if ((sprite == NULL) || (sprite->frameCount == 0)) return false;

    if ((tag == NULL) || (tag[0] == '\0'))
    {
        sprite->activeTag = -1;
        sprite->loop = true;
        AISpriteBuildOrder(sprite, 0, sprite->frameCount - 1, AI_SPRITE_FORWARD);
    }
    else
    {
        int found = -1;
        for (int i = 0; i < sprite->tagCount; i++)
        {
            if (strcmp(sprite->tags[i].name, tag) == 0)
            {
                found = i;
                break;
            }
        }
        if (found < 0) return false;

        sprite->activeTag = found;
        sprite->loop = sprite->tags[found].loop;
        AISpriteBuildOrder(sprite, sprite->tags[found].start, sprite->tags[found].end, sprite->tags[found].direction);
    }

    sprite->playing = true;
    sprite->finished = false;
    return (sprite->orderCount > 0);
}

void AISpriteStop(AISprite *sprite)
{
    if (sprite == NULL) return;
    sprite->playing = false;
}

void AISpriteUpdate(AISprite *sprite, float dt)
{
    if ((sprite == NULL) || !sprite->playing || (sprite->orderCount == 0)) return;

    float deltaMs = dt*1000.0f;
    if (deltaMs <= 0.0f) return;

    sprite->elapsedMs += deltaMs;

    // Cap catch-up steps so a huge dt cannot spin here
    for (int step = 0; step < 64; step++)
    {
        int frameIndex = sprite->order[sprite->orderPos];
        int duration = sprite->frames[frameIndex].durationMs;
        if (duration < 1) duration = 1;
        if (sprite->elapsedMs < (float)duration) break;

        sprite->elapsedMs -= (float)duration;

        if (sprite->orderPos + 1 < sprite->orderCount)
        {
            sprite->orderPos++;
        }
        else if (sprite->loop)
        {
            sprite->orderPos = 0;
        }
        else
        {
            sprite->elapsedMs = 0.0f;
            sprite->playing = false;
            sprite->finished = true;
            break;
        }
    }
}

bool AISpriteIsPlaying(const AISprite *sprite)
{
    return (sprite != NULL) && sprite->playing;
}

int AISpriteCurrentFrame(const AISprite *sprite)
{
    if ((sprite == NULL) || (sprite->frameCount == 0) || (sprite->orderCount == 0)) return -1;
    return sprite->frames[sprite->order[sprite->orderPos]].sourceIndex;
}

int AISpriteFrameCount(const AISprite *sprite)
{
    return (sprite == NULL) ? 0 : sprite->frameCount;
}

void AISpriteSeek(AISprite *sprite, int position)
{
    if ((sprite == NULL) || (sprite->orderCount == 0)) return;

    if (position < 0) position = 0;
    if (position >= sprite->orderCount) position = sprite->orderCount - 1;

    sprite->orderPos = position;
    sprite->elapsedMs = 0.0f;
    sprite->finished = false;
}

//----------------------------------------------------------------------------------
// Drawing + AI observation
//----------------------------------------------------------------------------------
void AISpriteDrawEx(AISprite *sprite, float x, float y, float scale,
                    AISpriteAnchor anchor, bool flipX, Color tint, int fx)
{
    if ((sprite == NULL) || (sprite->frameCount == 0)) return;
    if (sprite->orderCount == 0) AISpriteBuildOrder(sprite, 0, sprite->frameCount - 1, AI_SPRITE_FORWARD);
    if (scale < 0.01f) scale = 0.01f;   // sub-1 scales are allowed; guard against 0/negative dest

    const AISpriteFrame *frame = &sprite->frames[sprite->order[sprite->orderPos]];

    Rectangle source = frame->source;
    if (flipX)
    {
        source.x += source.width;
        source.width = -source.width;
    }

    float destWidth = frame->source.width*scale;
    float destHeight = frame->source.height*scale;

    // Convert the requested anchor point to the top-left corner of the dest rect
    float drawX = x;
    float drawY = y;
    if ((anchor == AI_SPRITE_CENTER) || (anchor == AI_SPRITE_BOTTOM_CENTER)) drawX -= destWidth*0.5f;
    if (anchor == AI_SPRITE_CENTER) drawY -= destHeight*0.5f;
    else if (anchor == AI_SPRITE_BOTTOM_CENTER) drawY -= destHeight;

    bool tintAdd = ((fx & AI_SPRITE_FX_TINT_ADD) != 0);
    bool additive = ((fx & (AI_SPRITE_FX_FLASH | AI_SPRITE_FX_TINT_ADD)) != 0);

    if (sprite->texture.id != 0)
    {
        Rectangle dest = { drawX, drawY, destWidth, destHeight };

        // TINT_ADD keeps the atlas unmodulated so tint acts purely as an additive amount
        Color baseTint = tintAdd ? WHITE : tint;
        DrawTexturePro(sprite->texture, source, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, baseTint);

        if (additive)
        {
            // Re-drawing the atlas additively does NOT flash a dark sprite:
            // DrawTexturePro multiplies the tint into the (dark) texels, so an
            // additive second pass merely doubles that darkness. The review game
            // worked around it with an additive SOLID pass over the frame rect
            // (game/src/render.c); this fx internalises exactly that patch, but
            // still registers only one scene element.
            // NOTE: this is a rectangle-level flash. An alpha-shaped flash would
            // require a white mask atlas, which is out of scope for this fix.
            Color addColor = tintAdd ? tint : (Color){ 255, 255, 255, tint.a };
            // Stretch raylib's 1x1 white texel over the dest rect. Unlike
            // DrawRectangleRec(), DrawTexturePro() has no observability hook,
            // so this pass cannot leak a stray rect element when the host has
            // enabled automatic scene capture.
            Texture2D white = GetShapesTexture();
            Rectangle whiteSrc = GetShapesTextureRectangle();
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(white, whiteSrc, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, addColor);
            EndBlendMode();
        }
    }

    AIAdapter *adapter = AIGetActiveAdapterInternal();
    if (adapter == NULL) return;

    AIDrawElement element = { 0 };
    element.type = AI_DRAW_SPRITE;
    element.x = x;                  // the anchor point, interpreted via element.anchor
    element.y = y;
    element.w = destWidth;
    element.h = destHeight;
    element.color = tint;
    element.scale = scale;
    element.flipX = flipX;
    element.anchor = (int)anchor;
    element.fx = fx;
    element.frame = frame->sourceIndex;
    element.frameCount = sprite->frameCount;
    element.loop = sprite->loop ? 1 : 0;
    snprintf(element.source, sizeof(element.source), "%s", sprite->imagePath);
    snprintf(element.sheet, sizeof(element.sheet), "%s", sprite->sheetPath);
    if ((sprite->activeTag >= 0) && (sprite->activeTag < sprite->tagCount))
    {
        snprintf(element.tag, sizeof(element.tag), "%s", sprite->tags[sprite->activeTag].name);
    }

    AIAddElement(adapter, &element);
}

void AISpriteDraw(AISprite *sprite, float x, float y, int scale, bool flipX, Color tint)
{
    // Source compatibility: the legacy integer-scale clamp is kept here, not
    // inside DrawEx (which deliberately supports scales below 1).
    if (scale < 1) scale = 1;
    AISpriteDrawEx(sprite, x, y, (float)scale, AI_SPRITE_TOPLEFT, flipX, tint, AI_SPRITE_FX_NONE);
}
