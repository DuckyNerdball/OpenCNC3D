/*
 * lnet.h -- the one HTTP client the launcher needs, on each platform's own.
 *
 * macOS links libcurl, which ships in the SDK. Windows uses WinINet, which is a
 * system DLL and needs nothing fetched, nothing bundled and no second SSL stack
 * beside the one the OS already trusts. Both are three calls; neither is a
 * dependency the player has to install.
 *
 * THE LAUNCHER USES ONE SHAPE OF REQUEST and that is deliberate: a GET, with the
 * launcher's key in a header, following redirects, failing loudly on any status
 * that is not 200. It never posts, never uploads and never sends anything about
 * the machine it is on.
 *
 * THE GAME ALSO LINKS THIS FILE, for the internet game list, and that one does
 * post: a host has to say its game is open. ln_post_text is the whole of that
 * addition. It is the same two backends and the same error sentences, so there
 * is one HTTP client here rather than a second one grown beside it.
 */

#ifndef LNET_H
#define LNET_H

/* Return 0 to abort the transfer. `total` is -1 when the server did not say. */
typedef int (*LN_Progress)(void *user, long long done, long long total);

/* Small responses, into memory. Returns a NUL-terminated buffer the caller frees,
 * or NULL with `err` filled in. Capped, because a manifest is a few hundred bytes
 * and anything claiming to be megabytes is not one. */
char *ln_get_text(const char *url, const char *key, long cap, char *err, int errlen);

/* ln_get_text, also saying whether a failure was the host never being reached at all
 * (*unreached = 1): no network, no such name, refused, no secure connection, or no
 * answer to the connect. That is the one failure an installer reports as "offline"
 * rather than as a problem with the site. */
char *ln_get_text_reach(const char *url, const char *key, long cap, int *unreached, char *err,
                        int errlen);

/* POST a small body and read the small answer back, otherwise exactly ln_get_text:
 * a NUL-terminated buffer the caller frees, or NULL with `err` filled in.
 * `content_type` may be NULL, which means application/json.
 *
 * THE ROUTE MUST ANSWER 200 AND MUST NOT REDIRECT. Both backends turn a redirected
 * POST into a GET and drop the body on the way, so a 301 here does not fail, it
 * silently sends nothing. That is a property of HTTP redirect handling rather than
 * of either library, and it is the service's job not to redirect this route.
 *
 * A REPLY THAT FILLS THE CAP EXACTLY WAS TRUNCATED. The two backends disagree about
 * an over-long reply -- one fails the transfer, the other stops reading and hands
 * back what it has -- so the only answer that means the same thing on both is the
 * length: strlen(result) == cap is truncation, with no ambiguity, because the
 * buffer is one byte longer than the cap. The same is true of ln_get_text. */
char *ln_post_text(const char *url, const char *key, const char *body,
                   const char *content_type, long cap, char *err, int errlen);

/* Large responses, straight to a file. Returns 1 on success. The file is written
 * whole or not at all: it lands on a temporary name and is renamed on success, so
 * an interrupted download cannot be mistaken for a finished one. A transfer that
 * stalls (under 1 KB a second for 30 seconds) fails rather than waiting forever, and
 * `cb` is asked while it waits as well as when bytes arrive, so a 0 from it stops a
 * stalled transfer too. */
int ln_get_file(const char *url, const char *key, const char *path, LN_Progress cb,
                void *user, char *err, int errlen);

/* What ln_get_range found, so a caller can tell a host that is down from one that is
 * answering wrongly, from a disk that is full, from a player who pressed Cancel. Each
 * one has a different right answer: try the next host, try this one again, stop. */
typedef enum
{
    LN_RANGE_OK = 0,
    LN_RANGE_NOCONNECT, /* never reached: no network, no such host, refused, no secure
                           connection, or no answer to the connect */
    LN_RANGE_HTTP,      /* it answered with an error status (*status says which)       */
    LN_RANGE_NORANGE,   /* it answered, but not with the bytes asked for: a 200 for the
                           whole file, a 206 starting somewhere else, more bytes than
                           were asked for, or redirects without end                   */
    LN_RANGE_BROKEN,    /* it was answering and stopped: cut off, reset, or stalled    */
    LN_RANGE_DISK,      /* the part file could not be written                          */
    LN_RANGE_CANCEL     /* *cancel was set, or `cb` returned 0                        */
} LN_RangeResult;

/* A PIECE OF A FILE, by HTTP Range, appended to `part`, resuming. Bytes
 * [offset, offset + length) of whatever `url` names end up in `part`: the part file's
 * own length says how many are there already, and only the rest is asked for, as
 * "Range: bytes=<offset + have>-<offset + length - 1>". A part file longer than
 * `length` cannot be a piece of this range and is started again.
 *
 * THE RANGE IS ENFORCED, NOT HOPED FOR. Only a 206 whose Content-Range starts at the
 * first byte asked for is written; a 200 is refused at its first byte, because a host
 * that ignores Range would otherwise stream the whole disc image (600 MB) for one
 * file. Redirects are followed, up to 5, and Range is sent again on every hop; on
 * Windows they are followed by hand for exactly that reason.
 *
 * `key` is the site's header and NULL for anyone else's host. `stall_secs` bounds a
 * transfer that stops moving (under 1 KB a second for that long). `cancel`, when not
 * NULL, is polled while waiting as well as between reads. `cb` gets the bytes in the
 * part file so far and `length`. `status`, when not NULL, gets the last HTTP status.
 *
 * Returns LN_RANGE_OK when the part file holds all `length` bytes. Nothing is
 * removed on any failure: what arrived stays in `part` for the next attempt. The
 * caller verifies the bytes and renames the file. Sentences in `err` name the host
 * and never mention updates. */
int ln_get_range(const char *url, const char *key, const char *part, long long offset,
                 long long length, int stall_secs, volatile int *cancel, LN_Progress cb,
                 void *user, long *status, char *err, int errlen);

/* The size of a file in bytes, 64-bit on every platform (the 32-bit Windows C
 * runtime's stat stops at 2 GiB), or -1 when there is no such file. */
long long ln_file_size(const char *path);

#endif /* LNET_H */
