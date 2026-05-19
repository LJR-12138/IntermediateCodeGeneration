#!/usr/bin/env python3
"""Step9 自动验收脚本。

功能：
1. 调用 yacc_parse_tool 执行第9步（读取 token 文件）。
2. 检查导出目录中的 summary / report / parse_trace 关键不变量。
3. 给出 PASS / FAIL 与失败原因。
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class CheckResult:
    ok: bool
    message: str


def parse_kv_file(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        result[k.strip()] = v.strip()
    return result


def parse_trace(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f, delimiter="\t")
        return list(reader)


def is_shift(action: str) -> bool:
    return len(action) >= 2 and action[0] == "s" and action[1:].isdigit()


def is_reduce(action: str) -> bool:
    return len(action) >= 2 and action[0] == "r" and action[1:].isdigit()


def run_and_validate(args: argparse.Namespace) -> CheckResult:
    export_dir = pathlib.Path(args.export_dir)
    export_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        args.bin,
        args.grammar,
        "--parse-tokens",
        args.tokens,
        "--export",
        "--export-dir",
        str(export_dir),
        "--max-parse-steps",
        str(args.max_steps),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        return CheckResult(False, f"工具执行失败，exit={proc.returncode}\n{proc.stderr}\n{proc.stdout}")

    summary_path = export_dir / "summary.txt"
    report_path = export_dir / "analysis" / "report.txt"
    trace_path = export_dir / "raw" / "parse_trace.tsv"
    error_path = export_dir / "raw" / "parse_error.txt"

    for p in [summary_path, report_path, trace_path, error_path]:
        if not p.exists():
            return CheckResult(False, f"缺少导出文件: {p}")

    summary = parse_kv_file(summary_path)
    report = parse_kv_file(report_path)
    rows = parse_trace(trace_path)

    if summary.get("step") != "9":
        return CheckResult(False, "summary.step 不是 9")
    if not rows:
        return CheckResult(False, "parse_trace.tsv 为空")

    # 通用不变量：state 必须是整数，input_index 非负
    for i, row in enumerate(rows):
        try:
            int(row["state"])
            idx = int(row["input_index"])
            if idx < 0:
                return CheckResult(False, f"第 {i+1} 行 input_index < 0")
        except Exception:
            return CheckResult(False, f"第 {i+1} 行字段格式非法")

    # 邻接动作不变量
    for i in range(len(rows) - 1):
        cur = rows[i]
        nxt = rows[i + 1]
        cur_idx = int(cur["input_index"])
        nxt_idx = int(nxt["input_index"])
        action = cur["action"]

        if is_shift(action):
            if nxt_idx != cur_idx + 1:
                return CheckResult(False, f"shift 不变量失败：row={i+1}, next_input_index={nxt_idx}, expect={cur_idx+1}")
        elif is_reduce(action):
            if nxt_idx != cur_idx:
                return CheckResult(False, f"reduce 不变量失败：row={i+1}, next_input_index={nxt_idx}, expect={cur_idx}")
        elif action == "acc":
            return CheckResult(False, f"acc 只能出现在最后一步：row={i+1}")
        elif action == "error":
            # error 允许在末尾；若中间出现，视为异常
            return CheckResult(False, f"error 动作出现在非末尾步骤：row={i+1}")
        else:
            return CheckResult(False, f"未知动作格式：row={i+1}, action={action}")

    expected = args.expect
    accepted = summary.get("parse_accepted", "").lower() == "true"
    has_error = report.get("parse_error", "").lower() == "true"
    last = rows[-1]

    if expected == "accept":
        if not accepted:
            return CheckResult(False, "预期 accept，但 parse_accepted=false")
        if has_error:
            return CheckResult(False, "预期 accept，但 report.parse_error=true")
        if last["action"] != "acc":
            return CheckResult(False, f"预期最后动作 acc，实际为 {last['action']}")
        if last["lookahead"] != "$":
            return CheckResult(False, f"预期 acc 时 lookahead=$，实际为 {last['lookahead']}")
    else:
        if accepted:
            return CheckResult(False, "预期 reject，但 parse_accepted=true")
        if not has_error:
            return CheckResult(False, "预期 reject，但 report.parse_error=false")
        if last["action"] != "error":
            # 允许达到 max_steps 的 reject，不强制最后一步是 error
            err_text = error_path.read_text(encoding="utf-8")
            if "message=" not in err_text:
                return CheckResult(False, "预期 reject，但既无 error 动作也无 parse_error 详细信息")

    return CheckResult(
        True,
        (
            f"PASS: expect={expected}, accepted={accepted}, trace_rows={len(rows)}, "
            f"steps={summary.get('parse_total_steps', '?')}, reductions={summary.get('parse_reductions', '?')}, "
            f"export_dir={export_dir}"
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="YACC Step9 自动验收")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法文件")
    parser.add_argument("--tokens", required=True, help="token 文件路径")
    parser.add_argument("--expect", choices=["accept", "reject"], required=True, help="预期结果")
    parser.add_argument("--export-dir", required=True, help="导出目录")
    parser.add_argument("--max-steps", type=int, default=200000, help="解析最大步数")
    args = parser.parse_args()

    result = run_and_validate(args)
    if result.ok:
        print(result.message)
        return 0
    print("FAIL:", result.message)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

