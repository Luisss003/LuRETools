# LuRETools
LuRETools is a research oriented reverse engineering suite for vulnerability analysis of compiled ELF binaries. It's a mix of binary structure recovery, P-code fact extraction, control-flow and dataflow analysis, dangerous sink identification, and heap-misuse triage to study how vulnerability-relevant behavior appears after compilation, optimization, stripping, and binary lowering.

The long-term goal is to support both practical reverse engineering workflows, such as identifying suspicious memcpy paths, allocation/free/use patterns, and UAF or double-free candidates, and research workflows, such as evaluating whether source-level CWE patterns remain recoverable in optimized binaries.

## Current commands
```sh
make
./luretools disarm <binary>
./luretools cfg <binary>
./luretools cfg --facts <binary>
./luretools cfg --dot <binary>
./luretools --help
```

## Project shape
LuRETools now starts from a module-oriented CLI instead of the old mixed
parser/hex/strings path. This change was caused by the experienced gained doing
the Decaf compiler project for my compilers course.

```text
include/lure_module.h   shared CLI module interface
src/lure_modules.c      module registry
include/lure_disarm.h   binary image and disassembly-facing data model
src/lure_disarm.c       ELF64 loader, function/section extraction, byte preview
include/lure_cfg.h      CFG graph model, function/block/edge records, options
src/lure_cfg.c          symbol-seeded CFG scaffold over LureDisarmImage
include/lure_pcode.h    P-code-facing IR records, varnodes, ops, and options
src/lure_pcode.c        disabled P-code export scaffold, kept for backend work
src/main.c              generic module dispatcher
```

## Instructions for Adding Custom Modules
1. Create `include/lure_<name>.h` and `src/lure_<name>.c`.
2. Expose `int lure_<name>_main(int argc, char **argv);`.
3. Add `src/lure_<name>.c` to `SRCS` in `Makefile`.
4. Include the header in `src/lure_modules.c`.
5. Add one `LureModule` entry to the `modules[]` table.
6. Reuse `LureDisarmImage` from `lure_disarm.h` when the module needs binary
   sections, functions, architecture, or entry-point metadata.

## `lure-disarm`
`lure-disarm` is the starting point for the binary-analysis side of the suite.
The current implementation intentionally focuses on structure:
- loads ELF64 binaries into a reusable image model
- records architecture, entry point, sections, permissions, and symbol-backed functions
- exposes a pass interface for future modules
- prints executable-section byte previews as `db` records until a decoder backend is added

The useful next modules should build on `LureDisarmImage` instead of reparsing
the file:

- decoder backend: turn bytes into `LureDisarmInstruction`
- CFG recovery: consume functions and decoded branches
- sinks: identify risky imported/library calls and instruction patterns
- taint/dataflow: track source-to-sink paths over decoded instructions or IR
- IR export: attach P-code/VEX-like facts per instruction or block

## `lure-pcode`
`lure-pcode` is currently disabled in the CLI until it has a real backend.
The code remains in the tree as the planned IR export layer. It uses the `lure-disarm`
binary image loader and emits explicit `CALLOTHER decode_pending` records for
executable bytes. That keeps the module honest: it does not pretend to recover
real P-code until a decoder/Sleigh backend is added.

Re-enable it by adding `lure_pcode_main` back to `src/lure_modules.c` after a
real Ghidra/Sleigh/pypcode backend exists, and by moving `src/lure_pcode.c`
from `DISABLED_SRCS` to `SRCS` in `Makefile`.

## `lure-cfg`
`lure-cfg` is the control-flow graph layer. The first implementation is a
seed graph, not full CFG recovery:
- loads binary metadata through `LureDisarmImage`
- creates one CFG function record per symbol-backed function
- creates one synthetic basic block per function
- marks blocks as `decode_pending`
- leaves edges empty until instruction decoding can identify branches, calls,
  fallthroughs, and returns

Useful commands:
```sh
./luretools cfg ./sample
./luretools cfg --facts ./sample
./luretools cfg --dot ./sample
```

The intended upgrade path is:
1. Feed decoded instructions or P-code records into `lure_cfg_build`.
2. Split each function seed block at branch targets and call/return boundaries.
3. Add resolved `fallthrough`, `branch`, `call`, and `return` edges.
4. Layer dominators, loops, call graph extraction, and path queries on top of
   the same `LureCfgGraph` model.
