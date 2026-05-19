#!/usr/bin/env python3
"""统一运行 YACC 测试 1/2/3/4。"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(cmd: list[str], cwd: pathlib.Path) -> int:
    print(f"$ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True)
    if proc.stdout:
        print(proc.stdout.rstrip())
    if proc.stderr:
        print(proc.stderr.rstrip())
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="运行 YACC 测试 1/2/2f/3/4")
    parser.add_argument(
        "--tests",
        default="1,2,3,4",
        help="要运行的测试编号，逗号分隔，例如 1,2,2f,4",
    )
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--strict-bison", action="store_true", help="测试2缺依赖时直接失败")
    parser.add_argument("--update-golden", action="store_true", help="测试1更新 golden 快照")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    test_ids = [x.strip() for x in args.tests.split(",") if x.strip()]
    rc_all = 0

    for test_id in test_ids:
        if test_id == "1":
            cmd = ["python3", "tests/scripts/test1_golden_regression.py"]
            if args.update_golden:
                cmd.append("--update-golden")
        elif test_id == "2":
            cmd = ["python3", "tests/scripts/test2_bison_compare.py"]
            if args.strict_bison:
                cmd.append("--strict")
        elif test_id == "2f":
            cmd = ["python3", "tests/scripts/test2_full_lalr_automaton_compare.py"]
            if args.strict_bison:
                cmd.append("--strict")
        elif test_id == "3":
            cmd = ["python3", "tests/scripts/test3_table_consistency.py"]
        elif test_id == "4":
            cmd = ["python3", "tests/scripts/test4_trace_invariants.py"]
        else:
            print(f"忽略未知测试编号: {test_id}")
            continue

        rc = run(cmd, cwd=root)
        if rc != 0:
            rc_all = rc

    if rc_all == 0:
        print("ALL PASS")
    else:
        print("SOME FAILED")
    return rc_all


if __name__ == "__main__":
    sys.exit(main())
