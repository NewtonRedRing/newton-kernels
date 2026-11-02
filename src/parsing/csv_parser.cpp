#include <newton/parsing/csv_parser.hpp>
#include <stdexcept>
#include <utility>
namespace newton::parsing {
std::size_t CsvParser::parse(std::string_view input, const RowCallback& on_row) const {
    enum class State { start, unquoted, quoted, after_quote };
    State state = State::start;
    CsvRow row;
    std::string field;
    std::size_t records = 0;
    if (input.empty()) return 0;
    auto finish_field = [&] { row.emplace_back(std::move(field)); field.clear(); state = State::start; };
    auto finish_row = [&] { finish_field(); on_row(row); ++records; row.clear(); };
    bool end_on_newline = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (state == State::quoted) {
            if (c == '"') {
                if (i + 1 < input.size() && input[i + 1] == '"') { field += '"'; ++i; }
                else state = State::after_quote;
            } else field += c;
            end_on_newline = false;
            continue;
        }
        if (c == ',' && state != State::quoted) {
            finish_field();
            end_on_newline = false;
            continue;
        }
        if (c == '\r' || c == '\n') {
            finish_row();
            if (c == '\r' && i + 1 < input.size() && input[i + 1] == '\n') ++i;
            end_on_newline = true;
            continue;
        }
        if (state == State::after_quote) throw std::invalid_argument("Unexpected character after closing CSV quote");
        if (c == '"') {
            if (state != State::start) throw std::invalid_argument("Quote in unquoted CSV field");
            state = State::quoted;
        } else {
            field += c;
            state = State::unquoted;
        }
        end_on_newline = false;
    }
    if (state == State::quoted) throw std::invalid_argument("Unterminated CSV quoted field");
    if (!end_on_newline) finish_row();
    return records;
}
} // namespace newton::parsing
