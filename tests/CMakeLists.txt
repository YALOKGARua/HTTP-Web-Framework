find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9fd7188b4e6e8ac29de32.tar.gz
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(framework_tests
    test_main.cpp
    test_router.cpp
    test_request.cpp
    test_response.cpp
    test_websocket.cpp
)

target_include_directories(framework_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(framework_tests
    gtest_main
    gmock_main
    Threads::Threads
    $<$<PLATFORM_ID:Windows>:${WS2_32_LIB}>
    $<$<PLATFORM_ID:Windows>:${MSWSOCK_LIB}>
)

target_compile_definitions(framework_tests PRIVATE
    HTTP_FRAMEWORK_VERSION="${PROJECT_VERSION}"
)

add_test(NAME FrameworkTests COMMAND framework_tests) 