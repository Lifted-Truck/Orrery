# engines/elastic-euclid — the equilibrium-rhythm engine (O2).
# Framework-free: depends ONLY on orrery_core (the IEngine contract). The
# core-boundary ctest fails the build if JUCE appears here.

add_library(orrery_elastic STATIC
    src/ElasticEuclid.cpp)
target_include_directories(orrery_elastic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(orrery_elastic PUBLIC orrery_core)
target_compile_features(orrery_elastic PUBLIC cxx_std_20)
target_compile_options(orrery_elastic PRIVATE -Wall -Wextra -Wpedantic)

# Acceptance tests (spec §8 — Layer-0, never weakened).
set(ELASTIC_TESTS
    test_elastic_equilibrium
    test_elastic_determinism
    test_elastic_latch)
foreach(t ${ELASTIC_TESTS})
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE orrery_elastic)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
    add_test(NAME ${t} COMMAND $<TARGET_FILE:${t}>)
endforeach()
