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

## Corpus layout

- **Recommended (fair, internal timing):** `wasm/corpus/`
  - Built from `wasm/src/` (C++ + WAT) via `wasm/build_corpus.py`.
  - Every benchmark prints `Time: <ms> ms` measured *inside* the wasm guest using WASI clocks.
  - Includes a balanced mix of compute/memory/control-flow/call/locals/operand-stack + WASI syscalls/I/O, plus crypto/db/vm/science workloads.
- **Legacy microbenches (flat, extreme few-variables):** `legacy_minvar_microbenches/`
  - Useful for “tiny wasm” experiments.
  - Most modules **do not** print `Time: ...`, so `--metric=internal` won’t work reliably here (use `--metric=wall` or `--metric=auto`).

## Fairness

This suite aims to avoid bias toward any specific VM/engine:

- **Same wasm binaries across engines:** u2bench runs the same `.wasm` corpus for every engine variant (no per-engine rebuilds).
- **Internal timing option:** for `wasm/corpus/`, time is measured inside the guest via WASI clocks, reducing host-side noise (process startup, scheduler jitter, harness overhead).
- **Steady-state vs end-to-end:** use `--metric=internal` to focus on execution; use `--metric=wall` when you *want* to include compilation/startup overheads.
- **Compatibility-first corpus:** the generated corpus targets `wasm32-wasip1` and disables newer proposals (bulk-memory, sign-ext, multi-value, ref-types, …) to keep the workload comparable across engines.
- **Intersection-based ratios:** ratios vs baseline are computed on the **common** subset where both variants produced a valid metric, avoiding “missing benchmark” bias.
- **Workload diversity:** covers IO/syscall/memory/local/operand-stack/call/control-flow, plus integer-heavy and float-heavy programs (filterable via tags like `int_dense` / `float_dense`).

Coverage cheat-sheet (examples from `wasm/corpus/`):
- `compute_dense`: `micro/loop_i64.wasm`, `micro/global_dense_i32.wasm`, `micro/bitops_i32_mix.wasm`, `micro/bitops_i64_mix.wasm`, `micro/bitops_i64_dense.wasm`, `micro/reg_pressure_i64_10m.wasm`, `micro/reg_pressure_f64_5m.wasm`, `micro/mul_add_i32_50m.wasm`, `micro/int128_mul_u64_2m.wasm`, `micro/divrem_i64.wasm`, `micro/divrem_i64_dense.wasm`, `micro/div_sqrt_f64.wasm`, `crypto/*` (e.g. `crypto/blake2b.wasm`, `crypto/poly1305_1m_x10.wasm`), `science/*` (e.g. `science/kmeans_f32_50k_k16_x25.wasm`)
- `io_dense`: `wasi/file_rw_8m.wasm`, `wasi/small_io_64b_100k.wasm`, `wasi/readv_4x16_200k.wasm`, `wasi/writev_4x16_100k.wasm`, `wasi/pread_64b_100k.wasm`, `wasi/pwrite_64b_50k.wasm`
- `syscall_dense`: `wasi/clock_gettime.wasm`, `wasi/clock_res_get_200k.wasm`, `wasi/clock_time_get_wat_200k.wasm`, `wasi/args_get_200k.wasm`, `wasi/args_sizes_get_200k.wasm`, `wasi/environ_get_200k.wasm`, `wasi/environ_sizes_get_200k.wasm`, `wasi/sched_yield_200k.wasm`, `wasi/open_close_200k.wasm`, `wasi/open_missing_200k.wasm`, `wasi/path_filestat_get_100k.wasm`, `wasi/prestat_dir_name_200k.wasm`, `wasi/poll_oneoff_clock_200k.wasm`, `wasi/seek_only_500k.wasm`, `wasi/fd_write_0len_100k.wasm`, `wasi/fd_write_0len_wat_100k.wasm`, `wasi/fd_read_0len_200k.wasm`, `wasi/fd_read_0len_wat_200k.wasm`, `wasi/fd_fdstat_get_200k.wasm`, `wasi/fd_filestat_get_200k.wasm`, `wasi/open_close_stat_20k.wasm`, `wasi/random_get_16m.wasm`, `wasi/random_get_32b_200k.wasm`
- `memory_dense`: `micro/mem_sum_i32.wasm`, `micro/mem_fill_i32.wasm`, `micro/mem_copy_i32.wasm`, `micro/mem_copy_u8_1m_x8.wasm`, `micro/mem_copy_libc_u8_4m_x32.wasm`, `micro/mem_copy_small_64b_5m.wasm`, `micro/mem_move_libc_u8_4m_x24.wasm`, `micro/mem_cmp_libc_u8_4m_x32.wasm`, `micro/mem_set_libc_u8_4m_x32.wasm`, `micro/mem_chr_libc_u8_4m_x32.wasm`, `micro/mem_hist_u8_4m_x16.wasm`, `micro/mem_stride_i32.wasm`, `micro/mem_load_store_i64.wasm`, `micro/mem_unaligned_i64.wasm`, `micro/memory_grow_1p_x256.wasm`, `micro/pointer_chase_u32_1m.wasm`, `micro/random_access_u32_16m.wasm`, `science/daxpy_f64.wasm`, `db/*`
- `local_dense`: `micro/local_dense_i32.wasm`, `micro/local_dense_i64.wasm`, `micro/local_dense_f32.wasm`, `micro/local_dense_f64.wasm`
- `operand_stack_dense`: `micro/operand_stack_dense_i32.wasm`, `micro/operand_stack_dense_i64.wasm`, `micro/operand_stack_dense_f32.wasm`, `micro/operand_stack_dense_f64.wasm`
- `call_dense`: `micro/call_dense_i32.wasm`, `micro/call_direct_dense_i32.wasm`, `micro/call_direct_many_args_i32.wasm`, `micro/call_indirect_many_args_i32.wasm`, `micro/call_indirect_i32_cpp_4m.wasm`, `vm/*`
- `control_flow_dense`: `micro/control_flow_dense_i32.wasm`, `micro/control_flow_dense_predictable_i32_50m.wasm`, `micro/br_if_dense_predictable_i32.wasm`, `micro/br_table_dense_i32.wasm`, `micro/big_switch_i32_10m.wasm`, `micro/json_tokenize_2m_x25.wasm`, `micro/varint_decode_u64_4m_x30.wasm`, `science/mandelbrot_f64.wasm`, `science/sieve_i32_2m.wasm`, `vm/*`
- `int_dense` (tag): `micro/loop_i32.wasm`, `crypto/*`, `science/matmul_i32.wasm`
- `float_dense` (tag): `micro/loop_f64.wasm`, `micro/div_sqrt_f32.wasm`, `micro/round_f64_dense.wasm`, `science/matmul_f32.wasm`, `science/matmul_f64.wasm`, `science/daxpy_f32.wasm`, `science/nbody_f64.wasm`

## Quick start

From the repo root:

```bash
python3 runbench.py \
  --add-uwvm2 --add-wasm3 --add-wamr --add-wasmtime \
  --runtime=int --runtime=jit --runtime=tiered \
  --mode=full --mode=lazy \
  --metric=internal \
  --root wasm/corpus \
  --timeout 25 \
  --out logs/results.json \
  --plot --plot-out logs/results.png
```

Notes:
- `uwvm2` is treated as supporting only `--runtime=int --mode=full` (the current `uwvm` binary reports other combinations unsupported).
- `wasm3 --compile` is mapped as `--mode=full`; no `--compile` is `--mode=lazy`.
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

Notes:
- For WasmEdge installed via its env script, run `source ~/.wasmedge/env` in your shell before launching `runbench.py` so the dynamic libraries and `wasmedge` binary are discoverable.
