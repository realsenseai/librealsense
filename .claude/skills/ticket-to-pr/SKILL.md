---
name: ticket-to-pr
description: Take a Jira ticket through design, implementation, review loop and PR. Use when given a ticket ID.
---
Argument: Jira ticket ID.

1. READ: fetch the ticket via the Jira REST API (Bearer token, supplied by the user's own
   setup - never write the token into the repo). Summarize requirements and acceptance
   criteria into `.review/<ticket>/design.md` with: goal, approach, files to touch,
   rejected alternatives, test plan. Stop and show the design for approval.
2. IMPLEMENT: new branch off development (no username prefix). Follow the cpp-coding,
   add-api, file-creation and pytest-infra skills. Build and test per the build/testing
   skills. Commit with short messages.
3. REVIEW LOOP (max 4 rounds):
   a. Spawn the `lrs-reviewer` subagent (fresh each round) with the review file
      `.review/<ticket>/review.md` and the round number.
   b. If it reports `OPEN: 0`, exit the loop.
   c. Otherwise fix every open finding yourself (you hold the design context). If you
      disagree with a finding, write a one-line rebuttal in the file instead of ignoring it.
      Rebuild and rerun affected tests. Commit.
   d. Go to (a).
   If still open after round 4, stop and report the remaining findings to the user.
4. PR: follow the pr-create skill. Show the title and description and wait for the user's
   explicit OK before any push or `gh pr create`.
