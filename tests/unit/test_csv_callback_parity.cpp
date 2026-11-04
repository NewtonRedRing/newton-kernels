#include <newton/parsing/csv_parser.hpp>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using newton::parsing::CsvParser;
using newton::parsing::CsvRow;

static void require(bool value) {
    if (!value) throw std::runtime_error("CSV erased/concrete callback mismatch");
}

struct Output {
    std::vector<CsvRow> rows;
    std::size_t n = 0;
    bool invalid = false;
};
static Output parse_erased(std::string_view text) {
    Output out;
    const CsvParser::RowCallback callback = [&](const CsvRow& row) {
        out.rows.push_back(row);
    };
    try { out.n = CsvParser{}.parse(text, callback); }
    catch (const std::invalid_argument&) { out.invalid=true; }
    return out;
}
static Output parse_concrete(std::string_view text) {
    Output out;
    try {
        out.n=CsvParser{}.parse(text,[&](const CsvRow& row) {
            out.rows.push_back(row);
        });
    } catch (const std::invalid_argument&) { out.invalid=true; }
    return out;
}
static Output parse_specialized(std::string_view text) {
    Output out;
    try {
        out.n=CsvParser{}.parse_inline(text,[&](const CsvRow& row) {
            out.rows.push_back(row);
        });
    } catch (const std::invalid_argument&) { out.invalid=true; }
    return out;
}
static void parity(std::string_view text) {
    const auto a=parse_erased(text), b=parse_concrete(text);
    require(a.invalid==b.invalid);
    require(a.n==b.n);
    require(a.rows==b.rows);
    const auto c=parse_specialized(text);
    require(a.invalid==c.invalid && a.n==c.n && a.rows==c.rows);
    if (!a.invalid) require(a.n==a.rows.size());
}
int main() {
    for (std::string_view s : {
            "", "a,b\n", "a,b\r\n", "a,b\r", "a,b",
            ",", "a,", ",a", "\n", "\r\n", "\"\"",
            "\"a,b\",c", "\"a\nb\",z", "\"a\"\"b\",z",
            "\"unterminated", "a\"bad,b", "\"a\"z,b",
            "\"a\",\"b\"\r\n\"c\"\"d\",q\n",
            "\"a\r\nb\",c\r\n" }) parity(s);

    std::mt19937_64 rng(0x569a1075);
    const std::string characters="abcd0123,\r\n\" ";
    for (int trial=0; trial<3000; ++trial) {
        std::string text;
        const auto size=static_cast<std::size_t>(rng()%150);
        text.reserve(size);
        for (std::size_t i=0;i<size;++i)
            text.push_back(characters[static_cast<std::size_t>(rng()%characters.size())]);
        parity(text);
    }
    // Long quote escaping, newline spans and callback lifetime/copy parity.
    std::string long_input;
    for (int i=0;i<2000;++i) {
        long_input += std::to_string(i);
        long_input += ",\"escaped \"\"value\"\"\",";
        long_input += (i%3==0 ? "\"multiline\nfield\"\r\n" : "tail\n");
    }
    parity(long_input);
}
