#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace newton::parsing {
// Decimal integer. Rejects empty, signs and overflow; no whitespace skipping.
inline std::optional<std::uint64_t> parse_ascii_uint64(std::string_view s) noexcept {
    if (s.empty()) return std::nullopt;
    // Every unsigned decimal string of at most 19 digits fits in uint64_t,
    // including the largest 19-digit value 9,999,999,999,999,999,999.
    // Do not perform an unnecessary per-digit overflow check on the common
    // short-number path (CSV group IDs, counts, timestamps, etc.).
    // Longer strings still use the exact checked parser so leading zeros
    // and the full UINT64_MAX boundary retain their original semantics.
    if (s.size() <= 19) {
        std::uint64_t result = 0;
        for (const char c : s) {
            if (c < '0' || c > '9') return std::nullopt;
            result = result * 10 + static_cast<unsigned>(c - '0');
        }
        return result;
    }
    std::uint64_t n = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        const auto digit = static_cast<unsigned>(c - '0');
        if (n > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return std::nullopt;
        n = n * 10 + digit;
    }
    return n;
}

// Exact decimal currency-style value with 0-2 decimal digits, no rounding.
// Returns signed integral cents; rejects locale separators, exponent and junk.
inline std::optional<std::int64_t> parse_decimal_cents(std::string_view s) noexcept {
    if (s.empty()) return std::nullopt;
    bool negative = false;
    if (s.front() == '-' || s.front() == '+') {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    if (s.empty()) return std::nullopt;
    const auto dot = s.find('.');
    const auto major = parse_ascii_uint64(s.substr(0, dot));
    if (!major) return std::nullopt;
    unsigned minor = 0;
    if (dot != std::string_view::npos) {
        const auto fraction = s.substr(dot + 1);
        if (fraction.empty() || fraction.size() > 2) return std::nullopt;
        for (char c : fraction) {
            if (c < '0' || c > '9') return std::nullopt;
            minor = minor * 10 + static_cast<unsigned>(c - '0');
        }
        if (fraction.size() == 1) minor *= 10;
    }
    const auto max_abs = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) +
                         static_cast<unsigned>(negative);
    if (*major > (max_abs - minor) / 100) return std::nullopt;
    const auto total = *major * 100 + minor;
    if (negative && total == max_abs) return std::numeric_limits<std::int64_t>::min();
    const auto signed_total = static_cast<std::int64_t>(total);
    return negative ? -signed_total : signed_total;
}

// Strict UTC "YYYY-MM-DDTHH:MM:SSZ", Gregorian dates 1970..9999.
// Seconds since Unix epoch. This is an opt-in grammar-specific fast path.
inline std::optional<std::uint64_t> parse_timestamp_utc_seconds(std::string_view s) noexcept {
    if (s.size() != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' ||
        s[13] != ':' || s[16] != ':' || s[19] != 'Z') return std::nullopt;
    auto digit = [&](std::size_t pos) noexcept {
        const char c = s[pos];
        return c >= '0' && c <= '9' ? static_cast<int>(c - '0') : -1;
    };
    constexpr std::size_t positions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (auto pos : positions) if (digit(pos) < 0) return std::nullopt;
    const int y = digit(0)*1000 + digit(1)*100 + digit(2)*10 + digit(3);
    const unsigned m = static_cast<unsigned>(digit(5)*10 + digit(6));
    const unsigned d = static_cast<unsigned>(digit(8)*10 + digit(9));
    const unsigned h = static_cast<unsigned>(digit(11)*10 + digit(12));
    const unsigned min = static_cast<unsigned>(digit(14)*10 + digit(15));
    const unsigned sec = static_cast<unsigned>(digit(17)*10 + digit(18));
    if (y < 1970 || m < 1 || m > 12 || h > 23 || min > 59 || sec > 59) return std::nullopt;
    const bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    constexpr unsigned month_lengths[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const unsigned mdays = month_lengths[m - 1] + static_cast<unsigned>(m == 2 && leap);
    if (d < 1 || d > mdays) return std::nullopt;
    // Civil calendar -> Unix days; arithmetic is bounded by 1970..9999.
    const int yy = y - static_cast<int>(m <= 2);
    const int era = yy / 400;
    const unsigned yoe = static_cast<unsigned>(yy - era * 400);
    const unsigned mp = m > 2 ? m - 3 : m + 9;
    const unsigned doy = (153 * mp + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const auto days = static_cast<std::int64_t>(era) * 146097 + doe - 719468;
    return static_cast<std::uint64_t>(days) * 86400 + h * 3600 + min * 60 + sec;
}
} // namespace newton::parsing
