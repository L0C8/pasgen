#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pasgen {

// Paths cross the API as UTF-8 std::string. On Windows a plain std::string
// is read in the ANSI code page, which breaks any non-ASCII path, so every
// file open goes through this.
std::filesystem::path path_from_utf8(const std::string& utf8);

// Replaces `path` with `data` so that a crash or power cut leaves either the
// old file or the new one, never a torn mix:
//   1. write a sibling temp file, created owner-only (0600 on POSIX),
//   2. flush it to disk (fsync / FlushFileBuffers),
//   3. rename it over the target, then (POSIX) fsync the directory.
// Throws std::runtime_error on failure, after removing the temp file.
void write_file_atomic(const std::string& path, const std::vector<uint8_t>& data);

} // namespace pasgen
