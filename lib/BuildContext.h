#ifndef BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3
#define BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3

#ifdef __cplusplus
extern "C" {
#endif

struct B_BuildContext;
struct B_Client;

struct B_BuildContext *
b_build_context_allocate(
    struct B_Client *);

void
b_build_context_deallocate(
    struct B_BuildContext *);

struct B_Client *
b_build_context_client(
    struct B_BuildContext const *);

#ifdef __cplusplus
}
#endif

#endif
