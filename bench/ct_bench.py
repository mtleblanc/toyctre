#!/usr/bin/env python3
"""Compile-time benchmark for the compile-time regex parser.

Generates one small translation unit per benchmark case and measures, for each
compiler and each source tree:

  wall time      hyperfine, front end only (-fsyntax-only). Median, min, sigma.
  peak RSS       /usr/bin/time; tracks the size of the types the parser builds.
  deterministic  GCC: -fmem-report GC memory in use at exit ("ggc").
                 Clang: -Xclang -print-stats class template specializations
                 ("specs") and total types ("types").

Everything except the baseline row is reported net of the baseline TU (the
headers alone), so rows show the cost of the regex itself.

Wall time is noisy at the ~10ms level; the deterministic counts are identical
run to run, so a small change in them is real.

Examples:
  bench/ct_bench.py                          # current tree, both compilers
  bench/ct_bench.py --rev HEAD               # HEAD vs working tree
  bench/ct_bench.py --rev base=HEAD~3 --tree new=.
  bench/ct_bench.py --quick --compiler g++   # fewer sizes and runs
  bench/ct_bench.py --counts-only --quick    # fastest: no timing, just counts
  bench/ct_bench.py --case seq --case nest   # only some series
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Benchmark cases
# ---------------------------------------------------------------------------


def seq(n: int) -> str:  # concatenation: abcabc... of length n
    return ("abc" * n)[:n]


def alt(n: int) -> str:  # alternation: a|b|c|... of length n
    return "|".join("abc"[i % 3] for i in range((n + 1) // 2))


def nest(n: int) -> str:  # n nested groups: ((((a))))
    return "(" * n + "a" + ")" * n


def star(n: int) -> str:  # repetitions: a*b?c+a*... of length n
    ops = "*?+"
    return "".join("abc"[i % 3] + ops[i % 3] for i in range(n // 2))


SERIES = {
    "seq": (seq, [32, 128, 512]),
    "alt": (alt, [32, 128, 512]),
    "nest": (nest, [8, 32, 64]),
    "star": (star, [32, 128, 512]),
}
QUICK_SIZES = {"seq": [32, 128], "alt": [32, 128], "nest": [8, 32],
               "star": [32, 128]}

# A handful of ordinary-looking regexes, parsed and matched in one TU, as a
# stand-in for "a typical user file". Matching instantiates the matcher too.
REALISTIC = [
    ("(a|b)*abb", "aababb"),
    ("(0|1(01*0)*1)*", "110"),
    ("(a|ab)(b|bb)*a?", "abbba"),
    ("((ab)*|b+)a", "ababa"),
    ("(a(b|c)*)+c", "abcac"),
    ("x(y|z)+\\*w?", "xyzz*w"),
    ("(hello|help|helium)( world)?", "help world"),
    ("(a?b?)+c", "abbac"),
]

PRELUDE = '#include "toyctre/compile_time.hpp"\n\n'


def cpp_string(s: str) -> str:
    return 'R"__re__(' + s + ')__re__"'


def parse_tu(regex: str) -> str:
    return (PRELUDE +
            f"using R = re::ct::compile_time_regex_t<"
            f"{cpp_string(regex)}>;\n"
            "static_assert(re::ct::IsRegex<R>);\n")


def realistic_tu() -> str:
    lines = [PRELUDE]
    for regex, text in REALISTIC:
        lines.append(f"static_assert(re::ct::match<"
                     f"{cpp_string(regex)}>({cpp_string(text)}));\n")
    return "".join(lines)


@dataclass
class Case:
    name: str
    source: str
    is_baseline: bool = False


def build_cases(selected: list[str] | None, quick: bool) -> list[Case]:
    cases = [Case("baseline", PRELUDE, is_baseline=True)]
    for name, (gen, sizes) in SERIES.items():
        if selected and name not in selected:
            continue
        for n in (QUICK_SIZES[name] if quick else sizes):
            cases.append(Case(f"{name} {n}", parse_tu(gen(n))))
    if not selected or "realistic" in selected:
        cases.append(Case("realistic", realistic_tu()))
    return cases


# ---------------------------------------------------------------------------
# Source trees
# ---------------------------------------------------------------------------


@dataclass
class Tree:
    label: str
    include: Path


def tree_from_path(label: str, path: str) -> Tree:
    p = Path(path).resolve()
    if (p / "include" / "toyctre").is_dir():
        p = p / "include"
    if not (p / "toyctre" / "compile_time.hpp").is_file():
        sys.exit(f"{path}: no toyctre/compile_time.hpp under it")
    return Tree(label, p)


def tree_from_rev(label: str, rev: str, work: Path) -> Tree:
    """Extract include/ at a git revision (no worktree is registered)."""
    sha = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--short", rev],
                         check=True, capture_output=True, text=True
                         ).stdout.strip()
    dest = work / "revs" / sha
    if not (dest / "include").is_dir():
        dest.mkdir(parents=True, exist_ok=True)
        archive = subprocess.run(
            ["git", "-C", str(REPO), "archive", sha, "include"],
            check=True, capture_output=True).stdout
        subprocess.run(["tar", "-x", "-C", str(dest)], input=archive,
                       check=True)
    return Tree(label or rev, dest / "include")


def split_label(spec: str) -> tuple[str, str]:
    label, sep, value = spec.partition("=")
    return (label, value) if sep else ("", spec)


# ---------------------------------------------------------------------------
# Measurement
# ---------------------------------------------------------------------------

BASE_FLAGS = ["-std=c++23", "-fsyntax-only"]


@dataclass
class Result:
    median: float = 0.0  # seconds
    min: float = 0.0
    stddev: float = 0.0
    rss_kb: int = 0
    counts: dict[str, float] = field(default_factory=dict)


def compile_cmd(compiler: str, tree: Tree, src: Path) -> list[str]:
    return [compiler, *BASE_FLAGS, f"-I{tree.include}", str(src)]


def wall_time(cmd: list[str], runs: int, warmup: int, tmp: Path) -> Result:
    out = tmp / "hyperfine.json"
    subprocess.run(
        ["hyperfine", "--shell=none", "--style=none", f"--runs={runs}",
         f"--warmup={warmup}", "--export-json", str(out),
         " ".join(map(quote, cmd))],
        check=True, capture_output=True)
    r = json.loads(out.read_text())["results"][0]
    return Result(median=r["median"], min=r["min"],
                  stddev=r["stddev"] or 0.0)


def quote(s: str) -> str:
    return "'" + s.replace("'", "'\\''") + "'" if re.search(r"[^\w./=+-]", s) \
        else s


def peak_rss(cmd: list[str]) -> int:
    p = subprocess.run(["/usr/bin/time", "-f", "%M", *cmd],
                       check=True, capture_output=True, text=True)
    return int(p.stderr.strip().splitlines()[-1])


SIZE_UNITS = {"": 1 / 1024, "k": 1, "M": 1024, "G": 1024 * 1024}


def gcc_counts(cmd: list[str]) -> dict[str, float]:
    """GC memory in use at exit, MiB, summed from -fmem-report's size table."""
    p = subprocess.run([*cmd, "-fmem-report"], check=True,
                       capture_output=True, text=True)
    used_kb = 0.0
    in_table = False
    for line in p.stderr.splitlines():
        if line.startswith("Size") and "Allocated" in line:
            in_table = True
            continue
        if in_table:
            m = re.match(r"\s*(\d+)\s+\S+\s+(\d+)([kMG]?)\s", line)
            if not m:
                break
            used_kb += int(m[2]) * SIZE_UNITS[m[3]]
    return {"ggc MiB": used_kb / 1024}


def clang_counts(cmd: list[str]) -> dict[str, float]:
    p = subprocess.run([*cmd, "-Xclang", "-print-stats"], check=True,
                       capture_output=True, text=True)
    counts = {}
    for key, pattern in [("specs", r"(\d+) ClassTemplateSpecialization decls"),
                         ("types", r"(\d+) types total")]:
        m = re.search(pattern, p.stderr)
        counts[key] = float(m[1]) if m else float("nan")
    return counts


def is_clang(compiler: str) -> bool:
    out = subprocess.run([compiler, "--version"], capture_output=True,
                         text=True).stdout
    return "clang" in out


def measure(compiler: str, tree: Tree, case: Case, src: Path, runs: int,
            warmup: int, tmp: Path) -> Result:
    cmd = compile_cmd(compiler, tree, src)
    check = subprocess.run(cmd, capture_output=True, text=True)
    if check.returncode != 0:
        print(f"\n{compiler} failed on {case.name} ({tree.label}):\n"
              f"{check.stderr[:2000]}", file=sys.stderr)
        return None
    r = wall_time(cmd, runs, warmup, tmp) if runs > 0 else Result()
    r.rss_kb = peak_rss(cmd)
    r.counts = clang_counts(cmd) if is_clang(compiler) else gcc_counts(cmd)
    return r


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------


def net(r: Result, base: Result) -> Result:
    return Result(median=r.median - base.median, min=r.min - base.min,
                  stddev=r.stddev, rss_kb=r.rss_kb - base.rss_kb,
                  counts={k: v - base.counts[k] for k, v in r.counts.items()})


def fmt_num(v: float) -> str:
    if v != v:  # nan
        return "?"
    if abs(v) >= 100 or v == int(v):
        return f"{v:,.0f}"
    return f"{v:,.1f}"


def pct(new: float, old: float) -> str:
    if old == 0 or old != old or new != new:
        return ""
    return f"{(new - old) / abs(old) * 100:+.0f}%"


def report(compiler: str, trees: list[Tree], cases: list[Case],
           results: dict, timed: bool) -> None:
    print(f"\n## {compiler}\n")
    print(("Times in ms. " if timed else "") +
          "Rows other than baseline are net of baseline.\n")
    count_keys = None
    for r in results.values():
        if r:
            count_keys = list(r.counts)
            break
    if count_keys is None:
        print("(no results)")
        return
    time_metrics = ["median", "min", "±σ"] if timed else []
    metrics = [*time_metrics, "RSS MiB", *count_keys]
    header = ["case"]
    for t in trees:
        for m in metrics:
            header.append(m if len(trees) == 1 else f"{t.label} {m}")
    if len(trees) > 1:
        header += [*(["Δ median"] if timed else []), "Δ RSS",
                   *(f"Δ {k}" for k in count_keys)]
    rows = []
    for case in cases:
        row = [case.name]
        vals = []
        for t in trees:
            r = results.get((t.label, case.name))
            base = results.get((t.label, "baseline"))
            if r is None or base is None:
                row += ["fail"] + [""] * (len(metrics) - 1)
                vals.append(None)
                continue
            shown = r if case.is_baseline else net(r, base)
            vals.append(shown)
            if timed:
                row += [fmt_num(shown.median * 1000),
                        fmt_num(shown.min * 1000),
                        fmt_num(shown.stddev * 1000)]
            row += [fmt_num(shown.rss_kb / 1024),
                    *(fmt_num(shown.counts[k]) for k in count_keys)]
        if len(trees) > 1:
            first, last = vals[0], vals[-1]
            if first and last:
                row += [*([pct(last.median, first.median)] if timed else []),
                        pct(last.rss_kb, first.rss_kb),
                        *(pct(last.counts[k], first.counts[k])
                          for k in count_keys)]
            else:
                row += [""] * (int(timed) + 1 + len(count_keys))
        rows.append(row)
    widths = [max(len(str(x)) for x in col) for col in zip(header, *rows)]
    line = lambda cells: "| " + " | ".join(
        str(c).ljust(w) if i == 0 else str(c).rjust(w)
        for i, (c, w) in enumerate(zip(cells, widths))) + " |"
    print(line(header))
    print("|" + "|".join("-" * (w + 2) for w in widths) + "|")
    for row in rows:
        print(line(row))
    if len(trees) > 1:
        print(f"\nΔ columns: {trees[-1].label} relative to {trees[0].label}.")


# ---------------------------------------------------------------------------


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tree", action="append", default=[], metavar="[LABEL=]PATH",
                    help="source tree (repo root or include dir); repeatable")
    ap.add_argument("--rev", action="append", default=[], metavar="[LABEL=]REV",
                    help="git revision to compare; repeatable. Revisions come "
                    "before --tree entries; with any --rev and no --tree, the "
                    "working tree is added last")
    ap.add_argument("--compiler", action="append", metavar="CXX",
                    help="compiler; repeatable (default: g++ and clang++)")
    ap.add_argument("--case", action="append", metavar="NAME",
                    choices=[*SERIES, "realistic"],
                    help="only these series; repeatable")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--counts-only", action="store_true",
                    help="skip wall time; report only peak RSS and the "
                    "deterministic counts (each case compiles ~twice)")
    ap.add_argument("--quick", action="store_true",
                    help="smaller sizes, 3 runs")
    ap.add_argument("--work", type=Path, default=REPO / "build" / "bench",
                    help="where generated sources and extracted revs go")
    ap.add_argument("--json", type=Path, help="also write raw results here")
    args = ap.parse_args()

    if not shutil.which("hyperfine"):
        sys.exit("hyperfine not found")
    compilers = args.compiler or ["g++", "clang++"]
    runs = 3 if args.quick and args.runs == 5 else args.runs
    if args.counts_only:
        runs = 0

    args.work.mkdir(parents=True, exist_ok=True)
    trees = [tree_from_rev(*split_label(s), args.work) for s in args.rev]
    trees += [tree_from_path(lbl or Path(p).resolve().name, p)
              for lbl, p in map(split_label, args.tree)]
    if not args.tree:
        trees.append(tree_from_path("current", str(REPO)))
    labels = [t.label for t in trees]
    if len(set(labels)) != len(labels):
        sys.exit(f"duplicate tree labels: {labels}; use LABEL=...")

    cases = build_cases(args.case, args.quick)
    src_dir = args.work / "src"
    src_dir.mkdir(exist_ok=True)
    sources = {}
    for i, case in enumerate(cases):
        p = src_dir / f"case_{i:02}.cpp"
        p.write_text(case.source)
        sources[case.name] = p

    print(f"trees: " + ", ".join(f"{t.label} ({t.include})" for t in trees))
    raw = {}
    with tempfile.TemporaryDirectory() as tmp:
        for compiler in compilers:
            results = {}
            for case in cases:
                for tree in trees:
                    print(f"\r{compiler}: {case.name:<12} {tree.label:<12}",
                          end="", file=sys.stderr, flush=True)
                    results[(tree.label, case.name)] = measure(
                        compiler, tree, case, sources[case.name], runs,
                        args.warmup, Path(tmp))
            print("\r" + " " * 60 + "\r", end="", file=sys.stderr)
            report(compiler, trees, cases, results, timed=runs > 0)
            raw[compiler] = {f"{t} / {c}": vars(r) if r else None
                             for (t, c), r in results.items()}
    if args.json:
        args.json.write_text(json.dumps(raw, indent=2))


if __name__ == "__main__":
    main()
