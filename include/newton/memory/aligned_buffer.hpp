#pragma once
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <span>
#include <type_traits>
#include <utility>

namespace newton::memory {
// Uninitialized storage suitable for trivial types; no constructor/destructor calls.
template<class T, std::size_t Alignment = 64>
class AlignedBuffer {
    static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>);
    static_assert(Alignment >= alignof(T) && (Alignment & (Alignment - 1)) == 0);
public:
    explicit AlignedBuffer(std::size_t count) : size_(count) {
        if (count > static_cast<std::size_t>(-1) / sizeof(T)) throw std::length_error("buffer overflow");
        if (count) data_ = static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{Alignment}));
    }
    ~AlignedBuffer() { release(); }
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    AlignedBuffer(AlignedBuffer&& rhs) noexcept : data_(std::exchange(rhs.data_, nullptr)), size_(std::exchange(rhs.size_, 0)) {}
    AlignedBuffer& operator=(AlignedBuffer&& rhs) noexcept {
        if (this != &rhs) { release(); data_ = std::exchange(rhs.data_, nullptr); size_ = std::exchange(rhs.size_, 0); }
        return *this;
    }
    // Communicate the allocation's guaranteed alignment to optimizer
    // vectorization and load/store scheduling. Null empty/moved-from buffers
    // do not enter std::assume_aligned.
    [[nodiscard]] T* data() noexcept {
        return data_ ? std::assume_aligned<Alignment>(data_) : nullptr;
    }
    [[nodiscard]] const T* data() const noexcept {
        return data_ ? std::assume_aligned<Alignment>(data_) : nullptr;
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::span<T> span() noexcept { return {data(), size_}; }
private:
    void release() noexcept { if (data_) ::operator delete(data_, std::align_val_t{Alignment}); }
    T* data_ = nullptr;
    std::size_t size_ = 0;
};
} // namespace newton::memory
