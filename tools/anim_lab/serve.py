#!/usr/bin/env python3
"""Serve the anim lab on this machine only (http://localhost:8791): the page and its
models as plain files, plus one small API for the soldier colours.

    GET  /api/colours   {"saved": <colours.json or null>, "game": <the game's palette>,
                         "clothRef": M3_CLOTH_REF, "logo": M3_LOGO_*}
    POST /api/colours   the page's values; checked, then written to colours.json
    POST /api/shot?n=N  a rendered frame (data URL), saved as out/shot_N.jpg

Nothing reaches the game from here: tools/anim_lab/push_colours.py does that, on request.
"""
import http.server
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import push_colours as P  # noqa: E402

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8791


class Lab(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=HERE, **k)

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.split("?")[0] == "/api/colours":
            saved = json.load(open(P.SAVED)) if os.path.exists(P.SAVED) else None
            return self._json(200, {"saved": saved, "game": P.read_game(),
                                    "clothRef": P.cloth_ref(), "logo": P.logo_colour()})
        return super().do_GET()

    def do_POST(self):
        if self.path.split("?")[0] == "/api/shot":
            # A render the page sends back (a data URL), saved as out/shot_<n>.jpg, so a
            # frame can be looked at without a visible window.
            import base64, re
            n = int(self.headers.get("Content-Length", "0"))
            if n > 8 << 20:
                return self._json(400, {"error": "too large"})
            body = self.rfile.read(n).decode()
            m = re.match(r"data:image/(png|jpeg);base64,(.*)$", body, re.S)
            q = re.search(r"[?&]n=(\d+)", self.path)
            if not m or not q:
                return self._json(400, {"error": "want ?n=<number> and a data URL"})
            path = os.path.join(HERE, "out", "shot_%s.%s" % (q.group(1), "jpg" if m.group(1) == "jpeg" else "png"))
            open(path, "wb").write(base64.b64decode(m.group(2)))
            return self._json(200, {"saved": os.path.basename(path)})
        if self.path.split("?")[0] != "/api/colours":
            return self._json(404, {"error": "no such thing"})
        try:
            n = int(self.headers.get("Content-Length", "0"))
            if n > 4096:
                raise ValueError("too large")
            p = P.validate(json.loads(self.rfile.read(n)))
        except (ValueError, KeyError, TypeError) as e:
            return self._json(400, {"error": str(e)})
        with open(P.SAVED, "w") as f:
            json.dump(p, f, indent=1)
            f.write("\n")
        return self._json(200, {"saved": p, "game": P.read_game()})

    def log_message(self, fmt, *args):
        if "/api/" in (args[0] if args else ""):
            sys.stderr.write("LAB|%s\n" % (fmt % args))


if __name__ == "__main__":
    http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Lab).serve_forever()
