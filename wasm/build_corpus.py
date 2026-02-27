#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class BuildUnit:
    kind: str  # "cc" | "wat"
    src: Path
    out: Path


def _run(cmd: list[str], *, cwd: Path, verbose: bool) -> None:
    if verbose:
        print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=str(cwd), check=True)


def _default_sysroot() -> Path | None:
    env = os.environ.get("WASI_SYSROOT")
    if env and Path(env).is_dir():
        return Path(env)
    p = Path("/Users/liyinan/Documents/MacroModel/src/wasi-libc/build-mvp/sysroot")
    if p.is_dir():
        return p
    return None


def build_cc(
    *,
    clangxx: str,
    sysroot: Path,
    unit: BuildUnit,
    verbose: bool,
) -> None:
    unit.out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        clangxx,
        "-o",
        str(unit.out),
        str(unit.src),
        "-Ofast",
        "-Wno-deprecated-ofast",
        "-s",
        "-flto",
        "-fuse-ld=lld",
        "-fno-rtti",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        "-fno-exceptions",
        "--target=wasm32-wasip1",
        f"--sysroot={sysroot}",
        "-std=c++26",
        "-mno-bulk-memory",
        "-mno-bulk-memory-opt",
        "-mno-nontrapping-fptoint",
        "-mno-sign-ext",
        "-mno-mutable-globals",
        "-mno-multivalue",
        "-mno-reference-types",
        "-mno-call-indirect-overlong",
    ]
    _run(cmd, cwd=unit.src.parent, verbose=verbose)


def build_wat(*, unit: BuildUnit, verbose: bool) -> None:
    unit.out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "wat2wasm",
        str(unit.src),
        "-o",
        str(unit.out),
        "--disable-bulk-memory",
        "--disable-sign-extension",
        "--disable-saturating-float-to-int",
        "--disable-multi-value",
        "--disable-reference-types",
    ]
    _run(cmd, cwd=unit.src.parent, verbose=verbose)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Build the u2bench wasm corpus (C++ + WAT).")
    ap.add_argument("--sysroot", default="", help="WASI sysroot (default: $WASI_SYSROOT or wasi-libc build-mvp sysroot)")
    ap.add_argument("--clangxx", default="clang++", help="clang++ path (default: clang++)")
    ap.add_argument("--out", default="wasm/corpus", help="output directory (default: wasm/corpus)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[1]
    out_root = (repo_root / args.out).resolve()

    sysroot: Path | None
    if args.sysroot:
        sysroot = Path(args.sysroot)
    else:
        sysroot = _default_sysroot()
    if not sysroot or not sysroot.is_dir():
        raise SystemExit(
            "WASI sysroot not found. Pass --sysroot or set $WASI_SYSROOT. "
            "Expected a wasi-libc sysroot with include/ and lib/."
        )

    units: list[BuildUnit] = [
        # WASI / syscalls
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_clock_gettime.cc", out=out_root / "wasi/clock_gettime.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_clock_res_get.cc", out=out_root / "wasi/clock_res_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_file_rw.cc", out=out_root / "wasi/file_rw_8m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_small_io.cc", out=out_root / "wasi/small_io_64b_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_open_close_stat.cc", out=out_root / "wasi/open_close_stat_20k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_random_get.cc", out=out_root / "wasi/random_get_16m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_write_0len.cc", out=out_root / "wasi/fd_write_0len_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_read_0len.cc", out=out_root / "wasi/fd_read_0len_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_fdstat_get.cc", out=out_root / "wasi/fd_fdstat_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_filestat_get_dense.cc", out=out_root / "wasi/fd_filestat_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_args_get_dense.cc", out=out_root / "wasi/args_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_environ_get_dense.cc", out=out_root / "wasi/environ_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_seek_read.cc", out=out_root / "wasi/seek_read_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_seek_only.cc", out=out_root / "wasi/seek_only_500k.wasm"),

        # Crypto
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_sha256.cc", out=out_root / "crypto/sha256.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_chacha20.cc", out=out_root / "crypto/chacha20.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_aes128.cc", out=out_root / "crypto/aes128.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_crc32.cc", out=out_root / "crypto/crc32_4m_x8.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_siphash24.cc", out=out_root / "crypto/siphash24.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_blake2s.cc", out=out_root / "crypto/blake2s.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_blake2b.cc", out=out_root / "crypto/blake2b.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_poly1305.cc", out=out_root / "crypto/poly1305_1m_x10.wasm"),

        # DB-ish workloads
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/db_kv_hash.cc", out=out_root / "db/kv_hash.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/db_radix_sort_u64.cc", out=out_root / "db/radix_sort_u64_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/db_bloom_filter.cc", out=out_root / "db/bloom_filter.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/db_btree_u64.cc", out=out_root / "db/btree_u64_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/db_skiplist_u64.cc", out=out_root / "db/skiplist_u64_50k_ops_400k.wasm"),

        # VM / interpreter-ish
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/vm_tinybytecode.cc", out=out_root / "vm/mini_lua_like_vm.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/vm_minilua_table_vm.cc", out=out_root / "vm/minilua_table_vm.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/vm_expr_parser.cc", out=out_root / "vm/expr_parser.wasm"),

        # Science / compute
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_matmul_i32.cc", out=out_root / "science/matmul_i32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_matmul_f64.cc", out=out_root / "science/matmul_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_matmul_f32.cc", out=out_root / "science/matmul_f32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_sieve_i32.cc", out=out_root / "science/sieve_i32_2m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_gcd_i64.cc", out=out_root / "science/gcd_i64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_daxpy_f64.cc", out=out_root / "science/daxpy_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_daxpy_f32.cc", out=out_root / "science/daxpy_f32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_mandelbrot_f64.cc", out=out_root / "science/mandelbrot_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_nbody_f64.cc", out=out_root / "science/nbody_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_fft_f64.cc", out=out_root / "science/fft_f64_2048_x120.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_black_scholes_f64.cc", out=out_root / "science/black_scholes_f64_20k_x25.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/science_kmeans_f32.cc", out=out_root / "science/kmeans_f32_50k_k16_x25.wasm"),

        # Microbenches (C++/libc)
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_pointer_chase_u32.cc", out=out_root / "micro/pointer_chase_u32_1m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_bitops_i32_mix.cc", out=out_root / "micro/bitops_i32_mix.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_divrem_i64.cc", out=out_root / "micro/divrem_i64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_div_sqrt_f64.cc", out=out_root / "micro/div_sqrt_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_div_sqrt_f32.cc", out=out_root / "micro/div_sqrt_f32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_malloc_free_small.cc", out=out_root / "micro/malloc_free_small_1m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_fnv1a_u64_fixedlen.cc", out=out_root / "micro/fnv1a_u64_fixedlen_80k_x10.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_utf8_validate.cc", out=out_root / "micro/utf8_validate_1m_x80.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_convert_i32_f64.cc", out=out_root / "micro/convert_i32_f64.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_convert_i64_f32.cc", out=out_root / "micro/convert_i64_f32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_rle_u8.cc", out=out_root / "micro/rle_u8_4m_x10.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_base64_u8.cc", out=out_root / "micro/base64_u8_3m_x12.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_json_tokenize.cc", out=out_root / "micro/json_tokenize_2m_x25.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_varint_decode_u64.cc", out=out_root / "micro/varint_decode_u64_4m_x30.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memcmp_libc_u8.cc", out=out_root / "micro/mem_cmp_libc_u8_4m_x32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_mem_hist_u8.cc", out=out_root / "micro/mem_hist_u8_4m_x16.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_indirect_call_i32.cc", out=out_root / "micro/call_indirect_i32_cpp_4m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_control_flow_dense_predictable_i32.cc", out=out_root / "micro/control_flow_dense_predictable_i32_50m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_big_switch_i32.cc", out=out_root / "micro/big_switch_i32_10m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_random_access_u32.cc", out=out_root / "micro/random_access_u32_16m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_mul_add_i32.cc", out=out_root / "micro/mul_add_i32_50m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_int128_mul_u64.cc", out=out_root / "micro/int128_mul_u64_2m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memcpy_libc_u8.cc", out=out_root / "micro/mem_copy_libc_u8_4m_x32.wasm"),

        # Microbenches (WAT, no libc)
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_i32.wat", out=out_root / "micro/loop_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_f32.wat", out=out_root / "micro/loop_f32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_f64.wat", out=out_root / "micro/loop_f64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_i64.wat", out=out_root / "micro/loop_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/memory_grow_1p_x256.wat", out=out_root / "micro/memory_grow_1p_x256.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/memory_grow_touch_1p_x256.wat", out=out_root / "micro/memory_grow_touch_1p_x256.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/clock_time_get_dense.wat", out=out_root / "wasi/clock_time_get_wat_200k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/fd_write_0len_dense.wat", out=out_root / "wasi/fd_write_0len_wat_100k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/fd_read_0len_dense.wat", out=out_root / "wasi/fd_read_0len_wat_200k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_sum_i32.wat", out=out_root / "micro/mem_sum_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_fill_i32.wat", out=out_root / "micro/mem_fill_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_copy_i32.wat", out=out_root / "micro/mem_copy_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_copy_u8.wat", out=out_root / "micro/mem_copy_u8_1m_x8.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_stride_i32.wat", out=out_root / "micro/mem_stride_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/global_dense_i32.wat", out=out_root / "micro/global_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/select_dense_i32.wat", out=out_root / "micro/select_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/local_dense_i32.wat", out=out_root / "micro/local_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/local_dense_i64.wat", out=out_root / "micro/local_dense_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/local_dense_f32.wat", out=out_root / "micro/local_dense_f32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/local_dense_f64.wat", out=out_root / "micro/local_dense_f64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/operand_stack_dense_i32.wat", out=out_root / "micro/operand_stack_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/operand_stack_dense_i64.wat", out=out_root / "micro/operand_stack_dense_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/operand_stack_dense_f32.wat", out=out_root / "micro/operand_stack_dense_f32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/operand_stack_dense_f64.wat", out=out_root / "micro/operand_stack_dense_f64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/call_dense_i32.wat", out=out_root / "micro/call_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/call_direct_dense_i32.wat", out=out_root / "micro/call_direct_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/control_flow_dense_i32.wat", out=out_root / "micro/control_flow_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/br_table_dense_i32.wat", out=out_root / "micro/br_table_dense_i32.wasm"),
    ]

    built = 0
    for u in units:
        if not u.src.exists():
            raise SystemExit(f"missing source: {u.src}")
        if u.kind == "cc":
            build_cc(clangxx=args.clangxx, sysroot=sysroot, unit=u, verbose=args.verbose)
        elif u.kind == "wat":
            build_wat(unit=u, verbose=args.verbose)
        else:
            raise SystemExit(f"unknown kind: {u.kind}")
        built += 1

    print(f"built {built} wasm files under: {out_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
