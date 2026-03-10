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
    cflags: tuple[str, ...] = ()


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
        *unit.cflags,
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
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_open_close_only.cc", out=out_root / "wasi/open_close_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_random_get.cc", out=out_root / "wasi/random_get_16m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_random_get_32b_dense.cc", out=out_root / "wasi/random_get_32b_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_write_0len.cc", out=out_root / "wasi/fd_write_0len_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_read_0len.cc", out=out_root / "wasi/fd_read_0len_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_fdstat_get.cc", out=out_root / "wasi/fd_fdstat_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_fd_filestat_get_dense.cc", out=out_root / "wasi/fd_filestat_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_args_get_dense.cc", out=out_root / "wasi/args_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_args_sizes_get_dense.cc", out=out_root / "wasi/args_sizes_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_environ_get_dense.cc", out=out_root / "wasi/environ_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_environ_sizes_get_dense.cc", out=out_root / "wasi/environ_sizes_get_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_sched_yield_dense.cc", out=out_root / "wasi/sched_yield_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_seek_read.cc", out=out_root / "wasi/seek_read_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_seek_only.cc", out=out_root / "wasi/seek_only_500k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_readv_4x16_dense.cc", out=out_root / "wasi/readv_4x16_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_writev_4x16_dense.cc", out=out_root / "wasi/writev_4x16_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_pread_64b_dense.cc", out=out_root / "wasi/pread_64b_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_pwrite_64b_dense.cc", out=out_root / "wasi/pwrite_64b_50k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_open_missing_dense.cc", out=out_root / "wasi/open_missing_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_path_filestat_get_dense.cc", out=out_root / "wasi/path_filestat_get_100k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_prestat_dir_name_dense.cc", out=out_root / "wasi/prestat_dir_name_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/wasi_poll_oneoff_clock_dense.cc", out=out_root / "wasi/poll_oneoff_clock_200k.wasm"),

        # Crypto
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_sha256.cc", out=out_root / "crypto/sha256.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_chacha20.cc", out=out_root / "crypto/chacha20.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_aes128.cc", out=out_root / "crypto/aes128.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_crc32.cc", out=out_root / "crypto/crc32_4m_x8.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_siphash24.cc", out=out_root / "crypto/siphash24.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_blake2s.cc", out=out_root / "crypto/blake2s.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_blake2b.cc", out=out_root / "crypto/blake2b.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_poly1305.cc", out=out_root / "crypto/poly1305_1m_x10.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/crypto_keccakf1600.cc", out=out_root / "crypto/keccakf1600.wasm"),

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
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/circuit_rc_mesh_f64_40x40_x36.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=1",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/poisson_cg_f64_64x64_x28.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=2",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/reaction_diffusion_f32_96x96_x60.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=3",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/pagerank_sparse_f64_4096n_x36.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=4",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/kalman_track_f64_6d_x40k.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=5",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/spmv_csr_f64_4096n_x72.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=6",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/value_iteration_f64_96x96_x80.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=7",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/graph_sssp_relax_f64_4096n_x48.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=8",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/ising_metropolis_i8_128x128_x28.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=9",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/stochastic_vol_mc_f64_4096p_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=10",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/boids_f32_320_x48.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=11",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/power_flow_gs_f64_144bus_x48.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=12",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/lbm_d2q9_f32_96x64_x36.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=13",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/fem_truss_f64_24x24_x40.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=14",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/lqr_control_f64_128sys_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=15",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/multigrid_vcycle_f32_128x128_x24.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=16",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/fdtd_em_f32_160x96_x60.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=17",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/ik_jacobian_f64_192arm_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=18",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/shallow_water_f32_128x96_x64.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=19",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/pose_graph_relax_f64_768n_x72.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=20",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/particle_filter_f32_32trk_256p_x72.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=21",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/bicgstab_f64_96d_x56.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=22",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/mpc_boxqp_f64_48sys_x120.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=23",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/gmres_restart_f64_88d_x44.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=24",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/dc_opf_proj_f64_96bus_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=25",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/bundle_adjustment_f64_48cam_192pt_x60.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=26",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/trajectory_sqp_f64_64traj_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=27",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/factor_graph_landmark_f64_384pose_256lm_x64.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=28",),
        ),
        BuildUnit(
            kind="cc",
            src=repo_root / "wasm/src/cc/science_extra_suite.cc",
            out=out_root / "science/contact_dynamics_f64_192body_x96.wasm",
            cflags=("-DU2BENCH_EXTRA_KIND=29",),
        ),

        # Microbenches (C++/libc)
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_pointer_chase_u32.cc", out=out_root / "micro/pointer_chase_u32_1m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_pointer_chase_u64.cc", out=out_root / "micro/pointer_chase_u64_4m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_bitops_i32_mix.cc", out=out_root / "micro/bitops_i32_mix.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_bitops_i64_mix.cc", out=out_root / "micro/bitops_i64_mix.wasm"),
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
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memset_libc_u8.cc", out=out_root / "micro/mem_set_libc_u8_4m_x32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memchr_libc_u8.cc", out=out_root / "micro/mem_chr_libc_u8_4m_x32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_mem_hist_u8.cc", out=out_root / "micro/mem_hist_u8_4m_x16.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_reg_pressure_i32.cc", out=out_root / "micro/reg_pressure_i32_20m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_reg_pressure_f32.cc", out=out_root / "micro/reg_pressure_f32_12m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_reg_pressure_i64.cc", out=out_root / "micro/reg_pressure_i64_10m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_reg_pressure_f64.cc", out=out_root / "micro/reg_pressure_f64_5m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_indirect_call_i32.cc", out=out_root / "micro/call_indirect_i32_cpp_4m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_control_flow_dense_predictable_i32.cc", out=out_root / "micro/control_flow_dense_predictable_i32_50m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_big_switch_i32.cc", out=out_root / "micro/big_switch_i32_10m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_random_access_u32.cc", out=out_root / "micro/random_access_u32_16m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_trig_mix_f64.cc", out=out_root / "micro/trig_mix_f64_200k.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_quicksort_i32.cc", out=out_root / "micro/quicksort_i32_200k_x3.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_mul_add_i32.cc", out=out_root / "micro/mul_add_i32_50m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_int128_mul_u64.cc", out=out_root / "micro/int128_mul_u64_2m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memcpy_libc_u8.cc", out=out_root / "micro/mem_copy_libc_u8_4m_x32.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memcpy_small_64b.cc", out=out_root / "micro/mem_copy_small_64b_5m.wasm"),
        BuildUnit(kind="cc", src=repo_root / "wasm/src/cc/micro_memmove_libc_u8.cc", out=out_root / "micro/mem_move_libc_u8_4m_x24.wasm"),

        # Microbenches (WAT, no libc)
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_i32.wat", out=out_root / "micro/loop_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_f32.wat", out=out_root / "micro/loop_f32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_f64.wat", out=out_root / "micro/loop_f64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/loop_i64.wat", out=out_root / "micro/loop_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/bitops_i32_dense.wat", out=out_root / "micro/bitops_i32_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/bitops_i64_dense.wat", out=out_root / "micro/bitops_i64_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/memory_grow_1p_x256.wat", out=out_root / "micro/memory_grow_1p_x256.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/memory_grow_touch_1p_x256.wat", out=out_root / "micro/memory_grow_touch_1p_x256.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/clock_time_get_dense.wat", out=out_root / "wasi/clock_time_get_wat_200k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/fd_write_0len_dense.wat", out=out_root / "wasi/fd_write_0len_wat_100k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/fd_read_0len_dense.wat", out=out_root / "wasi/fd_read_0len_wat_200k.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_sum_i32.wat", out=out_root / "micro/mem_sum_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_sum_u8.wat", out=out_root / "micro/mem_sum_u8.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_fill_i32.wat", out=out_root / "micro/mem_fill_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_fill_u8.wat", out=out_root / "micro/mem_fill_u8.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_copy_i32.wat", out=out_root / "micro/mem_copy_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_copy_u8.wat", out=out_root / "micro/mem_copy_u8_1m_x8.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_stride_i32.wat", out=out_root / "micro/mem_stride_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_unaligned_i32.wat", out=out_root / "micro/mem_unaligned_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_load_store_i64.wat", out=out_root / "micro/mem_load_store_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/mem_unaligned_i64.wat", out=out_root / "micro/mem_unaligned_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/global_dense_i32.wat", out=out_root / "micro/global_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/global_dense_i64.wat", out=out_root / "micro/global_dense_i64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/global_dense_f64.wat", out=out_root / "micro/global_dense_f64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/select_dense_i32.wat", out=out_root / "micro/select_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/select_dense_i64.wat", out=out_root / "micro/select_dense_i64.wasm"),
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
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/call_direct_many_args_i32.wat", out=out_root / "micro/call_direct_many_args_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/call_indirect_many_args_i32.wat", out=out_root / "micro/call_indirect_many_args_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/control_flow_dense_i32.wat", out=out_root / "micro/control_flow_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/br_if_dense_predictable_i32.wat", out=out_root / "micro/br_if_dense_predictable_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/br_if_dense_unpredictable_i32.wat", out=out_root / "micro/br_if_dense_unpredictable_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/br_table_dense_i32.wat", out=out_root / "micro/br_table_dense_i32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/div_sqrt_f32_dense.wat", out=out_root / "micro/div_sqrt_f32_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/divrem_i32_dense.wat", out=out_root / "micro/divrem_i32_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/divrem_i64_dense.wat", out=out_root / "micro/divrem_i64_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/round_f64_dense.wat", out=out_root / "micro/round_f64_dense.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_convolution2d_f32.wat", out=out_root / "science/convolution2d_f32_128x128_x40.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_game_of_life_i32.wat", out=out_root / "science/game_of_life_i32_128x128_x64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_jacobi_2d_f64.wat", out=out_root / "science/jacobi_2d_f64_128x128_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_knapsack_i32.wat", out=out_root / "science/knapsack_i32_160x2048_x64.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_fir_f32.wat", out=out_root / "science/fir_f32_32tap_32k_x24.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_lorenz_f64.wat", out=out_root / "science/lorenz_f64_256x1200.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_levenshtein_i32.wat", out=out_root / "science/levenshtein_i32_96x96_x192.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_monte_carlo_pi_f64.wat", out=out_root / "science/monte_carlo_pi_f64_4m.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_binomial_option_f64.wat", out=out_root / "science/binomial_option_f64_512x48.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_viterbi_i32.wat", out=out_root / "science/viterbi_i32_64state_x4096.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_wave_2d_f32.wat", out=out_root / "science/wave_2d_f32_128x128_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_smith_waterman_i32.wat", out=out_root / "science/smith_waterman_i32_128x128_x160.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_heat3d_f32.wat", out=out_root / "science/heat3d_f32_32x32x32_x32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_biquad_iir_f32.wat", out=out_root / "science/biquad_iir_f32_8sec_64k_x20.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_floyd_warshall_i32.wat", out=out_root / "science/floyd_warshall_i32_64n_x20.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_hopfield_i32.wat", out=out_root / "science/hopfield_i32_192n_x80.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_mass_spring_f32.wat", out=out_root / "science/mass_spring_f32_512n_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_advection_2d_f32.wat", out=out_root / "science/advection_2d_f32_128x128_x80.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_thomas_solver_f32.wat", out=out_root / "science/thomas_solver_f32_64sys_256n_x32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_bitonic_sort_i32.wat", out=out_root / "science/bitonic_sort_i32_2048_x40.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_red_black_sor_f32.wat", out=out_root / "science/red_black_sor_f32_128x128_x120.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_kaczmarz_f32.wat", out=out_root / "science/kaczmarz_f32_256x64_x48.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_rk4_lotka_volterra_f64.wat", out=out_root / "science/rk4_lotka_volterra_f64_256x1200.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_projected_gradient_qp_f32.wat", out=out_root / "science/projected_gradient_qp_f32_96d_x160.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_qubo_anneal_i32.wat", out=out_root / "science/qubo_anneal_i32_128v_x160.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_newton_raphson_f64.wat", out=out_root / "science/newton_raphson_f64_512x120.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_coordinate_descent_l1_f32.wat", out=out_root / "science/coordinate_descent_l1_f32_96d_x180.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_maxcut_local_search_i32.wat", out=out_root / "science/maxcut_local_search_i32_160v_x180.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_halley_root_f64.wat", out=out_root / "science/halley_root_f64_384x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_sinkhorn_f32.wat", out=out_root / "science/sinkhorn_f32_64x64_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_admm_lasso_f32.wat", out=out_root / "science/admm_lasso_f32_96d_x180.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_bfs_frontier_i32.wat", out=out_root / "science/bfs_frontier_i32_2048n_x40.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_auction_assignment_i32.wat", out=out_root / "science/auction_assignment_i32_96x96_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_primal_dual_tv_f32.wat", out=out_root / "science/primal_dual_tv_f32_96x96_x72.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_em_soft_kmeans_f32.wat", out=out_root / "science/em_soft_kmeans_f32_256x8_x72.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_push_relabel_i32.wat", out=out_root / "science/push_relabel_i32_128n_x32.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_finite_volume_burgers_f32.wat", out=out_root / "science/finite_volume_burgers_f32_256n_x160.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_belief_propagation_i32.wat", out=out_root / "science/belief_propagation_i32_96v_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_alpha_expansion_i32.wat", out=out_root / "science/alpha_expansion_i32_96v_x72.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_riemann_euler1d_f32.wat", out=out_root / "science/riemann_euler1d_f32_192n_x96.wasm"),
        BuildUnit(kind="wat", src=repo_root / "wasm/src/wat/science_hmm_forward_backward_f32.wat", out=out_root / "science/hmm_forward_backward_f32_48state_x1536.wasm"),
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
