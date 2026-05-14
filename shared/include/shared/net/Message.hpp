#pragma once
#include "MessageTypes.hpp"
#include <cstdint>
#include <vector>
#include <span>

namespace gs {

/// Nagłówek każdej wiadomości (8 bajtów, little-endian)
struct MessageHeader {
    MessageType type{};
    uint16_t    payloadSize{0};
    uint32_t    reserved{0};  // padding / future use (sequence number itp.)
};
static_assert(sizeof(MessageHeader) == 8);

/// Wiadomość = nagłówek + payload
struct Message {
    MessageHeader           header{};
    std::vector<uint8_t>    payload{};

    // Rozmiar całości (nagłówek + payload)
    [[nodiscard]] size_t totalSize() const noexcept {
        return sizeof(MessageHeader) + payload.size();
    }

    // Czy payload zgadza się z rozmiarem w nagłówku
    [[nodiscard]] bool isValid() const noexcept {
        return payload.size() == header.payloadSize;
    }
};

} // namespace gs
