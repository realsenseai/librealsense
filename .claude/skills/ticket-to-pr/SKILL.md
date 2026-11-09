---
name: ticket-to-pr
description: Take a Jira ticket through design, implementation, review loop, test loop and PR. Use when given a ticket ID.
---
Argument: Jira ticket ID. The user is asked for input only at step 5.

1. READ: fetch the ticket via the Jira REST API (Bearer token, supplied by the user's own
   setup - never write the token into the repo). Summarize requirements and acceptance
   criteria into a self-contained HTML page `.review/<ticket>/design.html` with: goal,
   approach, files to touch, rejected alternatives, test plan. Add a flow diagram and/or
   class diagram (Mermaid via cdnjs, pinned version) only where it clarifies the change;
   omit diagrams entirely when they add nothing. Open it for the user and continue
   without waiting.
2. IMPLEMENT: new branch off development (no username prefix). Follow the cpp-coding,
   add-api, file-creation and pytest-infra skills. Build and test per the build/testing
   skills. Commit with short messages. Then pin the review base:
   `git fetch origin && git merge-base origin/development HEAD` -> `<base>` (a commit SHA).
3. REVIEW LOOP (max 4 rounds):
   a. Spawn the `lrs-reviewer` subagent (fresh each round) with base commit `<base>`, the
      review file `.review/<ticket>/review.md` and the round number.
   b. If it reports `OPEN: 0`, go to step 4.
   c. Otherwise fix every open finding yourself (you hold the design context). If you
      disagree with a finding, write a one-line rebuttal in the file instead of ignoring it.
      Rebuild and rerun affected tests. Commit. Go to (a).
4. TEST LOOP (max 3 rounds): sub-agents never edit product code; only you do.
   a. Spawn the `lrs-tester` subagent with base commit `<base>`, the design file,
      `.review/<ticket>/test-report.md` and the round number. It decides whether a test
      already exists, needs a small extension or must be written, then runs it.
   b. Whatever the result, first commit the tester's `unit-tests/` changes (EXTEND/NEW) on
      their own, so product fixes never mix with them and the next tester sees them.
   c. `RESULT: FAIL`: fix the product code (never weaken the test without a reason you can
      state), commit, go to (a). `NOT RUN` (no device available) is not a pass: stop and
      report it to the user, who may claim a lab machine via `/lab-machine`.
   d. `RESULT: PASS`: spawn a fresh `lrs-reviewer` on the whole diff (same inputs as 3a,
      round continues). If `OPEN: 0`, go to step 5. Otherwise fix the findings as in 3c
      and go to (a).
   If anything is still open at the round cap, stop and report it to the user.
5. HAND-OFF: follow the pr-create skill. Show the title and description and ask the user
   whether to accept the PR. Only on an explicit yes push and run `gh pr create`.
   On a no, apply the feedback and repeat from step 3.
