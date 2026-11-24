#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static std::string sdl_base;
static int sdl_calls, sdl_allocations;
extern "C" char* SDL_GetBasePath(void)
{
    ++sdl_calls;
    if (sdl_base.empty()) return NULL;
    ++sdl_allocations;
    return strdup(sdl_base.c_str());
}
extern "C" void SDL_free(void* p)
{
    assert(p && sdl_allocations > 0);
    --sdl_allocations;
    free(p);
}

#include "../../game/brain_path.h"

static void directory(const std::string& path)
{
    assert(mkdir(path.c_str(), 0700) == 0);
}
static void file(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "wb");
    assert(f);
    assert(fputs("engine fixture", f) >= 0);
    assert(fclose(f) == 0);
}
static void expect(const std::string& expected, const char* explicit_path = NULL)
{
    const char* actual = opencnc3d_find_brain(explicit_path);
    if (actual != expected) {
        fprintf(stderr, "engine path mismatch: expected '%s', got '%s'\n",
                expected.c_str(), actual);
        abort();
    }
    assert(sdl_allocations == 0);
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string root = argv[1];
    const std::string lib = OPENCNC3D_BRAIN_LIB;
    directory(root + "/repo");
    const std::string repo = root + "/repo";
    directory(repo + "/app"); directory(repo + "/game");
    directory(repo + "/playable"); directory(repo + "/brain");
    directory(repo + "/brain/lib"); directory(repo + "/brain/vanilla");
    directory(repo + "/brain/vanilla/build-native");
    directory(repo + "/brain/vanilla/build-native/tiberiandawn");
    directory(root + "/unrelated");
    file(repo + "/app/opencnc3d.cpp"); file(repo + "/game/cnc_eyes.cpp");
    const std::string built = repo + "/brain/vanilla/build-native/tiberiandawn/" + lib;
    file(built);
    file(repo + "/app/" + lib); file(repo + "/game/" + lib);
    file(repo + "/playable/" + lib);
    file(root + "/unrelated/" + lib);

    // A non-existent explicit engine is still authoritative, including RA's
    // module path. The resolver must not silently switch games or call SDL.
    expect("private/override-engine", "private/override-engine");
    expect("ra3d/RedAlert.dylib", "ra3d/RedAlert.dylib");
    expect("ra3d/RedAlert.dll", "ra3d/RedAlert.dll");
    assert(sdl_calls == 0);

    // Installed playable nested in a repository: the installed engine wins over
    // both its older sibling build and a different engine in the working folder.
    sdl_base = repo + "/playable/";
    assert(chdir((repo + "/app").c_str()) == 0);
    expect(sdl_base + lib);
    assert(chdir((root + "/unrelated").c_str()) == 0);
    expect(sdl_base + lib, "");

    // Actual source executables preserve the repository-first developer policy
    // and resolve their own checkout independently of the current directory.
    for (int i = 0; i < 2; ++i) {
        sdl_base = repo + (i ? "/game/" : "/app/");
        expect(sdl_base + "../brain/vanilla/build-native/tiberiandawn/" + lib);
    }
    const std::string shared = repo + "/brain/lib/" + lib;
    file(shared);
    expect(sdl_base + "../brain/lib/" + lib);
    assert(unlink(shared.c_str()) == 0);
    assert(unlink(built.c_str()) == 0);
    expect(sdl_base + lib); // source build may only have its staged engine

    // SDL failure and missing installed engine retain the legacy working-folder
    // fallback. Oversized paths must never probe a truncated filename.
    sdl_base.clear(); expect(lib);
    sdl_base = root + "/missing-install/"; expect(lib);
    sdl_base.assign(2048, 'x'); sdl_base += '/'; expect(lib);
    assert(unlink((root + "/unrelated/" + lib).c_str()) == 0);
    sdl_base.clear(); expect(lib); // stable diagnostic name when nothing exists
    assert(sdl_calls > 0 && sdl_allocations == 0);
    printf("BRAIN-PATH|%s|installed-adjacent|developer-repository|foreign-cwd|"
           "explicit-TD-RA|missing-base|long-path|PASS\n", OPENCNC3D_BRAIN_LIB);
}
