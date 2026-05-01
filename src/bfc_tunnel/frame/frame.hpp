#ifndef BFC_TUNNEL_FRAME_FRAME_HPP
#define BFC_TUNNEL_FRAME_FRAME_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace bfc_tunnel
{

enum frame_type_e
{
    PEER = 0,
    NETWORK = 1,
    NETWORK_OVER_PEER = 2,
    PUBLIC = 3
};

template <bool IsConst>
class basic_frame_t
{
public:
    using byte_ptr = std::conditional_t<IsConst, const uint8_t*, uint8_t*>;

    basic_frame_t() = default;
    basic_frame_t(byte_ptr base, size_t size, size_t mac_size = 0)
        : base_(base)
        , max_size_(size)
        , mac_size_(mac_size)
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

    // template <bool B = IsConst, typename = std::enable_if_t<!B>>
    // void set_reserved(uint8_t v)
    // {
    //     set_bits(kByteVersionType, kMaskReserved, kShiftReserved, v);
    // }

    // uint8_t get_reserved() const
    // {
    //     return get_bits(kByteVersionType, kMaskReserved, kShiftReserved);
    // }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_version(uint8_t v)
    {
        set_bits(kByteVersionType, kMaskVersion, kShiftVersion, v);
    }

    uint8_t get_version() const
    {
        return get_bits(kByteVersionType, kMaskVersion, kShiftVersion);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_frame_type(frame_type_e t)
    {
        set_bits(kByteVersionType, kMaskFrameType, kShiftFrameType, static_cast<uint8_t>(t));
    }

    frame_type_e get_frame_type() const
    {
        return static_cast<frame_type_e>(get_bits(kByteVersionType, kMaskFrameType, kShiftFrameType));
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_sec_ctx(uint8_t sec_ctx)
    {
        base_[kByteSecCtx] = sec_ctx;
    }

    uint8_t get_sec_ctx() const
    {
        return base_[kByteSecCtx];
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ttl(uint8_t ttl)
    {
        base_[kByteTtl] = ttl;
    }

    uint8_t get_ttl() const
    {
        return base_[kByteTtl];
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_conf_algo(uint8_t v)
    {
        set_bits(kByteAlgo, kMaskConfAlgo, kShiftConfAlgo, v);
    }

    uint8_t get_conf_algo() const
    {
        return get_bits(kByteAlgo, kMaskConfAlgo, kShiftConfAlgo);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_inte_algo(uint8_t v)
    {
        set_bits(kByteAlgo, kMaskInteAlgo, kShiftInteAlgo, v);
    }

    uint8_t get_inte_algo() const
    {
        return get_bits(kByteAlgo, kMaskInteAlgo, kShiftInteAlgo);
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_sn(uint32_t sn)
    {
        write_u32(get_byte_sn(), sn);
    }

    uint32_t get_sn() const
    {
        return read_u32(get_byte_sn());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_ts(uint32_t ts)
    {
        write_u32(get_byte_ts(), ts);
    }

    uint32_t get_ts() const
    {
        return read_u32(get_byte_ts());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_src(uint32_t src)
    {
        write_u32(get_byte_src(), src);
    }

    uint32_t get_src() const
    {
        return read_u32(get_byte_src());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_dst(uint32_t dst)
    {
        write_u32(get_byte_dst(), dst);
    }

    uint32_t get_dst() const
    {
        return read_u32(get_byte_dst());
    }

    template <bool B = IsConst, typename = std::enable_if_t<!B>>
    void set_mac_size(size_t mac_size)
    {
        mac_size_ = mac_size;
    }

    size_t get_mac_size() const
    {
        return mac_size_;
    }

    byte_ptr get_mac() const
    {
        return base_ + kByteMac;
    }

    byte_ptr get_payload() const
    {
        return base_ + get_header_size();
    }

    size_t get_header_size() const
    {
        return kFixedHeaderSize + mac_size_;
    }

    size_t get_payload_size() const
    {
        return max_size_ - get_header_size();
    }

    bool is_header_valid() const
    {
        return max_size_ >= get_header_size();
    }

private:
    static constexpr size_t kByteVersionType = 0;
    static constexpr size_t kByteSecCtx      = 1;
    static constexpr size_t kByteTtl         = 2;
    static constexpr size_t kByteAlgo        = 3;
    static constexpr size_t kByteSn          = 4;
    static constexpr size_t kByteTs          = 8;
    static constexpr size_t kByteSrc         = 12;
    static constexpr size_t kByteDst         = 16;
    static constexpr size_t kByteMac         = 20;
    static constexpr size_t kFixedHeaderSize = 20;

    size_t get_byte_sn() const { return kByteSn; }
    size_t get_byte_ts() const { return kByteTs; }
    size_t get_byte_src() const { return kByteSrc; }
    size_t get_byte_dst() const { return kByteDst; }

    // static constexpr uint8_t kMaskReserved   = 0b10000000;
    static constexpr uint8_t kMaskVersion    = 0b01111100;
    static constexpr uint8_t kMaskFrameType  = 0b00000011;

    // static constexpr uint8_t kShiftReserved  = 7;
    static constexpr uint8_t kShiftVersion   = 2;
    static constexpr uint8_t kShiftFrameType = 0;

    static constexpr uint8_t kMaskConfAlgo   = 0b11110000;
    static constexpr uint8_t kMaskInteAlgo   = 0b00001111;

    static constexpr uint8_t kShiftConfAlgo  = 4;
    static constexpr uint8_t kShiftInteAlgo  = 0;

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

    byte_ptr base_    = nullptr;
    size_t max_size_  = 0;
    size_t mac_size_  = 0;
};

using frame_t       = basic_frame_t<false>;
using frame_const_t = basic_frame_t<true>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_FRAME_FRAME_HPP
