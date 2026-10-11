"""Unity YAML reader: a .prefab / .mat / .asset file -> {fileID: (classID, typeName, body)}.

Unity serialises scenes, prefabs and materials as YAML 1.1 with a private tag per
document (`--- !u!198 &12345`), which stock loaders reject. The tag is stripped and kept
beside the parsed body. Also resolves a GUID to an asset path through the unpacked
.unitypackage (GUID folders, each with `pathname`, `asset`, `asset.meta`).
"""
import os, re, yaml

_DOC = re.compile(r'^--- !u!(\d+) &(-?\d+)( stripped)?\s*$', re.M)

class _Loader(yaml.SafeLoader):
    pass
# Unity writes bare scalars such as `m_Script: {fileID: 11500000, guid: ..., type: 3}`;
# the SafeLoader handles them. Nothing else to register.

def load(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    out = {}
    ms = list(_DOC.finditer(text))
    for i, m in enumerate(ms):
        end = ms[i + 1].start() if i + 1 < len(ms) else len(text)
        body = yaml.load(text[m.end():end], Loader=_Loader) or {}
        (tname, val), = body.items() if len(body) == 1 else (('?', body),)
        out[int(m.group(2))] = (int(m.group(1)), tname, val)
    return out

class Package:
    """An unpacked .unitypackage: GUID -> path and back."""
    def __init__(self, root):
        self.root = root
        self.path = {}
        for g in os.listdir(root):
            p = os.path.join(root, g, 'pathname')
            if os.path.isfile(p):
                self.path[g] = open(p).readline().strip()
        self.guid = {v: k for k, v in self.path.items()}
    def asset(self, guid):
        return os.path.join(self.root, guid, 'asset')
    def meta(self, guid):
        return os.path.join(self.root, guid, 'asset.meta')
    def find(self, suffix):
        hits = [g for g, p in self.path.items() if p.endswith(suffix)]
        assert len(hits) == 1, (suffix, hits)
        return hits[0]
