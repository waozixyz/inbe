#!/usr/bin/env python3
"""Generate the Inbe appcast for a release.

The release workflow publishes appcast.json next to its artifacts:

    scripts/make_appcast.py \
        --version "$VERSION" \
        --base-url "https://github.com/OWNER/APP/releases/download/v$VERSION" \
        --notes-url "https://github.com/OWNER/APP/releases/tag/v$VERSION" \
        --changelog CHANGELOG.md \
        --out build/release/appcast.json \
        --channel appimage=build/release/linux/app-linux-x86_64.AppImage \
        --channel windows=build/release/windows/app-windows.zip

Each --channel becomes {url, sha256, size} in the document; the URL is
base-url/ plus the file's basename. Notes are the version's CHANGELOG.md
section, collapsed to one line and capped at 400 chars. Channels with a
missing file are skipped, an error when the flag is --channel-required.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import sys


def changelog_notes(path, version, limit=400):
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return ""
    match = re.search(
        r"^## \[" + re.escape(version) + r"\].*?\n(.*?)(?=^## \[|\Z)",
        text, re.S | re.M)
    if match is None:
        return ""
    return " ".join(match.group(1).split())[:limit]


def channel_entry(base_url, rel_path):
    with open(rel_path, "rb") as f:
        data = f.read()
    return {
        "url": base_url.rstrip("/") + "/" + os.path.basename(rel_path),
        "sha256": hashlib.sha256(data).hexdigest(),
        "size": len(data),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True,
                        help="release download URL prefix")
    parser.add_argument("--notes-url", default="",
                        help="release page URL (defaults to base-url's tag)")
    parser.add_argument("--changelog", default="CHANGELOG.md")
    parser.add_argument("--out", required=True)
    parser.add_argument("--channel", action="append", default=[],
                        metavar="NAME=PATH", dest="channels",
                        help="release artifact for a channel (repeatable)")
    parser.add_argument("--channel-required", action="store_true",
                        help="fail when a named channel has no artifact")
    args = parser.parse_args()

    channels = {}
    for item in args.channels:
        name, _, path = item.partition("=")
        if not name or not path:
            parser.error("--channel wants NAME=PATH, got %r" % item)
        if not os.path.isfile(path):
            if args.channel_required:
                sys.exit("missing artifact for channel %s: %s" % (name, path))
            print("skipping channel %s (no artifact)" % name)
            continue
        channels[name] = channel_entry(args.base_url, path)
    if not channels:
        sys.exit("no channel artifacts found")

    notes_url = args.notes_url
    if not notes_url:
        # derive ".../releases/download/vX" -> ".../releases/tag/vX"
        head, sep, tail = args.base_url.rstrip("/").partition("/download/")
        notes_url = head + "/tag/" + tail if sep else head

    appcast = {
        "version": args.version,
        "date": datetime.date.today().isoformat(),
        "notes": changelog_notes(args.changelog, args.version),
        "notes_url": notes_url,
        "channels": channels,
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(appcast, f, indent=2)
        f.write("\n")
    print("wrote %s" % args.out)
    print(json.dumps(appcast, indent=2))


if __name__ == "__main__":
    main()
