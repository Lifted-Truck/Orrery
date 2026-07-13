# engines/measured-euclid — measure-warped even onset engine (O3, organ).
# Framework-free: depends ONLY on orrery_core (the IEngine contract).

add_library(orrery_measured STATIC src/MeasuredEuclid.cpp)
target_include_directories(orrery_measured PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(orrery_measured PUBLIC orrery_core)
target_compile_features(orrery_measured PUBLIC cxx_std_20)
target_compile_options(orrery_measured PRIVATE -Wall -Wextra -Wpedantic)

set(MEASURED_TESTS test_measured_euclid test_measured_determinism)
foreach(t ${MEASURED_TESTS})
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE orrery_measured)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
    add_test(NAME ${t} COMMAND $<TARGET_FILE:${t}>)
endforeach()
