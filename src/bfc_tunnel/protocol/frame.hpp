#ifndef BFC_TUNNEL_PROTOCOL_FRAME_HPP
#define BFC_TUNNEL_PROTOCOL_FRAME_HPP

#include <bfc/buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

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

// MAC length in bytes for a given integrity algorithm. Table is implementation-defined.
inline size_t mac_size(uint8_t inte_algo)
{
    (void)inte_algo;
    return 0;
}

template <bool IsConst>
class basic_frame_t
{
public:
    static constexpr size_t k_fixed_header_size = 20;

    using byte_ptr = std::conditional_t<IsConst, const uint8_t*, uint8_t*>;

    basic_frame_t() = default;
    basic_frame_t(byte_ptr base, size_t size, size_t mac_sz = 0)
        : base_(base)
        , max_size_(size)
        , mac_size_(mac_sz)
    {}

    void rebase(byte_ptr base, size_t size)
    {
        base_ = base;
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
        base_[k_byte_sec_ctx] = sec_ctx;
    }

    uint8_t get_sec_ctx() const
    {
        return base_[k_byte_sec_ctx];
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ttl(uint8_t ttl)
    {
        base_[k_byte_ttl] = ttl;
    }

    uint8_t get_ttl() const
    {
        return base_[k_byte_ttl];
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_conf_algo(uint8_t v)
    {
        set_bits(k_byte_algo, k_mask_conf_algo, k_shift_conf_algo, v);
    }

    uint8_t get_conf_algo() const
    {
        return get_bits(k_byte_algo, k_mask_conf_algo, k_shift_conf_algo);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_inte_algo(uint8_t v)
    {
        set_bits(k_byte_algo, k_mask_inte_algo, k_shift_inte_algo, v);
    }

    uint8_t get_inte_algo() const
    {
        return get_bits(k_byte_algo, k_mask_inte_algo, k_shift_inte_algo);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_sn(uint32_t sn)
    {
        write_u32(k_byte_sn, sn);
    }

    uint32_t get_sn() const
    {
        return read_u32(k_byte_sn);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ts(uint32_t ts)
    {
        write_u32(k_byte_ts, ts);
    }

    uint32_t get_ts() const
    {
        return read_u32(k_byte_ts);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_src(uint32_t src)
    {
        write_u32(k_byte_src, src);
    }

    uint32_t get_src() const
    {
        return read_u32(k_byte_src);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_dst(uint32_t dst)
    {
        write_u32(k_byte_dst, dst);
    }

    uint32_t get_dst() const
    {
        return read_u32(k_byte_dst);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_mac_size(size_t mac_sz)
    {
        mac_size_ = mac_sz;
    }

    size_t get_mac_size() const
    {
        return mac_size_;
    }

    byte_ptr get_mac() const
    {
        return base_ + k_byte_mac;
    }

    byte_ptr get_payload() const
    {
        return base_ + get_header_size();
    }

    size_t get_header_size() const
    {
        return k_fixed_header_size + mac_size_;
    }

    size_t get_payload_size() const
    {
        if (max_size_ < get_header_size())
        {
            return 0;
        }
        return max_size_ - get_header_size();
    }

    bool is_header_valid() const
    {
        return base_ != nullptr && max_size_ >= get_header_size();
    }

private:
    static constexpr size_t k_byte_version_type = 0;
    static constexpr size_t k_byte_sec_ctx      = 1;
    static constexpr size_t k_byte_ttl          = 2;
    static constexpr size_t k_byte_algo         = 3;
    static constexpr size_t k_byte_sn           = 4;
    static constexpr size_t k_byte_ts           = 8;
    static constexpr size_t k_byte_src          = 12;
    static constexpr size_t k_byte_dst          = 16;
    static constexpr size_t k_byte_mac          = 20;

    static constexpr uint8_t k_mask_version    = 0b01111100;
    static constexpr uint8_t k_mask_frame_type = 0b00000011;
    static constexpr uint8_t k_shift_version   = 2;
    static constexpr uint8_t k_shift_frame_type = 0;

    static constexpr uint8_t k_mask_conf_algo  = 0b11110000;
    static constexpr uint8_t k_mask_inte_algo  = 0b00001111;
    static constexpr uint8_t k_shift_conf_algo = 4;
    static constexpr uint8_t k_shift_inte_algo = 0;

    uint8_t get_bits(size_t index, uint8_t mask, uint8_t shift) const
    {
        return (base_[index] & mask) >> shift;
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_bits(size_t index, uint8_t mask, uint8_t shift, uint8_t value)
    {
        uint8_t byte = base_[index] & static_cast<uint8_t>(~mask);
        byte |= static_cast<uint8_t>((value << shift) & mask);
        base_[index] = byte;
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
        base_[offset]     = static_cast<uint8_t>((value >> 24) & 0xFF);
        base_[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        base_[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        base_[offset + 3] = static_cast<uint8_t>(value & 0xFF);
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

    if (frame.get_mac_size() != mac_size(frame.get_inte_algo()))
    {
        return false;
    }

    return true;
}

template<typename BufferView>
inline bool validate_frame_buffer(const BufferView& buffer, size_t mac_sz = 0)
{
    if (buffer.size() < frame_const_t::k_fixed_header_size + mac_sz)
    {
        return false;
    }

    frame_const_t frame(
        reinterpret_cast<const uint8_t*>(buffer.data()),
        buffer.size(),
        mac_sz
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
