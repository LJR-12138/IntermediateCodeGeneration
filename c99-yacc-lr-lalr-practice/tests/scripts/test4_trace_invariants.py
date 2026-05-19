#!/usr/bin/env python3
"""测试4：LR 运行时语义不变式校验（LR1 + LALR）。"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys

from _common import DEFAULT_CASES, load_tsv, parse_kv_file, run_cmd


def fail(msg: str) -> int:
    print(f"FAIL: {msg}")
    return 1


def is_shift(action: str) -> bool:
    return len(action) >= 2 and action[0] == "s" and action[1:].isdigit()


def is_reduce(action: str) -> bool:
    return len(action) >= 2 and action[0] == "r" and action[1:].isdigit()


def check_trace(rows: list[dict[str, str]], expected_accept: bool, case_name: str, flavor: str) -> tuple[bool, str]:
    if not rows:
        return False, f"{case_name}/{flavor} trace 为空"

    for i, row in enumerate(rows):
        try:
            int(row["step"])
            int(row["state"])
            idx = int(row["input_index"])
            if idx < 0:
                return False, f"{case_name}/{flavor} 第{i+1}行 input_index < 0"
        except Exception:
            return False, f"{case_name}/{flavor} 第{i+1}行字段格式非法"

    for i in range(len(rows) - 1):
        cur = rows[i]
        nxt = rows[i + 1]
        cur_idx = int(cur["input_index"])
        nxt_idx = int(nxt["input_index"])
        action = cur["action"]
        if is_shift(action):
            if nxt_idx != cur_idx + 1:
                return False, f"{case_name}/{flavor} shift 邻接不变量失败@row={i+1}"
        elif is_reduce(action):
            if nxt_idx != cur_idx:
                return False, f"{case_name}/{flavor} reduce 邻接不变量失败@row={i+1}"
        elif action == "acc":
            return False, f"{case_name}/{flavor} acc 出现在非末尾@row={i+1}"
        elif action == "error":
            return False, f"{case_name}/{flavor} error 出现在非末尾@row={i+1}"
        else:
            return False, f"{case_name}/{flavor} 未知动作格式@row={i+1}: {action}"

    last = rows[-1]
    if expected_accept:
        if last["action"] != "acc":
            return False, f"{case_name}/{flavor} 预期 accept，但末动作不是 acc"
        if last["lookahead"] != "$":
            return False, f"{case_name}/{flavor} 预期 acc 时 lookahead=$，实际={last['lookahead']}"
    else:
        if last["action"] not in {"error", "acc"}:
            return False, f"{case_name}/{flavor} reject 用例末动作异常: {last['action']}"
    return True, f"rows={len(rows)}, last_action={last['action']}"


def main() -> int:
    parser = argparse.ArgumentParser(description="测试4：解析 trace 语义不变式")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-root", default="tests/out/test4", help="导出根目录")
    parser.add_argument("--max-steps", type=int, default=200000, help="解析最大步数")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    out_root = root / args.out_root
    if out_root.exists():
        shutil.rmtree(out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    messages: list[str] = []
    for case in DEFAULT_CASES:
        export_dir = out_root / case.name
        proc = run_cmd(
            [
                args.bin,
                args.grammar,
                "--parse-tokens",
                case.tokens,
                "--export",
                "--export-dir",
                str(export_dir),
                "--max-parse-steps",
                str(args.max_steps),
            ],
            cwd=root,
        )
        if proc.returncode != 0:
            return fail(f"{case.name} 工具执行失败，exit={proc.returncode}\n{proc.stderr}\n{proc.stdout}")

        summary = parse_kv_file(export_dir / "summary.txt")
        lr1_accepted = summary.get("lr1_parse_accepted", "false").lower() == "true"
        lalr_accepted = summary.get("lalr_parse_accepted", "false").lower() == "true"
        if lr1_accepted != case.expect_accept:
            return fail(f"{case.name} LR1 接受性与期望不一致")
        if lalr_accepted != case.expect_accept:
            return fail(f"{case.name} LALR 接受性与期望不一致")

        lr1_rows = load_tsv(export_dir / "raw" / "parse_trace.tsv")
        lalr_rows = load_tsv(export_dir / "raw" / "parse_trace_lalr.tsv")
        ok, msg = check_trace(lr1_rows, case.expect_accept, case.name, "LR1")
        if not ok:
            return fail(msg)
        ok, msg2 = check_trace(lalr_rows, case.expect_accept, case.name, "LALR")
        if not ok:
            return fail(msg2)
        messages.append(f"{case.name}: LR1({msg}); LALR({msg2})")

    print("PASS: 测试4通过")
    for m in messages:
        print(f"  - {m}")
    print(f"  out_root={out_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

