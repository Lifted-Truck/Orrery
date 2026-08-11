# shell/core — the framework-free Orrery studio core (O1).
# No JUCE here: pure C++20 contract logic gated by ctest. The JUCE VST3/AU
# wrapper + auval land at O1b as a separate target that links this library.
cmake_minimum_required(VERSION 3.20)

add_library(orrery_core STATIC
    src/Clock.cpp
    src/OffsetLayer.cpp
    src/MidiRouter.cpp
    src/Trace.cpp
    src/StubEngine.cpp
)
target_include_directories(orrery_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(orrery_core PUBLIC cxx_std_20)
target_compile_options(orrery_core PRIVATE -Wall -Wextra -Wpedantic)

# ── ctest suite — the O1 Layer-0 gate ────────────────────────────────────────
set(ORRERY_CORE_TESTS
    test_pcg32
    test_clock
    test_offset_layer
    test_trace_roundtrip
    test_stub_engine
    test_determinism
    # Contract v1.1 — includes Lathe's consumer-proposed contract tests
    # (brief lathe-2026-07-23-001), landed here per INTEGRATIONS §3 so a
    # provider-side regression fails THIS build.
    test_contract_v11
)
foreach(t ${ORRERY_CORE_TESTS})
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE orrery_core)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
    # $<TARGET_FILE> (not the bare target name) so ctest finds the binary even
    # when JUCE relocates the runtime output dir in the plugin build.
    add_test(NAME ${t} COMMAND $<TARGET_FILE:${t}>)
endforeach()
