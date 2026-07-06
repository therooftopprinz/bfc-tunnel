// Generating for C++
#ifndef __CUM_MSG_HPP__
#define __CUM_MSG_HPP__
#include "cum/cum.hpp"

namespace cum
{

/***********************************************
/
/            Message Definitions
/
************************************************/

using key_t = cum::vector<u8, 256>;
enum status_e
{
    OK,
    UNSUPPORTED,
    VERIFICATION_FAILED
};

struct msg1
{
    u8 id;
    u8 sec_ctx;
    u8 integrity_algorithm;
    u8 confidentiality_algorithm;
    u8 dh_key_type;
    key_t ephemeral;
    key_t signature;
};

struct msg2
{
    u8 id;
    status_e status;
    key_t ephemeral;
    key_t signature;
};

struct network_key
{
    u8 sec_ctx;
    key_t key;
    u64 creation_time_us;
    u16 duration_s;
    u8 integrity_algorithm;
    u8 confidentiality_algorithm;
};

using network_keys = cum::vector<network_key, 256>;
struct exchange_network_keys
{
    u8 record_index;
    u8 record_total;
    network_keys keys;
};

struct link_info
{
    u64 sender_time_us;
    u64 rcv_pkt;
    u64 snt_pkt;
    u64 rcv_byt;
    u64 snt_byt;
};

struct link_report
{
    u64 sender_time_us;
    u16 rx_drop_pct;
};

struct route_announce_entry
{
    u32 origin_node_id;
    u32 next_node_id;
    u32 target_node_id;
    u16 path_metric;
};

using route_announce_entries = cum::vector<route_announce_entry, 256>;
struct route_announce
{
    u16 announcement_number;
    u16 current_page;
    u16 total_page;
    u8 flags;
    route_announce_entries routes;
};

struct n2n_indication
{
    u32 origin;
    u32 target;
    u32 hostv4;
    u16 port;
};

/***********************************************
/
/            Codec Definitions
/
************************************************/

inline void str(const char* pName, const status_e& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (pName)
    {
        pCtx = pCtx + "\"" + pName + "\":";
    }
    if (status_e::OK == pIe) pCtx += "\"OK\"";
    if (status_e::UNSUPPORTED == pIe) pCtx += "\"UNSUPPORTED\"";
    if (status_e::VERIFICATION_FAILED == pIe) pCtx += "\"VERIFICATION_FAILED\"";
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const msg1& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
    encode_per(pIe.sec_ctx, pCtx);
    encode_per(pIe.integrity_algorithm, pCtx);
    encode_per(pIe.confidentiality_algorithm, pCtx);
    encode_per(pIe.dh_key_type, pCtx);
    encode_per(pIe.ephemeral, pCtx);
    encode_per(pIe.signature, pCtx);
}

inline void decode_per(msg1& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
    decode_per(pIe.sec_ctx, pCtx);
    decode_per(pIe.integrity_algorithm, pCtx);
    decode_per(pIe.confidentiality_algorithm, pCtx);
    decode_per(pIe.dh_key_type, pCtx);
    decode_per(pIe.ephemeral, pCtx);
    decode_per(pIe.signature, pCtx);
}

inline void str(const char* pName, const msg1& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 7;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    str("sec_ctx", pIe.sec_ctx, pCtx, !(--nMandatory+nOptional));
    str("integrity_algorithm", pIe.integrity_algorithm, pCtx, !(--nMandatory+nOptional));
    str("confidentiality_algorithm", pIe.confidentiality_algorithm, pCtx, !(--nMandatory+nOptional));
    str("dh_key_type", pIe.dh_key_type, pCtx, !(--nMandatory+nOptional));
    str("ephemeral", pIe.ephemeral, pCtx, !(--nMandatory+nOptional));
    str("signature", pIe.signature, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const msg2& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
    encode_per(pIe.status, pCtx);
    encode_per(pIe.ephemeral, pCtx);
    encode_per(pIe.signature, pCtx);
}

inline void decode_per(msg2& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
    decode_per(pIe.status, pCtx);
    decode_per(pIe.ephemeral, pCtx);
    decode_per(pIe.signature, pCtx);
}

inline void str(const char* pName, const msg2& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 4;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    str("status", pIe.status, pCtx, !(--nMandatory+nOptional));
    str("ephemeral", pIe.ephemeral, pCtx, !(--nMandatory+nOptional));
    str("signature", pIe.signature, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_key& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.sec_ctx, pCtx);
    encode_per(pIe.key, pCtx);
    encode_per(pIe.creation_time_us, pCtx);
    encode_per(pIe.duration_s, pCtx);
    encode_per(pIe.integrity_algorithm, pCtx);
    encode_per(pIe.confidentiality_algorithm, pCtx);
}

inline void decode_per(network_key& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.sec_ctx, pCtx);
    decode_per(pIe.key, pCtx);
    decode_per(pIe.creation_time_us, pCtx);
    decode_per(pIe.duration_s, pCtx);
    decode_per(pIe.integrity_algorithm, pCtx);
    decode_per(pIe.confidentiality_algorithm, pCtx);
}

inline void str(const char* pName, const network_key& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 6;
    str("sec_ctx", pIe.sec_ctx, pCtx, !(--nMandatory+nOptional));
    str("key", pIe.key, pCtx, !(--nMandatory+nOptional));
    str("creation_time_us", pIe.creation_time_us, pCtx, !(--nMandatory+nOptional));
    str("duration_s", pIe.duration_s, pCtx, !(--nMandatory+nOptional));
    str("integrity_algorithm", pIe.integrity_algorithm, pCtx, !(--nMandatory+nOptional));
    str("confidentiality_algorithm", pIe.confidentiality_algorithm, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const exchange_network_keys& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.record_index, pCtx);
    encode_per(pIe.record_total, pCtx);
    encode_per(pIe.keys, pCtx);
}

inline void decode_per(exchange_network_keys& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.record_index, pCtx);
    decode_per(pIe.record_total, pCtx);
    decode_per(pIe.keys, pCtx);
}

inline void str(const char* pName, const exchange_network_keys& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 3;
    str("record_index", pIe.record_index, pCtx, !(--nMandatory+nOptional));
    str("record_total", pIe.record_total, pCtx, !(--nMandatory+nOptional));
    str("keys", pIe.keys, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const link_info& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.sender_time_us, pCtx);
    encode_per(pIe.rcv_pkt, pCtx);
    encode_per(pIe.snt_pkt, pCtx);
    encode_per(pIe.rcv_byt, pCtx);
    encode_per(pIe.snt_byt, pCtx);
}

inline void decode_per(link_info& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.sender_time_us, pCtx);
    decode_per(pIe.rcv_pkt, pCtx);
    decode_per(pIe.snt_pkt, pCtx);
    decode_per(pIe.rcv_byt, pCtx);
    decode_per(pIe.snt_byt, pCtx);
}

inline void str(const char* pName, const link_info& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 5;
    str("sender_time_us", pIe.sender_time_us, pCtx, !(--nMandatory+nOptional));
    str("rcv_pkt", pIe.rcv_pkt, pCtx, !(--nMandatory+nOptional));
    str("snt_pkt", pIe.snt_pkt, pCtx, !(--nMandatory+nOptional));
    str("rcv_byt", pIe.rcv_byt, pCtx, !(--nMandatory+nOptional));
    str("snt_byt", pIe.snt_byt, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const link_report& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.sender_time_us, pCtx);
    encode_per(pIe.rx_drop_pct, pCtx);
}

inline void decode_per(link_report& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.sender_time_us, pCtx);
    decode_per(pIe.rx_drop_pct, pCtx);
}

inline void str(const char* pName, const link_report& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 2;
    str("sender_time_us", pIe.sender_time_us, pCtx, !(--nMandatory+nOptional));
    str("rx_drop_pct", pIe.rx_drop_pct, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const route_announce_entry& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.origin_node_id, pCtx);
    encode_per(pIe.next_node_id, pCtx);
    encode_per(pIe.target_node_id, pCtx);
    encode_per(pIe.path_metric, pCtx);
}

inline void decode_per(route_announce_entry& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.origin_node_id, pCtx);
    decode_per(pIe.next_node_id, pCtx);
    decode_per(pIe.target_node_id, pCtx);
    decode_per(pIe.path_metric, pCtx);
}

inline void str(const char* pName, const route_announce_entry& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 4;
    str("origin_node_id", pIe.origin_node_id, pCtx, !(--nMandatory+nOptional));
    str("next_node_id", pIe.next_node_id, pCtx, !(--nMandatory+nOptional));
    str("target_node_id", pIe.target_node_id, pCtx, !(--nMandatory+nOptional));
    str("path_metric", pIe.path_metric, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const route_announce& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.announcement_number, pCtx);
    encode_per(pIe.current_page, pCtx);
    encode_per(pIe.total_page, pCtx);
    encode_per(pIe.flags, pCtx);
    encode_per(pIe.routes, pCtx);
}

inline void decode_per(route_announce& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.announcement_number, pCtx);
    decode_per(pIe.current_page, pCtx);
    decode_per(pIe.total_page, pCtx);
    decode_per(pIe.flags, pCtx);
    decode_per(pIe.routes, pCtx);
}

inline void str(const char* pName, const route_announce& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 5;
    str("announcement_number", pIe.announcement_number, pCtx, !(--nMandatory+nOptional));
    str("current_page", pIe.current_page, pCtx, !(--nMandatory+nOptional));
    str("total_page", pIe.total_page, pCtx, !(--nMandatory+nOptional));
    str("flags", pIe.flags, pCtx, !(--nMandatory+nOptional));
    str("routes", pIe.routes, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const n2n_indication& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.origin, pCtx);
    encode_per(pIe.target, pCtx);
    encode_per(pIe.hostv4, pCtx);
    encode_per(pIe.port, pCtx);
}

inline void decode_per(n2n_indication& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.origin, pCtx);
    decode_per(pIe.target, pCtx);
    decode_per(pIe.hostv4, pCtx);
    decode_per(pIe.port, pCtx);
}

inline void str(const char* pName, const n2n_indication& pIe, std::string& pCtx, bool pIsLast)
{
    using namespace cum;
    if (!pName)
    {
        pCtx = pCtx + "{";
    }
    else
    {
        pCtx = pCtx + "\"" + pName + "\":{";
    }
    size_t nOptional = 0;
    size_t nMandatory = 4;
    str("origin", pIe.origin, pCtx, !(--nMandatory+nOptional));
    str("target", pIe.target, pCtx, !(--nMandatory+nOptional));
    str("hostv4", pIe.hostv4, pCtx, !(--nMandatory+nOptional));
    str("port", pIe.port, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

} // namespace cum
#endif //__CUM_MSG_HPP__
