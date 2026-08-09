# Rhyno skills (committed)

These skills ship in the repo so **local PC agents** and **remote Linux agents**
load the same files after clone.

| Folder | Role |
|--------|------|
| `RhynoDigital` | Voice and update rules for Jurien |
| `rhynoTodoList` | Stage tracker skill (living `list.yaml` stays PC-only) |
| `RhynoSprintPlanCreate` | Sprint plan gate + in-repo `kpi.yaml` |

Install / refresh from Jurien's PC workshop:

```text
powershell -File $HOME/.cursor/skills/install-rhyno-skills.ps1 -RepoRoot <this-repo>
```

Do not add `list.yaml` here. See each `SKILL.md` for local vs remote behaviour.
