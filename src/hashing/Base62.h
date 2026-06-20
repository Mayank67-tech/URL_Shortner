#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <array>

namespace url_shortener {

class Base62 {
public:
    static constexpr char kCharset[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    static constexpr int kBase = 62;
    static constexpr size_t kMinCodeLength = 6;

    [[nodiscard]] static std::string encode(int64_t n) {
        if (n < 0) {
            throw std::invalid_argument(
                "Base62::encode: negative numbers are not supported (got " +
                std::to_string(n) + ")");
        }

        if (n == 0) {
            return std::string(kMinCodeLength, '0');
        }

        std::string result;
        result.reserve(12);

        int64_t value = n;
        while (value > 0) {
            result += kCharset[static_cast<size_t>(value % kBase)];
            value /= kBase;
        }

        while (result.size() < kMinCodeLength) {
            result += '0';
        }

        std::reverse(result.begin(), result.end());
        return result;
    }

    [[nodiscard]] static int64_t decode(const std::string& code) {
        if (code.empty()) {
            throw std::invalid_argument(
                "Base62::decode: input string must not be empty");
        }

        static const auto lookup = buildReverseLookup();

        int64_t result = 0;
        for (const char c : code) {
            const int val = lookup[static_cast<unsigned char>(c)];
            if (val < 0) {
                throw std::invalid_argument(
                    std::string("Base62::decode: invalid character '") + c +
                    "' in input \"" + code + "\"");
            }

            if (result > (INT64_MAX - val) / kBase) {
                throw std::overflow_error(
                    "Base62::decode: decoded value overflows int64_t for "
                    "input \"" + code + "\"");
            }

            result = result * kBase + val;
        }

        return result;
    }

    [[nodiscard]] static bool isValid(const std::string& code) noexcept {
        if (code.empty()) return false;

        static const auto lookup = buildReverseLookup();
        for (const char c : code) {
            if (lookup[static_cast<unsigned char>(c)] < 0) {
                return false;
            }
        }
        return true;
    }

private:
    [[nodiscard]] static std::array<int, 256> buildReverseLookup() noexcept {
        std::array<int, 256> table{};
        table.fill(-1);

        for (int i = 0; i < kBase; ++i) {
            table[static_cast<unsigned char>(kCharset[i])] = i;
        }
        return table;
    }
};

} // namespace url_shortener
