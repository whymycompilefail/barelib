# Setup the project and settings
project(raylib C)
set(PROJECT_VERSION 6.0.0)
set(API_VERSION 600)

include(GNUInstallDirs)
include(JoinPaths)

# Sets build type if not set by now
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    if(PROJECT_IS_TOP_LEVEL)
        set(default_build_type Debug)
    else()
        message(WARNING "Default build type is not set (CMAKE_BUILD_TYPE)")
    endif()

    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")

    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Used as public API to be included into other projects
# NOTE: raylib.h internally #includes raylib_core.h, raylib_shapes.h, raylib_textures.h,
# raylib_text.h, raylib_audio.h, and config.h (raylib_models.h is conditional and
# disabled via SUPPORT_MODULE_RMODELS=0). rlgl.h is self-contained (batch types +
# immediate-mode declarations; candidate 4 removed the internal/ include).
# All of these must be present in bin/include so AI-authored game.c files can compile
# with only -I<bin/include>.
set(raylib_public_headers
    raylib.h
    raylib_core.h
    raylib_shapes.h
    raylib_textures.h
    raylib_text.h
    raylib_audio.h
    rlgl.h
    raymath.h
    config.h
    raylib_ai_adapter.h
    raylib_ai_input.h
    raylib_observability.h
    )

# Sources to be compiled
# The embedded CJK font is a ~81MB generated source file (git-ignored). Regenerate
# it on demand so a fresh clone still builds (requires pip: requests py7zr fonttools).
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cjk_font_data.c")
    message(STATUS "cjk_font_data.c missing, generating embedded CJK font...")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/prepare_cjk_font.py"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE CJK_FONT_RESULT)
        if(NOT CJK_FONT_RESULT EQUAL 0)
            message(FATAL_ERROR "Font generation failed. Install deps (pip install requests py7zr fonttools) "
                                "and run: python scripts/prepare_cjk_font.py")
        endif()
    else()
        message(FATAL_ERROR "src/cjk_font_data.c is missing and Python3 was not found. "
                            "Install Python 3 and run: python scripts/prepare_cjk_font.py")
    endif()
endif()

set(raylib_sources
    rcore.c
    rcore_fs.c
    rcore_input.c
    rlgl.c
    raymath.c
    rshapes.c
    rtext.c
    cjk_font_data.c
    rtextures.c
    )

# Platform backend files (separate compilation units)
# NOTE: Only the platform matching the target is compiled.
# Web and Android backends were removed: Windows is the only supported platform.
if (${PLATFORM} STREQUAL "Desktop")
    list(APPEND raylib_sources platform_glfw.c)
endif()

# Only build raudio if it's enabled
if (NOT DEFINED SUPPORT_MODULE_RAUDIO OR SUPPORT_MODULE_RAUDIO)
    list(APPEND raylib_sources raudio.c)
endif()

# Only build the AI adapter if it's enabled
# NOTE: Uses the same check pattern as SUPPORT_MODULE_RAUDIO - defaults to ON when
# CUSTOMIZE_BUILD=OFF (CMake variable not defined, but config.h defines it as 1).
if (NOT DEFINED SUPPORT_MODULE_AI_ADAPTER OR SUPPORT_MODULE_AI_ADAPTER)
    list(APPEND raylib_sources raylib_ai_adapter.c)

    # M4 code-comprehensibility refactor: AI adapter split into units
    list(APPEND raylib_sources ai_audio.c)
    list(APPEND raylib_sources ai_scene.c)
    list(APPEND raylib_sources ai_input.c)
    list(APPEND raylib_sources ai_protocol.c)

    # Platform-specific AI input simulation
    if (WIN32)
        list(APPEND raylib_sources raylib_ai_input_win32.c)
    else()
        list(APPEND raylib_sources raylib_ai_input_stubs.c)
    endif()
endif()

# <root>/cmake/GlfwImport.cmake handles the details around the inclusion of glfw
if (NOT ${PLATFORM} MATCHES "Web")
    include(GlfwImport)
endif ()

# Sets additional platform options and link libraries for each platform
# also selects the proper graphics API and version for that platform
# Produces a variable LIBS_PRIVATE that will be used later
include(LibraryConfigurations)

if (DEFINED SUPPORT_MODULE_RAUDIO AND NOT SUPPORT_MODULE_RAUDIO)
    MESSAGE(STATUS "Audio Backend: None (-DCUSTOMIZE_BUILD=ON -DSUPPORT_MODULE_RAUDIO=OFF)")
else ()
    MESSAGE(STATUS "Audio Backend: miniaudio")
endif ()

add_library(raylib STATIC ${raylib_sources} ${raylib_public_headers})
MESSAGE(STATUS "Building raylib static library (exe-only engine, no DLL builds)")
add_library(raylib_static ALIAS raylib)

if (${PLATFORM} MATCHES "Web")
    target_link_options(raylib PUBLIC "-sUSE_GLFW=3" -sEXPORTED_RUNTIME_METHODS=ccall)
    if(${GRAPHICS} MATCHES "GRAPHICS_API_OPENGL_ES3")
        target_link_options(raylib PUBLIC "-sMIN_WEBGL_VERSION=2")
        target_link_options(raylib PUBLIC "-sMAX_WEBGL_VERSION=2")
    endif()
endif()

set_target_properties(raylib PROPERTIES
                      PUBLIC_HEADER "${raylib_public_headers}"
                      VERSION ${PROJECT_VERSION}
                      SOVERSION ${API_VERSION}
                      )

if (WITH_PIC OR BUILD_SHARED_LIBS)
    set_property(TARGET raylib PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (BUILD_SHARED_LIBS)
    # Hide raylib's symbols by default so RLAPI can expose them
    set_property(TARGET raylib PROPERTY C_VISIBILITY_PRESET hidden)
endif ()

target_link_libraries(raylib PRIVATE $<BUILD_INTERFACE:${LIBS_PRIVATE}>)
target_link_libraries(raylib PUBLIC ${LIBS_PUBLIC})

# Sets some compile time definitions for the pre-processor
# If CUSTOMIZE_BUILD option is on you will not use config.h by default
# and you will be able to select more build options
include(CompileDefinitions)

# Registering include directories
target_include_directories(raylib
                           PUBLIC
                           $<INSTALL_INTERFACE:include>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${OPENGL_INCLUDE_DIR}
                           ${OPENAL_INCLUDE_DIR}
                           )

# MSVC: treat source files as UTF-8 so non-ASCII characters (comments, strings)
# don't trigger C4819 under non-English code pages (e.g. Chinese locale 936).
# PUBLIC so examples and downstream consumers inherit the flag.
if(MSVC)
    target_compile_options(raylib PUBLIC /utf-8)
endif()

# Copy the header files to the bin directory for convenience
# Copy public headers so bin/include is self-contained for AI-authored game.c
# files compiling with only -I<bin/include> (rlgl.h is self-contained since the
# batch types moved into it — candidate 4; no internal/ subdirectory needed).
file(COPY ${raylib_public_headers} DESTINATION "${CMAKE_SOURCE_DIR}/bin/include")
if (PLATFORM STREQUAL "Desktop")
    if (UNIX AND NOT APPLE)
        if (GLFW_BUILD_X11)
            find_package(X11 REQUIRED)

            target_compile_definitions(raylib PRIVATE _GLFW_X11)

            target_link_libraries(raylib PRIVATE
                ${X11_LIBRARIES}
            )

            message(STATUS "X11 support enabled for raylib")
        endif()
    endif()
endif()

# Includes information on how the library will be installed on the system
# when cmake --install is run
include(InstallConfigurations)

# Print the flags for the user
if (DEFINED CMAKE_BUILD_TYPE)
    message(STATUS "Generated build type: ${CMAKE_BUILD_TYPE}")
else ()
    message(STATUS "Generated config types: ${CMAKE_CONFIGURATION_TYPES}")
endif ()

message(STATUS "Compiling with the flags:")
message(STATUS "  PLATFORM=" ${PLATFORM_CPP})
message(STATUS "  GRAPHICS=" ${GRAPHICS})

# Options if you want to create an installer using CPack
include(PackConfigurations)
