# Orrery — composite studio plugin (root build).
#
# Two build modes:
#   • Core (default): framework-free shell/core + its ctest gate. No JUCE, no
#     network — this is what `./verify fast` configures.
#   • Plugin (-DORRERY_BUILD_PLUGIN=ON): additionally FetchContent-pulls JUCE and
#     builds the VST3/AU/Standalone instrument + RT no-alloc gate (O1b). The
#     codesign-seal + auval dance is machine-local / human-run per the global
#     CLAUDE.md build gotchas.
cmake_minimum_required(VERSION 3.24)
project(Orrery VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ORRERY_BUILD_PLUGIN "Build the JUCE VST3/AU/Standalone plugin shell (O1b)" OFF)

enable_testing()
add_subdirectory(shell/core)
add_subdirectory(engines/elastic-euclid)

# Framework-isolation gate (doctrine: pure, framework-free cores). Runs in every
# build — cheap, no compiler/network needed.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_test(NAME core_boundary
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_core_boundary.py)
endif()

if(ORRERY_BUILD_PLUGIN)
    include(FetchContent)
    # JUCE 8.0.14 — pinned (dependency policy: pin versions). Same commit AURICLE
    # pins, so a sibling's cached checkout can be reused offline via
    # -DFETCHCONTENT_SOURCE_DIR_JUCE=<path>.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        2cdfca8feb300fb424002ba2c2751569e5bacb64
        GIT_SHALLOW    FALSE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(shell/plugin)
endif()
