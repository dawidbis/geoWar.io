// shared/src/net/Serializer.cpp
#include "shared/net/Serializer.hpp"

namespace gs::net {

    void Serializer::writeU8(uint8_t v) {
        buf_.push_back(v);
    }

    void Serializer::writeU16(uint16_t v) {
        buf_.resize(buf_.size() + sizeof(v));
        std::memcpy(buf_.data() + buf_.size() - sizeof(v), &v, sizeof(v));
    }

    void Serializer::writeU32(uint32_t v) {
        buf_.resize(buf_.size() + sizeof(v));
        std::memcpy(buf_.data() + buf_.size() - sizeof(v), &v, sizeof(v));
    }

    void Serializer::writeStr(std::string_view s) {
        writeU16(static_cast<uint16_t>(s.length()));
        buf_.insert(buf_.end(), s.begin(), s.end());
    }

    std::vector<uint8_t> Serializer::take() {
        std::vector<uint8_t> result = std::move(buf_);
        buf_.clear();
        readPos_ = 0;
        return result;
    }

    uint8_t Serializer::readU8() {
        if (readPos_ + sizeof(uint8_t) > buf_.size()) throw std::out_of_range("Buffer underflow");
        return buf_[readPos_++];
    }

    uint16_t Serializer::readU16() {
        if (readPos_ + sizeof(uint16_t) > buf_.size()) throw std::out_of_range("Buffer underflow");
        uint16_t v;
        std::memcpy(&v, buf_.data() + readPos_, sizeof(v));
        readPos_ += sizeof(v);
        return v;
    }

    uint32_t Serializer::readU32() {
        if (readPos_ + sizeof(uint32_t) > buf_.size()) throw std::out_of_range("Buffer underflow");
        uint32_t v;
        std::memcpy(&v, buf_.data() + readPos_, sizeof(v));
        readPos_ += sizeof(v);
        return v;
    }

    std::string Serializer::readStr() {
        uint16_t len = readU16();
        if (readPos_ + len > buf_.size()) throw std::out_of_range("Buffer underflow");
        std::string s(reinterpret_cast<const char*>(buf_.data() + readPos_), len);
        readPos_ += len;
        return s;
    }

} // namespace gs::net