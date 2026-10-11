/*
 * lpath.c -- see lpath.h.
 */

#include "lpath.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#else
#include <sys/statvfs.h>
#include <unistd.h>
#endif

int lp_self(char *out, int outlen)
{
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)outlen);
    return n > 0 && n < (DWORD)outlen;
#elif defined(__APPLE__)
    /* _NSGetExecutablePath can hand back a path with symlinks and .. in it, which
     * realpath then settles. Both matter: the caller renames this file, and a
     * rename through an unresolved path can miss. */
    char raw[4096];
    unsigned int n = sizeof raw;
    if (_NSGetExecutablePath(raw, &n) != 0)
        return 0;
    {
        char resolved[4096];
        if (realpath(raw, resolved))
            snprintf(out, (size_t)outlen, "%s", resolved);
        else
            snprintf(out, (size_t)outlen, "%s", raw);
    }
    return 1;
#else
    ssize_t n = readlink("/proc/self/exe", out, (size_t)outlen - 1);
    if (n <= 0)
        return 0;
    out[n] = '\0';
    return 1;
#endif
}

void lp_dirname(char *path)
{
    char *slash = strrchr(path, '/');
#ifdef _WIN32
    char *back = strrchr(path, '\\');
    if (back > slash)
        slash = back;
#endif
    if (slash && slash != path)
        *slash = '\0';
    else if (slash)
        slash[1] = '\0';
    else
        snprintf(path, 2, ".");
}

int lp_free_bytes(const char *dir, long long *out)
{
#ifdef _WIN32
    /* The bytes free TO THIS USER, which a disk quota can make smaller than the bytes
     * free on the disk. The folder rather than its drive letter, so a folder mounted
     * from another volume answers for that volume. */
    ULARGE_INTEGER avail, total, free_all;
    if (!dir || !*dir || !GetDiskFreeSpaceExA(dir, &avail, &total, &free_all))
        return 0;
    *out = (long long)avail.QuadPart;
    return 1;
#else
    /* f_bavail, not f_bfree: the blocks an ordinary user may still take, without the
     * reserve the file system keeps back for the system. */
    struct statvfs v;
    if (!dir || !*dir || statvfs(dir, &v) != 0)
        return 0;
    *out = (long long)v.f_bavail * (long long)v.f_frsize;
    return 1;
#endif
}
