#pragma once

#if !defined(__cplusplus)
# error "B/Private/TemporaryDirectory.h is C++-only"
#endif

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

class B_TemporaryDirectory {
public:
  // Non-copyable.
#if __cplusplus >= 201100L
  B_TemporaryDirectory(
      B_TemporaryDirectory const &) = delete;
  B_TemporaryDirectory &
    operator=(
        B_TemporaryDirectory const &) = delete;
#endif

  // Moveable.
#if __cplusplus >= 201100L
  B_TemporaryDirectory(
      B_TemporaryDirectory &&other) :
      path_(other.release_path_()) {
  }

  B_TemporaryDirectory &
  operator=(
      B_TemporaryDirectory &&other) {
    if (&other == this) {
      return *this;
    }

    // FIXME(strager): Not exception-safe.
    this->maybe_delete_();
    this->path_ = other.release_path_();
    return *this;
  }
#endif

  ~B_TemporaryDirectory() {
    this->maybe_delete_();
  }

  // Factory method.  Creates a temporary directory in the
  // system's temporary file directory.
  static B_TemporaryDirectory
  create() {
    // FIXME(strager): Insecure.
    // FIXME(strager): Non-portable.
    char path[] = "/tmp/b_unit_test.XXXXXXXXXXXXXXXX";
    if (mkdtemp(path) != path) {
      throw std::exception();
    }
    return B_TemporaryDirectory(path);
  }

  std::string const &
  path() const {
    return path_;
  }

private:
  B_TemporaryDirectory(
      std::string path) :
      path_(path) {
  }

  std::string
  release_path_() {
    std::string path;
    std::swap(this->path_, path);
    return path;
  }

  void
  maybe_delete_() {
    // FIXME(strager): Not exception-safe.
    if (this->path_.empty()) {
      return;
    }
    size_t fdCount = 10;
    int rc = nftw(
      this->path_.c_str(),
      remove_path,
      fdCount,
      FTW_DEPTH | FTW_PHYS);
    if (rc != 0) {
      abort();
    }
    this->path_ = std::string();
  }

  std::string path_;

private:
  static int
  remove_path(
      char const *path,
      struct stat const *,
      int,
      struct FTW *) {
    return remove(path);
  }
};
