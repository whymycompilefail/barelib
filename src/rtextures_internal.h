/**********************************************************************************************
*
*   rtextures_internal.h - Internal image/texture codec abstraction for raylib
*
*   This header defines the abstract interface between rtextures.c and external image
*   decoding/encoding libraries (stb_image, qoi, etc.). Currently, the implementation
*   lives in rtextures.c directly, but this interface creates a seam for swapping
*   dependencies (e.g. replace stb_image with libpng/libjpg).
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

#ifndef RTEXTURES_INTERNAL_H
#define RTEXTURES_INTERNAL_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Image Codec Interface
//----------------------------------------------------------------------------------
// These functions wrap external image loading/saving libraries.
// To replace a codec backend, provide alternative implementations of these functions.

// Load raw image data from memory (wraps stb_image / qoi / pep / rltexgpu)
// Returns the raw pixel data and format information.
unsigned char *LoadImageDataFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int *width, int *height, int *channels);

// Save image to memory buffer (wraps stb_image_write)
// Returns the encoded buffer and its size.
unsigned char *SaveImageDataToMemory(const char *fileType, const unsigned char *imageData, int width, int height, int channels, int *outputSize);

// Check if file type is supported by the image codec
bool IsImageFileTypeSupported(const char *fileType);

#endif // RTEXTURES_INTERNAL_H