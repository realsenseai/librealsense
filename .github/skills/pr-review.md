# PR Review Workflow

## When to Use This Skill

Replying to review comments (human or bot) on a `realsenseai/librealsense` pull request.

For opening a PR, pushing commits to one, or updating its description, see `.github/skills/pr-create.md`.

## Responding to Review Comments

- **One reply per thread.** Don't summarise multiple threads in one comment.
- Cite the commit SHA that addresses the comment (`✅ Fixed in <sha>. <one-line summary>`).
- For deferred items: `⏸ Deferred — <reason>`.
- Be concise. Use short sentences.
- For disagreements: give a concrete reason (worked example, edge case) before declining.
- Don't auto-resolve threads — let the reviewer decide when their concern is settled.
- Don't link to PR comments as GitHub issues ("see https://github.com/realsenseai/librealsense/issues/4")

## Handling Automated Review Bots

| Bot | How to respond |
|---|---|
| **Aikido** (`aikido-pr-checks[bot]`) | Fix and reply with commit SHA, or reply `@AikidoSec ignore: <reason>` if the suggestion is wrong or the pattern is intentional. |
| **rs-agentic-bot** (posts as a teammate) | Treat as a real reviewer: fix or defer; reply per thread. |
| **Copilot suggestions** | Apply only if they preserve intent; reply with rationale if declined. |
