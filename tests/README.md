# Testing the semantic model

A disagreement between two compiler snapshots can indicate a compiler defect, an interpreter defect, or an inaccurate model of the environment. We therefore test the interpreter and the validation pipeline at several levels. This follows the development approach described in [§5.2 of the Gauntlet paper](https://www.usenix.org/system/files/osdi20-ruffy.pdf#page=8): use compiler tests and reduced programs to challenge the semantic model as it grows.

## What each layer establishes

**Semantic unit tests.** Tests in [gtest](gtest) check individual language operations and their symbolic results. They isolate choices such as copy-in/copy-out, header validity, guarded state updates, and parser rejection. A failing test at this level can be diagnosed without identifying a compiler transformation first.

**Program-pair regressions.** Tests in [violated](violated) pair an `orig.p4` source with numbered transformed programs. These comparisons must detect the intended disagreement. They exercise the complete interpretation and comparison path while keeping the semantic difference small enough to inspect. [Undefined-value cases](undef_violated) exercise the distinction between a strict mismatch and one tolerated when undefined-behavior filtering is enabled.

**Compiler integration tests.** We validate P4C's sample programs and the local [false-friend cases](false_friends) through the compiler pipeline. These tests cover interactions between language features and transformations that isolated unit tests may miss. A sample's presence in the compiler's own suite is useful evidence, but not proof that it is compiled correctly.

**Driver tests.** [Pass-generation tests](gtest/passes.cpp) simulate a compiler's pass trace and dumps. This lets us check snapshot selection, ordering, fallback, and error handling independently of changes to the real compiler pipeline. [Comparator tests](gtest/compare.cpp) check the semantic queries separately.

**Independent packet tests.** For a reported mismatch, running a reduced program on a target provides evidence outside the symbolic interpreter. The [archived reproducers](https://github.com/p4gauntlet/gauntlet/tree/master/bugs/validation/parser-union) use P4C's normal packet-test runners. A passing packet test covers its concrete inputs; it does not prove equality for all symbolic inputs. A failing test must also be interpreted in light of the target's own behavior.

## Expected violations and expected test failures

A regression that detects a known semantic difference is a successful test. The `violated` driver requires the comparator to return a violation for each transformed program; it does not expect the two programs to agree.

An XFAIL has a different meaning: the test currently cannot meet its intended expectation. For example, a compiler sample that should validate may fail because of a known compiler defect. We record such cases individually rather than excluding an entire compiler pass. An unexpected success then fails CTest and prompts us to revisit the expectation.

The registrations and explanations live in [CMakeLists.txt](../CMakeLists.txt). `P4C_VALIDATION_XFAIL_TESTS` records sample failures, while the violation and undefined-value groups have their own expectations. The [reproducer index](https://github.com/p4gauntlet/gauntlet/tree/master/bugs/validation/parser-union) maps the sample XFAILs to their underlying cases and filed reports.

Timeouts are separate from expected semantic failures. CTest's `WILL_FAIL` does not invert a timeout, so known timeout cases are excluded from registration through `P4C_VALIDATION_TIMEOUT_TESTS`. Unsupported cases are also listed explicitly. Normal test runs use no artificial process memory limit.

## Interpreting coverage

Compiler integration tests use the validator's default checkpoint-first strategy. A checkpoint mismatch triggers detailed comparison of the compiler passes. This provides broad coverage at lower cost, but a passing run can miss an intermediate change that a later pass reverses. See the [validation guide](../validate/README.md) for the distinction between checkpoint checking and `--all-passes`.

Tests also inherit the interpreter's execution model. Parser cycles without a proven stack-progress bound use an eight-round fallback budget; remaining paths reject with `ParserTimeout` and a warning. Proven stack bounds are not truncated by that budget. General loops use exact summaries where supported and otherwise have a 1,000-iteration interpretation limit. Exceeding that limit is an unsupported-feature error, not a successful equivalence check.

These limits matter when interpreting a passing result. We test equality under the model actually constructed, including its environment abstractions and execution bounds. The [interpreter guide](../common/README.md) explains those choices.

## Running the tests

From the P4C build directory, with `ENABLE_GTESTS=ON`:

```sh
cmake --build . --target p4validate p4compare p4toz3 toz3_tests -j 2
ctest --output-on-failure -R '^toz3-unit$'
ctest --output-on-failure -j 2 -R '^toz3-validate'
```

For the full extension suite, including the [pruner](../pruner/README.md) when enabled:

```sh
ctest --output-on-failure -j 2 -R '^(toz3-|pruner)'
```

Program-level tests use [run_validation_test.py](../tools/run_validation_test.py). Sample validation normally enables undefined-behavior filtering; undefined-value regressions explicitly exercise both comparison modes. The pruner has its own fixed-seed regression driver.

## Focused semantic cases

- [Short extraction](violated/parser_short_extract_def_use/README.md): an assignment remains observable when a later extraction rejects.
- [Argument order](violated/parser_stack_argument_order/README.md): evaluating an output destination before a lookahead changes which error is observed.
- [Explicit stack indexing](violated/parser_index_short_packet/README.md): parser unrolling changes an extraction's error behavior.
- [Union lowering](union-lowering.md): validity, copying, and extraction require effects across related headers.
