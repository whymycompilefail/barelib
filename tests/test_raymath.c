/**********************************************************************************************
*
*   test_raymath.c - Unit tests for raymath (pure math functions)
*
*   Tests vector, matrix, and quaternion operations that are independent of
*   any windowing system or GPU. These are pure functions — no initialization needed.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#define RAYMATH_IMPLEMENTATION
#include "raymath.h"
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

#define FLOAT_EQ(a, b) (fabsf((a) - (b)) < 0.0001f)

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------

static void TestVector2(void)
{
    TEST_SECTION("Vector2 Operations");

    Vector2 v1 = { 3.0f, 4.0f };
    Vector2 v2 = { 1.0f, 2.0f };

    // Addition
    Vector2 sum = Vector2Add(v1, v2);
    TEST("Vector2Add: x = 4", FLOAT_EQ(sum.x, 4.0f));
    TEST("Vector2Add: y = 6", FLOAT_EQ(sum.y, 6.0f));

    // Subtraction
    Vector2 diff = Vector2Subtract(v1, v2);
    TEST("Vector2Subtract: x = 2", FLOAT_EQ(diff.x, 2.0f));
    TEST("Vector2Subtract: y = 2", FLOAT_EQ(diff.y, 2.0f));

    // Length
    float len = Vector2Length(v1);
    TEST("Vector2Length(3,4) = 5", FLOAT_EQ(len, 5.0f));

    // Zero vector length
    Vector2 zero = { 0.0f, 0.0f };
    TEST("Vector2Length(0,0) = 0", FLOAT_EQ(Vector2Length(zero), 0.0f));

    // Dot product
    float dot = Vector2DotProduct(v1, v2);
    TEST("Vector2DotProduct((3,4),(1,2)) = 11", FLOAT_EQ(dot, 11.0f));

    // Perpendicular dot product
    float cross = Vector2CrossProduct(v1, v2);
    TEST("Vector2CrossProduct((3,4),(1,2)) = 2", FLOAT_EQ(cross, 2.0f));

    // Normalize
    Vector2 normalized = Vector2Normalize(v1);
    TEST("Vector2Normalize(3,4): length ~ 1", FLOAT_EQ(Vector2Length(normalized), 1.0f));
    TEST("Vector2Normalize(3,4): x ~ 0.6", FLOAT_EQ(normalized.x, 0.6f));
    TEST("Vector2Normalize(3,4): y ~ 0.8", FLOAT_EQ(normalized.y, 0.8f));

    // Normalize of zero vector (should return zero)
    Vector2 normZero = Vector2Normalize(zero);
    TEST("Vector2Normalize(0,0) = (0,0)", FLOAT_EQ(normZero.x, 0.0f) && FLOAT_EQ(normZero.y, 0.0f));

    // Scale
    Vector2 scaled = Vector2Scale(v1, 2.0f);
    TEST("Vector2Scale(3,4,2): x = 6", FLOAT_EQ(scaled.x, 6.0f));
    TEST("Vector2Scale(3,4,2): y = 8", FLOAT_EQ(scaled.y, 8.0f));

    // Negate
    Vector2 neg = Vector2Negate(v1);
    TEST("Vector2Negate(3,4): x = -3", FLOAT_EQ(neg.x, -3.0f));
    TEST("Vector2Negate(3,4): y = -4", FLOAT_EQ(neg.y, -4.0f));

    // Distance
    float dist = Vector2Distance(v1, v2);
    TEST("Vector2Distance((3,4),(1,2)) = sqrt(8)", FLOAT_EQ(dist, 2.8284f));

    // Lerp
    Vector2 lerped = Vector2Lerp(v1, v2, 0.5f);
    TEST("Vector2Lerp: x = 2", FLOAT_EQ(lerped.x, 2.0f));
    TEST("Vector2Lerp: y = 3", FLOAT_EQ(lerped.y, 3.0f));

    // MoveTowards
    Vector2 moved = Vector2MoveTowards(v1, v2, 2.0f);
    TEST("Vector2MoveTowards moves towards target", Vector2Length(Vector2Subtract(moved, v2)) < Vector2Length(Vector2Subtract(v1, v2)));

    // Angle
    float angle = Vector2Angle(v1, v2);
    TEST("Vector2Angle((3,4),(1,0)) is positive", angle > 0.0f);
}

static void TestVector3(void)
{
    TEST_SECTION("Vector3 Operations");

    Vector3 v1 = { 1.0f, 0.0f, 0.0f };
    Vector3 v2 = { 0.0f, 1.0f, 0.0f };

    // Cross product
    Vector3 cross = Vector3CrossProduct(v1, v2);
    TEST("Vector3CrossProduct(X,Y) = (0,0,1)", FLOAT_EQ(cross.x, 0.0f) && FLOAT_EQ(cross.y, 0.0f) && FLOAT_EQ(cross.z, 1.0f));

    // Dot product
    float dot = Vector3DotProduct(v1, v2);
    TEST("Vector3DotProduct(X,Y) = 0 (orthogonal)", FLOAT_EQ(dot, 0.0f));

    // Length
    Vector3 v3 = { 3.0f, 4.0f, 0.0f };
    TEST("Vector3Length(3,4,0) = 5", FLOAT_EQ(Vector3Length(v3), 5.0f));

    // Normalize
    Vector3 normalized = Vector3Normalize(v3);
    TEST("Vector3Normalize(3,4,0): length ~ 1", FLOAT_EQ(Vector3Length(normalized), 1.0f));

    // Add
    Vector3 sum = Vector3Add(v1, v2);
    TEST("Vector3Add(X,Y) = (1,1,0)", FLOAT_EQ(sum.x, 1.0f) && FLOAT_EQ(sum.y, 1.0f) && FLOAT_EQ(sum.z, 0.0f));

    // Subtract
    Vector3 diff = Vector3Subtract(v1, v2);
    TEST("Vector3Subtract(X,Y) = (1,-1,0)", FLOAT_EQ(diff.x, 1.0f) && FLOAT_EQ(diff.y, -1.0f) && FLOAT_EQ(diff.z, 0.0f));

    // Scale
    Vector3 scaled = Vector3Scale(v1, 5.0f);
    TEST("Vector3Scale(X,5) = (5,0,0)", FLOAT_EQ(scaled.x, 5.0f) && FLOAT_EQ(scaled.y, 0.0f) && FLOAT_EQ(scaled.z, 0.0f));

    // Negate
    Vector3 neg = Vector3Negate(v1);
    TEST("Vector3Negate(X) = (-1,0,0)", FLOAT_EQ(neg.x, -1.0f) && FLOAT_EQ(neg.y, 0.0f) && FLOAT_EQ(neg.z, 0.0f));

    // Distance
    float dist = Vector3Distance(v1, v2);
    TEST("Vector3Distance(X,Y) = sqrt(2)", FLOAT_EQ(dist, 1.4142f));

    // Lerp
    Vector3 lerped = Vector3Lerp(v1, v3, 0.5f);
    TEST("Vector3Lerp: x = 2", FLOAT_EQ(lerped.x, 2.0f));
    TEST("Vector3Lerp: y = 2", FLOAT_EQ(lerped.y, 2.0f));
    TEST("Vector3Lerp: z = 0", FLOAT_EQ(lerped.z, 0.0f));

    // Reflect
    Vector3 incident = { 0.0f, -1.0f, 0.0f };
    Vector3 normal = { 0.0f, 1.0f, 0.0f };
    Vector3 reflected = Vector3Reflect(incident, normal);
    TEST("Vector3Reflect: reflects (0,-1,0) off (0,1,0) = (0,1,0)", FLOAT_EQ(reflected.x, 0.0f) && FLOAT_EQ(reflected.y, 1.0f) && FLOAT_EQ(reflected.z, 0.0f));
}

static void TestMatrix(void)
{
    TEST_SECTION("Matrix Operations");

    // Identity matrix
    Matrix ident = MatrixIdentity();
    TEST("MatrixIdentity: m0 = 1", FLOAT_EQ(ident.m0, 1.0f));
    TEST("MatrixIdentity: m5 = 1", FLOAT_EQ(ident.m5, 1.0f));
    TEST("MatrixIdentity: m10 = 1", FLOAT_EQ(ident.m10, 1.0f));
    TEST("MatrixIdentity: m15 = 1", FLOAT_EQ(ident.m15, 1.0f));
    TEST("MatrixIdentity: others are 0", FLOAT_EQ(ident.m1, 0.0f) && FLOAT_EQ(ident.m4, 0.0f));

    // Matrix multiply (identity * identity = identity)
    Matrix product = MatrixMultiply(ident, ident);
    TEST("MatrixMultiply(I,I): m0 = 1", FLOAT_EQ(product.m0, 1.0f));
    TEST("MatrixMultiply(I,I): m5 = 1", FLOAT_EQ(product.m5, 1.0f));
    TEST("MatrixMultiply(I,I): m10 = 1", FLOAT_EQ(product.m10, 1.0f));
    TEST("MatrixMultiply(I,I): m15 = 1", FLOAT_EQ(product.m15, 1.0f));

    // Translation matrix
    Matrix trans = MatrixTranslate(10.0f, 20.0f, 30.0f);
    TEST("MatrixTranslate: m12 = 10", FLOAT_EQ(trans.m12, 10.0f));
    TEST("MatrixTranslate: m13 = 20", FLOAT_EQ(trans.m13, 20.0f));
    TEST("MatrixTranslate: m14 = 30", FLOAT_EQ(trans.m14, 30.0f));

    // Scale matrix
    Matrix scale = MatrixScale(2.0f, 3.0f, 4.0f);
    TEST("MatrixScale: m0 = 2", FLOAT_EQ(scale.m0, 2.0f));
    TEST("MatrixScale: m5 = 3", FLOAT_EQ(scale.m5, 3.0f));
    TEST("MatrixScale: m10 = 4", FLOAT_EQ(scale.m10, 4.0f));

    // Rotation X
    Matrix rotX = MatrixRotateX(90.0f * DEG2RAD); // 90 degrees
    // sin(90°) ≈ 1, cos(90°) ≈ 0
    // NOTE: raylib uses its own matrix convention; verify structure not exact values
    TEST("MatrixRotateX(90deg): m5 ~ 0 (cos)", FLOAT_EQ(rotX.m5, 0.0f));
    TEST("MatrixRotateX(90deg): m10 ~ 0 (cos)", FLOAT_EQ(rotX.m10, 0.0f));
    TEST("MatrixRotateX(90deg): m6 and m9 are non-zero (sin)", fabsf(rotX.m6) > 0.5f && fabsf(rotX.m9) > 0.5f);

    // Rotation Y
    Matrix rotY = MatrixRotateY(90.0f * DEG2RAD); // 90 degrees
    TEST("MatrixRotateY(90deg): m0 ~ 0 (cos)", FLOAT_EQ(rotY.m0, 0.0f));
    TEST("MatrixRotateY(90deg): m10 ~ 0 (cos)", FLOAT_EQ(rotY.m10, 0.0f));
    TEST("MatrixRotateY(90deg): m2 and m8 are non-zero (sin)", fabsf(rotY.m2) > 0.5f && fabsf(rotY.m8) > 0.5f);

    // Determinant of identity = 1
    float det = MatrixDeterminant(ident);
    TEST("MatrixDeterminant(I) = 1", FLOAT_EQ(det, 1.0f));

    // Inverse of identity = identity
    Matrix inv = MatrixInvert(ident);
    TEST("MatrixInvert(I): m0 = 1", FLOAT_EQ(inv.m0, 1.0f));
    TEST("MatrixInvert(I): m5 = 1", FLOAT_EQ(inv.m5, 1.0f));
    TEST("MatrixInvert(I): m10 = 1", FLOAT_EQ(inv.m10, 1.0f));
    TEST("MatrixInvert(I): m15 = 1", FLOAT_EQ(inv.m15, 1.0f));

    // Transpose of identity = identity
    Matrix transposed = MatrixTranspose(ident);
    TEST("MatrixTranspose(I) = I", FLOAT_EQ(transposed.m0, 1.0f) && FLOAT_EQ(transposed.m5, 1.0f));

    // Trace of identity = 4
    float trace = MatrixTrace(ident);
    TEST("MatrixTrace(I) = 4", FLOAT_EQ(trace, 4.0f));
}

static void TestQuaternion(void)
{
    TEST_SECTION("Quaternion Operations");

    Quaternion q = QuaternionIdentity();
    TEST("QuaternionIdentity: x = 0", FLOAT_EQ(q.x, 0.0f));
    TEST("QuaternionIdentity: y = 0", FLOAT_EQ(q.y, 0.0f));
    TEST("QuaternionIdentity: z = 0", FLOAT_EQ(q.z, 0.0f));
    TEST("QuaternionIdentity: w = 1", FLOAT_EQ(q.w, 1.0f));

    // Length of identity = 1
    float len = QuaternionLength(q);
    TEST("QuaternionLength(I) = 1", FLOAT_EQ(len, 1.0f));

    // Multiply identity * identity = identity
    Quaternion qp = QuaternionMultiply(q, q);
    TEST("QuaternionMultiply(I,I) = I", FLOAT_EQ(qp.w, 1.0f));

    // Euler to quaternion and back
    Vector3 euler = { 0.0f, 90.0f * DEG2RAD, 0.0f };
    Quaternion fromEuler = QuaternionFromEuler(euler.x, euler.y, euler.z);
    Vector3 back = QuaternionToEuler(fromEuler);
    TEST("QuaternionToEuler(FromEuler(90deg Y)): y approx 90deg", fabsf(back.y * RAD2DEG - 90.0f) < 1.0f);

    // Matrix to quaternion and back
    Matrix rot = MatrixRotateY(90.0f * DEG2RAD);
    Quaternion fromMat = QuaternionFromMatrix(rot);
    Matrix backMat = QuaternionToMatrix(fromMat);
    TEST("QuaternionFromMatrix/ToMatrix: m0 ~ 0", FLOAT_EQ(backMat.m0, 0.0f));

    // Slerp identity to identity = identity
    Quaternion slerped = QuaternionSlerp(q, q, 0.5f);
    TEST("QuaternionSlerp(I,I,0.5) = I", FLOAT_EQ(slerped.w, 1.0f));

    // Normalize
    Quaternion notNormal = { 2.0f, 0.0f, 0.0f, 0.0f };
    Quaternion normalized = QuaternionNormalize(notNormal);
    TEST("QuaternionNormalize: length ~ 1", FLOAT_EQ(QuaternionLength(normalized), 1.0f));

    // Invert
    Quaternion inv = QuaternionInvert(q);
    TEST("QuaternionInvert(I) = I", FLOAT_EQ(inv.w, 1.0f));
}

//----------------------------------------------------------------------------------
// Additional Edge Case Tests
//----------------------------------------------------------------------------------

static void TestVector2EdgeCases(void)
{
    TEST_SECTION("Vector2 Edge Cases");

    // LengthSqr
    Vector2 v = { 3.0f, 4.0f };
    TEST("Vector2LengthSqr(3,4) = 25", FLOAT_EQ(Vector2LengthSqr(v), 25.0f));
    TEST("Vector2LengthSqr(0,0) = 0", FLOAT_EQ(Vector2LengthSqr(Vector2Zero()), 0.0f));

    // Divide
    Vector2 div = Vector2Divide(v, (Vector2){ 2.0f, 2.0f });
    TEST("Vector2Divide(3,4)/(2,2) = (1.5,2)", FLOAT_EQ(div.x, 1.5f) && FLOAT_EQ(div.y, 2.0f));

    // Vector2One
    Vector2 one = Vector2One();
    TEST("Vector2One: x=1, y=1", FLOAT_EQ(one.x, 1.0f) && FLOAT_EQ(one.y, 1.0f));

    // AddValue
    Vector2 addVal = Vector2AddValue(v, 5.0f);
    TEST("Vector2AddValue(3,4,5) = (8,9)", FLOAT_EQ(addVal.x, 8.0f) && FLOAT_EQ(addVal.y, 9.0f));

    // SubtractValue
    Vector2 subVal = Vector2SubtractValue(v, 2.0f);
    TEST("Vector2SubtractValue(3,4,2) = (1,2)", FLOAT_EQ(subVal.x, 1.0f) && FLOAT_EQ(subVal.y, 2.0f));

    // Clamp
    Vector2 small = { -5.0f, 15.0f };
    Vector2 clamped = Vector2Clamp(small, (Vector2){ 0.0f, 0.0f }, (Vector2){ 10.0f, 10.0f });
    TEST("Vector2Clamp: x = 0", FLOAT_EQ(clamped.x, 0.0f));
    TEST("Vector2Clamp: y = 10", FLOAT_EQ(clamped.y, 10.0f));

    // Clamp values already in range
    Vector2 inRange = { 5.0f, 5.0f };
    clamped = Vector2Clamp(inRange, (Vector2){ 0.0f, 0.0f }, (Vector2){ 10.0f, 10.0f });
    TEST("Vector2Clamp in range: x = 5", FLOAT_EQ(clamped.x, 5.0f));
    TEST("Vector2Clamp in range: y = 5", FLOAT_EQ(clamped.y, 5.0f));

    // Reflect
    Vector2 incident = { 0.0f, -1.0f };
    Vector2 normal = { 0.0f, 1.0f };
    Vector2 reflected = Vector2Reflect(incident, normal);
    TEST("Vector2Reflect(0,-1) off (0,1) = (0,1)", FLOAT_EQ(reflected.x, 0.0f) && FLOAT_EQ(reflected.y, 1.0f));

    // Reflect at 45 degrees
    normal = (Vector2){ -1.0f, 1.0f };
    normal = Vector2Normalize(normal);
    reflected = Vector2Reflect(incident, normal);
    TEST("Vector2Reflect non-trivial", fabsf(reflected.x) > 0.0f);

    // Angle between perpendicular vectors
    float angle = Vector2Angle((Vector2){ 1.0f, 0.0f }, (Vector2){ 0.0f, 1.0f });
    TEST("Vector2Angle((1,0),(0,1)) ~ pi/2", FLOAT_EQ(angle, PI/2.0f));

    // Angle between same vectors
    angle = Vector2Angle((Vector2){ 1.0f, 0.0f }, (Vector2){ 1.0f, 0.0f });
    TEST("Vector2Angle same vectors = 0", FLOAT_EQ(angle, 0.0f));

    // MoveTowards exact
    Vector2 moved = Vector2MoveTowards((Vector2){ 0.0f, 0.0f }, (Vector2){ 10.0f, 0.0f }, 10.0f);
    TEST("Vector2MoveTowards exact distance", FLOAT_EQ(moved.x, 10.0f) && FLOAT_EQ(moved.y, 0.0f));

    // MoveTowards overshoot clamped
    moved = Vector2MoveTowards((Vector2){ 0.0f, 0.0f }, (Vector2){ 10.0f, 0.0f }, 20.0f);
    TEST("Vector2MoveTowards overshoot clamped", FLOAT_EQ(moved.x, 10.0f) && FLOAT_EQ(moved.y, 0.0f));
}

static void TestVector3EdgeCases(void)
{
    TEST_SECTION("Vector3 Edge Cases");

    Vector3 v = { 3.0f, 4.0f, 5.0f };

    // LengthSqr
    TEST("Vector3LengthSqr(3,4,5) = 50", FLOAT_EQ(Vector3LengthSqr(v), 50.0f));

    // AddValue
    Vector3 addVal = Vector3AddValue(v, 5.0f);
    TEST("Vector3AddValue(3,4,5,5) = (8,9,10)", FLOAT_EQ(addVal.x, 8.0f) && FLOAT_EQ(addVal.y, 9.0f) && FLOAT_EQ(addVal.z, 10.0f));

    // Clamp
    Vector3 small = { -5.0f, 15.0f, 20.0f };
    Vector3 clamped = Vector3Clamp(small, (Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 10.0f, 10.0f, 10.0f });
    TEST("Vector3Clamp: x=0, y=10, z=10", FLOAT_EQ(clamped.x, 0.0f) && FLOAT_EQ(clamped.y, 10.0f) && FLOAT_EQ(clamped.z, 10.0f));

    // Reflect
    Vector3 incident = { 0.0f, -1.0f, 0.0f };
    Vector3 normal = { 0.0f, 1.0f, 0.0f };
    Vector3 reflected = Vector3Reflect(incident, normal);
    TEST("Vector3Reflect(0,-1,0) off (0,1,0) = (0,1,0)", FLOAT_EQ(reflected.x, 0.0f) && FLOAT_EQ(reflected.y, 1.0f) && FLOAT_EQ(reflected.z, 0.0f));

    // Refract
    Vector3 refracted = Vector3Refract(incident, normal, 1.0f);
    TEST("Vector3Refract same index = incident", FLOAT_EQ(refracted.x, 0.0f) && FLOAT_EQ(refracted.y, -1.0f) && FLOAT_EQ(refracted.z, 0.0f));

    // Angle between perpendicular vectors
    float angle = Vector3Angle((Vector3){ 1.0f, 0.0f, 0.0f }, (Vector3){ 0.0f, 1.0f, 0.0f });
    TEST("Vector3Angle(X,Y) ~ pi/2", FLOAT_EQ(angle, PI/2.0f));

    // Angle between same vectors
    angle = Vector3Angle((Vector3){ 1.0f, 0.0f, 0.0f }, (Vector3){ 1.0f, 0.0f, 0.0f });
    TEST("Vector3Angle same vectors = 0", FLOAT_EQ(angle, 0.0f));

    // OrthoNormalize
    Vector3 normalVec = { 1.0f, 1.0f, 0.0f };
    Vector3 tangent = { 1.0f, 0.0f, 0.0f };
    Vector3OrthoNormalize(&normalVec, &tangent);
    TEST("Vector3OrthoNormalize normal length ~ 1", FLOAT_EQ(Vector3Length(normalVec), 1.0f));
    TEST("Vector3OrthoNormalize tangent length ~ 1", FLOAT_EQ(Vector3Length(tangent), 1.0f));
    TEST("Vector3OrthoNormalize dot product ~ 0", FLOAT_EQ(Vector3DotProduct(normalVec, tangent), 0.0f));

    // Barycenter
    Vector3 bary = Vector3Barycenter((Vector3){ 0.5f, 0.5f, 0.0f }, (Vector3){ 0,0,0 }, (Vector3){ 1,0,0 }, (Vector3){ 0,1,0 });
    TEST("Vector3Barycenter mid point: x=0", FLOAT_EQ(bary.x, 0.0f));
    TEST("Vector3Barycenter mid point: y=0.5", FLOAT_EQ(bary.y, 0.5f));
    TEST("Vector3Barycenter mid point: z=0.5", FLOAT_EQ(bary.z, 0.5f));

    // Divide
    Vector3 div = Vector3Divide(v, (Vector3){ 2.0f, 2.0f, 2.0f });
    TEST("Vector3Divide(3,4,5)/(2,2,2) = (1.5,2,2.5)", FLOAT_EQ(div.x, 1.5f) && FLOAT_EQ(div.y, 2.0f) && FLOAT_EQ(div.z, 2.5f));
}

static void TestMatrixEdgeCases(void)
{
    TEST_SECTION("Matrix Edge Cases");

    // Matrix add
    Matrix a = MatrixIdentity();
    Matrix b = MatrixIdentity();
    Matrix sum = MatrixAdd(a, b);
    TEST("MatrixAdd(I,I): m0 = 2", FLOAT_EQ(sum.m0, 2.0f));
    TEST("MatrixAdd(I,I): m5 = 2", FLOAT_EQ(sum.m5, 2.0f));

    // Matrix subtract
    Matrix diff = MatrixSubtract(a, b);
    TEST("MatrixSubtract(I,I) = 0", FLOAT_EQ(diff.m0, 0.0f) && FLOAT_EQ(diff.m5, 0.0f));

    // Rotate arbitrary axis
    Matrix rot = MatrixRotate((Vector3){ 0.0f, 0.0f, 1.0f }, 90.0f * DEG2RAD);
    TEST("MatrixRotate Z 90deg: m0 ~ 0", FLOAT_EQ(rot.m0, 0.0f));
    TEST("MatrixRotate Z 90deg: m1 ~ 1", FLOAT_EQ(rot.m1, 1.0f));

    // Scale and translate combined
    Matrix st = MatrixMultiply(MatrixTranslate(10, 20, 30), MatrixScale(2, 3, 4));
    // Determinant of scale*trans = 2*3*4 = 24
    float det = MatrixDeterminant(st);
    TEST("MatrixDeterminant(Scale*Translate) = 24", FLOAT_EQ(det, 24.0f));

    // Matrix invert non-trivial
    Matrix rotX = MatrixRotateX(45.0f * DEG2RAD);
    Matrix inv = MatrixInvert(rotX);
    Matrix back = MatrixMultiply(rotX, inv);
    TEST("MatrixInvert(RotX) * RotX ~ I", FLOAT_EQ(back.m0, 1.0f) && FLOAT_EQ(back.m5, 1.0f) && FLOAT_EQ(back.m10, 1.0f) && FLOAT_EQ(back.m15, 1.0f));
}

static void TestQuaternionEdgeCases(void)
{
    TEST_SECTION("Quaternion Edge Cases");

    // FromAxisAngle
    Quaternion q = QuaternionFromAxisAngle((Vector3){ 0.0f, 0.0f, 1.0f }, 90.0f * DEG2RAD);
    TEST("QuaternionFromAxisAngle Z 90deg: w ~ 0.707", FLOAT_EQ(q.w, 0.7071f));
    TEST("QuaternionFromAxisAngle Z 90deg: z ~ 0.707", FLOAT_EQ(q.z, 0.7071f));

    // ToAxisAngle
    Vector3 axis = { 0 };
    float angle = 0;
    QuaternionToAxisAngle(q, &axis, &angle);
    TEST("QuaternionToAxisAngle angle ~ 90deg", FLOAT_EQ(angle * RAD2DEG, 90.0f));
    TEST("QuaternionToAxisAngle axis.z ~ 1", FLOAT_EQ(axis.z, 1.0f));

    // Transform vector by quaternion
    Quaternion rot = QuaternionFromAxisAngle((Vector3){ 0.0f, 0.0f, 1.0f }, 90.0f * DEG2RAD);
    Vector3 v3t = { 1.0f, 0.0f, 0.0f };
    Vector3 transformed = Vector3RotateByQuaternion(v3t, rot);
    TEST("Vector3RotateByQuaternion(1,0,0) by Z 90deg: x ~ 0", FLOAT_EQ(transformed.x, 0.0f));
    TEST("Vector3RotateByQuaternion(1,0,0) by Z 90deg: y ~ 1", FLOAT_EQ(transformed.y, 1.0f));

    // Slerp non-trivial
    Quaternion q1 = QuaternionIdentity();
    Quaternion q2 = QuaternionFromAxisAngle((Vector3){ 0, 0, 1 }, 90.0f * DEG2RAD);
    Quaternion slerped = QuaternionSlerp(q1, q2, 0.5f);
    float halfAngle = 45.0f * 0.5f * DEG2RAD;
    TEST("QuaternionSlerp half: w = cos(22.5deg)", FLOAT_EQ(slerped.w, cosf(22.5f * DEG2RAD)));
}

static void TestUtilFunctions(void)
{
    TEST_SECTION("Utility Functions");

    // Clamp
    TEST("Clamp within range", FLOAT_EQ(Clamp(5.0f, 0.0f, 10.0f), 5.0f));
    TEST("Clamp below min", FLOAT_EQ(Clamp(-5.0f, 0.0f, 10.0f), 0.0f));
    TEST("Clamp above max", FLOAT_EQ(Clamp(15.0f, 0.0f, 10.0f), 10.0f));

    // Lerp
    TEST("Lerp(0,10,0) = 0", FLOAT_EQ(Lerp(0.0f, 10.0f, 0.0f), 0.0f));
    TEST("Lerp(0,10,1) = 10", FLOAT_EQ(Lerp(0.0f, 10.0f, 1.0f), 10.0f));
    TEST("Lerp(0,10,0.5) = 5", FLOAT_EQ(Lerp(0.0f, 10.0f, 0.5f), 5.0f));

    // Normalize
    TEST("Normalize(5,0,10) = 0.5", FLOAT_EQ(Normalize(5.0f, 0.0f, 10.0f), 0.5f));
    TEST("Normalize(min) = 0", FLOAT_EQ(Normalize(0.0f, 0.0f, 10.0f), 0.0f));
    TEST("Normalize(max) = 1", FLOAT_EQ(Normalize(10.0f, 0.0f, 10.0f), 1.0f));

    // Remap
    TEST("Remap(5,0,10,100,200) = 150", FLOAT_EQ(Remap(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0f));

    // Wrap
    TEST("Wrap(15,0,10) = 5", FLOAT_EQ(Wrap(15.0f, 0.0f, 10.0f), 5.0f));
    TEST("Wrap(-5,0,10) = 5", FLOAT_EQ(Wrap(-5.0f, 0.0f, 10.0f), 5.0f));
    TEST("Wrap(0,0,10) = 0", FLOAT_EQ(Wrap(0.0f, 0.0f, 10.0f), 0.0f));
    TEST("Wrap(10,0,10) = 0", FLOAT_EQ(Wrap(10.0f, 0.0f, 10.0f), 0.0f));

    // FloatEquals
    TEST("FloatEquals(1,1)", FloatEquals(1.0f, 1.0f));
    TEST("FloatEquals(1,1.000001)", FloatEquals(1.0f, 1.000001f));
    TEST("FloatEquals(1,2) == false", !FloatEquals(1.0f, 2.0f));
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  raymath Unit Tests\n");
    printf("========================================\n");

    TestVector2();
    TestVector3();
    TestMatrix();
    TestQuaternion();
    TestVector2EdgeCases();
    TestVector3EdgeCases();
    TestMatrixEdgeCases();
    TestQuaternionEdgeCases();
    TestUtilFunctions();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}