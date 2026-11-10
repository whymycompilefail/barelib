# Test targets for raylib
# NOTE: Tests are separated from src/CMakeLists.txt to keep library and test
# build descriptions independent.

# Helper function to eliminate the 5-line repeat pattern for each test target.
# Usage: add_raylib_test(test_name [source_file])
#   - source_file defaults to test_name.c if not specified.
#   - Additional properties can be set after the call (e.g. LABELS, LINK_LIBRARIES).
function(add_raylib_test name)
    set(src_file "${CMAKE_CURRENT_SOURCE_DIR}/${name}.c")
    list(LENGTH ARGN argn_len)
    if(argn_len GREATER 0)
        list(GET ARGN 0 src_file)
    endif()
    add_executable(${name} ${src_file})
    target_link_libraries(${name} raylib)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Test target for AI adapter
if (SUPPORT_MODULE_AI_ADAPTER)
    add_raylib_test(test_ai_adapter)
    # Sprite sheets: parsing and playback are pure CPU (no window/GPU needed)
    add_raylib_test(test_sprite)
endif()

# Protocol conformance harness for tests/ai_protocol_check.py
# A hidden-window game process driven over stdin/stdout by the Python checker,
# so protocol regressions can be verified without desktop interference (N5).
# Requires a GPU/OpenGL context; not registered as a ctest test.
if (SUPPORT_MODULE_AI_ADAPTER)
    add_executable(ai_protocol_harness ai_protocol_harness.c)
    target_link_libraries(ai_protocol_harness raylib)
    target_include_directories(ai_protocol_harness PRIVATE ${CMAKE_SOURCE_DIR}/src)

    # Protocol conformance regression over the live (hidden-window) harness.
    # Requires python + a GPU/OpenGL context, same tier as test_gpu (gpu label).
    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
        add_test(NAME ai_protocol_scenario
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/ai_scenario_check.py
                         ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/basic.json
                         $<TARGET_FILE:ai_protocol_harness>)
        set_tests_properties(ai_protocol_scenario PROPERTIES LABELS "gpu")
    endif()
endif()

# rtextures color manipulation (pure color functions, no GPU needed)
add_raylib_test(test_rtextures_color)

# rtext UTF-8 encoding functions (pure functions, no GPU needed)
add_raylib_test(test_rtext_utf8)

# raymath (pure math functions, no dependencies)
add_raylib_test(test_raymath)

# rtext (string utility functions, no dependencies)
add_raylib_test(test_rtext)

# rshapes collision detection (pure math, no dependencies)
add_raylib_test(test_rshapes_collision)

# raudio Wave operations (pure CPU, no audio device needed)
add_raylib_test(test_raudio)

# rcore file/path utility functions (pure CPU, no window needed)
add_raylib_test(test_rcore_file)

# rtextures image manipulation (pure CPU image operations, no GPU needed)
add_raylib_test(test_rtextures_image)

# rtext font loading functions (pure CPU font data operations, no GPU needed)
add_raylib_test(test_rtext_font)

# GPU-dependent test target (requires window/OpenGL context)
# NOTE: Uses FLAG_WINDOW_HIDDEN, can run on headless Windows systems
add_raylib_test(test_gpu)
set_tests_properties(test_gpu PROPERTIES LABELS "gpu")

# Minimal smoke test (verifies the build toolchain works)
add_raylib_test(test_rcore_minimal)