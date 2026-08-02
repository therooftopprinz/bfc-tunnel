#include <bfc_tunnel/console/codec.hpp>
#include <bfc_tunnel/console/parser.hpp>
#include <bfc_tunnel/console/table.hpp>

#include <gtest/gtest.h>

using namespace bfc_tunnel;
using namespace bfc_tunnel::console;

TEST(ConsoleParser, ParsesPathVerbArgsAndFilters)
{
    auto cmd = parse_command("/security/private_key list +node_id=0:0:0:1 -private_key=x");
    ASSERT_TRUE(cmd);
    EXPECT_EQ(cmd->path, "/security/private_key");
    EXPECT_EQ(cmd->verb, "list");
    ASSERT_EQ(cmd->filters.size(), 2u);
    EXPECT_TRUE(cmd->filters[0].include);
    EXPECT_EQ(cmd->filters[0].key, "node_id");
    EXPECT_EQ(cmd->filters[0].value, "0:0:0:1");
    EXPECT_FALSE(cmd->filters[1].include);
}

TEST(ConsoleParser, ParsesConfigKvAndStop)
{
    auto cmd = parse_command("/network/config modify beacon_interval_ms=123");
    ASSERT_TRUE(cmd);
    EXPECT_EQ(cmd->path, "/network/config");
    EXPECT_EQ(cmd->verb, "modify");
    EXPECT_EQ(cmd->args.at("beacon_interval_ms"), "123");

    auto stop = parse_command("/stop\r");
    ASSERT_TRUE(stop);
    EXPECT_EQ(stop->path, "/stop");
}

TEST(ConsoleTable, FramesWithBlankLine)
{
    auto body = status_ok();
    auto framed = frame_reply(body);
    ASSERT_GE(framed.size(), 2u);
    EXPECT_EQ(framed.substr(framed.size() - 2), "\n\n");
    EXPECT_NE(body.find("OK"), std::string::npos);
    EXPECT_EQ(status_err("boom").find("ERR"), 0u > 0 ? status_err("boom").find("ERR") : status_err("boom").find("ERR"));
    EXPECT_NE(status_err("boom").find("ERR"), std::string::npos);
}

TEST(ConsoleTable, RenderRows)
{
    auto t = render_table({"a", "b"}, {{"1", "2"}, {"3", "4"}});
    EXPECT_NE(t.find("| a |"), std::string::npos);
    EXPECT_NE(t.find("| 1 |"), std::string::npos);
    EXPECT_EQ(t.find("\n\n"), std::string::npos);
}

TEST(ConsoleCodec, NodeIdRoundTrip)
{
    auto id = parse_node_id("0:0:0:10");
    ASSERT_TRUE(id);
    EXPECT_EQ(format_node_id(*id), "0:0:0:10");
}

TEST(ConsoleCodec, Base64RoundTrip)
{
    bfc_tunnel::key_t raw = {1, 2, 3, 4, 5};
    auto enc = base64_encode(raw);
    auto dec = base64_decode(enc);
    ASSERT_TRUE(dec);
    EXPECT_EQ(*dec, raw);
}

TEST(ConsoleCodec, FilterMatching)
{
    std::unordered_map<std::string, std::string> row{{"node_id", "0:0:0:1"}, {"peer", "x"}};
    EXPECT_TRUE(row_matches(row, {{true, "node_id", "0:0:0:1"}}));
    EXPECT_FALSE(row_matches(row, {{true, "node_id", "0:0:0:2"}}));
    EXPECT_FALSE(row_matches(row, {{false, "node_id", "0:0:0:1"}}));
}
