#include <gtest/gtest.h>
#include "shared/net/Serializer.hpp"

using namespace gs::net;

TEST(SerializerTest, WriteAndReadBasicTypes) {
    Serializer out;
    out.writeU8(0x42);
    out.writeU16(0x1234);
    out.writeU32(0xDEADBEEF);

    std::string testStr = "TestString";
    out.writeStr(testStr);

    auto payload = out.take();
    EXPECT_FALSE(payload.empty());

    // Rozmiar payloadu: 1 (u8) + 2 (u16) + 4 (u32) + 2 (len) + 10 (str) = 19
    EXPECT_EQ(payload.size(), 19);

    Serializer in(payload);
    EXPECT_EQ(in.readU8(), 0x42);
    EXPECT_EQ(in.readU16(), 0x1234);
    EXPECT_EQ(in.readU32(), 0xDEADBEEF);
    EXPECT_EQ(in.readStr(), testStr);

    EXPECT_FALSE(in.hasData());
}

TEST(SerializerTest, BufferUnderflowThrows) {
    std::vector<uint8_t> smallData = { 0x01, 0x02 };
    Serializer in(smallData);

    EXPECT_THROW(in.readU32(), std::out_of_range);
}