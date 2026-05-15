#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gs {

    /// Prosty binarny serializer/deserializer (little-endian).
    /// Używany do budowania payloadów wiadomości sieciowych.
    class Serializer {
    public:
        // ── Zapis ────────────────────────────────────────────────────────────────

        void writeU8(uint8_t v) {
            buf_.push_back(v);
        }

        void writeU16(uint16_t v) {
            buf_.push_back(static_cast<uint8_t>(v & 0xFF));
            buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }

        void writeU32(uint32_t v) {
            buf_.push_back(static_cast<uint8_t>(v & 0xFF));
            buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        }

        void writeU64(uint64_t v) {
            for (int i = 0; i < 8; ++i)
                buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }

        void writeI64(int64_t v) {
            writeU64(static_cast<uint64_t>(v));
        }

        void writeF32(float v) {
            uint32_t bits{};
            std::memcpy(&bits, &v, sizeof(bits));
            writeU32(bits);
        }

        void writeBool(bool v) {
            writeU8(v ? 1u : 0u);
        }

        /// String: uint16_t length + bajty (bez null-terminatora)
        void writeStr(std::string_view s) {
            if (s.size() > 0xFFFF)
                throw std::runtime_error("Serializer: string too long");
            writeU16(static_cast<uint16_t>(s.size()));
            buf_.insert(buf_.end(),
                reinterpret_cast<const uint8_t*>(s.data()),
                reinterpret_cast<const uint8_t*>(s.data()) + s.size());
        }

        /// Zwraca zbudowany payload i resetuje bufor
        [[nodiscard]] std::vector<uint8_t> take() {
            return std::move(buf_);
        }

        /// Rozmiar zbudowanego payloadu
        [[nodiscard]] size_t size() const noexcept { return buf_.size(); }

        // ── Odczyt ───────────────────────────────────────────────────────────────

        explicit Serializer(std::span<const uint8_t> data)
            : buf_(data.begin(), data.end()), readPos_(0) {
        }

        Serializer() = default;

        [[nodiscard]] uint8_t readU8() {
            checkRemaining(1);
            return buf_[readPos_++];
        }

        [[nodiscard]] uint16_t readU16() {
            checkRemaining(2);
            uint16_t v = static_cast<uint16_t>(buf_[readPos_])
                | static_cast<uint16_t>(buf_[readPos_ + 1]) << 8;
            readPos_ += 2;
            return v;
        }

        [[nodiscard]] uint32_t readU32() {
            checkRemaining(4);
            uint32_t v = static_cast<uint32_t>(buf_[readPos_])
                | static_cast<uint32_t>(buf_[readPos_ + 1]) << 8
                | static_cast<uint32_t>(buf_[readPos_ + 2]) << 16
                | static_cast<uint32_t>(buf_[readPos_ + 3]) << 24;
            readPos_ += 4;
            return v;
        }

        [[nodiscard]] uint64_t readU64() {
            checkRemaining(8);
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v |= static_cast<uint64_t>(buf_[readPos_ + i]) << (i * 8);
            readPos_ += 8;
            return v;
        }

        [[nodiscard]] int64_t readI64() {
            return static_cast<int64_t>(readU64());
        }

        [[nodiscard]] float readF32() {
            uint32_t bits = readU32();
            float v{};
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        [[nodiscard]] bool readBool() {
            return readU8() != 0;
        }

        [[nodiscard]] std::string readStr() {
            uint16_t len = readU16();
            checkRemaining(len);
            std::string s(reinterpret_cast<const char*>(&buf_[readPos_]), len);
            readPos_ += len;
            return s;
        }

        [[nodiscard]] bool hasData() const noexcept {
            return readPos_ < buf_.size();
        }

        [[nodiscard]] size_t remaining() const noexcept {
            return buf_.size() - readPos_;
        }

    private:
        std::vector<uint8_t> buf_;
        size_t               readPos_{ 0 };

        void checkRemaining(size_t n) const {
            if (readPos_ + n > buf_.size())
                throw std::runtime_error("Serializer: buffer underflow");
        }
    };

} // namespace gs