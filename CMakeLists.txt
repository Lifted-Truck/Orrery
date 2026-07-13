# Orrery — composite studio plugin (root build).
#
# O1 stage: builds the framework-free studio core (shell/core) and its ctest
# gate only. The JUCE VST3/AU plugin target, engine territories, and the
# audio-plugin validation dance (codesign seal + auval) are added at O1b / O2.
cmake_minimum_required(VERSION 3.20)
project(Orrery VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()
add_subdirectory(shell/core)
