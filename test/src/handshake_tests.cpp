
#include <wildcat/ws/handshake.hpp>
#include "gtest/gtest.h"

namespace {

    TEST(HandshakeTests, GenerateKey) {
        wildcat::ws::KeyGenerator gen;

        {
            std::vector<uint8_t> key(16, 0);
            gen.fill(key);

            // Test that key vector was filled with random numbers not equal to fill value of 0
            for (auto &&k: key) {
                EXPECT_NE(k, 0);
            }
        }

        {
            auto key = gen.generate(8);
            EXPECT_EQ(key.size(), 8);
            for (auto &&k: key) {
                EXPECT_NE(k, 0);
            }
        }

    }

    TEST(HandshakeTests, GetUpgradeRequest) {
        std::string expectedRequest{
                "GET /foo HTTP/1.1\r\nHost: bar.com\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: abc123\r\n\r\n"};
        const auto req = wildcat::ws::getUpgradeRequest("bar.com", "foo", "abc123");
        EXPECT_STREQ(req.c_str(), expectedRequest.c_str());
    }

    TEST(HandshakeTests, ParseResponse) {
        const char *msg = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
        wildcat::ws::HttpResponse response;
        response.parse(msg, strlen(msg));
        EXPECT_EQ(response.status(), 101);
        const auto &headers = response.headers();

        auto h = headers.find("Upgrade");
        EXPECT_TRUE(h != headers.end());
        EXPECT_STREQ(h->second.c_str(), "websocket");

        h = headers.find("Connection");
        EXPECT_TRUE(h != headers.end());
        EXPECT_STREQ(h->second.c_str(), "Upgrade");

        h = headers.find("Sec-WebSocket-Accept");
        EXPECT_TRUE(h != headers.end());
        EXPECT_STREQ(h->second.c_str(), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    TEST(HandshakeTests, AcceptKey) {
        // https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers#server_handshake_response
        const auto key = wildcat::ws::getAcceptKey("dGhlIHNhbXBsZSBub25jZQ==");
        EXPECT_STREQ(key.data(), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

}
