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
    VERIFICATION_FAILED,
    UKNOWN_PEER
};

constexpr auto K_BEACON_FLAG_HAS_NETWORK_KEY = 1;
struct beacon
{
    u8 flags;
};

struct msg1
{
    u8 id;
    u8 sec_ctx;
    u8 integrity_algorithm;
    u8 confidentiality_algorithm;
    u8 dh_key_type;
    key_t ephemeral;
    u64 duration_s;
    u64 priority;
};

struct msg2
{
    u8 id;
    status_e status;
    key_t ephemeral;
};

struct query_network_security
{
    u8 id;
};

struct network_key_information
{
    u8 sec_ctx;
    u64 priority;
    u64 expiration_time_s;
};

using network_key_informations = cum::vector<network_key_information, 256>;
struct network_security_information
{
    u8 id;
    u8 current_page;
    u8 total_page;
    network_key_informations informations;
};

struct network_key
{
    u8 sec_ctx;
    u64 priority;
    u64 expiration_time_s;
    u8 integrity_algorithm;
    key_t integrity_key;
    u8 confidentiality_algorithm;
    key_t confidentiality_key;
};

using network_keys = cum::vector<network_key, 256>;
struct network_key_refresh
{
    network_keys keys;
};

struct network_keys_request
{
    u8 id;
};

struct network_keys_response
{
    u8 id;
    u8 current_page;
    u8 total_page;
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
    if (status_e::UKNOWN_PEER == pIe) pCtx += "\"UKNOWN_PEER\"";
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const beacon& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.flags, pCtx);
}

inline void decode_per(beacon& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.flags, pCtx);
}

inline void str(const char* pName, const beacon& pIe, std::string& pCtx, bool pIsLast)
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
    size_t nMandatory = 1;
    str("flags", pIe.flags, pCtx, !(--nMandatory+nOptional));
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
    encode_per(pIe.duration_s, pCtx);
    encode_per(pIe.priority, pCtx);
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
    decode_per(pIe.duration_s, pCtx);
    decode_per(pIe.priority, pCtx);
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
    size_t nMandatory = 8;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    str("sec_ctx", pIe.sec_ctx, pCtx, !(--nMandatory+nOptional));
    str("integrity_algorithm", pIe.integrity_algorithm, pCtx, !(--nMandatory+nOptional));
    str("confidentiality_algorithm", pIe.confidentiality_algorithm, pCtx, !(--nMandatory+nOptional));
    str("dh_key_type", pIe.dh_key_type, pCtx, !(--nMandatory+nOptional));
    str("ephemeral", pIe.ephemeral, pCtx, !(--nMandatory+nOptional));
    str("duration_s", pIe.duration_s, pCtx, !(--nMandatory+nOptional));
    str("priority", pIe.priority, pCtx, !(--nMandatory+nOptional));
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
}

inline void decode_per(msg2& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
    decode_per(pIe.status, pCtx);
    decode_per(pIe.ephemeral, pCtx);
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
    size_t nMandatory = 3;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    str("status", pIe.status, pCtx, !(--nMandatory+nOptional));
    str("ephemeral", pIe.ephemeral, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const query_network_security& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
}

inline void decode_per(query_network_security& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
}

inline void str(const char* pName, const query_network_security& pIe, std::string& pCtx, bool pIsLast)
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
    size_t nMandatory = 1;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_key_information& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.sec_ctx, pCtx);
    encode_per(pIe.priority, pCtx);
    encode_per(pIe.expiration_time_s, pCtx);
}

inline void decode_per(network_key_information& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.sec_ctx, pCtx);
    decode_per(pIe.priority, pCtx);
    decode_per(pIe.expiration_time_s, pCtx);
}

inline void str(const char* pName, const network_key_information& pIe, std::string& pCtx, bool pIsLast)
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
    str("sec_ctx", pIe.sec_ctx, pCtx, !(--nMandatory+nOptional));
    str("priority", pIe.priority, pCtx, !(--nMandatory+nOptional));
    str("expiration_time_s", pIe.expiration_time_s, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_security_information& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
    encode_per(pIe.current_page, pCtx);
    encode_per(pIe.total_page, pCtx);
    encode_per(pIe.informations, pCtx);
}

inline void decode_per(network_security_information& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
    decode_per(pIe.current_page, pCtx);
    decode_per(pIe.total_page, pCtx);
    decode_per(pIe.informations, pCtx);
}

inline void str(const char* pName, const network_security_information& pIe, std::string& pCtx, bool pIsLast)
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
    str("current_page", pIe.current_page, pCtx, !(--nMandatory+nOptional));
    str("total_page", pIe.total_page, pCtx, !(--nMandatory+nOptional));
    str("informations", pIe.informations, pCtx, !(--nMandatory+nOptional));
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
    encode_per(pIe.priority, pCtx);
    encode_per(pIe.expiration_time_s, pCtx);
    encode_per(pIe.integrity_algorithm, pCtx);
    encode_per(pIe.integrity_key, pCtx);
    encode_per(pIe.confidentiality_algorithm, pCtx);
    encode_per(pIe.confidentiality_key, pCtx);
}

inline void decode_per(network_key& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.sec_ctx, pCtx);
    decode_per(pIe.priority, pCtx);
    decode_per(pIe.expiration_time_s, pCtx);
    decode_per(pIe.integrity_algorithm, pCtx);
    decode_per(pIe.integrity_key, pCtx);
    decode_per(pIe.confidentiality_algorithm, pCtx);
    decode_per(pIe.confidentiality_key, pCtx);
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
    size_t nMandatory = 7;
    str("sec_ctx", pIe.sec_ctx, pCtx, !(--nMandatory+nOptional));
    str("priority", pIe.priority, pCtx, !(--nMandatory+nOptional));
    str("expiration_time_s", pIe.expiration_time_s, pCtx, !(--nMandatory+nOptional));
    str("integrity_algorithm", pIe.integrity_algorithm, pCtx, !(--nMandatory+nOptional));
    str("integrity_key", pIe.integrity_key, pCtx, !(--nMandatory+nOptional));
    str("confidentiality_algorithm", pIe.confidentiality_algorithm, pCtx, !(--nMandatory+nOptional));
    str("confidentiality_key", pIe.confidentiality_key, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_key_refresh& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.keys, pCtx);
}

inline void decode_per(network_key_refresh& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.keys, pCtx);
}

inline void str(const char* pName, const network_key_refresh& pIe, std::string& pCtx, bool pIsLast)
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
    size_t nMandatory = 1;
    str("keys", pIe.keys, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_keys_request& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
}

inline void decode_per(network_keys_request& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
}

inline void str(const char* pName, const network_keys_request& pIe, std::string& pCtx, bool pIsLast)
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
    size_t nMandatory = 1;
    str("id", pIe.id, pCtx, !(--nMandatory+nOptional));
    pCtx = pCtx + "}";
    if (!pIsLast)
    {
        pCtx += ",";
    }
}

inline void encode_per(const network_keys_response& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    encode_per(pIe.id, pCtx);
    encode_per(pIe.current_page, pCtx);
    encode_per(pIe.total_page, pCtx);
    encode_per(pIe.keys, pCtx);
}

inline void decode_per(network_keys_response& pIe, cum::per_codec_ctx& pCtx)
{
    using namespace cum;
    decode_per(pIe.id, pCtx);
    decode_per(pIe.current_page, pCtx);
    decode_per(pIe.total_page, pCtx);
    decode_per(pIe.keys, pCtx);
}

inline void str(const char* pName, const network_keys_response& pIe, std::string& pCtx, bool pIsLast)
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
    str("current_page", pIe.current_page, pCtx, !(--nMandatory+nOptional));
    str("total_page", pIe.total_page, pCtx, !(--nMandatory+nOptional));
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
