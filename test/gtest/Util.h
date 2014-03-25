#ifndef B_HEADER_GUARD_D40B1884_760C_46BE_8987_6313D7377327
#define B_HEADER_GUARD_D40B1884_760C_46BE_8987_6313D7377327

#if defined(__cplusplus)
# include <exception>
# include <ftw.h>
# include <unistd.h>

# define B_RETURN_OUTPTR(_type, _expr) \
    ([&]() -> _type { \
        _type _; \
        if ((_expr)) { \
            return _; \
        } else { \
            throw std::exception(); \
        } \
    }())

class B_TemporaryDirectory {
public:
    // Non-copyable.
    B_TemporaryDirectory(
            B_TemporaryDirectory const &) = delete;
    B_TemporaryDirectory &
    operator=(
            B_TemporaryDirectory const &) = delete;

    // Moveable.
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
        int rc = nftw(this->path_.c_str(), [](
                char const *path,
                struct stat const *,
                int,
                struct FTW *) {
            return remove(path);
        }, fdCount, FTW_DEPTH | FTW_PHYS);
        ASSERT_EQ(0, rc);
        this->path_ = std::string();
    }

    std::string path_;
};
#endif

#endif
