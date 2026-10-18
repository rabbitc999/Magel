# Borrow control flow and algorithm validation

`source/magel_borrow_checker.hpp` checks reference escapes; `source/magel_borrow_flow.hpp` checks
intraprocedural use after free and use after move. The adapter in
`source/epilogue/borrow_flow.cpp` records bound storage identities and control flow.
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
so reallocation never revives old aliases. Independent violations produce `MGL1205` diagnostics.

This does not implement all Rust borrowing rules. Magel retains multiple mutable aliases and
conservative lexical escape checks. Analysis is intraprocedural: ordinary reference-returning
functions conservatively propagate reference argument origins, and visible method bodies supply field, argument and fresh allocation return origins.
Callbacks and external functions do not receive inferred destruction summaries. A reference field with no known binding is not treated as its parent object; runtime checks cover unknown origins, preventing a linked-list successor from being mistaken for its freed predecessor. References held inside
containers and complex dynamic or recursive aliases have precision limits. Recursive paths
are widened after 16 segments to guarantee convergence. Runtime checks remain enabled.

## Data structures

[`borrow_algorithms.mgl`](../../tests/fixtures/borrow_algorithms.mgl) uses allocated nodes
and `Option<Node mut&>` links. It builds, reverses, verifies and frees a linked list; inserts
into a binary search tree, verifies sorted traversal and frees nodes in postorder; and compares
DFS reachability with BFS distances from every vertex in a graph with loops and parallel edges.
Destructor counts must match allocations, with no leak messages. An independent Python BFS
checks the distance checksum, and a set checks the number of unique BST nodes.

## NOIP 2015 Transport Plan

The integration test implements NOIP 2015 advanced group Day 2 problem 3
([original statement mirror](https://huang.lt/download.php?id=15296&type=statement)).
One tree edge is set to zero to minimize the longest requested path.
[`noip2015_transport.mgl`](../../tests/fixtures/noip2015_transport.mgl) combines iterative
tree traversal, binary lifting LCA, tree differences and binary search. Its graph and lifting tables share
a runtime array. Query records use an aggregate array and are cross-checked against a small generic container copy, avoiding deep recursion and large container growth.
It runs in `O((n+m) log n + (n+m) log W)`, where `W` is the longest original path.

[`magel_algorithm_robustness.py`](../../tests/magel_algorithm_robustness.py) independently
enumerates every removed edge and traverses each requested path for small cases. It covers
chains, stars, random trees, zero/equal weights and repeated/reversed queries, plus the official
sample answer `11`. Large equal-weight chains have the known answer `(n-2)*1000`.

```sh
python tests/magel_borrow_flow.py --magel magel.exe
g++ -std=c++17 -I. tests/magel_borrow_checker.cpp -o build/borrow-checker-test
python tests/magel_algorithm_robustness.py --magel magel.exe
python tests/magel_algorithm_robustness.py --magel magel.exe --stress-size 300000
```

Validation passed the original sample, 60 independently brute-forced random small trees, and an AOT chain with `n=m=300000` (output `299998000`, empty stderr). This stress check validates correctness and cleanup, rather than promising compliance with the original contest time limit.

Default cases run on VM and AOT. Stress inputs above 10000 vertices run on AOT only.
Use `--skip-aot` to isolate VM behavior and `--random-cases` to change the small-case count.
The latest input is saved to `build/algorithm-robustness/<compiler-name>/last-input.txt` for reproducing failures.

Both `drop()` and `delete` are idempotent. Repeated destruction has no effect, while later reads and writes remain invalid. The default VM/AOT heap reuses physical memory but assigns distinct logical identities, so old references cannot delete new allocations. Released identities are represented by a generation range in a fixed slot per physical allocation root. Reusing that root does not append tracking records. Explicit sweep() revokes idempotent deletion for retired identities while preserving live allocations; deleting a swept identity reports MGL1205. The caller must guarantee that these retired references are no longer needed. A complete watchdog restart clears tombstones and resets the arena.

Desktop and MCU builds share the single `library/stackvm/` core and lifecycle native instructions, with host features trimmed by build macros.
