#include <gtest/gtest.h>
#include "hashing/Base62.h"
#include <unordered_set>
#include <string>

using namespace url_shortener;

TEST(Base62Test, EncodeZero) {
    std::string result = Base62::encode(0);
    EXPECT_EQ(result.length(), 6);
    EXPECT_EQ(result, "000000");
}

TEST(Base62Test, EncodeOne) {
    std::string result = Base62::encode(1);
    EXPECT_EQ(result.length(), 6);
}

TEST(Base62Test, EncodeSmallNumber) {
    std::string result = Base62::encode(62);
    EXPECT_EQ(result.length(), 6);
}

TEST(Base62Test, EncodeLargeNumber) {
    std::string result = Base62::encode(1234567);
    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.length(), 6u);
}

TEST(Base62Test, EncodeMaxValue) {
    std::string result = Base62::encode(56800235583LL);
    EXPECT_FALSE(result.empty());
}

TEST(Base62Test, EncodeNegativeThrows) {
    EXPECT_THROW(Base62::encode(-1), std::invalid_argument);
    EXPECT_THROW(Base62::encode(-100), std::invalid_argument);
}

TEST(Base62Test, RoundtripZero) {
    std::string encoded = Base62::encode(0);
    int64_t decoded = Base62::decode(encoded);
    EXPECT_EQ(decoded, 0);
}

TEST(Base62Test, RoundtripOne) {
    std::string encoded = Base62::encode(1);
    int64_t decoded = Base62::decode(encoded);
    EXPECT_EQ(decoded, 1);
}

TEST(Base62Test, RoundtripVariousNumbers) {
    std::vector<int64_t> testValues = {
        0, 1, 61, 62, 63, 100, 999, 1234,
        123456, 1234567, 12345678,
        999999999LL, 56800235583LL
    };

    for (int64_t val : testValues) {
        std::string encoded = Base62::encode(val);
        int64_t decoded = Base62::decode(encoded);
        EXPECT_EQ(decoded, val) << "Roundtrip failed for value: " << val;
    }
}

TEST(Base62Test, UniqueCodesForDifferentInputs) {
    std::unordered_set<std::string> codes;

    for (int64_t i = 0; i < 10000; ++i) {
        std::string code = Base62::encode(i);
        EXPECT_TRUE(codes.insert(code).second)
            << "Duplicate code generated for i=" << i << ": " << code;
    }

    EXPECT_EQ(codes.size(), 10000u);
}

TEST(Base62Test, OnlyBase62Characters) {
    const std::string base62chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int64_t i = 0; i < 1000; ++i) {
        std::string code = Base62::encode(i);
        for (char c : code) {
            EXPECT_NE(base62chars.find(c), std::string::npos)
                << "Invalid character '" << c << "' in code: " << code;
        }
    }
}

TEST(Base62Test, IsValidCode) {
    EXPECT_TRUE(Base62::isValid("A7Kd91"));
    EXPECT_TRUE(Base62::isValid("000000"));
    EXPECT_TRUE(Base62::isValid("abc123"));
    EXPECT_FALSE(Base62::isValid(""));
    EXPECT_FALSE(Base62::isValid("abc!@#"));
    EXPECT_FALSE(Base62::isValid("abc 12"));
}

TEST(Base62Test, DecodeEmptyStringThrows) {
    EXPECT_THROW(Base62::decode(""), std::invalid_argument);
}

TEST(Base62Test, DecodeInvalidCharsThrows) {
    EXPECT_THROW(Base62::decode("abc!@#"), std::invalid_argument);
}
