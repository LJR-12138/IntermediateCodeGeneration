#!/usr/bin/env python3
"""测试2f：完整 LALR 状态机对拍（状态/转移/Action/Goto）。"""

from __future__ import annotations

import argparse
import csv
import pathlib
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass, field

from _common import load_tsv, run_cmd


@dataclass
class BisonState:
    state_id: int
    items: set[str] = field(default_factory=set)  # kernel items in bison .output
    shift_edges: dict[str, int] = field(default_factory=dict)
    goto_edges: dict[str, int] = field(default_factory=dict)
    explicit_actions: dict[str, str] = field(default_factory=dict)  # token -> sN/rN/acc
    default_action: str = ""  # rN/acc/""


def norm_spaces(text: str) -> str:
    return re.sub(r"\s+", " ", text.strip())


def norm_sym(text: str) -> str:
    t = norm_spaces(text)
    if t == "$end":
        return "$"
    return t


def norm_item(lhs: str, rhs: str) -> str:
    lhs_n = norm_spaces(lhs)
    rhs_n = norm_spaces(rhs).replace("•", "·")
    # 归一化增广文法差异：
    # bison: $accept -> translation_unit $end
    # our  : S'      -> translation_unit
    if lhs_n == "$accept":
        lhs_n = "S'"
        parts = [p for p in rhs_n.split(" ") if p]
        parts = [p for p in parts if p != "$end"]
        rhs_n = " ".join(parts)
    return f"{lhs_n} -> {rhs_n}"


def parse_our_lalr_states(path: pathlib.Path) -> dict[int, set[str]]:
    result: dict[int, set[str]] = {}
    cur = -1
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m_state = re.match(r"^\[state\s+(\d+)\]$", line)
        if m_state:
            cur = int(m_state.group(1))
            result[cur] = set()
            continue
        if cur < 0:
            continue
        if not (line.startswith("[") and line.endswith("]")):
            continue
        body = line[1:-1].strip()
        if " -> " not in body:
            continue
        lhs, rhs_lookahead = body.split(" -> ", 1)
        # 去掉尾部 lookahead：按最后一个 ", " 切分，避免 RHS 中的 "',' 干扰。
        rhs_core = rhs_lookahead.rsplit(", ", 1)[0]
        result[cur].add(norm_item(lhs, rhs_core))
    return result


def is_kernel_item(core_item: str) -> bool:
    # core_item 形如：A -> alpha · beta
    if " -> " not in core_item:
        return False
    lhs, rhs = core_item.split(" -> ", 1)
    rhs = rhs.strip()
    # LR(0) kernel: dot 不在最前，或是增广开始项 S' -> · S
    if rhs.startswith("·"):
        return lhs.strip() == "S'"
    return True


def parse_bison_output(path: pathlib.Path) -> tuple[dict[int, BisonState], dict[int, int]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    states: dict[int, BisonState] = {}
    conflict_count_by_state: dict[int, int] = {}
    for line in lines:
        m = re.match(r"^State\s+(\d+)\s+conflicts:\s+(\d+)\s+", line.strip())
        if m:
            conflict_count_by_state[int(m.group(1))] = int(m.group(2))

    i = 0
    while i < len(lines):
        m_state = re.match(r"^State\s+(\d+)$", lines[i].strip())
        if not m_state:
            i += 1
            continue
        sid = int(m_state.group(1))
        st = BisonState(state_id=sid)
        states[sid] = st
        i += 1

        last_lhs = ""
        while i < len(lines):
            line = lines[i]
            striped = line.strip()
            if re.match(r"^State\s+\d+(\s+conflicts:.*)?$", striped):
                break

            # 项目行：216 A: ...
            m_item = re.match(r"^\s*\d+\s+([^:]+):\s+(.*)$", line)
            if m_item:
                last_lhs = m_item.group(1)
                rhs = m_item.group(2)
                st.items.add(norm_item(last_lhs, rhs))
                i += 1
                continue
            # 项目续行：217 | ...
            m_item2 = re.match(r"^\s*\d+\s+\|\s+(.*)$", line)
            if m_item2 and last_lhs:
                rhs = m_item2.group(1)
                st.items.add(norm_item(last_lhs, rhs))
                i += 1
                continue

            # shift
            m_shift = re.match(r"^\s*(\S.*?)\s+shift, and go to state\s+(\d+)\s*$", line)
            if m_shift:
                sym = norm_sym(m_shift.group(1))
                to_state = int(m_shift.group(2))
                st.shift_edges[sym] = to_state
                st.explicit_actions[sym] = f"s{to_state}"
                i += 1
                continue

            # goto
            m_goto = re.match(r"^\s*([a-z_][a-z0-9_]*)\s+go to state\s+(\d+)\s*$", line)
            if m_goto:
                nt = norm_sym(m_goto.group(1))
                st.goto_edges[nt] = int(m_goto.group(2))
                i += 1
                continue

            # default action（必须在 token-specific 之前匹配）
            m_def_reduce = re.match(r"^\s*\$default\s+reduce using rule\s+(\d+)", line)
            if m_def_reduce:
                st.default_action = f"r{int(m_def_reduce.group(1))}"
                i += 1
                continue
            if re.match(r"^\s*\$default\s+accept\s*$", line):
                st.default_action = "acc"
                i += 1
                continue

            # token-specific reduce (忽略中括号候选动作)
            m_reduce = re.match(r"^\s*(\S.*?)\s+reduce using rule\s+(\d+)", line)
            if m_reduce and "[" not in line:
                tok = norm_sym(m_reduce.group(1))
                st.explicit_actions[tok] = f"r{int(m_reduce.group(2))}"
                i += 1
                continue

            # token-specific accept
            m_accept = re.match(r"^\s*(\S.*?)\s+accept\s*$", line)
            if m_accept and "[" not in line:
                tok = norm_sym(m_accept.group(1))
                st.explicit_actions[tok] = "acc"
                i += 1
                continue

            i += 1
    return states, conflict_count_by_state


def normalize_bison_accept_tail(states: dict[int, BisonState]) -> None:
    # 归一化 bison 的 $end 接受尾状态：
    # $accept -> translation_unit $end •   (default accept)
    # 对应我方一般是 S' -> translation_unit •, $ 直接 acc，不单独建 $end 状态。
    pure_accept_states: set[int] = set()
    for sid, st in states.items():
        if st.default_action != "acc":
            continue
        if st.shift_edges or st.goto_edges:
            continue
        if st.items == {"S' -> translation_unit ·"}:
            pure_accept_states.add(sid)

    if not pure_accept_states:
        return

    for st in states.values():
        to_remove_tokens: list[str] = []
        for tok, act in st.explicit_actions.items():
            if not act.startswith("s"):
                continue
            to_sid = int(act[1:])
            if to_sid in pure_accept_states and tok == "$":
                st.explicit_actions[tok] = "acc"
                to_remove_tokens.append(tok)
        for tok in to_remove_tokens:
            st.shift_edges.pop(tok, None)

    for sid in pure_accept_states:
        states.pop(sid, None)


def parse_our_automaton(raw_dir: pathlib.Path) -> tuple[
    dict[int, set[str]],
    dict[tuple[int, str], int],
    dict[tuple[int, str], str],
    dict[tuple[int, str], int],
    set[str],
]:
    our_states = parse_our_lalr_states(raw_dir / "lalr_state_items.txt")

    trans_rows = load_tsv(raw_dir / "lalr_transitions.tsv")
    trans_map: dict[tuple[int, str], int] = {}
    for r in trans_rows:
        trans_map[(int(r["from_state"]), r["symbol_name"])] = int(r["to_state"])

    action_rows = load_tsv(raw_dir / "lalr_action_table.tsv")
    action_map: dict[tuple[int, str], str] = {}
    terminal_names: set[str] = set()
    for r in action_rows:
        sid = int(r["state_id"])
        tname = r["terminal_name"]
        act = r["action"]
        action_map[(sid, tname)] = act
        terminal_names.add(tname)

    goto_rows = load_tsv(raw_dir / "lalr_goto_table.tsv")
    goto_map: dict[tuple[int, str], int] = {}
    for r in goto_rows:
        goto_map[(int(r["state_id"]), r["nonterminal_name"])] = int(r["to_state"])

    return our_states, trans_map, action_map, goto_map, terminal_names


def main() -> int:
    parser = argparse.ArgumentParser(description="测试2f：完整 LALR 状态机对拍")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help="文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-root", default="tests/out/test2_full", help="输出目录")
    parser.add_argument("--strict", action="store_true", help="缺依赖时失败")
    args = parser.parse_args()

    bison = shutil.which("bison")
    if not bison:
        msg = "SKIP: 缺少 bison"
        if args.strict:
            print(f"FAIL: {msg}")
            return 1
        print(msg)
        return 0

    root = pathlib.Path(args.workdir).resolve()
    out_root = root / args.out_root
    out_root.mkdir(parents=True, exist_ok=True)

    # 1) 导出我方 step10 LALR 产物
    our_dir = out_root / "our_step10"
    if our_dir.exists():
        shutil.rmtree(our_dir)
    our_dir.mkdir(parents=True, exist_ok=True)
    p_our = run_cmd([args.bin, args.grammar, "--export", "--export-dir", str(our_dir)], cwd=root)
    if p_our.returncode != 0:
        print(f"FAIL: 运行我方导出失败\n{p_our.stderr}\n{p_our.stdout}")
        return 1
    raw_dir = our_dir / "raw"

    # 2) 生成 bison .output
    with tempfile.TemporaryDirectory(prefix="yacc_test2_full_", dir=str(out_root)) as tmp:
        tmp_dir = pathlib.Path(tmp)
        parser_c = tmp_dir / "parser.tab.c"
        p_bison = run_cmd([bison, "--report=state", "-v", "-d", "-o", str(parser_c), str(root / args.grammar)], cwd=root)
        if p_bison.returncode != 0:
            print(f"FAIL: bison 生成失败\n{p_bison.stderr}\n{p_bison.stdout}")
            return 1
        bison_output = tmp_dir / "parser.output"
        if not bison_output.exists():
            print(f"FAIL: 未找到 bison 输出文件: {bison_output}")
            return 1
        (out_root / "bison_parser.output").write_text(bison_output.read_text(encoding="utf-8"), encoding="utf-8")

    our_states, our_trans, our_action, our_goto, terminal_names = parse_our_automaton(raw_dir)
    bison_states, bison_conflict_count_by_state = parse_bison_output(out_root / "bison_parser.output")
    normalize_bison_accept_tail(bison_states)

    # 3) 状态同构映射：按 LR(0) kernel 项签名
    our_sig_to_state: dict[str, int] = {}
    for sid, items in our_states.items():
        kernel = {x for x in items if is_kernel_item(x)}
        sig = "\n".join(sorted(kernel))
        if sig in our_sig_to_state:
            print(f"FAIL: 我方存在重复状态签名: states={our_sig_to_state[sig]},{sid}")
            return 1
        our_sig_to_state[sig] = sid

    bison_sig_to_state: dict[str, int] = {}
    for sid, st in bison_states.items():
        sig = "\n".join(sorted(st.items))
        if sig in bison_sig_to_state:
            print(f"FAIL: bison 存在重复状态签名: states={bison_sig_to_state[sig]},{sid}")
            return 1
        bison_sig_to_state[sig] = sid

    our_sig_set = set(our_sig_to_state.keys())
    bison_sig_set = set(bison_sig_to_state.keys())
    if our_sig_set != bison_sig_set:
        miss_in_bison = len(our_sig_set - bison_sig_set)
        miss_in_our = len(bison_sig_set - our_sig_set)
        report = [
            f"FAIL: 状态签名集合不一致 miss_in_bison={miss_in_bison}, miss_in_our={miss_in_our}",
            "",
            "[missing_in_bison sample]",
        ]
        for s in list(our_sig_set - bison_sig_set)[:3]:
            report.append(s.splitlines()[0] if s else "<empty>")
        report.append("")
        report.append("[missing_in_our sample]")
        for s in list(bison_sig_set - our_sig_set)[:3]:
            report.append(s.splitlines()[0] if s else "<empty>")
        (out_root / "compare_full_diff.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
        print(report[0])
        return 1

    # bison_state_id -> our_state_id
    b2o: dict[int, int] = {}
    o2b: dict[int, int] = {}
    for sig in bison_sig_set:
        bid = bison_sig_to_state[sig]
        oid = our_sig_to_state[sig]
        b2o[bid] = oid
        o2b[oid] = bid

    # 4) 转移边比较（终结符+非终结符）
    our_edge_set: set[tuple[int, str, int]] = set()
    for (from_sid, sym), to_sid in our_trans.items():
        our_edge_set.add((from_sid, sym, to_sid))

    bison_edge_set: set[tuple[int, str, int]] = set()
    for bid, st in bison_states.items():
        ofrom = b2o[bid]
        for sym, bto in st.shift_edges.items():
            bison_edge_set.add((ofrom, sym, b2o[bto]))
        for sym, bto in st.goto_edges.items():
            bison_edge_set.add((ofrom, sym, b2o[bto]))

    if our_edge_set != bison_edge_set:
        only_our = sorted(list(our_edge_set - bison_edge_set))[:20]
        only_bison = sorted(list(bison_edge_set - our_edge_set))[:20]
        lines = ["FAIL: 转移边集合不一致", "", "[only_our first20]"]
        lines.extend([f"{a} --{b}--> {c}" for a, b, c in only_our])
        lines.append("")
        lines.append("[only_bison first20]")
        lines.extend([f"{a} --{b}--> {c}" for a, b, c in only_bison])
        (out_root / "compare_full_diff.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("FAIL: 转移边集合不一致")
        return 1

    # 5) Goto 表比较（由 our_goto 与 bison goto_edges 对照）
    bison_goto_map: dict[tuple[int, str], int] = {}
    for bid, st in bison_states.items():
        ofrom = b2o[bid]
        for nt, bto in st.goto_edges.items():
            bison_goto_map[(ofrom, nt)] = b2o[bto]
    if our_goto != bison_goto_map:
        only_our = sorted(list(set(our_goto.items()) - set(bison_goto_map.items())))[:20]
        only_bison = sorted(list(set(bison_goto_map.items()) - set(our_goto.items())))[:20]
        lines = ["FAIL: Goto 表不一致", "", "[only_our first20]"]
        lines.extend([f"{k} => {v}" for k, v in only_our])
        lines.append("")
        lines.append("[only_bison first20]")
        lines.extend([f"{k} => {v}" for k, v in only_bison])
        (out_root / "compare_full_diff.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("FAIL: Goto 表不一致")
        return 1

    # 6) Action 表比较（展开 bison $default 到我方终结符全集）
    bison_action_map: dict[tuple[int, str], str] = {}
    bison_default_by_state: dict[int, str] = {}
    for bid, st in bison_states.items():
        osid = b2o[bid]
        for tok, act in st.explicit_actions.items():
            norm_act = act
            if act.startswith("s"):
                to_bid = int(act[1:])
                if to_bid not in b2o:
                    print(f"FAIL: bison action 目标状态不存在映射: state={bid}, token={tok}, action={act}")
                    return 1
                norm_act = f"s{b2o[to_bid]}"
            bison_action_map[(osid, tok)] = norm_act
        if st.default_action:
            bison_default_by_state[osid] = st.default_action

    # Action 对拍口径：
    # 1) bison 显式项必须在我方出现且动作一致；
    # 2) 我方项若 bison 未显式给出，则允许由 bison 的 $default 动作覆盖，且必须一致。
    action_mismatch: list[str] = []
    for key, b_act in bison_action_map.items():
        o_act = our_action.get(key)
        if o_act is None:
            action_mismatch.append(f"missing_our {key} bison={b_act}")
            if len(action_mismatch) >= 30:
                break
            continue
        if o_act != b_act:
            action_mismatch.append(f"diff {key} our={o_act} bison={b_act}")
            if len(action_mismatch) >= 30:
                break
    if not action_mismatch:
        for key, o_act in our_action.items():
            if key in bison_action_map:
                continue
            sid = key[0]
            d_act = bison_default_by_state.get(sid, "")
            if not d_act:
                action_mismatch.append(f"missing_bison {key} our={o_act} no_default")
            elif d_act != o_act:
                action_mismatch.append(f"default_diff {key} our={o_act} bison_default={d_act}")
            if len(action_mismatch) >= 30:
                break

    if action_mismatch:
        lines = ["FAIL: Action 表不一致（已按 bison $default 归一化）", "", "[mismatch first30]"]
        lines.extend(action_mismatch)
        (out_root / "compare_full_diff.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("FAIL: Action 表不一致")
        return 1

    # 7) 冲突数量比较（LALR）
    our_conflicts = load_tsv(raw_dir / "lalr_parse_table_conflicts.tsv")
    bison_conflicts_total = sum(bison_conflict_count_by_state.values())
    if len(our_conflicts) != bison_conflicts_total:
        print(
            f"FAIL: 冲突总数不一致: our={len(our_conflicts)}, bison={bison_conflicts_total}"
        )
        return 1

    # 8) 输出报告
    tsv_path = out_root / "compare_full_report.tsv"
    md_path = out_root / "compare_full_report.md"
    with tsv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["metric", "our", "bison", "equal"],
            delimiter="\t",
        )
        writer.writeheader()
        writer.writerows(
            [
                {"metric": "lalr_states", "our": str(len(our_states)), "bison": str(len(bison_states)), "equal": "true"},
                {"metric": "lalr_edges", "our": str(len(our_edge_set)), "bison": str(len(bison_edge_set)), "equal": "true"},
                {"metric": "goto_entries", "our": str(len(our_goto)), "bison": str(len(bison_goto_map)), "equal": "true"},
                {"metric": "action_entries_our", "our": str(len(our_action)), "bison": str(len(our_action)), "equal": "true"},
                {"metric": "action_entries_bison_explicit", "our": str(len(bison_action_map)), "bison": str(len(bison_action_map)), "equal": "true"},
                {"metric": "conflicts", "our": str(len(our_conflicts)), "bison": str(bison_conflicts_total), "equal": "true"},
            ]
        )

    md_lines = [
        "# 测试2f 完整 LALR 状态机对拍报告",
        "",
        "- 状态同构（按 LR(0) 核心项签名）：一致",
        "- 转移边集合：一致",
        "- Goto 表：一致",
        "- Action 表（含 bison `$default` 展开）：一致",
        "- 冲突总数：一致",
        "",
        f"- 明细 TSV: `{tsv_path.relative_to(root)}`",
        f"- bison 原始状态机: `{(out_root / 'bison_parser.output').relative_to(root)}`",
    ]
    md_path.write_text("\n".join(md_lines) + "\n", encoding="utf-8")

    print("PASS: 测试2f通过（完整 LALR 状态机对拍一致）")
    print(f"  - report: {tsv_path}")
    print(f"  - report: {md_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
