/**********************************************************************************************
*
*   rcore_fs - File system management functions for raylib
*
*   This file contains all file system related functions extracted from rcore.c.
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

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_FILEPATH_LENGTH
    #if defined(_WIN32)
        #define MAX_FILEPATH_LENGTH      256
    #else
        #define MAX_FILEPATH_LENGTH     4096
    #endif
#endif
#ifndef FILE_FILTER_TAG_ALL
    #define FILE_FILTER_TAG_ALL        "*.*"
#endif
#ifndef FILE_FILTER_TAG_FILE_ONLY
    #define FILE_FILTER_TAG_FILE_ONLY  "FILES*"
#endif
#ifndef FILE_FILTER_TAG_DIR_ONLY
    #define FILE_FILTER_TAG_DIR_ONLY   "DIRS*"
#endif

//----------------------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------------------
#include "raylib.h"
#include "config.h"
#include "raylib_context.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
    #include <io.h>
    #include <direct.h>
    #define GETCWD _getcwd
    #define CHDIR _chdir
    #define MKDIR(dir) _mkdir(dir)
    #define ACCESS(fn) _access(fn, 0)
#else
    #include <unistd.h>
    #define GETCWD getcwd
    #define CHDIR chdir
    #define MKDIR(dir) mkdir(dir, 0777)
    #define ACCESS(fn) access(fn, F_OK)
#endif
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
    #define DIRENT_MALLOC RL_MALLOC
    #define DIRENT_FREE RL_FREE
    #include "external/dirent.h"
#else
    #include <dirent.h>
#endif

// Platform specific declarations for GetApplicationDirectory()
#if defined(_WIN32)
    #if !defined(MAX_PATH)
        #define MAX_PATH 260
    #endif

    struct HINSTANCE__;
    #if defined(__cplusplus)
    extern "C" {
    #endif
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(struct HINSTANCE__ *hModule, char *lpFilename, unsigned long nSize);
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(struct HINSTANCE__ *hModule, wchar_t *lpFilename, unsigned long nSize);
    __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
    #if defined(__cplusplus)
    }
    #endif
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <sys/syslimits.h>
    #include <mach-o/dyld.h>
#endif

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static LoadFileDataCallback loadFileData = NULL;    // LoadFileData callback function pointer
static SaveFileDataCallback saveFileData = NULL;    // SaveFileText callback function pointer
static LoadFileTextCallback loadFileText = NULL;    // LoadFileText callback function pointer
static SaveFileTextCallback saveFileText = NULL;    // SaveFileText callback function pointer

//----------------------------------------------------------------------------------
// Module Functions Definition: File System management
//----------------------------------------------------------------------------------

// Load data from file into a buffer
unsigned char *LoadFileData(const char *fileName, int *dataSize)
{
    unsigned char *data = NULL;
    *dataSize = 0;

    if (fileName != NULL)
    {
        if (loadFileData) return loadFileData(fileName, dataSize);

        FILE *file = fopen(fileName, "rb");

        if (file != NULL)
        {
            // WARNING: On binary streams SEEK_END could not be found,
            // using fseek() and ftell() could not work in some (rare) cases
            fseek(file, 0, SEEK_END);
            int size = ftell(file);     // WARNING: ftell() returns 'long int', maximum size returned is INT_MAX (2147483647 bytes)
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                data = (unsigned char *)RL_CALLOC(size, sizeof(unsigned char));

                if (data != NULL)
                {
                    // NOTE: fread() returns number of read elements instead of bytes, so reading [1 byte, size elements]
                    size_t count = fread(data, sizeof(unsigned char), size, file);

                    // WARNING: fread() returns a size_t value, usually 'unsigned int' (32bit compilation) and 'unsigned long long' (64bit compilation)
                    // dataSize is unified along raylib as a 'int' type, so, for file-sizes >INT_MAX (2147483647 bytes) there is a limitation
                    if (count > 2147483647)
                    {
                        TRACELOG(LOG_WARNING, "FILEIO: [%s] File is bigger than 2147483647 bytes, avoid using LoadFileData()", fileName);

                        RL_FREE(data);
                        data = NULL;
                    }
                    else
                    {
                        *dataSize = (int)count;

                        if ((*dataSize) != size) TRACELOG(LOG_WARNING, "FILEIO: [%s] File partially loaded (%i bytes out of %i)", fileName, *dataSize, size);
                        else TRACELOG(LOG_INFO, "FILEIO: [%s] File loaded successfully", fileName);
                    }
                }
                else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to allocated memory for file reading", fileName);
            }
            else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to read file", fileName);

            fclose(file);
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return data;
}

// Unload file data allocated by LoadFileData()
void UnloadFileData(unsigned char *data)
{
    RL_FREE(data);
}

// Save data to file from buffer
bool SaveFileData(const char *fileName, const void *data, int dataSize)
{
    bool result = false;

    if (fileName != NULL)
    {
        if (saveFileData) return saveFileData(fileName, data, dataSize);

        FILE *file = fopen(fileName, "wb");

        if (file != NULL)
        {
            // WARNING: fwrite() returns a size_t value, usually 'unsigned int' (32bit compilation) and 'unsigned long long' (64bit compilation)
            // and expects a size_t input value but as dataSize is limited to INT_MAX (2147483647 bytes), there shouldn't be a problem
            int count = (int)fwrite(data, sizeof(unsigned char), dataSize, file);

            if (count == 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to write file", fileName);
            else if (count != dataSize) TRACELOG(LOG_WARNING, "FILEIO: [%s] File partially written", fileName);
            else TRACELOG(LOG_INFO, "FILEIO: [%s] File saved successfully", fileName);

            int closed = fclose(file);
            if (closed == 0) result = true;
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return result;
}

// Export data to code (.h), returns true on success
bool ExportDataAsCode(const unsigned char *data, int dataSize, const char *fileName)
{
    bool result = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    // NOTE: Text data buffer size is estimated considering raw data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    char *txtData = (char *)RL_CALLOC(dataSize*6 + 2000, sizeof(char));

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// DataAsCode exporter v1.0 - Raw data exported as an array of bytes                  //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path
    char varFileName[256] = { 0 };
    strncpy(varFileName, GetFileNameWithoutExt(fileName), 256 - 1);
    for (int i = 0; varFileName[i] != '\0'; i++)
    {
        // Convert variable name to uppercase
        if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = varFileName[i] - 32; }
        // Replace non valid character for C identifier with '_'
        else if (varFileName[i] == '.' || varFileName[i] == '-' || varFileName[i] == '?' || varFileName[i] == '!' || varFileName[i] == '+') { varFileName[i] = '_'; }
    }

    byteCount += sprintf(txtData + byteCount, "#define %s_DATA_SIZE     %i\n\n", varFileName, dataSize);

    byteCount += sprintf(txtData + byteCount, "static unsigned char %s_DATA[%s_DATA_SIZE] = { ", varFileName, varFileName);
    for (int i = 0; i < (dataSize - 1); i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), data[i]);
    byteCount += sprintf(txtData + byteCount, "0x%x };\n", data[dataSize - 1]);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    RL_FREE(txtData);

    if (result != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Data as code exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export data as code", fileName);

    return result;
}

// Load text data from file, returns a '\0' terminated string
// NOTE: text chars array should be freed manually
char *LoadFileText(const char *fileName)
{
    char *text = NULL;

    if (fileName != NULL)
    {
        if (loadFileText) return loadFileText(fileName);

        FILE *file = fopen(fileName, "rt");

        if (file != NULL)
        {
            // WARNING: When reading a file as 'text' file,
            // text mode causes carriage return-linefeed translation...
            // ...but using fseek() should return correct byte-offset
            fseek(file, 0, SEEK_END);
            unsigned int size = (unsigned int)ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                text = (char *)RL_CALLOC(size + 1, sizeof(char));

                if (text != NULL)
                {
                    unsigned int count = (unsigned int)fread(text, sizeof(char), size, file);

                    // WARNING: \r\n is converted to \n on reading, so,
                    // read bytes count gets reduced by the number of lines
                    if (count < size) text = (char *)RL_REALLOC(text, count + 1);

                    // Zero-terminate the string
                    text[count] = '\0';

                    TRACELOG(LOG_INFO, "FILEIO: [%s] Text file loaded successfully", fileName);
                }
                else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to allocated memory for file reading", fileName);
            }
            else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to read text file", fileName);

            fclose(file);
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open text file", fileName);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return text;
}

// Unload file text data allocated by LoadFileText()
void UnloadFileText(char *text)
{
    RL_FREE(text);
}

// Save text data to file (write), string must be '\0' terminated
bool SaveFileText(const char *fileName, const char *text)
{
    bool result = false;

    if (fileName != NULL)
    {
        if (saveFileText) return saveFileText(fileName, text);

        FILE *file = fopen(fileName, "wt");

        if (file != NULL)
        {
            int count = fprintf(file, "%s", text);

            if (count < 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to write text file", fileName);
            else TRACELOG(LOG_INFO, "FILEIO: [%s] Text file saved successfully", fileName);

            int closed = fclose(file);
            if (closed == 0) result = true;
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open text file", fileName);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return result;
}

// File access custom callbacks
// WARNING: Callbacks setup is intended for advanced users

// Set custom file binary data loader
void SetLoadFileDataCallback(LoadFileDataCallback callback)
{
    loadFileData = callback;
}

// Set custom file binary data saver
void SetSaveFileDataCallback(SaveFileDataCallback callback)
{
    saveFileData = callback;
}

// Set custom file text data loader
void SetLoadFileTextCallback(LoadFileTextCallback callback)
{
    loadFileText = callback;
}

// Set custom file text data saver
void SetSaveFileTextCallback(SaveFileTextCallback callback)
{
    saveFileText = callback;
}

// Rename file (if exists)
// NOTE: Only rename file name required, not full path
int FileRename(const char *fileName, const char *fileRename)
{
    int result = -1;

    if (FileExists(fileName))
    {
        result = rename(fileName, fileRename);
    }

    return result;
}

// Remove file (if exists)
int FileRemove(const char *fileName)
{
    int result = -1;

    if (FileExists(fileName))
    {
        result = remove(fileName);
    }

    return result;
}

// Copy file from one path to another
// NOTE: If destination path does not exist, it is created!
int FileCopy(const char *srcPath, const char *dstPath)
{
    int result = -1;
    int srcDataSize = 0;
    unsigned char *srcFileData = LoadFileData(srcPath, &srcDataSize);

    // Create required paths if they do not exist
    if (DirectoryExists(GetDirectoryPath(dstPath))) result = 0; // Already exists
    else result = MakeDirectory(GetDirectoryPath(dstPath));

    if (result == 0) // Directory created successfully or already exists
    {
        if ((srcFileData != NULL) && (srcDataSize > 0))
        {
            bool saved = SaveFileData(dstPath, srcFileData, srcDataSize);
            if (saved) result = 0;
        }
    }

    UnloadFileData(srcFileData);

    return result;
}

// Move file from one directory to another
// NOTE: If dst directories do not exists they are created
int FileMove(const char *srcPath, const char *dstPath)
{
    int result = -1;

    if (FileExists(srcPath))
    {
        result = FileCopy(srcPath, dstPath);

        if (result == 0)
        {
            // Make sure file has been correctly copied before removing
            if (FileExists(dstPath) && (GetFileLength(srcPath) == GetFileLength(dstPath)))
            {
                result = FileRemove(srcPath);
                if (result != 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to remove source file after copy", srcPath);
            }
            else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to copy file to [%s]", srcPath, dstPath);
        }
    }
	else TRACELOG(LOG_WARNING, "FILEIO: [%s] Source file does not exist", srcPath);

    return result;
}



// Check if the file exists
bool FileExists(const char *fileName)
{
    bool result = false;

    if ((fileName != NULL) && (ACCESS(fileName) != -1)) result = true;

    // NOTE: Alternatively, stat() can be used instead of access()
    //#include <sys/stat.h>
    //struct stat statbuf;
    //if (stat(filename, &statbuf) == 0) result = true;

    return result;
}

// Check file extension
bool IsFileExtension(const char *fileName, const char *ext)
{
    #define MAX_FILE_EXTENSIONS  32

    bool result = false;
    const char *fileExt = GetFileExtension(fileName);

    if ((fileExt == NULL) || (ext == NULL)) return result;

    // WARNING: fileExt points to last '.' on fileName string but it could happen
    // that fileName is not correct: "myfile.png more text following\n"

    if (fileExt != NULL)
    {
        int fileExtLength = (int)strlen(fileExt);
        char fileExtLower[16] = { 0 };
        for (int i = 0; (i < fileExtLength) && (i < 15); i++)
        {
            // Copy and convert to lower-case
            if ((fileExt[i] >= 'A') && (fileExt[i] <= 'Z')) fileExtLower[i] =  fileExt[i] + 32;
            else fileExtLower[i] =  fileExt[i];
        }

        int extCount = 1;
        int extLength = (int)strlen(ext);
        char *extList = (char *)RL_CALLOC(extLength + 1, 1);
        char *extListPtrs[MAX_FILE_EXTENSIONS] = { 0 };
        memcpy(extList, ext, extLength);
        extListPtrs[0] = extList;

        for (int i = 0; i < extLength; i++)
        {
            // Convert to lower-case if extension is upper-case
            if ((extList[i] >= 'A') && (extList[i] <= 'Z')) extList[i] += 32;

            // Get pointer to next extension and add null-terminator
            if (extList[i] == ';')
            {
                extList[i] = '\0';

                if (extCount < MAX_FILE_EXTENSIONS)
                {
                    extListPtrs[extCount] = extList + i + 1;
                    extCount++;
                }
            }
        }

        for (int i = 0; i < extCount; i++)
        {
            // Consider the case where extension provided
            // does not start with the '.'
            char *fileExtLowerPtr = fileExtLower;
            if (extListPtrs[i][0] != '.') fileExtLowerPtr++;

            if (strcmp(fileExtLowerPtr, extListPtrs[i]) == 0)
            {
                result = true;
                break;
            }
        }

        RL_FREE(extList);
    }

    return result;
}

// Check if directory path exists
bool DirectoryExists(const char *dirPath)
{
    bool result = false;
    DIR *dir = opendir(dirPath);

    if (dir != NULL)
    {
        result = true;
        closedir(dir);
    }

    return result;
}

// Get file length in bytes
// NOTE: GetFileSize() conflicts with windows.h
int GetFileLength(const char *fileName)
{
    int size = 0;

    if (fileName == NULL) return size;

    // NOTE: On Unix-like systems, it can by used the POSIX system call: stat(),
    // but depending on the platform that call could not be available
    //struct stat result = { 0 };
    //stat(fileName, &result);
    //return result.st_size;

    FILE *file = fopen(fileName, "rb");

    if (file != NULL)
    {
        fseek(file, 0L, SEEK_END);
        long int fileSize = ftell(file);

        // Check for size overflow (INT_MAX)
        if (fileSize > 2147483647) TRACELOG(LOG_WARNING, "[%s] File size overflows expected limit, do not use GetFileLength()", fileName);
        else size = (int)fileSize;

        fclose(file);
    }

    return size;
}

// Get file modification time (last write time)
long GetFileModTime(const char *fileName)
{
    struct stat result = { 0 };
    long modTime = 0;

    if (stat(fileName, &result) == 0)
    {
        time_t mod = result.st_mtime;
        modTime = (long)mod;
    }

    return modTime;
}

// Get pointer to extension for a filename string (includes the dot: .png)
// WARNING: Getting the pointer to the input string extension position (not a string copy)
const char *GetFileExtension(const char *fileName)
{
    const char *dot = NULL;
    
    if (fileName == NULL) return NULL;
    
    dot = strrchr(fileName, '.');

    if (!dot || (dot == fileName)) return NULL;

    return dot;
}

// String pointer reverse break: returns right-most occurrence of charset in s
static const char *strprbrk(const char *text, const char *charset)
{
    const char *latestMatch = NULL;

    for (; (text != NULL) && (text = strpbrk(text, charset)); latestMatch = text++) { }

    return latestMatch;
}

// Get pointer to filename for a path string
const char *GetFileName(const char *filePath)
{
    const char *fileName = NULL;

    if (filePath != NULL) fileName = strprbrk(filePath, "\\/");

    if (fileName == NULL) return filePath;

    return fileName + 1;
}

// Get filename string without extension (uses static string)
const char *GetFileNameWithoutExt(const char *filePath)
{
    #define MAX_FILENAME_LENGTH     256

    static char fileName[MAX_FILENAME_LENGTH] = { 0 };
    memset(fileName, 0, MAX_FILENAME_LENGTH);

    if (filePath == NULL) return NULL;

    if (filePath != NULL)
    {
        strncpy(fileName, GetFileName(filePath), MAX_FILENAME_LENGTH - 1); // Get filename.ext without path
        int fileNameLength = (int)strlen(fileName); // Get size in bytes

        for (int i = fileNameLength; i > 0; i--) // Reverse search '.'
        {
            if (fileName[i] == '.')
            {
                // NOTE: Break on first '.' found
                fileName[i] = '\0';
                break;
            }
        }
    }

    return fileName;
}

// Get directory for a given filePath
const char *GetDirectoryPath(const char *filePath)
{
    /*
    // NOTE: Directory separator is different in Windows and other platforms,
    // fortunately, Windows also support the '/' separator, that's the one should be used
    #if defined(_WIN32)
        char separator = '\\';
    #else
        char separator = '/';
    #endif
    */
    const char *lastSlash = NULL;
    static char dirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(dirPath, 0, MAX_FILEPATH_LENGTH);

    if (filePath == NULL) return NULL;

    // In case provided path does not contain a root drive letter (C:\, D:\)
    // nor leading path separator (\, /), add the current directory path to dirPath
    if ((filePath[1] != ':') && (filePath[0] != '\\') && (filePath[0] != '/'))
    {
        // For security, set starting path to current directory,
        // obtained path will be concatenated to this
        dirPath[0] = '.';
        dirPath[1] = '/';
    }

    lastSlash = strprbrk(filePath, "\\/");
    if (lastSlash)
    {
        if (lastSlash == filePath)
        {
            // The last and only slash is the leading one: path is in a root directory
            dirPath[0] = filePath[0];
            dirPath[1] = '\0';
        }
        else
        {
            // NOTE: Be careful, strncpy() is not safe, it does not care about '\0'
            char *dirPathPtr = dirPath;
            if ((filePath[1] != ':') && (filePath[0] != '\\') && (filePath[0] != '/')) dirPathPtr += 2;     // Skip drive letter, "C:"
            memcpy(dirPathPtr, filePath, strlen(filePath) - (strlen(lastSlash) - 1));
            dirPath[strlen(filePath) - strlen(lastSlash) + (((filePath[1] != ':') && (filePath[0] != '\\') && (filePath[0] != '/'))? 2 : 0)] = '\0';  // Add '\0' manually
        }
    }

    return dirPath;
}

// Get previous directory path for a given path
const char *GetPrevDirectoryPath(const char *dirPath)
{
    static char prevDirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(prevDirPath, 0, MAX_FILEPATH_LENGTH);

    if (dirPath == NULL) return NULL;

    int dirPathLength = (int)strlen(dirPath);

    if (dirPathLength <= 3) strncpy(prevDirPath, dirPath, MAX_FILEPATH_LENGTH  - 1);

    for (int i = (dirPathLength - 1); (i >= 0) && (dirPathLength > 3); i--)
    {
        if ((dirPath[i] == '\\') || (dirPath[i] == '/'))
        {
            // Check for root: "C:\" or "/"
            if (((i == 2) && (dirPath[1] ==':')) || (i == 0)) i++;

            strncpy(prevDirPath, dirPath, i);
            break;
        }
    }

    return prevDirPath;
}

// Get current working directory
const char *GetWorkingDirectory(void)
{
    static char currentDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(currentDir, 0, MAX_FILEPATH_LENGTH);

    char *path = GETCWD(currentDir, MAX_FILEPATH_LENGTH);

    return path;
}

const char *GetApplicationDirectory(void)
{
    static char appDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(appDir, 0, MAX_FILEPATH_LENGTH);

#if defined(_WIN32)
    int len = 0;

    #if defined(UNICODE)
    unsigned short widePath[MAX_PATH];
    len = GetModuleFileNameW(NULL, (wchar_t *)widePath, MAX_PATH);
    len = WideCharToMultiByte(0, 0, (wchar_t *)widePath, len, appDir, MAX_PATH, NULL, NULL);
    #else
    len = GetModuleFileNameA(NULL, appDir, MAX_PATH);
    #endif

    if (len > 0)
    {
        for (int i = len; i >= 0; i--)
        {
            if (appDir[i] == '\\')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '\\';
    }

#elif defined(__linux__)

    unsigned int size = sizeof(appDir);
    ssize_t len = readlink("/proc/self/exe", appDir, size);

    if (len > 0)
    {
        for (int i = len; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__APPLE__)

    uint32_t size = sizeof(appDir);

    if (_NSGetExecutablePath(appDir, &size) == 0)
    {
        int appDirLength = (int)strlen(appDir);
        for (int i = appDirLength; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__FreeBSD__)

    size_t size = sizeof(appDir);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};

    if (sysctl(mib, 4, appDir, &size, NULL, 0) == 0)
    {
        int appDirLength = (int)strlen(appDir);
        for (int i = appDirLength; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__wasm__)

    appDir[0] = '/';
#endif

    return appDir;
}

// Load directory filepaths
// NOTE: Base path is prepended to the scanned filepaths
// Forward declarations for functions used before their definitions
// NOTE: Required for MSVC C89 mode which allows implicit int declarations
static unsigned int GetDirectoryFileCountEx(const char *basePath, const char *filter, bool scanSubdirs);
static void ScanDirectoryFiles(const char *basePath, FilePathList *files, const char *filter, unsigned int expectedFileCount, bool scanSubdirs);

// WARNING: Directory is scanned twice, first time to get paths count
// Scanneed files and directories, no recursive/subdirs scanning
FilePathList LoadDirectoryFiles(const char *dirPath)
{
    return LoadDirectoryFilesEx(dirPath, FILE_FILTER_TAG_ALL, false);
}

// Load directory filepaths with extension filtering and recursive directory scan
// Use "*.*" to include all files and directories on scan
// Use "FILES*" to include only files on scan
// Use "DIRS*" to include only directories on scan
// WARNING: Directory is scanned twice, first time to get paths count
FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter, bool scanSubdirs)
{
    FilePathList files = { 0 };

    if (DirectoryExists(basePath)) // It's a directory
    {
        if ((filter != NULL) && (filter[0] == '\0')) filter = NULL;

        // SCAN 1: Count files
        unsigned int fileCounter = GetDirectoryFileCountEx(basePath, filter, scanSubdirs);

        // Memory allocation for dirFileCount
        files.paths = (char **)RL_CALLOC(fileCounter, sizeof(char *));
        for (unsigned int i = 0; i < fileCounter; i++) files.paths[i] = (char *)RL_CALLOC(MAX_FILEPATH_LENGTH, sizeof(char));

        // SCAN 2: Read filepaths
        // WARNING: basePath is always prepended to scanned paths
        ScanDirectoryFiles(basePath, &files, filter, fileCounter, scanSubdirs);

        // Security check: read files.count should match fileCounter
        if (files.count != fileCounter)
        {
            TRACELOG(LOG_WARNING, "FILEIO: Read files count (%u) does not match capacity allocated (%u)", files.count, fileCounter);
            files.count = fileCounter; // Avoid memory leak when unloading this FilePathList
        }
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Directory cannot be opened (%s)", basePath);  // Maybe it's a file...

    return files;
}

// Unload directory filepaths
// WARNING: files.count is not reseted to 0 after unloading
void UnloadDirectoryFiles(FilePathList files)
{
    if (files.paths != NULL)
    {
        for (unsigned int i = 0; i < files.count; i++) RL_FREE(files.paths[i]);

        RL_FREE(files.paths);
    }
}

// Create directories (including full path requested), returns 0 on success
int MakeDirectory(const char *dirPath)
{
    if ((dirPath == NULL) || (dirPath[0] == '\0')) return -1; // Path is not valid
    if (DirectoryExists(dirPath)) return 0; // Path already exists (is valid)

    // Copy path string to avoid modifying original
    int dirPathLength = (int)strlen(dirPath) + 1;
    char *pathcpy = (char *)RL_CALLOC(dirPathLength, 1);
    memcpy(pathcpy, dirPath, dirPathLength);

    // Iterate over pathcpy, create each subdirectory as needed
    for (int i = 0; (i < dirPathLength) && (pathcpy[i] != '\0'); i++)
    {
        if (pathcpy[i] == ':') i++;
        else
        {
            if ((pathcpy[i] == '\\') || (pathcpy[i] == '/'))
            {
                pathcpy[i] = '\0';
                if (!DirectoryExists(pathcpy)) MKDIR(pathcpy);
                pathcpy[i] = '/';
            }
        }
    }

    // Create final directory
    if (!DirectoryExists(pathcpy)) MKDIR(pathcpy);
    RL_FREE(pathcpy);

    // In case something failed and requested directory
    // was not successfully created, return -1
    if (!DirectoryExists(dirPath)) return -1;

    return 0;
}

// Change working directory, returns true on success
int ChangeDirectory(const char *dirPath)
{
    // NOTE: On success, CHDIR() return 0; on error, returns -1 and errno is set to indicate the error,
    // depending on the filesystem, other errors can be returned
    int result = CHDIR(dirPath);

    if (result != 0) TRACELOG(LOG_WARNING, "SYSTEM: Failed to change to directory: %s", dirPath);
    else TRACELOG(LOG_INFO, "SYSTEM: Working Directory: %s", dirPath);

    return result;
}

// Check if given path point to a file
bool IsPathFile(const char *path)
{
    struct stat result = { 0 };
    stat(path, &result);

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
    return S_ISREG(result.st_mode);
}

// Check if fileName is valid for the platform/OS
bool IsFileNameValid(const char *fileName)
{
    bool valid = true;

    if ((fileName == NULL) || (fileName[0] == '\0')) valid = false;
    else
    {
        int fileNameLength = (int)strlen(fileName);
        bool allPeriods = true;

        for (int i = 0; i < fileNameLength; i++)
        {
            // Check invalid characters
            if ((fileName[i] == '<') ||
                (fileName[i] == '>') ||
                (fileName[i] == ':') ||
                (fileName[i] == '\"') ||
                (fileName[i] == '/') ||
                (fileName[i] == '\\') ||
                (fileName[i] == '|') ||
                (fileName[i] == '?') ||
                (fileName[i] == '*')) { valid = false; break; }

            // Check non-glyph characters
            if ((unsigned char)fileName[i] < 32) { valid = false; break; }

            // Check if filename is not all periods
            if (fileName[i] != '.') allPeriods = false;
        }

        if (allPeriods) valid = false;

/*
        if (valid)
        {
            // Check invalid DOS names
            if (fileNameLength >= 3)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'N')) ||   // CON
                    ((fileName[0] == 'P') && (fileName[1] == 'R') && (fileName[2] == 'N')) ||   // PRN
                    ((fileName[0] == 'A') && (fileName[1] == 'U') && (fileName[2] == 'X')) ||   // AUX
                    ((fileName[0] == 'N') && (fileName[1] == 'U') && (fileName[2] == 'L'))) valid = false; // NUL
            }

            if (fileNameLength >= 4)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'M') && ((fileName[3] >= '0') && (fileName[3] <= '9'))) ||  // COM0-9
                    ((fileName[0] == 'L') && (fileName[1] == 'P') && (fileName[2] == 'T') && ((fileName[3] >= '0') && (fileName[3] <= '9')))) valid = false; // LPT0-9
            }
        }
*/
    }

    return valid;
}

// Get the file count in a directory
unsigned int GetDirectoryFileCount(const char *dirPath)
{
    return GetDirectoryFileCountEx(dirPath, FILE_FILTER_TAG_ALL, false);
}

// Get the file count in a directory with extension filtering and recursive directory scan. Use 'FILE_FILTER_TAG_DIR_ONLY' in the filter string to include directories in the result
unsigned int GetDirectoryFileCountEx(const char *basePath, const char *filter, bool scanSubdirs)
{
    unsigned int fileCounter = 0;

    // WARNING: Path can not be static or it will be reused between recursive function calls!
    char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *entity;
    DIR *dir = opendir(basePath);

    if (dir != NULL) // It's a directory
    {
        while ((entity = readdir(dir)) != NULL)
        {
            // NOTE: Skipping '.' (current dir) and '..' (parent dir) filepaths
            if ((strcmp(entity->d_name, ".") != 0) && (strcmp(entity->d_name, "..") != 0))
            {
                // Construct new path from our base path
                #if defined(_WIN32)
                    int pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s\\%s", basePath, entity->d_name);
                #else
                    int pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s/%s", basePath, entity->d_name);
                #endif
                // Don't add to count if path too long
                if ((pathLength < 0) || (pathLength >= MAX_FILEPATH_LENGTH))
                {
                    TRACELOG(LOG_WARNING, "FILEIO: Path longer than %d characters (%s...)", MAX_FILEPATH_LENGTH, basePath);
                }
                else if (IsPathFile(path))
                {
                    if ((filter == NULL) || (strstr(filter, FILE_FILTER_TAG_ALL) != NULL) ||
                        (strstr(filter, FILE_FILTER_TAG_FILE_ONLY) != NULL) || IsFileExtension(path, filter)) fileCounter++;
                }
                else
                {
                    if ((filter != NULL) && ((strstr(filter, FILE_FILTER_TAG_ALL) != NULL) || (strstr(filter, FILE_FILTER_TAG_DIR_ONLY) != NULL))) fileCounter++;
                    if (scanSubdirs) fileCounter += GetDirectoryFileCountEx(path, filter, scanSubdirs);
                }
            }
        }
        closedir(dir);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Directory cannot be opened (%s)", basePath);  // Maybe it's a file...
    return fileCounter;
}

// Scan all files and directories in a base path
// WARNING: files.paths[] must be previously allocated and
// contain enough space to store all required paths
static void ScanDirectoryFiles(const char *basePath, FilePathList *files, const char *filter, unsigned int expectedFileCount, bool scanSubdirs)
{
    // WARNING: Path can not be static or it will be reused between recursive function calls!
    char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *dp = NULL;
    DIR *dir = opendir(basePath);

    if (dir != NULL)
    {
        while (((dp = readdir(dir)) != NULL) && (files->count < expectedFileCount))
        {
            if ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..") != 0))
            {
                // Construct new path from our base path
            #if defined(_WIN32)
                int pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s\\%s", basePath, dp->d_name);
            #else
                int pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s/%s", basePath, dp->d_name);
            #endif

                if ((pathLength < 0) || (pathLength >= MAX_FILEPATH_LENGTH))
                {
                    TRACELOG(LOG_WARNING, "FILEIO: Path longer than %d characters (%s...)", MAX_FILEPATH_LENGTH, basePath);
                }
                else if (IsPathFile(path))
                {
                    if ((filter == NULL) || (strstr(filter, FILE_FILTER_TAG_ALL) != NULL) ||
                        (strstr(filter, FILE_FILTER_TAG_FILE_ONLY) != NULL) || IsFileExtension(path, filter))
                    {
                        memcpy(files->paths[files->count], path, pathLength);
                        files->count++;
                    }
                }
                else
                {
                    if ((filter != NULL) && ((strstr(filter, FILE_FILTER_TAG_DIR_ONLY) != NULL) || (strstr(filter, FILE_FILTER_TAG_ALL) != NULL)))
                    {
                        memcpy(files->paths[files->count], path, pathLength);
                        files->count++;
                    }

                    if (scanSubdirs) ScanDirectoryFiles(path, files, filter, expectedFileCount, scanSubdirs);
                }
            }
        }

        closedir(dir);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Directory cannot be opened (%s)", basePath);  // Maybe it's a file...
}