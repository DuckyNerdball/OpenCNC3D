#!/usr/bin/env python3
"""The fix-and-improve viewer's server: the page, a live status list, and the review
buttons. Local only.

    python3 tools/art/n64fix/viewer/serve.py [port]        (default 8795)

GET  /api/index    every building, with its status worked out now: queued (and where in
                   the queue), modelling (with the percentage and step its WORKING file
                   states), ready for review, approved or rejected.
POST /api/review   {"type": T, "action": "comment" | "approve" | "reject", "text": ...}

Review state is kept in tools/art/n64fix/reviews.json, which is committed: an approved
building is locked (run.sh refuses to overwrite it) and marked to replace the cartridge
model in Enhanced mode only; a rejected one is marked never to replace it. A comment is
also appended to tools/art/n64fix/viewer/inbox.jsonl, one JSON object per line, which
whoever is working the queue watches and acts on.
"""
import json
import os
import sys
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
N64FIX = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(N64FIX, "..", "..", ".."))
MODELS = os.path.join(HERE, "models")
REVIEWS = os.path.join(N64FIX, "reviews.json")
QUEUE = os.path.join(N64FIX, "queue.txt")
INBOX = os.path.join(HERE, "inbox.jsonl")
LOCK = threading.Lock()


def read_json(path, default):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return default


def write_json(path, data):
    tmp = "%s.%d.tmp" % (path, os.getpid())
    with open(tmp, "w") as f:
        json.dump(data, f, indent=1, sort_keys=True)
        f.write("\n")
    os.replace(tmp, path)


def queue():
    try:
        return [l.split()[0] for l in open(QUEUE) if l.strip() and not l.startswith("#")]
    except OSError:
        return []


def index():
    rows = read_json(os.path.join(MODELS, "index.json"), [])
    reviews = read_json(REVIEWS, {})
    busy = {r["type"] for r in rows if os.path.exists(os.path.join(MODELS, r["type"], "WORKING"))}
    q = [t for t in queue() if t not in busy]
    for r in rows:
        t = r["type"]
        d = os.path.join(MODELS, t)
        r["old"] = os.path.exists(os.path.join(d, "old.json"))
        r["new"] = os.path.exists(os.path.join(d, "new.json"))
        for side in ("old", "new"):
            p = os.path.join(d, side + ".json")
            if os.path.exists(p):
                r[side + "_stamp"] = int(os.path.getmtime(p))
        work = os.path.join(d, "WORKING")
        r["working"] = os.path.exists(work)
        if r["working"]:
            w = {}
            try:
                txt = open(work).read().strip()
                w = json.loads(txt) if txt else {}
            except (OSError, ValueError):
                pass
            r["pct"] = int(w.get("pct", 5))
            r["step"] = w.get("step", "Modelling")
        rv = reviews.get(t, {})
        r["review"] = rv.get("state")
        r["comments"] = rv.get("comments", [])
        waiting = [x for x in q if x == t]
        r["queue_pos"] = (q.index(t) + 1) if waiting and not r["working"] else None
        if r["review"] in ("approved", "rejected"):
            r["status"] = r["review"]
        elif r["working"]:
            r["status"] = "modelling"
        elif r["new"]:
            r["status"] = "review"
        elif r["queue_pos"]:
            r["status"] = "queued"
        else:
            r["status"] = "not scheduled"
    return rows


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=HERE, **k)

    def log_message(self, fmt, *args):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.split("?")[0] == "/api/index":
            return self._json(200, index())
        return super().do_GET()

    def do_POST(self):
        if self.path != "/api/review":
            return self._json(404, {"error": "no such endpoint"})
        try:
            n = int(self.headers.get("Content-Length", "0"))
            req = json.loads(self.rfile.read(n) or b"{}")
        except ValueError:
            return self._json(400, {"error": "bad json"})
        t, action = req.get("type", ""), req.get("action", "")
        text = (req.get("text") or "").strip()
        known = {r["type"] for r in read_json(os.path.join(MODELS, "index.json"), [])}
        if t not in known or action not in ("comment", "approve", "reject", "reopen"):
            return self._json(400, {"error": "unknown type or action"})
        if action == "comment" and not text:
            return self._json(400, {"error": "empty comment"})
        now = time.strftime("%Y-%m-%d %H:%M")
        with LOCK:
            reviews = read_json(REVIEWS, {})
            rv = reviews.setdefault(t, {"state": "review", "comments": []})
            if rv.get("state") == "approved" and action == "comment":
                return self._json(409, {"error": "approved and locked: reopen it first"})
            if action == "comment":
                c = {"at": now, "text": text, "status": "open"}
                rv.setdefault("comments", []).append(c)
                rv["state"] = "review"
                with open(INBOX, "a") as f:
                    f.write(json.dumps({"type": t, "at": now, "text": text}) + "\n")
            elif action == "approve":
                rv["state"] = "approved"
                rv["enhanced"] = "replace"
                rv["at"] = now
            elif action == "reject":
                rv["state"] = "rejected"
                rv["enhanced"] = "keep cartridge"
                rv["at"] = now
            elif action == "reopen":
                rv["state"] = "review"
                rv.pop("enhanced", None)
            write_json(REVIEWS, reviews)
        return self._json(200, {"ok": True, "state": rv["state"]})


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8795
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
