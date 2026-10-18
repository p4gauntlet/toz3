# Reducing failure-inducing programs

The pruner searches for a smaller P4 program that still triggers a compiler failure or a translation-validation violation. We use it after finding a failure, when a large generated program makes the cause difficult to understand.

Reduction preserves an observed failure, not the program's normal behavior. A candidate may change packet processing substantially and still be useful if it reproduces the failure. This distinguishes the pruner's transformations from the compiler transformations checked by the validator.

## The failure oracle

We parse the program into P4C's IR, propose a simplification, emit the candidate as P4, and run an external failure check. We retain the candidate when its exit code and classified error type match the original failure; otherwise, we keep the previous program.

```text
current program → candidate reduction → compiler or validator
       ↑                                      |
       └──────── accept if failure matches ────┘
```

For compiler failures, classification examines the diagnostic text, including the `Compiler Bug` marker. For validation failures, the expected exit code is `20`. The oracle does not require an identical diagnostic, the same failing compiler pass, or the same counterexample. Consequently, preserving the failure class is not proof that a reduction has preserved its original cause. The reduced program still needs inspection.

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
