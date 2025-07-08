#pragma once

#include "core/types.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "async/future.hpp"
#include <regex>

namespace http_framework::graphql {
    enum class TypeKind : std::uint8_t {
        SCALAR = 0,
        OBJECT = 1,
        INTERFACE = 2,
        UNION = 3,
        ENUM = 4,
        INPUT_OBJECT = 5,
        LIST = 6,
        NON_NULL = 7
    };
    
    enum class ScalarType : std::uint8_t {
        STRING = 0,
        INT = 1,
        FLOAT = 2,
        BOOLEAN = 3,
        ID = 4
    };
    
    struct Value {
        variant<string, int64_t, double, bool, vector<Value>, hash_map<string, Value>, std::monostate> data;
        
        Value() = default;
        
        template<typename T>
        explicit Value(T&& val);
        
        template<typename T>
        optional<T> get() const;
        
        bool is_null() const noexcept;
        string to_json() const;
        static Value from_json(string_view json);
    };
    
    struct Variable {
        string name;
        string type;
        optional<Value> default_value;
        bool non_null{false};
    };
    
    struct Argument {
        string name;
        Value value;
        optional<string> variable_name;
    };
    
    struct Directive {
        string name;
        vector<Argument> arguments;
    };
    
    struct Field {
        string name;
        optional<string> alias;
        vector<Argument> arguments;
        vector<Directive> directives;
        vector<Field> selection_set;
        
        string get_response_key() const { return alias.value_or(name); }
    };
    
    struct FragmentSpread {
        string name;
        vector<Directive> directives;
    };
    
    struct InlineFragment {
        optional<string> type_condition;
        vector<Directive> directives;
        vector<Field> selection_set;
    };
    
    struct Selection {
        variant<Field, FragmentSpread, InlineFragment> data;
    };
    
    struct FragmentDefinition {
        string name;
        string type_condition;
        vector<Directive> directives;
        vector<Field> selection_set;
    };
    
    struct OperationDefinition {
        enum class Type {
            QUERY,
            MUTATION,
            SUBSCRIPTION
        };
        
        Type operation_type{Type::QUERY};
        optional<string> name;
        vector<Variable> variables;
        vector<Directive> directives;
        vector<Field> selection_set;
    };
    
    struct Document {
        vector<OperationDefinition> operations;
        vector<FragmentDefinition> fragments;
        
        optional<OperationDefinition> get_operation(string_view name = "") const;
        optional<FragmentDefinition> get_fragment(string_view name) const;
    };
    
    struct ExecutionResult {
        optional<Value> data;
        vector<string> errors;
        optional<hash_map<string, Value>> extensions;
        
        string to_json() const;
        bool has_errors() const noexcept { return !errors.empty(); }
    };
    
    class TypeSystem;
    class Resolver;
    
    struct ResolverInfo {
        const Field& field;
        const hash_map<string, Value>& variables;
        const TypeSystem& schema;
        optional<Value> parent_value;
        hash_map<string, Value> context;
    };
    
    using ResolverFunction = function<async::Future<Value>(const ResolverInfo&)>;
    using ScalarSerializer = function<Value(const Value&)>;
    using ScalarParser = function<Value(const Value&)>;
    
    class FieldType {
    public:
        TypeKind kind() const noexcept { return kind_; }
        const string& name() const noexcept { return name_; }
        bool is_non_null() const noexcept { return non_null_; }
        bool is_list() const noexcept { return list_; }
        
        static FieldType scalar(ScalarType type);
        static FieldType object(string_view name);
        static FieldType interface(string_view name);
        static FieldType union_(string_view name);
        static FieldType enum_(string_view name);
        static FieldType input_object(string_view name);
        static FieldType list(FieldType element_type);
        static FieldType non_null(FieldType wrapped_type);
        
        string to_string() const;
        
    private:
        TypeKind kind_;
        string name_;
        bool non_null_{false};
        bool list_{false};
        unique_ptr<FieldType> wrapped_type_;
        
        FieldType(TypeKind k, string_view n);
    };
    
    struct FieldDefinition {
        string name;
        string description;
        FieldType type;
        vector<pair<string, FieldType>> arguments;
        ResolverFunction resolver;
        vector<string> directives;
        bool deprecated{false};
        string deprecation_reason;
    };
    
    struct ObjectType {
        string name;
        string description;
        vector<FieldDefinition> fields;
        vector<string> interfaces;
        hash_map<string, Value> extensions;
        
        optional<FieldDefinition> get_field(string_view name) const;
        bool implements_interface(string_view interface_name) const;
    };
    
    struct InterfaceType {
        string name;
        string description;
        vector<FieldDefinition> fields;
        function<vector<string>(const Value&)> type_resolver;
        
        optional<FieldDefinition> get_field(string_view name) const;
    };
    
    struct UnionType {
        string name;
        string description;
        vector<string> possible_types;
        function<string(const Value&)> type_resolver;
    };
    
    struct EnumValue {
        string name;
        Value value;
        string description;
        bool deprecated{false};
        string deprecation_reason;
        vector<string> directives;
    };
    
    struct EnumType {
        string name;
        string description;
        vector<EnumValue> values;
        
        optional<EnumValue> get_value(string_view name) const;
        optional<EnumValue> get_value_by_internal(const Value& internal_value) const;
    };
    
    struct InputFieldDefinition {
        string name;
        string description;
        FieldType type;
        optional<Value> default_value;
        vector<string> directives;
    };
    
    struct InputObjectType {
        string name;
        string description;
        vector<InputFieldDefinition> fields;
        
        optional<InputFieldDefinition> get_field(string_view name) const;
    };
    
    struct ScalarTypeDefinition {
        string name;
        string description;
        ScalarSerializer serializer;
        ScalarParser parser;
        function<bool(const Value&)> validator;
    };
    
    class TypeSystem {
    public:
        TypeSystem();
        
        void add_object_type(ObjectType type);
        void add_interface_type(InterfaceType type);
        void add_union_type(UnionType type);
        void add_enum_type(EnumType type);
        void add_input_object_type(InputObjectType type);
        void add_scalar_type(ScalarTypeDefinition type);
        
        void set_query_type(string_view type_name);
        void set_mutation_type(string_view type_name);
        void set_subscription_type(string_view type_name);
        
        optional<ObjectType> get_object_type(string_view name) const;
        optional<InterfaceType> get_interface_type(string_view name) const;
        optional<UnionType> get_union_type(string_view name) const;
        optional<EnumType> get_enum_type(string_view name) const;
        optional<InputObjectType> get_input_object_type(string_view name) const;
        optional<ScalarTypeDefinition> get_scalar_type(string_view name) const;
        
        const string& query_type() const noexcept { return query_type_; }
        const string& mutation_type() const noexcept { return mutation_type_; }
        const string& subscription_type() const noexcept { return subscription_type_; }
        
        bool validate() const;
        string to_sdl() const;
        vector<string> get_validation_errors() const;
        
        static TypeSystem from_sdl(string_view sdl);
        
    private:
        hash_map<string, ObjectType> object_types_;
        hash_map<string, InterfaceType> interface_types_;
        hash_map<string, UnionType> union_types_;
        hash_map<string, EnumType> enum_types_;
        hash_map<string, InputObjectType> input_object_types_;
        hash_map<string, ScalarTypeDefinition> scalar_types_;
        
        string query_type_{"Query"};
        string mutation_type_{"Mutation"};
        string subscription_type_{"Subscription"};
        
        void add_builtin_scalars();
        void validate_type_references() const;
        void validate_interface_implementations() const;
    };
    
    class QueryParser {
    public:
        static Document parse(string_view query);
        static Document parse_with_variables(string_view query, const hash_map<string, Value>& variables);
        
        struct ParseError {
            string message;
            size_type line;
            size_type column;
            size_type position;
        };
        
        const vector<ParseError>& errors() const noexcept { return errors_; }
        bool has_errors() const noexcept { return !errors_.empty(); }
        
    private:
        vector<ParseError> errors_;
        string_view source_;
        size_type position_{0};
        size_type line_{1};
        size_type column_{1};
        
        void add_error(string_view message);
        char current_char() const;
        char peek_char(size_type offset = 1) const;
        void advance();
        void skip_whitespace();
        void skip_comment();
        bool is_at_end() const;
        
        Document parse_document();
        OperationDefinition parse_operation();
        FragmentDefinition parse_fragment();
        vector<Variable> parse_variable_definitions();
        vector<Directive> parse_directives();
        vector<Field> parse_selection_set();
        Field parse_field();
        vector<Argument> parse_arguments();
        Value parse_value();
        string parse_name();
        string parse_string();
        
        bool expect_token(char token);
        bool consume_keyword(string_view keyword);
    };
    
    class QueryValidator {
    public:
        explicit QueryValidator(const TypeSystem& schema);
        
        vector<string> validate(const Document& document, const hash_map<string, Value>& variables = {});
        
    private:
        const TypeSystem& schema_;
        vector<string> errors_;
        hash_map<string, FragmentDefinition> fragments_;
        hash_map<string, Value> variables_;
        
        void validate_operation(const OperationDefinition& operation);
        void validate_selection_set(const vector<Field>& selection_set, const string& parent_type);
        void validate_field(const Field& field, const string& parent_type);
        void validate_arguments(const vector<Argument>& arguments, const vector<pair<string, FieldType>>& expected);
        void validate_variable_usage();
        void validate_fragment_spreads();
        
        void add_error(string_view message);
        bool is_valid_type_for_field(const FieldType& field_type, const string& parent_type);
    };
    
    class ExecutionEngine {
    public:
        explicit ExecutionEngine(const TypeSystem& schema);
        
        async::Future<ExecutionResult> execute(const Document& document, 
                                              const hash_map<string, Value>& variables = {},
                                              optional<string> operation_name = std::nullopt,
                                              hash_map<string, Value> context = {});
        
        async::Future<ExecutionResult> execute_query(string_view query,
                                                    const hash_map<string, Value>& variables = {},
                                                    hash_map<string, Value> context = {});
        
        void set_field_resolver(string_view type_name, string_view field_name, ResolverFunction resolver);
        void set_default_resolver(ResolverFunction resolver);
        
        void enable_tracing(bool enable = true) { tracing_enabled_ = enable; }
        void enable_caching(bool enable = true) { caching_enabled_ = enable; }
        void set_max_depth(size_type depth) { max_depth_ = depth; }
        void set_max_complexity(size_type complexity) { max_complexity_ = complexity; }
        
    private:
        const TypeSystem& schema_;
        hash_map<string, hash_map<string, ResolverFunction>> resolvers_;
        optional<ResolverFunction> default_resolver_;
        bool tracing_enabled_{false};
        bool caching_enabled_{false};
        size_type max_depth_{15};
        size_type max_complexity_{1000};
        
        async::Future<Value> execute_operation(const OperationDefinition& operation,
                                              const hash_map<string, Value>& variables,
                                              const hash_map<string, Value>& context);
        
        async::Future<Value> execute_selection_set(const vector<Field>& selection_set,
                                                  const string& parent_type,
                                                  const Value& parent_value,
                                                  const hash_map<string, Value>& variables,
                                                  const hash_map<string, Value>& context);
        
        async::Future<Value> execute_field(const Field& field,
                                          const string& parent_type,
                                          const Value& parent_value,
                                          const hash_map<string, Value>& variables,
                                          const hash_map<string, Value>& context);
        
        async::Future<Value> resolve_field_value(const Field& field,
                                                const string& parent_type,
                                                const Value& parent_value,
                                                const hash_map<string, Value>& variables,
                                                const hash_map<string, Value>& context);
        
        Value coerce_result_value(const Value& value, const FieldType& type);
        hash_map<string, Value> coerce_argument_values(const vector<Argument>& arguments,
                                                      const vector<pair<string, FieldType>>& argument_definitions,
                                                      const hash_map<string, Value>& variables);
        
        ResolverFunction get_field_resolver(string_view type_name, string_view field_name);
        void validate_query_complexity(const Document& document);
        void validate_query_depth(const vector<Field>& selection_set, size_type current_depth = 0);
        
        struct ExecutionContext {
            const hash_map<string, Value>& variables;
            const hash_map<string, Value>& context;
            hash_map<string, FragmentDefinition> fragments;
            vector<string> errors;
            size_type complexity_score{0};
        };
    };
    
    class GraphQLHandler {
    public:
        explicit GraphQLHandler(TypeSystem schema);
        
        void handle_request(const http::Request& request, http::Response& response);
        async::Task<void> handle_request_async(const http::Request& request, http::Response& response);
        
        void enable_playground(bool enable = true) { playground_enabled_ = enable; }
        void enable_introspection(bool enable = true) { introspection_enabled_ = enable; }
        void set_playground_title(string_view title) { playground_title_ = title; }
        void set_endpoint_path(string_view path) { endpoint_path_ = path; }
        
        void add_schema_directive(string_view name, function<void(const Directive&, ExecutionContext&)> handler);
        void add_field_middleware(function<async::Future<Value>(const ResolverInfo&, function<async::Future<Value>()>)> middleware);
        
    private:
        TypeSystem schema_;
        ExecutionEngine engine_;
        bool playground_enabled_{true};
        bool introspection_enabled_{true};
        string playground_title_{"GraphQL Playground"};
        string endpoint_path_{"/graphql"};
        
        struct QueryRequest {
            string query;
            optional<string> operation_name;
            hash_map<string, Value> variables;
        };
        
        optional<QueryRequest> parse_request(const http::Request& request);
        void send_playground_response(http::Response& response);
        void send_introspection_response(http::Response& response);
        void send_error_response(http::Response& response, string_view error_message, status_code_t status = 400);
        
        string generate_playground_html();
        TypeSystem build_introspection_schema();
        
        vector<function<async::Future<Value>(const ResolverInfo&, function<async::Future<Value>()>)>> field_middlewares_;
        hash_map<string, function<void(const Directive&, ExecutionContext&)>> schema_directives_;
    };
    
    template<typename T>
    Value::Value(T&& val) {
        if constexpr (std::is_same_v<std::decay_t<T>, string>) {
            data = std::forward<T>(val);
        } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            data = std::forward<T>(val);
        } else if constexpr (std::is_integral_v<std::decay_t<T>>) {
            data = static_cast<int64_t>(val);
        } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
            data = static_cast<double>(val);
        } else if constexpr (std::is_same_v<std::decay_t<T>, vector<Value>>) {
            data = std::forward<T>(val);
        } else if constexpr (std::is_same_v<std::decay_t<T>, hash_map<string, Value>>) {
            data = std::forward<T>(val);
        }
    }
    
    template<typename T>
    optional<T> Value::get() const {
        if (std::holds_alternative<T>(data)) {
            return std::get<T>(data);
        }
        return std::nullopt;
    }
} 