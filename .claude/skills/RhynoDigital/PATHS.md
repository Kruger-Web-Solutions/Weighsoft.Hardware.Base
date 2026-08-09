# Rhyno skill paths — local vs remote

## In every product repo (committed — both agents)

```text
.claude/skills/RhynoDigital/SKILL.md
.claude/skills/rhynoTodoList/SKILL.md
.claude/skills/RhynoSprintPlanCreate/SKILL.md
.claude/skills/RhynoSprintPlanCreate/sprint-template.md
.claude/skills/RhynoSprintPlanCreate/kpi.yaml
```

Sprint plans:

```text
docs/superpowers/plans/
```

## PC-only (not in git — local agents on Jurien's machine)

```text
$HOME/.cursor/skills/rhynoTodoList/list.yaml
$HOME/.cursor/skills/rhynoTodoList/LIST.md
```

Authoring copies of the skills (install source):

```text
$HOME/.cursor/skills/RhynoDigital/
$HOME/.cursor/skills/rhynoTodoList/
$HOME/.cursor/skills/RhynoSprintPlanCreate/
```

Install into a product repo (from PC):

```text
pwsh $HOME/.cursor/skills/install-rhyno-skills.ps1 -RepoRoot <path-to-product-repo>
# or
bash $HOME/.cursor/skills/install-rhyno-skills.sh <path-to-product-repo>
```

## Rules

- Never put `list.yaml` under `.claude/skills/`.
- Never use Windows drive-letter paths or backslash paths inside SKILL.md bodies.
- Folder name = frontmatter `name:` = invoke name. Skill is **RhynoDigital**, never digitalrhyno.
