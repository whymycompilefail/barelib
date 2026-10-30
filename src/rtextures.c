/**********************************************************************************************
*
*   rtextures - Basic functions to load and draw textures
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RTEXTURES    1
*           rtextures module is included in the build
*
*       #define SUPPORT_FILEFORMAT_BMP      1
*       #define SUPPORT_FILEFORMAT_PNG      1
*       #define SUPPORT_FILEFORMAT_TGA      0
*       #define SUPPORT_FILEFORMAT_JPG      0
*       #define SUPPORT_FILEFORMAT_GIF      1
*       #define SUPPORT_FILEFORMAT_QOI      1
*       #define SUPPORT_FILEFORMAT_PEP      1
*       #define SUPPORT_FILEFORMAT_PSD      0
*       #define SUPPORT_FILEFORMAT_HDR      0
*       #define SUPPORT_FILEFORMAT_PIC      0
*       #define SUPPORT_FILEFORMAT_PNM      0
*       #define SUPPORT_FILEFORMAT_DDS      1
*       #define SUPPORT_FILEFORMAT_PKM      0
*       #define SUPPORT_FILEFORMAT_KTX      0
*       #define SUPPORT_FILEFORMAT_PVR      0
*       #define SUPPORT_FILEFORMAT_ASTC     0
*           Selected desired fileformats to be supported for image data loading. Some of those formats are
*           supported by default, to remove support, #define as 0 in this module or your build system
*
*       #define SUPPORT_IMAGE_EXPORT        1
*           Support image export in multiple file formats
*
*       #define SUPPORT_IMAGE_GENERATION    1
*           Support procedural image generation functionality (gradient, spot, perlin-noise, cellular)
*
*   DEPENDENCIES:
*       stb_image        - Multiple image formats loading (JPEG, PNG, BMP, TGA, PSD, GIF, PIC)
*                          NOTE: stb_image has been slightly modified
*       stb_image_resize - Multiple image resize algorithms
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
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

#include "raylib.h"             // Declares module functions

#include "config.h"             // Defines module configuration flags

#if SUPPORT_MODULE_RTEXTURES

#include "rlgl.h"               // OpenGL abstraction layer to multiple versions

#include <stdlib.h>             // Required for: malloc(), calloc(), free()
#include <string.h>             // Required for: strlen() [Used in ImageTextEx()], strcmp() [Used in LoadImageFromMemory()/LoadImageAnimFromMemory()/ExportImageToMemory()]
#include <math.h>               // Required for: fabsf() [Used in DrawTextureRec()]
#include <stdio.h>              // Required for: sprintf() [Used in ExportImageAsCode()]

// Support only desired texture formats on stb_image
#if !SUPPORT_FILEFORMAT_BMP
    #define STBI_NO_BMP
#endif
#if !SUPPORT_FILEFORMAT_PNG
    #define STBI_NO_PNG
#endif
#if !SUPPORT_FILEFORMAT_TGA
    #define STBI_NO_TGA
#endif
#if !SUPPORT_FILEFORMAT_JPG
    #define STBI_NO_JPEG        // Image format .jpg and .jpeg
#endif
#if !SUPPORT_FILEFORMAT_PSD
    #define STBI_NO_PSD
#endif
#if !SUPPORT_FILEFORMAT_GIF
    #define STBI_NO_GIF
#endif
#if !SUPPORT_FILEFORMAT_PIC
    #define STBI_NO_PIC
#endif
#if !SUPPORT_FILEFORMAT_HDR
    #define STBI_NO_HDR
#endif
#if !SUPPORT_FILEFORMAT_PNM
    #define STBI_NO_PNM
#endif

#if SUPPORT_FILEFORMAT_DDS
    #define RLTEXGPU_SUPPORT_DDS
#endif
#if SUPPORT_FILEFORMAT_PKM
    #define RLTEXGPU_SUPPORT_PKM
#endif
#if SUPPORT_FILEFORMAT_KTX
    #define RLTEXGPU_SUPPORT_KTX
#endif
#if SUPPORT_FILEFORMAT_PVR
    #define RLTEXGPU_SUPPORT_PVR
#endif
#if SUPPORT_FILEFORMAT_ASTC
    #define RLTEXGPU_SUPPORT_ASTC
#endif

// Image fileformats not supported by default
#if (SUPPORT_FILEFORMAT_BMP || \
     SUPPORT_FILEFORMAT_PNG || \
     SUPPORT_FILEFORMAT_TGA || \
     SUPPORT_FILEFORMAT_JPG || \
     SUPPORT_FILEFORMAT_PSD || \
     SUPPORT_FILEFORMAT_GIF || \
     SUPPORT_FILEFORMAT_HDR || \
     SUPPORT_FILEFORMAT_PIC || \
     SUPPORT_FILEFORMAT_PNM)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define STBI_MALLOC RL_MALLOC
    #define STBI_FREE RL_FREE
    #define STBI_REALLOC RL_REALLOC

    #define STBI_NO_THREAD_LOCALS

    #if defined(__TINYC__)
        #define STBI_NO_SIMD
    #endif

    #define STB_IMAGE_IMPLEMENTATION
    #include "external/stb_image.h"         // Required for: stbi_load_from_file()
                                            // NOTE: Used to read image data (multiple formats support)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

#if (SUPPORT_FILEFORMAT_DDS || \
     SUPPORT_FILEFORMAT_PKM || \
     SUPPORT_FILEFORMAT_KTX || \
     SUPPORT_FILEFORMAT_PVR || \
     SUPPORT_FILEFORMAT_ASTC)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define RLTEXGPU_MALLOC RL_MALLOC
    #define RLTEXGPU_FREE RL_FREE
    #define RLTEXGPU_LOG(...) TRACELOG(LOG_WARNING, "IMAGE: " __VA_ARGS__)
    #define RLTEXGPU_SHOW_LOG_INFO
    #define RLTEXGPU_IMPLEMENTATION
    #include "external/rltexgpu.h"         // Required for: rl_load_xxx_from_memory()
                                           // NOTE: Used to read compressed textures data (multiple formats support)
    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

#if SUPPORT_FILEFORMAT_QOI
    #define QOI_MALLOC RL_MALLOC
    #define QOI_FREE RL_FREE

    #if defined(_MSC_VER)               // Disable some MSVC warning
        #pragma warning(push)
        #pragma warning(disable : 4267)
    #endif

    #define QOI_IMPLEMENTATION
    #include "external/qoi.h"

    #if defined(_MSC_VER)
        #pragma warning(pop)            // Disable MSVC warning suppression
    #endif
#endif

#if SUPPORT_FILEFORMAT_PEP
	#define PEP_MALLOC RL_MALLOC
	#define PEP_REALLOC RL_REALLOC
	#define PEP_FREE RL_FREE

    #define PEP_IMPLEMENTATION
    #include "external/pep.h"
#endif

#if SUPPORT_IMAGE_EXPORT
    #define STBIW_MALLOC RL_MALLOC
    #define STBIW_FREE RL_FREE
    #define STBIW_REALLOC RL_REALLOC

    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "external/stb_image_write.h"   // Required for: stbi_write_*()
#endif

#if SUPPORT_IMAGE_GENERATION
    #define STB_PERLIN_IMPLEMENTATION
    #include "external/stb_perlin.h"        // Required for: stb_perlin_fbm_noise3
#endif

#define STBIR_MALLOC(size,c) ((void)(c), RL_MALLOC(size))
#define STBIR_FREE(ptr,c) ((void)(c), RL_FREE(ptr))

#if defined(__GNUC__) // GCC and Clang
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if defined(__TINYC__)
    #define STBIR_NO_SIMD
#endif
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "external/stb_image_resize2.h"     // Required for: stbir_resize_uint8_linear() [ImageResize()]

#if defined(__GNUC__) // GCC and Clang
    #pragma GCC diagnostic pop
#endif

// Internal image codec abstraction
#include "rtextures_internal.h"

//--------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD
    #define PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD  50    // Threshold over 255 to set alpha as 0
#endif

#ifndef GAUSSIAN_BLUR_ITERATIONS
    #define GAUSSIAN_BLUR_ITERATIONS  4    // Number of box blur iterations to approximate gaussian blur
#endif

#ifndef MIN
    #define MIN(a,b) (((a)<(b))? (a):(b))
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// It's lonely here...

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by text)
//----------------------------------------------------------------------------------
extern void LoadFontDefault(void);          // [Module: text] Loads default font, required by ImageDrawText()

//----------------------------------------------------------------------------------
// Image codec interface implementation
//----------------------------------------------------------------------------------
// Load image data from memory via stb_image (default codec backend)
unsigned char *LoadImageDataFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int *width, int *height, int *channels)
{
#if defined(STBI_REQUIRED)
    return stbi_load_from_memory(fileData, dataSize, width, height, channels, 0);
#else
    return NULL;
#endif
}

// Save image data to memory via stb_image_write (default codec backend)
unsigned char *SaveImageDataToMemory(const char *fileType, const unsigned char *imageData, int width, int height, int channels, int *outputSize)
{
#if defined(SUPPORT_IMAGE_EXPORT)
    // Route to stb_image_write based on fileType
    // Returns NULL for unsupported formats
    return NULL;
#else
    return NULL;
#endif
}

// Check if file type is supported by the image codec
bool IsImageFileTypeSupported(const char *fileType)
{
    if (fileType == NULL) return false;

    if ((false)
#if SUPPORT_FILEFORMAT_PNG
        || (strcmp(fileType, ".png") == 0) || (strcmp(fileType, ".PNG") == 0)
#endif
#if SUPPORT_FILEFORMAT_BMP
        || (strcmp(fileType, ".bmp") == 0) || (strcmp(fileType, ".BMP") == 0)
#endif
#if SUPPORT_FILEFORMAT_TGA
        || (strcmp(fileType, ".tga") == 0) || (strcmp(fileType, ".TGA") == 0)
#endif
#if SUPPORT_FILEFORMAT_JPG
        || (strcmp(fileType, ".jpg") == 0) || (strcmp(fileType, ".jpeg") == 0)
        || (strcmp(fileType, ".JPG") == 0) || (strcmp(fileType, ".JPEG") == 0)
#endif
#if SUPPORT_FILEFORMAT_GIF
        || (strcmp(fileType, ".gif") == 0) || (strcmp(fileType, ".GIF") == 0)
#endif
#if SUPPORT_FILEFORMAT_PIC
        || (strcmp(fileType, ".pic") == 0) || (strcmp(fileType, ".PIC") == 0)
#endif
#if SUPPORT_FILEFORMAT_PNM
        || (strcmp(fileType, ".ppm") == 0) || (strcmp(fileType, ".pgm") == 0)
        || (strcmp(fileType, ".PPM") == 0) || (strcmp(fileType, ".PGM") == 0)
#endif
#if SUPPORT_FILEFORMAT_PSD
        || (strcmp(fileType, ".psd") == 0) || (strcmp(fileType, ".PSD") == 0)
#endif
        ) return true;

    return false;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static float HalfToFloat(unsigned short x);
static unsigned short FloatToHalf(float x);
static Vector4 *LoadImageDataNormalized(Image image);       // Load pixel data from image as Vector4 array (float normalized)

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Load image from file into CPU memory (RAM)
Image LoadImage(const char *fileName)
{
    Image image = { 0 };

#if SUPPORT_FILEFORMAT_PNG || \
    SUPPORT_FILEFORMAT_BMP || \
    SUPPORT_FILEFORMAT_TGA || \
    SUPPORT_FILEFORMAT_JPG || \
    SUPPORT_FILEFORMAT_GIF || \
    SUPPORT_FILEFORMAT_PIC || \
    SUPPORT_FILEFORMAT_HDR || \
    SUPPORT_FILEFORMAT_PNM || \
    SUPPORT_FILEFORMAT_PSD

    #define STBI_REQUIRED
#endif

    // Loading file to memory
    int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);

    // Loading image from memory data
    if (fileData != NULL)
    {
        image = LoadImageFromMemory(GetFileExtension(fileName), fileData, dataSize);

        UnloadFileData(fileData);
    }

    return image;
}

// Load an image from RAW file data
Image LoadImageRaw(const char *fileName, int width, int height, int format, int headerSize)
{
    Image image = { 0 };

    int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);

    if (fileData != NULL)
    {
        unsigned char *dataPtr = fileData;
        int size = GetPixelDataSize(width, height, format);

        if (size <= dataSize)   // Security check
        {
            // Offset file data to expected raw image by header size
            if ((headerSize > 0) && ((headerSize + size) <= dataSize)) dataPtr += headerSize;

            image.data = RL_MALLOC(size);      // Allocate required memory in bytes
            memcpy(image.data, dataPtr, size); // Copy required data to image
            image.width = width;
            image.height = height;
            image.mipmaps = 1;
            image.format = format;
        }

        UnloadFileData(fileData);
    }

    return image;
}

// Load animated image data
//  - Image.data buffer includes all frames: [image#0][image#1][image#2][...]
//  - Number of frames is returned through 'frames' parameter
//  - All frames are returned in RGBA format
//  - Frames delay data is discarded
Image LoadImageAnim(const char *fileName, int *frames)
{
    Image image = { 0 };
    int frameCount = 0;

#if SUPPORT_FILEFORMAT_GIF
    if (IsFileExtension(fileName, ".gif"))
    {
        int dataSize = 0;
        unsigned char *fileData = LoadFileData(fileName, &dataSize);

        if (fileData != NULL)
        {
            int comp = 0;
            int *delays = NULL;
            image.data = stbi_load_gif_from_memory(fileData, dataSize, &delays, &image.width, &image.height, &frameCount, &comp, 4);

            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            UnloadFileData(fileData);
            RL_FREE(delays);        // NOTE: Frames delays are discarded
        }
    }
#else
    if (false) { }
#endif
    else
    {
        image = LoadImage(fileName);
        frameCount = 1;
    }

    *frames = frameCount;
    return image;
}

// Load animated image data from memory
//  - Image.data buffer includes all frames: [image#0][image#1][image#2][...]
//  - Number of frames is returned through 'frames' parameter
//  - All frames are returned in RGBA format
//  - Frames delay data is discarded
Image LoadImageAnimFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int *frames)
{
    Image image = { 0 };
    int frameCount = 0;

    if ((fileType == NULL) || (fileData == NULL) || (dataSize == 0)) return image; // Security check

#if SUPPORT_FILEFORMAT_GIF
    if ((strcmp(fileType, ".gif") == 0) || (strcmp(fileType, ".GIF") == 0))
    {
        if (fileData != NULL)
        {
            int comp = 0;
            int *delays = NULL;
            image.data = stbi_load_gif_from_memory(fileData, dataSize, &delays, &image.width, &image.height, &frameCount, &comp, 4);

            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            RL_FREE(delays);        // NOTE: Frames delays are discarded
        }
    }
#else
    if (false) { }
#endif
    else
    {
        image = LoadImageFromMemory(fileType, fileData, dataSize);
        frameCount = 1;
    }

    *frames = frameCount;
    return image;
}

// Load image from memory buffer, fileType refers to extension: i.e. ".png"
// WARNING: File extension must be provided in lower-case
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize)
{
    Image image = { 0 };

    // Security checks for input data
    if ((fileData == NULL) || (dataSize == 0))
    {
        TRACELOG(LOG_WARNING, "IMAGE: Invalid file data");
        return image;
    }
    if (fileType == NULL)
    {
        TRACELOG(LOG_WARNING, "IMAGE: Missing file extension");
        return image;
    }

    if ((false)
#if SUPPORT_FILEFORMAT_PNG
        || (strcmp(fileType, ".png") == 0) || (strcmp(fileType, ".PNG") == 0)
#endif
#if SUPPORT_FILEFORMAT_BMP
        || (strcmp(fileType, ".bmp") == 0) || (strcmp(fileType, ".BMP") == 0)
#endif
#if SUPPORT_FILEFORMAT_TGA
        || (strcmp(fileType, ".tga") == 0) || (strcmp(fileType, ".TGA") == 0)
#endif
#if SUPPORT_FILEFORMAT_JPG
        || (strcmp(fileType, ".jpg") == 0) || (strcmp(fileType, ".jpeg") == 0)
        || (strcmp(fileType, ".JPG") == 0) || (strcmp(fileType, ".JPEG") == 0)
#endif
#if SUPPORT_FILEFORMAT_GIF
        || (strcmp(fileType, ".gif") == 0) || (strcmp(fileType, ".GIF") == 0)
#endif
#if SUPPORT_FILEFORMAT_PIC
        || (strcmp(fileType, ".pic") == 0) || (strcmp(fileType, ".PIC") == 0)
#endif
#if SUPPORT_FILEFORMAT_PNM
        || (strcmp(fileType, ".ppm") == 0) || (strcmp(fileType, ".pgm") == 0)
        || (strcmp(fileType, ".PPM") == 0) || (strcmp(fileType, ".PGM") == 0)
#endif
#if SUPPORT_FILEFORMAT_PSD
        || (strcmp(fileType, ".psd") == 0) || (strcmp(fileType, ".PSD") == 0)
#endif
        )
    {
#if defined(STBI_REQUIRED)
        // NOTE: Using stb_image to load images (Supports multiple image formats)

        if (fileData != NULL)
        {
            int comp = 0;
            image.data = stbi_load_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            if (image.data != NULL)
            {
                image.mipmaps = 1;

                if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                else if (comp == 2) image.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
                else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            }
        }
#endif
    }
#if SUPPORT_FILEFORMAT_HDR
    else if ((strcmp(fileType, ".hdr") == 0) || (strcmp(fileType, ".HDR") == 0))
    {
#if defined(STBI_REQUIRED)
        if (fileData != NULL)
        {
            int comp = 0;
            image.data = stbi_loadf_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            image.mipmaps = 1;

            if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_R32;
            else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32;
            else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
            else
            {
                TRACELOG(LOG_WARNING, "IMAGE: HDR file format not supported");
                UnloadImage(image);
            }
        }
#endif
    }
#endif
#if SUPPORT_FILEFORMAT_QOI
    else if ((strcmp(fileType, ".qoi") == 0) || (strcmp(fileType, ".QOI") == 0))
    {
        if (fileData != NULL)
        {
            qoi_desc desc = { 0 };
            image.data = qoi_decode(fileData, dataSize, &desc, (int) fileData[12]);
            image.width = desc.width;
            image.height = desc.height;
            image.format = (desc.channels == 4)? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 : PIXELFORMAT_UNCOMPRESSED_R8G8B8;
            image.mipmaps = 1;
        }
    }
#endif
#if SUPPORT_FILEFORMAT_PEP
    else if ((strcmp(fileType, ".pep") == 0) || (strcmp(fileType, ".PEP") == 0))
    {
        if (fileData != NULL)
        {
            // Deserialize file data into pep structure
            pep p = pep_deserialize(fileData, dataSize);

            // Decompress pep data for image struct
            // Decompression options:
            //  - First color transparent, 3rd parameter: 1
            //  - Premultiply alpha on loading, 4th parameter: 1
            unsigned int *pepData = pep_decompress(&p, pep_rgba, 1, 1);

            image.data = pepData;
            image.width = p.width;
            image.height = p.height;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            image.mipmaps = 1;

            PEP_FREE(pepData);
            pep_free(&p);
        }
    }
#endif
#if SUPPORT_FILEFORMAT_DDS
    else if ((strcmp(fileType, ".dds") == 0) || (strcmp(fileType, ".DDS") == 0))
    {
        image.data = rl_load_dds_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_PKM
    else if ((strcmp(fileType, ".pkm") == 0) || (strcmp(fileType, ".PKM") == 0))
    {
        image.data = rl_load_pkm_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_KTX
    else if ((strcmp(fileType, ".ktx") == 0) || (strcmp(fileType, ".KTX") == 0))
    {
        image.data = rl_load_ktx_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_PVR
    else if ((strcmp(fileType, ".pvr") == 0) || (strcmp(fileType, ".PVR") == 0))
    {
        image.data = rl_load_pvr_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_ASTC
    else if ((strcmp(fileType, ".astc") == 0) || (strcmp(fileType, ".ASTC") == 0))
    {
        image.data = rl_load_astc_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
    else TRACELOG(LOG_WARNING, "IMAGE: Data format not supported");

    if (image.data != NULL) TRACELOG(LOG_INFO, "IMAGE: Data loaded successfully (%ix%i | %s | %i mipmaps)", image.width, image.height, rlGetPixelFormatName(image.format), image.mipmaps);
    else TRACELOG(LOG_WARNING, "IMAGE: Failed to load image data");

    return image;
}

// Load image from GPU texture data
// NOTE: Compressed texture formats not supported
Image LoadImageFromTexture(Texture2D texture)
{
    Image image = { 0 };

    if (texture.format < PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        image.data = rlReadTexturePixels(texture.id, texture.width, texture.height, texture.format);

        if (image.data != NULL)
        {
            image.width = texture.width;
            image.height = texture.height;
#if defined(FBO_READ_TEXTURE_AS_RGBA)
            // WARNING: On OpenGL ES 2.0/WebGL 1.0, there is no glGetTexImage() so,
            // texture data is retrieved by creating an RGBA fbo, binding the texture
            // to the fbo color attachment, and reading it as RGBA data with glReadPixels()
            // Returned data *should* be RGBA but it seems on some platforms (RPI, WASM)
            // original texture format is retrieved, so adding a define for this patch
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
#else
            image.format = texture.format;
#endif
            image.mipmaps = 1;

            TRACELOG(LOG_INFO, "TEXTURE: [ID %i] Pixel data retrieved successfully", texture.id);
        }
        else TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] Failed to retrieve pixel data", texture.id);
    }
    else TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] Failed to retrieve compressed pixel data", texture.id);

    return image;
}

// Load image from screen buffer (screenshot)
Image LoadImageFromScreen(void)
{
    Image image = { 0 };

    image.width = (int)(GetRenderWidth());
    image.height = (int)(GetRenderHeight());
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data = rlReadScreenPixels(image.width, image.height);

    return image;
}

// Check if an image is ready
bool IsImageValid(Image image)
{
    bool result = false;

    if ((image.data != NULL) &&     // Validate pixel data available
        (image.width > 0) &&        // Validate image width
        (image.height > 0) &&       // Validate image height
        (image.format > 0) &&       // Validate image format
        (image.mipmaps > 0)) result = true; // Validate image mipmaps (at least 1 for basic mipmap level)

    return result;
}

// Unload image from CPU memory (RAM)
void UnloadImage(Image image)
{
    RL_FREE(image.data);
}

// Export image data to file
// NOTE: File format depends on fileName extension
bool ExportImage(Image image, const char *fileName)
{
    int result = 0;

    if ((image.width == 0) || (image.height == 0) || (image.data == NULL)) return result; // Security check

#if SUPPORT_IMAGE_EXPORT
    int channels = 4;
    bool allocatedData = false;
    unsigned char *imgData = (unsigned char *)image.data;

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) channels = 1;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) channels = 2;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
    else
    {
        // NOTE: Getting Color array as RGBA unsigned char values
        imgData = (unsigned char *)LoadImageColors(image);
        allocatedData = true;
    }

    if (false) { } // Required to attach following 'else' cases
#if SUPPORT_FILEFORMAT_PNG
    else if (IsFileExtension(fileName, ".png"))
    {
        int dataSize = 0;
        unsigned char *fileData = stbi_write_png_to_mem((const unsigned char *)imgData, image.width*channels, image.width, image.height, channels, &dataSize);
        result = SaveFileData(fileName, fileData, dataSize);
        RL_FREE(fileData);
    }
#endif
#if SUPPORT_FILEFORMAT_BMP
    else if (IsFileExtension(fileName, ".bmp")) result = stbi_write_bmp(fileName, image.width, image.height, channels, imgData);
#endif
#if SUPPORT_FILEFORMAT_TGA
    else if (IsFileExtension(fileName, ".tga")) result = stbi_write_tga(fileName, image.width, image.height, channels, imgData);
#endif
#if SUPPORT_FILEFORMAT_JPG
    else if (IsFileExtension(fileName, ".jpg") ||
             IsFileExtension(fileName, ".jpeg")) result = stbi_write_jpg(fileName, image.width, image.height, channels, imgData, 90);  // JPG quality: between 1 and 100
#endif
#if SUPPORT_FILEFORMAT_QOI
    else if (IsFileExtension(fileName, ".qoi"))
    {
        channels = 0;
        if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
        else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
        else TRACELOG(LOG_WARNING, "IMAGE: Image pixel format must be R8G8B8 or R8G8B8A8");

        if ((channels == 3) || (channels == 4))
        {
            qoi_desc desc = { 0 };
            desc.width = image.width;
            desc.height = image.height;
            desc.channels = channels;
            desc.colorspace = QOI_SRGB;

            result = qoi_write(fileName, imgData, &desc);
        }
    }
#endif
#if SUPPORT_FILEFORMAT_PEP
    else if (IsFileExtension(fileName, ".pep"))
    {
        if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        {
            // NOTE: Only RGBA 32bit (8bit per channel) image format supported
            pep p = pep_compress(image.data, image.width, image.height, pep_rgba, pep_8bit);

            unsigned int pepDataSize = 0;
            unsigned char *pepData = pep_serialize(&p, &pepDataSize);

            result = SaveFileData(fileName, pepData, pepDataSize);

            PEP_FREE(pepData);
            pep_free(&p);
        }
        else TRACELOG(LOG_WARNING, "IMAGE: Image can not be exported, .pep requires RGBA 32bit input");
    }
#endif
#if SUPPORT_FILEFORMAT_KTX
    else if (IsFileExtension(fileName, ".ktx"))
    {
        result = rl_save_ktx(fileName, image.data, image.width, image.height, image.format, image.mipmaps);
    }
#endif
    else if (IsFileExtension(fileName, ".raw"))
    {
        // Export raw pixel data (without header)
        // NOTE: It's up to the user to track image parameters
        result = SaveFileData(fileName, image.data, GetPixelDataSize(image.width, image.height, image.format));
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Export image format requested not supported");

    if (allocatedData) RL_FREE(imgData);
#endif // SUPPORT_IMAGE_EXPORT

    if (result != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Image exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export image", fileName);

    return result;
}

// Export image to memory buffer
unsigned char *ExportImageToMemory(Image image, const char *fileType, int *dataSize)
{
    unsigned char *fileData = NULL;
    *dataSize = 0;

    if ((image.width == 0) || (image.height == 0) || (image.data == NULL)) return fileData; // Security check

    int channels = 4;

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) channels = 1;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) channels = 2;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;

#if SUPPORT_IMAGE_EXPORT
    if ((strcmp(fileType, ".png") == 0) || (strcmp(fileType, ".PNG") == 0))
    {
        fileData = stbi_write_png_to_mem((const unsigned char *)image.data, image.width*channels, image.width, image.height, channels, dataSize);
    }
#else
    TRACELOG(LOG_WARNING, "IMAGE: To export image, enable flag SUPPORT_IMAGE_EXPORT");
#endif

    return fileData;
}

// Export image as code file (.h) defining an array of bytes
bool ExportImageAsCode(Image image, const char *fileName)
{
    bool result = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    int dataSize = GetPixelDataSize(image.width, image.height, image.format);

    // NOTE: Text data buffer size is estimated considering image data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    char *txtData = (char *)RL_CALLOC(dataSize*6 + 2000, sizeof(char));

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// ImageAsCode exporter v1.0 - Image pixel data exported as an array of bytes         //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2026 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path and convert variable name to uppercase
    char varFileName[256] = { 0 };
    strncpy(varFileName, GetFileNameWithoutExt(fileName), 256 - 1); // NOTE: Using function provided by [rcore] module
    for (int i = 0; varFileName[i] != '\0'; i++) if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = varFileName[i] - 32; }

    // Add image information
    byteCount += sprintf(txtData + byteCount, "// Image data information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_WIDTH    %i\n", varFileName, image.width);
    byteCount += sprintf(txtData + byteCount, "#define %s_HEIGHT   %i\n", varFileName, image.height);
    byteCount += sprintf(txtData + byteCount, "#define %s_FORMAT   %i          // raylib internal pixel format\n\n", varFileName, image.format);

    byteCount += sprintf(txtData + byteCount, "static unsigned char %s_DATA[%i] = { ", varFileName, dataSize);
    for (int i = 0; i < dataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), ((unsigned char *)image.data)[i]);
    byteCount += sprintf(txtData + byteCount, "0x%x };\n", ((unsigned char *)image.data)[dataSize - 1]);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    RL_FREE(txtData);

    if (result != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Image as code exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export image as code", fileName);

    return result;
}

//------------------------------------------------------------------------------------
// Image generation functions
//------------------------------------------------------------------------------------
// Generate image: plain color
Image GenImageColor(int width, int height, Color color)
{
    Color *pixels = (Color *)RL_CALLOC(width*height, sizeof(Color));

    for (int i = 0; i < width*height; i++) pixels[i] = color;

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

//------------------------------------------------------------------------------------
// Image manipulation functions
//------------------------------------------------------------------------------------
// Copy an image to a new image
Image ImageCopy(Image image)
{
    Image newImage = { 0 };

    int width = image.width;
    int height = image.height;
    int size = 0;

    for (int i = 0; i < image.mipmaps; i++)
    {
        size += GetPixelDataSize(width, height, image.format);

        width /= 2;
        height /= 2;

        // Security check for NPOT textures
        if (width < 1) width = 1;
        if (height < 1) height = 1;
    }

    newImage.data = RL_CALLOC(size, 1);

    if (newImage.data != NULL)
    {
        // NOTE: Size must be provided in bytes
        memcpy(newImage.data, image.data, size);

        newImage.width = image.width;
        newImage.height = image.height;
        newImage.mipmaps = image.mipmaps;
        newImage.format = image.format;
    }

    return newImage;
}

// Create an image from another image piece
Image ImageFromImage(Image image, Rectangle rec)
{
    Image result = { 0 };

    int bytesPerPixel = GetPixelDataSize(1, 1, image.format);

    result.width = (int)rec.width;
    result.height = (int)rec.height;
    result.data = RL_CALLOC((int)rec.width*(int)rec.height*bytesPerPixel, 1);
    result.format = image.format;
    result.mipmaps = 1;

    for (int y = 0; y < (int)rec.height; y++)
    {
        memcpy(((unsigned char *)result.data) + y*(int)rec.width*bytesPerPixel, ((unsigned char *)image.data) + ((y + (int)rec.y)*image.width + (int)rec.x)*bytesPerPixel, (int)rec.width*bytesPerPixel);
    }

    return result;
}

// Crop an image to area defined by a rectangle
// NOTE: Security checks are performed in case rectangle goes out of bounds
void ImageCrop(Image *image, Rectangle crop)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    // Security checks to validate crop rectangle
    if (crop.x < 0) { crop.width += crop.x; crop.x = 0; }
    if (crop.y < 0) { crop.height += crop.y; crop.y = 0; }
    if ((crop.x + crop.width) > image->width) crop.width = image->width - crop.x;
    if ((crop.y + crop.height) > image->height) crop.height = image->height - crop.y;
    if ((crop.x > image->width) || (crop.y > image->height))
    {
        TRACELOG(LOG_WARNING, "IMAGE: Failed to crop, rectangle out of bounds");
        return;
    }

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);

        unsigned char *croppedData = (unsigned char *)RL_MALLOC((int)(crop.width*crop.height)*bytesPerPixel);

        // OPTION 1: Move cropped data line-by-line
        for (int y = (int)crop.y, offsetSize = 0; y < (int)(crop.y + crop.height); y++)
        {
            memcpy(croppedData + offsetSize, ((unsigned char *)image->data) + (y*image->width + (int)crop.x)*bytesPerPixel, (int)crop.width*bytesPerPixel);
            offsetSize += ((int)crop.width*bytesPerPixel);
        }

        /*
        // OPTION 2: Move cropped data pixel-by-pixel or byte-by-byte
        for (int y = (int)crop.y; y < (int)(crop.y + crop.height); y++)
        {
            for (int x = (int)crop.x; x < (int)(crop.x + crop.width); x++)
            {
                //memcpy(croppedData + ((y - (int)crop.y)*(int)crop.width + (x - (int)crop.x))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) croppedData[((y - (int)crop.y)*(int)crop.width + (x - (int)crop.x))*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + x)*bytesPerPixel + i];
            }
        }
        */

        RL_FREE(image->data);
        image->data = croppedData;
        image->width = (int)crop.width;
        image->height = (int)crop.height;
    }
}

// Convert image data to desired format
void ImageFormat(Image *image, int newFormat)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if ((newFormat != 0) && (image->format != newFormat))
    {
        if ((image->format < PIXELFORMAT_COMPRESSED_DXT1_RGB) && (newFormat < PIXELFORMAT_COMPRESSED_DXT1_RGB))
        {
            Vector4 *pixels = LoadImageDataNormalized(*image);     // Supports 8 to 32 bit per channel

            RL_FREE(image->data);      // WARNING! Loosing mipmaps data --> Regenerated at the end
            image->data = NULL;
            image->format = newFormat;

            switch (image->format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*sizeof(unsigned char));

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)((pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f)*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*2*sizeof(unsigned char));

                    for (int i = 0, k = 0; i < image->width*image->height*2; i += 2, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)((pixels[k].x*0.299f + (float)pixels[k].y*0.587f + (float)pixels[k].z*0.114f)*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].w*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*31.0f));
                        g = (unsigned char)(round(pixels[i].y*63.0f));
                        b = (unsigned char)(round(pixels[i].z*31.0f));

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 5 | (unsigned short)b;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*3*sizeof(unsigned char));

                    for (int i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)(pixels[k].x*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].y*255.0f);
                        ((unsigned char *)image->data)[i + 2] = (unsigned char)(pixels[k].z*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;
                    unsigned char a = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*31.0f));
                        g = (unsigned char)(round(pixels[i].y*31.0f));
                        b = (unsigned char)(round(pixels[i].z*31.0f));
                        a = (pixels[i].w > ((float)PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD/255.0f))? 1 : 0;

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;
                    unsigned char a = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*15.0f));
                        g = (unsigned char)(round(pixels[i].y*15.0f));
                        b = (unsigned char)(round(pixels[i].z*15.0f));
                        a = (unsigned char)(round(pixels[i].w*15.0f));

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*4*sizeof(unsigned char));

                    for (int i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)(pixels[k].x*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].y*255.0f);
                        ((unsigned char *)image->data)[i + 2] = (unsigned char)(pixels[k].z*255.0f);
                        ((unsigned char *)image->data)[i + 3] = (unsigned char)(pixels[k].w*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    // WARNING: Image is converted to GRAYSCALE equivalent 32bit

                    image->data = (float *)RL_MALLOC(image->width*image->height*sizeof(float));

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        ((float *)image->data)[i] = (float)(pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    image->data = (float *)RL_MALLOC(image->width*image->height*3*sizeof(float));

                    for (int i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        ((float *)image->data)[i] = pixels[k].x;
                        ((float *)image->data)[i + 1] = pixels[k].y;
                        ((float *)image->data)[i + 2] = pixels[k].z;
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    image->data = (float *)RL_MALLOC(image->width*image->height*4*sizeof(float));

                    for (int i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        ((float *)image->data)[i] = pixels[k].x;
                        ((float *)image->data)[i + 1] = pixels[k].y;
                        ((float *)image->data)[i + 2] = pixels[k].z;
                        ((float *)image->data)[i + 3] = pixels[k].w;
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    // WARNING: Image is converted to GRAYSCALE equivalent 16bit

                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        ((unsigned short *)image->data)[i] = FloatToHalf((float)(pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f));
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*3*sizeof(unsigned short));

                    for (int i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        ((unsigned short *)image->data)[i] = FloatToHalf(pixels[k].x);
                        ((unsigned short *)image->data)[i + 1] = FloatToHalf(pixels[k].y);
                        ((unsigned short *)image->data)[i + 2] = FloatToHalf(pixels[k].z);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*4*sizeof(unsigned short));

                    for (int i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        ((unsigned short *)image->data)[i] = FloatToHalf(pixels[k].x);
                        ((unsigned short *)image->data)[i + 1] = FloatToHalf(pixels[k].y);
                        ((unsigned short *)image->data)[i + 2] = FloatToHalf(pixels[k].z);
                        ((unsigned short *)image->data)[i + 3] = FloatToHalf(pixels[k].w);
                    }
                } break;
                default: break;
            }

            RL_FREE(pixels);
            pixels = NULL;

            // In case original image had mipmaps, generate mipmaps for formatted image
            // NOTE: Original mipmaps are replaced by new ones, if custom mipmaps were used, they are lost
            if (image->mipmaps > 1)
            {
                image->mipmaps = 1;
                if (image->data != NULL) ImageMipmaps(image);
            }
        }
        else TRACELOG(LOG_WARNING, "IMAGE: Data format is compressed, can not be converted");
    }
}


// Create an image from a selected channel of another image
Image ImageFromChannel(Image image, int selectedChannel)
{
    Image result = { 0 };

    if ((image.data == NULL) || (image.width == 0) || (image.height == 0)) return result; // Security check

    // Check selected channel is valid
    if (selectedChannel < 0)
    {
        TRACELOG(LOG_WARNING, "Channel cannot be negative. Setting channel to 0.");
        selectedChannel = 0;
    }

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE ||
        image.format == PIXELFORMAT_UNCOMPRESSED_R32 ||
        image.format == PIXELFORMAT_UNCOMPRESSED_R16)
    {
        if (selectedChannel > 0)
        {
            TRACELOG(LOG_WARNING, "This image has only 1 channel. Setting channel to it.");
            selectedChannel = 0;
        }
    }
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
    {
        if (selectedChannel > 1)
        {
            TRACELOG(LOG_WARNING, "This image has only 2 channels. Setting channel to alpha.");
            selectedChannel = 1;
        }
    }
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R5G6B5 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16)
    {
        if (selectedChannel > 2)
        {
            TRACELOG(LOG_WARNING, "This image has only 3 channels. Setting channel to red.");
            selectedChannel = 0;
        }
    }

    // Check for RGBA formats
    if (selectedChannel > 3)
    {
        TRACELOG(LOG_WARNING, "ImageFromChannel supports channels 0 to 3 (RGBA). Setting channel to alpha.");
        selectedChannel = 3;
    }

    // TODO: Consider other one-channel formats: R16, R32
    result.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    result.height = image.height;
    result.width = image.width;
    result.mipmaps = 1;

    unsigned char *pixels = (unsigned char *)RL_CALLOC(image.width*image.height, sizeof(unsigned char)); // Values from 0 to 255

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        for (int i = 0, k = 0; i < image.width*image.height; i++)
        {
            float pixelValue = -1;
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixelValue = (float)((unsigned char *)image.data)[i + selectedChannel]/255.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixelValue = (float)((unsigned char *)image.data)[k + selectedChannel]/255.0f;
                    k += 2;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    if (selectedChannel == 0) pixelValue = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    else if (selectedChannel == 1) pixelValue = (float)((pixel & 0b0000011111000000) >> 6)*(1.0f/31);
                    else if (selectedChannel == 2) pixelValue = (float)((pixel & 0b0000000000111110) >> 1)*(1.0f/31);
                    else if (selectedChannel == 3) pixelValue = ((pixel & 0b0000000000000001) == 0)? 0.0f : 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    if (selectedChannel == 0) pixelValue = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    else if (selectedChannel == 1) pixelValue = (float)((pixel & 0b0000011111100000) >> 5)*(1.0f/63);
                    else if (selectedChannel == 2) pixelValue = (float)(pixel & 0b0000000000011111)*(1.0f/31);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    if (selectedChannel == 0) pixelValue = (float)((pixel & 0b1111000000000000) >> 12)*(1.0f/15);
                    else if (selectedChannel == 1) pixelValue = (float)((pixel & 0b0000111100000000) >> 8)*(1.0f/15);
                    else if (selectedChannel == 2) pixelValue = (float)((pixel & 0b0000000011110000) >> 4)*(1.0f/15);
                    else if (selectedChannel == 3) pixelValue = (float)(pixel & 0b0000000000001111)*(1.0f/15);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixelValue = (float)((unsigned char *)image.data)[k + selectedChannel]/255.0f;
                    k += 4;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixelValue = (float)((unsigned char *)image.data)[k + selectedChannel]/255.0f;
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixelValue = ((float *)image.data)[k];
                    k += 1;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixelValue = ((float *)image.data)[k + selectedChannel];
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixelValue = ((float *)image.data)[k + selectedChannel];
                    k += 4;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixelValue = HalfToFloat(((unsigned short *)image.data)[k]);
                    k += 1;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixelValue = HalfToFloat(((unsigned short *)image.data)[k+selectedChannel]);
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixelValue = HalfToFloat(((unsigned short *)image.data)[k + selectedChannel]);
                    k += 4;

                } break;
                default: break;
            }

            pixels[i] = (unsigned char)(pixelValue*255);
        }
    }

    result.data = pixels;

    return result;
}

// Resize and image to new size using Nearest-Neighbor scaling algorithm
void ImageResizeNN(Image *image, int newWidth, int newHeight)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);
    Color *output = (Color *)RL_MALLOC(newWidth*newHeight*sizeof(Color));

    // EDIT: added +1 to account for an early rounding problem
    int xRatio = (int)((image->width << 16)/newWidth) + 1;
    int yRatio = (int)((image->height << 16)/newHeight) + 1;

    int x2 = 0;
    int y2 = 0;
    for (int y = 0; y < newHeight; y++)
    {
        for (int x = 0; x < newWidth; x++)
        {
            x2 = ((x*xRatio) >> 16);
            y2 = ((y*yRatio) >> 16);

            output[(y*newWidth) + x] = pixels[(y2*image->width) + x2] ;
        }
    }

    int format = image->format;

    RL_FREE(image->data);

    image->data = output;
    image->width = newWidth;
    image->height = newHeight;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);  // Reformat 32bit RGBA image to original format

    UnloadImageColors(pixels);
}

// Resize and image to new size
// NOTE: Uses stb default scaling filters (both bicubic):
// STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_CATMULLROM
// STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_MITCHELL   (high-quality Catmull-Rom)
void ImageResize(Image *image, int newWidth, int newHeight)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    // Check if fast path can be used on image scaling
    // It can be for 8 bit per channel images with 1 to 4 channels per pixel
    if ((image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8))
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *output = (unsigned char *)RL_MALLOC(newWidth*newHeight*bytesPerPixel);

        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: stbir_resize_uint8_linear((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, (stbir_pixel_layout)1); break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: stbir_resize_uint8_linear((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, (stbir_pixel_layout)2); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8: stbir_resize_uint8_linear((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, (stbir_pixel_layout)3); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: stbir_resize_uint8_linear((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, (stbir_pixel_layout)4); break;
            default: break;
        }

        RL_FREE(image->data);
        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
    }
    else
    {
        // Get data as Color pixels array to work with it
        Color *pixels = LoadImageColors(*image);
        Color *output = (Color *)RL_MALLOC(newWidth*newHeight*sizeof(Color));

        // NOTE: Color data is cast to (unsigned char *), there shouldn't been any problem...
        stbir_resize_uint8_linear((unsigned char *)pixels, image->width, image->height, 0, (unsigned char *)output, newWidth, newHeight, 0, (stbir_pixel_layout)4);

        int format = image->format;

        UnloadImageColors(pixels);
        RL_FREE(image->data);

        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
        image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        ImageFormat(image, format);  // Reformat 32bit RGBA image to original format
    }
}

// Resize canvas and fill with color
// NOTE: Resize offset is relative to the top-left corner of the original image
void ImageResizeCanvas(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else if ((newWidth != image->width) || (newHeight != image->height))
    {
        Rectangle srcRec = { 0, 0, (float)image->width, (float)image->height };
        Vector2 dstPos = { (float)offsetX, (float)offsetY };

        if (offsetX < 0)
        {
            srcRec.x = (float)-offsetX;
            srcRec.width += (float)offsetX;
            dstPos.x = 0;
        }
        else if ((offsetX + image->width) > newWidth) srcRec.width = (float)(newWidth - offsetX);

        if (offsetY < 0)
        {
            srcRec.y = (float)-offsetY;
            srcRec.height += (float)offsetY;
            dstPos.y = 0;
        }
        else if ((offsetY + image->height) > newHeight) srcRec.height = (float)(newHeight - offsetY);

        if (newWidth < srcRec.width) srcRec.width = (float)newWidth;
        if (newHeight < srcRec.height) srcRec.height = (float)newHeight;

        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *resizedData = (unsigned char *)RL_CALLOC(newWidth*newHeight*bytesPerPixel, 1);

        // Fill resized canvas with fill color
        // Set first pixel with image->format
        SetPixelColor(resizedData, fill, image->format);

        // Fill remaining bytes of first row
        for (int x = 1; x < newWidth; x++)
        {
            memcpy(resizedData + x*bytesPerPixel, resizedData, bytesPerPixel);
        }
        // Copy the first row into the other rows
        for (int y = 1; y < newHeight; y++)
        {
            memcpy(resizedData + y*newWidth*bytesPerPixel, resizedData, newWidth*bytesPerPixel);
        }

        // Copy old image to resized canvas
        int dstOffsetSize = ((int)dstPos.y*newWidth + (int)dstPos.x)*bytesPerPixel;

        for (int y = 0; y < (int)srcRec.height; y++)
        {
            memcpy(resizedData + dstOffsetSize, ((unsigned char *)image->data) + ((y + (int)srcRec.y)*image->width + (int)srcRec.x)*bytesPerPixel, (int)srcRec.width*bytesPerPixel);
            dstOffsetSize += (newWidth*bytesPerPixel);
        }

        RL_FREE(image->data);
        image->data = resizedData;
        image->width = newWidth;
        image->height = newHeight;
    }
}


// Crop image depending on alpha value
// NOTE: Threshold is defined as a percentage: 0.0f -> 1.0f
void ImageAlphaCrop(Image *image, float threshold)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Rectangle crop = GetImageAlphaBorder(*image, threshold);

    // Crop if rectangle is valid
    if (((int)crop.width != 0) && ((int)crop.height != 0)) ImageCrop(image, crop);
}

// Clear alpha channel to desired color
// NOTE: Threshold defines the alpha limit, 0.0f to 1.0f
void ImageAlphaClear(Image *image, Color color, float threshold)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*255.0f);
                for (int i = 1; i < image->width*image->height*2; i += 2)
                {
                    if (((unsigned char *)image->data)[i] <= thresholdValue)
                    {
                        ((unsigned char *)image->data)[i - 1] = color.r;
                        ((unsigned char *)image->data)[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                unsigned char thresholdValue = ((threshold < 0.5f)? 0 : 1);

                unsigned char r = (unsigned char)(round((float)color.r*31.0f));
                unsigned char g = (unsigned char)(round((float)color.g*31.0f));
                unsigned char b = (unsigned char)(round((float)color.b*31.0f));
                unsigned char a = (color.a < 128)? 0 : 1;

                for (int i = 0; i < image->width*image->height; i++)
                {
                    if ((((unsigned short *)image->data)[i] & 0b0000000000000001) <= thresholdValue)
                    {
                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*15.0f);

                unsigned char r = (unsigned char)(round((float)color.r*15.0f));
                unsigned char g = (unsigned char)(round((float)color.g*15.0f));
                unsigned char b = (unsigned char)(round((float)color.b*15.0f));
                unsigned char a = (unsigned char)(round((float)color.a*15.0f));

                for (int i = 0; i < image->width*image->height; i++)
                {
                    if ((((unsigned short *)image->data)[i] & 0x000f) <= thresholdValue)
                    {
                        ((unsigned short *)image->data)[i] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*255.0f);
                for (int i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (((unsigned char *)image->data)[i] <= thresholdValue)
                    {
                        ((unsigned char *)image->data)[i - 3] = color.r;
                        ((unsigned char *)image->data)[i - 2] = color.g;
                        ((unsigned char *)image->data)[i - 1] = color.b;
                        ((unsigned char *)image->data)[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                for (int i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (((float *)image->data)[i] <= threshold)
                    {
                        ((float *)image->data)[i - 3] = (float)color.r/255.0f;
                        ((float *)image->data)[i - 2] = (float)color.g/255.0f;
                        ((float *)image->data)[i - 1] = (float)color.b/255.0f;
                        ((float *)image->data)[i] = (float)color.a/255.0f;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
            {
                for (int i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (HalfToFloat(((unsigned short *)image->data)[i]) <= threshold)
                    {
                        ((unsigned short *)image->data)[i - 3] = FloatToHalf((float)color.r/255.0f);
                        ((unsigned short *)image->data)[i - 2] = FloatToHalf((float)color.g/255.0f);
                        ((unsigned short *)image->data)[i - 1] = FloatToHalf((float)color.b/255.0f);
                        ((unsigned short *)image->data)[i] = FloatToHalf((float)color.a/255.0f);
                    }
                }
            } break;
            default: break;
        }
    }
}

// Apply alpha mask to image
// NOTE 1: Returned image is GRAY_ALPHA (16bit) or RGBA (32bit)
// NOTE 2: alphaMask should be same size as image
void ImageAlphaMask(Image *image, Image alphaMask)
{
    if ((image->width != alphaMask.width) || (image->height != alphaMask.height))
    {
        TRACELOG(LOG_WARNING, "IMAGE: Alpha mask must be same size as image");
    }
    else if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        TRACELOG(LOG_WARNING, "IMAGE: Alpha mask can not be applied to compressed data formats");
    }
    else
    {
        // Force mask to be Grayscale
        Image mask = ImageCopy(alphaMask);
        if (mask.format != PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

        // In case image is only grayscale, add alpha channel
        if (image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
        {
            unsigned char *data = (unsigned char *)RL_MALLOC(image->width*image->height*2);

            // Apply alpha mask to alpha channel
            for (int i = 0, k = 0; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 2)
            {
                data[k] = ((unsigned char *)image->data)[i];
                data[k + 1] = ((unsigned char *)mask.data)[i];
            }

            RL_FREE(image->data);
            image->data = data;
            image->format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        }
        else
        {
            // Convert image to RGBA
            if (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            // Apply alpha mask to alpha channel
            for (int i = 0, k = 3; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 4)
            {
                ((unsigned char *)image->data)[k] = ((unsigned char *)mask.data)[i];
            }
        }

        UnloadImage(mask);
    }
}

// Premultiply alpha channel
void ImageAlphaPremultiply(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    float alpha = 0.0f;
    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        if (pixels[i].a == 0)
        {
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
        }
        else if (pixels[i].a < 255)
        {
            alpha = (float)pixels[i].a/255.0f;
            pixels[i].r = (unsigned char)((float)pixels[i].r*alpha);
            pixels[i].g = (unsigned char)((float)pixels[i].g*alpha);
            pixels[i].b = (unsigned char)((float)pixels[i].b*alpha);
        }
    }

    RL_FREE(image->data);

    int format = image->format;
    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Apply box blur to image
void ImageBlurGaussian(Image *image, int blurSize)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    ImageAlphaPremultiply(image);

    Color *pixels = LoadImageColors(*image);

    // Loop switches between pixelsCopy1 and pixelsCopy2
    Vector4 *pixelsCopy1 = (Vector4 *)RL_MALLOC((image->height)*(image->width)*sizeof(Vector4));
    Vector4 *pixelsCopy2 = (Vector4 *)RL_MALLOC((image->height)*(image->width)*sizeof(Vector4));

    for (int i = 0; i < (image->height*image->width); i++)
    {
        pixelsCopy1[i].x = pixels[i].r;
        pixelsCopy1[i].y = pixels[i].g;
        pixelsCopy1[i].z = pixels[i].b;
        pixelsCopy1[i].w = pixels[i].a;
    }

    // Repeated convolution of rectangular window signal by itself converges to a gaussian distribution
    for (int j = 0; j < GAUSSIAN_BLUR_ITERATIONS; j++)
    {
        // Horizontal motion blur
        for (int row = 0; row < image->height; row++)
        {
            float avgR = 0.0f;
            float avgG = 0.0f;
            float avgB = 0.0f;
            float avgAlpha = 0.0f;
            int convolutionSize = blurSize;

            for (int i = 0; i < blurSize; i++)
            {
                avgR += pixelsCopy1[row*image->width + i].x;
                avgG += pixelsCopy1[row*image->width + i].y;
                avgB += pixelsCopy1[row*image->width + i].z;
                avgAlpha += pixelsCopy1[row*image->width + i].w;
            }

            for (int x = 0; x < image->width; x++)
            {
                if (x-blurSize-1 >= 0)
                {
                    avgR -= pixelsCopy1[row*image->width + x-blurSize-1].x;
                    avgG -= pixelsCopy1[row*image->width + x-blurSize-1].y;
                    avgB -= pixelsCopy1[row*image->width + x-blurSize-1].z;
                    avgAlpha -= pixelsCopy1[row*image->width + x-blurSize-1].w;
                    convolutionSize--;
                }

                if (x+blurSize < image->width)
                {
                    avgR += pixelsCopy1[row*image->width + x+blurSize].x;
                    avgG += pixelsCopy1[row*image->width + x+blurSize].y;
                    avgB += pixelsCopy1[row*image->width + x+blurSize].z;
                    avgAlpha += pixelsCopy1[row*image->width + x+blurSize].w;
                    convolutionSize++;
                }

                pixelsCopy2[row*image->width + x].x = avgR/convolutionSize;
                pixelsCopy2[row*image->width + x].y = avgG/convolutionSize;
                pixelsCopy2[row*image->width + x].z = avgB/convolutionSize;
                pixelsCopy2[row*image->width + x].w = avgAlpha/convolutionSize;
            }
        }

        // Vertical motion blur
        for (int col = 0; col < image->width; col++)
        {
            float avgR = 0.0f;
            float avgG = 0.0f;
            float avgB = 0.0f;
            float avgAlpha = 0.0f;
            int convolutionSize = blurSize;

            for (int i = 0; i < blurSize; i++)
            {
                avgR += pixelsCopy2[i*image->width + col].x;
                avgG += pixelsCopy2[i*image->width + col].y;
                avgB += pixelsCopy2[i*image->width + col].z;
                avgAlpha += pixelsCopy2[i*image->width + col].w;
            }

            for (int y = 0; y < image->height; y++)
            {
                if (y-blurSize-1 >= 0)
                {
                    avgR -= pixelsCopy2[(y-blurSize-1)*image->width + col].x;
                    avgG -= pixelsCopy2[(y-blurSize-1)*image->width + col].y;
                    avgB -= pixelsCopy2[(y-blurSize-1)*image->width + col].z;
                    avgAlpha -= pixelsCopy2[(y-blurSize-1)*image->width + col].w;
                    convolutionSize--;
                }
                if (y+blurSize < image->height)
                {
                    avgR += pixelsCopy2[(y+blurSize)*image->width + col].x;
                    avgG += pixelsCopy2[(y+blurSize)*image->width + col].y;
                    avgB += pixelsCopy2[(y+blurSize)*image->width + col].z;
                    avgAlpha += pixelsCopy2[(y+blurSize)*image->width + col].w;
                    convolutionSize++;
                }

                pixelsCopy1[y*image->width + col].x = (unsigned char) (avgR/convolutionSize);
                pixelsCopy1[y*image->width + col].y = (unsigned char) (avgG/convolutionSize);
                pixelsCopy1[y*image->width + col].z = (unsigned char) (avgB/convolutionSize);
                pixelsCopy1[y*image->width + col].w = (unsigned char) (avgAlpha/convolutionSize);
            }
        }
    }

    // Reverse premultiply
    for (int i = 0; i < (image->width)*(image->height); i++)
    {
        if (pixelsCopy1[i].w == 0.0f)
        {
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
            pixels[i].a = 0;
        }
        else if (pixelsCopy1[i].w <= 255.0f)
        {
            float alpha = (float)pixelsCopy1[i].w/255.0f;
            pixels[i].r = (unsigned char)fminf((float)pixelsCopy1[i].x/alpha, 255.0);
            pixels[i].g = (unsigned char)fminf((float)pixelsCopy1[i].y/alpha, 255.0);
            pixels[i].b = (unsigned char)fminf((float)pixelsCopy1[i].z/alpha, 255.0);
            pixels[i].a = (unsigned char) pixelsCopy1[i].w;
        }
    }

    int format = image->format;
    RL_FREE(image->data);
    RL_FREE(pixelsCopy1);
    RL_FREE(pixelsCopy2);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}


// Generate all mipmap levels for a provided image
// NOTE 1: Supports POT and NPOT images
// NOTE 2: image.data is scaled to include mipmap levels
// NOTE 3: Mipmaps format is the same as base image
void ImageMipmaps(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    int mipCount = 1;                   // Required mipmap levels count (including base level)
    int mipWidth = image->width;        // Base image width
    int mipHeight = image->height;      // Base image height
    int mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);  // Image data size (in bytes)

    // Count mipmap levels required
    while ((mipWidth != 1) || (mipHeight != 1))
    {
        if (mipWidth != 1) mipWidth /= 2;
        if (mipHeight != 1) mipHeight /= 2;

        // Security check for NPOT textures
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;

        TRACELOG(LOG_DEBUG, "IMAGE: Next mipmap level: %i x %i - current size %i", mipWidth, mipHeight, mipSize);

        mipCount++;
        mipSize += GetPixelDataSize(mipWidth, mipHeight, image->format);       // Add mipmap size (in bytes)
    }

    if (image->mipmaps < mipCount)
    {
        // Create second buffer and copy data manually to it
        void *temp = RL_CALLOC(mipSize, 1);
        memcpy(temp, image->data, GetPixelDataSize(image->width, image->height, image->format));
        RL_FREE(image->data);
        image->data = temp;

        // Pointer to allocated memory point where store next mipmap level data
        unsigned char *nextmip = (unsigned char *)image->data;

        mipWidth = image->width;
        mipHeight = image->height;
        mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);
        Image imCopy = ImageCopy(*image);

        for (int i = 1; i < mipCount; i++)
        {
            nextmip += mipSize;

            mipWidth /= 2;
            mipHeight /= 2;

            // Security check for NPOT textures
            if (mipWidth < 1) mipWidth = 1;
            if (mipHeight < 1) mipHeight = 1;

            mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);

            if (i < image->mipmaps) continue;

            TRACELOG(LOG_DEBUG, "IMAGE: Generating mipmap level: %i (%i x %i) - size: %i - offset: 0x%x", i, mipWidth, mipHeight, mipSize, nextmip);
            ImageResize(&imCopy, mipWidth, mipHeight); // Uses internally Mitchell cubic downscale filter
            memcpy(nextmip, imCopy.data, mipSize);
        }

        UnloadImage(imCopy);

        image->mipmaps = mipCount;
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Mipmaps already available");
}


// Flip image vertically
void ImageFlipVertical(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *flippedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int i = (image->height - 1), offsetSize = 0; i >= 0; i--)
        {
            memcpy(flippedData + offsetSize, ((unsigned char *)image->data) + i*image->width*bytesPerPixel, image->width*bytesPerPixel);
            offsetSize += image->width*bytesPerPixel;
        }

        RL_FREE(image->data);
        image->data = flippedData;
    }
}

// Flip image horizontally
void ImageFlipHorizontal(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *flippedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                // OPTION 1: Move pixels with memcpy()
                //memcpy(flippedData + (y*image->width + x)*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - 1 - x))*bytesPerPixel, bytesPerPixel);

                // OPTION 2: Copy data pixel by pixel
                for (int i = 0; i < bytesPerPixel; i++) flippedData[(y*image->width + x)*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + (image->width - 1 - x))*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = flippedData;

        /*
        // OPTION 3: Faster implementation (specific for 32bit pixels)
        // NOTE: It does not require additional allocations
        uint32_t *ptr = (uint32_t *)image->data;
        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width/2; x++)
            {
                uint32_t backup = ptr[y*image->width + x];
                ptr[y*image->width + x] = ptr[y*image->width + (image->width - 1 - x)];
                ptr[y*image->width + (image->width - 1 - x)] = backup;
            }
        }
        */
    }
}

// Rotate image in degrees
void ImageRotate(Image *image, int degrees)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        float rad = degrees*PI/180.0f;
        float sinRadius = sinf(rad);
        float cosRadius = cosf(rad);

        int width = (int)(fabsf(image->width*cosRadius) + fabsf(image->height*sinRadius));
        int height = (int)(fabsf(image->height*cosRadius) + fabsf(image->width*sinRadius));

        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *rotatedData = (unsigned char *)RL_CALLOC(width*height, bytesPerPixel);

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                float oldX = ((x - width/2.0f)*cosRadius + (y - height/2.0f)*sinRadius) + image->width/2.0f;
                float oldY = ((y - height/2.0f)*cosRadius - (x - width/2.0f)*sinRadius) + image->height/2.0f;

                if ((oldX >= 0) && (oldX < image->width) && (oldY >= 0) && (oldY < image->height))
                {
                    int x1 = (int)floorf(oldX);
                    int y1 = (int)floorf(oldY);
                    int x2 = MIN(x1 + 1, image->width - 1);
                    int y2 = MIN(y1 + 1, image->height - 1);

                    float px = oldX - x1;
                    float py = oldY - y1;

                    for (int i = 0; i < bytesPerPixel; i++)
                    {
                        float f1 = ((unsigned char *)image->data)[(y1*image->width + x1)*bytesPerPixel + i];
                        float f2 = ((unsigned char *)image->data)[(y1*image->width + x2)*bytesPerPixel + i];
                        float f3 = ((unsigned char *)image->data)[(y2*image->width + x1)*bytesPerPixel + i];
                        float f4 = ((unsigned char *)image->data)[(y2*image->width + x2)*bytesPerPixel + i];

                        float val = f1*(1 - px)*(1 - py) + f2*px*(1 - py) + f3*(1 - px)*py + f4*px*py;

                        rotatedData[(y*width + x)*bytesPerPixel + i] = (unsigned char)val;
                    }
                }
            }
        }

        RL_FREE(image->data);
        image->data = rotatedData;
        image->width = width;
        image->height = height;
    }
}

// Rotate image clockwise 90deg
void ImageRotateCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *rotatedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + (image->height - y - 1))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + (image->height - y - 1))*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + x)*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = rotatedData;
        int width = image->width;
        int height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Rotate image counter-clockwise 90deg
void ImageRotateCCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *rotatedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + y))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - x - 1))*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + y)*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + (image->width - x - 1))*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = rotatedData;
        int width = image->width;
        int height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Modify image color: tint
void ImageColorTint(Image *image, Color color)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        unsigned char r = (unsigned char)(((int)pixels[i].r*(int)color.r)/255);
        unsigned char g = (unsigned char)(((int)pixels[i].g*(int)color.g)/255);
        unsigned char b = (unsigned char)(((int)pixels[i].b*(int)color.b)/255);
        unsigned char a = (unsigned char)(((int)pixels[i].a*(int)color.a)/255);

        pixels[i].r = r;
        pixels[i].g = g;
        pixels[i].b = b;
        pixels[i].a = a;
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: invert
void ImageColorInvert(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        pixels[i].r = 255 - pixels[i].r;
        pixels[i].g = 255 - pixels[i].g;
        pixels[i].b = 255 - pixels[i].b;
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: grayscale
void ImageColorGrayscale(Image *image)
{
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
}

// Modify image color: contrast
// NOTE: Contrast values between -100 and 100
void ImageColorContrast(Image *image, int contrast)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (contrast < -100) contrast = -100;
    if (contrast > 100) contrast = 100;

    float factor = (float)(100.0f + contrast)/100.0f;
    factor *= factor;

    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        float pR = (float)pixels[i].r/255.0f;
        pR -= 0.5f;
        pR *= factor;
        pR += 0.5f;
        pR *= 255;
        if (pR < 0) pR = 0;
        if (pR > 255) pR = 255;

        float pG = (float)pixels[i].g/255.0f;
        pG -= 0.5f;
        pG *= factor;
        pG += 0.5f;
        pG *= 255;
        if (pG < 0) pG = 0;
        if (pG > 255) pG = 255;

        float pB = (float)pixels[i].b/255.0f;
        pB -= 0.5f;
        pB *= factor;
        pB += 0.5f;
        pB *= 255;
        if (pB < 0) pB = 0;
        if (pB > 255) pB = 255;

        pixels[i].r = (unsigned char)pR;
        pixels[i].g = (unsigned char)pG;
        pixels[i].b = (unsigned char)pB;
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: brightness
// NOTE: Brightness values between -255 and 255
void ImageColorBrightness(Image *image, int brightness)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (brightness < -255) brightness = -255;
    if (brightness > 255) brightness = 255;

    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        int cR = pixels[i].r + brightness;
        int cG = pixels[i].g + brightness;
        int cB = pixels[i].b + brightness;

        if (cR < 0) cR = 1;
        if (cR > 255) cR = 255;

        if (cG < 0) cG = 1;
        if (cG > 255) cG = 255;

        if (cB < 0) cB = 1;
        if (cB > 255) cB = 255;

        pixels[i].r = (unsigned char)cR;
        pixels[i].g = (unsigned char)cG;
        pixels[i].b = (unsigned char)cB;
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: replace color
void ImageColorReplace(Image *image, Color color, Color replace)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        if ((pixels[i].r == color.r) &&
            (pixels[i].g == color.g) &&
            (pixels[i].b == color.b) &&
            (pixels[i].a == color.a))
        {
            pixels[i].r = replace.r;
            pixels[i].g = replace.g;
            pixels[i].b = replace.b;
            pixels[i].a = replace.a;
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    // Only convert back to original format if it supported alpha
    if ((format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R5G6B5) ||
        (format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R16G16B16) ||
        (format == PIXELFORMAT_COMPRESSED_DXT1_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_ETC1_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_ETC2_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_PVRT_RGB)) ImageFormat(image, format);
}

// Load color data from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImageColors();
Color *LoadImageColors(Image image)
{
    if ((image.width == 0) || (image.height == 0)) return NULL; // Security check

    Color *pixels = (Color *)RL_MALLOC(image.width*image.height*sizeof(Color));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        if ((image.format == PIXELFORMAT_UNCOMPRESSED_R32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)) TRACELOG(LOG_WARNING, "IMAGE: Pixel format converted from 32bit to 8bit per channel");

        if ((image.format == PIXELFORMAT_UNCOMPRESSED_R16) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16)) TRACELOG(LOG_WARNING, "IMAGE: Pixel format converted from 16bit to 8bit per channel");

        for (int i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].r = ((unsigned char *)image.data)[i];
                    pixels[i].g = ((unsigned char *)image.data)[i];
                    pixels[i].b = ((unsigned char *)image.data)[i];
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].r = ((unsigned char *)image.data)[k];
                    pixels[i].g = ((unsigned char *)image.data)[k];
                    pixels[i].b = ((unsigned char *)image.data)[k];
                    pixels[i].a = ((unsigned char *)image.data)[k + 1];

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000011111000000) >> 6)*(255/31));
                    pixels[i].b = (unsigned char)((float)((pixel & 0b0000000000111110) >> 1)*(255/31));
                    pixels[i].a = (unsigned char)((pixel & 0b0000000000000001)*255);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000011111100000) >> 5)*(255/63));
                    pixels[i].b = (unsigned char)((float)(pixel & 0b0000000000011111)*(255/31));
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111000000000000) >> 12)*(255/15));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000111100000000) >> 8)*(255/15));
                    pixels[i].b = (unsigned char)((float)((pixel & 0b0000000011110000) >> 4)*(255/15));
                    pixels[i].a = (unsigned char)((float)(pixel & 0b0000000000001111)*(255/15));

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].r = ((unsigned char *)image.data)[k];
                    pixels[i].g = ((unsigned char *)image.data)[k + 1];
                    pixels[i].b = ((unsigned char *)image.data)[k + 2];
                    pixels[i].a = ((unsigned char *)image.data)[k + 3];

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].r = (unsigned char)((unsigned char *)image.data)[k];
                    pixels[i].g = (unsigned char)((unsigned char *)image.data)[k + 1];
                    pixels[i].b = (unsigned char)((unsigned char *)image.data)[k + 2];
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                    pixels[i].a = 255;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = (unsigned char)(((float *)image.data)[k + 1]*255.0f);
                    pixels[i].b = (unsigned char)(((float *)image.data)[k + 2]*255.0f);
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = (unsigned char)(((float *)image.data)[k + 1]*255.0f);
                    pixels[i].b = (unsigned char)(((float *)image.data)[k + 2]*255.0f);
                    pixels[i].a = (unsigned char)(((float *)image.data)[k + 3]*255.0f);

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixels[i].r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k])*255.0f);
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                    pixels[i].a = 255;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixels[i].r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k])*255.0f);
                    pixels[i].g = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k + 1])*255.0f);
                    pixels[i].b = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k + 2])*255.0f);
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixels[i].r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k])*255.0f);
                    pixels[i].g = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k + 1])*255.0f);
                    pixels[i].b = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k + 2])*255.0f);
                    pixels[i].a = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[k + 3])*255.0f);

                    k += 4;
                } break;
                default: break;
            }
        }
    }

    return pixels;
}

// Load colors palette from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImagePalette()
Color *LoadImagePalette(Image image, int maxPaletteSize, int *colorCount)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    int palCount = 0;
    Color *palette = NULL;
    Color *pixels = LoadImageColors(image);

    if (pixels != NULL)
    {
        palette = (Color *)RL_MALLOC(maxPaletteSize*sizeof(Color));

        for (int i = 0; i < maxPaletteSize; i++) palette[i] = BLANK;   // Set all colors to BLANK

        for (int i = 0; i < image.width*image.height; i++)
        {
            if (pixels[i].a > 0)
            {
                bool colorInPalette = false;

                // Check if the color is already on palette
                for (int j = 0; j < maxPaletteSize; j++)
                {
                    if (COLOR_EQUAL(pixels[i], palette[j]))
                    {
                        colorInPalette = true;
                        break;
                    }
                }

                // Store color if not on the palette
                if (!colorInPalette)
                {
                    palette[palCount] = pixels[i];      // Add pixels[i] to palette
                    palCount++;

                    // Reached the limit of colors supported by palette
                    if (palCount >= maxPaletteSize)
                    {
                        i = image.width*image.height;   // Finish palette get
                        TRACELOG(LOG_WARNING, "IMAGE: Palette is greater than %i colors", maxPaletteSize);
                    }
                }
            }
        }

        UnloadImageColors(pixels);
    }

    *colorCount = palCount;

    return palette;
}

// Unload color data loaded with LoadImageColors()
void UnloadImageColors(Color *colors)
{
    RL_FREE(colors);
}

// Unload colors palette loaded with LoadImagePalette()
void UnloadImagePalette(Color *colors)
{
    RL_FREE(colors);
}

// Get image alpha border rectangle
// NOTE: Threshold is defined as a percentage: 0.0f -> 1.0f
Rectangle GetImageAlphaBorder(Image image, float threshold)
{
    Rectangle crop = { 0 };

    Color *pixels = LoadImageColors(image);

    if (pixels != NULL)
    {
        int xMin = 65536;   // Define a big enough number
        int xMax = 0;
        int yMin = 65536;
        int yMax = 0;

        for (int y = 0; y < image.height; y++)
        {
            for (int x = 0; x < image.width; x++)
            {
                if (pixels[y*image.width + x].a > (unsigned char)(threshold*255.0f))
                {
                    if (x < xMin) xMin = x;
                    if (x > xMax) xMax = x;
                    if (y < yMin) yMin = y;
                    if (y > yMax) yMax = y;
                }
            }
        }

        // Check for empty blank image
        if ((xMin != 65536) && (xMax != 65536))
        {
            crop = (Rectangle){ (float)xMin, (float)yMin, (float)((xMax + 1) - xMin), (float)((yMax + 1) - yMin) };
        }

        UnloadImageColors(pixels);
    }

    return crop;
}

// Get image pixel color at (x, y) position
Color GetImageColor(Image image, int x, int y)
{
    Color color = { 0 };

    if ((x >=0) && (x < image.width) && (y >= 0) && (y < image.height))
    {
        switch (image.format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
            {
                color.r = ((unsigned char *)image.data)[y*image.width + x];
                color.g = ((unsigned char *)image.data)[y*image.width + x];
                color.b = ((unsigned char *)image.data)[y*image.width + x];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                color.r = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.g = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.b = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.a = ((unsigned char *)image.data)[(y*image.width + x)*2 + 1];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                color.g = (unsigned char)((float)((pixel & 0b0000011111000000) >> 6)*(255/31));
                color.b = (unsigned char)((float)((pixel & 0b0000000000111110) >> 1)*(255/31));
                color.a = (unsigned char)((pixel & 0b0000000000000001)*255);

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                color.g = (unsigned char)((float)((pixel & 0b0000011111100000) >> 5)*(255/63));
                color.b = (unsigned char)((float)(pixel & 0b0000000000011111)*(255/31));
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111000000000000) >> 12)*(255/15));
                color.g = (unsigned char)((float)((pixel & 0b0000111100000000) >> 8)*(255/15));
                color.b = (unsigned char)((float)((pixel & 0b0000000011110000) >> 4)*(255/15));
                color.a = (unsigned char)((float)(pixel & 0b0000000000001111)*(255/15));

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                color.r = ((unsigned char *)image.data)[(y*image.width + x)*4];
                color.g = ((unsigned char *)image.data)[(y*image.width + x)*4 + 1];
                color.b = ((unsigned char *)image.data)[(y*image.width + x)*4 + 2];
                color.a = ((unsigned char *)image.data)[(y*image.width + x)*4 + 3];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            {
                color.r = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3];
                color.g = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3 + 1];
                color.b = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3 + 2];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32:
            {
                color.r = (unsigned char)(((float *)image.data)[y*image.width + x]*255.0f);
                color.g = 0;
                color.b = 0;
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
            {
                color.r = (unsigned char)(((float *)image.data)[(y*image.width + x)*3]*255.0f);
                color.g = (unsigned char)(((float *)image.data)[(y*image.width + x)*3 + 1]*255.0f);
                color.b = (unsigned char)(((float *)image.data)[(y*image.width + x)*3 + 2]*255.0f);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                color.r = (unsigned char)(((float *)image.data)[(y*image.width + x)*4]*255.0f);
                color.g = (unsigned char)(((float *)image.data)[(y*image.width + x)*4 + 1]*255.0f);
                color.b = (unsigned char)(((float *)image.data)[(y*image.width + x)*4 + 2]*255.0f);
                color.a = (unsigned char)(((float *)image.data)[(y*image.width + x)*4 + 3]*255.0f);

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16:
            {
                color.r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[y*image.width + x])*255.0f);
                color.g = 0;
                color.b = 0;
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
            {
                color.r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*3])*255.0f);
                color.g = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*3 + 1])*255.0f);
                color.b = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*3 + 2])*255.0f);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
            {
                color.r = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*4])*255.0f);
                color.g = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*4 + 1])*255.0f);
                color.b = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*4 + 2])*255.0f);
                color.a = (unsigned char)(HalfToFloat(((unsigned short *)image.data)[(y*image.width + x)*4 + 3])*255.0f);

            } break;
            default: TRACELOG(LOG_WARNING, "Compressed image format does not support color reading"); break;
        }
    }
    else TRACELOG(LOG_WARNING, "Requested image pixel (%i, %i) out of bounds", x, y);

    return color;
}

//------------------------------------------------------------------------------------
// Image drawing functions
//------------------------------------------------------------------------------------

// Draw a pixel within an image
static void ImageDrawPixel(Image *dst, int x, int y, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    // Check if the pixel is within the image bounds
    if ((x < 0) || (x >= dst->width) || (y < 0) || (y >= dst->height)) return;

    // Set pixel color based on format
    SetPixelColor((unsigned char *)dst->data + (y*dst->width + x)*GetPixelDataSize(1, 1, dst->format), color, dst->format);
}

// Draw a line within an image (Bresenham's algorithm)
static void ImageDrawLine(Image *dst, int startPosX, int startPosY, int endPosX, int endPosY, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    int dx = abs(endPosX - startPosX);
    int dy = abs(endPosY - startPosY);

    int sx = (startPosX < endPosX) ? 1 : -1;
    int sy = (startPosY < endPosY) ? 1 : -1;

    int err = dx - dy;

    int x = startPosX;
    int y = startPosY;

    while (1)
    {
        ImageDrawPixel(dst, x, y, color);

        if ((x == endPosX) && (y == endPosY)) break;

        int e2 = 2*err;

        if (e2 > -dy)
        {
            err -= dy;
            x += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

// Clear image background with given color
void ImageClearBackground(Image *dst, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    // Fill in first pixel based on image format
    ImageDrawPixel(dst, 0, 0, color);

    unsigned char *pSrcPixel = (unsigned char *)dst->data;
    int bytesPerPixel = GetPixelDataSize(1, 1, dst->format);
    int totalPixels = dst->width*dst->height;

    // Repeat the first pixel data throughout the image,
    // doubling the pixels copied on each iteration
    for (int i = 1; i < totalPixels; i *= 2)
    {
        int pixelsToCopy = MIN(i, totalPixels - i);
        memcpy(pSrcPixel + i*bytesPerPixel, pSrcPixel, pixelsToCopy*bytesPerPixel);
    }
}

// Draw triangle within an image
void ImageDrawTriangle(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    // Calculate the 2D bounding box of the triangle
    // Determine the minimum and maximum x and y coordinates of the triangle vertices
    int xMin = (int)((v1.x < v2.x)? ((v1.x < v3.x)? v1.x : v3.x) : ((v2.x < v3.x)? v2.x : v3.x));
    int yMin = (int)((v1.y < v2.y)? ((v1.y < v3.y)? v1.y : v3.y) : ((v2.y < v3.y)? v2.y : v3.y));
    int xMax = (int)((v1.x > v2.x)? ((v1.x > v3.x)? v1.x : v3.x) : ((v2.x > v3.x)? v2.x : v3.x));
    int yMax = (int)((v1.y > v2.y)? ((v1.y > v3.y)? v1.y : v3.y) : ((v2.y > v3.y)? v2.y : v3.y));

    // Clamp the bounding box to the image dimensions
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax > dst->width) xMax = dst->width;
    if (yMax > dst->height) yMax = dst->height;

    // Check the order of the vertices to determine if it's a front or back face
    // NOTE: if signedArea is equal to 0, the face is degenerate
    float signedArea = (v2.x - v1.x)*(v3.y - v1.y) - (v3.x - v1.x)*(v2.y - v1.y);
    bool isBackFace = (signedArea > 0);

    // Barycentric interpolation setup
    // Calculate the step increments for the barycentric coordinates
    int w1XStep = (int)(v3.y - v2.y), w1YStep = (int)(v2.x - v3.x);
    int w2XStep = (int)(v1.y - v3.y), w2YStep = (int)(v3.x - v1.x);
    int w3XStep = (int)(v2.y - v1.y), w3YStep = (int)(v1.x - v2.x);

    // If the triangle is a back face, invert the steps
    if (isBackFace)
    {
        w1XStep = -w1XStep, w1YStep = -w1YStep;
        w2XStep = -w2XStep, w2YStep = -w2YStep;
        w3XStep = -w3XStep, w3YStep = -w3YStep;
    }

    // Calculate the initial barycentric coordinates for the top-left point of the bounding box
    int w1Row = (int)((xMin - v2.x)*w1XStep + w1YStep*(yMin - v2.y));
    int w2Row = (int)((xMin - v3.x)*w2XStep + w2YStep*(yMin - v3.y));
    int w3Row = (int)((xMin - v1.x)*w3XStep + w3YStep*(yMin - v1.y));

    // Rasterization loop
    // Iterate through each pixel in the bounding box
    for (int y = yMin; y <= yMax; y++)
    {
        int w1 = w1Row;
        int w2 = w2Row;
        int w3 = w3Row;

        for (int x = xMin; x <= xMax; x++)
        {
            // Check if the pixel is inside the triangle using barycentric coordinates
            // If it is, the pixel can be drawn with the given color
            if ((w1 | w2 | w3) >= 0) ImageDrawPixel(dst, x, y, color);

            // Increment the barycentric coordinates for the next pixel
            w1 += w1XStep;
            w2 += w2XStep;
            w3 += w3XStep;
        }

        // Move to the next row in the bounding box
        w1Row += w1YStep;
        w2Row += w2YStep;
        w3Row += w3YStep;
    }
}

// Draw triangle with interpolated colors within an image
void ImageDrawTriangleGradient(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color c1, Color c2, Color c3)
{
    // Calculate the 2D bounding box of the triangle
    // Determine the minimum and maximum x and y coordinates of the triangle vertices
    int xMin = (int)((v1.x < v2.x)? ((v1.x < v3.x)? v1.x : v3.x) : ((v2.x < v3.x)? v2.x : v3.x));
    int yMin = (int)((v1.y < v2.y)? ((v1.y < v3.y)? v1.y : v3.y) : ((v2.y < v3.y)? v2.y : v3.y));
    int xMax = (int)((v1.x > v2.x)? ((v1.x > v3.x)? v1.x : v3.x) : ((v2.x > v3.x)? v2.x : v3.x));
    int yMax = (int)((v1.y > v2.y)? ((v1.y > v3.y)? v1.y : v3.y) : ((v2.y > v3.y)? v2.y : v3.y));

    // Clamp the bounding box to the image dimensions
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax > dst->width) xMax = dst->width;
    if (yMax > dst->height) yMax = dst->height;

    // Check the order of the vertices to determine if it's a front or back face
    // NOTE: if signedArea is equal to 0, the face is degenerate
    float signedArea = (v2.x - v1.x)*(v3.y - v1.y) - (v3.x - v1.x)*(v2.y - v1.y);
    bool isBackFace = (signedArea > 0);

    // Barycentric interpolation setup
    // Calculate the step increments for the barycentric coordinates
    int w1XStep = (int)(v3.y - v2.y), w1YStep = (int)(v2.x - v3.x);
    int w2XStep = (int)(v1.y - v3.y), w2YStep = (int)(v3.x - v1.x);
    int w3XStep = (int)(v2.y - v1.y), w3YStep = (int)(v1.x - v2.x);

    // If the triangle is a back face, invert the steps
    if (isBackFace)
    {
        w1XStep = -w1XStep, w1YStep = -w1YStep;
        w2XStep = -w2XStep, w2YStep = -w2YStep;
        w3XStep = -w3XStep, w3YStep = -w3YStep;
    }

    // Calculate the initial barycentric coordinates for the top-left point of the bounding box
    int w1Row = (int)((xMin - v2.x)*w1XStep + w1YStep*(yMin - v2.y));
    int w2Row = (int)((xMin - v3.x)*w2XStep + w2YStep*(yMin - v3.y));
    int w3Row = (int)((xMin - v1.x)*w3XStep + w3YStep*(yMin - v1.y));

    // Calculate the inverse of the sum of the barycentric coordinates for normalization
    float wInvSum = 255.0f/(w1Row + w2Row + w3Row);

    // Rasterization loop
    // Iterate through each pixel in the bounding box
    for (int y = yMin; y <= yMax; y++)
    {
        int w1 = w1Row;
        int w2 = w2Row;
        int w3 = w3Row;

        for (int x = xMin; x <= xMax; x++)
        {
            // Check if the pixel is inside the triangle using barycentric coordinates
            if ((w1 | w2 | w3) >= 0)
            {
                // Compute the normalized barycentric coordinates
                unsigned char aW1 = (unsigned char)((float)w1*wInvSum);
                unsigned char aW2 = (unsigned char)((float)w2*wInvSum);
                unsigned char aW3 = (unsigned char)((float)w3*wInvSum);

                // Interpolate the color using the barycentric coordinates
                Color finalColor = { 0 };
                finalColor.r = (c1.r*aW1 + c2.r*aW2 + c3.r*aW3)/255;
                finalColor.g = (c1.g*aW1 + c2.g*aW2 + c3.g*aW3)/255;
                finalColor.b = (c1.b*aW1 + c2.b*aW2 + c3.b*aW3)/255;
                finalColor.a = (c1.a*aW1 + c2.a*aW2 + c3.a*aW3)/255;

                // Draw the pixel with the interpolated color
                ImageDrawPixel(dst, x, y, finalColor);
            }

            // Increment the barycentric coordinates for the next pixel
            w1 += w1XStep;
            w2 += w2XStep;
            w3 += w3XStep;
        }

        // Move to the next row in the bounding box
        w1Row += w1YStep;
        w2Row += w2YStep;
        w3Row += w3YStep;
    }
}

// Draw triangle outline within an image
void ImageDrawTriangleLines(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    ImageDrawLine(dst, (int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, color);
    ImageDrawLine(dst, (int)v2.x, (int)v2.y, (int)v3.x, (int)v3.y, color);
    ImageDrawLine(dst, (int)v3.x, (int)v3.y, (int)v1.x, (int)v1.y, color);
}

// Draw a triangle fan defined by points within an image (first vertex is the center)
void ImageDrawTriangleFan(Image *dst, const Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        for (int i = 1; i < pointCount - 1; i++)
        {
            ImageDrawTriangle(dst, points[0], points[i], points[i + 1], color);
        }
    }
}

// Draw a triangle strip defined by points within an image
void ImageDrawTriangleStrip(Image *dst, const Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        for (int i = 2; i < pointCount; i++)
        {
            if ((i%2) == 0) ImageDrawTriangle(dst, points[i], points[i - 2], points[i - 1], color);
            else ImageDrawTriangle(dst, points[i], points[i - 1], points[i - 2], color);
        }
    }
}




// Draw a color-filled rectangle with pro parameters within and image
void ImageDrawRectanglePro(Image *dst, Rectangle rec, Vector2 origin, float rotation, Color color)
{
    // TODO: NEW: Implement ImageDrawRectanglePro()
}












// Draw a part of an image defined by a rectangle into destination rectangle, with scaling and rotation, within an image
// TODO: REVIEW: ImageDrawImagePro(), implement origin and rotation for image drawing
void ImageDrawImagePro(Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Vector2 origin, float rotation, Color tint)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0) ||
        (src.data == NULL) || (src.width == 0) || (src.height == 0)) return;

    if (dst->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image drawing not supported for compressed formats");
    else
    {
        Image srcMod = { 0 };       // Source copy (in case it was required)
        Image *srcPtr = &src;       // Pointer to source image
        bool useSrcMod = false;     // Track source copy required

        // Source rectangle out-of-bounds security checks
        if (srcRec.x < 0) { srcRec.width += srcRec.x; srcRec.x = 0; }
        if (srcRec.y < 0) { srcRec.height += srcRec.y; srcRec.y = 0; }
        if ((srcRec.x + srcRec.width) > src.width) srcRec.width = src.width - srcRec.x;
        if ((srcRec.y + srcRec.height) > src.height) srcRec.height = src.height - srcRec.y;

        // Check if source rectangle needs to be resized to destination rectangle
        // In that case, make a copy of source, and apply all required transform
        if (((int)srcRec.width != (int)dstRec.width) || ((int)srcRec.height != (int)dstRec.height))
        {
            srcMod = ImageFromImage(src, srcRec); // Create image from another image
            ImageResize(&srcMod, (int)dstRec.width, (int)dstRec.height); // Resize to destination rectangle
            srcRec = (Rectangle){ 0, 0, (float)srcMod.width, (float)srcMod.height };

            srcPtr = &srcMod;
            useSrcMod = true;
        }

        // Destination rectangle out-of-bounds security checks
        if (dstRec.x < 0)
        {
            srcRec.x -= dstRec.x;
            srcRec.width += dstRec.x;
            dstRec.x = 0;
        }
        else if ((dstRec.x + srcRec.width) > dst->width) srcRec.width = dst->width - dstRec.x;

        if (dstRec.y < 0)
        {
            srcRec.y -= dstRec.y;
            srcRec.height += dstRec.y;
            dstRec.y = 0;
        }
        else if ((dstRec.y + srcRec.height) > dst->height) srcRec.height = dst->height - dstRec.y;

        if (dst->width < srcRec.width) srcRec.width = (float)dst->width;
        if (dst->height < srcRec.height) srcRec.height = (float)dst->height;

        // This blitting method is quite fast! The process followed is:
        // for every pixel -> [get_src_format/get_dst_format -> blend -> format_to_dst]
        // Some optimization ideas:
        //    [x] Avoid creating source copy if not required (no resize required)
        //    [x] Optimize ImageResize() for pixel format (alternative: ImageResizeNN())
        //    [x] Optimize ColorAlphaBlend() to avoid processing (alpha = 0) and (alpha = 1)
        //    [x] Optimize ColorAlphaBlend() for faster operations (maybe avoiding divs?)
        //    [x] Consider fast path: no alpha blending required cases (src has no alpha)
        //    [x] Consider fast path: same src/dst format with no alpha -> direct line copy
        //    [-] GetPixelColor(): Get Vector4 instead of Color, easier for ColorAlphaBlend()
        //    [-] Support 16bit and 32bit (float) channels drawing

        Color colSrc = { 0 };
        Color colDst = { 0 };
        Color blend = { 0 };
        bool blendRequired = true;

        // Fast path: Avoid blend if source has no alpha to blend
        if ((tint.a == 255) &&
            ((srcPtr->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R5G6B5) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R32) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R16) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R16G16B16)))
            blendRequired = false;

        int strideDst = GetPixelDataSize(dst->width, 1, dst->format);
        int bytesPerPixelDst = strideDst/(dst->width);

        int strideSrc = GetPixelDataSize(srcPtr->width, 1, srcPtr->format);
        int bytesPerPixelSrc = strideSrc/(srcPtr->width);

        unsigned char *pSrcBase = (unsigned char *)srcPtr->data + ((int)srcRec.y*srcPtr->width + (int)srcRec.x)*bytesPerPixelSrc;
        unsigned char *pDstBase = (unsigned char *)dst->data + ((int)dstRec.y*dst->width + (int)dstRec.x)*bytesPerPixelDst;

        for (int y = 0; y < (int)srcRec.height; y++)
        {
            unsigned char *pSrc = pSrcBase;
            unsigned char *pDst = pDstBase;

            // Fast path: Avoid moving pixel by pixel if no blend required and same format
            if (!blendRequired && (srcPtr->format == dst->format)) memcpy(pDst, pSrc, (int)(srcRec.width)*bytesPerPixelSrc);
            else
            {
                for (int x = 0; x < (int)srcRec.width; x++)
                {
                    colSrc = GetPixelColor(pSrc, srcPtr->format);
                    colDst = GetPixelColor(pDst, dst->format);

                    // Fast path: Avoid blend if source has no alpha to blend
                    if (blendRequired) blend = ColorAlphaBlend(colDst, colSrc, tint);
                    else blend = colSrc;

                    SetPixelColor(pDst, blend, dst->format);

                    pDst += bytesPerPixelDst;
                    pSrc += bytesPerPixelSrc;
                }
            }

            pSrcBase += strideSrc;
            pDstBase += strideDst;
        }

        if (useSrcMod) UnloadImage(srcMod);     // Unload source modified image

        if ((dst->mipmaps > 1) && (src.mipmaps > 1))
        {
            Image mipmapDst = *dst;
            mipmapDst.data = (char *)mipmapDst.data + GetPixelDataSize(mipmapDst.width, mipmapDst.height, mipmapDst.format);
            mipmapDst.width /= 2;
            mipmapDst.height /= 2;
            mipmapDst.mipmaps--;

            Image mipmapSrc = src;
            mipmapSrc.data = (char *)mipmapSrc.data + GetPixelDataSize(mipmapSrc.width, mipmapSrc.height, mipmapSrc.format);
            mipmapSrc.width /= 2;
            mipmapSrc.height /= 2;
            mipmapSrc.mipmaps--;

            Rectangle mipmapSrcRec = srcRec;
            mipmapSrcRec.width /= 2;
            mipmapSrcRec.height /= 2;
            mipmapSrcRec.x /= 2;
            mipmapSrcRec.y /= 2;

            Rectangle mipmapDstRec = dstRec;
            mipmapDstRec.width /= 2;
            mipmapDstRec.height /= 2;
            mipmapDstRec.x /= 2;
            mipmapDstRec.y /= 2;

            ImageDrawImagePro(&mipmapDst, mipmapSrc, mipmapSrcRec, mipmapDstRec, (Vector2){ 0 }, 0.0f, tint);
        }
    }
}

// Draw text using Font and pro parameters (rotation)
void ImageDrawTextPro(Image *dst, Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint)
{
    // TODO: NEW: Implement ImageDrawTextPro()
}

//------------------------------------------------------------------------------------
// Texture loading functions
//------------------------------------------------------------------------------------
// Load texture from file into GPU memory (VRAM)
Texture2D LoadTexture(const char *fileName)
{
    Texture2D texture = { 0 };

    Image image = LoadImage(fileName);

    if (image.data != NULL)
    {
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }

    return texture;
}

// Load a texture from image data
// NOTE: image is not unloaded, it must be done manually
Texture2D LoadTextureFromImage(Image image)
{
    Texture2D texture = { 0 };

    if ((image.width != 0) && (image.height != 0))
    {
        texture.id = rlLoadTexture(image.data, image.width, image.height, image.format, image.mipmaps);
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Data is not valid to load texture");

    texture.width = image.width;
    texture.height = image.height;
    texture.mipmaps = image.mipmaps;
    texture.format = image.format;

    return texture;
}


// Load texture for rendering (framebuffer)
// NOTE: Render texture is loaded by default with RGBA color attachment and depth RenderBuffer
RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(NULL, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Check if texture is valid (loaded in GPU)
bool IsTextureValid(Texture2D texture)
{
    bool result = false;

    if ((texture.id > 0) &&         // Validate OpenGL id (texture uplaoded to GPU)
        (texture.width > 0) &&      // Validate texture width
        (texture.height > 0) &&     // Validate texture height
        (texture.format > 0) &&     // Validate texture pixel format
        (texture.mipmaps > 0)) result = true; // Validate texture mipmaps (at least 1 for basic mipmap level)

    return result;
}

// Unload texture from GPU memory (VRAM)
void UnloadTexture(Texture2D texture)
{
    if (texture.id > 0)
    {
        rlUnloadTexture(texture.id);

        TRACELOG(LOG_INFO, "TEXTURE: [ID %i] Unloaded texture data from VRAM (GPU)", texture.id);
    }
}

// Check if render texture is valid (loaded in GPU)
bool IsRenderTextureValid(RenderTexture2D target)
{
    bool result = false;

    if ((target.id > 0) &&                  // Validate OpenGL id (loaded on GPU)
        IsTextureValid(target.depth) &&     // Validate FBO depth texture/renderbuffer attachment
        IsTextureValid(target.texture)) result = true; // Validate FBO texture attachment

    return result;
}

// Unload render texture from GPU memory (VRAM)
void UnloadRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        if (target.texture.id > 0)
        {
            // Color texture attached to FBO is deleted
            rlUnloadTexture(target.texture.id);
        }

        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}

// Update GPU texture with new data
// NOTE 1: pixels data must match texture.format
// NOTE 2: pixels data must contain at least as many pixels as texture
void UpdateTexture(Texture2D texture, const void *pixels)
{
    rlUpdateTexture(texture.id, 0, 0, texture.width, texture.height, texture.format, pixels);
}

// Update GPU texture rectangle with new data
// NOTE 1: pixels data must match texture.format
// NOTE 2: pixels data must contain as many pixels as rec contains
// NOTE 3: rec must fit completely within texture's width and height
void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels)
{
    rlUpdateTexture(texture.id, (int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, texture.format, pixels);
}

//------------------------------------------------------------------------------------
// Texture configuration functions
//------------------------------------------------------------------------------------
// Generate GPU mipmaps for a texture
void GenTextureMipmaps(Texture2D *texture)
{
    // NOTE: NPOT textures support check inside function
    // On WebGL (OpenGL ES 2.0) NPOT textures support is limited
    rlGenTextureMipmaps(texture->id, texture->width, texture->height, texture->format, &texture->mipmaps);
}

// Set texture scaling filter mode
void SetTextureFilter(Texture2D texture, int filter)
{
    switch (filter)
    {
        case TEXTURE_FILTER_POINT:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_NEAREST - tex filter: POINT, mipmaps filter: POINT (sharp switching between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_NEAREST);

                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
            else
            {
                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
        } break;
        case TEXTURE_FILTER_BILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST - tex filter: BILINEAR, mipmaps filter: POINT (sharp switching between mipmaps)
                // Alternative: RL_TEXTURE_FILTER_NEAREST_MIP_LINEAR - tex filter: POINT, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_TRILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_LINEAR - tex filter: BILINEAR, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] No mipmaps available for TRILINEAR texture filtering", texture.id);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_ANISOTROPIC_4X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 4); break;
        case TEXTURE_FILTER_ANISOTROPIC_8X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 8); break;
        case TEXTURE_FILTER_ANISOTROPIC_16X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 16); break;
        default: break;
    }
}

// Set texture wrapping mode
void SetTextureWrap(Texture2D texture, int wrap)
{
    switch (wrap)
    {
        case TEXTURE_WRAP_REPEAT:
        {
            // NOTE: It only works if NPOT textures are supported, i.e. OpenGL ES 2.0 could not support it
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
        } break;
        case TEXTURE_WRAP_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        } break;
        case TEXTURE_WRAP_MIRROR_REPEAT:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_REPEAT);
        } break;
        case TEXTURE_WRAP_MIRROR_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_CLAMP);
        } break;
        default: break;
    }
}

//------------------------------------------------------------------------------------
// Texture drawing functions
//------------------------------------------------------------------------------------
// Draw a texture
void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTextureEx(texture, (Vector2){ (float)posX, (float)posY }, 0.0f, 1.0f, tint);
}

// Draw a texture with position defined as Vector2
void DrawTextureV(Texture2D texture, Vector2 position, Color tint)
{
    DrawTextureEx(texture, position, 0, 1.0f, tint);
}

// Draw a texture with rotation and scale
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint)
{
    Rectangle srcrec = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
    Rectangle dstrec = { position.x, position.y, (float)texture.width*scale, (float)texture.height*scale };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texture, srcrec, dstrec, origin, rotation, tint);
}

// Draw a part of a texture (defined by a rectangle)
void DrawTextureRec(Texture2D texture, Rectangle rec, Vector2 position, Color tint)
{
    Rectangle dstrec = { position.x, position.y, fabsf(rec.width), fabsf(rec.height) };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texture, rec, dstrec, origin, 0.0f, tint);
}

// Draw a part of a texture (defined by a rectangle) with 'pro' parameters
// NOTE: origin is relative to destination rectangle size
void DrawTexturePro(Texture2D texture, Rectangle srcrec, Rectangle dstrec, Vector2 origin, float rotation, Color tint)
{
    // Check if texture is valid
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        bool flipX = false;

        if (srcrec.width < 0) { flipX = true; srcrec.width *= -1; }
        if (srcrec.height < 0) srcrec.y -= srcrec.height;

        if (dstrec.width < 0) dstrec.width *= -1;
        if (dstrec.height < 0) dstrec.height *= -1;

        Vector2 topLeft = { 0 };
        Vector2 topRight = { 0 };
        Vector2 bottomLeft = { 0 };
        Vector2 bottomRight = { 0 };

        // Only calculate rotation if needed
        if (rotation == 0.0f)
        {
            float x = dstrec.x - origin.x;
            float y = dstrec.y - origin.y;
            topLeft = (Vector2){ x, y };
            topRight = (Vector2){ x + dstrec.width, y };
            bottomLeft = (Vector2){ x, y + dstrec.height };
            bottomRight = (Vector2){ x + dstrec.width, y + dstrec.height };
        }
        else
        {
            float sinRotation = sinf(rotation*DEG2RAD);
            float cosRotation = cosf(rotation*DEG2RAD);
            float x = dstrec.x;
            float y = dstrec.y;
            float dx = -origin.x;
            float dy = -origin.y;

            topLeft.x = x + dx*cosRotation - dy*sinRotation;
            topLeft.y = y + dx*sinRotation + dy*cosRotation;

            topRight.x = x + (dx + dstrec.width)*cosRotation - dy*sinRotation;
            topRight.y = y + (dx + dstrec.width)*sinRotation + dy*cosRotation;

            bottomLeft.x = x + dx*cosRotation - (dy + dstrec.height)*sinRotation;
            bottomLeft.y = y + dx*sinRotation + (dy + dstrec.height)*cosRotation;

            bottomRight.x = x + (dx + dstrec.width)*cosRotation - (dy + dstrec.height)*sinRotation;
            bottomRight.y = y + (dx + dstrec.width)*sinRotation + (dy + dstrec.height)*cosRotation;
        }

        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);

            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

            // Top-left corner for texture and quad
            if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
            else rlTexCoord2f(srcrec.x/width, srcrec.y/height);
            rlVertex2f(topLeft.x, topLeft.y);

            // Bottom-left corner for texture and quad
            if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
            else rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
            rlVertex2f(bottomLeft.x, bottomLeft.y);

            // Bottom-right corner for texture and quad
            if (flipX) rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
            else rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
            rlVertex2f(bottomRight.x, bottomRight.y);

            // Top-right corner for texture and quad
            if (flipX) rlTexCoord2f(srcrec.x/width, srcrec.y/height);
            else rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
            rlVertex2f(topRight.x, topRight.y);

        rlEnd();
        rlSetTexture(0);

        // NOTE: Vertex position can be transformed using matrices
        // but the process is way more costly than calculating
        // the vertex positions manually, like done above
        // Old implementation is left here for educational purposes,
        // in case someone wants to do some performance test
        /*
        rlSetTexture(texture.id);
        rlPushMatrix();
            rlTranslatef(dstrec.x, dstrec.y, 0.0f);
            if (rotation != 0.0f) rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

                // Bottom-left corner for texture and quad
                if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
                else rlTexCoord2f(srcrec.x/width, srcrec.y/height);
                rlVertex2f(0.0f, 0.0f);

                // Bottom-right corner for texture and quad
                if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
                else rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
                rlVertex2f(0.0f, dstrec.height);

                // Top-right corner for texture and quad
                if (flipX) rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
                else rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
                rlVertex2f(dstrec.width, dstrec.height);

                // Top-left corner for texture and quad
                if (flipX) rlTexCoord2f(srcrec.x/width, srcrec.y/height);
                else rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
                rlVertex2f(dstrec.width, 0.0f);
            rlEnd();
        rlPopMatrix();
        rlSetTexture(0);
        */
    }
}


// Check if two colors are equal
bool ColorIsEqual(Color col1, Color col2)
{
    bool result = false;

    if ((col1.r == col2.r) && (col1.g == col2.g) && (col1.b == col2.b) && (col1.a == col2.a)) result = true;

    return result;
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color Fade(Color color, float alpha)
{
    Color result = color;

    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    result.a = (unsigned char)(255.0f*alpha);

    return result;
}

// Get hexadecimal value for a Color
int ColorToInt(Color color)
{
    int result = 0;

    result = (int)(((unsigned int)color.r << 24) |
                   ((unsigned int)color.g << 16) |
                   ((unsigned int)color.b << 8) |
                    (unsigned int)color.a);

    return result;
}

// Get color normalized as float [0..1]
Vector4 ColorNormalize(Color color)
{
    Vector4 result;

    result.x = (float)color.r/255.0f;
    result.y = (float)color.g/255.0f;
    result.z = (float)color.b/255.0f;
    result.w = (float)color.a/255.0f;

    return result;
}

// Get color from normalized values [0..1]
Color ColorFromNormalized(Vector4 normalized)
{
    Color result;

    result.r = (unsigned char)(normalized.x*255.0f);
    result.g = (unsigned char)(normalized.y*255.0f);
    result.b = (unsigned char)(normalized.z*255.0f);
    result.a = (unsigned char)(normalized.w*255.0f);

    return result;
}

// Get HSV values for a Color
// NOTE: Hue is returned as degrees [0..360]
Vector3 ColorToHSV(Color color)
{
    Vector3 hsv = { 0 };
    Vector3 rgb = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
    float min = 0.0f;
    float max = 0.0f;
    float delta = 0.0f;

    min = (rgb.x < rgb.y)? rgb.x : rgb.y;
    min = (min < rgb.z)? min : rgb.z;

    max = (rgb.x > rgb.y)? rgb.x : rgb.y;
    max = (max > rgb.z)? max : rgb.z;

    hsv.z = max; // Value
    delta = max - min;

    if (delta < 0.00001f)
    {
        hsv.y = 0.0f;
        hsv.x = 0.0f; // Undefined, maybe NAN?
        return hsv;
    }

    if (max > 0.0f)
    {
        // NOTE: If max is 0, this divide would cause a crash
        hsv.y = (delta/max); // Saturation
    }
    else
    {
        // NOTE: If max is 0, then r = g = b = 0, s = 0, h is undefined
        hsv.y = 0.0f;
        hsv.x = NAN; // Undefined
        return hsv;
    }

    // NOTE: Comparing float values could not work properly
    if (rgb.x >= max) hsv.x = (rgb.y - rgb.z)/delta; // Between yellow & magenta
    else
    {
        if (rgb.y >= max) hsv.x = 2.0f + (rgb.z - rgb.x)/delta; // Between cyan & yellow
        else hsv.x = 4.0f + (rgb.x - rgb.y)/delta; // Between magenta & cyan
    }

    hsv.x *= 60.0f; // Convert to degrees

    if (hsv.x < 0.0f) hsv.x += 360.0f;

    return hsv;
}

// Get a Color from HSV values
// Implementation reference: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSV_conversion
// NOTE: Color->HSV->Color conversion will not yield exactly the same color due to rounding errors
// Hue is provided in degrees: [0..360]
// Saturation/Value are provided normalized: [0.0f..1.0f]
Color ColorFromHSV(float hue, float saturation, float value)
{
    Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}

// Get color multiplied with another color
Color ColorTint(Color color, Color tint)
{
    Color result = color;

    unsigned char r = (unsigned char)(((int)color.r*(int)tint.r)/255);
    unsigned char g = (unsigned char)(((int)color.g*(int)tint.g)/255);
    unsigned char b = (unsigned char)(((int)color.b*(int)tint.b)/255);
    unsigned char a = (unsigned char)(((int)color.a*(int)tint.a)/255);

    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;

    return result;
}

// Get color with brightness correction, brightness factor goes from -1.0f to 1.0f
Color ColorBrightness(Color color, float factor)
{
    Color result = color;

    if (factor > 1.0f) factor = 1.0f;
    else if (factor < -1.0f) factor = -1.0f;

    float red = (float)color.r;
    float green = (float)color.g;
    float blue = (float)color.b;

    if (factor < 0.0f)
    {
        factor = 1.0f + factor;
        red *= factor;
        green *= factor;
        blue *= factor;
    }
    else
    {
        red = (255 - red)*factor + red;
        green = (255 - green)*factor + green;
        blue = (255 - blue)*factor + blue;
    }

    result.r = (unsigned char)red;
    result.g = (unsigned char)green;
    result.b = (unsigned char)blue;

    return result;
}

// Get color with contrast correction
// NOTE: Contrast values between -1.0f and 1.0f
Color ColorContrast(Color color, float contrast)
{
    Color result = color;

    if (contrast < -1.0f) contrast = -1.0f;
    else if (contrast > 1.0f) contrast = 1.0f;

    contrast = (1.0f + contrast);
    contrast *= contrast;

    float pR = (float)color.r/255.0f;
    pR -= 0.5f;
    pR *= contrast;
    pR += 0.5f;
    pR *= 255;
    if (pR < 0) pR = 0;
    else if (pR > 255) pR = 255;

    float pG = (float)color.g/255.0f;
    pG -= 0.5f;
    pG *= contrast;
    pG += 0.5f;
    pG *= 255;
    if (pG < 0) pG = 0;
    else if (pG > 255) pG = 255;

    float pB = (float)color.b/255.0f;
    pB -= 0.5f;
    pB *= contrast;
    pB += 0.5f;
    pB *= 255;
    if (pB < 0) pB = 0;
    else if (pB > 255) pB = 255;

    result.r = (unsigned char)pR;
    result.g = (unsigned char)pG;
    result.b = (unsigned char)pB;

    return result;
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color ColorAlpha(Color color, float alpha)
{
    Color result = color;

    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    result.a = (unsigned char)(255.0f*alpha);

    return result;
}

// Get src alpha-blended into dst color with tint
Color ColorAlphaBlend(Color dst, Color src, Color tint)
{
    Color result = WHITE;

    // Apply color tint to source color
    src.r = (unsigned char)(((unsigned int)src.r*((unsigned int)tint.r+1)) >> 8);
    src.g = (unsigned char)(((unsigned int)src.g*((unsigned int)tint.g+1)) >> 8);
    src.b = (unsigned char)(((unsigned int)src.b*((unsigned int)tint.b+1)) >> 8);
    src.a = (unsigned char)(((unsigned int)src.a*((unsigned int)tint.a+1)) >> 8);

//#define COLORALPHABLEND_FLOAT
#define COLORALPHABLEND_INTEGERS
#if defined(COLORALPHABLEND_INTEGERS)
    if (src.a == 0) result = dst;
    else if (src.a == 255) result = src;
    else
    {
        unsigned int alpha = (unsigned int)src.a + 1; // Shifting by 8 (dividing by 256), so need to take that excess into account
        result.a = (unsigned char)(((unsigned int)alpha*256 + (unsigned int)dst.a*(256 - alpha)) >> 8);

        if (result.a > 0)
        {
            result.r = (unsigned char)((((unsigned int)src.r*alpha*256 + (unsigned int)dst.r*(unsigned int)dst.a*(256 - alpha))/result.a) >> 8);
            result.g = (unsigned char)((((unsigned int)src.g*alpha*256 + (unsigned int)dst.g*(unsigned int)dst.a*(256 - alpha))/result.a) >> 8);
            result.b = (unsigned char)((((unsigned int)src.b*alpha*256 + (unsigned int)dst.b*(unsigned int)dst.a*(256 - alpha))/result.a) >> 8);
        }
    }
#endif
#if defined(COLORALPHABLEND_FLOAT)
    if (src.a == 0) result = dst;
    else if (src.a == 255) result = src;
    else
    {
        Vector4 fdst = ColorNormalize(dst);
        Vector4 fsrc = ColorNormalize(src);
        Vector4 ftint = ColorNormalize(tint);
        Vector4 fout = { 0 };

        fout.w = fsrc.w + fdst.w*(1.0f - fsrc.w);

        if (fout.w > 0.0f)
        {
            fout.x = (fsrc.x*fsrc.w + fdst.x*fdst.w*(1 - fsrc.w))/fout.w;
            fout.y = (fsrc.y*fsrc.w + fdst.y*fdst.w*(1 - fsrc.w))/fout.w;
            fout.z = (fsrc.z*fsrc.w + fdst.z*fdst.w*(1 - fsrc.w))/fout.w;
        }

        result = (Color){ (unsigned char)(fout.x*255.0f), (unsigned char)(fout.y*255.0f), (unsigned char)(fout.z*255.0f), (unsigned char)(fout.w*255.0f) };
    }
#endif

    return result;
}

// Get color lerp interpolation between two colors, factor [0.0f..1.0f]
Color ColorLerp(Color color1, Color color2, float factor)
{
    Color color = { 0 };

    if (factor < 0.0f) factor = 0.0f;
    else if (factor > 1.0f) factor = 1.0f;

    color.r = (unsigned char)((1.0f - factor)*color1.r + factor*color2.r);
    color.g = (unsigned char)((1.0f - factor)*color1.g + factor*color2.g);
    color.b = (unsigned char)((1.0f - factor)*color1.b + factor*color2.b);
    color.a = (unsigned char)((1.0f - factor)*color1.a + factor*color2.a);

    return color;
}

// Get a Color struct from hexadecimal value
Color GetColor(unsigned int hexValue)
{
    Color color;

    color.r = (unsigned char)(hexValue >> 24) & 0xff;
    color.g = (unsigned char)(hexValue >> 16) & 0xff;
    color.b = (unsigned char)(hexValue >> 8) & 0xff;
    color.a = (unsigned char)hexValue & 0xff;

    return color;
}

// Get color from a pixel from certain format
Color GetPixelColor(const void *srcPtr, int format)
{
    Color color = { 0 };

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 11)*255/31);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 5) & 0b0000000000111111)*255/63);
            color.b = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000011111)*255/31);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 11)*255/31);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 6) & 0b0000000000011111)*255/31);
            color.b = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000011111)*255/31);
            color.a = (((unsigned short *)srcPtr)[0] & 0b0000000000000001)? 255 : 0;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 12)*255/15);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 8) & 0b0000000000001111)*255/15);
            color.b = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 4) & 0b0000000000001111)*255/15);
            color.a = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000001111)*255/15);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1], ((unsigned char *)srcPtr)[2], ((unsigned char *)srcPtr)[3] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1], ((unsigned char *)srcPtr)[2], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[1]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[2]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[1]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[2]*255.0f);
            color.a = (unsigned char)(((float *)srcPtr)[3]*255.0f);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[0])*255.0f);
            color.g = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[0])*255.0f);
            color.b = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[0])*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[0])*255.0f);
            color.g = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[1])*255.0f);
            color.b = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[2])*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[0])*255.0f);
            color.g = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[1])*255.0f);
            color.b = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[2])*255.0f);
            color.a = (unsigned char)(HalfToFloat(((unsigned short *)srcPtr)[3])*255.0f);

        } break;
        default: break;
    }

    return color;
}

// Set pixel color formatted into destination pointer
void SetPixelColor(void *dstPtr, Color color, int format)
{
    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dstPtr)[0] = gray;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dstPtr)[0] = gray;
            ((unsigned char *)dstPtr)[1] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            // NOTE: Calculate R5G6B5 equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*63.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 11 | (unsigned short)g << 5 | (unsigned short)b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*31.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));
            unsigned char a = (coln.w > ((float)PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD/255.0f))? 1 : 0;

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*15.0f));
            unsigned char g = (unsigned char)(round(coln.y*15.0f));
            unsigned char b = (unsigned char)(round(coln.z*15.0f));
            unsigned char a = (unsigned char)(round(coln.w*15.0f));

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
        {
            ((unsigned char *)dstPtr)[0] = color.r;
            ((unsigned char *)dstPtr)[1] = color.g;
            ((unsigned char *)dstPtr)[2] = color.b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        {
            ((unsigned char *)dstPtr)[0] = color.r;
            ((unsigned char *)dstPtr)[1] = color.g;
            ((unsigned char *)dstPtr)[2] = color.b;
            ((unsigned char *)dstPtr)[3] = color.a;

        } break;
        default: break;
    }
}

// Get pixel data size in bytes for certain format
// NOTE: Size can be requested for Image or Texture data
int GetPixelDataSize(int width, int height, int format)
{
    int dataSize = 0;       // Size in bytes
    int bpp = 0;            // Bits per pixel

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: bpp = 8; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: bpp = 16; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: bpp = 24; break;
        case PIXELFORMAT_UNCOMPRESSED_R32: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32: bpp = 32*3; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: bpp = 32*4; break;
        case PIXELFORMAT_UNCOMPRESSED_R16: bpp = 16; break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16: bpp = 16*3; break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: bpp = 16*4; break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGB:
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC1_RGB:
        case PIXELFORMAT_COMPRESSED_ETC2_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA: bpp = 4; break;
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA:
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA:
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA: bpp = 8; break;
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA: bpp = 2; break;
        default: break;
    }

    double bytesPerPixel = (double)bpp/8.0;
    dataSize = (int)(bytesPerPixel*width*height); // Total data size in bytes

    // Most compressed formats works on 4x4 blocks,
    // if texture is smaller, minimum dataSize is 8 or 16
    if ((width < 4) && (height < 4))
    {
        if ((format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) && (format < PIXELFORMAT_COMPRESSED_DXT3_RGBA)) dataSize = 8;
        else if ((format >= PIXELFORMAT_COMPRESSED_DXT3_RGBA) && (format < PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA)) dataSize = 16;
    }

    return dataSize;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
// Convert half-float (stored as unsigned short) to float
// REF: https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion/60047308#60047308
static float HalfToFloat(unsigned short x)
{
    float result = 0.0f;

    union {
        float fm;
        unsigned int ui;
    } uni;

    const unsigned int e = (x & 0x7c00) >> 10; // Exponent
    const unsigned int m = (x & 0x03ff) << 13; // Mantissa
    uni.fm = (float)m;
    const unsigned int v = uni.ui >> 23; // Evil log2 bit hack to count leading zeros in denormalized format
    uni.ui = (x & 0x8000) << 16 | (e != 0)*((e + 112) << 23 | m) | ((e == 0)&(m != 0))*((v - 37) << 23 | ((m << (150 - v)) & 0x007fe000)); // sign : normalized : denormalized

    result = uni.fm;

    return result;
}

// Convert float to half-float (stored as unsigned short)
static unsigned short FloatToHalf(float x)
{
    unsigned short result = 0;

    union {
        float fm;
        unsigned int ui;
    } uni;
    uni.fm = x;

    const unsigned int b = uni.ui + 0x00001000; // Round-to-nearest-even: add last bit after truncated mantissa
    const unsigned int e = (b & 0x7f800000) >> 23; // Exponent
    const unsigned int m = b & 0x007fffff; // Mantissa; in line below: 0x007ff000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding

    result = (b & 0x80000000) >> 16 | (e > 112)*((((e - 112) << 10) & 0x7c00) | m >> 13) | ((e < 113) & (e > 101))*((((0x007ff000 + m) >> (125 - e)) + 1) >> 1) | (e > 143)*0x7fff; // sign : normalized : denormalized : saturate

    return result;
}

// Get pixel data from image as Vector4 array (float normalized)
static Vector4 *LoadImageDataNormalized(Image image)
{
    Vector4 *pixels = (Vector4 *)RL_MALLOC(image.width*image.height*sizeof(Vector4));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        for (int i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].w = (float)((unsigned char *)image.data)[k + 1]/255.0f;

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = (float)((pixel & 0b0000011111000000) >> 6)*(1.0f/31);
                    pixels[i].z = (float)((pixel & 0b0000000000111110) >> 1)*(1.0f/31);
                    pixels[i].w = ((pixel & 0b0000000000000001) == 0)? 0.0f : 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = (float)((pixel & 0b0000011111100000) >> 5)*(1.0f/63);
                    pixels[i].z = (float)(pixel & 0b0000000000011111)*(1.0f/31);
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111000000000000) >> 12)*(1.0f/15);
                    pixels[i].y = (float)((pixel & 0b0000111100000000) >> 8)*(1.0f/15);
                    pixels[i].z = (float)((pixel & 0b0000000011110000) >> 4)*(1.0f/15);
                    pixels[i].w = (float)(pixel & 0b0000000000001111)*(1.0f/15);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k + 1]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k + 2]/255.0f;
                    pixels[i].w = (float)((unsigned char *)image.data)[k + 3]/255.0f;

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k + 1]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k + 2]/255.0f;
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = 0.0f;
                    pixels[i].z = 0.0f;
                    pixels[i].w = 1.0f;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = ((float *)image.data)[k + 1];
                    pixels[i].z = ((float *)image.data)[k + 2];
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = ((float *)image.data)[k + 1];
                    pixels[i].z = ((float *)image.data)[k + 2];
                    pixels[i].w = ((float *)image.data)[k + 3];

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixels[i].x = HalfToFloat(((unsigned short *)image.data)[k]);
                    pixels[i].y = 0.0f;
                    pixels[i].z = 0.0f;
                    pixels[i].w = 1.0f;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixels[i].x = HalfToFloat(((unsigned short *)image.data)[k]);
                    pixels[i].y = HalfToFloat(((unsigned short *)image.data)[k + 1]);
                    pixels[i].z = HalfToFloat(((unsigned short *)image.data)[k + 2]);
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixels[i].x = HalfToFloat(((unsigned short *)image.data)[k]);
                    pixels[i].y = HalfToFloat(((unsigned short *)image.data)[k + 1]);
                    pixels[i].z = HalfToFloat(((unsigned short *)image.data)[k + 2]);
                    pixels[i].w = HalfToFloat(((unsigned short *)image.data)[k + 3]);

                    k += 4;
                } break;
                default: break;
            }
        }
    }

    return pixels;
}

#endif // SUPPORT_MODULE_RTEXTURES
