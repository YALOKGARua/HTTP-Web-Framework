#pragma once

#include "core/server.hpp"
#include "core/connection.hpp"
#include "core/thread_pool.hpp"
#include "core/event_loop.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "http/router.hpp"
#include "http/middleware.hpp"
#include "websocket/websocket.hpp"
#include "utils/logger.hpp"
#include "utils/config.hpp"
#include "async/future.hpp"
#include "async/promise.hpp"
#include "async/task.hpp"
#include "middleware/cors.hpp"
#include "middleware/rate_limiter.hpp"
#include "middleware/compression.hpp"
#include "middleware/auth.hpp"
#include "static/file_server.hpp"
#include "template/engine.hpp"
#include "validation/validator.hpp"
#include "auth/jwt.hpp"
#include "cache/lru_cache.hpp"
#include "database/connection_pool.hpp"

namespace http_framework {
    using Server = core::Server;
    using Router = http::Router;
    using Request = http::Request;
    using Response = http::Response;
    using Middleware = http::Middleware;
    using WebSocket = websocket::WebSocket;
    using Logger = utils::Logger;
    using Config = utils::Config;
    
    template<typename T>
    using Future = async::Future<T>;
    
    template<typename T>
    using Promise = async::Promise<T>;
    
    using Task = async::Task;
    using FileServer = static_files::FileServer;
    using TemplateEngine = template_engine::Engine;
    using Validator = validation::Validator;
    using JWT = auth::JWT;
    
    template<typename K, typename V>
    using LRUCache = cache::LRUCache<K, V>;
    
    using ConnectionPool = database::ConnectionPool;
} 