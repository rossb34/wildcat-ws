
#include <wildcat/ws/client.hpp>
#include "gtest/gtest.h"

namespace {

    std::string genRandomMessage(int n) {
        static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string out;
        out.reserve(n);
        for (int i = 0; i < n; ++i)
            out += alphanum[rand() % (sizeof(alphanum) - 1)];

        return out;
    }

    TEST(ClientTests, FrameReadWrite) {

        // message size less than 126 bytes
        {
            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = 5;
            header.mask = true;
            header.maskKeys[0] = 1;
            header.maskKeys[1] = 2;
            header.maskKeys[2] = 3;
            header.maskKeys[3] = 4;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            const char *message = "hello";
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message));

            wildcat::ws::FrameReader frameReader(buffer, 1024);
            EXPECT_EQ(frameReader.isFinal(), header.isFinal);
            EXPECT_EQ(frameReader.opCode(), header.opCode);
            EXPECT_EQ(frameReader.isMasked(), header.mask);
            EXPECT_EQ(frameReader.messageLength(), header.messageLength);
            const auto maskKeys = frameReader.maskKeys();
            EXPECT_EQ(maskKeys[0], header.maskKeys[0]);
            EXPECT_EQ(maskKeys[1], header.maskKeys[1]);
            EXPECT_EQ(maskKeys[2], header.maskKeys[2]);
            EXPECT_EQ(maskKeys[3], header.maskKeys[3]);
            auto msg = std::string(reinterpret_cast<const char *>(frameReader.messageBegin()), frameReader.messageLength());
            EXPECT_STREQ(msg.c_str(), message);
        }

        // 126 <= message size < 65536
        {
            const auto message = genRandomMessage(500);

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = true;
            header.maskKeys[0] = 1;
            header.maskKeys[1] = 2;
            header.maskKeys[2] = 3;
            header.maskKeys[3] = 4;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));

            wildcat::ws::FrameReader frameReader(buffer, 1024);
            EXPECT_EQ(frameReader.isFinal(), header.isFinal);
            EXPECT_EQ(frameReader.opCode(), header.opCode);
            EXPECT_EQ(frameReader.isMasked(), header.mask);
            EXPECT_EQ(frameReader.messageLength(), header.messageLength);
            const auto maskKeys = frameReader.maskKeys();
            EXPECT_EQ(maskKeys[0], header.maskKeys[0]);
            EXPECT_EQ(maskKeys[1], header.maskKeys[1]);
            EXPECT_EQ(maskKeys[2], header.maskKeys[2]);
            EXPECT_EQ(maskKeys[3], header.maskKeys[3]);
        }

        // 65535 < message size < max 64 bit int
        {
            const auto message = genRandomMessage(1024 * 1024 * 2);

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = true;
            header.maskKeys[0] = 1;
            header.maskKeys[1] = 2;
            header.maskKeys[2] = 3;
            header.maskKeys[3] = 4;

            std::uint8_t buffer[1024 * 1024 * 4];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024 * 1024 * 4);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));

            wildcat::ws::FrameReader frameReader(buffer, 1024);
            EXPECT_EQ(frameReader.isFinal(), header.isFinal);
            EXPECT_EQ(frameReader.opCode(), header.opCode);
            EXPECT_EQ(frameReader.isMasked(), header.mask);
            EXPECT_EQ(frameReader.messageLength(), header.messageLength);
            const auto maskKeys = frameReader.maskKeys();
            EXPECT_EQ(maskKeys[0], header.maskKeys[0]);
            EXPECT_EQ(maskKeys[1], header.maskKeys[1]);
            EXPECT_EQ(maskKeys[2], header.maskKeys[2]);
            EXPECT_EQ(maskKeys[3], header.maskKeys[3]);
        }

        // Partial read off socket
        {
            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = 5;
            header.mask = true;
            header.maskKeys[0] = 1;
            header.maskKeys[1] = 2;
            header.maskKeys[2] = 3;
            header.maskKeys[3] = 4;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            const char *message = "hello";
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message));

            // Read less than the full message to simulate the byte stream nature of tcp
            auto bytesRead = frameWriter.headerLength() + frameWriter.messageLength() - 2;
            wildcat::ws::FrameReader frameReader(buffer, bytesRead);
            EXPECT_EQ(frameReader.isFinal(), header.isFinal);
            EXPECT_EQ(frameReader.opCode(), header.opCode);
            EXPECT_EQ(frameReader.isMasked(), header.mask);
            EXPECT_EQ(frameReader.messageLength(), header.messageLength);
            EXPECT_FALSE(frameReader.isComplete());
        }

    }

    TEST(ClientTests, AssembleFrame) {

        // incomplete frame
        {
            std::string message = "hello";

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = false;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));
            auto totalSize = frameWriter.messageEnd() - buffer;

            int i = 0;
            auto f = [&i, &message](wildcat::ws::OpCode opCode, const std::uint8_t *buffer, std::size_t length) {
                EXPECT_EQ(opCode, wildcat::ws::OpCode::TEXT);
                EXPECT_EQ(std::string(reinterpret_cast<const char *>(buffer), length), message);
                ++i;
            };
            auto n = wildcat::ws::assembleFrame(buffer, totalSize - 1, f);
            EXPECT_EQ(n, 0);
            EXPECT_EQ(i, 0);
        }

        // single complete frame
        {
            std::string message = "hello";

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = false;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));
            auto totalSize = frameWriter.messageEnd() - buffer;

            int i = 0;
            auto f = [&i, &message](wildcat::ws::OpCode opCode, const std::uint8_t *buffer, std::size_t length) {
                EXPECT_EQ(opCode, wildcat::ws::OpCode::TEXT);
                EXPECT_EQ(std::string(reinterpret_cast<const char *>(buffer), length), message);
                ++i;
            };
            auto n = wildcat::ws::assembleFrame(buffer, totalSize, f);
            EXPECT_EQ(n, totalSize);
            EXPECT_EQ(i, 1);
        }

        // single complete frame, then partial second frame
        {
            std::string message = "hello";

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = false;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));
            auto sizeFrame1 = frameWriter.messageEnd() - buffer;

            std::string message2 = "foobar";
            header.messageLength = message2.size();
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message2.data()));
            auto sizeFrame2 = frameWriter.messageEnd() - (buffer + sizeFrame1);

            int i = 0;
            auto f = [&i, &message](wildcat::ws::OpCode opCode, const std::uint8_t *buffer, std::size_t length) {
                EXPECT_EQ(opCode, wildcat::ws::OpCode::TEXT);
                EXPECT_EQ(std::string(reinterpret_cast<const char *>(buffer), length), message);
                ++i;
            };
            auto totalSize = sizeFrame1 + sizeFrame2;
            auto n = wildcat::ws::assembleFrame(buffer, totalSize - 2, f);
            EXPECT_EQ(n, sizeFrame1);
            EXPECT_EQ(i, 1);
        }

        // 2 complete frames
        {
            std::string message = "hello";

            wildcat::ws::FrameHeader header;
            header.opCode = wildcat::ws::OpCode::TEXT;
            header.isFinal  = true;
            header.messageLength = message.size();
            header.mask = false;

            std::uint8_t buffer[1024];
            wildcat::ws::FrameWriter frameWriter(buffer, 1024);
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message.data()));
            auto sizeFrame1 = frameWriter.messageEnd() - buffer;

            std::string message2 = "foobar";
            header.messageLength = message2.size();
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(message2.data()));
            auto sizeFrame2 = frameWriter.messageEnd() - (buffer + sizeFrame1);

            std::vector<std::string> messages{message, message2};
            auto i = 0;
            auto f = [&i, &messages](wildcat::ws::OpCode opCode, const std::uint8_t *buffer, std::size_t length) {
                EXPECT_EQ(std::string(reinterpret_cast<const char *>(buffer), length), messages[i]);
                ++i;
            };
            auto totalSize = sizeFrame1 + sizeFrame2;
            auto n = wildcat::ws::assembleFrame(buffer, totalSize, f);
            EXPECT_EQ(n, totalSize);
            EXPECT_EQ(i, 2);
        }

    }

}
