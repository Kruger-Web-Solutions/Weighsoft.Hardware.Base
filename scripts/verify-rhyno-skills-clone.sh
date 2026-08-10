#!/usr/bin/env bash
set -euo pipefail

VERIFY="/tmp/rhyno-skills-verify-$$"
SRC="${1:-/mnt/c/Projects/Weighsoft.Hardware.Base}"
rm -rf "$VERIFY"

echo "=== uname ==="
uname -srm

echo "=== clone ==="
git clone --depth 1 --branch RelayBoardEspBuildIn "file://${SRC}" "$VERIFY"
cd "$VERIFY"

echo "=== HEAD ==="
git rev-parse --short HEAD
git log -1 --oneline

echo "=== find RhynoDigital by directory ==="
ls -la .claude/skills/
test -d .claude/skills/RhynoDigital
test ! -d .claude/skills/digitalrhyno
echo "PASS: directory RhynoDigital exists; digitalrhyno absent"

echo "=== frontmatter name matches dir ==="
for d in RhynoDigital rhynoTodoList RhynoSprintPlanCreate; do
  name=$(grep -E '^name:' ".claude/skills/$d/SKILL.md" | head -1 | awk '{print $2}')
  echo "dir=$d name=$name"
  test "$name" = "$d"
done
echo "PASS: name == directory"

echo "=== read every referenced in-repo file ==="
for f in \
  .claude/skills/RhynoDigital/SKILL.md \
  .claude/skills/RhynoDigital/PATHS.md \
  .claude/skills/rhynoTodoList/SKILL.md \
  .claude/skills/RhynoSprintPlanCreate/SKILL.md \
  .claude/skills/RhynoSprintPlanCreate/sprint-template.md \
  .claude/skills/RhynoSprintPlanCreate/kpi.yaml
do
  test -f "$f"
  test -r "$f"
  echo "OK read $f ($(wc -c < "$f") bytes)"
done

echo "=== banned absolute Windows paths in skills ==="
if grep -REn '[A-Za-z]:\\|[A-Za-z]:/' .claude/skills --include='*.md' --include='*.yaml'; then
  echo "FAIL: drive-letter path found"
  exit 1
fi
echo "PASS: no drive-letter paths"

echo "=== list.yaml must NOT be in repo skills ==="
if test -f .claude/skills/rhynoTodoList/list.yaml; then
  echo "FAIL: list.yaml committed under skills"
  exit 1
fi
echo "PASS: list.yaml not in clone"

echo "=== PC-only path unavailable here (expected) ==="
if test -f "$HOME/.cursor/skills/rhynoTodoList/list.yaml"; then
  echo "NOTE: list.yaml exists in this HOME (WSL may see Windows home)"
else
  echo "PASS: no PC list.yaml — remote fallback applies"
fi

echo "=== remote limitation statement present ==="
grep -n "PC-only" .claude/skills/rhynoTodoList/SKILL.md | head -5

echo "ALL CHECKS PASSED"
rm -rf "$VERIFY"
