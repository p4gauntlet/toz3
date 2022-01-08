#!/usr/bin/env python3
import argparse
import pathlib
import sys
import subprocess
import logging
import shutil

log = logging.getLogger(__name__)

SEED = 1

FILE_DIR = pathlib.Path(__file__).parent.resolve()
REFERENCE_DIR = FILE_DIR.joinpath("references/")


EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def exec_process(cmd, *args, silent=False, **kwargs):
    log.debug("Executing %s ", cmd)
    result = subprocess.run(cmd.split(), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, *args, **kwargs)
    if result.stdout:
        log.debug("Process output: %s", result.stdout.decode("utf-8"))
    if result.returncode != EXIT_SUCCESS and not silent:
        log.error("BEGIN %s", 40 * "#")
        log.error("Failed while executing:\n%s\n", cmd)
        log.error("Output:\n%s", result.stderr.decode("utf-8"))
        log.error("END %s", 40 * "#")

    return result


def main(args):

    compiler_bin = args.compiler.absolute()
    validation_bin = args.validation
    pruner_bin = args.pruner_path.absolute()
    p4_prog = args.p4prog.absolute()

    if not shutil.which(compiler_bin):
        log.error("Please provide the path to a valid compiler binary")
        return EXIT_FAILURE

    if validation_bin and not validation_bin.exists():
        validation_bin = validation_bin.absolute()
        log.error("Please provide the path to a valid validation binary")
        return EXIT_FAILURE

    if not shutil.which(pruner_bin):
        log.error("Please provide a valid path to the pruner")
        return EXIT_FAILURE

    if not p4_prog.is_file():
        log.error("Please provide the path to a valid p4 program")
        return EXIT_FAILURE

    prog_dir = p4_prog.parent.joinpath(f"pruned_{p4_prog.stem}")

    cmd_args = f"{pruner_bin} --seed {SEED} "
    cmd_args += f"--compiler-bin {compiler_bin} --bug-type {args.type} "
    cmd_args += f"--working-dir {prog_dir} {p4_prog} "

    if validation_bin:
        cmd_args += f"--validation-bin {validation_bin} "

    pruner_result = exec_process(cmd_args)

    if pruner_result.returncode == EXIT_FAILURE:
        return EXIT_FAILURE

    PRUNED_FILE = pathlib.PosixPath(
        ".".join(str(p4_prog).split('.')[:-1]) + '_stripped.p4')

    FILE_NAME = p4_prog.parts[-1]

    ref_file = ".".join(
        str(FILE_NAME).split('.')[:-1]) + '_reference.p4'

    ref_folder = 'validation_bugs' if args.type == 'VALIDATION' else 'crash_bugs'

    REFERENCE_FILE = REFERENCE_DIR.joinpath(f"{ref_folder}/{ref_file}")

    if REFERENCE_FILE.is_file():
        result = exec_process(f"diff {PRUNED_FILE} {REFERENCE_FILE}").returncode
        if result == EXIT_FAILURE:
            log.error("Test failed")
        else:
            log.info("Test passed")
        PRUNED_FILE.unlink()
    else:
        log.error("Reference file not found")
        return EXIT_FAILURE
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="This file takes a p4 file along with a p4c binary with bugs, and a validation binary, passes it to the pruner and then compares it to a reference file to test the working of the pruner.")
    parser.add_argument(
        "-c", "--compiler", dest='compiler', help="The path to the compiler binary", required=True, type=pathlib.Path)
    parser.add_argument(
        "-v", "--validation", dest='validation', help="The path to the validation binary", type=pathlib.Path)

    parser.add_argument(
        "-p4", "--p4prog", dest="p4prog", help="The path to the p4 program", required=True, type=pathlib.Path)
    parser.add_argument(
        "-p", "--pruner_path", dest='pruner_path', help="The path to the pruner", required=True, type=pathlib.Path)
    parser.add_argument(
        "-t", "--type", dest='type', help="Validation or Crash bug [VALIDATION/CRASH]", required=True, choices=['VALIDATION', 'CRASH'])

    parser.add_argument("-l", "--log_file", dest="log_file",
                        default="pruner_test.log", help="Specifies name of the log file.")

    parser.add_argument("-ll", "--log_level", dest="log_level", default="INFO",
                        choices=["CRITICAL", "ERROR", "WARNING",
                                 "INFO", "DEBUG", "NOTSET"],
                        help="The log level to choose.")
    args = parser.parse_args()

    logging.basicConfig(filename=args.log_file,
                        format="%(levelname)s:%(message)s",
                        level=getattr(logging, args.log_level),
                        filemode='w')
    stderr_log = logging.StreamHandler()
    stderr_log.setFormatter(logging.Formatter("%(levelname)s:%(message)s"))
    logging.getLogger().addHandler(stderr_log)

    sys.exit(main(args))
