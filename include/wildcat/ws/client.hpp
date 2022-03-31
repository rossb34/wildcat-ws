
#ifndef WILDCAT_WS_CLIENT_HPP
#define WILDCAT_WS_CLIENT_HPP

#include <cstdint>
#include <cstring>
#include <array>
#include <functional>
#include <memory>
#include <byteswap.h>
#include <wildcat/net/socket_stream.hpp>

#include "handshake.hpp"


namespace wildcat::ws {

    /*
     * https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
     * Frame format:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
     */

    /// Op Code
    enum class OpCode : std::uint8_t {
        CONTINUATION = 0,
        TEXT = 1,
        BINARY = 2,
        CLOSE = 8,
        PING = 9,
        PONG = 10,
        NULL_VALUE = 255
    };

    std::ostream &operator<<(std::ostream &os, OpCode val) {
        switch (val) {
            case OpCode::CONTINUATION:
                os << "Continuation";
                break;
            case OpCode::TEXT:
                os << "Text";
                break;
            case OpCode::BINARY:
                os << "Binary";
                break;
            case OpCode::CLOSE:
                os << "Close";
                break;
            case OpCode::PING:
                os << "Ping";
                break;
            case OpCode::PONG:
                os << "Pong";
                break;
            case OpCode::NULL_VALUE:
                os << "NullValue";
                break;
        }
        return os;
    }

    /// Gets an OpCode from the specified value
    static OpCode opCodeFrom(std::uint8_t val) {
        switch (val) {
            case 0:
                return OpCode::CONTINUATION;
            case 1:
                return OpCode::TEXT;
            case 2:
                return OpCode::BINARY;
            case 8:
                return OpCode::CLOSE;
            case 9:
                return OpCode::PING;
            case 10:
                return OpCode::PONG;
            default:
                return OpCode::NULL_VALUE;
        }
    }

    union wildcat_u16 {
        uint16_t val;
        uint8_t b[2];
    };

    union wildcat_u32 {
        uint64_t val;
        uint8_t b[4];
    };

    union wildcat_u64 {
        uint64_t val;
        uint8_t b[8];
    };


    /// Header of the web socket frame
    struct FrameHeader {
        OpCode opCode;
        bool isFinal;
        std::size_t messageLength;
        bool mask;
        std::array<std::uint8_t, 4> maskKeys{};
    };


    /// Web socket frame writer
    class FrameWriter {
    public:
        /// Constructs a frame writer from the specified buffer and length
        FrameWriter(std::uint8_t *buffer, std::size_t length)
                : buffer_(buffer), bufferEnd_(buffer + length), next_(buffer), messageBegin_(nullptr),
                  messageEnd_(nullptr) {}

        /// Writes a message to the frame writer
        ///
        /// \param header frame header for the message
        /// \param message pointer to the beginning of the message data
        void write(const FrameHeader &header, const std::uint8_t *message) {
            // get the op code as underlying type uint8_t
            const auto opCode = static_cast<const uint8_t>(header.opCode);
            // bitwise OR the final flag, op code, and reserved bits for the first element
            // assume rsv1, rsv2, and rsv3 are false
            // TODO: support rsv1, rsv2, rsv3 in frame header
            auto first = opCode;
            first |= (header.isFinal ? 0x80 : 0);
            std::memcpy(next_, &first, 1);
            ++next_;

            if (header.messageLength < 126) {
                auto second = (header.messageLength & 0xff) | (header.mask ? 0x80 : 0);
                std::memcpy(next_, &second, 1);
                ++next_;
            } else if (header.messageLength < 65536) {
                auto second = 126 | (header.mask ? 0x80 : 0);
                std::memcpy(next_, &second, 1);
                ++next_;
                const auto val = static_cast<std::uint16_t>(header.messageLength);
                wildcat_u16 tmp{};
                tmp.val = __builtin_bswap16(val);
                std::memcpy(next_, &tmp.b, sizeof(std::uint16_t));
                next_ += 2;
            } else {
                auto second = 127 | (header.mask ? 0x80 : 0);
                std::memcpy(next_, &second, 1);
                ++next_;
                const auto val = static_cast<std::uint64_t>(header.messageLength);
                wildcat_u64 tmp{};
                tmp.val = __builtin_bswap64(val);
                std::memcpy(next_, &tmp.b, sizeof(std::uint64_t));
                next_ += 8;
            }

            if (header.mask) {
                std::memcpy(next_, header.maskKeys.data(), 4);
                next_ += 4;
            }

            // Check for enough space in the buffer to write the message payload
            if (header.messageLength > bufferEnd_ - next_) {
                throw std::runtime_error("Buffer too short for message payload");
            }

            messageBegin_ = next_;
            messageEnd_ = messageBegin_ + header.messageLength;

            std::memcpy(next_, message, header.messageLength);
            if (header.mask) {
                int i = 0;
                for (auto *b = messageBegin_; b != messageEnd_; ++b) {
                    // i & 0x3 result will always be in the range of [0, 3]
                    *b ^= header.maskKeys[i & 0x3];
                    ++i;
                }
            }
            next_ += header.messageLength;
        }

        /// Gets the length of the message
        [[nodiscard]] std::size_t messageLength() const noexcept {
            return messageEnd_ - messageBegin_;
        }

        /// Gets pointer to one past the end of the message
        std::uint8_t *messageEnd() {
            return messageEnd_;
        }

        /// Gets pointer to one past the end of the message
        const std::uint8_t *bufferBegin() {
            return buffer_;
        }

        /// Gets the length of the header
        [[nodiscard]] std::size_t headerLength() const noexcept {
            return messageBegin_ - buffer_;
        }

        /// Gets the length of the frame
        [[nodiscard]] std::size_t frameLength() const noexcept {
            return messageEnd_ - buffer_;
        }

        /// Gets the remaining size of the bufferBegin that is available to write to
        [[nodiscard]] std::size_t bufferSizeRemaining() const noexcept {
            return bufferEnd_ - next_;
        }

    private:
        std::uint8_t *buffer_;
        std::uint8_t *bufferEnd_;
        std::uint8_t *next_;
        std::uint8_t *messageBegin_;
        std::uint8_t *messageEnd_;
    };

    /// Web socket frame reader
    class FrameReader {
    public:

        /// Constructs a FrameReader from the specified buffer and length
        FrameReader(std::uint8_t *buffer, std::size_t length)
                : buffer_(buffer), bufferEnd_(buffer + length), next_(buffer),
                  messageBegin_(nullptr), messageEnd_(nullptr), isComplete_(false), final_(false),
                  opCode_(OpCode::NULL_VALUE), isMasked_(false), messageLength_(0),
                  maskKeys_() {
            init();
        }

        /// Gets true/false if the message is final
        [[nodiscard]] bool isFinal() const noexcept {
            return final_;
        }

        /// Gets the opcode
        [[nodiscard]] OpCode opCode() const noexcept {
            return opCode_;
        }

        /// Gets true/false if the message is masked
        [[nodiscard]] bool isMasked() const noexcept {
            return isMasked_;
        }

        /// Gets the message length
        [[nodiscard]] std::size_t messageLength() const noexcept {
            return messageLength_;
        }

        /// Gets the mask keys
        [[nodiscard]] const std::array<std::uint8_t, 4> &maskKeys() const noexcept {
            return maskKeys_;
        }

        /// Gets a pointer to the beginning of the buffer
        const std::uint8_t *bufferBegin() {
            return buffer_;
        }

        /// Gets a pointer to the beginning of the message
        const std::uint8_t *messageBegin() {
            return messageBegin_;
        }

        /// Gets a pointer to one past the end of the message
        const std::uint8_t *messageEnd() {
            return messageEnd_;
        }

        /// Gets true/false if the message is complete
        [[nodiscard]] bool isComplete() const {
            return isComplete_;
        }

    private:
        std::uint8_t *buffer_;
        std::uint8_t *bufferEnd_;
        std::uint8_t *next_;
        std::uint8_t *messageBegin_;
        std::uint8_t *messageEnd_;
        bool isComplete_;
        bool final_;
        OpCode opCode_;
        bool isMasked_;
        std::size_t messageLength_;
        std::array<std::uint8_t, 4> maskKeys_;

        void init() {
            final_ = (*next_ & 0x80) == 0x80;
            opCode_ = opCodeFrom(*next_ & 0x0f);
            ++next_;
            isMasked_ = (*next_ & 0x80) == 0x80;
            std::uint8_t lengthByte = (*next_ & 0x7f);
            ++next_;
            if (lengthByte < 126) {
                // The message length is the value of the length byte
                messageLength_ = static_cast<std::size_t>(lengthByte);
            } else if (lengthByte == 126) {
                // Read the next 16 bits and interpret as an unsigned integer
                wildcat_u16 tmp{};
                std::memcpy(&tmp.b, next_, sizeof(std::uint16_t));
                messageLength_ = __builtin_bswap16(tmp.val);
                next_ += 2;
            } else if (lengthByte == 127) {
                // Read the next 64 bits and interpret as an unsigned integer
                wildcat_u64 tmp{};
                std::memcpy(&tmp.b, next_, sizeof(std::uint64_t));
                messageLength_ = __builtin_bswap64(tmp.val);
                next_ += 8;
            } else {
                // unknown length byte
                throw std::runtime_error("Invalid message length");
            }

            if (isMasked_) {
                std::memcpy(maskKeys_.data(), next_, 4);
                next_ += 4;
            }
            messageBegin_ = next_;
            messageEnd_ = messageBegin_ + messageLength_;

            isComplete_ = messageEnd_ <= bufferEnd_;
            if (!isComplete_)
                return;

            if (isMasked_) {
                // unmask
                int i = 0;
                for (auto *b = messageBegin_; b != messageEnd_; ++b) {
                    *b ^= maskKeys_[i & 0x3];
                    ++i;
                }
            }
            next_ += messageLength_;
        }
    };

    // Message handler
    typedef std::function<void(OpCode opCode, const std::uint8_t *buffer, std::size_t length)> message_handler_t;

    namespace {

        /// Assembles frames according to the frame boundary of the protocol
        ///
        /// Returns the total number of bytes processed for complete frames in the bufferBegin. If no complete frame is
        /// in the buffer, then the function will return 0.
        template<typename F>
        static std::size_t assembleFrame(std::uint8_t *buffer, std::size_t length, F &&f) {
            std::size_t cursor = 0;
            while (cursor < length) {
                FrameReader frameReader(buffer + cursor, length - cursor);
                if (frameReader.isComplete()) {
                    // A complete message has been read so call the callback function
                    const auto msgLength = frameReader.messageEnd() - frameReader.messageBegin();
                    f(frameReader.opCode(), frameReader.messageBegin(), msgLength);

                    // bytes processed is the total frame size (i.e. header length + payload/message length)
                    const auto bytesProcessed = frameReader.messageEnd() - frameReader.bufferBegin();
                    // advance the cursor by the total number of bytes processed for the frame
                    cursor += bytesProcessed;
                } else {
                    break;
                }
            }

            return cursor;
        }

    }

    /// Web socket client config
    struct Config {
        std::string host;
        std::string path;
    };

    /// Web Socket Client
    class Client {
    public:
        /// Constructs a web socket Client from the specified socket stream
        explicit Client(std::unique_ptr<wildcat::net::SocketStream> stream)
                : stream_(std::move(stream)), hostName_(), path_(), offset_(0), rxBuf_(), txBuf_(), maskKeys_(4) {
            KeyGenerator generator;
            generator.fill(maskKeys_);
        }

        /// Constructs a web socket Client from the specified stream and config
        ///
        /// \param stream TCP socket stream
        /// \param config configuration used for the handshake when sending the upgrade request. The configuration is
        /// useful when connecting through a proxy, e.g. stunnel.
        Client(std::unique_ptr<wildcat::net::SocketStream> stream, const Config &config)
                : stream_(std::move(stream)), hostName_(config.host), path_(config.path), offset_(0), rxBuf_(),
                  txBuf_(), maskKeys_(4) {
            KeyGenerator generator;
            generator.fill(maskKeys_);
        }

        /// Connects to the endpoint and initiates the web socket handshake
        bool connect(const std::string &host, std::uint16_t port) {
            try {
                stream_->connect(host, port);
                const auto hostName = hostName_.empty() ? host : hostName_;
                const auto path = path_.empty() ? "" : path_;
                Handshaker::doHandshake(hostName, path, stream_.get());
            } catch (const std::exception &e) {
                throw;
            }
            return true;
        }

        /// Polls the connection
        template<typename F>
        int poll(F &&f) {
            struct pollfd pfd{};
            pfd.fd = stream_->fd();
            pfd.events = POLLIN;

            if (::poll(&pfd, 1, 0) < 1)
                return 0;

            const auto bytesRead = stream_->recvBytes(reinterpret_cast<char *>(rxBuf_.data()) + offset_,
                                                      rxBuf_.size() - offset_);
            if (bytesRead > 0) {
                const auto len = offset_ + bytesRead;
                const auto pos = assembleFrame(rxBuf_.data(), len, f);
                // check for partial frame at end of buffer
                const auto remaining = len - pos;
                if (remaining > 0) {
                    std::memcpy(rxBuf_.data(), rxBuf_.data() + pos, remaining);
                    offset_ = pos;
                } else {
                    offset_ = 0;
                }
                return 1;
            }
            return 0;
        }

        std::size_t send(const std::string &msg) {
            FrameHeader header;
            header.opCode = OpCode::TEXT;
            header.isFinal = true;
            header.messageLength = msg.size();
            header.mask = true;
            std::memcpy(header.maskKeys.data(), maskKeys_.data(), 4 * sizeof(std::uint8_t));

            FrameWriter frameWriter(txBuf_.data(), txBuf_.size());
            frameWriter.write(header, reinterpret_cast<const uint8_t *>(msg.data()));
            std::size_t bytesSent = 0;
            // effectively a blocking send until all bytes are sent
            while (bytesSent < frameWriter.frameLength()) {
                const auto n = stream_->sendBytes(reinterpret_cast<const char *>(frameWriter.bufferBegin() + bytesSent),
                                                  frameWriter.frameLength() - bytesSent);
                bytesSent += n;
            }
            return bytesSent;
        }

    private:
        std::unique_ptr<wildcat::net::SocketStream> stream_;
        std::string hostName_;
        std::string path_;
        std::size_t offset_;
        std::array<std::uint8_t, 1024 * 1024 * 4> rxBuf_;
        std::array<std::uint8_t, 1024> txBuf_;
        std::vector<std::uint8_t> maskKeys_;
    };

}

#endif //WILDCAT_WS_CLIENT_HPP
