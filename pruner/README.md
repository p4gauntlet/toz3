# Reducing failure-inducing programs

The pruner searches for a smaller P4 program that still triggers a compiler failure or a translation-validation violation. We use it after finding a failure, when a large generated program makes the cause difficult to understand.

Reduction preserves an observed failure, not the program's normal behavior. A candidate may change packet processing substantially and still be useful if it reproduces the failure. This distinguishes the pruner's transformations from the compiler transformations checked by the validator.

## The failure oracle

We parse the program into P4C's IR, propose a simplification, emit the candidate as P4, and run an external interestingness test. We follow the test-driven reduction approach used by [C-Reduce](https://manpages.ubuntu.com/manpages/xenial/man1/creduce.1.html): the reducer proposes smaller programs, while the test defines the property that must survive. A candidate is retained only if the test accepts it; otherwise, we keep the previous program.

```text
current program → candidate reduction → interestingness test
        ↑                                      |
        └──────── accept if interesting ────────┘
```

With `--interestingness-test`, an executable program, usually a shell script, supplies this test. Exit status **0** means interesting; any nonzero status or termination by a signal means uninteresting. The original input must pass the test before reduction starts. The script can check a particular compiler diagnostic, a validation violation, or any other property of the candidate.

The invocation is:

```text
test-script [--test-arg values, in order] /absolute/path/to/candidate.p4
```

The script runs in the directory from which the pruner was invoked. The candidate's absolute path is always the last argument; each repeated `--test-arg` supplies one preceding argument. Arguments are passed literally, without shell expansion by the pruner. Treat the candidate as read-only. Tests run sequentially, and the pruner waits for each result without imposing a timeout. A test should be deterministic and place a timeout on commands that may hang.

The built-in `--bug-type` and `--config` modes instead compare the candidate's exit code and classified error type with the original failure. For compiler failures, classification examines the diagnostic text, including the `Compiler Bug` marker. For validation failures, the expected exit code is `20`. These modes do not require an identical diagnostic, the same failing compiler pass, or the same counterexample. A script can make the acceptance condition more specific. `--interestingness-test` cannot be combined with `--bug-type` or `--config`.

## Searching from coarse to fine

We first remove groups of statements. The search chooses statements randomly and adjusts the number of removals using additive increase and multiplicative decrease: successful attempts encourage larger reductions, while unsuccessful attempts reduce the batch size. This makes large reductions possible without testing every statement independently from the start.

We then simplify expressions and replace Boolean expressions with literals. These finer changes can preserve a failure after statement removal stops making progress. The search uses iteration and unsuccessful-attempt limits; it does not establish that the result is globally minimal or even minimal under every available transformation.

A final stage uses compiler analyses to replace variables with constants and remove unused declarations. Constant replacement breaks dependencies that would otherwise keep declarations alive. These stages need resolved references and inferred types, so they run only if the preparatory compiler passes preserve the failure according to the oracle.

This ordering is deliberate. Early reductions work directly on syntax and can retain failures that occur before type checking. Later reductions use richer compiler information when it is available. Unlike the symbolic interpreter, the pruner can use compiler transformations as candidate simplifications because every candidate is checked again by the external oracle.

## Reproducibility

The random seed determines the sequence of candidate choices. Supplying a seed allows a reduction to be repeated with the same program and toolchain. Different compiler revisions or diagnostics can still change which candidates the oracle accepts.

Regression tests use a fixed seed and a compiler binary that retains known failures. They compare the emitted program with a reference reduction. Each test has its own working directory, so parallel tests do not modify each other's candidates or the source tree.

## Usage

Enable `ENABLE_GAUNTLET_PRUNER` in the P4C CMake configuration and build the `p4pruner` target. From the P4C build directory, a compiler-failure reduction can be run as follows:

```sh
./extensions/toz3/pruner/p4pruner \
  --compiler-bin ./p4test --bug-type CRASH --seed 1 \
  --working-dir /tmp/opencode/pruner-work \
  --output reduced.p4 input.p4
```

The working directory must not already exist. It holds intermediate candidates and is removed after reduction. `--output` selects the final program; without it, the pruner uses the input name with a `_stripped.p4` suffix.

### Script-based reduction

For example, save the following as `interesting.sh` to retain a compiler failure with a particular diagnostic. It takes the compiler path and diagnostic text before the candidate path, and uses GNU `timeout` to bound each compiler run:

```sh
#!/bin/sh
compiler=$1
diagnostic=$2
candidate=$3
log=$(mktemp) || exit 1
trap 'rm -f "$log"' EXIT

status=0
timeout --kill-after=5s 30s "$compiler" "$candidate" >"$log" 2>&1 || status=$?
case "$status" in
    0|124|125|126|127|137) exit 1 ;;
esac
grep -Fq -- "$diagnostic" "$log"
```

Replace `Compiler Bug: your diagnostic` below with distinctive text from the failure you want to preserve. Make the script executable and check it on the original input from the P4C build directory:

```sh
chmod +x interesting.sh
./interesting.sh "$PWD/p4test" 'Compiler Bug: your diagnostic' "$PWD/input.p4"
echo $?
```

The result must be `0`. Then run the pruner with the same test and arguments:

```sh
./extensions/toz3/pruner/p4pruner \
  --interestingness-test ./interesting.sh \
  --test-arg "$PWD/p4test" --test-arg 'Compiler Bug: your diagnostic' \
  --seed 1 --working-dir /tmp/opencode/pruner-work \
  --output reduced.p4 input.p4
```

The script determines interestingness; the pruner does not interpret its diagnostic output. In this example, successful compilations, timeouts, and command-launch failures are rejected, and a failing compilation is accepted only when its output contains the selected diagnostic.

### Validation integration

The pruner's `VALIDATION` mode still expects the legacy Python validation-driver interface, not the current C++ `p4validate` command. The driver must accept `-i`, `-p`, `-o`, `-ll`, and `-u`; configuration discovery also invokes it with `-d` and reads the resulting `<program>_info.json` file.

```sh
./extensions/toz3/pruner/p4pruner \
  --compiler-bin ./p4test --bug-type VALIDATION \
  --validation-bin /path/to/legacy-validation-driver.py \
  --working-dir /tmp/opencode/pruner-work input.p4
```

A recorded configuration can instead be supplied with `--config failure.json`. The fields read by the current implementation are:

```json
{
  "compiler": "/path/to/p4test",
  "validation_bin": "/path/to/legacy-validation-driver.py",
  "exit_code": 20,
  "allow_undef": true,
  "err_string": "",
  "prog_before": "/path/to/before.p4",
  "prog_after": "/path/to/after.p4"
}
```

The recorded before/after paths do not pin the oracle to a particular failing pass. Candidate validation still uses the configured driver and failure classification.

## Implementation map

| Responsibility | Main implementation |
| --- | --- |
| Stage ordering and configuration | [main.cpp](src/main.cpp), [pruner_options.cpp](src/pruner_options.cpp) |
| Failure classification and candidate acceptance | [pruner_util.cpp](src/pruner_util.cpp) |
| Syntax-directed reductions | [statement_pruner.cpp](src/statement_pruner.cpp), [expression_pruner.cpp](src/expression_pruner.cpp), [boolean_pruner.cpp](src/boolean_pruner.cpp) |
| Compiler-assisted reductions | [compiler_pruner.cpp](src/compiler_pruner.cpp), [replace_variables.cpp](src/replace_variables.cpp), [extended_unused.cpp](src/extended_unused.cpp) |
| Repeatable regression checks | [check_prog.py](tests/check_prog.py) |

Run the registered regressions from the P4C build directory with `ctest -R '^pruner' --output-on-failure`.
