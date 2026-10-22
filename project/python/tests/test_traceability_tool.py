"""Tests of the traceability checker itself, and of the repository against it.   (TC-H-07, VER-003)"""
import contextlib
import io
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent.parent / "tools"
sys.path.insert(0, str(TOOLS))
import check_traceability as tool  # noqa: E402


class TagParsing(unittest.TestCase):
    def test_shorthand_is_expanded(self):
        cases = {
            "TC-A-02/03": {"TC-A-02", "TC-A-03"},
            "TC-A-03, B-03, C-06": {"TC-A-03", "TC-B-03", "TC-C-06"},
            "TC-C-02, TC-C-03": {"TC-C-02", "TC-C-03"},
            "TC-D-01 to TC-D-05": {f"TC-D-0{i}" for i in range(1, 6)},
            "(TC-G-01 .. TC-G-07, EXP-001)": {f"TC-G-0{i}" for i in range(1, 8)},
            "TC-D-09 … TC-D-14": {f"TC-D-{i:02d}" for i in range(9, 15)},
        }
        for line, expected in cases.items():
            self.assertEqual(tool.expand_tags(line), expected, line)

    def test_digits_in_ordinary_prose_are_not_test_numbers(self):
        # these lines once produced phantom tags (numbers taken from '(REL-004)' and 'Eq. (21)')
        self.assertEqual(tool.expand_tags("TC-D-08 (REL-004)"), {"TC-D-08"})
        self.assertEqual(tool.expand_tags("TC-D-06 / Eq. (21) cross-check, TC-F-07 precision"), {"TC-D-06", "TC-F-07"})
        self.assertEqual(tool.expand_tags("TC-D-05 (ORC-8): the closed form at beta = 0.05, 0.2, 0.5"), {"TC-D-05"})
        self.assertEqual(tool.expand_tags("no tags here, just 12 and 34"), set())

    def test_a_reversed_range_is_not_expanded(self):
        self.assertEqual(tool.expand_tags("TC-D-05 to TC-D-02"), {"TC-D-05", "TC-D-02"})


class Repository(unittest.TestCase):
    # Traceability: TC-H-07
    def test_the_repository_has_no_traceability_problem(self):
        self.assertEqual(tool.main([]), 0)

    def test_the_checker_notices_a_missing_tag(self):
        original = tool.read_test_code_tags
        try:
            tool.read_test_code_tags = lambda: {k: v for k, v in original().items() if k != "TC-C-03"}
            with contextlib.redirect_stdout(io.StringIO()) as captured:  # the tool reports the problem on purpose
                self.assertEqual(tool.main([]), 1)
            self.assertIn("TC-C-03", captured.getvalue())
        finally:
            tool.read_test_code_tags = original


if __name__ == "__main__":
    unittest.main()
