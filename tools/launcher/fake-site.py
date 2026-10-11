#!/usr/bin/env python3
"""
fake-site.py -- opencnc3d.com's three build routes, on this machine, in one file.

    tools/launcher/fake-site.py <folder-of-zips> [--tag v0.6.3] [--port 0]
                                [--prior v0.7.3]
                                [--cutoff 0.8.0 --cutoff-agent <prefix>]

The launcher reads the live site and nothing else, so a test that does not speak
the site's API is not testing the launcher. This serves the same three routes,
with the same JSON shapes, recorded from the real thing:

    GET /api/builds                {"ok":true,"latest":{"tag":...,"assets":[
                                    {"id":..,"name":..,"size":..,
                                     "platform":"macos|windows",
                                     "kind":"full|binaries"}]},"releases":[...]}
    GET /api/changelog             {"ok":true,"total":N,"entries":[
                                    {"title":..,"version":..,"codename":..,
                                     "date":..,"body":"markdown"}]}
    GET /api/download?asset=<id>   302 -> /files/<name>

THE REDIRECT IS THE POINT OF THAT LAST ROUTE. The real site answers with a 302 to
a short-lived signed githubusercontent URL, so the launcher's HTTP layer has to
follow a CROSS-HOST redirect on both platforms: libcurl needs FOLLOWLOCATION, and
WinINet needs not to have been told otherwise. A test host that served the bytes
directly would pass without ever exercising the one thing most likely to be wrong.

THE CUTOFF IS THE LIVE SITE'S RULE TOO. A request whose User-Agent starts with
the --cutoff-agent prefix is shown only the releases below --cutoff, so a launcher
from before a build that stopped supporting it is never offered that build. It is a
PREFIX match, as on the site: a later launcher's agent may contain the same words
further in, and must still be shown everything. --prior lists one older release
beside the newest, so a launcher that is cut off still sees the release it can
take rather than an empty list. Every request's User-Agent is logged, so a test
can see which agent a launcher actually sent.

Asset ids are derived from the filename so a run is reproducible, and the port is
taken from the kernel because several sessions work this machine at once and a
hardcoded port means measuring somebody else's server.

/files/<name> HONOURS RANGE, as the signed GitHub address the live site redirects to
does, because the first install of the game resumes its package by Range.

THE MOVIES AND MUSIC MIRRORS, with --media <folder>: the disc images the launcher
fetches movies and music out of, by byte range, served the way the real mirrors serve
them, and the ways the real mirrors go wrong, chosen by the URL so one site can be
every mirror a test manifest lists:

    GET /media/<name>                    the file, honouring Range (206 and
                                         Content-Range), like archive.org's storage
    GET /media-302/<name>                302 -> /media/<name>, like archive.org's
                                         /download/ address handing off to a node
    GET /media-fault/<knob>/<name>       /media/<name>, gone wrong:
        ignore-range    a 200 and the whole file, whatever was asked for
        cut=<bytes>     the first answer for this address stops after that many bytes
                        of its body, having promised the whole range
        html            a 206 of the right length whose bytes are a web page
        stall=<s>       the headers, then nothing for that many seconds, every time
        slow=<bytes/s>  the body at that rate
        500x<n>         the first n requests for this address answer 500
        404             always 404

Every request's Range is logged beside its User-Agent, and every media answer logs
how many bytes of body it actually sent, so a test can see that a refused answer was
not streamed whole and that a retry resumed where the last one stopped.
"""

import argparse
import json
import os
import re
import sys
import threading
import time
import zlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

CHANGELOG = [
    {
        "title": 'OpenCNC3D v0.6.3 "The Front Door"',
        "version": "0.6.3",
        "codename": "The Front Door",
        "date": "2026-08-24",
        "body": "### New features\n\n- A launcher on both platforms.\n"
                "- Play becomes Update when a newer build is up.\n",
    },
    {
        "title": 'OpenCNC3D v0.6.2 "Now You See It"',
        "version": "0.6.2",
        "codename": "Now You See It",
        "date": "2026-08-24",
        "body": "### Bugs fixed\n\n- The Hand of Nod dropped its globe.\n",
    },
]


def version(tag):
    """(major, minor, patch) out of "v0.6.3" or "0.6.3"; anything after the
    numbers is ignored, as the launcher ignores it."""
    nums = []
    for part in tag.lstrip("vV").split(".")[:3]:
        digits = ""
        for ch in part:
            if not ch.isdigit():
                break
            digits += ch
        nums.append(int(digits) if digits else 0)
    while len(nums) < 3:
        nums.append(0)
    return tuple(nums)


def classify(name):
    """Exactly what the site does: platform and kind out of the filename."""
    platform = "macos" if "macos" in name else "windows" if "windows" in name else None
    kind = "binaries" if name.endswith("-bins.zip") else "full" if name.endswith(".zip") else None
    return platform, kind


def parse_range(header, size):
    """(first, last) for "bytes=a-b" or "bytes=a-", None with no usable header (the
    whole file is served), "bad" for a range starting past the end."""
    if not header:
        return None
    m = re.match(r"\s*bytes=(\d+)-(\d*)\s*$", header)
    if not m:
        return None
    first = int(m.group(1))
    last = int(m.group(2)) if m.group(2) else size - 1
    if first >= size or last < first:
        return "bad"
    return first, min(last, size - 1)


class Handler(SimpleHTTPRequestHandler):
    root = "."
    media = None  # see --media
    counts = {}   # address -> requests so far, for cut= and 500x
    counts_lock = threading.Lock()
    tag = "v0.6.3"
    assets = {}   # id -> filename
    short = None  # a filename to serve TRUNCATED, see --short
    pad = 0       # bytes of extra changelog, see --changelog-pad
    prior = None  # an older release listed beside the newest, see --prior
    cutoff = None         # see --cutoff
    cutoff_agent = None   # see --cutoff-agent

    def send_json(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("content-type", "application/json")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_file(self, name):
        path = os.path.join(self.root, name)
        if not os.path.isfile(path):
            self.send_error(404, "no such asset")
            return
        size = os.path.getsize(path)
        rng = parse_range(self.headers.get("Range"), size)
        if rng == "bad":
            self.send_response(416)
            self.send_header("content-range", "bytes */%d" % size)
            self.send_header("content-length", "0")
            self.end_headers()
            return
        first, last = rng if rng else (0, size - 1)
        length = last - first + 1
        # --short models a transfer that DIES MID-FLIGHT: the true length is
        # advertised, in the asset list and in the header, and fewer bytes
        # arrive. Truncating the file on disk instead would shrink what
        # /api/builds reports too, so the launcher would compare a short download
        # against a short expectation, agree, and never exercise the check the
        # test is there to exercise. That is exactly how this test first passed
        # for the wrong reason. A ranged request gets half of ITS range, so a
        # download that resumes gets further each time and still never finishes.
        limit = (length // 2) if (self.short and name == self.short) else length
        self.send_response(206 if rng else 200)
        self.send_header("content-type", "application/octet-stream")
        self.send_header("content-length", str(length))
        if rng:
            self.send_header("content-range", "bytes %d-%d/%d" % (first, last, size))
        self.send_header("accept-ranges", "bytes")
        self.send_header("content-disposition", 'attachment; filename=%s' % name)
        self.end_headers()
        sent = 0
        try:
            with open(path, "rb") as f:
                f.seek(first)
                while sent < limit:
                    chunk = f.read(min(64 * 1024, limit - sent))
                    if not chunk:
                        break
                    self.wfile.write(chunk)
                    sent += len(chunk)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def count(self, key):
        with self.counts_lock:
            self.counts[key] = self.counts.get(key, 0) + 1
            return self.counts[key]

    def send_media(self, name, knob):
        """One mirror's answer for /media/<name>, gone wrong in the way `knob` says.
        See the module docstring."""
        path = os.path.join(self.media or "", os.path.basename(name))
        if not self.media or not os.path.isfile(path):
            self.send_error(404, "no such disc")
            return
        n = self.count(self.path)
        size = os.path.getsize(path)
        if knob == "404":
            self.send_error(404, "not on this mirror")
            return
        if knob.startswith("500x") and n <= int(knob[4:] or "0"):
            self.send_error(500, "the mirror is having a bad day")
            return
        rng = parse_range(self.headers.get("Range"), size)
        if rng == "bad":
            self.send_response(416)
            self.send_header("content-range", "bytes */%d" % size)
            self.send_header("content-length", "0")
            self.end_headers()
            return
        whole = rng is None or knob == "ignore-range"
        first, last = (0, size - 1) if whole else rng
        length = last - first + 1
        limit = length
        if knob.startswith("cut=") and n == 1:
            limit = min(length, int(knob[4:]))
        self.send_response(200 if whole else 206)
        self.send_header("content-type", "application/octet-stream")
        self.send_header("content-length", str(length))
        if not whole:
            self.send_header("content-range", "bytes %d-%d/%d" % (first, last, size))
        self.send_header("accept-ranges", "bytes")
        self.end_headers()
        if knob.startswith("stall="):
            time.sleep(float(knob[6:]))
        rate = int(knob[5:]) if knob.startswith("slow=") else 0
        page = (b"<!DOCTYPE html><html><head><title>Download</title></head><body>"
                b"Your download will begin shortly.</body></html>\n")
        sent = 0
        try:
            with open(path, "rb") as f:
                f.seek(first)
                while sent < limit:
                    step = min(16 * 1024 if rate else 64 * 1024, limit - sent)
                    chunk = f.read(step)
                    if not chunk:
                        break
                    if knob == "html":
                        chunk = (page * (len(chunk) // len(page) + 1))[:len(chunk)]
                    self.wfile.write(chunk)
                    self.wfile.flush()
                    sent += len(chunk)
                    if rate:
                        time.sleep(len(chunk) / float(rate))
        except (BrokenPipeError, ConnectionResetError):
            pass
        sys.stderr.write("  media: %s range=%s status=%d sent=%d of %d\n"
                         % (self.path, self.headers.get("Range", "-"), 200 if whole else 206,
                            sent, length))

    def do_GET(self):
        u = urlparse(self.path)

        if u.path == "/api/builds":
            entries = []
            for name in sorted(self.assets.values()):
                platform, kind = classify(name)
                item = {
                    "id": [k for k, v in self.assets.items() if v == name][0],
                    "name": name,
                    "size": os.path.getsize(os.path.join(self.root, name)),
                }
                # The site only labels what it recognises. A manifest .txt gets
                # no platform and no kind, which is exactly why the launcher
                # matches it by name.
                if platform:
                    item["platform"] = platform
                if kind:
                    item["kind"] = kind
                entries.append(item)
            releases = [{"tag": self.tag, "name": "OpenCNC3D " + self.tag,
                         "published": "2026-08-24T09:05:43Z", "prerelease": False,
                         "assets": entries}]
            if self.prior:
                releases.append(dict(releases[0], tag=self.prior,
                                     name="OpenCNC3D " + self.prior))
            releases.sort(key=lambda r: version(r["tag"]), reverse=True)
            agent = self.headers.get("User-Agent", "")
            if self.cutoff and self.cutoff_agent and agent.startswith(self.cutoff_agent):
                releases = [r for r in releases if version(r["tag"]) < version(self.cutoff)]
            self.send_json({"ok": True, "latest": releases[0] if releases else None,
                            "releases": releases})
            return

        if u.path == "/api/changelog":
            entries = list(CHANGELOG)
            if self.pad:
                # --changelog-pad: one more entry, that many bytes long, so the
                # CHANGELOG.txt an update writes is big enough to be killed inside.
                entries.append({"title": "OpenCNC3D v0.6.0 \"Padding\"", "version": "0.6.0",
                                "codename": "Padding", "date": "2026-08-01",
                                "body": "- padding\n" * (self.pad // 10)})
            self.send_json({"ok": True, "total": len(entries), "entries": entries})
            return

        if u.path == "/api/download":
            q = parse_qs(u.query)
            try:
                aid = int(q.get("asset", ["0"])[0])
            except ValueError:
                self.send_error(400, "bad asset")
                return
            name = self.assets.get(aid)
            if not name:
                self.send_error(404, "no such asset")
                return
            # A 302, like the real one. See the module docstring.
            self.send_response(302)
            self.send_header("location", "/files/" + name)
            self.send_header("content-length", "0")
            self.end_headers()
            return

        if u.path.startswith("/files/"):
            name = os.path.basename(u.path[len("/files/"):])
            self.send_file(name)
            return

        if u.path.startswith("/media/"):
            self.send_media(u.path[len("/media/"):], "")
            return

        if u.path.startswith("/media-302/"):
            self.send_response(302)
            self.send_header("location", "/media/" + u.path[len("/media-302/"):])
            self.send_header("content-length", "0")
            self.end_headers()
            return

        if u.path.startswith("/media-fault/"):
            rest = u.path[len("/media-fault/"):]
            knob, _, name = rest.partition("/")
            self.send_media(name, knob)
            return

        self.send_error(404, "not a route this site has")

    def log_message(self, fmt, *args):
        headers = getattr(self, "headers", None)
        agent = headers.get("User-Agent", "-") if headers else "-"
        rng = headers.get("Range", "-") if headers else "-"
        sys.stderr.write("  site: " + (fmt % args) + " ua=" + agent + " range=" + rng + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--tag", default="v0.6.3")
    ap.add_argument("--changelog-pad", type=int, default=0,
                    help="add a changelog entry of about this many bytes, so the "
                         "CHANGELOG.txt an update writes is large")
    ap.add_argument("--short", default=None,
                    help="serve this filename truncated to half, while still "
                         "advertising its true length: a transfer that died")
    ap.add_argument("--prior", default=None,
                    help="an older release tag, listed beside --tag with the same "
                         "assets")
    ap.add_argument("--cutoff", default=None,
                    help="with --cutoff-agent: the version a matching launcher is "
                         "shown nothing at or above")
    ap.add_argument("--cutoff-agent", default=None,
                    help="a User-Agent prefix; requests that start with it see only "
                         "releases below --cutoff")
    ap.add_argument("--media", default=None,
                    help="a folder of disc images to serve as the movies and music "
                         "mirrors, under /media/, /media-302/ and /media-fault/")
    ap.add_argument("--port", type=int, default=0,
                    help="0 (the default) takes a free port, which is the only "
                         "safe choice on a machine running several sessions")
    a = ap.parse_args()

    Handler.root = os.path.abspath(a.folder)
    Handler.tag = a.tag
    Handler.short = a.short
    Handler.pad = a.changelog_pad
    Handler.prior = a.prior
    Handler.cutoff = a.cutoff
    Handler.cutoff_agent = a.cutoff_agent
    Handler.media = os.path.abspath(a.media) if a.media else None
    # Ids from the name, so a run is reproducible and a log is readable.
    Handler.assets = {
        (zlib.crc32(n.encode()) & 0x7FFFFFFF): n
        for n in sorted(os.listdir(Handler.root))
        if os.path.isfile(os.path.join(Handler.root, n))
    }

    srv = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    port = srv.server_address[1]
    print("PORT %d" % port, flush=True)
    print("serving %s as %s at http://127.0.0.1:%d/"
          % (Handler.root, a.tag, port), flush=True)
    if a.short:
        print("  NOTE: %s will be served truncated to half its length" % a.short,
              flush=True)
    if a.prior:
        print("  also listing %s, older" % a.prior, flush=True)
    if a.media:
        print("  movies and music mirrors from %s" % Handler.media, flush=True)
    if a.cutoff and a.cutoff_agent:
        print("  agents starting %r see only releases below %s"
              % (a.cutoff_agent, a.cutoff), flush=True)
    for aid, name in sorted(Handler.assets.items()):
        print("  asset %d  %s" % (aid, name), flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
