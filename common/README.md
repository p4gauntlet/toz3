# Interpreter architecture

To check whether a compiler pass preserves a P4 program's behavior, we interpret the program before and after the pass and compare their outputs. We represent inputs symbolically so that the comparison covers many executions at once. The solver looks for an input on which the two programs disagree.

Our interpreter constructs a **functional representation** of each programmable block: a tuple of expressions that gives the block's outputs in terms of its inputs and modeled environment. This follows the terminology of the [Gauntlet paper, §5.2 and Figure 3](https://www.usenix.org/system/files/osdi20-ruffy.pdf#page=8). We construct this representation by **symbolic execution with state merging**. Branches become if-then-else (`ite`) expressions, and assignments update the expressions associated with program variables.

## Representing programs in functional form

The interpreter maintains a symbolic store that maps program locations to expressions. For example, consider the following fragment:

```p4
if (choose) {
    x = x + 1;
} else {
    x = 0;
}
y = x + 2;
```

We interpret both branches from the same incoming state and select between their results using the branch condition:

```text
x_out = ite(choose_in, x_in + 1, 0)
y_out = ite(choose_in, x_in + 1, 0) + 2
```

Arithmetic follows the P4 type. For a fixed-width value, addition is bit-vector addition and includes overflow. After the branches merge, we interpret the assignment to `y` once using the merged value of `x`. We do not need to repeat the common continuation for every preceding execution path.

This technique moves control-flow choices into the expressions that describe program data. The expressions form a directed acyclic graph (DAG), so repeated subexpressions can share the same representation. State merging reduces repeated interpretation, but it does not eliminate the cost of representing many distinct effects.

The paper describes the result as a single nested if-then-else expression whose branches return output tuples. The current interpreter also merges individual fields. These two forms express the same selection:

```text
ite(c, (a, b), (d, e)) = (ite(c, a, d), ite(c, b, e))
```

Thus, **ite-based symbolic summary** describes the result, while **symbolic execution with state merging** describes how we construct it. We use *functional representation* for the overall model.

## Comparing observable outputs

Each programmable block of the architecture instantiated by `main` has an independent summary. We compare corresponding blocks rather than simulating an entire target pipeline. This is also the boundary used in the paper: for example, the parser and ingress control have separate input–output semantics.

This choice lets us check transformations without modeling a target's scheduling or runtime. It also means that a block is checked over its admissible symbolic inputs, not just the states reachable from a particular preceding pipeline stage. A difference remains visible even if a later block would discard it.

The summaries expose parameter values and header validity. For the parser, we also record acceptance, the parser error, and the packet cursor. We record termination separately and mask output values on nonterminating paths. These observations determine which changes the validator can detect.

Let `F_P(I, E)` denote the output tuple for program `P`, where `I` contains its inputs and `E` describes the modeled environment. Let `A(I, E)` constrain the admissible inputs. The basic comparison asks whether the following formula is satisfiable:

```text
A(I, E) ∧ F_before(I, E) ≠ F_after(I, E)
```

A satisfying assignment identifies a disagreement under the model. An unsatisfiable query establishes equality for the modeled observations and assumptions. It does not establish correctness of the interpreter or of environment behavior that we have abstracted away.

The output tuple is an SMT **term**: it denotes a value. The inequality above is an SMT **formula**: it denotes a proposition. The paper often calls both “Z3 formulas.” This is understandable shorthand, but distinguishing the two makes the representation clearer. Our queries are quantifier-free; symbolic inputs are free constants whose values the solver chooses. Quantifier-free does not mean that every query is inexpensive.

One can also write an input–output relation `A(I, E) ∧ O = F_P(I, E)`. Calling this a *symbolic transition relation* is reasonable if one block invocation is a transition. However, the interpreter directly constructs output terms, so *functional representation* is more specific and closer to the paper's description.

## Preserving structure and validity

We retain P4's data structures during interpretation. Scalar values contain SMT terms. Structs, headers, unions, and stacks organize these values and implement the operations defined for their types. We decompose aggregates into scalar observations when constructing their output summaries.

A header has both field values and a validity bit. We represent these separately because modifying data does not necessarily change validity. A header union also constrains its members: at most one member may be valid. Consequently, an operation on one member can affect the entire union.

For example, making a union member valid invalidates its siblings. Under the specification's [union-operation rules](https://github.com/p4lang/p4-spec/blob/main/p4-16/spec/P4-16-spec.adoc#sec-expr-hu), invalidating a member clears the union. Copying a whole union transfers its state, while assigning to an individual member applies that member's validity effects. Writing a field of an invalid union member can make data unspecified without changing validity. Treating a union as a collection of unrelated headers would miss these distinctions.

We distinguish executing an operation from merging its results. A merge selects existing field and validity values; it does not invoke validity methods again. Otherwise, merging two states could introduce an effect that neither branch performed. A write through a symbolic stack index similarly guards all affected values, including the siblings of a selected union member.

Input constraints must survive compiler transformations. For instance, flattening a union into separate headers does not make previously illegal input combinations admissible. We retain the source's input constraints during comparison and identify header-input validity by position within a parameter rather than by compiler-generated member names.

## Preserving evaluation order

P4 expressions may change state as well as produce values. We therefore save both argument values and copy-out destinations in source order. We then execute the callee with local bindings and copy its results to the saved destinations.

Saving a destination matters when its receiver or index has side effects. Re-evaluating it after the call could select a different location or repeat an effect. We evaluate it once even if a symbolic index represents several possible locations. Calls through extern arrays follow the same rule.

Return, exit, break, continue, and parser rejection also affect which statements execute. We retain the conditions under which these events occur and preserve their states separately from paths that continue. A traversal flag stops interpretation when the entire current branch has completed; symbolic guards handle completion on only some inputs.

In particular, rejection preserves the state at the failure point. A failed extraction does not assign its output, but earlier successful extractions and assignments remain observable. Recording only an error flag on the final normal state would lose these effects.

## Handling undefined values

An unknown input is a legitimate value that the solver may choose. An undefined value instead identifies data for which the source program does not prescribe a result. Reading an invalid header's field is one example. We represent undefined data symbolically and mask exported fields according to their header's validity.

When undefined-behavior filtering is enabled, the comparator computes a condition under which each source output is defined. This condition depends on the branch selected by an `ite`:

```text
defined(ite(c, a, b)) = defined(c) ∧ ite(c, defined(a), defined(b))
```

An undefined read in one branch therefore does not invalidate an observation made through the other branch. For output `j`, we look for a disagreement satisfying:

```text
admissible_inputs ∧ defined(source_j) ∧ source_j ≠ target_j
```

This check is source-directed: the target must preserve an observation where the source defines it. We do not discard all outputs merely because one field is undefined. This differs from the taint-substitution procedure described in [§5.2 of the paper](https://www.usenix.org/system/files/osdi20-ruffy.pdf#page=9). The current implementation computes guarded definedness instead of replacing tainted expressions with concrete values. It remains a check over the selected observations, rather than a general relational model of unspecified behavior.

## Modeling packets, tables, and externs

**Packets.** We represent packet contents using uninterpreted read functions indexed by packet identity, position, and width. Equal-width lookahead and extraction at the same position therefore observe the same data. We model length checks, cursor movement, extraction validity, and rejection explicitly. This avoids materializing a packet byte array, but reads of different widths are not generally constrained to be views of the same bytes. Runtime `varbit` lengths are also only partially modeled.

**Tables.** Table contents are not generally known at compile time. Following the paper's approach, we use symbolic match values, action choices, and control-plane arguments. We interpret the selected actions and merge their effects under the corresponding conditions. This lets us compare table-related transformations without selecting a concrete control-plane configuration or representing every entry in a target's table.

**Externs.** For externs without explicit effect models, we use symbolic return and output values. This follows the paper's abstraction of an extern as a call returning an arbitrary value. The current implementation names these values by method and output parameter; it does not generally distinguish repeated calls or model receiver-specific mutable state. Extern arrays use the same abstraction. This is not an uninterpreted function of all call arguments, nor does it implement the paper's description of a new free variable for every call.

These environment models determine which relationships between observations we preserve. A validation result is relative to those choices. For example, the packet-read model preserves consistency between identical reads, while the general extern model does not describe how a register's contents evolve across calls.

## Obtaining finite summaries for cycles

The paper motivates translation validation using P4's finite-state, finite-time character. In the interpreter, we must still construct a finite expression for every supported execution. The current implementation makes this obligation explicit for loops and the parser.

For ordinary loops, we use exact arithmetic summaries for supported additive patterns. Otherwise, we interpret iterations up to an implementation limit and report an unsupported feature if the limit is exceeded. We recognize nontermination only when restricted conditions make a repeated state sufficient evidence; seeing the same counter value alone is not enough.

The parser uses a worklist. Forward paths merge before executing a common continuation, and back edges schedule another unfolding round. When every cycle consumes a finite header-stack slot and no operation can restore that capacity, remaining stack capacity provides a progress measure. This is a ranking-function argument that justifies an unfolding bound. An extraction somewhere in a cyclic component is insufficient if another cycle can bypass it.

Without such a proof, we use a bounded execution model. Paths that exceed the fallback budget reject with `ParserTimeout`, preserve their state, and produce a warning. Proven stack bounds are not truncated by this fallback. Thus, a proved bound covers the modeled parser's executions, while the fallback checks behavior under a specified execution budget. The paper's finiteness argument should not be read as a claim that the current interpreter constructs exact, unbounded summaries for arbitrary cycles.

## Interpreting programs before and after lowering

Like the interpreter described in the paper, we interpret P4C's IR without first running the compiler's normal lowering passes. Those passes may contain the defect we want to detect. Applying them before interpretation could remove the difference between the source and transformed programs.

This requires us to resolve declarations and types during interpretation. Generic calls use call-local inference: match parameter and argument types, solve for the declaration's type variables, and specialize the callee. We follow the local approach described by Doenges et al. in *Petr4*. Type inference is separate from effectful evaluation, so determining an argument's type does not execute it again. Constructors and architecture instantiation use the same principle.

The goal is a common semantic model for several IR levels. High-level operations and their lowered forms should agree on their symbolic outputs, even when their statements and data structures differ.

## Implementation map

| Responsibility | Main implementation |
| --- | --- |
| Architecture instantiation and output summaries | [create_z3.cpp](create_z3.cpp) |
| Symbolic store, calls, and statement effects | [state.cpp](state.cpp), [visitor_interpret.cpp](visitor_interpret.cpp), [expressions.cpp](expressions.cpp) |
| Scalar and structured values | [type_simple.cpp](type_simple.cpp), [type_complex.cpp](type_complex.cpp), [operands.cpp](operands.cpp) |
| Parser execution, packets, and loops | [parser.cpp](parser.cpp), [packet.cpp](packet.cpp), [loops.cpp](loops.cpp) |
| Tables | [type_table.cpp](type_table.cpp) |
| Type inference and specialization | [type_inference.cpp](type_inference.cpp), [visitor_specialize.cpp](visitor_specialize.cpp) |
| Comparison and definedness | [compare.cpp](../compare/compare.cpp) |

See the [test guide](../tests/README.md) for execution limits, test commands, and regression expectations.
