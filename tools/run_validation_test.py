#!/usr/bin/env python3
# Copyright 2013-present Barefoot Networks, Inc.
# Copyright 2018 VMware, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" Contains different eBPF models and specifies their individual behavior
    Currently five phases are defined:
   1. Invokes the specified compiler on a provided p4 file.
   2. Parses an stf file and generates an pcap output.
   3. Loads the generated template or compiles it to a runnable binary.
   4. Feeds the generated pcap test packets into the P4 "filter"
   5. Evaluates the output with the expected result from the .stf file
"""

import sys
import warnings
import tempfile
from pathlib import Path
import argparse
import util


# Append tools to the import path.
FILE_DIR = Path(__file__).resolve().parent


class Options():
    def __init__(self):
        self.rootdir = ""               # Path to the compiler root tree.
        self.builddir = ""              # Path to the build directory.
        self.binary = ""                # This program's name.
        self.cleanupTmp = True          # Remove tmp folder?
        self.compiler_bin = ""          # Path to the P4 compiler binary.
        self.validation_bin = ""        # Path to the P4 compiler binary.
        self.p4_file = ""               # File that is being compiled.
        self.ignore_crashes = False     # Treat crashes as benign.
        self.check_violation = False    # Test must produce a violation.
        self.check_undefined = False    # Test must be sensitive to undefined behavior.
        self.disallow_undefined = False # Undefined violations must be detected.
        self.verbose = False            # Enable verbose output.


def run_validation_test(options, target_dir, allow_undefined):
    cmd = "%s " % options.validation_bin
    cmd += "--dump-dir %s " % target_dir
    cmd += "--compiler-bin %s " % options.compiler_bin
    if allow_undefined:
        cmd += "--allow-undefined "
    cmd += "%s " % options.p4_file
    result = util.exec_process(cmd)
    if options.verbose:
        print("Validation output:\n%s" % result.stdout.decode())
        print("Validation error output:\n%s" % result.stderr.decode())
    if result.returncode not in [util.EXIT_SUCCESS, util.EXIT_UNDEF, util.EXIT_VIOLATION] and options.ignore_crashes:
        msg = "Ignored crash in %s" % options.p4_file
        warnings.warn(msg)
        return util.EXIT_SUCCESS
    if result.returncode == util.EXIT_UNDEF:
        if allow_undefined:
            msg = "Ignored undefined behavior in %s" % options.p4_file
            warnings.warn(msg)
            return util.EXIT_SUCCESS
        else:
            return util.EXIT_VIOLATION
    if result.returncode == util.EXIT_SKIPPED:
        print("Skipping file %s." % options.p4_file)
        return util.EXIT_SUCCESS
    return result.returncode


def run_violation_test(options, allow_undefined):
    src_p4_file = options.p4_file.joinpath("orig.p4")
    for p4_file in list(options.p4_file.glob("**/[0-9]*.p4")):
        cmd = "%s %s,%s " % (options.validation_bin, src_p4_file, p4_file)
        if allow_undefined:
            cmd += "--allow-undefined "
        result = util.exec_process(cmd).returncode
        if result != util.EXIT_VIOLATION:
            return util.EXIT_FAILURE
    return util.EXIT_SUCCESS

def run_undef_test(options):
    if run_violation_test(options, False) != util.EXIT_SUCCESS:
        return util.EXIT_FAILURE
    if run_violation_test(options, True) != util.EXIT_FAILURE:
        return util.EXIT_FAILURE
    return util.EXIT_SUCCESS

def run_test(options, argv):
    if (options.check_violation):
        return run_violation_test(options, not options.disallow_undefined)

    if (options.check_undefined):
        return run_undef_test(options)

    tmpdir = Path(tempfile.mkdtemp(dir=str(options.builddir)))

    if options.verbose:
        print("Writing temporary files into ", tmpdir)

    result = run_validation_test(options, tmpdir, not options.disallow_undefined)

    if options.cleanupTmp:
        if options.verbose:
            print("Removing", tmpdir)
        util.rm_tree(tmpdir)

    return result


if __name__ == '__main__':
    """ main """
    # Parse options and process argv
    parser = argparse.ArgumentParser()
    parser.add_argument("rootdir", help="the root directory of "
                        "the compiler source tree")
    parser.add_argument("p4_file", help="the p4 file to process")
    parser.add_argument("-d", "--nocleanup", action="store_false",
                        help="do not remove temporary results for failing tests")
    parser.add_argument("-vb", "--validation-bin", dest="validation_bin", required=True,
                        help="Specify the path to the validation binary, ")
    parser.add_argument("-bd", "--build-dir", dest="builddir", required=True,
                        help="Specify the path to the build directory.")
    parser.add_argument("-c", "--compiler-bin", dest="compiler", required=True,
                        help="Specify the path to the compiler binary, "
                        "default is p4validate")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="verbose operation")
    parser.add_argument("-ic", "--ignore-crashes", action="store_true",
                        help="Ignore any crashes that may happen.")
    parser.add_argument("-da", "--disallow-undefined", action="store_true",
                        help="If active, undefined violations must be detected.")
    parser.add_argument("-cv", "--check-violation", action="store_true",
                        help="If active, the test must produce a violation error. Also p4_file must be an input folder.")
    parser.add_argument("-cu", "--check-undefined", action="store_true",
                        help="If active, the test must not produce a violation error if allow-undefined is true. Otherwise, it must throw an error.")
    args, argv = parser.parse_known_args()
    options = Options()
    options.rootdir = util.is_valid_file(parser, args.rootdir)
    options.builddir = util.is_valid_file(parser, args.builddir)
    options.compiler_bin = util.is_valid_file(parser, args.compiler)
    options.validation_bin = util.is_valid_file(parser, args.validation_bin)
    options.p4_file = util.is_valid_file(parser, args.p4_file)
    options.ignore_crashes = args.ignore_crashes
    options.check_violation = args.check_violation
    options.check_undefined = args.check_undefined
    options.disallow_undefined = args.disallow_undefined
    options.verbose = args.verbose
    options.cleanupTmp = args.nocleanup

    # All args after '--' are intended for the p4 compiler
    argv = argv[1:]
    # Run the test with the extracted options and modified argv
    result = run_test(options, argv)
    sys.exit(result)
