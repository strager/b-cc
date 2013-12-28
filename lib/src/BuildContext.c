#include <B/BuildContext.h>
#include <B/Internal/Allocate.h>

struct B_BuildContext {
    struct B_Client *const client;
};

struct B_BuildContext *
b_build_context_allocate(
    struct B_Client *client) {

    B_ALLOCATE_RETURN(struct B_BuildContext, {
        .client = client,
    });
}

void
b_build_context_deallocate(
    struct B_BuildContext *build_context) {

    free(build_context);
}

struct B_Client *
b_build_context_client(
    struct B_BuildContext const *build_context) {

    return build_context->client;
}
