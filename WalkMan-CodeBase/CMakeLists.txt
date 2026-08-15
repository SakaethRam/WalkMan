cmake_minimum_required(VERSION 3.20)
project(WalkMan VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WALKMAN_WARNINGS "Enable extra compiler warnings" ON)

add_library(walkman
    src/audio_io.cpp
    src/analysis.cpp
    src/detection.cpp
    src/corruption.cpp
    src/repair.cpp
    src/evaluation.cpp
    src/engine.cpp
)

target_include_directories(walkman
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WALKMAN_WARNINGS)
    if(MSVC)
        target_compile_options(walkman PRIVATE /W4 /permissive-)
    else()
        target_compile_options(walkman PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

add_executable(walkman_demo examples/walkman_demo.cpp)
target_link_libraries(walkman_demo PRIVATE walkman)

add_executable(walkman_cells notebook/walkman_cells.cpp)
target_link_libraries(walkman_cells PRIVATE walkman)

add_library(walkman_time_machine
    src/time_machine_types.cpp
    src/time_machine_snapshot.cpp
    src/time_machine_history.cpp
    src/time_machine_timeline.cpp
    src/time_machine_confidence.cpp
    src/time_machine_comparison.cpp
    src/time_machine_json.cpp
    src/time_machine_report.cpp
    src/time_machine_engine.cpp
    src/time_machine_audit.cpp
)

target_include_directories(walkman_time_machine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(walkman_time_machine PUBLIC walkman)

if(WALKMAN_WARNINGS)
    if(MSVC)
        target_compile_options(walkman_time_machine PRIVATE /W4 /permissive-)
    else()
        target_compile_options(walkman_time_machine PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

add_executable(walkman_time_machine_cli examples/time_machine_cli.cpp)
target_link_libraries(walkman_time_machine_cli PRIVATE walkman_time_machine)

add_executable(walkman_time_machine_cells notebook/time_machine_cells.cpp)
target_link_libraries(walkman_time_machine_cells PRIVATE walkman_time_machine)

if(UNIX AND NOT APPLE)
    target_link_libraries(walkman_demo PRIVATE m)
    target_link_libraries(walkman_cells PRIVATE m)
    target_link_libraries(walkman_time_machine_cli PRIVATE m)
    target_link_libraries(walkman_time_machine_cells PRIVATE m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(walkman_smoke_test tests/smoke_test.cpp)
    target_link_libraries(walkman_smoke_test PRIVATE walkman)
    add_test(NAME walkman_smoke_test COMMAND walkman_smoke_test)

    add_executable(walkman_time_machine_test tests/time_machine_test.cpp)
    target_link_libraries(walkman_time_machine_test PRIVATE walkman_time_machine)
    add_test(NAME walkman_time_machine_test COMMAND walkman_time_machine_test)
endif()