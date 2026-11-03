#include <newton/analytics/csv_fused.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <newton/parsing/csv_records.hpp>
#include <newton/parsing/csv_fields.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <stdexcept>

// Coverage-guided robustness target. Input bytes may be arbitrary and invalid;
// exceptions are expected, crashes, UB and memory safety violations are not.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > (1u << 18)) return 0;
    std::string_view input(reinterpret_cast<const char*>(data), size);
    std::vector<std::string_view> fields(64);
    std::vector<char> scratch(size + 1);
    try {
        (void)newton::parsing::for_each_csv_record(input, [&](std::string_view raw) {
            try {
                (void)newton::parsing::decode_csv_fields_into(raw, fields, scratch);
            } catch (const std::exception&) {}
        });
    } catch (const std::exception&) {}
    try {
        newton::parsing::CsvParser parser;
        (void)parser.parse(input, [](const newton::parsing::CsvRow&) {});
    } catch (const std::exception&) {}
    try {
        std::uint64_t counts[16]{}, sums[16]{};
        (void)newton::analytics::parse_filter_group_cents_into(
            input, {}, counts, sums, fields, scratch);
    } catch (const std::exception&) {}
    return 0;
}
