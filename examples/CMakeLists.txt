# barelib feature examples
#
# These are the ENGINE's own examples — AI-driven games showcasing the fork's
# differentiators (AI observability/operability, CJK text, procedural audio).
# The upstream raylib sample set (core/shapes/text/textures/audio/... ) was
# removed: this fork targets Windows + AI-agent workflows, not upstream demos.

set(BARELIB_EXAMPLES
    ai_music_demo     # Feature showcase: AI-observable music visualizer (procedural audio + CJK)
    ai_shapes_demo    # Feature showcase: AI-observable 2D shape gallery (all primitives incl. rounded rect + sector)
    ai_replay_demo    # Feature showcase: record/playback protocol + input injection (AI operability)
    )

foreach (example ${BARELIB_EXAMPLES})
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} raylib)
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    if (MSVC)
        # Sources are UTF-8 (CJK text in examples); without this MSVC decodes
        # string literals as the system ANSI codepage and CJK turns to garbage
        target_compile_options(${example} PRIVATE /utf-8)
    endif ()
endforeach ()
