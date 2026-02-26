# u2bench

Minimal, CLI-driven wasm benchmark harness to compare multiple engines under the same wasm corpus.

## Engines supported

- `wasm3`
- `uwvm2` (`uwvm`)
- WAMR (`iwasm`)
- `wasmtime`
- `wasmer`
- `wasmedge`
- `wavm`

## Concepts

- `--runtime`:
  - `int`: interpreter
  - `jit`: JIT compiler
  - `tiered`: tiered compiler
- `--mode`:
  - `full`: eager compilation (engine-specific)
  - `lazy`: lazy/on-demand compilation (engine-specific)
- `--metric`:
  - `internal`: use wasm-reported `Time: ... ms` / `time: ... ms` when present (more comparable across engines; excludes host-side compilation overhead)
  - `wall`: use external wall-clock time measured by the harness
  - `auto`: prefer `internal` when available, otherwise fall back to `wall`
- `--bench-kind` / `--bench-tag`:
  - filter the wasm corpus by benchmark kind (compute/io/syscall/call/control-flow/etc) or tags
  - the run prints an additional per-kind summary at the end

If a runtime/mode is not supported by an engine, that combination is skipped.
At least one engine+runtime+mode combination must remain, otherwise the run aborts.

## Quick start

From `/tmp/u2bench`:

```bash
python3 runbench.py \
  --add-uwvm2 --add-wasm3 --add-wamr --add-wasmtime \
  --runtime=int --runtime=jit --runtime=tiered \
  --mode=full --mode=lazy \
  --metric=auto \
  --root uwvm2bench \
  --timeout 25 \
  --out logs/results.json \
  --plot --plot-out logs/results.png
```

Notes:
- `uwvm2` is treated as **not supporting `jit`** by default (per project policy), even if the binary exposes a `jit` flag.
- `wasm3 --compile` is mapped as `--mode=full`; no `--compile` is `--mode=lazy`.
- `uwvm2 -Rcm full|lazy` is mapped as `--mode=full|lazy`.
- `wasmtime`:
  - `--mode=lazy` runs the `.wasm` directly.
  - `--mode=full` precompiles via `wasmtime compile` then runs with `wasmtime run --allow-precompiled` (cache under `<root>/cache/u2bench/wasmtime/`).
- Most engines run with a WASI directory mapping of the current working directory (engine-specific).

## MVP WASI corpus (C++ + WAT)

This repo also includes a small, generated corpus under `wasm/corpus/`, built from sources in `wasm/src/` using:
- `clang++ --target=wasm32-wasip1` + a WASI sysroot
- `wat2wasm` (WABT)

Build:

```bash
WASI_SYSROOT=/path/to/wasi-sysroot python3 wasm/build_corpus.py
```

Run it with internal timing (recommended):

```bash
python3 runbench.py \
  --add-uwvm2 --add-wasm3 --add-wamr --add-wasmtime \
  --runtime=int --runtime=jit --runtime=tiered \
  --mode=full --mode=lazy \
  --metric=internal \
  --root wasm/corpus
```

## Output

- JSON results are written to `--out`.
- The summary prints geomean/median wall-time per engine-config and ratios vs a baseline.
- `--plot` draws a bar chart of geomean ratios vs baseline (optional dependency: `matplotlib`).
  - If `matplotlib` is missing, the run still completes and plot is skipped.

## Custom engine paths

If an engine is not in `PATH`, pass `--*-bin`:

```bash
python3 runbench.py --add-wasmedge --wasmedge-bin /path/to/wasmedge ...
```
