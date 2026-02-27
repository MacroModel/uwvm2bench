#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, NamedTuple


def variant_key(*, engine: str, runtime: str, mode: str, label: str = "") -> str:
    if label:
        return f"{engine}#{label}:{runtime}:{mode}"
    return f"{engine}:{runtime}:{mode}"


@dataclass(frozen=True)
class EngineVariant:
    engine: str
    runtime: str
    mode: str
    bin: str
    label: str = ""
    cli: str = ""

    @property
    def key(self) -> str:
        return variant_key(engine=self.engine, runtime=self.runtime, mode=self.mode, label=self.label)


@dataclass
class RunResult:
    engine: str
    runtime: str
    mode: str
    label: str
    wasm: str
    bench_kind: str
    bench_tags: list[str]
    ok: bool
    rc: int
    wall_ms: float
    internal_ms: float | None
    metric: str
    metric_kind: str
    metric_ms: float | None
    stdout_tail: str
    stderr_tail: str


TIME_PATTERNS: list[re.Pattern[str]] = [
    re.compile(r"^Elapsed time:\s*(?P<ms>\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
    re.compile(r"^Elapsed:\s*(?P<ms>\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
    re.compile(r"^Time:\s*(?P<ms>\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE),
    re.compile(r"^time:\s*(?P<ms>\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
]


def extract_internal_ms(out: str) -> float | None:
    last: float | None = None
    for pat in TIME_PATTERNS:
        for m in pat.finditer(out):
            try:
                last = float(m.group("ms"))
            except Exception:
                continue
    if last is not None:
        return last
    return None


def tail(s: str, max_chars: int = 800) -> str:
    s = s.strip("\n")
    if len(s) <= max_chars:
        return s
    return s[-max_chars:]


def decode(b: bytes | str | None) -> str:
    if b is None:
        return ""
    if isinstance(b, str):
        return b
    return b.decode("utf-8", errors="replace")


class CmdOut(NamedTuple):
    rc: int
    wall_ms: float
    out: str
    err: str


def run_one(cmd: list[str], cwd: Path, timeout_s: float) -> CmdOut:
    t0 = time.perf_counter()
    try:
        cp = subprocess.run(
            cmd,
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=False,
            timeout=timeout_s,
            check=False,
        )
        wall_ms = (time.perf_counter() - t0) * 1000.0
        return CmdOut(cp.returncode, wall_ms, decode(cp.stdout), decode(cp.stderr))
    except subprocess.TimeoutExpired as e:
        wall_ms = (time.perf_counter() - t0) * 1000.0
        return CmdOut(124, wall_ms, decode(e.stdout), decode(e.stderr))


def geomean(values: Iterable[float]) -> float:
    vals = [v for v in values if v > 0.0 and math.isfinite(v)]
    if not vals:
        return float("nan")
    return math.exp(sum(math.log(v) for v in vals) / len(vals))


def which_or(path: str | None, default: str) -> str | None:
    if path:
        return path
    return shutil.which(default)


def parse_labeled_bin(spec: str) -> tuple[str, str]:
    spec = spec.strip()
    if not spec:
        raise SystemExit("empty --*-bin spec")
    if "=" not in spec:
        return ("", spec)
    label, path = spec.split("=", 1)
    label = label.strip()
    path = path.strip()
    if not label or not path:
        raise SystemExit(f"invalid --*-bin spec (expected label=path): {spec!r}")
    if ":" in label or "#" in label:
        raise SystemExit(f"invalid label in --*-bin spec (must not contain ':' or '#'): {label!r}")
    return (label, path)


def resolve_executable(path: str, *, engine: str) -> str:
    p = os.path.expanduser(path.strip())
    if not p:
        raise SystemExit(f"{engine} binary path is empty")

    # Treat anything that looks like a path as a path.
    if os.path.sep in p or p.startswith("."):
        if Path(p).exists():
            return p
        raise SystemExit(f"{engine} binary not found: {p}")

    found = shutil.which(p)
    if found:
        return found
    if Path(p).exists():
        return p
    raise SystemExit(f"{engine} binary not found in PATH: {p}")


def uniquify_bin_entries(engine: str, entries: list[tuple[str, str]]) -> list[tuple[str, str]]:
    if not entries:
        return entries

    seen: set[str] = set()
    for label, _ in entries:
        if not label:
            continue
        if label in seen:
            raise SystemExit(f"duplicate label for {engine}: {label!r}")
        seen.add(label)

    if len(entries) == 1:
        return entries

    out: list[tuple[str, str]] = []
    empty_used = False
    for idx, (label, path) in enumerate(entries):
        if label:
            out.append((label, path))
            continue
        if not empty_used:
            empty_used = True
            out.append(("", path))
            continue
        auto = str(idx + 1)
        if auto in seen:
            auto = f"auto{idx + 1}"
        base = auto
        n = 2
        while auto in seen or not auto:
            auto = f"{base}_{n}"
            n += 1
        seen.add(auto)
        out.append((auto, path))
    return out


def resolve_bin_entries(
    *,
    engine: str,
    specs: list[str],
    default_cmd: str | None = None,
    default_path: str | None = None,
) -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []
    if specs:
        for spec in specs:
            label, path = parse_labeled_bin(spec)
            entries.append((label, resolve_executable(path, engine=engine)))
        return uniquify_bin_entries(engine, entries)

    if default_path:
        p = os.path.expanduser(default_path)
        if Path(p).exists():
            return [("", p)]

    if default_cmd:
        found = shutil.which(default_cmd)
        if found:
            return [("", found)]

    return []


def detect_wamr_cli_kind(bin_path: str, *, cwd: Path, timeout_s: float = 2.0) -> str:
    """
    Detect whether an iwasm binary is the full CLI (supports --dir/args) or a minimal CLI.

    Returns: "full" or "minimal"
    """

    cp = run_one([bin_path, "-h"], cwd, timeout_s)
    text = (cp.out + "\n" + cp.err).strip()

    if "Usage: iwasm" in text:
        return "full"
    if "Required arguments:" in text:
        return "minimal"
    if "--dir=<dir>" in text or "--dir=" in text or "\n  --dir" in text:
        return "full"

    # Conservative fallback: treat unknown output as minimal.
    return "minimal"


def find_wasms(root: Path) -> list[Path]:
    skip_parts = {".git", "__pycache__", ".venv", "logs", "cache"}
    wasms: list[Path] = []
    for p in root.rglob("*.wasm"):
        if set(p.parts) & skip_parts:
            continue
        wasms.append(p)
    return sorted(wasms)


def classify_bench(wasm_rel: str) -> tuple[str, list[str]]:
    """
    Classify a benchmark into a primary "kind" plus optional tags.

    Kinds are intended to match high-level performance characteristics:
      - compute_dense
      - memory_dense
      - io_dense
      - syscall_dense
      - local_dense
      - operand_stack_dense
      - call_dense
      - control_flow_dense
      - unknown
    """

    rel = wasm_rel.replace("\\", "/").lower()
    name = Path(rel).name

    tags: set[str] = set()

    def ret(primary: str) -> tuple[str, list[str]]:
        tags.add(primary)
        return (primary, sorted(tags))

    # Numeric flavor tags (orthogonal to primary kind).
    if any(k in name for k in ("f32", "f64")):
        tags.add("float_dense")
    if any(k in name for k in ("i8", "u8", "i16", "u16", "i32", "i64", "u32", "u64")):
        tags.add("int_dense")

    # Generated WASI corpus under wasm/corpus/.
    if rel.startswith("wasi/"):
        tags.add("wasi")
        tags.add("syscall_dense")
        if "file_rw" in name or "small_io" in name:
            tags.add("io_dense")
            return ret("io_dense")
        if "readv" in name or "writev" in name:
            tags.add("io_dense")
            return ret("io_dense")
        if "pread" in name or "pwrite" in name:
            tags.add("io_dense")
            return ret("io_dense")
        if "seek_read" in name:
            tags.add("io_dense")
            return ret("io_dense")
        return ret("syscall_dense")

    if rel.startswith("micro/"):
        tags.add("micro")
        if "global_dense" in name:
            tags.add("compute_dense")
            tags.add("global_dense")
            return ret("compute_dense")
        if "select_dense" in name:
            tags.add("compute_dense")
            tags.add("operand_stack_dense")
            return ret("compute_dense")
        if "local_dense" in name:
            tags.add("compute_dense")
            return ret("local_dense")
        if "operand_stack_dense" in name:
            tags.add("compute_dense")
            return ret("operand_stack_dense")
        if "call_direct" in name:
            tags.add("compute_dense")
            return ret("call_dense")
        if "call_dense" in name:
            tags.add("compute_dense")
            return ret("call_dense")
        if "call_indirect" in name or "indirect_call" in name:
            tags.add("compute_dense")
            return ret("call_dense")
        if "br_table" in name:
            tags.add("compute_dense")
            return ret("control_flow_dense")
        if "br_if" in name:
            tags.add("compute_dense")
            return ret("control_flow_dense")
        if "control_flow_dense" in name:
            tags.add("compute_dense")
            return ret("control_flow_dense")
        if "switch" in name:
            tags.add("compute_dense")
            return ret("control_flow_dense")
        if "mem_sum" in name or "mem_fill" in name or "mem_copy" in name:
            return ret("memory_dense")
        if name.startswith("mem_") or "mem_" in name:
            return ret("memory_dense")
        if "pointer_chase" in name:
            return ret("memory_dense")
        if "random_access" in name:
            return ret("memory_dense")
        if "memory_grow" in name:
            return ret("memory_dense")
        if "malloc" in name or "alloc" in name:
            return ret("memory_dense")
        if "rle_" in name or name.startswith("rle"):
            tags.add("control_flow_dense")
            return ret("memory_dense")
        if "utf8" in name:
            tags.add("control_flow_dense")
            return ret("control_flow_dense")
        if "json" in name:
            tags.add("control_flow_dense")
            tags.add("memory_dense")
            return ret("control_flow_dense")
        if "varint" in name:
            tags.add("control_flow_dense")
            tags.add("memory_dense")
            return ret("control_flow_dense")
        tags.add("compute_dense")
        return ret("compute_dense")

    if rel.startswith("crypto/"):
        tags.add("crypto")
        tags.add("int_dense")
        return ret("compute_dense")

    if rel.startswith("science/"):
        tags.add("science")
        tags.add("compute_dense")
        if "daxpy" in name:
            tags.add("memory_dense")
            return ("memory_dense", sorted(tags))
        if any(k in name for k in ("mandelbrot", "sieve", "gcd")):
            tags.add("control_flow_dense")
            return ("control_flow_dense", sorted(tags))
        return ("compute_dense", sorted(tags))

    if rel.startswith("db/"):
        tags.add("db")
        tags.add("int_dense")
        tags.add("memory_dense")
        tags.add("control_flow_dense")
        return ("memory_dense", sorted(tags))

    if rel.startswith("vm/"):
        tags.add("vm")
        tags.add("int_dense")
        tags.add("control_flow_dense")
        tags.add("call_dense")
        return ("control_flow_dense", sorted(tags))

    # Legacy flat corpus (extreme few-variable microbenches).
    if name.startswith(("call", "inline")) or "call_" in name or name.startswith("bench_call"):
        return ret("call_dense")
    if (
        name.startswith(("branch", "br_table", "br_"))
        or "branch" in name
        or name.startswith("bench_branchy")
        or "_br_" in name
        or "br_ret" in name
    ):
        return ret("control_flow_dense")
    if name.startswith("mem_") or "mem_" in name or name.startswith("stack_spill"):
        return ret("memory_dense")
    if name.startswith(("blake", "sha", "siphash", "chacha")) or name in {"blake2s.wasm", "chacha20.wasm", "sha512.wasm", "sha512_constant.wasm", "siphash.wasm"}:
        tags.add("crypto")
        return ret("compute_dense")
    if name.startswith(
        (
            "arith_",
            "bench_compute_",
            "bitops_",
            "divrem_",
            "deepstack",
            "inloop_deepstack",
            "stack_reduce",
            "keepstack",
            "local_",
            "global_",
            "inline_step",
            "inline_empty",
            "aabb",
        )
    ):
        if name.startswith("local_") or "local_" in name:
            tags.add("compute_dense")
            return ret("local_dense")
        if name.startswith(("deepstack", "inloop_deepstack", "stack_reduce", "keepstack")):
            tags.add("compute_dense")
            return ret("operand_stack_dense")
        return ret("compute_dense")
    if name in {"coremark.wasm", "python.wasm"}:
        tags.add("control_flow_dense")
        tags.add("vm")
        return ("control_flow_dense", sorted(tags))

    return ret("unknown")


def wasm3_cmd(bin_path: str, wasm_rel: str, mode: str) -> list[str]:
    cmd = [bin_path]
    if mode == "full":
        cmd.append("--compile")
    cmd.append(wasm_rel)
    return cmd


def uwvm2_cmd(bin_path: str, wasm_rel: str, runtime: str, mode: str) -> list[str]:
    return [
        bin_path,
        "-Rcc",
        runtime,
        "-Rcm",
        mode,
        "-I1dir",
        ".",
        ".",
        "--",
        wasm_rel,
    ]


def wamr_cmd(bin_path: str, wasm_rel: str) -> list[str]:
    raise AssertionError("call wamr_cmd_with_cli() instead")


def wamr_cmd_with_cli(bin_path: str, wasm_rel: str, cli: str) -> list[str]:
    if cli == "minimal":
        # Minimal WAMR iwasm CLI (product-mini variants) uses: -f <wasm> -d <dir>
        return [bin_path, "-f", wasm_rel, "-d", "."]
    # Full WAMR iwasm CLI (recommended): runtime options are not forwarded into guest argv.
    return [bin_path, "--dir=.", wasm_rel]


def wasmtime_cmd(bin_path: str, wasm_rel: str) -> list[str]:
    return [bin_path, "run", "--dir", ".", wasm_rel]


def wasmer_cmd(bin_path: str, wasm_rel: str) -> list[str]:
    return [bin_path, "run", "--dir", ".", wasm_rel]


def wasmedge_cmd(bin_path: str, wasm_rel: str, runtime: str) -> list[str]:
    # WasmEdge uses guest:host mapping; ".:." is safe (same either way).
    cmd = [bin_path, "--dir", ".:.", wasm_rel]
    if runtime == "jit":
        return [bin_path, "--enable-jit", "--dir", ".:.", wasm_rel]
    if runtime == "int":
        return [bin_path, "--force-interpreter", "--dir", ".:.", wasm_rel]
    raise ValueError(f"unsupported wasmedge runtime: {runtime}")


def wavm_cmd(bin_path: str, wasm_rel: str) -> list[str]:
    # WAVM's WASI flags differ across builds; this is a best-effort default.
    return [bin_path, "run", "--mount-root", ".", wasm_rel]


def supported_variants(
    *,
    engine: str,
    bin_path: str,
    label: str = "",
    cli: str = "",
    runtimes: list[str],
    modes: list[str],
) -> list[EngineVariant]:
    variants: list[EngineVariant] = []

    # NOTE: be conservative. If we can't *control* a dimension, mark it unsupported.
    if engine == "wasm3":
        supp_r = {"int"}
        supp_m = {"full", "lazy"}
    elif engine == "uwvm2":
        # The uwvm2 binary used in this repo only supports: -Rcc int -Rcm full.
        # Be conservative and skip unsupported combinations.
        supp_r = {"int"}
        supp_m = {"full"}
    elif engine == "wamr":
        supp_r = {"int"}
        supp_m = {"full"}
    elif engine == "wasmtime":
        supp_r = {"jit"}
        # Mode semantics:
        # - lazy: run the wasm directly (compilation happens on first run)
        # - full: precompile (wasmtime compile) then run with --allow-precompiled
        supp_m = {"full", "lazy"}
    elif engine == "wasmer":
        supp_r = {"jit"}
        supp_m = {"full"}
    elif engine == "wasmedge":
        supp_r = {"int", "jit"}
        supp_m = {"full"}
    elif engine == "wavm":
        supp_r = {"jit"}
        supp_m = {"full"}
    else:
        raise ValueError(f"unknown engine: {engine}")

    for r in runtimes:
        if r not in supp_r:
            continue
        for m in modes:
            if m not in supp_m:
                continue
            variants.append(EngineVariant(engine=engine, runtime=r, mode=m, bin=bin_path, label=label, cli=cli))
    return variants


def build_cmd(variant: EngineVariant, wasm_rel: str) -> list[str]:
    eng = variant.engine
    if eng == "wasm3":
        return wasm3_cmd(variant.bin, wasm_rel, variant.mode)
    if eng == "uwvm2":
        return uwvm2_cmd(variant.bin, wasm_rel, variant.runtime, variant.mode)
    if eng == "wamr":
        return wamr_cmd_with_cli(variant.bin, wasm_rel, variant.cli)
    if eng == "wasmtime":
        # "full" is handled specially (precompile+run) in the main loop.
        # For "lazy", run the wasm directly.
        if variant.mode == "lazy":
            return wasmtime_cmd(variant.bin, wasm_rel)
        return [variant.bin, "run", "--dir", ".", wasm_rel]
    if eng == "wasmer":
        return wasmer_cmd(variant.bin, wasm_rel)
    if eng == "wasmedge":
        return wasmedge_cmd(variant.bin, wasm_rel, variant.runtime)
    if eng == "wavm":
        return wavm_cmd(variant.bin, wasm_rel)
    raise ValueError(f"unknown engine: {eng}")


def _metric_value(r: RunResult, metric: str) -> float | None:
    if metric == "wall":
        return r.wall_ms
    if metric == "internal":
        return r.internal_ms
    if metric == "auto":
        return r.internal_ms if r.internal_ms is not None else r.wall_ms
    raise ValueError(f"unknown metric: {metric}")


def metric_kind_and_value(*, wall_ms: float, internal_ms: float | None, metric: str) -> tuple[str, float | None]:
    if metric == "wall":
        return ("wall", wall_ms)
    if metric == "internal":
        return ("internal", internal_ms)
    if metric == "auto":
        if internal_ms is not None:
            return ("internal", internal_ms)
        return ("wall", wall_ms)
    raise ValueError(f"unknown metric: {metric}")


def summarize(results: list[RunResult], baseline_key: str, *, metric: str) -> dict[str, object]:
    by_key: dict[str, list[RunResult]] = {}
    for r in results:
        key = variant_key(engine=r.engine, runtime=r.runtime, mode=r.mode, label=r.label)
        by_key.setdefault(key, []).append(r)

    # Compute per-config stats
    stats: dict[str, dict[str, object]] = {}
    for key, rs in sorted(by_key.items()):
        vals: list[float] = []
        ok_rc = 0
        ok_metric = 0
        for r in rs:
            if not r.ok:
                continue
            ok_rc += 1
            v = _metric_value(r, metric)
            if v is None:
                continue
            if v > 0.0 and math.isfinite(v):
                ok_metric += 1
                vals.append(v)
        stats[key] = {
            "ok_rc": ok_rc,
            "ok_metric": ok_metric,
            "total": len(rs),
            "ms_geomean": geomean(vals),
            "ms_median": statistics.median(vals) if vals else float("nan"),
        }

    # Ratios vs baseline (pairwise intersection for fairness)
    base_rs = [r for r in by_key.get(baseline_key, []) if r.ok]
    base_vals: dict[str, float] = {}
    for r in base_rs:
        v = _metric_value(r, metric)
        if v is None or not (v > 0.0 and math.isfinite(v)):
            continue
        base_vals[r.wasm] = v
    ratios: dict[str, dict[str, object]] = {}
    for key, rs in sorted(by_key.items()):
        if key == baseline_key:
            continue
        cur_vals: dict[str, float] = {}
        for r in rs:
            if not r.ok:
                continue
            v = _metric_value(r, metric)
            if v is None or not (v > 0.0 and math.isfinite(v)):
                continue
            cur_vals[r.wasm] = v
        common = [w for w in base_vals.keys() if w in cur_vals]
        pair: list[float] = []
        for w in common:
            a = cur_vals[w]
            b = base_vals[w]
            if a > 0.0 and b > 0.0 and math.isfinite(a) and math.isfinite(b):
                pair.append(a / b)
        ratios[key] = {
            "common_ok": len(common),
            "ratio_geomean": geomean(pair),
            "ratio_median": statistics.median(pair) if pair else float("nan"),
        }

    return {"metric": metric, "stats": stats, "ratios_vs_baseline": ratios}


def wasmtime_precompile_path(root: Path, wasm_rel: str, *, bin_path: str) -> Path:
    wasm_path = (root / wasm_rel).resolve()
    wasm_st = wasm_path.stat()
    bin_real = Path(bin_path).resolve()
    bin_st = bin_real.stat()
    # Include the wasmtime binary identity so multi-binary comparisons don't share precompiled artifacts.
    key = f"{bin_real}|{bin_st.st_size}|{bin_st.st_mtime_ns}|{wasm_rel}|{wasm_st.st_size}|{wasm_st.st_mtime_ns}".encode(
        "utf-8", errors="strict"
    )
    h = hashlib.sha256(key).hexdigest()[:20]
    out_dir = root / "cache" / "u2bench" / "wasmtime"
    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir / f"{h}.cwasm"


def run_wasmtime_full(bin_path: str, *, root: Path, wasm_rel: str, timeout_s: float) -> CmdOut:
    out_path = wasmtime_precompile_path(root, wasm_rel, bin_path=bin_path)
    compile_cmd = [bin_path, "compile", wasm_rel, "-o", str(out_path)]
    run_cmd = [bin_path, "run", "--allow-precompiled", "--dir", ".", str(out_path)]

    # Compile if missing.
    compile_wall_ms = 0.0
    compile_out = ""
    compile_err = ""
    if not out_path.exists():
        cp_c = run_one(compile_cmd, root, timeout_s)
        compile_wall_ms = cp_c.wall_ms
        compile_out = cp_c.out
        compile_err = cp_c.err
        if cp_c.rc != 0:
            return cp_c

    cp_r = run_one(run_cmd, root, timeout_s)
    return CmdOut(
        cp_r.rc,
        compile_wall_ms + cp_r.wall_ms,
        cp_r.out,
        (compile_out + "\n" + compile_err + "\n" + cp_r.err).strip("\n"),
    )


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()

    # Engines
    ap.add_argument("--add-wasm3", action="store_true")
    ap.add_argument("--add-uwvm2", action="store_true")
    ap.add_argument("--add-wamr", action="store_true")
    ap.add_argument("--add-wasmtime", action="store_true")
    ap.add_argument("--add-wasmer", action="store_true")
    ap.add_argument("--add-wasmedge", action="store_true")
    ap.add_argument("--add-wavm", action="store_true")

    ap.add_argument("--wasm3-bin", action="append", default=[], help="repeatable; optionally label=PATH")
    ap.add_argument("--uwvm2-bin", action="append", default=[], help="repeatable; optionally label=PATH")
    ap.add_argument("--wamr-bin", action="append", default=[], help="repeatable; optionally label=PATH (iwasm)")
    ap.add_argument("--wasmtime-bin", action="append", default=[], help="repeatable; optionally label=PATH")
    ap.add_argument("--wasmer-bin", action="append", default=[], help="repeatable; optionally label=PATH")
    ap.add_argument("--wasmedge-bin", action="append", default=[], help="repeatable; optionally label=PATH")
    ap.add_argument("--wavm-bin", action="append", default=[], help="repeatable; optionally label=PATH")

    # Filters
    ap.add_argument("--runtime", action="append", choices=["int", "jit", "tiered"], default=[])
    ap.add_argument("--mode", action="append", choices=["full", "lazy"], default=[])

    # Corpus
    ap.add_argument("--root", default="wasm/corpus", help="directory to scan for wasm files (relative to CWD ok)")
    ap.add_argument("--timeout", type=float, default=25.0, help="timeout per run (seconds)")
    ap.add_argument("--max-wasm", type=int, default=0, help="limit number of wasm files (0 = all)")
    ap.add_argument(
        "--bench-kind",
        action="append",
        choices=[
            "compute_dense",
            "memory_dense",
            "io_dense",
            "syscall_dense",
            "local_dense",
            "operand_stack_dense",
            "call_dense",
            "control_flow_dense",
            "unknown",
        ],
        default=[],
        help="filter wasm corpus by primary bench kind (repeatable)",
    )
    ap.add_argument(
        "--bench-tag",
        action="append",
        default=[],
        help="filter wasm corpus by tag (repeatable; OR semantics, e.g. --bench-tag crypto --bench-tag syscall_dense)",
    )

    # Output
    ap.add_argument("--out", default="logs/results.json")
    ap.add_argument("--baseline", default="", help="baseline key, e.g. wasm3:int:full or uwvm2#old:int:full (default prefers wasm3:int:full)")
    ap.add_argument(
        "--metric",
        choices=["wall", "internal", "auto"],
        default="auto",
        help="metric for summary/ratios/plot: wall, internal (wasm-reported Time: .. ms), or auto (prefer internal, else wall)",
    )

    # Plot
    ap.add_argument("--plot", action="store_true", help="render a bar chart (requires matplotlib)")
    ap.add_argument("--plot-out", default="logs/plot.png")

    args = ap.parse_args(argv)

    selected_engines = [
        ("wasm3", args.add_wasm3),
        ("uwvm2", args.add_uwvm2),
        ("wamr", args.add_wamr),
        ("wasmtime", args.add_wasmtime),
        ("wasmer", args.add_wasmer),
        ("wasmedge", args.add_wasmedge),
        ("wavm", args.add_wavm),
    ]
    selected = [name for name, on in selected_engines if on]
    if not selected:
        raise SystemExit("no engine selected: pass at least one --add-<engine> flag")
    if not args.runtime:
        raise SystemExit("no runtime selected: pass at least one --runtime={int,jit,tiered}")
    if not args.mode:
        raise SystemExit("no mode selected: pass at least one --mode={full,lazy}")

    root = Path(args.root).resolve()
    if not root.is_dir():
        raise SystemExit(f"root not found: {root}")

    wasms = find_wasms(root)
    # Pre-classify for filtering and reporting.
    wasm_items: list[tuple[Path, str, str, list[str]]] = []
    for w in wasms:
        rel = os.path.relpath(w, root)
        kind, tags = classify_bench(rel)
        wasm_items.append((w, rel, kind, tags))

    if args.bench_kind:
        keep_kinds = set(args.bench_kind)
        wasm_items = [it for it in wasm_items if it[2] in keep_kinds]
    if args.bench_tag:
        wanted = {t.strip().lower() for t in args.bench_tag if t.strip()}
        if wanted:
            wasm_items = [it for it in wasm_items if wanted & set(it[3])]

    if args.max_wasm and args.max_wasm > 0:
        wasm_items = wasm_items[: args.max_wasm]

    wasms = [it[0] for it in wasm_items]
    bench_meta: dict[str, dict[str, object]] = {it[1]: {"kind": it[2], "tags": it[3]} for it in wasm_items}
    if not wasms:
        raise SystemExit(f"no wasm files found under: {root}")
    probe_rel = os.path.relpath(wasms[0], root)

    # Resolve binaries (each engine can have multiple bins, e.g. two uwvm2 builds).
    bins: dict[str, list[tuple[str, str]]] = {}
    wamr_cli_by_bin: dict[str, str] = {}
    warnings: list[str] = []

    if args.add_wasm3:
        entries = resolve_bin_entries(engine="wasm3", specs=args.wasm3_bin, default_cmd="wasm3")
        if not entries:
            raise SystemExit("wasm3 not found: pass --wasm3-bin or install wasm3 in PATH")
        bins["wasm3"] = entries
    if args.add_uwvm2:
        entries = resolve_bin_entries(engine="uwvm2", specs=args.uwvm2_bin, default_cmd="uwvm")
        if not entries:
            raise SystemExit("uwvm not found: pass --uwvm2-bin or install uwvm in PATH")
        bins["uwvm2"] = entries
    if args.add_wamr:
        default_iwasm = "/Users/liyinan/Documents/MacroModel/src/wasm-micro-runtime/build/iwasm-productmini-fastinterp-clang/iwasm"
        if not Path(default_iwasm).exists():
            default_iwasm = "/Users/liyinan/Documents/MacroModel/src/wasm-micro-runtime/build/iwasm-fastinterp-clang/iwasm"
        entries = resolve_bin_entries(engine="wamr", specs=args.wamr_bin, default_path=default_iwasm, default_cmd="iwasm")
        if not entries:
            raise SystemExit("iwasm not found: pass --wamr-bin or install iwasm in PATH")
        bins["wamr"] = entries
        for label, bin_path in entries:
            kind = detect_wamr_cli_kind(bin_path, cwd=root)
            wamr_cli_by_bin[bin_path] = kind
            if kind == "minimal":
                lab = f" ({label})" if label else ""
                warnings.append(
                    "warning: WAMR iwasm minimal CLI detected"
                    f"{lab}: guest argv support is limited; argv-dependent workloads may become invalid. "
                    "Prefer a full iwasm build that supports --dir (e.g. iwasm-productmini-*) via --wamr-bin."
                )
    if args.add_wasmtime:
        entries = resolve_bin_entries(engine="wasmtime", specs=args.wasmtime_bin, default_cmd="wasmtime")
        if not entries:
            raise SystemExit("wasmtime not found: pass --wasmtime-bin or install wasmtime in PATH")
        bins["wasmtime"] = entries
    if args.add_wasmer:
        entries = resolve_bin_entries(engine="wasmer", specs=args.wasmer_bin, default_cmd="wasmer")
        if not entries:
            raise SystemExit("wasmer not found: pass --wasmer-bin or install wasmer in PATH")
        bins["wasmer"] = entries
    if args.add_wasmedge:
        entries = resolve_bin_entries(engine="wasmedge", specs=args.wasmedge_bin, default_cmd="wasmedge")
        if not entries:
            raise SystemExit("wasmedge not found: pass --wasmedge-bin or install wasmedge in PATH")
        bins["wasmedge"] = entries
    if args.add_wavm:
        entries = resolve_bin_entries(engine="wavm", specs=args.wavm_bin, default_cmd="wavm")
        if not entries:
            raise SystemExit("wavm not found: pass --wavm-bin or install wavm in PATH")
        bins["wavm"] = entries

    # Build variants
    variants: list[EngineVariant] = []
    skipped: list[str] = []
    for eng in selected:
        eng_vs: list[EngineVariant] = []
        for label, bin_path in bins.get(eng, []):
            cli = wamr_cli_by_bin.get(bin_path, "") if eng == "wamr" else ""
            eng_vs.extend(
                supported_variants(engine=eng, bin_path=bin_path, label=label, cli=cli, runtimes=args.runtime, modes=args.mode)
            )
        if not eng_vs:
            skipped.append(eng)
        variants.extend(eng_vs)

    # Preflight: drop variants that an engine explicitly reports as unsupported.
    pruned: list[str] = []
    kept: list[EngineVariant] = []
    for v in variants:
        if v.engine != "uwvm2":
            kept.append(v)
            continue

        cp = run_one(build_cmd(v, probe_rel), root, min(args.timeout, 10.0))
        msg = (cp.out + "\n" + cp.err).lower()
        if cp.rc != 0 and "not currently supported" in msg:
            pruned.append(v.key)
            continue
        kept.append(v)
    variants = kept

    if not variants:
        raise SystemExit("no runnable engine variants after applying --runtime/--mode filters")

    print(f"root: {root}")
    print(f"wasm files: {len(wasms)}")
    print("variants:")
    for v in variants:
        print(f"  - {v.key}  ({v.bin})")
    for w in warnings:
        print(w)
    if skipped:
        print("skipped engines (no supported variants for current filters): " + ", ".join(skipped))
    if pruned:
        print("pruned variants (engine reported unsupported): " + ", ".join(pruned))
    print("", flush=True)

    baseline = args.baseline
    if not baseline:
        pref = "wasm3:int:full"
        baseline = pref if any(v.key == pref for v in variants) else variants[0].key
    if not any(v.key == baseline for v in variants):
        raise SystemExit(f"baseline not present in variants: {baseline}")

    results: list[RunResult] = []

    total_runs = len(wasms) * len(variants)
    run_idx = 0
    for wasm_idx, wasm in enumerate(wasms, start=1):
        rel = os.path.relpath(wasm, root)
        print(f"[{wasm_idx}/{len(wasms)}] {rel}", flush=True)
        for v in variants:
            run_idx += 1
            cmd = build_cmd(v, rel)
            if v.engine == "wasmtime" and v.mode == "full":
                cp = run_wasmtime_full(v.bin, root=root, wasm_rel=rel, timeout_s=args.timeout)
            else:
                cp = run_one(cmd, root, args.timeout)

            internal = extract_internal_ms(cp.out + "\n" + cp.err)
            metric_kind, metric_ms = metric_kind_and_value(wall_ms=cp.wall_ms, internal_ms=internal, metric=args.metric)
            bm = bench_meta.get(rel)
            if bm:
                bench_kind = str(bm["kind"])
                bench_tags = list(bm["tags"])  # type: ignore[arg-type]
            else:
                bench_kind, bench_tags = classify_bench(rel)
            results.append(
                RunResult(
                    engine=v.engine,
                    runtime=v.runtime,
                    mode=v.mode,
                    label=v.label,
                    wasm=rel,
                    bench_kind=bench_kind,
                    bench_tags=bench_tags,
                    ok=(cp.rc == 0),
                    rc=cp.rc,
                    wall_ms=cp.wall_ms,
                    internal_ms=internal,
                    metric=args.metric,
                    metric_kind=metric_kind,
                    metric_ms=metric_ms,
                    stdout_tail=tail(cp.out),
                    stderr_tail=tail(cp.err),
                )
            )

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "meta": {
            "root": str(root),
            "timeout_s": args.timeout,
            "count_wasm": len(wasms),
            "bench_meta": bench_meta,
            "bench_kind_semantics": {
                "compute_dense": "dense arithmetic/crypto/science compute",
                "memory_dense": "memory bandwidth / data-structure heavy",
                "io_dense": "WASI filesystem I/O heavy",
                "syscall_dense": "WASI syscall overhead heavy",
                "local_dense": "local.get/local.set heavy",
                "operand_stack_dense": "deep operand-stack manipulation heavy",
                "call_dense": "function call overhead heavy",
                "control_flow_dense": "branch/jump/switch heavy",
                "unknown": "uncategorized",
            },
            "variants": [asdict(v) for v in variants],
            "baseline": baseline,
            "metric": args.metric,
            "metric_semantics": {
                "wall_ms": "harness wall-clock time (includes engine startup/compilation overheads)",
                "internal_ms": "wasm-reported time extracted from stdout/stderr via Time/time patterns (excludes host-side overheads)",
                "metric": "metric requested for summary/ratios/plot; auto prefers internal_ms when available, else wall_ms",
                "per_result": "each result includes metric_kind (wall|internal) and metric_ms (value used for this run under the chosen metric)",
            },
            "date_epoch": time.time(),
            "argv": sys.argv,
        },
        "results": [asdict(r) for r in results],
    }
    out_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    summ = summarize(results, baseline, metric=args.metric)
    metric_label = args.metric
    if args.metric == "auto":
        metric_label = "auto (internal preferred)"

    print(f"\n=== Summary ({metric_label}) ===")
    print(f"baseline: {baseline}")
    for key, s in summ["stats"].items():  # type: ignore[union-attr]
        print(
            f"{key}: ok {s['ok_rc']}/{s['total']}, metric_ok {s['ok_metric']}/{s['total']}, "
            f"geomean {s['ms_geomean']:.3f} ms, median {s['ms_median']:.3f} ms"
        )

    print(f"\n=== Ratios vs baseline ({metric_label}, variant/baseline, lower is faster) ===")
    for key, r in summ["ratios_vs_baseline"].items():  # type: ignore[union-attr]
        print(f"{key}: common_ok {r['common_ok']}, geomean {r['ratio_geomean']:.4f}, median {r['ratio_median']:.4f}")
    print(f"\nwrote: {out_path}")

    # Additional summary split by bench kind.
    kinds = sorted({r.bench_kind for r in results})
    if kinds:
        print(f"\n=== Summary by bench_kind ({metric_label}) ===")
        print(f"baseline: {baseline}")
        for kind in kinds:
            sub = [r for r in results if r.bench_kind == kind]
            wasm_count = len({r.wasm for r in sub})
            print(f"\n[{kind}] wasm {wasm_count}")
            ssub = summarize(sub, baseline, metric=args.metric)
            for key, s in ssub["stats"].items():  # type: ignore[union-attr]
                print(
                    f"{key}: ok {s['ok_rc']}/{s['total']}, metric_ok {s['ok_metric']}/{s['total']}, "
                    f"geomean {s['ms_geomean']:.3f} ms, median {s['ms_median']:.3f} ms"
                )
            print(f"ratios vs baseline ({metric_label}, variant/baseline, lower is faster):")
            for key, r in ssub["ratios_vs_baseline"].items():  # type: ignore[union-attr]
                print(f"  {key}: common_ok {r['common_ok']}, geomean {r['ratio_geomean']:.4f}, median {r['ratio_median']:.4f}")

    if args.plot:
        try:
            import matplotlib.pyplot as plt  # type: ignore[import-not-found]
        except Exception as e:
            print(f"plot skipped: matplotlib not available: {e}")
            return 0

        ratios = summ["ratios_vs_baseline"]  # type: ignore[assignment]
        labels: list[str] = []
        values: list[float] = []
        for v in variants:
            if v.key == baseline:
                continue
            rv = ratios.get(v.key)
            if not rv:
                continue
            val = float(rv["ratio_geomean"])
            if math.isfinite(val) and val > 0:
                labels.append(v.key)
                values.append(val)

        if labels:
            order = sorted(range(len(labels)), key=lambda i: values[i])
            labels = [labels[i] for i in order]
            values = [values[i] for i in order]

            fig_h = max(4.0, 0.35 * len(labels) + 1.0)
            fig, ax = plt.subplots(figsize=(12, fig_h))
            ax.barh(labels, values)
            ax.axvline(1.0, color="black", linewidth=1.0)
            ax.set_xlabel(f"geomean {metric_label} ratio vs baseline ({baseline})")
            ax.set_title(f"u2bench ratios ({metric_label}, lower is faster)")
            ax.invert_yaxis()
            fig.tight_layout()

            plot_out = Path(args.plot_out)
            plot_out.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(plot_out)
            print(f"plot: {plot_out}")
        else:
            print("plot skipped: no comparable ratios")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
