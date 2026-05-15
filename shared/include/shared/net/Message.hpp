#pragma once
#include "MessageTypes.hpp"
#include "Serializer.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace gs {

    /// Nagłówek wiadomości TCP (8 bajtów, little-endian)
    struct MessageHeader {
        MessageType type{};
        uint16_t    payloadSize{ 0 };
        uint32_t    reserved{ 0 };
    };
    static_assert(sizeof(MessageHeader) == 8);

    /// Wiadomość = nagłówek + payload
    struct Message {
        MessageHeader        header{};
        std::vector<uint8_t> payload{};

        // ── Fabryki ──────────────────────────────────────────────────────────────

        static Message make(MessageType type, std::vector<uint8_t> payload = {}) {
            Message m;
            m.header.type = type;
            m.header.payloadSize = static_cast<uint16_t>(payload.size());
            m.payload = std::move(payload);
            return m;
        }

        static Message make(MessageType type, Serializer& s) {
            return make(type, s.take());
        }

        // ── Serializacja nagłówka do raw bytes (wysyłanie przez TCP) ─────────────

        /// Zwraca 8 bajtów nagłówka (little-endian)
        [[nodiscard]] std::array<uint8_t, sizeof(MessageHeader)> headerBytes() const {
            std::array<uint8_t, sizeof(MessageHeader)> buf{};
            auto t = static_cast<uint16_t>(header.type);
            buf[0] = static_cast<uint8_t>(t & 0xFF);
            buf[1] = static_cast<uint8_t>((t >> 8) & 0xFF);
            buf[2] = static_cast<uint8_t>(header.payloadSize & 0xFF);
            buf[3] = static_cast<uint8_t>((header.payloadSize >> 8) & 0xFF);
            // reserved = 0, już wyzerowane
            return buf;
        }

        /// Parsuje 8 bajtów nagłówka (po odebraniu z TCP)
        static MessageHeader parseHeader(std::span<const uint8_t, sizeof(MessageHeader)> raw) {
            MessageHeader h;
            uint16_t t = static_cast<uint16_t>(raw[0])
                | static_cast<uint16_t>(raw[1]) << 8;
            h.type = static_cast<MessageType>(t);
            h.payloadSize = static_cast<uint16_t>(raw[2])
                | static_cast<uint16_t>(raw[3]) << 8;
            // reserved — ignorujemy
            return h;
        }

        // ── Walidacja ─────────────────────────────────────────────────────────────

        [[nodiscard]] bool isValid() const noexcept {
            return payload.size() == header.payloadSize;
        }

        [[nodiscard]] size_t totalSize() const noexcept {
            return sizeof(MessageHeader) + payload.size();
        }
    };

    // ── Pomocnicze funkcje budowania konkretnych wiadomości ─────────────────────

    namespace messages {

        // C→S: ClientHello
        // payload: string name
        inline Message makeClientHello(std::string_view name) {
            Serializer s;
            s.writeStr(name);
            return Message::make(MessageType::ClientHello, s);
        }

        // S→C: ServerWelcome
        // payload: uint32_t entityId, string name (echo)
        inline Message makeServerWelcome(uint32_t entityId, std::string_view name) {
            Serializer s;
            s.writeU32(entityId);
            s.writeStr(name);
            return Message::make(MessageType::ServerWelcome, s);
        }

        // S→C: LobbyState
        // payload: uint8_t playerCount, [playerCount × (uint32_t id, string name, bool isBot)]
        struct LobbyPlayerInfo {
            uint32_t    id;
            std::string name;
            bool        isBot{ false };
        };

        inline Message makeLobbyState(const std::vector<LobbyPlayerInfo>& players) {
            Serializer s;
            s.writeU8(static_cast<uint8_t>(players.size()));
            for (auto& p : players) {
                s.writeU32(p.id);
                s.writeStr(p.name);
                s.writeBool(p.isBot);
            }
            return Message::make(MessageType::LobbyState, s);
        }

        // S→C: LobbyStartGame
        // payload: uint32_t tickrateHz, uint64_t mapSeed
        inline Message makeLobbyStartGame(uint32_t tickrateHz, uint64_t mapSeed) {
            Serializer s;
            s.writeU32(tickrateHz);
            s.writeU64(mapSeed);
            return Message::make(MessageType::LobbyStartGame, s);
        }

        // S→C: ErrorResponse
        // payload: uint16_t errorCode, string message
        inline Message makeError(uint16_t code, std::string_view msg) {
            Serializer s;
            s.writeU16(code);
            s.writeStr(msg);
            return Message::make(MessageType::ErrorResponse, s);
        }

        // C→S: DebugStep
        // payload: uint32_t numTicks
        inline Message makeDebugStep(uint32_t numTicks) {
            Serializer s;
            s.writeU32(numTicks);
            return Message::make(MessageType::DebugStep, s);
        }

        // C→S: DebugSetTickrate
        // payload: uint32_t hz
        inline Message makeDebugSetTickrate(uint32_t hz) {
            Serializer s;
            s.writeU32(hz);
            return Message::make(MessageType::DebugSetTickrate, s);
        }

    } // namespace messages
} // namespace gs