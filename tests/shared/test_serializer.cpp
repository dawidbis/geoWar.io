#include <gtest/gtest.h>
#include <shared/net/Serializer.hpp>
#include <shared/net/Message.hpp>
#include <shared/net/MessageTypes.hpp>
#include <span>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

using namespace gs;

// Pomocnicze funkcje — izolują Serializer od EXPECT_* w osobnym stack frame
static std::vector<uint8_t> serU8(uint8_t a, uint8_t b, uint8_t c) {
    Serializer w; w.writeU8(a); w.writeU8(b); w.writeU8(c); return w.take();
}
static std::vector<uint8_t> serU16(uint16_t a, uint16_t b, uint16_t c) {
    Serializer w; w.writeU16(a); w.writeU16(b); w.writeU16(c); return w.take();
}
static std::vector<uint8_t> serU32(uint32_t a, uint32_t b, uint32_t c) {
    Serializer w; w.writeU32(a); w.writeU32(b); w.writeU32(c); return w.take();
}
static std::vector<uint8_t> serI64(int64_t a, int64_t b, int64_t c, int64_t d) {
    Serializer w; w.writeI64(a); w.writeI64(b); w.writeI64(c); w.writeI64(d); return w.take();
}
static std::vector<uint8_t> serF32(float a, float b, float c) {
    Serializer w; w.writeF32(a); w.writeF32(b); w.writeF32(c); return w.take();
}
static std::vector<uint8_t> serBool(bool a, bool b, bool c) {
    Serializer w; w.writeBool(a); w.writeBool(b); w.writeBool(c); return w.take();
}
static std::vector<uint8_t> serStr(const char* a, const char* b, const char* c) {
    Serializer w; w.writeStr(a); w.writeStr(b); w.writeStr(c); return w.take();
}

TEST(Serializer, U8Roundtrip) {
    std::vector<uint8_t> buf = serU8(0, 127, 255);
    Serializer r{ std::span<const uint8_t>(buf) };
    uint8_t v0 = r.readU8();
    uint8_t v1 = r.readU8();
    uint8_t v2 = r.readU8();
    bool done = !r.hasData();
    EXPECT_EQ(v0, uint8_t(0));
    EXPECT_EQ(v1, uint8_t(127));
    EXPECT_EQ(v2, uint8_t(255));
    EXPECT_TRUE(done);
}

TEST(Serializer, U16Roundtrip) {
    std::vector<uint8_t> buf = serU16(0, 1000, 65535);
    Serializer r{ std::span<const uint8_t>(buf) };
    uint16_t v0 = r.readU16();
    uint16_t v1 = r.readU16();
    uint16_t v2 = r.readU16();
    EXPECT_EQ(v0, uint16_t(0));
    EXPECT_EQ(v1, uint16_t(1000));
    EXPECT_EQ(v2, uint16_t(65535));
}

TEST(Serializer, U32Roundtrip) {
    std::vector<uint8_t> buf = serU32(0, 100000, 0xFFFFFFFF);
    Serializer r{ std::span<const uint8_t>(buf) };
    uint32_t v0 = r.readU32();
    uint32_t v1 = r.readU32();
    uint32_t v2 = r.readU32();
    EXPECT_EQ(v0, uint32_t(0));
    EXPECT_EQ(v1, uint32_t(100000));
    EXPECT_EQ(v2, uint32_t(0xFFFFFFFF));
}

TEST(Serializer, I64Roundtrip) {
    std::vector<uint8_t> buf = serI64(0, -1, INT64_MAX, INT64_MIN);
    Serializer r{ std::span<const uint8_t>(buf) };
    int64_t v0 = r.readI64();
    int64_t v1 = r.readI64();
    int64_t v2 = r.readI64();
    int64_t v3 = r.readI64();
    EXPECT_EQ(v0, int64_t(0));
    EXPECT_EQ(v1, int64_t(-1));
    EXPECT_EQ(v2, INT64_MAX);
    EXPECT_EQ(v3, INT64_MIN);
}

TEST(Serializer, F32Roundtrip) {
    std::vector<uint8_t> buf = serF32(0.0f, 3.14f, -99.5f);
    Serializer r{ std::span<const uint8_t>(buf) };
    float v0 = r.readF32();
    float v1 = r.readF32();
    float v2 = r.readF32();
    EXPECT_FLOAT_EQ(v0, 0.0f);
    EXPECT_FLOAT_EQ(v1, 3.14f);
    EXPECT_FLOAT_EQ(v2, -99.5f);
}

TEST(Serializer, BoolRoundtrip) {
    std::vector<uint8_t> buf = serBool(true, false, true);
    Serializer r{ std::span<const uint8_t>(buf) };
    bool v0 = r.readBool();
    bool v1 = r.readBool();
    bool v2 = r.readBool();
    EXPECT_TRUE(v0);
    EXPECT_FALSE(v1);
    EXPECT_TRUE(v2);
}

TEST(Serializer, StringRoundtrip) {
    std::vector<uint8_t> buf = serStr("hello", "", "Grand Strategy 2D");
    Serializer r{ std::span<const uint8_t>(buf) };
    std::string v0 = r.readStr();
    std::string v1 = r.readStr();
    std::string v2 = r.readStr();
    EXPECT_EQ(v0, std::string("hello"));
    EXPECT_EQ(v1, std::string(""));
    EXPECT_EQ(v2, std::string("Grand Strategy 2D"));
}

TEST(Serializer, MixedRoundtrip) {
    std::vector<uint8_t> buf;
    {
        Serializer w;
        w.writeU32(42);
        w.writeStr("Player");
        w.writeBool(false);
        w.writeF32(1.5f);
        w.writeI64(-9999);
        buf = w.take();
    }
    Serializer r{ std::span<const uint8_t>(buf) };
    uint32_t    u = r.readU32();
    std::string s = r.readStr();
    bool        b = r.readBool();
    float       f = r.readF32();
    int64_t     i = r.readI64();
    bool        done = !r.hasData();
    EXPECT_EQ(u, uint32_t(42));
    EXPECT_EQ(s, std::string("Player"));
    EXPECT_FALSE(b);
    EXPECT_FLOAT_EQ(f, 1.5f);
    EXPECT_EQ(i, int64_t(-9999));
    EXPECT_TRUE(done);
}

TEST(Serializer, UnderflowThrows) {
    std::vector<uint8_t> buf;
    { Serializer w; w.writeU8(1); buf = w.take(); }
    Serializer r{ std::span<const uint8_t>(buf) };
    uint8_t v = r.readU8();
    EXPECT_EQ(v, uint8_t(1));
    EXPECT_THROW(r.readU8(), std::runtime_error);
}

TEST(Serializer, TakeResetsBuffer) {
    std::vector<uint8_t> buf;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    {
        Serializer s;
        s.writeU32(1);
        s.writeU32(2);
        sizeBefore = s.size();
        buf = s.take();
        sizeAfter = s.size();
    }
    EXPECT_EQ(sizeBefore, size_t(8));
    EXPECT_EQ(sizeAfter, size_t(0));
    EXPECT_EQ(buf.size(), size_t(8));
}

TEST(Serializer, LittleEndianU16) {
    std::vector<uint8_t> buf;
    { Serializer w; w.writeU16(0x0102); buf = w.take(); }
    ASSERT_EQ(buf.size(), size_t(2));
    EXPECT_EQ(buf[0], uint8_t(0x02));
    EXPECT_EQ(buf[1], uint8_t(0x01));
}

TEST(Serializer, LittleEndianU32) {
    std::vector<uint8_t> buf;
    { Serializer w; w.writeU32(0x01020304); buf = w.take(); }
    ASSERT_EQ(buf.size(), size_t(4));
    EXPECT_EQ(buf[0], uint8_t(0x04));
    EXPECT_EQ(buf[1], uint8_t(0x03));
    EXPECT_EQ(buf[2], uint8_t(0x02));
    EXPECT_EQ(buf[3], uint8_t(0x01));
}

TEST(Message, DefaultIsValid) {
    // Domyślna wiadomość z rozmiarem payloadu 0 i pustym wektorem jest technicznie poprawna
    Message msg;
    EXPECT_TRUE(msg.isValid());
}

TEST(Message, MismatchedSizeIsInvalid) {
    // Jeśli nagłówek deklaruje 5 bajtów, a wektor ma tylko 3, wiadomość jest uszkodzona
    Message msg;
    msg.header.payloadSize = 5;
    msg.payload = { 0x01, 0x02, 0x03 };
    EXPECT_FALSE(msg.isValid());
}

TEST(Message, ValidAfterSetup) {
    std::vector<uint8_t> payload = { 0x01, 0x02 };
    Message msg = Message::make(MessageType::ClientHello, payload);
    bool valid = msg.isValid();
    uint16_t sz = msg.header.payloadSize;
    size_t psz = msg.payload.size();
    EXPECT_TRUE(valid);
    EXPECT_EQ(sz, uint16_t(2));
    EXPECT_EQ(psz, size_t(2));
}

TEST(Message, HeaderBytesRoundtrip) {
    Message msg = Message::make(MessageType::GameStateDelta, std::vector<uint8_t>{});
    auto hdrBytes = msg.headerBytes();
    EXPECT_EQ(hdrBytes.size(), sizeof(MessageHeader));
    auto parsed = Message::parseHeader(std::span<const uint8_t, sizeof(MessageHeader)>(hdrBytes));
    EXPECT_EQ(parsed.type, MessageType::GameStateDelta);
    EXPECT_EQ(parsed.payloadSize, uint16_t(0));
}

TEST(Message, ClientHelloFactory) {
    auto msg = messages::makeClientHello("TestPlayer");
    bool valid = msg.isValid();
    EXPECT_EQ(msg.header.type, MessageType::ClientHello);
    EXPECT_TRUE(valid);
    std::string name;
    {
        Serializer r{ std::span<const uint8_t>(msg.payload) };
        name = r.readStr();
    }
    EXPECT_EQ(name, std::string("TestPlayer"));
}

TEST(Message, ServerWelcomeFactory) {
    auto msg = messages::makeServerWelcome(42, "Alice");
    EXPECT_EQ(msg.header.type, MessageType::ServerWelcome);
    uint32_t id = 0;
    std::string name;
    {
        Serializer r{ std::span<const uint8_t>(msg.payload) };
        id = r.readU32();
        name = r.readStr();
    }
    EXPECT_EQ(id, uint32_t(42));
    EXPECT_EQ(name, std::string("Alice"));
}

TEST(Message, LobbyStateFactory) {
    // Dodajemy 4 parametr (isReady) do inicjalizacji
    std::vector<messages::LobbyPlayerInfo> players = {
        {1, "Alice", false, true},
        {2, "Bot",   true,  false},
    };
    auto msg = messages::makeLobbyState(players);
    EXPECT_EQ(msg.header.type, MessageType::LobbyState);

    uint8_t count = 0;
    uint32_t id1 = 0; std::string name1; bool bot1 = false; bool ready1 = false;
    uint32_t id2 = 0; std::string name2; bool bot2 = false; bool ready2 = false;

    {
        Serializer r{ std::span<const uint8_t>(msg.payload) };
        count = r.readU8();
        // Odczytujemy 4 parametry per gracz!
        id1 = r.readU32(); name1 = r.readStr(); bot1 = r.readBool(); ready1 = r.readBool();
        id2 = r.readU32(); name2 = r.readStr(); bot2 = r.readBool(); ready2 = r.readBool();
    }

    EXPECT_EQ(count, uint8_t(2));

    EXPECT_EQ(id1, uint32_t(1));
    EXPECT_EQ(name1, std::string("Alice"));
    EXPECT_FALSE(bot1);
    EXPECT_TRUE(ready1); // Oczekujemy true

    EXPECT_EQ(id2, uint32_t(2));
    EXPECT_EQ(name2, std::string("Bot"));
    EXPECT_TRUE(bot2);
    EXPECT_FALSE(ready2); // Oczekujemy false
}

TEST(Message, DebugStepFactory) {
    auto msg = messages::makeDebugStep(10);
    uint32_t n = 0;
    {
        Serializer r{ std::span<const uint8_t>(msg.payload) };
        n = r.readU32();
    }
    EXPECT_EQ(n, uint32_t(10));
}