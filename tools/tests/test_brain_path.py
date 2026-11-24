#!/usr/bin/env python3
"""Exercise the shared engine resolver without loading SDL or either game.

The Windows case checks target library naming using the same filesystem fixtures;
it is not a substitute for a native Windows launch.
"""
import os
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="opencnc3d-brain-path-") as scratch:
    work = Path(scratch)
    (work / "SDL.h").write_text(
        '#pragma once\nextern "C" char* SDL_GetBasePath(void);\n'
        'extern "C" void SDL_free(void*);\n'
    )
    for platform in ("macos", "windows-name"):
        binary = work / platform
        command = [os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra",
                   "-Werror", "-I", str(work), str(repo / "tools/tests/test_brain_path.cpp"),
                   "-o", str(binary)]
        if platform == "windows-name":
            command.append("-D_WIN32")
        subprocess.run(command, check=True)
        fixture = work / (platform + "-fixture")
        fixture.mkdir()
        subprocess.run([str(binary), str(fixture)], check=True)
