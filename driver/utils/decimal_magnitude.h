#pragma once

#include "driver/platform/platform.h"

#include <array>
#include <cstddef>
#include <cstdint>

// The unsigned magnitude of a decimal in the little-endian byte order used by SQL_NUMERIC_STRUCT.
struct DecimalMagnitude {
    std::array<SQLCHAR, SQL_MAX_NUMERIC_LEN> bytes{};

    DecimalMagnitude() = default;

    explicit DecimalMagnitude(std::uint64_t value) {
        for (std::size_t i = 0; i < sizeof(value); ++i) {
            bytes[i] = static_cast<SQLCHAR>(value);
            value >>= 8;
        }
    }

    bool isZero() const {
        return bytes == decltype(bytes){};
    }

    bool appendDigit(std::uint32_t digit) {
        auto carry = digit;
        for (auto & byte : bytes) {
            carry += byte * 10;
            byte = static_cast<SQLCHAR>(carry);
            carry >>= 8;
        }
        return carry == 0;
    }

    std::uint32_t removeDigit() {
        std::uint32_t remainder = 0;
        for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
            const auto value = (remainder << 8) | *it;
            *it = static_cast<SQLCHAR>(value / 10);
            remainder = value % 10;
        }
        return remainder;
    }
};
