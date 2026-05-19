#!/usr/bin/env python3
"""YACC 测试公共工具。"""

from __future__ import annotations

import csv
import pathlib
import subprocess
from dataclasses import dataclass


@dataclass
class TestCase:
    name: str
    tokens: str
    expect_accept: bool


DEFAULT_CASES: list[TestCase] = [
    TestCase(name="decl_int", tokens="contracts/yacc/tokens/c99_decl_int.tokens", expect_accept=True),
    TestCase(name="decl_multi", tokens="contracts/yacc/tokens/c99_decl_multi.tokens", expect_accept=True),
    TestCase(name="decl_pointer", tokens="contracts/yacc/tokens/c99_decl_pointer.tokens", expect_accept=True),
    TestCase(name="func_return_const", tokens="contracts/yacc/tokens/c99_func_return_const.tokens", expect_accept=True),
    TestCase(name="func_param_return", tokens="contracts/yacc/tokens/c99_func_param_return.tokens", expect_accept=True),
    TestCase(name="if_else_return", tokens="contracts/yacc/tokens/c99_if_else_return.tokens", expect_accept=True),
    TestCase(name="struct_decl", tokens="contracts/yacc/tokens/c99_struct_decl.tokens", expect_accept=True),
    TestCase(name="enum_decl", tokens="contracts/yacc/tokens/c99_enum_decl.tokens", expect_accept=True),
    TestCase(name="invalid_if", tokens="contracts/yacc/tokens/c99_invalid_if.tokens", expect_accept=False),
    TestCase(name="invalid_missing_semi", tokens="contracts/yacc/tokens/c99_invalid_missing_semi.tokens", expect_accept=False),
    TestCase(name="invalid_unclosed_block", tokens="contracts/yacc/tokens/c99_invalid_unclosed_block.tokens", expect_accept=False),
]


def parse_kv_file(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        result[k.strip()] = v.strip()
    return result


def load_tsv(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def run_cmd(cmd: list[str], cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True)
