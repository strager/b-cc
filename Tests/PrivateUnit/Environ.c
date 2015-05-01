#if defined(__APPLE__)
# include <crt_externs.h>

char **
get_environ(
    void) {
  return *_NSGetEnviron();
}
#else
char **
get_environ(
    void) {
  extern char **environ;
  return environ;
}
#endif
