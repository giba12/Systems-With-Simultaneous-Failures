#!/usr/bin/env python3
"""check_traceability.py - machine-check the requirements -> test-case -> test-code traceability.
                                                                  (Plan: Day 9; VER-003, gate G6)

WHAT IT CHECKS
    1. Every requirement ID defined in docs/requirements-spec.md appears in the matrix of docs/test-plan.md
       section 6 and maps to at least one test case (or is explicitly marked as not planned).
    2. Every test case named in the matrix is defined in docs/test-plan.md.
    3. Every test case defined in the test plan is either referenced from real test code (a TC-x-nn tag in a
       C++ or Python test) or classified below as verified another way (an experiment, a review, CI).
    4. No test case is defined twice; no classification names an unknown test case.

    Test-code tags are read from comment lines containing "TC-". Shorthand such as "TC-A-02/03", "TC-C-02, C-03"
    and "TC-D-01 to TC-D-05" is expanded.

USAGE
    python3 tools/check_traceability.py [--write docs/traceability-matrix.md]      exit status 1 on any problem
"""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
DOCS = PROJECT.parent / "docs"

REQ_ID = r"(?:SYS|ALG|REL|VER|EXP|DOC|INT|NFR)-\d{3}"

# Test cases that are verified by something other than an automated unit/integration test. Each needs a reason.
VERIFIED_ELSEWHERE = {
    "TC-H-01": "measured by Experiment 4: analysis of the six-component system takes 0.08 ms (docs/experiment-report.md section 6)",
    "TC-H-02": "clean configure + build + ctest on every push: .github/workflows/ci.yml and the local fresh-build runs in docs/verification-report.md",
    "TC-H-03": "documentation review: docs/deviation-log.md and docs/experiment-report.md section 8",
    "TC-J-01": "optional P2 repair policies: not implemented (see docs/verification-report.md)",
    "TC-J-02": "optional P2 repair policies: not implemented (see docs/verification-report.md)",
    "TC-J-03": "optional P2 repair policies: not implemented (see docs/verification-report.md)",
}
NOT_PLANNED_REQS = {"EXP-004": "optional P2 repair policies"}


# A tag list: one full id, then any number of "/", ",", "to", ".." or an ellipsis followed by another id, a
# letter-number ("B-03") or a bare two-digit number ("03"). Bare numbers are only accepted right after such a
# separator, so digits elsewhere in a sentence ("(REL-004)", "Eq. (21)") are never mistaken for test numbers.
TAG_LIST = re.compile(
    r"TC-[A-J]-\d{2}"
    r"(?:(?:\s*/\s*|\s*,\s*|\s+to\s+|\s*\.\.\s*|\s*…\s*)"
    r"(?:TC-[A-J]-\d{2}|[A-J]-\d{2}|\d{2}(?!\d)))*")
RANGE_SEPARATORS = re.compile(r"\s+to\s+|\s*\.\.\s*|\s*…\s*")


def expand_tags(line):
    """Return the set of full TC ids named on a line, expanding shorthand and ranges."""
    found = set()
    for match in TAG_LIST.finditer(line):
        text = match.group(0)
        letter = None
        previous = None
        position = 0
        for token_match in re.finditer(r"TC-[A-J]-\d{2}|[A-J]-\d{2}|\d{2}", text):
            token = token_match.group(0)
            between = text[position:token_match.start()]
            is_range = bool(RANGE_SEPARATORS.fullmatch(between)) if previous is not None else False
            if token.startswith("TC-"):
                letter, number = token[3], int(token[5:])
            elif re.fullmatch(r"[A-J]-\d{2}", token):
                letter, number = token[0], int(token[2:])
            else:
                number = int(token)
            if is_range and number > previous:
                for n in range(previous, number + 1):
                    found.add(f"TC-{letter}-{n:02d}")
            found.add(f"TC-{letter}-{number:02d}")
            previous, position = number, token_match.end()
    return found


def read_requirements():
    ids = []
    for line in (DOCS / "requirements-spec.md").read_text().splitlines():
        match = re.match(rf"^\| ({REQ_ID})\b", line)
        if match:
            ids.append(match.group(1))
    return ids


def read_test_plan():
    """Return (defined test cases {id: title}, matrix {requirement: [test case ids]}, duplicate ids)."""
    defined, duplicates, matrix = {}, [], {}
    in_matrix = False
    for line in (DOCS / "test-plan.md").read_text().splitlines():
        if line.startswith("## 6."):
            in_matrix = True
            continue
        if in_matrix and line.startswith("## "):
            in_matrix = False
        match = re.match(r"^\| (TC-[A-J]-\d{2}) \| ([^|]*)\|", line)
        if match and not in_matrix:
            if match.group(1) in defined:
                duplicates.append(match.group(1))
            defined[match.group(1)] = match.group(2).strip()
        elif in_matrix and line.startswith("|") and not line.startswith("|---") and "Requirement" not in line:
            cells = [c.strip() for c in line.strip("|").split("|")]
            if len(cells) >= 2:
                reqs = re.findall(REQ_ID, cells[0])
                tcs = expand_tags(cells[1])
                for req in reqs:
                    matrix.setdefault(req, set()).update(tcs)
                    if "Not planned" in cells[1] or "Not planned" in cells[0]:
                        matrix[req].add("NOT-PLANNED")
    return defined, matrix, duplicates


def read_test_code_tags():
    """Return {test case id: sorted list of 'file:line'} from C++ and Python test sources."""
    where = defaultdict(set)
    files = sorted((PROJECT / "tests").glob("*.cpp")) + sorted((PROJECT / "python" / "tests").glob("*.py"))
    for path in files:
        # the checker's own test file quotes tags as sample data; only its explicit "Traceability:" lines count
        explicit_only = path.name == "test_traceability_tool.py"
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if explicit_only and "Traceability:" not in line:
                continue
            for tc in expand_tags(line):
                where[tc].add(f"{path.relative_to(PROJECT)}:{number}")
    return {tc: sorted(places) for tc, places in where.items()}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check requirements -> test case -> test code traceability.")
    parser.add_argument("--write", help="write the generated matrix to this markdown file")
    args = parser.parse_args(argv)

    requirements = read_requirements()
    defined, matrix, duplicates = read_test_plan()
    tagged = read_test_code_tags()
    problems = []

    for req in requirements:
        mapped = matrix.get(req, set())
        if not mapped:
            problems.append(f"requirement {req} is missing from the traceability matrix")
        elif req not in NOT_PLANNED_REQS and not (mapped - {"NOT-PLANNED"}):
            problems.append(f"requirement {req} maps to no test case")
    for req in matrix:
        if req not in requirements:
            problems.append(f"the matrix names {req}, which the requirements specification does not define")
    for req, tcs in matrix.items():
        for tc in sorted(tcs - {"NOT-PLANNED"}):
            if tc not in defined:
                problems.append(f"the matrix maps {req} to {tc}, which the test plan does not define")
    for tc in duplicates:
        problems.append(f"test case {tc} is defined more than once")
    for tc in VERIFIED_ELSEWHERE:
        if tc not in defined:
            problems.append(f"the checker classifies {tc} as verified elsewhere, but the test plan does not define it")
    for tc in defined:
        if tc not in tagged and tc not in VERIFIED_ELSEWHERE:
            problems.append(f"test case {tc} is defined but no test code carries its tag (and it is not classified "
                            "as verified elsewhere)")
    for tc in tagged:
        if tc not in defined:
            problems.append(f"test code refers to {tc}, which the test plan does not define")

    lines = ["# Traceability matrix (generated)", "",
             "Generated by `project/tools/check_traceability.py` from the requirements specification, the test plan and the",
             "tags in the test code. Do not edit by hand; run the tool to regenerate.", "",
             "| Requirement | Test cases | Where implemented |", "|---|---|---|"]
    for req in requirements:
        tcs = sorted(matrix.get(req, set()) - {"NOT-PLANNED"})
        places = []
        for tc in tcs:
            if tc in tagged:
                files = sorted({p.split(":")[0] for p in tagged[tc]})
                places.append(f"{tc}: " + ", ".join(f"`{f}`" for f in files))
            elif tc in VERIFIED_ELSEWHERE:
                places.append(f"{tc}: {VERIFIED_ELSEWHERE[tc]}")
        if req in NOT_PLANNED_REQS and not tcs:
            places.append(f"not implemented ({NOT_PLANNED_REQS[req]})")
        lines.append(f"| {req} | {', '.join(tcs) if tcs else '—'} | {'<br>'.join(places) if places else '—'} |")
    implemented = sum(1 for tc in defined if tc in tagged)
    lines += ["", f"**{len(requirements)} requirements, {len(defined)} test cases defined: {implemented} carry a tag in "
              f"automated test code, {len([t for t in defined if t not in tagged])} are verified elsewhere "
              "(see the classification in the tool).**"]
    if args.write:
        Path(args.write).write_text("\n".join(lines) + "\n")

    for problem in problems:
        print("TRACEABILITY PROBLEM:", problem)
    print(f"{len(requirements)} requirements, {len(defined)} test cases, {implemented} tagged in test code, "
          f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
