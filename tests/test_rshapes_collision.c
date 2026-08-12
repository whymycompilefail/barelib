/**********************************************************************************************
*
*   test_rshapes_collision.c - Unit tests for rshapes collision detection
*
*   Tests pure collision detection functions — no windowing or GPU needed.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib.h"
#include <stdio.h>
#include <math.h>

//----------------------------------------------------------------------------------
// Test Framework Macros
//----------------------------------------------------------------------------------

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_SECTION(name) \
    do { \
        printf("\n=== %s ===\n", name); \
    } while(0)

#define TEST(name, condition) \
    do { \
        if (!(condition)) { \
            printf("  FAIL: %s\n", name); \
            testsFailed++; \
        } else { \
            printf("  PASS: %s\n", name); \
            testsPassed++; \
        } \
    } while(0)

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------

static void TestCheckCollisionRecs(void)
{
    TEST_SECTION("CheckCollisionRecs");

    Rectangle a = { 0, 0, 100, 100 };

    // Overlapping
    Rectangle b = { 50, 50, 100, 100 };
    TEST("overlapping rectangles", CheckCollisionRecs(a, b));

    // Separated
    Rectangle c = { 200, 200, 50, 50 };
    TEST("separated rectangles", !CheckCollisionRecs(a, c));

    // Identical
    TEST("identical rectangles", CheckCollisionRecs(a, a));

    // Touching edge (not overlapping, strict comparison)
    Rectangle d = { 100, 0, 50, 50 };
    TEST("touching right edge", !CheckCollisionRecs(a, d));

    // One contains the other
    Rectangle e = { -10, -10, 120, 120 };
    TEST("contained", CheckCollisionRecs(a, e));

    // Negative coordinates
    Rectangle f = { -50, -50, 60, 60 };
    Rectangle g = { -10, -10, 40, 40 };
    TEST("negative coords overlapping", CheckCollisionRecs(f, g));
}

static void TestGetCollisionRec(void)
{
    TEST_SECTION("GetCollisionRec");

    Rectangle a = { 0, 0, 100, 100 };

    // Partial overlap
    Rectangle b = { 50, 50, 100, 100 };
    Rectangle overlap = GetCollisionRec(a, b);
    TEST("partial overlap x", overlap.x == 50 && overlap.width == 50);
    TEST("partial overlap y", overlap.y == 50 && overlap.height == 50);

    // Full containment
    Rectangle c = { 10, 10, 20, 20 };
    overlap = GetCollisionRec(a, c);
    TEST("contained rec", overlap.x == 10 && overlap.y == 10 && overlap.width == 20 && overlap.height == 20);

    // No overlap
    Rectangle d = { 200, 200, 50, 50 };
    overlap = GetCollisionRec(a, d);
    TEST("no overlap width=0", overlap.width == 0 && overlap.height == 0);

    // Touching edge (no overlap)
    Rectangle e = { 100, 0, 50, 50 };
    overlap = GetCollisionRec(a, e);
    TEST("touching edge no collision", overlap.width == 0 && overlap.height == 0);
}

static void TestCheckCollisionCircles(void)
{
    TEST_SECTION("CheckCollisionCircles");

    // Overlapping
    TEST("overlapping circles", CheckCollisionCircles((Vector2){0,0}, 10, (Vector2){5,0}, 10));

    // Separated
    TEST("separated circles", !CheckCollisionCircles((Vector2){0,0}, 10, (Vector2){100,0}, 10));

    // Concentric
    TEST("concentric circles", CheckCollisionCircles((Vector2){0,0}, 10, (Vector2){0,0}, 5));

    // Touching
    TEST("touching circles", CheckCollisionCircles((Vector2){0,0}, 10, (Vector2){20,0}, 10));

    // Zero radius
    TEST("zero radius point inside", CheckCollisionCircles((Vector2){0,0}, 0, (Vector2){0,0}, 5));
    TEST("zero radius point outside", !CheckCollisionCircles((Vector2){100,0}, 0, (Vector2){0,0}, 5));
}

static void TestCheckCollisionCircleRec(void)
{
    TEST_SECTION("CheckCollisionCircleRec");

    Rectangle rec = { 0, 0, 100, 100 };

    // Circle center inside rect
    TEST("center inside rect", CheckCollisionCircleRec((Vector2){50,50}, 10, rec));

    // Circle overlapping rect from outside
    TEST("overlapping from outside", CheckCollisionCircleRec((Vector2){-10,50}, 20, rec));

    // Separated
    TEST("separated", !CheckCollisionCircleRec((Vector2){-50,50}, 10, rec));

    // Corner collision
    TEST("corner collision", CheckCollisionCircleRec((Vector2){-10,-10}, 15, rec));

    // No corner collision
    TEST("no corner collision", !CheckCollisionCircleRec((Vector2){-30,-30}, 10, rec));

    // Circle contains rect
    TEST("circle contains rect", CheckCollisionCircleRec((Vector2){50,50}, 200, rec));
}

static void TestCheckCollisionPointRec(void)
{
    TEST_SECTION("CheckCollisionPointRec");

    Rectangle rec = { 10, 10, 100, 100 };

    // Inside
    TEST("point inside", CheckCollisionPointRec((Vector2){50,50}, rec));

    // Outside
    TEST("point outside", !CheckCollisionPointRec((Vector2){200,200}, rec));

    // On left edge (inclusive)
    TEST("point on left edge", CheckCollisionPointRec((Vector2){10,50}, rec));

    // On right edge (exclusive)
    TEST("point on right edge", !CheckCollisionPointRec((Vector2){110,50}, rec));

    // On top edge (inclusive)
    TEST("point on top edge", CheckCollisionPointRec((Vector2){50,10}, rec));

    // On bottom edge (exclusive)
    TEST("point on bottom edge", !CheckCollisionPointRec((Vector2){50,110}, rec));
}

static void TestCheckCollisionPointCircle(void)
{
    TEST_SECTION("CheckCollisionPointCircle");

    Vector2 center = { 0, 0 };

    // Inside
    TEST("point inside circle", CheckCollisionPointCircle((Vector2){5,0}, center, 10));

    // Outside
    TEST("point outside circle", !CheckCollisionPointCircle((Vector2){100,0}, center, 10));

    // On edge
    TEST("point on edge", CheckCollisionPointCircle((Vector2){10,0}, center, 10));

    // At center
    TEST("point at center", CheckCollisionPointCircle((Vector2){0,0}, center, 10));

    // Zero radius
    TEST("zero radius at center", CheckCollisionPointCircle((Vector2){0,0}, center, 0));
    TEST("zero radius away", !CheckCollisionPointCircle((Vector2){5,0}, center, 0));
}

static void TestCheckCollisionPointTriangle(void)
{
    TEST_SECTION("CheckCollisionPointTriangle");

    Vector2 p1 = { 0, 0 };
    Vector2 p2 = { 100, 0 };
    Vector2 p3 = { 50, 100 };

    // Inside
    TEST("point inside triangle", CheckCollisionPointTriangle((Vector2){50,40}, p1, p2, p3));

    // Outside
    TEST("point outside triangle", !CheckCollisionPointTriangle((Vector2){200,200}, p1, p2, p3));

    // On vertex (not inside, strict >)
    TEST("point on vertex", !CheckCollisionPointTriangle((Vector2){0,0}, p1, p2, p3));

    // On edge (not inside, strict >)
    TEST("point on edge", !CheckCollisionPointTriangle((Vector2){50,0}, p1, p2, p3));
}

static void TestCheckCollisionPointLine(void)
{
    TEST_SECTION("CheckCollisionPointLine");

    Vector2 p1 = { 0, 0 };
    Vector2 p2 = { 100, 0 };

    // Point on line
    TEST("point on line", CheckCollisionPointLine((Vector2){50,0}, p1, p2, 1));

    // Point off line
    TEST("point off line", !CheckCollisionPointLine((Vector2){50,10}, p1, p2, 1));

    // Point within threshold
    TEST("point within threshold", CheckCollisionPointLine((Vector2){50,2}, p1, p2, 5));

    // Point beyond threshold
    TEST("point beyond threshold", !CheckCollisionPointLine((Vector2){50,10}, p1, p2, 5));

    // Point at start
    TEST("point at start", CheckCollisionPointLine((Vector2){0,0}, p1, p2, 1));

    // Point at end
    TEST("point at end", CheckCollisionPointLine((Vector2){100,0}, p1, p2, 1));

    // Point before start (outside segment)
    TEST("point before start", !CheckCollisionPointLine((Vector2){-10,0}, p1, p2, 1));
}

static void TestCheckCollisionLines(void)
{
    TEST_SECTION("CheckCollisionLines");

    Vector2 collisionPoint = { 0 };

    // Intersecting lines
    TEST("intersecting lines", CheckCollisionLines((Vector2){0,0}, (Vector2){100,100}, (Vector2){0,100}, (Vector2){100,0}, &collisionPoint));
    TEST("intersection point x=50", fabsf(collisionPoint.x - 50) < 0.1f);
    TEST("intersection point y=50", fabsf(collisionPoint.y - 50) < 0.1f);

    // Non-intersecting
    TEST("non-intersecting lines", !CheckCollisionLines((Vector2){0,0}, (Vector2){10,10}, (Vector2){20,20}, (Vector2){30,30}, &collisionPoint));

    // Parallel lines
    TEST("parallel lines", !CheckCollisionLines((Vector2){0,0}, (Vector2){100,0}, (Vector2){0,50}, (Vector2){100,50}, &collisionPoint));

    // NULL collision point (should not crash)
    TEST("NULL collision point", CheckCollisionLines((Vector2){0,0}, (Vector2){100,100}, (Vector2){0,100}, (Vector2){100,0}, NULL));
}

static void TestCheckCollisionPointPoly(void)
{
    TEST_SECTION("CheckCollisionPointPoly");

    // Square polygon
    Vector2 square[] = {
        { 0, 0 },
        { 100, 0 },
        { 100, 100 },
        { 0, 100 }
    };

    // Inside
    TEST("point inside square", CheckCollisionPointPoly((Vector2){50,50}, square, 4));

    // Outside
    TEST("point outside square", !CheckCollisionPointPoly((Vector2){200,200}, square, 4));

    // Triangle polygon
    Vector2 triangle[] = {
        { 0, 0 },
        { 100, 0 },
        { 50, 100 }
    };

    TEST("point inside triangle poly", CheckCollisionPointPoly((Vector2){50,40}, triangle, 3));

    // Too few points
    TEST("too few points (2)", !CheckCollisionPointPoly((Vector2){50,50}, square, 2));
}

static void TestCheckCollisionCircleLine(void)
{
    TEST_SECTION("CheckCollisionCircleLine");

    Vector2 p1 = { 0, 0 };
    Vector2 p2 = { 100, 0 };

    // Circle center near line
    TEST("circle near line", CheckCollisionCircleLine((Vector2){50,10}, 15, p1, p2));

    // Circle far from line
    TEST("circle far from line", !CheckCollisionCircleLine((Vector2){50,50}, 10, p1, p2));

    // Circle center on line
    TEST("circle on line", CheckCollisionCircleLine((Vector2){50,0}, 10, p1, p2));

    // Line as a single point
    TEST("line as single point", CheckCollisionCircleLine((Vector2){5,0}, 10, (Vector2){0,0}, (Vector2){0,0}));
    TEST("line as single point far", !CheckCollisionCircleLine((Vector2){50,0}, 10, (Vector2){0,0}, (Vector2){0,0}));
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  rshapes Collision Unit Tests\n");
    printf("========================================\n");

    TestCheckCollisionRecs();
    TestGetCollisionRec();
    TestCheckCollisionCircles();
    TestCheckCollisionCircleRec();
    TestCheckCollisionPointRec();
    TestCheckCollisionPointCircle();
    TestCheckCollisionPointTriangle();
    TestCheckCollisionPointLine();
    TestCheckCollisionLines();
    TestCheckCollisionPointPoly();
    TestCheckCollisionCircleLine();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}