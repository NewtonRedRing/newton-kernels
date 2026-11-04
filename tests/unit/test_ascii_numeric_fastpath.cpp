#include <newton/parsing/ascii_numeric.hpp>
#include <charconv>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

static void require(bool ok) {
    if (!ok) throw std::runtime_error("ASCII u64 fastpath parity regression");
}
static void check(std::string_view text) {
    const auto parsed=newton::parsing::parse_ascii_uint64(text);
    std::uint64_t reference=0;
    auto [end, error]=std::from_chars(text.data(), text.data()+text.size(),reference,10);
    const bool valid=!text.empty() && error==std::errc{} &&
                     end==text.data()+text.size();
    require(static_cast<bool>(parsed)==valid);
    if (valid) require(*parsed==reference);
}
int main() {
    for (const std::string_view text : {
        "", "0", "1", "9", "10", "000", "0001",
        "9999999999999999999", "18446744073709551615",
        "18446744073709551616", "99999999999999999999",
        "000018446744073709551615", "000018446744073709551616",
        "000000000000000000000000000000000000000000000000001",
        "000000000000000000000000000000000000000000000000000",
        "+1", "-1", "-0", " 42", "42 ", "12x", "0x10",
        "00123\n", "18446744073709551615X"
    }) check(text);
    for (int length=0;length<=24;++length) {
        std::string zeros(static_cast<std::size_t>(length),'0');
        check(zeros);
        if (length) {
            std::string nines(static_cast<std::size_t>(length),'9');
            check(nines);
            zeros.back()='1';
            check(zeros);
        }
    }
    std::mt19937_64 rng(0x5b19e24aca);
    const std::string alphabet="0123456789+-x., \r\n";
    for (int trial=0;trial<35000;++trial) {
        const auto len=static_cast<std::size_t>(rng()%100);
        std::string input;
        input.reserve(len);
        for (std::size_t i=0;i<len;++i) {
            const auto ch=alphabet[static_cast<std::size_t>(rng()%alphabet.size())];
            input.push_back(ch);
        }
        check(input);
        // Exercise long leading-zero numbers that remain valid and must take
        // the checked slow path without prematurely rejecting by string length.
        if (trial%7==0) {
            std::string digits(len,'0');
            if (!digits.empty()) digits.back()='9';
            check(digits);
        }
    }
    require(newton::parsing::parse_ascii_uint64("18446744073709551615") ==
            std::optional<std::uint64_t>(std::numeric_limits<std::uint64_t>::max()));
}
