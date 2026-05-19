#!/usr/bin/env python3
"""测试3：Action/Goto 表构造正确性校验（独立于程序内置校验）。"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys

from _common import load_tsv, parse_kv_file, run_cmd


def fail(msg: str) -> int:
    print(f"FAIL: {msg}")
    return 1


def check_table(
    raw_dir: pathlib.Path,
    symbols_by_id: dict[int, str],
    symbol_kind_by_id: dict[int, str],
    state_ids: set[int],
    action_file: str,
    goto_file: str,
    conflict_file: str,
    resolution_file: str,
) -> tuple[bool, str]:
    actions = load_tsv(raw_dir / action_file)
    gotos = load_tsv(raw_dir / goto_file)
    conflicts = load_tsv(raw_dir / conflict_file)
    resolutions = load_tsv(raw_dir / resolution_file)

    if len(conflicts) != len(resolutions):
        return False, f"{conflict_file} 与 {resolution_file} 数量不一致"

    for row in actions:
        state_id = int(row["state_id"])
        terminal_id = int(row["terminal_id"])
        target_text = row["target"]
        action = row["action"]
        if state_id not in state_ids:
            return False, f"{action_file} 出现未知状态 state_id={state_id}"
        kind = symbol_kind_by_id.get(terminal_id, "Unknown")
        if kind not in {"Terminal", "Special"}:
            return False, f"{action_file} 出现非终结符列 terminal_id={terminal_id}, kind={kind}"
        if not (action.startswith("s") or action.startswith("r") or action == "acc"):
            return False, f"{action_file} action 非法: {action}"
        if action.startswith("s"):
            try:
                target = int(target_text)
            except ValueError:
                return False, f"{action_file} shift 目标状态不是整数: target={target_text}"
            if target not in state_ids:
                return False, f"{action_file} shift 目标状态越界: target={target}"

    for row in gotos:
        state_id = int(row["state_id"])
        nonterminal_id = int(row["nonterminal_id"])
        to_state = int(row["to_state"])
        if state_id not in state_ids:
            return False, f"{goto_file} 出现未知状态 state_id={state_id}"
        if to_state not in state_ids:
            return False, f"{goto_file} to_state 越界: {to_state}"
        kind = symbol_kind_by_id.get(nonterminal_id, "Unknown")
        if kind != "Nonterminal":
            return False, f"{goto_file} 出现非非终结符列 nonterminal_id={nonterminal_id}, kind={kind}"

    for row in conflicts:
        symbol_id = int(row["symbol_id"])
        symbol_name = row["symbol_name"]
        if symbols_by_id.get(symbol_id, "") != symbol_name:
            return False, f"{conflict_file} symbol_id 与 symbol_name 不一致: {symbol_id}/{symbol_name}"

    return True, (
        f"action={len(actions)}, goto={len(gotos)}, conflicts={len(conflicts)}, "
        f"resolutions={len(resolutions)}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="测试3：Action/Goto 表构造独立校验")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--export-dir", default="tests/out/test3_step10", help="导出目录")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    export_dir = root / args.export_dir
    if export_dir.exists():
        shutil.rmtree(export_dir)
    export_dir.mkdir(parents=True, exist_ok=True)

    proc = run_cmd(
        [args.bin, args.grammar, "--export", "--export-dir", str(export_dir)],
        cwd=root,
    )
    if proc.returncode != 0:
        return fail(f"工具执行失败，exit={proc.returncode}\n{proc.stderr}\n{proc.stdout}")

    summary = parse_kv_file(export_dir / "summary.txt")
    if summary.get("step") != "10":
        return fail("summary.step 不是 10")
    if summary.get("lr1_step10_validation_passed", "false").lower() != "true":
        return fail("lr1_step10_validation_passed != true")

    raw_dir = export_dir / "raw"
    symbols = load_tsv(raw_dir / "symbols.tsv")
    symbol_kind_by_id: dict[int, str] = {}
    symbols_by_id: dict[int, str] = {}
    for row in symbols:
        sid = int(row["id"])
        symbol_kind_by_id[sid] = row["kind"]
        symbols_by_id[sid] = row["name"]

    lr1_states = load_tsv(raw_dir / "lr1_states.tsv")
    lr1_state_ids = {int(r["state_id"]) for r in lr1_states}
    if not lr1_state_ids:
        return fail("lr1_states.tsv 为空")

    map_rows = load_tsv(raw_dir / "lr1_to_lalr_state_map.tsv")
    lalr_state_ids = {int(r["lalr_state"]) for r in map_rows}
    if not lalr_state_ids:
        return fail("lr1_to_lalr_state_map.tsv 未提取到 LALR 状态")

    ok, msg = check_table(
        raw_dir=raw_dir,
        symbols_by_id=symbols_by_id,
        symbol_kind_by_id=symbol_kind_by_id,
        state_ids=lr1_state_ids,
        action_file="action_table.tsv",
        goto_file="goto_table.tsv",
        conflict_file="parse_table_conflicts.tsv",
        resolution_file="parse_table_conflict_resolution.tsv",
    )
    if not ok:
        return fail(f"LR1 表校验失败: {msg}")

    ok, msg2 = check_table(
        raw_dir=raw_dir,
        symbols_by_id=symbols_by_id,
        symbol_kind_by_id=symbol_kind_by_id,
        state_ids=lalr_state_ids,
        action_file="lalr_action_table.tsv",
        goto_file="lalr_goto_table.tsv",
        conflict_file="lalr_parse_table_conflicts.tsv",
        resolution_file="lalr_parse_table_conflict_resolution.tsv",
    )
    if not ok:
        return fail(f"LALR 表校验失败: {msg2}")

    print("PASS: 测试3通过")
    print(f"  LR1: {msg}")
    print(f"  LALR: {msg2}")
    print(f"  export_dir={export_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
