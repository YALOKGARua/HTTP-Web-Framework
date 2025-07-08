cmake_minimum_required(VERSION 3.16)
project(HttpFramework VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(WIN32)
    find_library(WS2_32_LIB ws2_32)
    find_library(MSWSOCK_LIB mswsock)
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/server.cpp
    src/core/connection.cpp
    src/core/thread_pool.cpp
    src/core/event_loop.cpp
    src/http/parser.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/router.cpp
    src/http/middleware.cpp
    src/websocket/websocket.cpp
    src/websocket/frame.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
    src/utils/file_handler.cpp
    src/utils/json_parser.cpp
    src/utils/crypto.cpp
    src/utils/base64.cpp
    src/utils/sha1.cpp
    src/network/socket.cpp
    src/network/epoll.cpp
    src/network/select.cpp
    src/memory/pool.cpp
    src/memory/arena.cpp
    src/async/future.cpp
    src/async/promise.cpp
    src/async/task.cpp
    src/compression/gzip.cpp
    src/compression/deflate.cpp
    src/ssl/tls_context.cpp
    src/ssl/certificate.cpp
    src/cache/lru_cache.cpp
    src/cache/memory_cache.cpp
    src/auth/jwt.cpp
    src/auth/session.cpp
    src/database/connection_pool.cpp
    src/database/query_builder.cpp
    src/static/file_server.cpp
    src/static/mime_types.cpp
    src/middleware/cors.cpp
    src/middleware/rate_limiter.cpp
    src/middleware/compression.cpp
    src/middleware/auth.cpp
    src/middleware/logger.cpp
    src/template/engine.cpp
    src/template/parser.cpp
    src/validation/validator.cpp
    src/validation/rules.cpp
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
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
) 