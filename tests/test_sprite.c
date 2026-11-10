/**********************************************************************************************
*
*   test_sprite - Sprite sheet parsing and playback (pure CPU, no window/GPU needed)
*
*   Covers Pixel Loom native JSON, Aseprite hash/array JSON, tag direction,
*   per-frame timing, looping and non-looping playback.
*
**********************************************************************************************/

#include "raylib_ai_sprite.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(condition, message) \
    do { \
        checks++; \
        if (!(condition)) { failures++; printf("  FAIL: %s (%s:%d)\n", message, __FILE__, __LINE__); } \
    } while (0)

// Pixel Loom native export with three 32x32 frames and three tags.
static const char *NATIVE_JSON =
    "{"
    "\"image\":\"hero.png\","
    "\"frames\":["
    "{\"index\":0,\"source_index\":0,\"x\":0,\"y\":0,\"width\":32,\"height\":32,\"duration_ms\":100},"
    "{\"index\":1,\"source_index\":1,\"x\":32,\"y\":0,\"width\":32,\"height\":32,\"duration_ms\":200},"
    "{\"index\":2,\"source_index\":2,\"x\":64,\"y\":0,\"width\":32,\"height\":32,\"duration_ms\":100}"
    "],"
    "\"tags\":["
    "{\"name\":\"walk\",\"start\":0,\"end\":2,\"direction\":\"forward\"},"
    "{\"name\":\"back\",\"start\":0,\"end\":2,\"direction\":\"reverse\"},"
    "{\"name\":\"ping\",\"start\":0,\"end\":2,\"direction\":\"pingpong\"},"
    "{\"name\":\"\\u8d70\\u8def\",\"start\":1,\"end\":2,\"direction\":\"forward\"}"
    "],"
    "\"canvas\":{\"width\":32,\"height\":32}"
    "}";

// Aseprite JSON hash export: frames keyed by filename, tags under meta.frameTags.
static const char *ASEPRITE_HASH_JSON =
    "{"
    "\"frames\":{"
    "\"ase_0000.png\":{\"frame\":{\"x\":0,\"y\":0,\"w\":16,\"h\":16},\"rotated\":false,\"trimmed\":false,\"duration\":80},"
    "\"ase_0001.png\":{\"frame\":{\"x\":16,\"y\":0,\"w\":16,\"h\":16},\"rotated\":false,\"trimmed\":false,\"duration\":120}"
    "},"
    "\"meta\":{"
    "\"app\":\"https://github.com/yangsir2077/pixel-loom\",\"version\":\"0.7.17\","
    "\"image\":\"ase.png\",\"format\":\"RGBA8888\",\"size\":{\"w\":16,\"h\":16},"
    "\"frameTags\":["
    "{\"name\":\"run\",\"from\":0,\"to\":1,\"direction\":\"forward\"},"
    "{\"name\":\"once\",\"from\":0,\"to\":1,\"direction\":\"forward\",\"repeat\":\"1\"}"
    "]"
    "}"
    "}";

// Aseprite JSON array export.
static const char *ASEPRITE_ARRAY_JSON =
    "{"
    "\"frames\":["
    "{\"filename\":\"a_0000.png\",\"frame\":{\"x\":0,\"y\":0,\"w\":8,\"h\":8},\"duration\":50},"
    "{\"filename\":\"a_0001.png\",\"frame\":{\"x\":8,\"y\":0,\"w\":8,\"h\":8},\"duration\":50}"
    "],"
    "\"meta\":{\"image\":\"a.png\",\"size\":{\"w\":8,\"h\":8},"
    "\"frameTags\":[{\"name\":\"idle\",\"from\":0,\"to\":1,\"direction\":\"forward\"}]}"
    "}";

static int Parse(const char *json, const char *hint, AISprite **out)
{
    *out = AIParseSpriteSheet(json, (int)strlen(json), hint);
    return (*out != NULL);
}

static void TestNativeParse(void)
{
    printf("Native Pixel Loom sheet\n");

    AISprite *sprite = NULL;
    CHECK(Parse(NATIVE_JSON, "art/hero.json", &sprite), "native sheet parses");
    if (sprite == NULL) return;

    CHECK(AISpriteFrameCount(sprite) == 3, "frame count is 3");
    CHECK(sprite->tagCount == 4, "tag count is 4");
    CHECK(sprite->canvasWidth == 32 && sprite->canvasHeight == 32, "canvas is 32x32");
    CHECK(strcmp(sprite->imagePath, "art/hero.png") == 0, "atlas path resolves next to the JSON");
    CHECK(strcmp(sprite->sheetPath, "art/hero.json") == 0, "sheet path is recorded");
    CHECK(sprite->frames[1].source.x == 32.0f && sprite->frames[1].source.width == 32.0f, "frame 1 rect");
    CHECK(sprite->frames[1].durationMs == 200, "frame 1 duration");
    CHECK(AISpriteCurrentFrame(sprite) == 0, "starts on the first frame");
    CHECK(AISpriteIsPlaying(sprite) == false, "starts paused");
    CHECK(strcmp(sprite->tags[3].name, "\xe8\xb5\xb0\xe8\xb7\xaf") == 0, "unicode escape decodes to UTF-8");

    AIUnloadSpriteSheet(sprite);
}

static void TestForwardPlayback(void)
{
    printf("Forward playback and looping\n");

    AISprite *sprite = NULL;
    if (!Parse(NATIVE_JSON, "hero.json", &sprite)) { CHECK(0, "parse"); return; }

    CHECK(AISpritePlay(sprite, "walk") == true, "play walk");
    CHECK(AISpriteIsPlaying(sprite), "playing after play");

    AISpriteUpdate(sprite, 0.05f);
    CHECK(AISpriteCurrentFrame(sprite) == 0, "50ms keeps frame 0");

    AISpriteUpdate(sprite, 0.06f);
    CHECK(AISpriteCurrentFrame(sprite) == 1, "110ms advances to frame 1");

    AISpriteUpdate(sprite, 0.15f);
    CHECK(AISpriteCurrentFrame(sprite) == 1, "260ms still holds frame 1 (200ms duration)");

    AISpriteUpdate(sprite, 0.06f);
    CHECK(AISpriteCurrentFrame(sprite) == 2, "320ms advances to frame 2");

    AISpriteUpdate(sprite, 0.10f);
    CHECK(AISpriteCurrentFrame(sprite) == 0, "loops back to frame 0");

    AIUnloadSpriteSheet(sprite);
}

static void TestReverseAndPingpong(void)
{
    printf("Reverse and pingpong order\n");

    AISprite *sprite = NULL;
    if (!Parse(NATIVE_JSON, "hero.json", &sprite)) { CHECK(0, "parse"); return; }

    CHECK(AISpritePlay(sprite, "back") == true, "play reverse tag");
    CHECK(AISpriteCurrentFrame(sprite) == 2, "reverse starts on the last frame");
    AISpriteUpdate(sprite, 0.10f);
    CHECK(AISpriteCurrentFrame(sprite) == 1, "reverse advances backwards");

    CHECK(AISpritePlay(sprite, "ping") == true, "play pingpong tag");
    AISpriteSeek(sprite, 0);
    CHECK(AISpriteCurrentFrame(sprite) == 0, "pingpong step 0");
    AISpriteSeek(sprite, 3);
    CHECK(AISpriteCurrentFrame(sprite) == 1, "pingpong step 3 returns to frame 1");
    AISpriteSeek(sprite, 4);
    CHECK(AISpriteCurrentFrame(sprite) == 1, "pingpong clamps to its 4-step order");

    CHECK(AISpritePlay(sprite, "missing") == false, "unknown tag is rejected");
    CHECK(AISpritePlay(sprite, NULL) == true, "NULL tag plays the whole sheet");
    CHECK(AISpriteCurrentFrame(sprite) == 0, "whole sheet starts at frame 0");

    AIUnloadSpriteSheet(sprite);
}

static void TestAseprite(void)
{
    printf("Aseprite hash and array sheets\n");

    AISprite *sprite = NULL;
    CHECK(Parse(ASEPRITE_HASH_JSON, "art/ase.json", &sprite), "hash sheet parses");
    if (sprite != NULL)
    {
        CHECK(AISpriteFrameCount(sprite) == 2, "hash frame count is 2");
        CHECK(sprite->frames[0].source.x == 0.0f && sprite->frames[1].source.x == 16.0f, "hash frame rects");
        CHECK(sprite->frames[0].durationMs == 80 && sprite->frames[1].durationMs == 120, "hash durations");
        CHECK(sprite->tagCount == 2, "hash tag count is 2");
        CHECK(strcmp(sprite->imagePath, "art/ase.png") == 0, "hash atlas path");
        CHECK(sprite->tags[0].loop == true, "tag without repeat loops");
        CHECK(sprite->tags[1].loop == false, "tag with repeat does not loop");

        CHECK(AISpritePlay(sprite, "once") == true, "play non-looping tag");
        AISpriteUpdate(sprite, 0.05f);
        CHECK(AISpriteCurrentFrame(sprite) == 0, "50ms holds frame 0");
        AISpriteUpdate(sprite, 0.2f);
        CHECK(AISpriteIsPlaying(sprite) == false, "non-looping tag stops at the end");
        CHECK(AISpriteCurrentFrame(sprite) == 1, "non-looping tag rests on the last frame");

        AIUnloadSpriteSheet(sprite);
    }

    sprite = NULL;
    CHECK(Parse(ASEPRITE_ARRAY_JSON, "a.json", &sprite), "array sheet parses");
    if (sprite != NULL)
    {
        CHECK(AISpriteFrameCount(sprite) == 2, "array frame count is 2");
        CHECK(sprite->frames[1].source.x == 8.0f, "array frame rect");
        CHECK(AISpritePlay(sprite, "idle") == true, "array tag plays");
        AIUnloadSpriteSheet(sprite);
    }
}

static void TestInvalidInput(void)
{
    printf("Invalid input\n");

    AISprite *sprite = NULL;
    CHECK(Parse("{not json", NULL, &sprite) == false, "malformed JSON is rejected");
    CHECK(sprite == NULL, "malformed JSON returns NULL");

    sprite = NULL;
    CHECK(Parse("{\"image\":\"x.png\",\"frames\":[]}", NULL, &sprite) == false, "empty frames rejected");

    CHECK(AIParseSpriteSheet(NULL, 0, NULL) == NULL, "NULL buffer rejected");
    CHECK(AIParseSpriteSheet("{}", 2, NULL) == NULL, "sheet without frames rejected");

    AIUnloadSpriteSheet(NULL);
    AISpriteUpdate(NULL, 0.016f);
    AISpriteStop(NULL);
    AISpriteSeek(NULL, 0);
    CHECK(AISpriteCurrentFrame(NULL) == -1, "NULL sprite has no frame");
    CHECK(AISpriteFrameCount(NULL) == 0, "NULL sprite has no frames");
    CHECK(AISpritePlay(NULL, "walk") == false, "NULL sprite cannot play");
}

int main(void)
{
    printf("=== test_sprite ===\n");

    TestNativeParse();
    TestForwardPlayback();
    TestReverseAndPingpong();
    TestAseprite();
    TestInvalidInput();

    printf("%d checks, %d failures\n", checks, failures);
    if (failures == 0) printf("ALL TESTS PASSED\n");

    return (failures == 0) ? 0 : 1;
}
