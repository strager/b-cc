#ifndef B_HEADER_GUARD_D40B1884_760C_46BE_8987_6313D7377327
#define B_HEADER_GUARD_D40B1884_760C_46BE_8987_6313D7377327

#if defined(__cplusplus)
# include <exception>

# define B_RETURN_OUTPTR(_type, _expr) \
    ([&]() -> _type { \
        _type _; \
        if ((_expr)) { \
            return _; \
        } else { \
            throw std::exception(); \
        } \
    }())
#endif

#endif
