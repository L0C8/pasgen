#include "util/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <cstring>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace pasgen {

std::filesystem::path path_from_utf8(const std::string& utf8) {
    return std::filesystem::u8path(utf8);
}

#ifdef _WIN32

void write_file_atomic(const std::string& path, const std::vector<uint8_t>& data) {
    const std::filesystem::path target = path_from_utf8(path);
    const std::filesystem::path tmp = path_from_utf8(path + ".tmp");

    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to write: " + path + ".tmp");

    bool ok = true;
    size_t off = 0;
    while (ok && off < data.size()) {
        DWORD chunk = (DWORD)std::min<size_t>(data.size() - off, 1u << 30);
        DWORD written = 0;
        ok = WriteFile(h, data.data() + off, chunk, &written, nullptr) && written == chunk;
        off += written;
    }
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);

    if (ok) ok = MoveFileExW(tmp.c_str(), target.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) {
        DeleteFileW(tmp.c_str());
        throw std::runtime_error("Failed to write: " + path);
    }
}

#else

namespace {
bool write_all(int fd, const uint8_t* p, size_t n) {
    while (n > 0) {
        ssize_t w = ::write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w;
        n -= (size_t)w;
    }
    return true;
}
} // namespace

void write_file_atomic(const std::string& path, const std::vector<uint8_t>& data) {
    const std::string tmp = path + ".tmp";

    // 0600 regardless of umask: the vault is encrypted, but there is no reason
    // for other local users to be able to copy it and attack it offline.
    int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) throw std::runtime_error("Failed to write: " + tmp + " (" + std::strerror(errno) + ")");
    ::fchmod(fd, 0600); // O_CREAT keeps the old mode if a stale temp file already existed

    bool ok = write_all(fd, data.data(), data.size()) && ::fsync(fd) == 0;
    ok = (::close(fd) == 0) && ok;
    if (ok) ok = ::rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok) {
        const int err = errno;
        ::unlink(tmp.c_str());
        throw std::runtime_error("Failed to write: " + path + " (" + std::strerror(err) + ")");
    }

    // Make the rename itself durable.
    std::string dir = std::filesystem::path(path).parent_path().string();
    if (dir.empty()) dir = ".";
    int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0) { ::fsync(dfd); ::close(dfd); }
}

#endif

} // namespace pasgen
