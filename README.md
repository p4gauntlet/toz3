# ToZ3
ToZ3 is a tool that produces Z3 expressions from [P4-16](https://p4.org/p4-spec/docs/P4-16-v1.2.0) programs.

## Requirements
ToZ3 is written as an extension to [P4C](https://github.com/p4lang/p4c) and uses Z3. Its CMake configuration obtains Z3 through P4C's dependency helper. First set up a P4C source tree and its build dependencies using the [P4C installation instructions](https://github.com/p4lang/p4c#dependencies).

## Install
Once the P4C source tree and build directory are ready, ToZ3 can be added as an extension. The following commands provide an example:

    cd ~/p4c/
    mkdir extensions
    cd extensions
    git clone https://github.com/p4gauntlet/toz3
    cd ~/p4c/build
    cmake .. -DENABLE_TOZ3=ON
    cmake --build . --target p4toz3 p4compare p4validate
Afterwards, the ToZ3 executables can be found in `p4c/build/extensions/toz3/`. ToZ3 can be installed globally by running `sudo cmake --install .` from the build directory.

## Usage
Generating Z3 from P4-16 with ToZ3 is straightforward. The tool currently supports three different modes.

Run these commands from the P4C build directory:

```sh
# Interpret a P4-16 program and print its symbolic output state.
./extensions/toz3/p4toz3 program.p4

# Compare the output states of two or more programs in the supplied order.
./extensions/toz3/p4compare before.p4,after.p4

# Dump compiler passes and compare successive distinct programs.
./extensions/toz3/p4validate --compiler-bin ./p4test --dump-dir validated program.p4
```

Use `--allow-undefined` with `p4compare` or `p4validate` to tolerate differences caused by undefined behavior. Exit codes are `0` for success, `1` for an error, `10` when there is nothing to validate, and `20` for a detected violation. Each executable supports `--help`.

When P4C is configured with `ENABLE_GTESTS=ON`, build and run the focused regression tests with:

```sh
cmake --build . --target toz3_tests
ctest --output-on-failure -R '^toz3-unit$'
```

Run the full extension suite, including pruner tests when enabled, with two
workers and the 3 GiB process limit used to audit the memory-failure XFAILs:

```sh
(
  ulimit -v 3145728
  ctest --output-on-failure -j 2 -R '^(toz3-|pruner)'
)
```

The pass-generation tests use Python 3 to simulate compiler output.

Loop interpretation supports `break`, `continue`, and inclusive constant ranges.
Control results include a `$terminated` flag; output values are compared on
terminating paths. Simple additive loops use exact arithmetic summaries. Other
loops support up to 1,000 interpreted iterations and report an unsupported-feature
error beyond that limit. A repeated counter proves nontermination only when
condition and update inputs other than the counter stay unchanged, and the body
has no calls or early exits.

Parser outputs are validated independently of the other architecture blocks.
Their summaries include extracted fields, acceptance, parser error, and packet
cursor, including the state preserved when parsing rejects. Packet extraction,
lookahead, advance, length, and `verify` have explicit effect models.

Parser paths merge with guarded `If` expressions and combined reachability.
Acyclic joins run once per unfolding round; back edges start another round.
When every cycle unconditionally extracts a stack's `next` element and no operation
can reset a stack, analysis derives a bound from the combined stack capacities.
These cycles avoid solver queries on each back edge. Conditional extraction,
stack replacement, `pop_front`, and unknown calls prevent this proof; an extraction
elsewhere in the same cycle component is insufficient if a cycle can bypass it.
Other cycles retain the 1,000-round limit, and all parsers retain a 10,000-state
execution limit. Remaining paths produce an unsupported-feature error. Cycles
that consume packet data without advancing stacks can terminate on a finite
packet, but may still require loop summaries beyond these limits.

Packet reads are abstracted by packet identity, position, and width. Equal-width
lookahead and extraction share data, but different widths are not generally
constrained as views of the same bytes. Variable-width extraction includes its
requested size in the read model; full runtime `varbit` length semantics remain
incomplete. `FlattenHeaderUnion` remains excluded from pass comparisons.

The [short-packet regression](tests/violated/parser_short_extract_def_use/README.md)
documents the `SimplifyDefUse` compiler defect detected in `invalid-hdr-warnings4`.
The [argument-order regression](tests/violated/parser_stack_argument_order/README.md)
documents the `SideEffectOrdering` compiler defect detected in `issue2090` and
`spec-ex19`. Both reports include concrete packets and before/after parser outputs
verified at P4C revision `3d9a20e985`. These are expected violations in the reduced
comparison tests; the original P4C sample validation tests still report them.
The sample tests are registered as XFAILs, together with unsupported extern
arrays, parser unfolding limits, and known memory failures. Programs that time
out are excluded separately in `P4C_VALIDATION_TIMEOUT_TESTS`; its comments
distinguish interpretation from Z3 comparison. XFAILs continue to run, and an
unexpected success fails CTest so the expectation can be removed.
