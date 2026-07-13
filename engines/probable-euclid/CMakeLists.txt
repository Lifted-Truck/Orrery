# engines/probable-euclid — probability-field rhythm engine (O3, organ).
# Framework-free: depends ONLY on orrery_core (the IEngine contract).

add_library(orrery_probable STATIC src/ProbableEuclid.cpp)
target_include_directories(orrery_probable PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(orrery_probable PUBLIC orrery_core)
target_compile_features(orrery_probable PUBLIC cxx_std_20)
target_compile_options(orrery_probable PRIVATE -Wall -Wextra -Wpedantic)

set(PROBABLE_TESTS test_probable_field test_probable_determinism)
foreach(t ${PROBABLE_TESTS})
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE orrery_probable)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
    add_test(NAME ${t} COMMAND $<TARGET_FILE:${t}>)
endforeach()
