#include <newton/memory/mapped_file.hpp>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>
#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace newton::memory {
MappedFile::MappedFile(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateFileW");
    file_ = f;
    try {
        LARGE_INTEGER n{};
        if (!GetFileSizeEx(f, &n) || n.QuadPart < 0 || static_cast<unsigned long long>(n.QuadPart) > std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("GetFileSizeEx failed or file too large");
        size_ = static_cast<std::size_t>(n.QuadPart);
        if (size_ == 0) return;
        HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!m) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateFileMappingW");
        mapping_ = m;
        data_ = static_cast<const std::byte*>(MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0));
        if (!data_) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "MapViewOfFile");
    } catch (...) { close(); throw; }
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "open");
    struct stat st{};
    if (::fstat(fd, &st) != 0 || st.st_size < 0 || static_cast<unsigned long long>(st.st_size) > std::numeric_limits<std::size_t>::max()) {
        const int code = errno ? errno : EOVERFLOW;
        ::close(fd);
        throw std::system_error(code, std::generic_category(), "fstat");
    }
    size_ = static_cast<std::size_t>(st.st_size);
    if (size_ == 0) { ::close(fd); return; }
    void* m = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
    const int code = errno;
    ::close(fd);
    if (m == MAP_FAILED) { size_ = 0; throw std::system_error(code, std::generic_category(), "mmap"); }
    data_ = static_cast<const std::byte*>(m);
#endif
}

MappedFile::~MappedFile() noexcept { close(); }
void MappedFile::close() noexcept {
#ifdef _WIN32
    if (data_) UnmapViewOfFile(data_);
    if (mapping_) CloseHandle(static_cast<HANDLE>(mapping_));
    if (file_) CloseHandle(static_cast<HANDLE>(file_));
    file_ = nullptr;
    mapping_ = nullptr;
#else
    if (data_) ::munmap(const_cast<std::byte*>(data_), size_);
#endif
    data_ = nullptr;
    size_ = 0;
}
MappedFile::MappedFile(MappedFile&& other) noexcept { *this = std::move(other); }
MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this == &other) return *this;
    close();
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
#ifdef _WIN32
    file_ = std::exchange(other.file_, nullptr);
    mapping_ = std::exchange(other.mapping_, nullptr);
#endif
    return *this;
}
} // namespace newton::memory
