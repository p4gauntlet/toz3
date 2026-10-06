#!/usr/bin/env python3
"""Check the pruner test driver's exit handling and output isolation."""

import argparse
import pathlib
import tempfile
import unittest
from unittest import mock

import check_prog


class CheckProgramTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="pruner-driver-")
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name) / "paths with spaces"
        self.root.mkdir()
        self.source = self.root / "source"
        self.source.mkdir()
        self.program = self.source / "program.p4"
        self.program.write_text("expected program\n")
        references = self.root / "references"
        (references / "crash_bugs").mkdir(parents=True)
        self.reference = references / "crash_bugs" / "program_reference.p4"
        self.reference.write_text(self.program.read_text())
        patch = mock.patch.object(check_prog, "REFERENCE_DIR", references)
        patch.start()
        self.addCleanup(patch.stop)
        self.pruner = self.root / "mock pruner"
        self.args = argparse.Namespace(
            compiler=pathlib.Path("/bin/true"),
            validation=None,
            pruner_path=self.pruner,
            p4prog=self.program,
            type="CRASH",
        )

    def write_pruner(self, exit_code=0, emit=True):
        self.pruner.write_text(
            "#!/usr/bin/env python3\n"
            "import pathlib, sys\n"
            "args = sys.argv[1:]\n"
            "work = pathlib.Path(args[args.index('--working-dir') + 1])\n"
            "work.mkdir()\n"
            "output = pathlib.Path(args[args.index('--output') + 1])\n"
            "program = pathlib.Path(args[-1])\n"
            + ("output.write_text(program.read_text())\n" if emit else "")
            + f"sys.exit({exit_code})\n"
        )
        self.pruner.chmod(0o700)

    def test_successful_output_is_isolated_and_paths_keep_spaces(self):
        self.write_pruner()
        self.assertEqual(check_prog.main(self.args), check_prog.EXIT_SUCCESS)
        self.assertEqual(list(self.source.iterdir()), [self.program])

    def test_nonzero_exit_is_failure_even_with_matching_output(self):
        self.write_pruner(exit_code=2)
        self.assertEqual(check_prog.main(self.args), check_prog.EXIT_FAILURE)

    def test_missing_output_is_failure(self):
        self.write_pruner(emit=False)
        self.assertEqual(check_prog.main(self.args), check_prog.EXIT_FAILURE)

    def test_different_output_is_failure(self):
        self.write_pruner()
        self.reference.write_text("different program\n")
        self.assertEqual(check_prog.main(self.args), check_prog.EXIT_FAILURE)

    def test_missing_reference_is_failure(self):
        self.write_pruner()
        self.reference.unlink()
        self.assertEqual(check_prog.main(self.args), check_prog.EXIT_FAILURE)


if __name__ == "__main__":
    unittest.main()
