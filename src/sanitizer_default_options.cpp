// Baked-in default sanitizer runtime options for local sanitizer builds, so
// nobody has to remember to export ASAN_OPTIONS / LSAN_OPTIONS / UBSAN_OPTIONS.
// The environment still wins: any option set via the env vars is merged on top
// of (and overrides) these defaults at startup.
//
// Compiled into the target only when a sanitizer is enabled (see CMakeLists).

extern "C" __attribute__((used)) const char* __asan_default_options()
{
    // Strict checks; abort (=> core) instead of continuing so we get a stack.
    //
    // To resolve "<unknown module>" frames in leak/error reports, add
    //   fast_unwind_on_malloc=0
    // here or via `export ASAN_OPTIONS=fast_unwind_on_malloc=0` (it merges with
    // these defaults). It gives full malloc stacks but noticeably slows every
    // allocation, so it's left off by default — flip it on only when hunting an
    // unsymbolized allocation.
    return "abort_on_error=1"
           ":detect_stack_use_after_return=1"
           ":strict_string_checks=1";
}

extern "C" __attribute__((used)) const char* __ubsan_default_options()
{
    return "print_stacktrace=1:halt_on_error=1";
}

#ifdef STV_LSAN_SUPPRESSIONS
extern "C" __attribute__((used)) const char* __lsan_default_options()
{
    // Suppress the third-party at-exit leaks (fontconfig/pango/gtk/libdecor/…).
    return "suppressions=" STV_LSAN_SUPPRESSIONS ":print_suppressions=0";
}
#endif
