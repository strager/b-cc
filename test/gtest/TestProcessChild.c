#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>

static int
basic_exec_child_has_clean_signal_mask_(void) {
    sigset_t old_mask;
    int rc = sigprocmask(SIG_SETMASK, NULL, &old_mask);
    if (rc != 0) {
        perror("sigprocmask");
        return 1;
    }
    if (sigismember(&old_mask, SIGCHLD)) {
        fprintf(stderr, "SIGCHLD is masked\n");
        return 1;
    }
    // TODO(strager): Test other important signals.
    return 0;
}

int
main(
        int argc,
        char **argv) {
    if (argc != 2) {
        fprintf(stderr, "error: expected argument\n");
        return 1;
    }
    if (strcmp(
            argv[1],
            "BasicExecChildHasCleanSignalMask") == 0) {
        return basic_exec_child_has_clean_signal_mask_();
    } else {
        (void) fprintf(
            stderr,
            "error: unknown argument %s\n",
            argv[1]);
        return 1;
    }
}
