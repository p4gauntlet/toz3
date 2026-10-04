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

The pass-generation tests use Python 3 to simulate compiler output.

Loop interpretation supports `break`, `continue`, and inclusive constant ranges.
Control results include a `$terminated` flag; output values are compared on
terminating paths. Simple additive loops use exact arithmetic summaries. Other
loops support up to 1,000 interpreted iterations and report an unsupported-feature
error beyond that limit. A repeated counter proves nontermination only when
condition and update inputs other than the counter stay unchanged, and the body
has no calls or early exits.
