#pragma once
#include <string_view>
#include <vector>
#include <utility>
namespace newton::parsing {
// Lightweight zero-copy records for formats WITHOUT quoting or escapes.
template <class Callback>
void for_each_delimited_record(std::string_view input, char delimiter, Callback&& callback) {
    if (input.empty()) return;
    std::vector<std::string_view> fields;
    std::size_t record_start = 0;
    while (record_start < input.size()) {
        std::size_t eol = input.find('\n', record_start);
        if (eol == std::string_view::npos) eol = input.size();
        std::size_t record_end = eol;
        if (record_end > record_start && input[record_end - 1] == '\r') --record_end;
        std::size_t begin = record_start;
        while (begin <= record_end) {
            auto sep = input.find(delimiter, begin);
            if (sep == std::string_view::npos || sep >= record_end) sep = record_end;
            fields.push_back(input.substr(begin, sep - begin));
            if (sep == record_end) break;
            begin = sep + 1;
        }
        callback(fields);
        fields.clear();
        record_start = eol == input.size() ? input.size() : eol + 1;
    }
}
} // namespace newton::parsing
