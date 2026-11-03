#include <newton/analytics/columnar_kernels.hpp>
#include <newton/analytics/csv_fused.hpp>
#include <newton/columnar/arrow_c_data.hpp>
#include <newton/columnar/views.hpp>
#include <newton/columnar/simd_filter.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <newton/parsing/ascii_numeric.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
template<class E, class F>
void must_throw(F&& f) {
    bool caught = false;
    try { f(); } catch (const E&) { caught = true; }
    require(caught, "expected input validation exception");
}

struct TestArrowSchema {
    const char* format = "l";
    std::int64_t n_children = 0;
    void* dictionary = nullptr;
    void (*release)(TestArrowSchema*) = nullptr;
};
struct TestArrowArray {
    std::int64_t length = 0, offset = 0, null_count = 0, n_buffers = 2, n_children = 0;
    const void** buffers = nullptr;
    void* dictionary = nullptr;
    void (*release)(TestArrowArray*) = nullptr;
};
void release_array(TestArrowArray*) {}
void release_schema(TestArrowSchema*) {}

void test_column_views() {
    using namespace newton::columnar;
    const std::array<std::int64_t, 6> input{-1, 2, 3, 4, 5, 6};
    const std::array<std::uint64_t, 6> weights{7, 20, 30, 40, 50, 60};
    const std::array<std::uint8_t, 1> valid{0b00101111}; // 0,1,2,3,5 valid
    const auto pred = ColumnView<std::int64_t>(input, {valid});
    const auto val = ColumnView<std::uint64_t>(weights, {valid});
    std::array<std::uint32_t, 6> selected{};
    auto count = newton::analytics::select_greater_into(pred, std::int64_t{2}, selected);
    require(count == 2 && selected[0] == 3 && selected[1] == 5, "null-aware selection");
    const auto summed = newton::analytics::sum_where_greater(pred, val, 2);
    require(summed.count == 2 && summed.sum == 100, "fused null-aware sum");
    must_throw<std::length_error>([&] {
        (void)newton::analytics::select_greater_into(pred, std::int64_t{2},
            std::span<std::uint32_t>(selected).first(2));
    });
    must_throw<std::length_error>([&] {
        const std::array<std::uint8_t, 1> short_mask{};
        (void)ColumnView<std::int64_t>(input, {short_mask, 7});
    });
    const std::array<std::int32_t, 4> offsets{0, 1, 1, 4};
    const StringColumnView words(offsets, "abcd");
    require(words.size() == 3 && words.value(0) == "a" &&
        words.value(1).empty() && words.value(2) == "bcd", "borrowed string offsets");
    const std::array<std::int32_t, 2> invalid_offsets{2, 1};
    must_throw<std::invalid_argument>([&] { (void)StringColumnView(invalid_offsets, "a"); });
    const std::array<ColumnDescriptor, 2> descriptors{{
        {ColumnKind::I64, input.data(), input.size()},
        {ColumnKind::U64, weights.data(), weights.size()}
    }};
    require(RecordBatchView(descriptors).rows() == 6, "borrowed heterogeneous batch");
}

void test_arrow() {
    const std::array<std::int64_t, 5> values{10, 20, 30, 40, 50};
    const std::array<std::uint8_t, 1> valid{0b00011011}; // valid bits 0,1,3,4
    std::array<const void*, 2> buffers{valid.data(), values.data()};
    TestArrowArray array;
    array.length = 3;
    array.offset = 1;
    array.null_count = 1;
    array.buffers = buffers.data();
    array.release = &release_array;
    TestArrowSchema schema;
    schema.release = &release_schema;
    const auto view = newton::columnar::borrow_arrow_primitive<std::int64_t>(array, schema);
    require(view.size() == 3 && view.value(0) == 20 &&
        view.valid(0) && !view.valid(1) && view.valid(2),
        "Arrow zero-copy offset and validity adaptation");
    array.n_buffers = 1;
    must_throw<std::invalid_argument>([&] {
        (void)newton::columnar::borrow_arrow_primitive<std::int64_t>(array, schema);
    });
    array.n_buffers = 2;
    schema.format = "I";
    must_throw<std::invalid_argument>([&] {
        (void)newton::columnar::borrow_arrow_primitive<std::int64_t>(array, schema);
    });
}

void test_simd_filter() {
    std::mt19937_64 random(0x91bec1ULL);
    for (std::size_t n = 0; n < 513; ++n) {
        std::vector<std::uint64_t> values(n);
        std::vector<std::uint32_t> output(n), expected;
        const auto threshold = random();
        for (auto& value : values) value = random();
        if (n > 0) values[0] = std::numeric_limits<std::uint64_t>::max();
        if (n > 1) values[1] = 0;
        if (n > 2) values[2] = threshold;
        for (std::size_t i = 0; i < n; ++i)
            if (values[i] > threshold)
                expected.push_back(static_cast<std::uint32_t>(i));
        const auto size = newton::columnar::select_gt_u64_into(values, threshold, output);
        require(size == expected.size() &&
            std::equal(expected.begin(), expected.end(), output.begin()),
            "AOT SIMD/NEON filter differs from independent scalar");
    }
}
void test_random_columnar() {
    std::mt19937_64 random(0x4526bacfedULL);
    for (int run = 0; run < 120; ++run) {
        const auto n = static_cast<std::size_t>(random() % 3200);
        std::vector<std::int64_t> predicate(n);
        std::vector<std::uint64_t> weights(n);
        std::vector<std::uint8_t> mask((n + 7) / 8, 0);
        for (std::size_t i = 0; i < n; ++i) {
            predicate[i] = static_cast<std::int64_t>(random() % 1000);
            weights[i] = random() % 10000;
            if (random() % 5) mask[i / 8] |= static_cast<std::uint8_t>(1 << (i % 8));
        }
        const newton::columnar::ValidityView validity{
            std::span<const std::uint8_t>(mask.data(), mask.size())};
        const auto p = newton::columnar::ColumnView<std::int64_t>(predicate, validity);
        const auto v = newton::columnar::ColumnView<std::uint64_t>(weights, validity);
        std::vector<std::uint32_t> ids(n);
        const auto got = newton::analytics::select_greater_into(p, std::int64_t{500}, ids);
        const auto folded = newton::analytics::sum_where_greater(p, v, 500);
        std::uint64_t count = 0, sum = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (!(mask[i / 8] & (1 << (i % 8))) || predicate[i] <= 500) continue;
            require(ids[count] == i, "selection ordering or row mismatch");
            ++count;
            sum += weights[i];
        }
        require(count == got && folded.count == count && folded.sum == sum,
                "random columnar differential mismatch");
    }
}

void test_csv_fused() {
    const std::string sample = "group,filter,value\r\n"
        "2,5,1.25\r\n"
        "1,9,2.50\n"
        "\"2\",10,\"1.75\"\n"
        "1,1,0.00\n"
        "0,3,7\n";
    std::array<std::string_view, 4> fields{};
    std::array<char, 100> scratch{};
    std::array<std::uint64_t, 3> counts{}, sums{};
    const auto result = newton::analytics::parse_filter_group_cents_into(
        sample, {.filter_greater_than = 4, .has_header = true},
        counts, sums, fields, scratch);
    require(result.records == 5 && result.selected == 3 &&
            counts[1] == 1 && counts[2] == 2 &&
            sums[1] == 250 && sums[2] == 300,
            "fused CSV parse-filter-aggregate mismatch");

    std::mt19937 random(87129);
    for (int run = 0; run < 50; ++run) {
        std::string csv;
        std::array<std::uint64_t, 10> want_counts{}, want_sums{};
        const int n = 100 + run * 27;
        std::size_t selected = 0;
        for (int i = 0; i < n; ++i) {
            const auto group = static_cast<std::size_t>(random() % 10);
            const auto predicate = static_cast<unsigned>(random() % 100);
            const auto cents = static_cast<unsigned>(random() % 20000);
            const auto quote = (i % 7 == 0);
            if (quote) csv += '"';
            csv += std::to_string(group);
            if (quote) csv += '"';
            csv += "," + std::to_string(predicate) + ",";
            csv += std::to_string(cents / 100) + ".";
            if (cents % 100 < 10) csv += "0";
            csv += std::to_string(cents % 100);
            csv += (i % 2 == 0 ? "\r\n" : "\n");
            if (predicate > 45) {
                ++want_counts[group];
                want_sums[group] += cents;
                ++selected;
            }
        }
        std::array<std::uint64_t, 10> actual_counts{}, actual_sums{};
        const auto observed = newton::analytics::parse_filter_group_cents_into(
            csv, {.filter_greater_than = 45}, actual_counts, actual_sums,
            fields, scratch);
        require(observed.records == static_cast<std::size_t>(n) &&
                observed.selected == selected &&
                actual_counts == want_counts && actual_sums == want_sums,
                "random fused CSV versus independent arithmetic");
    }
    must_throw<std::invalid_argument>([&] {
        (void)newton::analytics::parse_filter_group_cents_into(
            "1,2,-5\n", {}, counts, sums, fields, scratch);
    });
    must_throw<std::invalid_argument>([&] {
        (void)newton::analytics::parse_filter_group_cents_into(
            "99,2,1.0\n", {}, counts, sums, fields, scratch);
    });
}
} // namespace

int main() {
    try {
        test_column_views();
        test_simd_filter();
        test_arrow();
        test_random_columnar();
        test_csv_fused();
        std::cout << "Columnar, Arrow and fused-kernel differential tests: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
