import re
import sys

REQUIRED = "Checked against AGENTS.md"

AGENT_MARKERS = [
    re.compile(r"^\s*Co-Authored-By:.*\b(claude|anthropic|haiku|opus|sonnet|fable)\b", re.I | re.M),
    re.compile(r"Generated with\s*\[?Claude", re.I),
    re.compile(r"Generated (by|with) .*\b(AI|agent|LLM)\b", re.I),
]
REQUIRED_LINE = re.compile(r"^\s*" + re.escape(REQUIRED) + r"\s*$", re.M | re.I)


def message_text(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = [l for l in fh.read().split("\n") if not l.startswith("#")]
    return "\n".join(lines)


def main(argv):
    if len(argv) != 1:
        print("usage: check_commit_message.py <commit-message-file>", file=sys.stderr)
        return 2
    text = message_text(argv[0])
    if not any(marker.search(text) for marker in AGENT_MARKERS):
        return 0
    if REQUIRED_LINE.search(text):
        return 0
    print(
        "This commit message says it was written by an agent, but it doesn't include the line:\n"
        f"    {REQUIRED}\n\n"
        "Before committing, check your change against the rules in AGENTS.md (for example, run\n"
        "`python scripts/check_agent_rules.py` and review the staged diff against the rules). Then add\n"
        f"the line `{REQUIRED}` to the commit message and commit again.\n\n"
        "This check is skipped only with `git commit --no-verify`, which needs the user's approval.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
