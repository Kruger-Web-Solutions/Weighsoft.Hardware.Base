"""Mutation harness: prove the unit tests actually catch the bugs they claim to.

A test that passes against broken code is worse than no test - it certifies
nothing while looking like assurance. Each mutation below reintroduces a bug
that really shipped. If the suite still passes, that test is decoration.
"""
import io
import subprocess
import sys

SENDER = "scripts/send-weight.py"
TESTS = "scripts/qa/test_send_weight.py"

MUTATIONS = [
    (
        "drop the final flush (the settled reading is lost)",
        "    flush()  # the settled reading always goes",
        "    pass  # MUTATED: no final flush",
    ),
    (
        "wrong device falls through to UDP hunting (hides the imposter)",
        "    if reason == WRONG_DEVICE or host:",
        "    if host:",
    ),
    (
        "wrong-device returns the not-found exit code (the original inversion)",
        "            return 3\n",
        "            return 2\n",
    ),
    (
        "treat a 0.00 reading as no reading (falsy-check trap)",
        "        if weight is None:",
        "        if not weight:",
    ),
]


def run_tests():
    r = subprocess.run([sys.executable, TESTS], capture_output=True, text=True)
    return r.returncode


def restore():
    subprocess.run(["git", "checkout", "--", SENDER], check=True)


def main():
    original = io.open(SENDER, encoding="utf-8").read()
    baseline = run_tests()
    print(f"baseline (unmutated): {'PASS' if baseline == 0 else 'FAIL'}")
    if baseline != 0:
        print("suite is red before mutating - fix that first")
        return 1

    survivors = []
    for name, old, new in MUTATIONS:
        if old not in original:
            print(f"  SKIP  {name}  (anchor not found - mutation is stale)")
            survivors.append(name + " [stale anchor]")
            continue
        io.open(SENDER, "w", encoding="utf-8", newline="").write(original.replace(old, new, 1))
        rc = run_tests()
        restore()
        if rc == 0:
            print(f"  SURVIVED  {name}  <-- tests did NOT catch this")
            survivors.append(name)
        else:
            print(f"  killed    {name}")

    print()
    if survivors:
        print(f"{len(survivors)} mutation(s) survived - the suite has blind spots:")
        for s in survivors:
            print("  -", s)
        return 1
    print(f"all {len(MUTATIONS)} mutations killed - the tests bite")
    return 0


if __name__ == "__main__":
    sys.exit(main())
