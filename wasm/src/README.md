# wasm/src

Sources for the generated MVP-ish WASI benchmark corpus under `wasm/corpus/`.

All benchmarks report **internal time** by printing a line like:

```
Time: <ms> ms
```

The time is measured inside the wasm guest using WASI clocks (`clock_gettime` in C/C++, or `clock_time_get` in WAT),
so `runbench.py --metric=internal` can compare engines while minimizing host-side timing noise.

## Benchmarks

### C/C++ (`wasm/src/cc/`)

- `wasi_clock_gettime.cc`: syscall-ish overhead of repeated `clock_gettime`.
- `wasi_clock_res_get.cc`: repeated WASI `clock_res_get` calls (syscall overhead).
- `wasi_file_rw.cc`: simple WASI filesystem workload (write+read 8 MiB).
- `wasi_small_io.cc`: many small WASI reads/writes (64B * 100k).
- `wasi_open_close_stat.cc`: repeated open+fstat+close of a small file.
- `wasi_random_get.cc`: WASI `random_get` throughput/overhead (16 MiB total).
- `wasi_fd_write_0len.cc`: repeated WASI `fd_write` calls with zero-length iovec (syscall overhead).
- `wasi_fd_read_0len.cc`: repeated WASI `fd_read` calls with zero-length iovec (syscall overhead).
- `wasi_fd_fdstat_get.cc`: repeated WASI `fd_fdstat_get` calls (syscall overhead).
- `wasi_fd_filestat_get_dense.cc`: repeated `fstat` calls on an open fd (syscall overhead).
- `wasi_args_get_dense.cc`: repeated WASI `args_get` calls (syscall overhead + small memory writes).
- `wasi_environ_get_dense.cc`: repeated WASI `environ_get` calls (syscall overhead + small memory writes).
- `wasi_seek_read.cc`: repeated `lseek`+small `read` on a small file (syscall + small I/O).
- `wasi_seek_only.cc`: repeated `lseek` on an open file descriptor (syscall overhead).
- `crypto_sha256.cc`: SHA-256 compression rounds.
- `crypto_chacha20.cc`: ChaCha20 block function.
- `crypto_aes128.cc`: AES-128 encrypt many blocks.
- `crypto_crc32.cc`: CRC32 over a 4 MiB buffer, repeated 8 times (32 MiB).
- `crypto_siphash24.cc`: SipHash-2-4 over many 64-bit keys.
- `crypto_blake2s.cc`: BLAKE2s compression rounds (32-bit-heavy crypto).
- `crypto_blake2b.cc`: BLAKE2b compression rounds (64-bit-heavy crypto).
- `crypto_poly1305.cc`: Poly1305 authenticator over a 1 MiB message (integer-heavy MAC).
- `db_kv_hash.cc`: in-memory open-addressing KV hash table (puts/gets, hits+misses).
- `db_radix_sort_u64.cc`: radix sort 200k u64 values.
- `db_bloom_filter.cc`: Bloom filter insert + membership queries.
- `db_btree_u64.cc`: B-tree insert + point lookups (DB-style pointer chasing + control flow).
- `db_skiplist_u64.cc`: skiplist insert + point lookups (pointer chasing + branches).
- `vm_tinybytecode.cc`: tiny bytecode interpreter (Lua-like VM style: switch dispatch + jumps).
- `vm_minilua_table_vm.cc`: Lua-ish bytecode VM with table set/get.
- `vm_expr_parser.cc`: parse+eval a small arithmetic expression (Lua-ish frontend work).
- `science_matmul_i32.cc`: int32 matrix multiply.
- `science_matmul_f64.cc`: float64 matrix multiply.
- `science_matmul_f32.cc`: float32 matrix multiply.
- `science_sieve_i32.cc`: prime sieve up to 2,000,000.
- `science_gcd_i64.cc`: Euclid GCD over many random pairs (integer compute).
- `science_daxpy_f64.cc`: DAXPY-like vector kernel (float compute + memory bandwidth).
- `science_daxpy_f32.cc`: DAXPY-like vector kernel (float32).
- `science_mandelbrot_f64.cc`: Mandelbrot set iterations (branchy float compute).
- `science_nbody_f64.cc`: simple N-body simulation (float + sqrt).
- `science_fft_f64.cc`: FFT forward+inverse (transcendentals + complex math).
- `science_black_scholes_f64.cc`: Black-Scholes (exp/log/sqrt + normal CDF approx).
- `science_kmeans_f32.cc`: k-means clustering (float compute + memory).
- `micro_pointer_chase_u32.cc`: pointer-chase / dependent-load loop (memory latency-ish).
- `micro_bitops_i32_mix.cc`: i32 bit operations (`popcount`/`clz`/`ctz`/rotates).
- `micro_divrem_i64.cc`: i64 division+remainder heavy loop.
- `micro_div_sqrt_f32.cc`: f32 division + sqrt heavy loop.
- `micro_div_sqrt_f64.cc`: f64 division + sqrt heavy loop.
- `micro_malloc_free_small.cc`: many small `malloc`+`free` pairs (allocator + memory.grow behavior).
- `micro_fnv1a_u64_fixedlen.cc`: fixed-length string hashing (FNV-1a 64-bit).
- `micro_utf8_validate.cc`: UTF-8 validation-style branchy byte scanning.
- `micro_convert_i32_f64.cc`: int/float conversion + mixed arithmetic.
- `micro_convert_i64_f32.cc`: int/float conversion + mixed arithmetic.
- `micro_rle_u8.cc`: simple RLE encode+decode over a 4 MiB buffer.
- `micro_base64_u8.cc`: base64 encode+decode throughput (byte/int-heavy).
- `micro_json_tokenize.cc`: JSON tokenization-style scan (branchy control flow).
- `micro_varint_decode_u64.cc`: varint decode scan (branchy control flow + bit ops).
- `micro_memcmp_libc_u8.cc`: libc `memcmp` throughput (memory read/compare).
- `micro_mem_hist_u8.cc`: byte histogram over a 4 MiB buffer (memory + scattered updates).
- `micro_indirect_call_i32.cc`: function-pointer / indirect call overhead (call_indirect style).
- `micro_control_flow_dense_predictable_i32.cc`: branch-heavy loop with predictable branching.
- `micro_big_switch_i32.cc`: large dense `switch` / jump-table control flow.
- `micro_random_access_u32.cc`: pseudo-random u32 array access (memory + cache effects).
- `micro_mul_add_i32.cc`: i32 multiply-add-heavy arithmetic loop.
- `micro_int128_mul_u64.cc`: 128-bit multiply emulation via `__int128` (soft-integer heavy).
- `micro_memcpy_libc_u8.cc`: libc `memcpy` throughput (memory bandwidth + stores).

### WAT (`wasm/src/wat/`)

- `loop_i32.wat`: hand-written tight loop micro-benchmark + WASI timing + `fd_write` output.
- `loop_f32.wat`: hand-written float32 loop micro-benchmark.
- `loop_f64.wat`: hand-written float loop micro-benchmark.
- `loop_i64.wat`: hand-written i64 loop micro-benchmark.
- `memory_grow_1p_x256.wat`: repeated `memory.grow` by 1 page (256 times), no page touching.
- `memory_grow_touch_1p_x256.wat`: repeated `memory.grow` by 1 page (256 times) and touches the new pages.
- `clock_time_get_dense.wat`: repeated `clock_time_get` calls (WAT, minimal overhead).
- `fd_write_0len_dense.wat`: repeated `fd_write` calls with zero-length iovec (WAT, syscall overhead).
- `fd_read_0len_dense.wat`: repeated `fd_read` calls with zero-length iovec (WAT, syscall overhead).
- `mem_sum_i32.wat`: memory read bandwidth (sum 1 MiB of i32s for many reps).
- `mem_fill_i32.wat`: memory write bandwidth (store 1 MiB of i32s for many reps).
- `mem_copy_i32.wat`: memory copy bandwidth (load+store 1 MiB of i32s for many reps).
- `mem_copy_u8.wat`: memory copy bandwidth (byte load+store, 1 MiB for many reps).
- `mem_stride_i32.wat`: strided memory read-modify-write over a 1 MiB region.
- `global_dense_i32.wat`: `global.get`/`global.set` dense loop.
- `select_dense_i32.wat`: `select` instruction dense loop (branchless-ish).
- `local_dense_i32.wat`: `local.get`/`local.set` heavy loop.
- `local_dense_i64.wat`: `local.get`/`local.set` heavy loop (i64 locals).
- `local_dense_f32.wat`: `local.get`/`local.set` heavy loop (f32 locals).
- `local_dense_f64.wat`: `local.get`/`local.set` heavy loop (f64 locals).
- `operand_stack_dense_i32.wat`: deep operand-stack push/reduce loop.
- `operand_stack_dense_i64.wat`: deep operand-stack push/reduce loop (i64).
- `operand_stack_dense_f32.wat`: deep operand-stack push/reduce loop (f32).
- `operand_stack_dense_f64.wat`: deep operand-stack push/reduce loop (f64).
- `call_dense_i32.wat`: indirect call overhead micro-benchmark.
- `call_direct_dense_i32.wat`: direct call overhead micro-benchmark.
- `control_flow_dense_i32.wat`: branch-heavy control-flow micro-benchmark.
- `br_table_dense_i32.wat`: `br_table` / switch dispatch heavy micro-benchmark.
