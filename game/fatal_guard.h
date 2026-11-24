/* ---------------------------------------------------------------------------------- *
 *  A LINE FOR EVERY FATAL END, and a main thread whose stack is committed.
 *
 *  The crash-stage handler further down cnc_eyes.cpp prints "FATAL: signal N while:
 *  <stage>" for the C signals. Three ways to die never reached it, and each ended the game
 *  with no line at all:
 *
 *  - AN UNCAUGHT EXCEPTION. std::terminate called abort, which did reach the handler, but
 *    the line said "signal 6" and not what was thrown. The terminate handler here prints
 *    the exception's what() and the stage, then hands over to the handler that was there
 *    before (the runtime's own, or the game program's) and abort follows as before.
 *
 *  - A STACK OVERFLOW ON WINDOWS. It is a structured exception, which no C signal sees,
 *    and the stack it happens on has no room left to run much. A vectored handler, which
 *    sees every exception first, writes a line built from fixed text and the stage string
 *    with WriteFile to the standard-error handle taken in advance: no heap, no C runtime,
 *    and nothing on the stack but a few words. It then lets the search go on, so the
 *    process ends exactly as it would have.
 *    A Windows thread commits its stack page by page as it grows. Under a commit limit (a
 *    job object's cap, or a machine whose commit is spent) the next page can be refused,
 *    and that is reported as a stack overflow however little of the stack is in use. So
 *    the game programs are linked with 512 KB of every thread's stack committed at the
 *    start (tools/win/build-win.sh, the reserve is the default 2 MB), and the main thread
 *    asks for 64 KB to be kept for the overflow handler (SetThreadStackGuarantee).
 *
 *  - ANY OTHER UNHANDLED EXCEPTION, on any thread, including threads the graphics driver
 *    starts. The process-wide unhandled-exception filter writes the exception's code, its
 *    address and the module it fell in, the thread and the stage, the same way, and then
 *    calls the filter that was installed before it. That earlier filter is the C runtime's,
 *    which is what turns an access violation into the SIGSEGV the crash-stage handler
 *    prints, so that line still comes after this one. A C++ exception on its way to
 *    std::terminate also passes through here and is left to the terminate handler.
 *
 *  Two test switches, read once:
 *    OPENCNC3D_TEST_STACK_OVERFLOW (Windows) overflows a stack at the mission start: =1 a
 *      worker thread's and then the main thread's, =worker or =main only that one. The
 *      worker is let go after its line (only in this test), so =1 shows both lines; the
 *      main thread's overflow then ends the process as any overflow does.
 *    OPENCNC3D_TEST_UNCAUGHT=1 (every platform) throws a std::bad_alloc that nothing
 *      catches at the mission start, to show the terminate line.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_FATAL_GUARD_H
#define OPENCNC3D_FATAL_GUARD_H

#include <exception>

static std::terminate_handler g_fatalPrevTerminate = NULL;

static void fatal_terminate(void)
{
    static int entered = 0;   /* a second terminate from inside this one goes straight on */
    if (!entered++) {
        const char* what = "an exception that is not a std::exception";
        std::exception_ptr e = std::current_exception();
        if (!e) what = "none: std::terminate was called with no exception";
        else {
            try { std::rethrow_exception(e); }
            catch (const std::exception& ex) { what = ex.what(); }
            catch (...) {}
        }
        fprintf(stderr, "\nFATAL: uncaught exception (%s) while: %s\n",
                what ? what : "?", g_bootStage ? g_bootStage : "?");
        fflush(stderr);
        if (g_fatalPrevTerminate && g_fatalPrevTerminate != fatal_terminate) g_fatalPrevTerminate();
    }
    abort();
}

#ifdef _WIN32
#include <io.h>
static HANDLE g_fatalOut = NULL;              /* standard error, as the C runtime has it now */
static volatile LONG g_fatalStackWritten = 0;
static volatile LONG g_fatalFilterWritten = 0;
static char g_fatalLine[768];
static LPTOP_LEVEL_EXCEPTION_FILTER g_fatalPrevFilter = NULL;

/* Building a line with no C runtime: text, a hexadecimal and a decimal number. */
static size_t fatal_put(size_t at, const char* s)
{
    while (s && *s && at + 3 < sizeof g_fatalLine) g_fatalLine[at++] = *s++;
    return at;
}
static size_t fatal_hex(size_t at, unsigned long long v, int digits)
{
    at = fatal_put(at, "0x");
    for (int d = digits - 1; d >= 0 && at + 3 < sizeof g_fatalLine; d--)
        g_fatalLine[at++] = "0123456789ABCDEF"[(v >> (4 * d)) & 15];
    return at;
}
static size_t fatal_dec(size_t at, unsigned long long v)
{
    char t[24];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 24);
    while (n && at + 3 < sizeof g_fatalLine) g_fatalLine[at++] = t[--n];
    return at;
}
static void fatal_emit(size_t at)
{
    g_fatalLine[at++] = '\r';
    g_fatalLine[at++] = '\n';
    if (g_fatalOut) {
        DWORD wrote = 0;
        WriteFile(g_fatalOut, g_fatalLine, (DWORD)at, &wrote, NULL);
    }
}
static DWORD g_fatalMainThread = 0;
static volatile DWORD g_fatalTestWorker = 0;   /* the test switch's worker, let go after its line */
static size_t fatal_thread(size_t at)
{
    const DWORD id = GetCurrentThreadId();
    at = fatal_put(at, " (thread ");
    at = fatal_dec(at, id);
    at = fatal_put(at, id == g_fatalMainThread ? ", the main thread)" : ")");
    return at;
}

static LONG CALLBACK fatal_vectored(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord || ep->ExceptionRecord->ExceptionCode != EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&g_fatalStackWritten, 1, 0) != 0) return EXCEPTION_CONTINUE_SEARCH;
    size_t at = fatal_put(0, "\r\nFATAL: stack overflow while: ");
    at = fatal_put(at, g_bootStage ? g_bootStage : "?");
    at = fatal_thread(at);
    fatal_emit(at);
    if (g_fatalTestWorker && GetCurrentThreadId() == g_fatalTestWorker) {
        g_fatalTestWorker = 0;
        InterlockedExchange(&g_fatalStackWritten, 0);
        ExitThread(1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI fatal_unhandled(EXCEPTION_POINTERS* ep)
{
    const EXCEPTION_RECORD* er = ep ? ep->ExceptionRecord : NULL;
    const DWORD code = er ? er->ExceptionCode : 0;
    /* GCC's own code for a C++ throw ("GCC" in its low bytes): the runtime continues it and
       std::terminate follows, which prints its own line */
    const bool cxx = (code & 0x20ffffffu) == 0x20474343u;
    const bool overflowDone = code == EXCEPTION_STACK_OVERFLOW && g_fatalStackWritten;
    if (!cxx && !overflowDone && InterlockedCompareExchange(&g_fatalFilterWritten, 1, 0) == 0) {
        const uintptr_t where = er ? (uintptr_t)er->ExceptionAddress : 0;
        size_t at = fatal_put(0, "\r\nFATAL: unhandled exception ");
        at = fatal_hex(at, code, 8);
        at = fatal_put(at, " at ");
        at = fatal_hex(at, (unsigned long long)where, 16);
        /* the module it fell in, as module+offset; not on an overflowed stack */
        HMODULE mod = NULL;
        static wchar_t path[MAX_PATH];
        if (code != EXCEPTION_STACK_OVERFLOW && where
            && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                  | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)where, &mod)
            && mod) {
            const DWORD len = GetModuleFileNameW(mod, path, MAX_PATH);
            if (len && len < MAX_PATH) {
                const wchar_t* base = path;
                for (const wchar_t* p = path; *p; p++) if (*p == L'\\' || *p == L'/') base = p + 1;
                at = fatal_put(at, " in ");
                for (; *base && at + 3 < sizeof g_fatalLine; base++)
                    g_fatalLine[at++] = *base < 128 ? (char)*base : '?';
                at = fatal_put(at, "+");
                at = fatal_hex(at, (unsigned long long)(where - (uintptr_t)mod), 8);
            }
        }
        at = fatal_thread(at);
        at = fatal_put(at, " while: ");
        at = fatal_put(at, g_bootStage ? g_bootStage : "?");
        fatal_emit(at);
    }
    return g_fatalPrevFilter ? g_fatalPrevFilter(ep) : EXCEPTION_CONTINUE_SEARCH;
}

/* _get_osfhandle reports a closed descriptor through the invalid-parameter handler, which
   would end the process; it is quietened for the one call. */
static void __cdecl fatal_ignore_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {}
static void fatal_out_refresh(void)
{
    HANDLE h = NULL;
    const int fd = stderr ? _fileno(stderr) : -1;
    if (fd >= 0) {
        const _invalid_parameter_handler was = _set_invalid_parameter_handler(fatal_ignore_parameter);
        const intptr_t os = _get_osfhandle(fd);
        _set_invalid_parameter_handler(was);
        if (os != -1 && os != -2) h = (HANDLE)os;
    }
    if (!h || h == INVALID_HANDLE_VALUE) h = GetStdHandle(STD_ERROR_HANDLE);
    g_fatalOut = h == INVALID_HANDLE_VALUE ? NULL : h;
}

static int __attribute__((noinline)) fatal_test_deep(volatile unsigned char* up, int depth)
{
    volatile unsigned char pad[4096];
    pad[0] = (unsigned char)depth;
    pad[sizeof pad - 1] = up ? up[0] : 0;
    if (depth > 100000000) return pad[0];          /* never: no stack is this deep */
    return fatal_test_deep(pad, depth + 1) + pad[sizeof pad - 1];
}
static DWORD WINAPI fatal_test_worker(LPVOID)
{
    ULONG keep = 65536;                            /* room to write the line and leave */
    SetThreadStackGuarantee(&keep);
    g_fatalTestWorker = GetCurrentThreadId();
    return (DWORD)fatal_test_deep(NULL, 0);
}
#else
static void fatal_out_refresh(void) {}
#endif

/* Once, on the main thread, as early as the program reaches the renderer. */
static void fatal_guard_install(void)
{
    g_fatalPrevTerminate = std::set_terminate(fatal_terminate);
#ifdef _WIN32
    g_fatalMainThread = GetCurrentThreadId();
    fatal_out_refresh();
    if (!AddVectoredExceptionHandler(1, fatal_vectored))
        fprintf(stderr, "FATAL-GUARD|no vectored handler: a stack overflow will leave no line\n");
    g_fatalPrevFilter = SetUnhandledExceptionFilter(fatal_unhandled);
    ULONG keep = 65536;
    if (!SetThreadStackGuarantee(&keep))
        fprintf(stderr, "FATAL-GUARD|the main thread's stack guarantee was refused (error %lu)\n",
                (unsigned long)GetLastError());
#endif
}

/* The test switches, at the mission start (the handlers are in by then). */
static void fatal_guard_tests(void)
{
    const char* uncaught = getenv("OPENCNC3D_TEST_UNCAUGHT");
    if (uncaught && *uncaught && strcmp(uncaught, "0")) {
        fprintf(stderr, "FATAL-GUARD|test|throwing a std::bad_alloc nothing catches\n");
        fflush(stderr);
        throw std::bad_alloc();
    }
#ifdef _WIN32
    const char* overflow = getenv("OPENCNC3D_TEST_STACK_OVERFLOW");
    if (overflow && *overflow && strcmp(overflow, "0")) {
        const bool worker = strcmp(overflow, "main") != 0;
        const bool onMain = strcmp(overflow, "worker") != 0;
        if (worker) {
            fprintf(stderr, "FATAL-GUARD|test|overflowing the stack of a worker thread\n");
            fflush(stderr);
            HANDLE t = CreateThread(NULL, 0, fatal_test_worker, NULL, 0, NULL);
            if (!t) fprintf(stderr, "FATAL-GUARD|test|no worker thread (error %lu)\n",
                            (unsigned long)GetLastError());
            else { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
        }
        if (onMain) {
            fprintf(stderr, "FATAL-GUARD|test|overflowing the stack of the main thread\n");
            fflush(stderr);
            fatal_test_deep(NULL, 0);
        }
    }
#endif
}

#endif
