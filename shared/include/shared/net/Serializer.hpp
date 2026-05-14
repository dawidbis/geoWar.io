// shared/include/shared/net/Serializer.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
#include <span>
#include <cstring>

namespace gs::net {

    class Serializer {
    public:
        Serializer() = default;
        explicit Serializer(std::span<const uint8_t> data)
            : buf_(data.begin(), data.end()) {
        }

        // Zapis
        void writeU8(uint8_t v);
        void writeU16(uint16_t v);
        void writeU32(uint32_t v);
        void writeStr(std::string_view s);

        std::vector<uint8_t> take(); // Zwraca payload i czyœci bufor

        // Odczyt
        uint8_t readU8();
        uint16_t readU16();
        uint32_t readU32();
        std::string readStr();

        bool hasData() const { return readPos_ < buf_.size(); }

    private:
        std::vector<uint8_t> buf_;
        size_t readPos_{ 0 };
    };

} // namespace gs::net