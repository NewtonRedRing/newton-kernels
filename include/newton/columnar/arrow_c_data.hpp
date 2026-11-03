#pragma once
#include <newton/columnar/views.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#if __has_include(<arrow/c/abi.h>)
#include <arrow/c/abi.h>
#endif

namespace newton::columnar {

// Compatible with the official Apache Arrow C Data Interface structures.
// The caller includes arrow/c/abi.h (or supplies ABI-compatible types).
// This adapter BORROWS buffers; it does not steal or call release callbacks.
// Call release only AFTER the imported ColumnView is no longer used.
template<class T> struct ArrowFormat;
template<> struct ArrowFormat<std::int32_t> { static constexpr std::string_view value = "i"; };
template<> struct ArrowFormat<std::uint32_t> { static constexpr std::string_view value = "I"; };
template<> struct ArrowFormat<std::int64_t> { static constexpr std::string_view value = "l"; };
template<> struct ArrowFormat<std::uint64_t> { static constexpr std::string_view value = "L"; };
template<> struct ArrowFormat<float> { static constexpr std::string_view value = "f"; };
template<> struct ArrowFormat<double> { static constexpr std::string_view value = "g"; };

template<class T, class ArrowArrayT, class ArrowSchemaT>
[[nodiscard]] ColumnView<T> borrow_arrow_primitive(
        const ArrowArrayT& array, const ArrowSchemaT& schema) {
    if (array.release == nullptr || schema.release == nullptr)
        throw std::invalid_argument("Arrow array/schema already released");
    if (!schema.format || std::string_view(schema.format) != ArrowFormat<T>::value ||
        array.n_children != 0 || schema.n_children != 0 ||
        array.dictionary || schema.dictionary)
        throw std::invalid_argument("Unsupported Arrow type, children or dictionary");
    if (array.length < 0 || array.offset < 0 ||
        array.n_buffers != 2 || !array.buffers ||
        array.null_count < -1 || array.null_count > array.length)
        throw std::invalid_argument("Invalid Arrow primitive metadata");
    const auto len = static_cast<std::uint64_t>(array.length);
    const auto offset = static_cast<std::uint64_t>(array.offset);
    const auto max_elements = static_cast<std::uint64_t>(
        std::numeric_limits<std::ptrdiff_t>::max() / sizeof(T));
    if (len > max_elements || offset > max_elements - len)
        throw std::length_error("Arrow offset/length overflows addressable buffer");
    const auto physical = static_cast<std::size_t>(offset + len);
    const auto* ptr = static_cast<const T*>(array.buffers[1]);
    if (len && !ptr) throw std::invalid_argument("Arrow values are missing");
    if (ptr && reinterpret_cast<std::uintptr_t>(ptr) % alignof(T))
        throw std::invalid_argument("Arrow primitive buffer misaligned");
    std::span<const T> values{};
    if (len) values = {ptr + static_cast<std::size_t>(offset), static_cast<std::size_t>(len)};
    ValidityView validity{};
    if (const auto* bitmap = static_cast<const std::uint8_t*>(array.buffers[0])) {
        const auto nbytes = physical / 8 + (physical % 8 != 0);
        validity = {std::span<const std::uint8_t>(bitmap, nbytes),
                    static_cast<std::size_t>(offset)};
    } else if (array.null_count != 0) {
        throw std::invalid_argument("Missing Arrow null bitmap for unknown/nonzero null count");
    }
    return ColumnView<T>(values, validity);
}

} // namespace newton::columnar
