"""
SessionStart hook — reads project conventions and injects them into Claude's context.
Called automatically at the start of every Claude Code session.
"""
import glob
import json
import os


def read(path):
    try:
        # Verify path before opening file
        base_real = os.path.realpath(".github")
        target_real = os.path.realpath(path)
        if os.path.commonpath([base_real, target_real]) != base_real:
            raise Exception("Invalid file path")
        with open(target_real, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return None


parts = []

content = read(".github/copilot-instructions.md")
if content:
    parts.append("# copilot-instructions.md\n" + content)

for path in sorted(glob.glob(".github/skills/*.md")):
    content = read(path)
    if content:
        parts.append(f"# {os.path.basename(path)}\n" + content)

print(json.dumps({
    "hookSpecificOutput": {
        "hookEventName": "SessionStart",
        "additionalContext": "\n\n".join(parts),
    }
}))
