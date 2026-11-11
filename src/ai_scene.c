/**********************************************************************************************
*
*   ai_scene.c - AI adapter scene subsystem (M4 split from raylib_ai_adapter.c)
*
*   Owns: scene element buffer + texture registry, the 16 M4-slim AIDrawXXX
*   wrappers, the AI_ELEMENT_TYPES X-macro list (element type -> JSON name),
*   scene JSON serialization, and snapshot/diff comparison.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "ai_adapter_internal.h"
#include "raylib.h"          // Required for: draw functions, Font, Texture2D
#include "raylib_ai_sprite.h" // Required for: AISpriteAnchor / AI_SPRITE_FX_* names
#include "raylib_context.h"  // Required for: RaylibContext, GetRaylibContext()
#include "config.h"

#include <string.h>          // Required for: strncpy(), strlen()
#include <math.h>            // Required for: sinf(), cosf(), floorf(), fabsf()

//----------------------------------------------------------------------------------
// Element type knowledge — single source of truth (X-macro list):
//   X(type, json_type_name, field_flags)
// Drives the JSON "type" name and the field set compared by snapshot diff
// (AIElementChanged). Scene serialization stays explicit per type because each
// type has a different key layout, but its fields must match these flags.
//----------------------------------------------------------------------------------
#define AI_HAS_SIZE      (1 << 0)   // w/h fields are meaningful
#define AI_HAS_RADIUS    (1 << 1)   // radius field
#define AI_HAS_SIDES     (1 << 2)   // sides field
#define AI_HAS_ROTATION  (1 << 3)   // rotation field
#define AI_HAS_POINTS    (1 << 4)   // x2,y2 (and x3,y3) fields
#define AI_HAS_SECTOR    (1 << 5)   // startAngle/endAngle (stored in rotation/sides)
#define AI_HAS_TEXT      (1 << 6)   // text + fontSize fields
#define AI_HAS_SOURCE    (1 << 7)   // source field (texture path)
#define AI_HAS_SPRITE    (1 << 8)   // sprite animation fields (tag/frame/frameCount/loop/scale/flipX)

#define AI_ELEMENT_TYPES(X) \
    X(AI_DRAW_CLEAR,         "clear",         0) \
    X(AI_DRAW_RECT,          "rect",          AI_HAS_SIZE|AI_HAS_ROTATION) \
    X(AI_DRAW_RECT_LINES,    "rect_lines",    AI_HAS_SIZE) \
    X(AI_DRAW_RECT_ROUNDED,  "rect_rounded",  AI_HAS_SIZE|AI_HAS_RADIUS) \
    X(AI_DRAW_CIRCLE,        "circle",        AI_HAS_RADIUS) \
    X(AI_DRAW_CIRCLE_LINES,  "circle_lines",  AI_HAS_RADIUS) \
    X(AI_DRAW_CIRCLE_SECTOR, "circle_sector", AI_HAS_RADIUS|AI_HAS_SECTOR) \
    X(AI_DRAW_TRIANGLE,      "triangle",      AI_HAS_POINTS) \
    X(AI_DRAW_TRIANGLE_LINES,"triangle_lines",AI_HAS_POINTS) \
    X(AI_DRAW_POLY,          "poly",          AI_HAS_RADIUS|AI_HAS_SIDES|AI_HAS_ROTATION) \
    X(AI_DRAW_POLY_LINES,    "poly_lines",    AI_HAS_RADIUS|AI_HAS_SIDES|AI_HAS_ROTATION) \
    X(AI_DRAW_LINE,          "line",          AI_HAS_POINTS) \
    X(AI_DRAW_LINE_BEZIER,   "line_bezier",   AI_HAS_POINTS) \
    X(AI_DRAW_TEXT,          "text",          AI_HAS_TEXT) \
    X(AI_DRAW_TEXTURE,       "texture",       AI_HAS_SIZE|AI_HAS_SOURCE|AI_HAS_ROTATION) \
    X(AI_DRAW_PIXEL,         "pixel",         0) \
    X(AI_DRAW_SPRITE,        "sprite",        AI_HAS_SIZE|AI_HAS_SOURCE|AI_HAS_SPRITE)

// JSON "type" name lookup (generated from the single list above)
#define AI_GEN_NAME(type, name, flags) name,
static const char *aiElementTypeNames[] = { AI_ELEMENT_TYPES(AI_GEN_NAME) };

// Per-type field flags (generated from the same list; used by snapshot diff)
#define AI_GEN_FLAGS(type, name, flags) flags,
static const int aiElementTypeFlags[] = { AI_ELEMENT_TYPES(AI_GEN_FLAGS) };

static int AIElementFieldFlags(AIDrawElementType type)
{
    int index = (int)type;
    int count = (int)(sizeof(aiElementTypeFlags)/sizeof(aiElementTypeFlags[0]));
    if ((index < 0) || (index >= count)) return 0;
    return aiElementTypeFlags[index];
}

static AIAdapter *AIGetActiveAdapter(void)
{
    RaylibContext *ctx = GetRaylibContext();
    #if SUPPORT_MODULE_AI_ADAPTER
    // Single-adapter premise (candidate 2): AIDrawXXX wrappers serve the adapter
    // explicitly marked active (the first registered). Explicit pointer, no
    // aiAdapters[0] hardcode and no per-call warning — the "additional adapter"
    // warning fires once at registration time instead.
    return ctx->aiActiveAdapter;
    #else
    (void)ctx;
    return NULL;
    #endif
}

AIAdapter *AIGetActiveAdapterInternal(void)
{
    return AIGetActiveAdapter();
}

const char *AILookupTexturePath(AIAdapter *adapter, unsigned int textureId)
{
    if (adapter == NULL) return "unknown";
    for (int i = 0; i < adapter->textureRegistryCount; i++)
    {
        if (adapter->textureRegistry[i].id == textureId)
        {
            return adapter->textureRegistry[i].path;
        }
    }
    return "unknown";
}

void AIRegisterTexture(AIAdapter *adapter, unsigned int textureId, const char *path)
{
    if (adapter == NULL || path == NULL) return;

    for (int i = 0; i < adapter->textureRegistryCount; i++)
    {
        if (adapter->textureRegistry[i].id == textureId)
        {
            strncpy(adapter->textureRegistry[i].path, path, 255);
            adapter->textureRegistry[i].path[255] = '\0';
            return;
        }
    }

    int slot = adapter->textureRegistryCount;
    if (slot >= MAX_AI_TEXTURE_REGISTRY)
    {
        // Truly circular: overwrite the oldest entry, keep the rest addressable
        slot = adapter->textureRegistryNext;
        adapter->textureRegistryNext = (slot + 1) % MAX_AI_TEXTURE_REGISTRY;
    }
    adapter->textureRegistry[slot].id = textureId;
    strncpy(adapter->textureRegistry[slot].path, path, 255);
    adapter->textureRegistry[slot].path[255] = '\0';
    if (adapter->textureRegistryCount < MAX_AI_TEXTURE_REGISTRY) adapter->textureRegistryCount++;
}

// Add an element to the adapter's buffer (cross-unit: facade auto-capture calls this)
void AIAddElement(AIAdapter *adapter, const AIDrawElement *elem)
{
    if (adapter == NULL || elem == NULL) return;
    if (adapter->elementCount >= MAX_AI_ELEMENTS)
    {
        adapter->elementOverflowCount++;
        AI_SET_ERROR(adapter, AI_ERR_BUFFER_FULL, "scene element buffer full (%d)", MAX_AI_ELEMENTS);
        if (adapter->elementOverflowCount == 1)
        {
            TRACELOG(LOG_WARNING, "AIADAPTER: Scene element buffer full (%d), elements dropped — check scene JSON 'overflow' field", MAX_AI_ELEMENTS);
        }
        return;
    }
    adapter->elements[adapter->elementCount++] = *elem;
}

// Escape special characters for JSON string output (cross-unit: facade AIGetErrorsJSON uses it)
int AIEscapeJSON(const char *input, char *output, size_t outputSize)
{
    if (input == NULL || output == NULL || outputSize == 0) return 0;

    int written = 0;
    for (const char *p = input; *p != '\0' && written < (int)outputSize - 1; p++)
    {
        unsigned char c = (unsigned char)*p;
        const char *replace = NULL;

        switch (c)
        {
            case '"':  replace = "\\\""; break;
            case '\\': replace = "\\\\"; break;
            case '\n': replace = "\\n";  break;
            case '\r': replace = "\\r";  break;
            case '\t': replace = "\\t";  break;
            case '\b': replace = "\\b";  break;
            case '\f': replace = "\\f";  break;
            default:
                if (c < 0x20)
                {
                    if (written + 6 < (int)outputSize)
                    {
                        written += snprintf(output + written, outputSize - written, "\\u%04x", c);
                    }
                    continue;
                }
                output[written++] = (char)c;
                continue;
        }

        if (replace != NULL)
        {
            int needed = (int)strlen(replace);
            if (written + needed < (int)outputSize)
            {
                memcpy(output + written, replace, (size_t)needed);
                written += needed;
            }
        }
    }

    output[written] = '\0';
    return written;
}

const char *AIGetSceneJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    int screenWidth = adapter->deps->getScreenWidth();
    int screenHeight = adapter->deps->getScreenHeight();

    AI_JSON_APPEND(json, remaining, offset,
        "{\n"
        "  \"protocol\": \"%s\",\n"
        "  \"screen\": {\"width\": %d, \"height\": %d},\n",
        AI_PROTOCOL_VERSION, screenWidth, screenHeight);

    int elemCount = adapter->elementCount;
    const AIDrawElement *elems = adapter->elements;

    AI_JSON_APPEND(json, remaining, offset,
        "  \"elements\": [\n");

    int i = 0;
    int elementsWritten = 0;
    bool truncated = false;
    for (i = 0; i < elemCount; i++)
    {
        if (remaining - offset < 512)
        {
            truncated = true;
            break;
        }

        const AIDrawElement *e = &elems[i];
        Color c = e->color;

        if (e->type < AI_DRAW_CLEAR || e->type > AI_DRAW_SPRITE) continue;

        if (elementsWritten > 0)
        {
            AI_JSON_APPEND(json, remaining, offset, ",\n");
        }

        switch (e->type)
        {
        case AI_DRAW_CLEAR:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_RECT:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f, \"rotation\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->w, e->h, e->rotation, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_RECT_LINES:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->w, e->h, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_RECT_ROUNDED:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f, \"radius\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->w, e->h, e->radius, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_CIRCLE:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"radius\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->radius, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_CIRCLE_LINES:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"radius\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->radius, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_CIRCLE_SECTOR:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"radius\": %.1f, \"startAngle\": %.1f, \"endAngle\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->radius, e->rotation, (float)e->sides, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_TRIANGLE:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"v1\": [%.1f, %.1f], \"v2\": [%.1f, %.1f], \"v3\": [%.1f, %.1f], \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->x2, e->y2, e->x3, e->y3, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_TRIANGLE_LINES:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"v1\": [%.1f, %.1f], \"v2\": [%.1f, %.1f], \"v3\": [%.1f, %.1f], \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->x2, e->y2, e->x3, e->y3, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_POLY:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"sides\": %d, \"radius\": %.1f, \"rotation\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->sides, e->radius, e->rotation, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_POLY_LINES:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"sides\": %d, \"radius\": %.1f, \"rotation\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->sides, e->radius, e->rotation, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_TEXT: {
            char escapedText[256];
            AIEscapeJSON(e->text, escapedText, sizeof(escapedText));
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"text\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"size\": %d, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], escapedText, e->x, e->y, e->fontSize, c.r, c.g, c.b, c.a);
            break;
        }
        case AI_DRAW_TEXTURE: {
            char escapedSource[300];
            AIEscapeJSON(e->source, escapedSource, sizeof(escapedSource));
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f, \"rotation\": %.1f, \"source\": \"%s\", \"tint\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->w, e->h, e->rotation, escapedSource, c.r, c.g, c.b, c.a);
            break;
        }
        case AI_DRAW_LINE:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x1\": %.1f, \"y1\": %.1f, \"x2\": %.1f, \"y2\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->x2, e->y2, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_SPRITE: {
            char escapedSource[300];
            char escapedSheet[300];
            char escapedTag[80];
            char fxList[64] = "[]";
            AIEscapeJSON(e->source, escapedSource, sizeof(escapedSource));
            AIEscapeJSON(e->sheet, escapedSheet, sizeof(escapedSheet));
            AIEscapeJSON(e->tag, escapedTag, sizeof(escapedTag));

            const char *anchorName = (e->anchor == AI_SPRITE_CENTER) ? "center" :
                                     (e->anchor == AI_SPRITE_BOTTOM_CENTER) ? "bottom_center" : "top_left";
            if ((e->fx & (AI_SPRITE_FX_FLASH | AI_SPRITE_FX_TINT_ADD)) != 0)
            {
                bool hasFlash = (e->fx & AI_SPRITE_FX_FLASH) != 0;
                bool hasTintAdd = (e->fx & AI_SPRITE_FX_TINT_ADD) != 0;
                snprintf(fxList, sizeof(fxList), "[%s%s%s]",
                         hasFlash ? "\"flash\"" : "",
                         (hasFlash && hasTintAdd) ? "," : "",
                         hasTintAdd ? "\"tint_add\"" : "");
            }

            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f, \"scale\": %.2f, \"source\": \"%s\", \"sheet\": \"%s\", \"tag\": \"%s\", \"frame\": %d, \"frame_count\": %d, \"loop\": %s, \"flip_x\": %s, \"anchor\": \"%s\", \"fx\": %s, \"tint\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->w, e->h, e->scale,
                escapedSource, escapedSheet, escapedTag, e->frame, e->frameCount,
                (e->loop != 0) ? "true" : "false", e->flipX ? "true" : "false",
                anchorName, fxList,
                c.r, c.g, c.b, c.a);
            break;
        }
        case AI_DRAW_LINE_BEZIER:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x1\": %.1f, \"y1\": %.1f, \"x2\": %.1f, \"y2\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, e->x2, e->y2, c.r, c.g, c.b, c.a);
            break;
        case AI_DRAW_PIXEL:
            AI_JSON_APPEND(json, remaining, offset,
                "    {\"type\": \"%s\", \"x\": %.1f, \"y\": %.1f, \"color\": [%d, %d, %d, %d]}",
                aiElementTypeNames[e->type], e->x, e->y, c.r, c.g, c.b, c.a);
            break;
        default:
            continue;
        }

        elementsWritten++;
    }

    if (elementsWritten > 0)
    {
        AI_JSON_APPEND(json, remaining, offset, "\n");
    }
    AI_JSON_APPEND(json, remaining, offset, "  ]");
    if (truncated || (adapter->elementOverflowCount > 0))
    {
        AI_JSON_APPEND(json, remaining, offset,
            ",\n"
            "  \"truncated\": %s,\n"
            "  \"overflow\": %d\n"
            "}", truncated ? "true" : "false", adapter->elementOverflowCount);
    }
    else
    {
        AI_JSON_APPEND(json, remaining, offset,
            "\n"
            "}");
    }

    return json;
}

void AIRegisterElement(AIAdapter *adapter, const AIDrawElement *element)
{
    AIAddElement(adapter, element);
}

void AIClearElements(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    adapter->elementCount = 0;
    adapter->elementOverflowCount = 0;
}

int AIGetElementCount(AIAdapter *adapter)
{
    if (adapter == NULL) return 0;
    return adapter->elementCount;
}

void AIClearBackground(Color color)
{
    ClearBackground(color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIClearElements(adapter);

        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_CLEAR;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawRect(float x, float y, float w, float h, float rotation, Color color)
{
    DrawRectanglePro((Rectangle){ x, y, w, h }, (Vector2){ 0, 0 }, rotation, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_RECT;
        elem.x = x; elem.y = y;
        elem.w = w; elem.h = h;
        elem.rotation = rotation;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawRectLines(float x, float y, float w, float h, float lineThick, Color color)
{
    if (lineThick <= 0.0f) lineThick = 1.0f;
    DrawRectangleLinesEx((Rectangle){ x, y, w, h }, (int)lineThick, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_RECT_LINES;
        elem.x = x; elem.y = y;
        elem.w = w; elem.h = h;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawRectRounded(float x, float y, float w, float h, float roundness, float lineThick, Color color)
{
    Rectangle rec = { x, y, w, h };
    if (roundness > 0.5f) roundness = 0.5f;
    float minSide = (rec.width < rec.height) ? rec.width : rec.height;
    float radius = minSide * roundness;

    // Engine-native rounded rect (fill or outline with real corner rounding)
    if (lineThick > 0.0f)
    {
        DrawRectangleRoundedLines(rec, roundness, 16, lineThick, color);
    }
    else
    {
        DrawRectangleRounded(rec, roundness, 16, color);
    }

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_RECT_ROUNDED;
        elem.x = rec.x; elem.y = rec.y;
        elem.w = rec.width; elem.h = rec.height;
        elem.radius = radius;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawCircle(float x, float y, float radius, Color color)
{
    DrawCircleV((Vector2){ x, y }, radius, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_CIRCLE;
        elem.x = x; elem.y = y;
        elem.radius = radius;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawCircleLines(float x, float y, float radius, Color color)
{
    DrawCircleLinesV((Vector2){ x, y }, radius, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_CIRCLE_LINES;
        elem.x = x; elem.y = y;
        elem.radius = radius;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawCircleSector(float x, float y, float radius, float startAngle, float endAngle, float lineThick, Color color)
{
    Vector2 center = { x, y };

    // Engine-native circle sector (triangle fan fill or outline)
    if (lineThick > 0.0f)
    {
        DrawCircleSectorLines(center, radius, startAngle, endAngle, 32, color);
    }
    else
    {
        DrawCircleSector(center, radius, startAngle, endAngle, 32, color);
    }

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_CIRCLE_SECTOR;
        elem.x = center.x; elem.y = center.y;
        elem.radius = radius;
        elem.rotation = startAngle;   // startAngle stored in rotation
        elem.sides = (int)endAngle;   // endAngle stored in sides (degrees)
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color)
{
    DrawTriangle((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, (Vector2){ x3, y3 }, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_TRIANGLE;
        elem.x = x1; elem.y = y1;
        elem.x2 = x2; elem.y2 = y2;
        elem.x3 = x3; elem.y3 = y3;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawTriangleLines(float x1, float y1, float x2, float y2, float x3, float y3, Color color)
{
    DrawTriangleLines((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, (Vector2){ x3, y3 }, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_TRIANGLE_LINES;
        elem.x = x1; elem.y = y1;
        elem.x2 = x2; elem.y2 = y2;
        elem.x3 = x3; elem.y3 = y3;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawPoly(float x, float y, int sides, float radius, float rotation, Color color)
{
    DrawPoly((Vector2){ x, y }, sides, radius, rotation, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_POLY;
        elem.x = x; elem.y = y;
        elem.sides = sides;
        elem.radius = radius;
        elem.rotation = rotation;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawPolyLines(float x, float y, int sides, float radius, float rotation, float lineThick, Color color)
{
    if (lineThick <= 0.0f) lineThick = 1.0f;
    DrawPolyLinesEx((Vector2){ x, y }, sides, radius, rotation, lineThick, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_POLY_LINES;
        elem.x = x; elem.y = y;
        elem.sides = sides;
        elem.radius = radius;
        elem.rotation = rotation;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawLine(float x1, float y1, float x2, float y2, float thick, Color color)
{
    if (thick <= 0.0f) thick = 1.0f;
    DrawLineEx((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, thick, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_LINE;
        elem.x = x1; elem.y = y1;
        elem.x2 = x2; elem.y2 = y2;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawLineBezier(float x1, float y1, float x2, float y2, float thick, Color color)
{
    if (thick <= 0.0f) thick = 1.0f;
    DrawLineBezier((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, thick, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_LINE_BEZIER;
        elem.x = x1; elem.y = y1;
        elem.x2 = x2; elem.y2 = y2;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawPixel(float x, float y, Color color)
{
    DrawPixelV((Vector2){ x, y }, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_PIXEL;
        elem.x = x; elem.y = y;
        elem.color = color;
        AIAddElement(adapter, &elem);
    }
}

void AIDrawText(Font font, const char *text, float x, float y, float fontSize, float spacing, Color color)
{
    if (font.texture.id == 0) font = GetFontDefault();
    if (spacing <= 0.0f) spacing = 1.0f;
    DrawTextEx(font, text, (Vector2){ x, y }, fontSize, spacing, color);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_TEXT;
        elem.x = x; elem.y = y;
        elem.fontSize = (int)fontSize;
        elem.color = color;
        if (text != NULL)
        {
            strncpy(elem.text, text, sizeof(elem.text) - 1);
            elem.text[sizeof(elem.text) - 1] = '\0';
        }
        AIAddElement(adapter, &elem);
    }
}

void AIDrawTexture(Texture2D texture, Rectangle source, float x, float y, float w, float h, float rotation, Color tint)

{
    if (source.width == 0.0f) { source.x = 0.0f; source.y = 0.0f; source.width = (float)texture.width; source.height = (float)texture.height; }
    if (w <= 0.0f) w = source.width;
    if (h <= 0.0f) h = source.height;

    if (rotation == 0.0f)
    {
        DrawTextureRec(texture, source, (Vector2){ x, y }, tint);
    }
    else
    {
        Rectangle dest = { x, y, w, h };
        DrawTexturePro(texture, source, dest, (Vector2){ 0, 0 }, rotation, tint);
    }

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL)
    {
        AIDrawElement elem = { 0 };
        elem.type = AI_DRAW_TEXTURE;
        elem.x = x; elem.y = y;
        elem.w = w; elem.h = h;
        elem.rotation = rotation;
        elem.color = tint;
        const char *path = AILookupTexturePath(adapter, texture.id);
        strncpy(elem.source, path, sizeof(elem.source) - 1);
        elem.source[sizeof(elem.source) - 1] = '\0';
        AIAddElement(adapter, &elem);
    }
}

Texture2D AILoadTexture(const char *fileName)
{
    Texture2D texture = LoadTexture(fileName);

    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter != NULL && texture.id > 0)
    {
        AIRegisterTexture(adapter, texture.id, fileName);
    }

    return texture;
}

static const char *AIElementTypeStr(AIDrawElementType t)
{
    if ((int)t >= 0 && (int)t < (int)(sizeof(aiElementTypeNames)/sizeof(aiElementTypeNames[0])))
    {
        return aiElementTypeNames[t];
    }
    return "unknown";
}

void AISaveSnapshot(AIAdapter *adapter)

{
    if (adapter == NULL) return;
    int count = adapter->elementCount;
    if (count > MAX_AI_ELEMENTS) count = MAX_AI_ELEMENTS;
    memcpy(adapter->snapshotElements, adapter->elements, sizeof(AIDrawElement) * count);
    adapter->snapshotElementCount = count;
    adapter->snapshotCount++;
    TRACELOG(LOG_INFO, "AIADAPTER: Snapshot saved (%d elements, total snapshots: %d)",
             count, adapter->snapshotCount);
}

void AIClearSnapshots(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    adapter->snapshotElementCount = 0;
    adapter->snapshotCount = 0;
    TRACELOG(LOG_INFO, "AIADAPTER: Snapshots cleared");
}

int AIGetSnapshotCount(AIAdapter *adapter)
{
    return (adapter != NULL) ? adapter->snapshotCount : 0;
}

// Semantic comparison between the live scene and its snapshot. Which extra
// fields matter is decided by the type's X-macro flags, so text/rotation/sides
// changes are no longer silently ignored (see C1/C2 in docs/improvement-plan.md).
static bool AIElementChanged(const AIDrawElement *c, const AIDrawElement *s)
{
    if (c->type != s->type) return true;

    int flags = AIElementFieldFlags(c->type);

    if ((c->x != s->x) || (c->y != s->y) || (c->w != s->w) || (c->h != s->h)) return true;
    if (c->radius != s->radius) return true;
    if ((c->color.r != s->color.r) || (c->color.g != s->color.g) ||
        (c->color.b != s->color.b) || (c->color.a != s->color.a)) return true;

    if ((flags & AI_HAS_TEXT) &&
        ((c->fontSize != s->fontSize) || (strcmp(c->text, s->text) != 0))) return true;
    if ((flags & AI_HAS_ROTATION) && (c->rotation != s->rotation)) return true;
    if ((flags & AI_HAS_SIDES) && (c->sides != s->sides)) return true;
    // Circle sector stores startAngle/endAngle in rotation/sides
    if ((flags & AI_HAS_SECTOR) && ((c->rotation != s->rotation) || (c->sides != s->sides))) return true;
    if ((flags & AI_HAS_POINTS) &&
        ((c->x2 != s->x2) || (c->y2 != s->y2) || (c->x3 != s->x3) || (c->y3 != s->y3))) return true;
    if ((flags & AI_HAS_SOURCE) && (strcmp(c->source, s->source) != 0)) return true;
    if (flags & AI_HAS_SPRITE)
    {
        if ((strcmp(c->tag, s->tag) != 0) || (strcmp(c->sheet, s->sheet) != 0)) return true;
        if ((c->frame != s->frame) || (c->frameCount != s->frameCount)) return true;
        if ((c->loop != s->loop) || (c->scale != s->scale) || (c->flipX != s->flipX)) return true;
        if ((c->anchor != s->anchor) || (c->fx != s->fx)) return true;
    }

    return false;
}

const char *AIGetSnapshotDiff(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    int offset = 0;
    int remaining = MAX_JSON_STRING;
    char *buf = adapter->diffBuffer;

    int curCount = adapter->elementCount;
    int snapCount = adapter->snapshotElementCount;
    int minCount = (curCount < snapCount) ? curCount : snapCount;

    int changed = 0;
    int added = (curCount > snapCount) ? (curCount - snapCount) : 0;
    int removed = (snapCount > curCount) ? (snapCount - curCount) : 0;

    for (int i = 0; i < minCount; i++)
    {
        AIDrawElement *c = &adapter->elements[i];
        AIDrawElement *s = &adapter->snapshotElements[i];
        if (AIElementChanged(c, s)) changed++;
    }

    AI_JSON_APPEND(buf, remaining, offset,
        "{\"snapshot_elements\": %d, \"current_elements\": %d, \"added\": %d, \"removed\": %d, \"changed\": %d, \"changes\": [",
        snapCount, curCount, added, removed, changed);

    int detailCount = 0;
    int maxDetails = 32;

    for (int i = 0; i < minCount && detailCount < maxDetails; i++)
    {
        AIDrawElement *c = &adapter->elements[i];
        AIDrawElement *s = &adapter->snapshotElements[i];

        bool isChanged = AIElementChanged(c, s);

        if (isChanged)
        {
            char oldText[800] = { 0 };
            char newText[800] = { 0 };
            if (AIElementFieldFlags(c->type) & AI_HAS_TEXT)
            {
                AIEscapeJSON(s->text, oldText, sizeof(oldText));
                AIEscapeJSON(c->text, newText, sizeof(newText));
            }

            if (detailCount > 0) AI_JSON_APPEND(buf, remaining, offset, ", ");
            AI_JSON_APPEND(buf, remaining, offset,
                "{\"index\": %d, \"action\": \"changed\", \"old_type\": \"%s\", \"new_type\": \"%s\", "
                "\"old_pos\": [%.1f, %.1f], \"new_pos\": [%.1f, %.1f], "
                "\"old_size\": [%.1f, %.1f], \"new_size\": [%.1f, %.1f], "
                "\"old_radius\": %.1f, \"new_radius\": %.1f, "
                "\"old_color\": [%d, %d, %d, %d], \"new_color\": [%d, %d, %d, %d], "
                "\"old_text\": \"%s\", \"new_text\": \"%s\", "
                "\"old_rotation\": %.1f, \"new_rotation\": %.1f, "
                "\"old_font_size\": %d, \"new_font_size\": %d, "
                "\"old_sides\": %d, \"new_sides\": %d}",
                i, AIElementTypeStr(s->type), AIElementTypeStr(c->type),
                s->x, s->y, c->x, c->y,
                s->w, s->h, c->w, c->h,
                s->radius, c->radius,
                s->color.r, s->color.g, s->color.b, s->color.a,
                c->color.r, c->color.g, c->color.b, c->color.a,
                oldText, newText,
                s->rotation, c->rotation,
                s->fontSize, c->fontSize,
                s->sides, c->sides);
            detailCount++;
        }
    }

    for (int i = snapCount; i < curCount && detailCount < maxDetails; i++)
    {
        AIDrawElement *c = &adapter->elements[i];
        if (detailCount > 0) AI_JSON_APPEND(buf, remaining, offset, ", ");
        AI_JSON_APPEND(buf, remaining, offset,
            "{\"index\": %d, \"action\": \"added\", \"type\": \"%s\", \"pos\": [%.1f, %.1f], "
            "\"size\": [%.1f, %.1f], \"color\": [%d, %d, %d, %d]}",
            i, AIElementTypeStr(c->type), c->x, c->y, c->w, c->h,
            c->color.r, c->color.g, c->color.b, c->color.a);
        detailCount++;
    }

    for (int i = curCount; i < snapCount && detailCount < maxDetails; i++)
    {
        AIDrawElement *s = &adapter->snapshotElements[i];
        if (detailCount > 0) AI_JSON_APPEND(buf, remaining, offset, ", ");
        AI_JSON_APPEND(buf, remaining, offset,
            "{\"index\": %d, \"action\": \"removed\", \"type\": \"%s\", \"pos\": [%.1f, %.1f], "
            "\"size\": [%.1f, %.1f], \"color\": [%d, %d, %d, %d]}",
            i, AIElementTypeStr(s->type), s->x, s->y, s->w, s->h,
            s->color.r, s->color.g, s->color.b, s->color.a);
        detailCount++;
    }

    if (detailCount >= maxDetails && (changed + added + removed) > maxDetails)
    {
        if (detailCount > 0) AI_JSON_APPEND(buf, remaining, offset, ", ");
        AI_JSON_APPEND(buf, remaining, offset,
            "{\"action\": \"truncated\", \"note\": \"more changes not shown\"}");
    }

    AI_JSON_APPEND(buf, remaining, offset, "]}");

    buf[MAX_JSON_STRING - 1] = '\0';
    return buf;
}
