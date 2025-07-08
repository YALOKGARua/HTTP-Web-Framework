#pragma once

#include "core/types.hpp"

namespace http_framework::websocket {
    class Frame {
    public:
        Frame() = default;
        Frame(WebSocketOpcode opcode, const buffer_t& payload, bool fin = true, bool masked = false);
        Frame(const Frame&) = default;
        Frame(Frame&&) = default;
        Frame& operator=(const Frame&) = default;
        Frame& operator=(Frame&&) = default;
        ~Frame() = default;
        
        bool fin() const noexcept { return fin_; }
        bool rsv1() const noexcept { return rsv1_; }
        bool rsv2() const noexcept { return rsv2_; }
        bool rsv3() const noexcept { return rsv3_; }
        WebSocketOpcode opcode() const noexcept { return opcode_; }
        bool masked() const noexcept { return masked_; }
        uint64_t payload_length() const noexcept { return payload_length_; }
        const array<byte_t, 4>& masking_key() const noexcept { return masking_key_; }
        const buffer_t& payload() const noexcept { return payload_; }
        
        void set_fin(bool fin) noexcept { fin_ = fin; }
        void set_rsv1(bool rsv1) noexcept { rsv1_ = rsv1; }
        void set_rsv2(bool rsv2) noexcept { rsv2_ = rsv2; }
        void set_rsv3(bool rsv3) noexcept { rsv3_ = rsv3; }
        void set_opcode(WebSocketOpcode opcode) noexcept { opcode_ = opcode; }
        void set_masked(bool masked) noexcept { masked_ = masked; }
        void set_payload(const buffer_t& payload);
        void set_payload(buffer_t&& payload);
        void set_masking_key(const array<byte_t, 4>& key) { masking_key_ = key; }
        
        bool is_control_frame() const noexcept;
        bool is_data_frame() const noexcept;
        bool is_continuation_frame() const noexcept;
        bool is_text_frame() const noexcept;
        bool is_binary_frame() const noexcept;
        bool is_close_frame() const noexcept;
        bool is_ping_frame() const noexcept;
        bool is_pong_frame() const noexcept;
        
        buffer_t serialize() const;
        bool deserialize(const buffer_t& data);
        bool deserialize(byte_span data);
        
        static optional<Frame> parse(const buffer_t& data);
        static optional<Frame> parse(byte_span data);
        
        size_type serialized_size() const;
        size_type header_size() const;
        
        void apply_mask();
        void remove_mask();
        void generate_masking_key();
        
        bool validate() const;
        string to_string() const;
        
        static Frame create_text_frame(string_view text, bool fin = true, bool masked = false);
        static Frame create_binary_frame(const buffer_t& data, bool fin = true, bool masked = false);
        static Frame create_close_frame(uint16_t code = 1000, string_view reason = "", bool masked = false);
        static Frame create_ping_frame(const buffer_t& payload = {}, bool masked = false);
        static Frame create_pong_frame(const buffer_t& payload = {}, bool masked = false);
        static Frame create_continuation_frame(const buffer_t& data, bool fin = false, bool masked = false);
        
        static size_type calculate_header_size(uint64_t payload_length, bool masked);
        static bool is_valid_opcode(uint8_t opcode);
        static bool is_control_opcode(WebSocketOpcode opcode);
        static bool is_data_opcode(WebSocketOpcode opcode);
        
        static const size_type MAX_CONTROL_FRAME_PAYLOAD_SIZE = 125;
        static const size_type SMALL_PAYLOAD_THRESHOLD = 125;
        static const size_type MEDIUM_PAYLOAD_THRESHOLD = 65535;
        
    private:
        bool fin_{true};
        bool rsv1_{false};
        bool rsv2_{false};
        bool rsv3_{false};
        WebSocketOpcode opcode_{WebSocketOpcode::TEXT};
        bool masked_{false};
        uint64_t payload_length_{0};
        array<byte_t, 4> masking_key_{};
        buffer_t payload_;
        
        void update_payload_length();
        void mask_payload();
        void unmask_payload();
        
        static array<byte_t, 4> generate_random_masking_key();
        static void apply_masking_key(buffer_t& data, const array<byte_t, 4>& key);
    };
    
    class FrameBuilder {
    public:
        FrameBuilder() = default;
        
        FrameBuilder& fin(bool fin) { frame_.set_fin(fin); return *this; }
        FrameBuilder& rsv1(bool rsv1) { frame_.set_rsv1(rsv1); return *this; }
        FrameBuilder& rsv2(bool rsv2) { frame_.set_rsv2(rsv2); return *this; }
        FrameBuilder& rsv3(bool rsv3) { frame_.set_rsv3(rsv3); return *this; }
        FrameBuilder& opcode(WebSocketOpcode opcode) { frame_.set_opcode(opcode); return *this; }
        FrameBuilder& masked(bool masked) { frame_.set_masked(masked); return *this; }
        FrameBuilder& payload(const buffer_t& payload) { frame_.set_payload(payload); return *this; }
        FrameBuilder& payload(buffer_t&& payload) { frame_.set_payload(std::move(payload)); return *this; }
        FrameBuilder& text_payload(string_view text);
        FrameBuilder& masking_key(const array<byte_t, 4>& key) { frame_.set_masking_key(key); return *this; }
        FrameBuilder& random_masking_key();
        
        Frame build() const { return frame_; }
        Frame build_and_reset();
        
    private:
        Frame frame_;
    };
    
    class FrameParser {
    public:
        enum class ParseResult {
            SUCCESS,
            INCOMPLETE,
            INVALID_HEADER,
            INVALID_OPCODE,
            INVALID_PAYLOAD_LENGTH,
            PAYLOAD_TOO_LARGE,
            CONTROL_FRAME_TOO_LARGE,
            FRAGMENTED_CONTROL_FRAME,
            INVALID_UTF8,
            PROTOCOL_ERROR
        };
        
        FrameParser() = default;
        
        ParseResult parse(byte_span data, Frame& frame, size_type& bytes_consumed);
        ParseResult parse_header(byte_span data, size_type& header_size);
        
        void reset();
        bool is_parsing() const noexcept { return parsing_state_ != ParsingState::READY; }
        
        void set_max_payload_size(size_type max_size) { max_payload_size_ = max_size; }
        size_type max_payload_size() const noexcept { return max_payload_size_; }
        
        void set_strict_mode(bool strict) { strict_mode_ = strict; }
        bool is_strict_mode() const noexcept { return strict_mode_; }
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
    private:
        enum class ParsingState {
            READY,
            READING_HEADER,
            READING_EXTENDED_LENGTH,
            READING_MASKING_KEY,
            READING_PAYLOAD
        };
        
        ParsingState parsing_state_{ParsingState::READY};
        Frame current_frame_;
        buffer_t parsing_buffer_;
        size_type bytes_needed_{0};
        size_type max_payload_size_{1024 * 1024};
        bool strict_mode_{true};
        
        atomic<size_type> frames_parsed_{0};
        atomic<size_type> bytes_parsed_{0};
        atomic<size_type> parse_errors_{0};
        
        ParseResult parse_frame_header(byte_span data, size_type& bytes_consumed);
        ParseResult parse_extended_length(byte_span data, size_type& bytes_consumed);
        ParseResult parse_masking_key(byte_span data, size_type& bytes_consumed);
        ParseResult parse_payload(byte_span data, size_type& bytes_consumed);
        
        bool validate_frame_header() const;
        bool validate_payload() const;
        bool is_valid_utf8(const buffer_t& data) const;
        
        void transition_to_state(ParsingState new_state, size_type bytes_needed = 0);
        void handle_parse_error(ParseResult error);
    };
    
    class FrameSerializer {
    public:
        static buffer_t serialize(const Frame& frame);
        static bool serialize(const Frame& frame, buffer_t& output);
        static bool serialize_to_buffer(const Frame& frame, mutable_byte_span buffer, size_type& bytes_written);
        
        static size_type calculate_serialized_size(const Frame& frame);
        static size_type calculate_header_size(const Frame& frame);
        
        static bool validate_before_serialization(const Frame& frame);
        
    private:
        static void serialize_header(const Frame& frame, buffer_t& output);
        static void serialize_payload_length(uint64_t length, bool masked, buffer_t& output);
        static void serialize_masking_key(const array<byte_t, 4>& key, buffer_t& output);
        static void serialize_payload(const buffer_t& payload, buffer_t& output);
    };
} 