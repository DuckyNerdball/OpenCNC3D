#!/usr/bin/env python3
"""Score the generator with module settings overridden:
    python3 ablate.py NAME KEY=VALUE [KEY=VALUE ...]   -> out/abl_NAME.json"""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate as G, score as S
name = sys.argv[1]
for kv in sys.argv[2:]:
    k, v = kv.split("=")
    setattr(G, k, eval(v))
res = S.run(["generate"])
json.dump(res, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", f"abl_{name}.json"), "w"))
print(name, " ".join(sys.argv[2:]))
S.summary(res)
