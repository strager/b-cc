#pragma once

#if !defined(__cplusplus)
# error "Executable.h is C++-only"
#endif

#if defined(__APPLE__)
# include <mach-o/dyld.h>
#elif defined(__linux__)
# include <unistd.h>
# include <vector>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

static inline std::string
get_executable_path() {
# if defined(__APPLE__)
  std::vector<char> buffer(1);
  uint32_t buffer_size;
  static_cast<void>(_NSGetExecutablePath(
    buffer.data(), &buffer_size));
  buffer.resize(buffer_size);
  if (_NSGetExecutablePath(
      buffer.data(), &buffer_size) == -1) {
    abort();
  }
  return std::string(buffer.data(), buffer_size - 1);
# elif defined(__linux__)
  static char const exe_path[] = "/proc/self/exe";
  std::vector<char> buffer(1);
retry:;
  ssize_t rc = readlink(
    exe_path, buffer.data(), buffer.size());
  if (rc == -1) {
    abort();
  }
  if (static_cast<size_t>(rc) >= buffer.size()) {
    buffer.resize(buffer.size() + 1024);
    goto retry;
  }
  return std::string(buffer.data(), buffer.size());
# else
#  warning "Unknown implementation for get_executable_path"
# endif
}

static inline std::string
dirname(
    std::string path) {
  size_t separator_index = path.find_last_of("\\/");
  if (separator_index == std::string::npos) {
    return std::string();
  } else {
    return std::string(path, 0, separator_index);
  }
}
