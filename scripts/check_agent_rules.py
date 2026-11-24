import os
import re
import subprocess
import sys

USAGE = """usage: check_agent_rules.py [--full | --diff-stdin]
  Checks the lines a commit adds (git diff --cached) against the mechanical rules in AGENTS.md.
  --full        scan every checked file under GWToolboxdll instead of the staged diff.
  --diff-stdin  read a unified diff from stdin instead (for testing)."""

EXEMPT_PATH = re.compile(r"(imgui_impl_|imconfig\.h|ToolboxIni\.|sha1\.|/Unused/|/Shaders/|^Dependencies/|^vcpkg)", re.I)
CHECKED_EXT = (".cpp", ".h")
MODULE_DIRS = ("GWToolboxdll/Modules/", "GWToolboxdll/Windows/", "GWToolboxdll/Widgets/")
INCLUDE_ROOTS = ["GWToolboxdll", "Dependencies/GWCA/include", "Dependencies", "Core", "Core/include", "RestClient/include", ""]

TODO = re.compile(r"\b(TODO|FIXME|XXX|HACK)\b")
FALLTHROUGH = re.compile(r"\[\[fallthrough\]\]")
PRIVATE_LABEL = re.compile(r"^\s*private\s*:\s*$")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
BANNER = re.compile(r"^[=\-*/#~_ ]{3,}$")
LICENCE = re.compile(r"\b(licen[sc]e|copyright)\b", re.I)


def repo_root():
    return subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True, check=True).stdout.strip()


def staged_diff():
    return subprocess.run(["git", "diff", "--cached", "-U0", "--no-color", "--diff-filter=AM"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


def added_lines(diff_text):
    """Yield (path, line_no, text) for every added line in a -U0 unified diff."""
    path = None
    new_no = 0
    for raw in diff_text.splitlines():
        if raw.startswith("+++ "):
            target = raw[4:]
            path = target[2:] if target.startswith("b/") else None
            continue
        m = re.match(r"^@@ -\S+ \+(\d+)(?:,\d+)? @@", raw)
        if m:
            new_no = int(m.group(1))
            continue
        if raw.startswith("+") and not raw.startswith("+++") and path:
            yield path, new_no, raw[1:]
            new_no += 1


def full_scan_lines(root):
    """Yield (path, line_no, text) for every line of every checked file under GWToolboxdll."""
    base = os.path.join(root, "GWToolboxdll")
    for directory, _, names in os.walk(base):
        for name in sorted(names):
            full = os.path.join(directory, name)
            rel = os.path.relpath(full, root).replace("\\", "/")
            if not rel.endswith(CHECKED_EXT) or EXEMPT_PATH.search(rel):
                continue
            with open(full, encoding="utf-8", errors="replace") as fh:
                for line_no, text in enumerate(fh, 1):
                    yield rel, line_no, text.rstrip("\r\n")


def disabled_lines(full_path):
    """Line numbers inside #if 0 blocks (or #else branches of one), read from the file on disk."""
    try:
        with open(full_path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
    except OSError:
        return set()
    disabled = set()
    stack = []
    for line_no, text in enumerate(lines, 1):
        s = text.strip()
        if re.match(r"#\s*if\s+0\b", s):
            stack.append(True)
        elif re.match(r"#\s*if", s):
            stack.append(False)
        elif re.match(r"#\s*(else|elif)\b", s) and stack:
            stack[-1] = False
        elif re.match(r"#\s*endif", s) and stack:
            stack.pop()
        if any(stack):
            disabled.add(line_no)
    return disabled


def is_comment(text):
    s = text.strip()
    return s.startswith("//") or s.startswith("/*") or s.startswith("*")


def comment_body(text):
    s = text.strip()
    if s.startswith("//"):
        return s[2:].strip()
    return re.sub(r"^(/\*+|\*+)|\*+/$", "", s).strip()


def looks_like_code(body):
    b = re.sub(r'"(?:\\.|[^"\\])*"', '""', body).strip()
    if not b or re.search(r"[a-z]{3,}\s+[a-z]{3,}\s+[a-z]{3,}\s+[a-z]{3,}", b) or b.endswith("."):
        return False
    return bool(re.search(r"[;{}]$", b)) and bool(re.search(r"[(=\[]|::|->|\breturn\b", b))


def include_exists(root, source_dir, inc):
    candidates = [os.path.join(source_dir, inc)] + [os.path.join(r, inc) for r in INCLUDE_ROOTS]
    for candidate in candidates:
        full = os.path.join(root, candidate) if not os.path.isabs(candidate) else candidate
        directory, name = os.path.split(full)
        if os.path.isdir(directory) and name in os.listdir(directory) and os.path.isfile(full):
            return True
    return False


def check(lines, root):
    violations = []
    run = []

    def flush_run(path):
        if len(run) > 2:
            violations.append((path, run[0][0], "comment-block", f"{len(run)} consecutive comment lines (max 2)"))
        run.clear()

    current = None
    last_line = None
    disabled = set()
    for path, line_no, text in lines:
        if current != path or line_no != (last_line or 0) + 1:
            flush_run(current)
            current = path
            disabled = disabled_lines(os.path.join(root, path)) if path.endswith(CHECKED_EXT) else set()
        last_line = line_no
        if EXEMPT_PATH.search(path) or not path.endswith(CHECKED_EXT):
            continue
        if line_no in disabled:
            flush_run(path)
            continue
        if FALLTHROUGH.search(text):
            violations.append((path, line_no, "no-fallthrough", "[[fallthrough]] is banned; handle each case explicitly"))
        body = comment_body(text) if is_comment(text) else None
        if body is None or BANNER.match(body) or LICENCE.search(text):
            flush_run(path)
        else:
            if TODO.search(text):
                violations.append((path, line_no, "no-todo", "TODO/FIXME comments are not allowed"))
            if text.strip().startswith("//") and looks_like_code(body):
                violations.append((path, line_no, "no-commented-code", "commented-out code; delete it"))
            run.append((line_no, text))
        if path.endswith(".h") and path.startswith(MODULE_DIRS) and PRIVATE_LABEL.match(text):
            violations.append((path, line_no, "no-module-privates",
                               "module headers must not declare private members; move them to the .cpp anonymous namespace"))
        m = INCLUDE.match(text)
        if m and not EXEMPT_PATH.search(path):
            inc = m.group(1)
            if inc.startswith(("GWCA/", "Utils/", "Windows/", "Modules/", "Constants/", "Defines.h", "Color.h", "ToolboxWindow.h", "Widgets/", "ToolboxModule.h", "ToolboxUIElement.h")):
                source_dir = os.path.dirname(path)
                if not include_exists(root, source_dir, inc):
                    violations.append((path, line_no, "include-case", f"#include <{inc}> is not found with this exact casing (the Linux build is case-sensitive)"))
    flush_run(current)
    return violations


def main(argv):
    if "-h" in argv or "--help" in argv:
        print(USAGE)
        return 0
    root = repo_root()
    if "--full" in argv:
        violations = check(full_scan_lines(root), root)
        header = "AGENTS.md scan of GWToolboxdll found problems:\n"
    else:
        diff_text = sys.stdin.read() if "--diff-stdin" in argv else staged_diff()
        violations = check(added_lines(diff_text), root)
        header = "AGENTS.md check failed. Fix these lines, or commit with --no-verify if this is intentional:\n"
    if not violations:
        if "--full" in argv:
            print("AGENTS.md scan: no violations in GWToolboxdll.")
        return 0
    print(header)
    for path, line_no, rule, message in violations:
        print(f"  {path}:{line_no}: [{rule}] {message}")
    print(f"\n{len(violations)} violation(s).")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
