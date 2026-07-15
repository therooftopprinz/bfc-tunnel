#ifndef BFC_TUNNEL_PROTOCOL_FRAME_HPP
#define BFC_TUNNEL_PROTOCOL_FRAME_HPP

#include <bfc/buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cum
{
struct msg1;
struct msg2;
struct beacon;
struct query_network_security;
struct network_security_information;
struct network_keys_request;
struct network_keys_response;
struct network_key_refresh;
struct link_info;
struct link_report;
struct route_announce;
struct n2n_indication;
} // namespace cum

namespace bfc_tunnel
{

constexpr uint8_t k_frame_protocol_version = 1;

enum frame_type_e
{
    E_FRAME_TYPE_PEER              = 0,
    E_FRAME_TYPE_NETWORK           = 1,
    E_FRAME_TYPE_NETWORK_OVER_PEER = 2,
    E_FRAME_TYPE_PUBLIC            = 3,
};

enum payload_type_e
{
    E_PAYLOAD_TYPE_BEACON                       = 0x00,
    E_PAYLOAD_TYPE_MSG1                         = 0x01,
    E_PAYLOAD_TYPE_MSG2                         = 0x02,
    E_PAYLOAD_TYPE_QUERY_NETWORK_SECURITY       = 0x03,
    E_PAYLOAD_TYPE_NETWORK_SECURITY_INFORMATION = 0x04,
    E_PAYLOAD_TYPE_NETWORK_KEYS_REQUEST         = 0x05,
    E_PAYLOAD_TYPE_NETWORK_KEYS_RESPONSE        = 0x06,
    E_PAYLOAD_TYPE_NETWORK_KEY_REFRESH          = 0x07,
    E_PAYLOAD_TYPE_LINK_INFO                    = 0x08,
    E_PAYLOAD_TYPE_LINK_REPORT                  = 0x09,
    E_PAYLOAD_TYPE_ROUTE_ANNOUNCE               = 0x0A,
    E_PAYLOAD_TYPE_N2N_INDICATION               = 0x0B,
    E_PAYLOAD_TYPE_TUNNEL_DATA                  = 0x0C,
};

template<typename T> struct msg_id;

template<> struct msg_id<::cum::msg1>                         {static constexpr payload_type_e value = E_PAYLOAD_TYPE_MSG1;};
template<> struct msg_id<::cum::msg2>                         {static constexpr payload_type_e value = E_PAYLOAD_TYPE_MSG2;};
template<> struct msg_id<::cum::beacon>                       {static constexpr payload_type_e value = E_PAYLOAD_TYPE_BEACON;};
template<> struct msg_id<::cum::query_network_security>       {static constexpr payload_type_e value = E_PAYLOAD_TYPE_QUERY_NETWORK_SECURITY;};
template<> struct msg_id<::cum::network_security_information> {static constexpr payload_type_e value = E_PAYLOAD_TYPE_NETWORK_SECURITY_INFORMATION;};
template<> struct msg_id<::cum::network_keys_request>         {static constexpr payload_type_e value = E_PAYLOAD_TYPE_NETWORK_KEYS_REQUEST;};
template<> struct msg_id<::cum::network_keys_response>        {static constexpr payload_type_e value = E_PAYLOAD_TYPE_NETWORK_KEYS_RESPONSE;};
template<> struct msg_id<::cum::network_key_refresh>          {static constexpr payload_type_e value = E_PAYLOAD_TYPE_NETWORK_KEY_REFRESH;};
template<> struct msg_id<::cum::link_info>                    {static constexpr payload_type_e value = E_PAYLOAD_TYPE_LINK_INFO;};
template<> struct msg_id<::cum::link_report>                  {static constexpr payload_type_e value = E_PAYLOAD_TYPE_LINK_REPORT;};
template<> struct msg_id<::cum::route_announce>               {static constexpr payload_type_e value = E_PAYLOAD_TYPE_ROUTE_ANNOUNCE;};
template<> struct msg_id<::cum::n2n_indication>               {static constexpr payload_type_e value = E_PAYLOAD_TYPE_N2N_INDICATION;};

enum integrity_algorithm_e
{
    E_EA_NONE             = 0,
    E_EA_HMAC_SHA2_512    = 1,
    E_EA_HMAC_SHA2_256    = 2,
    E_EA_HMAC_BLAKE3      = 3,
    E_EA_MAX              = 4
};

enum confidentiality_algorithm_e
{
    E_CA_NONE       = 0,
    E_CA_AES128     = 1,
    E_CA_AES256     = 2,
    E_CA_CHACHA20   = 3,
    E_CA_MAX        = 4
};

enum dh_key_type_e
{
    E_DHKT_NONE       = 0,
    E_DHKT_X25519     = 1,
    E_DHKT_SECP256R1  = 2,
    E_DHKT_CURVE448   = 3,
    E_DHKT_MAX        = 4
};

constexpr uint8_t k_sec_ctx_none = 0;

inline size_t integrity_mac_size(uint8_t int_algo)
{
    switch (int_algo)
    {
        case E_EA_NONE:
            return 0;
        case E_EA_HMAC_SHA2_512:
            return 64;
        case E_EA_HMAC_SHA2_256:
            return 32;
        case E_EA_HMAC_BLAKE3:
            return 64;
    }
    return 0;
}

inline size_t integrity_key_size(uint8_t int_algo)
{
    switch (int_algo)
    {
        case E_EA_NONE:
            return 0;
        case E_EA_HMAC_SHA2_512:
            return 64;
        case E_EA_HMAC_SHA2_256:
            return 32;
        case E_EA_HMAC_BLAKE3:
            return 32;
    }
    return 0;
}

inline size_t confidentiality_key_size(uint8_t conf_algo)
{
    switch (conf_algo)
    {
        case E_CA_NONE:
            return 0;
        case E_CA_AES128:
            return 16;
        case E_CA_AES256:
            return 32;
        case E_CA_CHACHA20:
            return 32;
    }
    return 0;
}

template <bool IsConst>
class basic_frame_t
{
public:
    static constexpr size_t k_fixed_prefix_size     = 3;
    static constexpr size_t k_fixed_post_mac_size     = 16;
    static constexpr size_t k_fixed_header_size       = k_fixed_prefix_size + k_fixed_post_mac_size;
    static constexpr size_t k_payload_type_size       = 1;

    using byte_ptr = std::conditional_t<IsConst, const std::byte*, std::byte*>;

    basic_frame_t() = default;
    basic_frame_t(byte_ptr base, size_t size)
        : base_(base)
        , max_size_(size)
    {
        if (base != nullptr && size >= k_fixed_prefix_size)
        {
            mac_size_ = static_cast<size_t>(
                (static_cast<uint8_t>(base[k_byte_sec_mac]) & k_mask_mac_size) >> k_shift_mac_size) * 4;
        }
    }

    void rebase(byte_ptr base, size_t size)
    {
        base_ = base;
        max_size_ = size;
    }

    void resize(size_t size)
    {
        max_size_ = size;
    }

    byte_ptr get_base() const
    {
        return base_;
    }

    size_t get_max_size() const
    {
        return max_size_;
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ttl(uint8_t ttl)
    {
        base_[k_byte_ttl] = static_cast<std::byte>(ttl);
    }

    uint8_t get_ttl() const
    {
        return static_cast<uint8_t>(base_[k_byte_ttl]);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_reserved(uint8_t v)
    {
        set_bits(k_byte_version_type, k_mask_reserved, k_shift_reserved, v);
    }

    uint8_t get_reserved() const
    {
        return get_bits(k_byte_version_type, k_mask_reserved, k_shift_reserved);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_version(uint8_t v)
    {
        set_bits(k_byte_version_type, k_mask_version, k_shift_version, v);
    }

    uint8_t get_version() const
    {
        return get_bits(k_byte_version_type, k_mask_version, k_shift_version);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_frame_type(frame_type_e t)
    {
        set_bits(k_byte_version_type, k_mask_frame_type, k_shift_frame_type, static_cast<uint8_t>(t));
    }

    frame_type_e get_frame_type() const
    {
        return static_cast<frame_type_e>(get_bits(k_byte_version_type, k_mask_frame_type, k_shift_frame_type));
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_sec_ctx(uint8_t sec_ctx)
    {
        set_bits(k_byte_sec_mac, k_mask_sec_ctx, k_shift_sec_ctx, sec_ctx);
    }

    uint8_t get_sec_ctx() const
    {
        return get_bits(k_byte_sec_mac, k_mask_sec_ctx, k_shift_sec_ctx);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_mac_size_units(uint8_t units)
    {
        set_bits(k_byte_sec_mac, k_mask_mac_size, k_shift_mac_size, units);
        mac_size_ = static_cast<size_t>(units) * 4;
    }

    uint8_t get_mac_size_units() const
    {
        return get_bits(k_byte_sec_mac, k_mask_mac_size, k_shift_mac_size);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_sn(uint32_t sn)
    {
        write_u32(sn_offset(), sn);
    }

    uint32_t get_sn() const
    {
        return read_u32(sn_offset());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ts(uint32_t ts)
    {
        write_u32(ts_offset(), ts);
    }

    uint32_t get_ts() const
    {
        return read_u32(ts_offset());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_src(uint32_t src)
    {
        write_u32(src_offset(), src);
    }

    uint32_t get_src() const
    {
        return read_u32(src_offset());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_dst(uint32_t dst)
    {
        write_u32(dst_offset(), dst);
    }

    uint32_t get_dst() const
    {
        return read_u32(dst_offset());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_mac_size(size_t mac_sz)
    {
        set_mac_size_units(static_cast<uint8_t>(mac_sz / 4));
    }

    size_t get_mac_size() const
    {
        return mac_size_;
    }

    byte_ptr get_mac() const
    {
        return base_ + k_byte_mac;
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_payload_type(payload_type_e type)
    {
        base_[payload_type_offset()] = static_cast<std::byte>(type);
    }

    payload_type_e get_payload_type() const
    {
        return static_cast<payload_type_e>(static_cast<uint8_t>(base_[payload_type_offset()]));
    }

    byte_ptr get_payload() const
    {
        return base_ + get_header_size();
    }

    size_t get_header_size() const
    {
        return k_fixed_header_size + mac_size_ + k_payload_type_size;
    }

    size_t get_payload_size() const
    {
        if (max_size_ < get_header_size())
        {
            return 0;
        }
        return max_size_ - get_header_size();
    }

    size_t get_size() const
    {
        return get_header_size() + get_payload_size();
    }

    bool is_header_valid() const
    {
        return base_ != nullptr && max_size_ >= get_header_size();
    }

    size_t sn_offset() const
    {
        return k_fixed_prefix_size + mac_size_;
    }

    size_t ts_offset() const
    {
        return sn_offset() + 4;
    }

    size_t src_offset() const
    {
        return ts_offset() + 4;
    }

    size_t dst_offset() const
    {
        return src_offset() + 4;
    }

    size_t payload_type_offset() const
    {
        return dst_offset() + 4;
    }

private:
    static constexpr size_t k_byte_ttl          = 0;
    static constexpr size_t k_byte_version_type = 1;
    static constexpr size_t k_byte_sec_mac      = 2;
    static constexpr size_t k_byte_mac          = 3;

    static constexpr uint8_t k_mask_reserved    = 0b10000000;
    static constexpr uint8_t k_mask_version     = 0b01111100;
    static constexpr uint8_t k_mask_frame_type  = 0b00000011;
    static constexpr uint8_t k_shift_reserved   = 7;
    static constexpr uint8_t k_shift_version    = 2;
    static constexpr uint8_t k_shift_frame_type = 0;

    static constexpr uint8_t k_mask_sec_ctx     = 0b11110000;
    static constexpr uint8_t k_mask_mac_size    = 0b00001111;
    static constexpr uint8_t k_shift_sec_ctx    = 4;
    static constexpr uint8_t k_shift_mac_size   = 0;

    uint8_t get_bits(size_t index, uint8_t mask, uint8_t shift) const
    {
        return (static_cast<uint8_t>(base_[index]) & mask) >> shift;
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_bits(size_t index, uint8_t mask, uint8_t shift, uint8_t value)
    {
        uint8_t byte = static_cast<uint8_t>(base_[index]) & static_cast<uint8_t>(~mask);
        byte |= static_cast<uint8_t>((value << shift) & mask);
        base_[index] = static_cast<std::byte>(byte);
    }

    uint32_t read_u32(size_t offset) const
    {
        return (static_cast<uint32_t>(base_[offset]) << 24) |
               (static_cast<uint32_t>(base_[offset + 1]) << 16) |
               (static_cast<uint32_t>(base_[offset + 2]) << 8) |
               static_cast<uint32_t>(base_[offset + 3]);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void write_u32(size_t offset, uint32_t value)
    {
        base_[offset]     = static_cast<std::byte>((value >> 24) & 0xFF);
        base_[offset + 1] = static_cast<std::byte>((value >> 16) & 0xFF);
        base_[offset + 2] = static_cast<std::byte>((value >> 8) & 0xFF);
        base_[offset + 3] = static_cast<std::byte>(value & 0xFF);
    }

    byte_ptr base_   = nullptr;
    size_t max_size_ = 0;
    size_t mac_size_ = 0;
};

using frame_t       = basic_frame_t<false>;
using frame_const_t = basic_frame_t<true>;

inline bool validate_frame(const frame_const_t& frame)
{
    if (!frame.is_header_valid())
    {
        return false;
    }

    if (frame.get_version() != k_frame_protocol_version)
    {
        return false;
    }

    if (frame.get_frame_type() > E_FRAME_TYPE_PUBLIC)
    {
        return false;
    }

    if (frame.get_payload_type() > E_PAYLOAD_TYPE_TUNNEL_DATA)
    {
        return false;
    }

    if (frame.get_mac_size() != static_cast<size_t>(frame.get_mac_size_units()) * 4)
    {
        return false;
    }

    if (frame.get_sec_ctx() == k_sec_ctx_none && frame.get_mac_size() != 0)
    {
        return false;
    }

    return true;
}

template<typename BufferView>
inline bool validate_frame_buffer(const BufferView& buffer)
{
    if (buffer.size() < frame_const_t::k_fixed_prefix_size)
    {
        return false;
    }

    frame_const_t frame(
        reinterpret_cast<const std::byte*>(buffer.data()),
        buffer.size()
    );
    return validate_frame(frame);
}

template<typename BufferView>
inline bfc::const_buffer_view frame_payload_view(const frame_const_t& frame, const BufferView& buffer)
{
    const auto header_size = frame.get_header_size();
    if (buffer.size() < header_size)
    {
        return {};
    }
    return bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(buffer.data()) + header_size,
        buffer.size() - header_size
    );
}

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_PROTOCOL_FRAME_HPP
