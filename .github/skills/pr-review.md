# Pull Request Workflow

## When to Use This Skill

Opening a PR, pushing new commits to one, or replying to review comments on `realsenseai/librealsense`.

## Description Format

Every PR description starts with a TL;DR (1–2 sentences stating the user-visible change), then the body:

```markdown
**TL;DR:** <1-2 sentences>

## Summary
- <what changed and why>

## Why
<bug, regression, or capability gap>

## Test plan
- [ ] <unit/pytest entries, marker/context/iteration counts>
- [ ] <manual or CI verification>
```

Keep the TL;DR jargon-free. Put tables (behavior change, latency, benchmarks) in the body, never in the TL;DR.

If you **know** which Jira ticket the PR is tracking (the user told you, or you opened the ticket yourself this session), append a `Tracked on [RSDEV-1234]` line right after the TL;DR — bare text, no link:

```markdown
**TL;DR:** <1-2 sentences>

Tracked on [RSDEV-1234]
```

Only add this line when you are sure. If you only suspect a ticket is related, ask the user before adding it — never guess.

## Before Every Push — Description Audit

Re-read the PR description before `git push`. If any concrete detail in the description (iteration counts, timeouts, file paths, marker lists, behavior tables, referenced PR numbers) no longer matches what's actually on the branch, update the description in the same push (`gh pr edit <num> --body ...`).

The description and the diff must stay in sync.

## Responding to Review Comments

- **One reply per thread.** Don't summarise multiple threads in one comment.
- Cite the commit SHA that addresses the comment (`✅ Fixed in <sha>. <one-line summary>`).
- For deferred items: `⏸ Deferred — <reason>`.
- For disagreements: give a concrete reason (worked example, edge case) before declining.
- Don't auto-resolve threads — let the reviewer decide when their concern is settled.

## Handling Automated Review Bots

| Bot | How to respond |
|---|---|
| **Aikido** (`aikido-pr-checks[bot]`) | Fix and reply with commit SHA, or reply `@AikidoSec ignore: <reason>` if the suggestion is wrong or the pattern is intentional. |
| **rs-agentic-bot** (posts as a teammate) | Treat as a real reviewer: fix or defer; reply per thread. |
| **Copilot suggestions** | Apply only if they preserve intent; reply with rationale if declined. |
