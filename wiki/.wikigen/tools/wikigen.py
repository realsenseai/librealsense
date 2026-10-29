#!/usr/bin/env python3
"""Helper for the librealsense Wiki Writer workflow.

Commands (WIKI is the wiki/ folder of a gh-pages checkout, SRC a development checkout):
  validate --wiki WIKI                         check .wikigen/manifest.json and state files
  plan --src SRC --wiki WIKI --base REF        JSON: pending subjects, subjects to update,
                                               waiting_new, uncovered repo items
  state --wiki WIKI --file F --commit SHA [--tag T]   record that subject F is accurate at SHA
  nav --wiki WIKI                              regenerate _Sidebar.md and Home.md's subject list
  block get|set|ensure --file F --name N [--top]      read, replace (stdin) or add a marked block
  freeze --file F --heading H                  turn the upcoming-release draft into a released section
  links --src SRC --wiki WIKI [--external] [--block N] FILE...   JSON report of broken links
                                               (--block: only lines inside that marked block)
  cmake-options --src SRC                      Markdown table of CMake/lrs_options.cmake

Exit codes: 0 ok, 1 validation problems, 2 usage or input error.
Keep this file free of brace-wrapped words: CrewAI reads them as template variables.
"""
import argparse
import concurrent.futures
import json
import os
import re
import socket
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

KINDS = ("nav", "guide", "reference", "release")
REQUIRED_KEYS = ("version", "write_first", "sections", "out_of_scope", "exclusions", "sidebar_links", "subjects")
USER_AGENT = "Mozilla/5.0 (compatible; librealsense-wiki-writer)"
REPO_URL = re.compile(
    r"^https?://github\.com/(?:intelrealsense|realsenseai)/librealsense/(?:blob|tree|raw)/([^/]+)/([^?#]*)",
    re.IGNORECASE,
)
WIKI_URL = re.compile(r"^https?://github\.com/(?:intelrealsense|realsenseai)/librealsense/wiki/?([^?#]*)", re.IGNORECASE)
FENCE = re.compile(r"^\s*(`{3}|~{3})")
CODE_SPAN = re.compile(r"`[^`]*`")
WIKI_LINK = re.compile(r"\[\[([^\]]+)\]\]")
MD_LINK = re.compile(r"!?\[[^\]]*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
HTML_LINK = re.compile(r"(?:href|src)\s*=\s*[\"']([^\"']+)[\"']", re.IGNORECASE)
HEADER = re.compile(r"^include/librealsense2/(h/|hpp/)?[^/]+\.(h|hpp)$")


class InputError(Exception):
    pass


# ---------- shared helpers ----------

def git(cwd, *args):
    proc = subprocess.run(["git", "-C", cwd] + list(args), capture_output=True, text=True)
    if proc.returncode != 0:
        raise InputError("git " + " ".join(args) + ": " + proc.stderr.strip())
    return proc.stdout


def git_ok(cwd, *args):
    return subprocess.run(["git", "-C", cwd] + list(args), capture_output=True, text=True).returncode == 0


def glob_match(path, pattern):
    """'**' matches across folders, '*' within one folder."""
    regex = re.escape(pattern).replace(r"\*\*", ".*").replace(r"\*", "[^/]*")
    return re.fullmatch(regex, path) is not None


def matches_any(path, patterns):
    return any(glob_match(path, pattern) for pattern in patterns)


def title_of(file_name):
    return file_name[:-3].replace("-", " ")


def norm_page(name):
    return urllib.parse.unquote(name).strip().replace("-", " ").lower()


def load_manifest(wiki):
    path = os.path.join(wiki, ".wikigen", "manifest.json")
    try:
        with open(path) as handle:
            return json.load(handle)
    except (OSError, ValueError) as err:
        raise InputError("cannot read " + path + ": " + str(err))


def subject_files(manifest, include_nav=False):
    return [s["file"] for s in manifest["subjects"] if include_nav or s["kind"] != "nav"]


def state_dir(wiki):
    return os.path.join(wiki, ".wikigen", "state")


def read_state(wiki, file_name):
    path = os.path.join(state_dir(wiki), file_name[:-3] + ".json")
    if not os.path.exists(path):
        return None
    with open(path) as handle:
        return json.load(handle)


def read_text(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def write_text(path, text):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


# ---------- validate ----------

def validate(manifest, wiki):
    problems = []
    for key in REQUIRED_KEYS:
        if key not in manifest:
            problems.append("manifest: missing key " + key)
    if problems:
        return problems
    seen = set()
    for subject in manifest["subjects"]:
        name = subject.get("file", "")
        if not name.endswith(".md"):
            problems.append("subject without a .md file name: " + json.dumps(subject))
            continue
        if name in seen:
            problems.append("duplicate file " + name)
        seen.add(name)
        if subject.get("kind") not in KINDS:
            problems.append(name + ": unknown kind " + str(subject.get("kind")))
        elif subject["kind"] != "nav":
            # section "" = a maintained page that is not listed in the sidebar (Home)
            if subject.get("section") not in manifest["sections"] and subject.get("section") != "":
                problems.append(name + ": section not in sections list: " + str(subject.get("section")))
            if not subject.get("sources"):
                problems.append(name + ": no sources")
    for name in manifest["write_first"]:
        if name not in seen:
            problems.append("write_first names " + name + ", which is not a subject")
    stems = set(f[:-3] for f in subject_files(manifest))
    if os.path.isdir(state_dir(wiki)):
        for entry in sorted(os.listdir(state_dir(wiki))):
            if not entry.endswith(".json") or entry[:-5] not in stems:
                problems.append("state file " + entry + " matches no subject")
    return problems


# ---------- coverage + plan ----------

def coverage(manifest, src):
    files = [f for f in git(src, "ls-files").splitlines() if f]
    patterns = []
    for subject in manifest["subjects"]:
        if subject["kind"] != "nav" and not subject.get("catch_all"):
            patterns.extend(subject["sources"])
    excluded = [e["path"] for e in manifest["exclusions"]]
    prefixes = tuple(manifest["out_of_scope"]["prefixes"])
    oos_files = set(manifest["out_of_scope"]["files"])

    def handled(path):
        return matches_any(path, patterns) or matches_any(path, excluded)

    result = dict(doc=[], tool=[], wrapper=[], example=[], header=[])
    for path in files:
        if path.lower().endswith(".md") and not path.startswith(prefixes) and path not in oos_files:
            if not handled(path):
                result["doc"].append(path)
        if HEADER.match(path) and not handled(path):
            result["header"].append(path)
    for kind, top in (("tool", "tools/"), ("wrapper", "wrappers/"), ("example", "examples/")):
        folders = dict()
        for path in files:
            parts = path.split("/")
            if path.startswith(top) and len(parts) >= 3:
                folder = top + parts[1] + "/"
                folders[folder] = folders.get(folder, False) or handled(path)
        result[kind] = sorted(folder for folder, ok in folders.items() if not ok)
    for kind in result:
        result[kind] = sorted(result[kind])
    return result


def write_order(manifest):
    order = [f for f in manifest["write_first"]]
    order += [f for f in subject_files(manifest) if f not in order]
    return order


def plan(manifest, src, wiki, base):
    end = git(src, "rev-parse", "HEAD").strip()
    tag_proc = subprocess.run(
        ["git", "-C", src, "describe", "--tags", "--abbrev=0", "--match", "v*", "HEAD"],
        capture_output=True, text=True,
    )
    last_tag = tag_proc.stdout.strip() if tag_proc.returncode == 0 else ""
    sources = dict((s["file"], s) for s in manifest["subjects"])
    pending, written, to_update = [], [], []
    for name in write_order(manifest):
        state = read_state(wiki, name)
        if state is None:
            pending.append(name)
            continue
        commit = state.get("commit", "")
        missing = not (git_ok(src, "cat-file", "-e", commit + "^0") and git_ok(src, "merge-base", "--is-ancestor", commit, "HEAD"))
        changed = []
        if not missing:
            diff = git(src, "diff", "--name-only", "--no-renames", commit, "HEAD").splitlines()
            changed = sorted(p for p in diff if p and matches_any(p, sources[name]["sources"]))
        entry = dict(file=name, commit=commit, tag=state.get("tag", ""), changed=changed, commit_missing=missing)
        if sources[name]["kind"] == "release":
            entry["new_tag"] = bool(last_tag) and state.get("tag", "") != last_tag
        written.append(entry)
    for name in subject_files(manifest):
        entry = [w for w in written if w["file"] == name]
        if entry and (entry[0]["changed"] or entry[0]["commit_missing"] or entry[0].get("new_tag")):
            to_update.append(name)
    prefix = git(wiki, "rev-parse", "--show-prefix").strip()
    state_prefix = prefix + ".wikigen/state/"
    base_files = git(wiki, "ls-tree", "-r", "--name-only", "--full-name", base).splitlines()
    merged = set(p for p in base_files if p.startswith(state_prefix))
    waiting = 0
    if os.path.isdir(state_dir(wiki)):
        for entry_name in os.listdir(state_dir(wiki)):
            if entry_name.endswith(".json") and state_prefix + entry_name not in merged:
                waiting += 1
    return dict(
        end=end, end7=end[:7], last_tag=last_tag, pending=pending, written=written,
        to_update=to_update, waiting_new=waiting, uncovered=coverage(manifest, src),
    )


# ---------- state ----------

def write_state(manifest, wiki, file_name, commit, tag):
    if file_name not in subject_files(manifest):
        raise InputError(file_name + " is not a (non-nav) subject in the manifest")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise InputError("commit must be a full 40-character SHA")
    os.makedirs(state_dir(wiki), exist_ok=True)
    text = json.dumps(dict(commit=commit, file=file_name, tag=tag), indent=1, sort_keys=True) + "\n"
    write_text(os.path.join(state_dir(wiki), file_name[:-3] + ".json"), text)


# ---------- blocks ----------

def markers(name):
    return "<!-- wikigen:" + name + " -->", "<!-- /wikigen:" + name + " -->"


def find_block(text, name):
    """Return (start, end): text[start:end] is the block content, or None."""
    open_m, close_m = markers(name)
    lines = text.split("\n")
    pos, start, end = 0, None, None
    for line in lines:
        if line.strip() == open_m and start is None:
            start = pos + len(line) + 1
        elif line.strip() == close_m and start is not None:
            end = pos
            break
        pos += len(line) + 1
    if start is None or end is None:
        return None
    return start, end


def block_get(path, name):
    text = read_text(path)
    span = find_block(text, name)
    if span is None:
        raise InputError("no wikigen:" + name + " block in " + path)
    return text[span[0]:span[1]]


def block_set(path, name, content):
    text = read_text(path)
    span = find_block(text, name)
    if span is None:
        raise InputError("no wikigen:" + name + " block in " + path)
    if content and not content.endswith("\n"):
        content += "\n"
    write_text(path, text[:span[0]] + content + text[span[1]:])


def block_ensure(path, name, top):
    text = read_text(path) if os.path.exists(path) else ""
    if find_block(text, name) is not None:
        return
    open_m, close_m = markers(name)
    empty = open_m + "\n" + close_m + "\n"
    if top:
        text = empty + ("\n" + text if text else "")
    else:
        text = text + ("\n" if text and not text.endswith("\n\n") else "") + empty
    write_text(path, text)


def freeze(path, heading):
    content = block_get(path, "upcoming")
    lines = content.strip("\n").split("\n")
    if lines and lines[0].startswith("## "):
        lines = lines[1:]
    body = "\n".join(lines).strip("\n")
    if not body.strip():
        raise InputError("the upcoming block in " + path + " is empty")
    text = read_text(path)
    span = find_block(text, "upcoming")
    close_m = markers("upcoming")[1]
    after_close = text.index(close_m, span[1]) + len(close_m) + 1
    section = heading + "\n\n_Draft, not yet reviewed._\n\n" + body + "\n"
    text = text[:span[0]] + text[span[1]:after_close] + "\n" + section + text[after_close:]
    write_text(path, text)


# ---------- nav ----------

def subject_list(manifest):
    lines = []
    for section in manifest["sections"]:
        names = [s["file"] for s in manifest["subjects"] if s["kind"] != "nav" and s["section"] == section]
        if names:
            lines.append("### " + section)
            lines.extend("- [[" + title_of(n) + "]]" for n in names)
            lines.append("")
    return lines


def nav(manifest, wiki):
    lines = ["**RealSense SDK Wiki**", "", "- [[Home]]", ""] + subject_list(manifest)
    for group in manifest["sidebar_links"]:
        lines.append("### " + group["section"])
        lines.extend("- [" + label + "](" + url + ")" for label, url in group["links"])
        lines.append("")
    write_text(os.path.join(wiki, "_Sidebar.md"), "\n".join(lines).rstrip("\n") + "\n")
    home = os.path.join(wiki, "Home.md")
    block_ensure(home, "subjects", top=False)
    block_set(home, "subjects", "\n".join(["## Contents", ""] + subject_list(manifest)).rstrip("\n") + "\n")


# ---------- links ----------

def default_fetch(url, timeout=15):
    for method in ("HEAD", "GET"):
        request = urllib.request.Request(url, method=method, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return response.status, None
        except urllib.error.HTTPError as err:
            if method == "HEAD" and err.code in (403, 405, 501):
                continue
            return err.code, None
        except urllib.error.URLError as err:
            if isinstance(err.reason, socket.gaierror):
                return None, "dns"
            return None, "no response"
        except Exception:
            return None, "no response"
    return None, "no response"


def check_external(items):
    by_url = dict()
    for item in items:
        by_url.setdefault(item["link"], []).append(item)
    broken, unknown, ok = [], [], 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        results = pool.map(lambda url: (url, default_fetch(url)), sorted(by_url))
        for url, (status, error) in results:
            if status is not None and 200 <= status < 400:
                ok += 1
                continue
            if error == "dns" or status in (404, 410):
                reason, target = ("domain does not resolve" if error == "dns" else "HTTP " + str(status)), broken
            else:
                reason, target = (error or "HTTP " + str(status)), unknown
            for item in by_url[url]:
                target.append(dict(item, reason=reason))
    return broken, unknown, ok


def known_pages(manifest, wiki):
    pages = set(norm_page(title_of(f)) for f in subject_files(manifest, include_nav=True))
    for entry in os.listdir(wiki):
        if entry.endswith(".md"):
            pages.add(norm_page(title_of(entry)))
    return pages


def block_lines(text, name):
    """1-based line numbers inside a marked block (empty set when there is none)."""
    open_m, close_m = markers(name)
    inside, numbers = False, set()
    for number, line in enumerate(text.split("\n"), 1):
        if line.strip() == close_m:
            inside = False
        if inside:
            numbers.add(number)
        if line.strip() == open_m:
            inside = True
    return numbers


def check_links(manifest, src, wiki, files, external, block=None):
    pages = known_pages(manifest, wiki)
    broken, outside, skipped, ok = [], [], 0, 0
    for path in files:
        rel = os.path.basename(path)
        text = read_text(path)
        only = block_lines(text, block) if block else None
        in_fence = False
        for number, line in enumerate(text.split("\n"), 1):
            if only is not None and number not in only:
                continue
            if FENCE.match(line):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            plain = CODE_SPAN.sub("", line)
            for inner in WIKI_LINK.findall(plain):
                page = inner.split("|")[-1].split("#")[0]
                if norm_page(page) in pages:
                    ok += 1
                else:
                    broken.append(dict(file=rel, line=number, link="[[" + inner + "]]", reason="no such wiki page"))
            for url in MD_LINK.findall(plain) + HTML_LINK.findall(plain):
                lower = url.lower()
                repo = REPO_URL.match(url)
                wiki_url = WIKI_URL.match(url)
                if lower.startswith(("mailto:", "tel:", "#")):
                    skipped += 1
                elif repo:
                    if repo.group(1) != "development":
                        skipped += 1
                    elif os.path.exists(os.path.join(src, urllib.parse.unquote(repo.group(2)).rstrip("/"))):
                        ok += 1
                    else:
                        broken.append(dict(file=rel, line=number, link=url, reason="not found on development"))
                elif wiki_url:
                    page = wiki_url.group(1) or "Home"
                    if norm_page(page) in pages:
                        ok += 1
                    else:
                        broken.append(dict(file=rel, line=number, link=url, reason="no such wiki page"))
                elif lower.startswith(("http://", "https://", "//")):
                    outside.append(dict(file=rel, line=number, link=("https:" + url) if url.startswith("//") else url))
                else:
                    target = urllib.parse.unquote(url.split("#")[0])
                    if not target:
                        skipped += 1
                    elif os.path.exists(os.path.join(wiki, target)) or norm_page(target) in pages:
                        ok += 1
                    else:
                        broken.append(dict(file=rel, line=number, link=url, reason="not found in the wiki folder"))
    unknown = []
    if external:
        ext_broken, unknown, ext_ok = check_external(outside)
        broken += ext_broken
        ok += ext_ok
    else:
        unknown = [dict(item, reason="not checked (run with --external)") for item in outside]
    return dict(broken=broken, unknown=unknown, ok=ok, skipped=skipped)


# ---------- cmake-options ----------

OPTION_RE = re.compile(r'^\s*option\s*\(\s*([A-Za-z0-9_]+)\s+"([^"]*)"\s+([^)\s]+)\s*\)\s*(#.*)?$')
CACHE_RE = re.compile(r'^\s*set\s*\(\s*([A-Za-z0-9_]+)\s+"([^"]*)"\s+CACHE\s+(\w+)\s+"([^"]*)"\s*\)\s*(#.*)?$')
IF_RE = re.compile(r"^\s*if\s*\((.*)\)\s*$", re.IGNORECASE)
ELSE_RE = re.compile(r"^\s*else\s*\(\s*\)\s*$", re.IGNORECASE)
ENDIF_RE = re.compile(r"^\s*endif\s*\(.*\)\s*$", re.IGNORECASE)


def cmake_options(src):
    path = os.path.join(src, "CMake", "lrs_options.cmake")
    stack, frame_id, comments = [], 0, []
    order, rows = [], dict()
    for raw in read_text(path).split("\n"):
        line = raw.strip()
        if line.startswith("#"):
            if not line.startswith("##"):
                comments.append(line.lstrip("#").strip())
            continue
        cond = IF_RE.match(line)
        if cond:
            frame_id += 1
            stack.append(dict(id=frame_id, cond=cond.group(1).strip(), in_else=False))
            continue
        if ELSE_RE.match(line) and stack:
            stack[-1]["in_else"] = True
            continue
        if ENDIF_RE.match(line) and stack:
            stack.pop()
            continue
        option = OPTION_RE.match(line)
        cache = CACHE_RE.match(line)
        if not option and not cache:
            continue
        if option:
            name, desc, value, note = option.group(1), option.group(2), option.group(3), option.group(4)
            ref = re.fullmatch(r"\$\{([A-Za-z0-9_]+)\}", value)
            shown = ("same as `" + ref.group(1) + "`") if ref else ("`" + value + "`")
        else:
            name, value, kind, desc, note = cache.group(1), cache.group(2), cache.group(3), cache.group(4), cache.group(5)
            shown = '`"' + value + '"` (' + kind.lower() + ")"
        frames = [dict(f) for f in stack]
        if name not in rows:
            order.append(name)
            notes = [" ".join(comments)] if comments else []
            if note:
                notes.append(note.lstrip("#").strip())
            rows[name] = dict(desc=desc, notes=notes, entries=[])
        rows[name]["entries"].append((shown, frames))
        comments = []
    out = ["| Option | Default | Description |", "|---|---|---|"]
    for name in order:
        row = rows[name]
        parts = []
        for shown, frames in row["entries"]:
            conds = []
            for frame in frames:
                if not frame["in_else"]:
                    conds.append("if `" + frame["cond"] + "`")
                else:
                    sibling = any(
                        any(f["id"] == frame["id"] and not f["in_else"] for f in other)
                        for _, other in row["entries"]
                    )
                    conds.append("otherwise" if sibling else "if not `" + frame["cond"] + "`")
            parts.append(shown + (" " + " and ".join(conds) if conds else ""))
        desc = row["desc"]
        for note in row["notes"]:
            desc += (" " if desc.endswith(".") else ". ") + note
        cells = ["`" + name + "`", "; ".join(parts), desc]
        out.append("| " + " | ".join(c.replace("|", "\\|") for c in cells) + " |")
    return "\n".join(out) + "\n"


# ---------- CLI ----------

def main(argv):
    parser = argparse.ArgumentParser(prog="wikigen.py")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("validate")
    p.add_argument("--wiki", required=True)
    p = sub.add_parser("plan")
    p.add_argument("--src", required=True)
    p.add_argument("--wiki", required=True)
    p.add_argument("--base", required=True)
    p = sub.add_parser("state")
    p.add_argument("--wiki", required=True)
    p.add_argument("--file", required=True)
    p.add_argument("--commit", required=True)
    p.add_argument("--tag", default="")
    p = sub.add_parser("nav")
    p.add_argument("--wiki", required=True)
    p = sub.add_parser("block")
    p.add_argument("action", choices=("get", "set", "ensure"))
    p.add_argument("--file", required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--top", action="store_true")
    p = sub.add_parser("freeze")
    p.add_argument("--file", required=True)
    p.add_argument("--heading", required=True)
    p = sub.add_parser("links")
    p.add_argument("--src", required=True)
    p.add_argument("--wiki", required=True)
    p.add_argument("--external", action="store_true")
    p.add_argument("--block", default=None)
    p.add_argument("files", nargs="+")
    p = sub.add_parser("cmake-options")
    p.add_argument("--src", required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "validate":
            problems = validate(load_manifest(args.wiki), args.wiki)
            print("\n".join(problems) if problems else "OK")
            return 1 if problems else 0
        if args.command == "plan":
            print(json.dumps(plan(load_manifest(args.wiki), args.src, args.wiki, args.base), indent=1))
        elif args.command == "state":
            write_state(load_manifest(args.wiki), args.wiki, args.file, args.commit, args.tag)
        elif args.command == "nav":
            nav(load_manifest(args.wiki), args.wiki)
        elif args.command == "block":
            if args.action == "get":
                sys.stdout.write(block_get(args.file, args.name))
            elif args.action == "set":
                block_set(args.file, args.name, sys.stdin.read())
            else:
                block_ensure(args.file, args.name, args.top)
        elif args.command == "freeze":
            freeze(args.file, args.heading)
        elif args.command == "links":
            report = check_links(load_manifest(args.wiki), args.src, args.wiki, args.files, args.external, args.block)
            print(json.dumps(report, indent=1))
        elif args.command == "cmake-options":
            sys.stdout.write(cmake_options(args.src))
    except InputError as err:
        print("error: " + str(err), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
