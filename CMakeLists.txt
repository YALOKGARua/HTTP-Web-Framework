cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HTTP_FRAMEWORK_VERSION)
string(STRIP "${HTTP_FRAMEWORK_VERSION}" HTTP_FRAMEWORK_VERSION)

project(HttpFramework 
    VERSION ${HTTP_FRAMEWORK_VERSION} 
    DESCRIPTION "Modern C++20 Web Framework"
    HOMEPAGE_URL "https://github.com/YALOKGARua/HTTP-Web-Framework"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis" OFF)

find_package(Threads REQUIRED)

if(WIN32)
    find_library(WS2_32_LIB ws2_32)
    find_library(MSWSOCK_LIB mswsock)
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/http/request.cpp
    src/http/response.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
    $<$<PLATFORM_ID:Windows>:${WS2_32_LIB}>
    $<$<PLATFORM_ID:Windows>:${MSWSOCK_LIB}>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_MODE)
    target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3 -DNDEBUG)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-unused-parameter>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HTTP_FRAMEWORK_VERSION="${PROJECT_VERSION}"
    HTTP_FRAMEWORK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    HTTP_FRAMEWORK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    HTTP_FRAMEWORK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(ENABLE_STATIC_ANALYSIS)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,modernize-*"
        )
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "http-framework"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/http-framework
    FILES_MATCHING PATTERN "*.hpp"
)

install(FILES README.md LICENSE VERSION DESTINATION share/doc/http-framework) 