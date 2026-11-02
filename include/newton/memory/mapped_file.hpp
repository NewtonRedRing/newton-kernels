#pragma once
#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

namespace newton::memory {
class MappedFile {
public:
    explicit MappedFile(const std::filesystem::path& path);
    ~MappedFile() noexcept;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return {data_, size_}; }
    [[nodiscard]] std::string_view text() const noexcept {
        return {reinterpret_cast<const char*>(data_), size_};
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
private:
    void close() noexcept;
    const std::byte* data_ = nullptr;
    std::size_t size_ = 0;
#ifdef _WIN32
    void* file_ = nullptr;
    void* mapping_ = nullptr;
#endif
};
} // namespace newton::memory
