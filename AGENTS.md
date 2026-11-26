# AGENTS.md

## When triaging issues

To set, check, or explain the Priority of an issue, follow [`.claude/skills/issue-priority/SKILL.md`](.claude/skills/issue-priority/SKILL.md). It is the single source of truth for the priority rules.

To classify an issue (its impacts, its area, and its native classes), follow [`.claude/skills/issue-classification/SKILL.md`](.claude/skills/issue-classification/SKILL.md).

A vulnerability that is not public yet goes into a private security report (**Security** → **Report a vulnerability**), never into an issue or a pull request. The classification skill defines such a vulnerability, and the priority skill says how to handle the public issue.

## Upstream SQL test harness

- The upstream SQL test harness (the ClickHouse core SQL tests through the driver): see [`test/clickhouse-test-runner/AGENTS.md`](test/clickhouse-test-runner/AGENTS.md).
