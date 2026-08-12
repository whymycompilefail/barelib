/**********************************************************************************************
*
*   test_rcore_file.c - Unit tests for rcore file/path utility functions
*
*   Tests file system, path, and random utility functions that don't require a window.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static void TestPathGetFileName(void)
{
    TEST_SECTION("GetFileName");

    const char *s = NULL;

    s = GetFileName("path/to/file.txt");
    TEST("GetFileName simple path", strcmp(s, "file.txt") == 0);

    s = GetFileName("file.txt");
    TEST("GetFileName no dir", strcmp(s, "file.txt") == 0);

    s = GetFileName("C:\\path\\to\\file.txt");
    TEST("GetFileName Windows path", strcmp(s, "file.txt") == 0);

    s = GetFileName(NULL);
    TEST("GetFileName NULL", s == NULL);
}

static void TestPathGetFileNameWithoutExt(void)
{
    TEST_SECTION("GetFileNameWithoutExt");

    const char *s = NULL;

    s = GetFileNameWithoutExt("path/to/file.txt");
    TEST("GetFileNameWithoutExt simple", strcmp(s, "file") == 0);

    s = GetFileNameWithoutExt("file.txt");
    TEST("GetFileNameWithoutExt no dir", strcmp(s, "file") == 0);

    s = GetFileNameWithoutExt("file");
    TEST("GetFileNameWithoutExt no ext", strcmp(s, "file") == 0);

    s = GetFileNameWithoutExt(NULL);
    TEST("GetFileNameWithoutExt NULL", s == NULL);
}

static void TestPathGetFileExtension(void)
{
    TEST_SECTION("GetFileExtension");

    const char *s = NULL;

    s = GetFileExtension("file.txt");
    TEST("GetFileExtension simple", strcmp(s, ".txt") == 0);

    s = GetFileExtension("path/to/file.png");
    TEST("GetFileExtension with path", strcmp(s, ".png") == 0);

    s = GetFileExtension("file");
    TEST("GetFileExtension no ext", s == NULL || s[0] == '\0');

    s = GetFileExtension(NULL);
    TEST("GetFileExtension NULL", s == NULL);
}

static void TestPathGetDirectoryPath(void)
{
    TEST_SECTION("GetDirectoryPath");

    const char *s = NULL;

    s = GetDirectoryPath("path/to/file.txt");
    TEST("GetDirectoryPath not NULL", s != NULL);
    if (s != NULL) {
        // On Windows, may return "./path/to" or "path/to"
        TEST("GetDirectoryPath contains path", strstr(s, "path") != NULL);
    }

    s = GetDirectoryPath("file.txt");
    TEST("GetDirectoryPath no dir", s != NULL);

    s = GetDirectoryPath(NULL);
    TEST("GetDirectoryPath NULL", s == NULL);
}

static void TestPathGetPrevDirectoryPath(void)
{
    TEST_SECTION("GetPrevDirectoryPath");

    const char *s = NULL;

    s = GetPrevDirectoryPath("path/to/dir");
    TEST("GetPrevDirectoryPath one level up", s != NULL);

    s = GetPrevDirectoryPath("dir");
    TEST("GetPrevDirectoryPath single dir", s != NULL);

    s = GetPrevDirectoryPath(NULL);
    TEST("GetPrevDirectoryPath NULL", s == NULL);
}

static void TestPathIsFileNameValid(void)
{
    TEST_SECTION("IsFileNameValid");

    TEST("IsFileNameValid simple", IsFileNameValid("file.txt"));
    TEST("IsFileNameValid with chars", IsFileNameValid("my_file-123.txt"));

    TEST("IsFileNameValid NULL", !IsFileNameValid(NULL));
    TEST("IsFileNameValid empty", !IsFileNameValid(""));
}

static void TestPathIsFileExtension(void)
{
    TEST_SECTION("IsFileExtension");

    TEST("IsFileExtension .txt", IsFileExtension("file.txt", ".txt"));

    TEST("IsFileExtension wrong", !IsFileExtension("file.txt", ".png"));

    TEST("IsFileExtension NULL file", !IsFileExtension(NULL, ".txt"));
    TEST("IsFileExtension NULL ext", !IsFileExtension("file.txt", NULL));
}

static void TestFileExists(void)
{
    TEST_SECTION("FileExists");

    TEST("FileExists with invalid", !FileExists("nonexistent_file_xyzzy"));

    TEST("FileExists NULL", !FileExists(NULL));
}

static void TestDirectoryExists(void)
{
    TEST_SECTION("DirectoryExists");

    TEST("DirectoryExists nonexistent", !DirectoryExists("nonexistent_dir_xyzzy"));

    TEST("DirectoryExists NULL", !DirectoryExists(NULL));
}

static void TestGetFileLength(void)
{
    TEST_SECTION("GetFileLength");

    int len = GetFileLength("nonexistent_file_xyzzy");
    TEST("GetFileLength nonexistent", len == 0);

    TEST("GetFileLength NULL", GetFileLength(NULL) == 0);
}

static void TestRandom(void)
{
    TEST_SECTION("Random");

    SetRandomSeed(42);
    int v1 = GetRandomValue(0, 100);
    int v2 = GetRandomValue(0, 100);

    SetRandomSeed(42);
    int v3 = GetRandomValue(0, 100);
    int v4 = GetRandomValue(0, 100);

    TEST("SetRandomSeed deterministic", v1 == v3 && v2 == v4);

    TEST("GetRandomValue in range", v1 >= 0 && v1 <= 100);

    TEST("GetRandomValue min==max", GetRandomValue(5, 5) == 5);
    TEST("GetRandomValue zero range", GetRandomValue(0, 0) == 0);
}

static void TestWorkingDirectory(void)
{
    TEST_SECTION("Working Directory");

    const char *wd = GetWorkingDirectory();
    TEST("GetWorkingDirectory not NULL", wd != NULL);
    TEST("GetWorkingDirectory not empty", wd != NULL && wd[0] != '\0');

    const char *appDir = GetApplicationDirectory();
    TEST("GetApplicationDirectory not NULL", appDir != NULL);
}

static void TestLogging(void)
{
    TEST_SECTION("Logging");

    // These should not crash
    SetTraceLogLevel(LOG_WARNING);
    TraceLog(LOG_INFO, "This info should be suppressed");
    TraceLog(LOG_WARNING, "Test warning: %d", 42);
    TraceLog(LOG_ERROR, "Test error: %s", "fail");

    // Reset to default
    SetTraceLogLevel(LOG_INFO);

    TEST("Logging no crash", 1 == 1);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    // Suppress raylib log output during tests
    SetTraceLogLevel(LOG_WARNING);

    printf("========================================\n");
    printf("  rcore File/Path Utility Tests\n");
    printf("========================================\n");

    TestPathGetFileName();
    TestPathGetFileNameWithoutExt();
    TestPathGetFileExtension();
    TestPathGetDirectoryPath();
    TestPathGetPrevDirectoryPath();
    TestPathIsFileNameValid();
    TestPathIsFileExtension();
    TestFileExists();
    TestDirectoryExists();
    TestGetFileLength();
    TestRandom();
    TestWorkingDirectory();
    TestLogging();

    SetTraceLogLevel(LOG_INFO);

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}