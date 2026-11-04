#include <newton/parsing/csv_parser.hpp>

namespace newton::parsing {
std::size_t CsvParser::parse(std::string_view input, const RowCallback& on_row) const {
    return detail::parse_csv_impl(input, on_row);
}
} // namespace newton::parsing
