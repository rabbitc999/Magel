# Standard library ownership and string methods

Host storage follows actual ownership. Database connections, arenas and timer sources have one owner; containers and random generators copy values; shared cells retain explicit sharing. See the [API index](../library/README.md) and [targeted regressions](../../tests/chali_opcode_values.py).

## Ownership and copying

`std::sql::Database` and `std::memory::Arena` are noncopyable owning opcodes. Transfer ownership with `std::lang::move(&value)`. Database keeps connection state, driver, error and affected rows in one host record, closes automatically and permits repeated close calls. Resetting or destroying an arena invalidates all borrowed spans.

`std::event::Timer` owns a noncopyable private opcode. Scope exit and stop cancel the source; `id()` and `Event.timer` are source identities and carry no release responsibility. `Random` deeply copies the current sequence position, so copies advance independently while fixed seeds remain reproducible.

Graph, UnionFind, binary and general trees, red-black trees, heaps and linked lists keep persistent host state. Ordinary copies are deep copies; queries and mutations do not round-trip internal arrays. Graph.snapshot exports adjacency arrays and attributes. UnionFind.parents/sizes export explicit copies; find compresses paths in place and unionSets merges by size without copying the full capacity. BinarySearchTree and GeneralTree snapshots include live-slot flags and the root; MinHeap.snapshot exports its heap array. Traversals and LinkedList.toArray return independent result lists. Modifying any snapshot does not modify the source.

LinkedList owns doubly linked nodes and a cursor index through a private opcode while preserving Iterator. Copies rebuild their index and retain the correspondence of original cursors within the copy. Deleted or cleared cursors never become valid again. Cursor and end insertion/removal have average constant complexity; value lookup and indexed iteration remain linear.

The [raw capacity measurement](../benchmarks/opcode-values.json) records the version, configuration and seed. Excluding construction, 30,000 queries took about 57.5 ms at 1,000 elements and 56.9 ms at 1,000,000 elements. This is one acceptance measurement on the same host, not a cross-machine performance guarantee.

JsonValue and TomlValue share one owning storage protocol, retain their Iterator wrappers and own independent subtrees. Copying deeply copies the subtree. Extracted children are independent values. Closing a value leaves copies and previously extracted children valid. TOML root dump preserves original comments, whitespace and line endings; repeated parse/save does not add or remove comments. Child dumps serialize only their own value.

Shared<T> copies a reference to the same cell, whose value belongs to the thread group. Individual get/set/swap calls are atomic; combinations are not transactions. Regex copies share an immutable compiled pattern, use separate match state per call and release the pattern with the last reference.

## Red-black deletion

RedBlackTree.remove(key) returns true when the key existed and was removed, otherwise false. Black-node deletion restores black height by sibling recoloring, rotations and upward repair. Removed node slots are reused. Insert, remove and lookup remain logarithmic. Two-child deletion replaces the key with its successor; internal node slots are not public references.

validate checks ordering, parent links, a black root, black children of red nodes, equal black heights and the live count. inorder exports an independent sorted snapshot. Regressions include 12,000 deterministic mixed mutations with invariant checks after every step and comparisons to an independent membership model, plus empty trees, deletion to empty, reinsertion, copy/snapshot isolation and int64 extremes.

## String methods

String is automatically available from std::lang. Transformations return independent values. Embedded zero bytes require List<uint8>. Ordinary lengths, searches and slices use UTF-8 **bytes**. Explicit Unicode operations use **code points**, not grapheme clusters, and perform no Unicode normalization.

| Category | Methods |
| --- | --- |
| Length and state | length, size, byteLength, isEmpty, isBlank |
| ASCII classification | isDigit, isAlpha, isAlnum, isAscii |
| Search and comparison | indexOf, lastIndexOf, contains, startsWith, endsWith, count, compare |
| Byte access and slicing | byteAt, substring, slice, left, right, bytes |
| Trimming and case | trim, trimStart, trimEnd, toLower, toUpper |
| Replacement and editing | replaceFirst, replaceAll, replace, removePrefix, removeSuffix, insert, remove |
| Combining and padding | repeat, padStart, padEnd, join, concat, prepend |
| Splitting | split, splitWhitespace, lines |
| Unicode code points | charLength, charAt, utf8Substring, unicodeChars, reverse |

indexOf(text,start=0) and lastIndexOf(text,start=-1) return byte offsets or -1. Negative forward starts clamp to zero; negative reverse starts search from the end. Empty needles match at the effective boundary. count counts nonoverlapping matches; an empty needle counts byteLength+1. compare returns only -1, 0 or 1.

substring(start,count=-1) clamps negative starts to zero and positions to the byte length; negative count means the remainder. slice(start,end) uses a clamped half-open interval and returns empty for a reversed interval. left/right return empty for negative lengths. Byte slicing, editing and padding may split multibyte encodings; use utf8Substring or unicodeChars for whole code points.

byteAt and charAt return None out of range. charAt returns one complete code point as a String. Unicode methods strictly reject malformed, truncated, overlong, surrogate or out-of-range UTF-8 encodings. reverse preserves each code point's encoding, while combining marks and joined emoji sequences are reversed as separate code points.

Case conversion and classification cover ASCII only. trim, isBlank and splitWhitespace recognize space and tab through carriage return independently of the host locale. Empty strings are blank and ASCII, but are not digit, alpha or alphanumeric strings.

replace aliases replaceAll. Replacements are nonoverlapping; an empty needle leaves the value unchanged. Negative repeat counts or padding lengths report errors. Repetition and joining check length overflow before allocation. Padding cycles fill to a target byte length, leaves values unchanged for empty fill or a target no longer than the input, and defaults to a space.

split(separator,keepEmpty=true) retains leading, interior and trailing empty fields. Empty separators split UTF-8 code points. splitWhitespace drops empty fields. lines(keepEmpty=true) treats CRLF as one separator and also accepts standalone CR/LF, retaining trailing empty lines by default. join(items) uses the receiver as separator and returns empty for an empty list. Existing chars() remains a lazy std::iter entry point; unicodeChars returns an independent List<String> directly.

See the [string example](../../example/samples/string_methods.chl), [ownership example](../../example/samples/opcode_value_semantics.chl) and [red-black deletion example](../../example/samples/std_red_black_delete.chl). Strings have 1,050 additional deterministic Python differential assertions. Other libraries are sampled with a fixed seed rather than tested in full.
