# PR Creation & Update Workflow

## When to Use This Skill

Opening a PR, pushing new commits to one, or updating its description on `realsenseai/librealsense`.

## Tracking Ticket — Required

Every PR requires a tracking Jira ticket.

- If you **know** the ticket (the user told you, or you opened the ticket yourself this session), add a `Tracked on [RSDEV-1234]` line right after the Overview — bare text, no link.
- If you are **not sure** which ticket is relevant, **ask the user before creating the PR**. Tell them each PR requires a ticket, and let them either provide one or explicitly approve creating the PR without it.
- Never guess a ticket. Only omit the `Tracked on` line when the user explicitly says it's OK without one.

## Description Format

Every PR description starts with an Overview (1–2 sentences stating the user-visible change), then the body:

```markdown
**Overview:** <1-2 sentences>

Tracked on [RSDEV-1234]

## Why
<bug, regression, or capability gap>

## Summary
- <what changed and why>

## Test plan
- [ ] <unit/pytest entries, marker/context/iteration counts>
- [ ] <manual or CI verification>
```

Keep the Overview jargon-free. Put tables (behavior change, latency, benchmarks) in the body, never in the Overview.
Be concise when describing changes. Use short sentences. e.g. avoid listing multiple enum literals when describing added enum.

## Before Every Push — Description Audit

Re-read the PR description before `git push`. If any concrete detail in the description (iteration counts, timeouts, file paths, marker lists, behavior tables, referenced PR numbers) no longer matches what's actually on the branch, update the description in the same push (`gh pr edit <num> --body ...`).

The description and the diff must stay in sync.
