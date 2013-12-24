#ifndef IDENTITY_H_1D83BD3B_407B_466A_B7CD_51C87B765678
#define IDENTITY_H_1D83BD3B_407B_466A_B7CD_51C87B765678

#ifdef __cplusplus
extern "C" {
#endif

struct B_Identity {
    char const *const data;
    size_t const data_size;
};

struct B_Identity *
b_identity_allocate(
    char const *data,
    size_t data_size);

void
b_identity_deallocate(
    struct B_Identity *);

#if 0
struct B_Identity *
b_identity_allocate_random(
    void);
#endif

#ifdef __cplusplus
}
#endif

#endif
