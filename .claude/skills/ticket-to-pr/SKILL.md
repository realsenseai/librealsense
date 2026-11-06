---
name: ticket-to-pr
description: Take a Jira ticket through design, implementation, review loop and PR. Use when given a ticket ID.
---
Argument: Jira ticket ID.

1. READ: fetch the ticket via the Jira REST API (Bearer token, supplied by the user's own
   setup - never write the token into the repo). Summarize requirements and acceptance
   criteria into a self-contained HTML page `.review/<ticket>/design.html` with: goal,
   approach, files to touch, rejected alternatives, test plan. Add a flow diagram and/or
   class diagram (Mermaid via cdnjs, pinned version) only where it clarifies the change;
   omit diagrams entirely when they add nothing. Open it for the user, then stop and wait
   for approval.
2. IMPLEMENT: new branch off development (no username prefix). Follow the cpp-coding,
   add-api, file-creation and pytest-infra skills. Build and test per the build/testing
   skills. Commit with short messages. Then pin the review base:
   `git fetch origin && git merge-base origin/development HEAD` -> `<base>` (a commit SHA).
3. VALIDATE (max 3 rounds): spawn the `lrs-tester` subagent with base commit
   `<base>`, the design file, `.review/<ticket>/test-report.md` and the round number.
   It reports whether existing tests cover the change (run them), need a small extension,
   or a new test is needed, and runs the result.
   On `RESULT: FAIL`, fix the product code (never weaken the test without a reason you
   can state), commit, and respawn it. `NOT RUN` (no device available) is not a pass:
   report it to the user, who may claim a lab machine via `/lab-machine`.
4. REVIEW LOOP (max 4 rounds):
   a. Spawn the `lrs-reviewer` subagent (fresh each round) with base commit
      `<base>`, the review file `.review/<ticket>/review.md` and the round number.
   b. If it reports `OPEN: 0`, exit the loop.
   c. Otherwise fix every open finding yourself (you hold the design context). If you
      disagree with a finding, write a one-line rebuttal in the file instead of ignoring it.
      Rebuild and rerun affected tests. Commit.
   d. Go to (a).
   If still open after round 4, stop and report the remaining findings to the user.
   If the loop changed product code, rerun `lrs-tester` once more before the PR.
5. PR: follow the pr-create skill. Show the title and description and wait for the user's
   explicit OK before any push or `gh pr create`.
