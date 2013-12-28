#ifndef VTABLE_H_01F5AC11_3675_4990_8792_682951499993
#define VTABLE_H_01F5AC11_3675_4990_8792_682951499993

#define B_DEFINE_VTABLE_FUNCTION(type, name, ...) \
    static type const * \
    name( \
        void) { \
    \
        static type _vtable; \
        /* FIXME(strager): Is this thread-safe? */ \
        _vtable = ((type) __VA_ARGS__); \
        return &_vtable; \
    }

#endif
