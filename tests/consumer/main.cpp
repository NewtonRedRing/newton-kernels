#include <newton/analytics/csv_fused.hpp>
#include <newton/columnar/views.hpp>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
int main() {
    std::array<std::uint64_t, 3> counts{}, sums{};
    std::array<std::string_view, 3> fields{};
    std::array<char, 32> scratch{};
    const auto result = newton::analytics::parse_filter_group_cents_into(
        "0,9,1.25\n2,1,1.00\n0,8,0.50\n",
        {.filter_greater_than = 5}, counts, sums, fields, scratch);
    const std::array<std::int64_t, 3> values{2, 4, 6};
    const newton::columnar::ColumnView<std::int64_t> view(values);
    return result.records == 3 && result.selected == 2 &&
        counts[0] == 2 && sums[0] == 175 && view.value(2) == 6 ? 0 : 1;
}
