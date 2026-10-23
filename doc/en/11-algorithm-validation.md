# Borrow control flow and algorithm validation

Historical configurations and measurements are preserved below. Programs execute through the VM.

`source/chali_borrow_checker.hpp` checks reference escapes; `source/chali_borrow_flow.hpp` checks
intraprocedural use after free and use after move. The adapter in
`source/semantics/borrow_flow.cpp` records bound storage identities and control flow.
The independent checker runs after all functions and generic instances have been lowered.

The design draws on [Rust MIR dataflow](https://rustc-dev-guide.rust-lang.org/mir/dataflow.html)
and [NLL RFC 2094](https://github.com/rust-lang/rfcs/blob/main/text/2094-nll.md):
branch joins union possible origins, and a work queue computes loop fixed points before
diagnosing reachable operations. Return, break, continue and short circuit edges are explicit.
Constant Boolean conditions prune unreachable edges.

Reference slots are separate from their referents. Field paths and constant indices can be
distinguished; dynamic indices may overlap. Deletion invalidates heap storage, while move and
explicit drop invalidate resource values. Numeric move keeps its existing zeroing semantics.
Whole value assignment can restore moved values. Writing through a dangling reference cannot
restore freed storage. Allocation sites distinguish their latest instance from older instances,
so reallocation never revives old aliases. Independent violations produce `CHL1205` diagnostics.

This does not implement all Rust borrowing rules. ChaLi retains multiple mutable aliases and
conservative lexical escape checks. Analysis is intraprocedural: ordinary reference-returning
functions conservatively propagate reference argument origins, and visible method bodies supply field, argument and fresh allocation return origins.
Callbacks and external functions do not receive inferred destruction summaries. A reference field with no known binding is not treated as its parent object; runtime checks cover unknown origins, preventing a linked-list successor from being mistaken for its freed predecessor. References held inside
containers and complex dynamic or recursive aliases have precision limits. Recursive paths
are widened after 16 segments to guarantee convergence. Runtime checks remain enabled.

## Data structures

[`borrow_algorithms.chl`](../../tests/fixtures/borrow_algorithms.chl) uses allocated nodes
and `Option<Node mut&>` links. It builds, reverses, verifies and frees a linked list; inserts
into a binary search tree, verifies sorted traversal and frees nodes in postorder; and compares
DFS reachability with BFS distances from every vertex in a graph with loops and parallel edges.
Destructor counts must match allocations, with no leak messages. An independent Python BFS
checks the distance checksum, and a set checks the number of unique BST nodes.

## NOIP 2015 Transport Plan

The integration test implements NOIP 2015 advanced group Day 2 problem 3
([original statement mirror](https://huang.lt/download.php?id=15296&type=statement)).
One tree edge is set to zero to minimize the longest requested path.
[`noip2015_transport.chl`](../../tests/fixtures/noip2015_transport.chl) combines iterative
tree traversal, binary lifting LCA, tree differences and binary search. Its graph and lifting tables share
a runtime array. Query records use an aggregate array and are cross-checked against a small generic container copy, avoiding deep recursion and large container growth.
It runs in `O((n+m) log n + (n+m) log W)`, where `W` is the longest original path.

[`chali_algorithm_robustness.py`](../../tests/chali_algorithm_robustness.py) independently
enumerates every removed edge and traverses each requested path for small cases. It covers
chains, stars, random trees, zero/equal weights and repeated/reversed queries, plus the official
sample answer `11`. Large equal-weight chains have the known answer `(n-2)*1000`.

```sh
python tests/chali_borrow_flow.py --chali ChaLi.exe
g++ -std=c++17 -I. tests/chali_borrow_checker.cpp -o build/borrow-checker-test
python tests/chali_algorithm_robustness.py --chali ChaLi.exe
python tests/chali_algorithm_robustness.py --chali ChaLi.exe --stress-size 300000
```

Desktop and embedded Linux builds share the single `source/stackvm/` core and lifecycle native instructions, with host features trimmed by build macros.

Local module gates run after building the compiler with `make`:

```sh
python tools/run_module_gates.py generics tools docs --jobs 4
python tools/run_module_gates.py language --random-recheck 1 --seed 20261013
sh ci.sh --jobs 4
```

Gates default to at most four worker threads. Set `--jobs 1` for serial execution,
or configure the default with `CHALI_GATE_JOBS`. Only explicitly registered independent
Python and VM checks run concurrently. Generation, builds, samples, networking and unknown checks
form serial barriers. Failed parser generation or compiler builds skip subsequent checks,
preventing tests from using stale executables. Ordinary test failures do not stop other checks.
CI writes individual logs and ordered JSON results to `tmp/ci/logs/`; module gates use a
separate directory for each run. Random rechecks use registration order and the recorded seed,
then run serially to avoid duplicate tests sharing a working directory.
Module gates cover only the selected scope and do not replace full release validation.
