#!/usr/bin/env python3
"""测试2：与 Bison 基线实现对拍。"""

from __future__ import annotations

import argparse
import csv
import pathlib
import re
import shutil
import sys
import tempfile
from typing import Sequence

from _common import DEFAULT_CASES, parse_kv_file, run_cmd


def parse_token_names_from_grammar(grammar_path: pathlib.Path) -> list[str]:
    token_names: list[str] = []
    for line in grammar_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith("%token"):
            continue
        parts = stripped.split()
        for item in parts[1:]:
            if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", item):
                token_names.append(item)
    return token_names


def build_driver_c(token_names: Sequence[str]) -> str:
    token_map_rows = "\n".join(
        [f'    if (strcmp(name, "{t}") == 0) return {t};' for t in token_names]
    )
    return f"""#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser.tab.h"

static FILE* g_token_fp = NULL;
int column = 1;

static int decode_char_literal(const char* name) {{
    size_t n = strlen(name);
    if (n < 3 || name[0] != '\\'' || name[n - 1] != '\\'') {{
        return -1;
    }}
    if (n == 3) {{
        return (unsigned char)name[1];
    }}
    if (n == 4 && name[1] == '\\\\') {{
        switch (name[2]) {{
            case 'n': return '\\n';
            case 't': return '\\t';
            case 'r': return '\\r';
            case '\\\\': return '\\\\';
            case '\\'': return '\\'';
            default: return (unsigned char)name[2];
        }}
    }}
    return -1;
}}

static int token_code_from_name(const char* name) {{
    int ch = decode_char_literal(name);
    if (ch >= 0) return ch;
{token_map_rows}
    return -1;
}}

int yylex(void) {{
    if (!g_token_fp) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), g_token_fp) != NULL) {{
        char* p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\\0' || *p == '#') continue;

        char token_name[256];
        if (sscanf(p, "%255s", token_name) != 1) continue;
        if (strcmp(token_name, "$") == 0) return 0;

        int code = token_code_from_name(token_name);
        if (code < 0) {{
            fprintf(stderr, "[bison-driver] unknown token: %s\\n", token_name);
            return 0;
        }}
        return code;
    }}
    return 0;
}}

int main(int argc, char** argv) {{
    if (argc < 2) {{
        fprintf(stderr, "usage: %s <tokens-file>\\n", argv[0]);
        return 2;
    }}
    g_token_fp = fopen(argv[1], "r");
    if (!g_token_fp) {{
        perror("open tokens");
        return 3;
    }}
#if YYDEBUG
    extern int yydebug;
    yydebug = 1;
#endif
    int rc = yyparse();
    fclose(g_token_fp);
    if (rc == 0) {{
        fprintf(stdout, "BISON_RESULT:accept\\n");
    }} else {{
        fprintf(stdout, "BISON_RESULT:reject\\n");
    }}
    return rc;
}}
"""


def parse_bison_reduction_sequence(output: str) -> list[int]:
    seq: list[int] = []
    pattern = re.compile(r"Reducing stack by rule\s+(\d+)")
    for line in output.splitlines():
        m = pattern.search(line)
        if m:
            seq.append(int(m.group(1)))
    return seq


def parse_our_reduction_sequence(path: pathlib.Path) -> list[int]:
    seq: list[int] = []
    pattern = re.compile(r"^\s*\d+\.\s+#(\d+)\b")
    for raw in path.read_text(encoding="utf-8").splitlines():
        m = pattern.match(raw)
        if m:
            seq.append(int(m.group(1)))
    return seq


def main() -> int:
    parser = argparse.ArgumentParser(description="测试2：Bison 对拍")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help="文法文件路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-root", default="tests/out/test2", help="输出目录")
    parser.add_argument("--max-steps", type=int, default=200000, help="最大解析步数")
    parser.add_argument("--strict", action="store_true", help="严格模式（无 bison/gcc 时失败）")
    args = parser.parse_args()

    bison = shutil.which("bison")
    gcc = shutil.which("gcc") or shutil.which("cc")
    if not bison or not gcc:
        msg = f"SKIP: 缺少依赖，bison={bool(bison)}, gcc/cc={bool(gcc)}"
        if args.strict:
            print(f"FAIL: {msg}")
            return 1
        print(msg)
        return 0

    root = pathlib.Path(args.workdir).resolve()
    out_root = root / args.out_root
    out_root.mkdir(parents=True, exist_ok=True)

    grammar_path = root / args.grammar
    token_names = parse_token_names_from_grammar(grammar_path)
    if not token_names:
        print("FAIL: 未从 grammar 提取到 %token 名称")
        return 1

    with tempfile.TemporaryDirectory(prefix="yacc_test2_", dir=str(out_root)) as tmp:
        tmp_dir = pathlib.Path(tmp)
        parser_c = tmp_dir / "parser.tab.c"
        parser_h = tmp_dir / "parser.tab.h"
        driver_c = tmp_dir / "token_driver.c"
        exe = tmp_dir / "bison_parser"

        p1 = run_cmd([bison, "-d", "-t", "-o", str(parser_c), str(grammar_path)], cwd=root)
        if p1.returncode != 0:
            print(f"FAIL: bison 生成失败\n{p1.stderr}\n{p1.stdout}")
            return 1
        if not parser_h.exists():
            print(f"FAIL: 未生成头文件: {parser_h}")
            return 1

        driver_c.write_text(build_driver_c(token_names), encoding="utf-8")
        p2 = run_cmd([gcc, "-O2", "-std=c11", str(parser_c), str(driver_c), "-o", str(exe)], cwd=root)
        if p2.returncode != 0:
            print(f"FAIL: bison 基线编译失败\n{p2.stderr}\n{p2.stdout}")
            return 1

        messages: list[str] = []
        report_rows: list[dict[str, str]] = []
        for case in DEFAULT_CASES:
            export_dir = out_root / f"{case.name}_our"
            p_our = run_cmd(
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
            if p_our.returncode != 0:
                print(f"FAIL: {case.name} 运行本实现失败\n{p_our.stderr}\n{p_our.stdout}")
                return 1

            summary = parse_kv_file(export_dir / "summary.txt")
            our_lr1_accept = summary.get("lr1_parse_accepted", "false").lower() == "true"
            our_lalr_accept = summary.get("lalr_parse_accepted", "false").lower() == "true"
            our_lalr_reductions = int(summary.get("lalr_parse_reductions", "0"))
            our_seq = parse_our_reduction_sequence(export_dir / "raw" / "parse_reductions_lalr.txt")

            p_bison = run_cmd([str(exe), str(root / case.tokens)], cwd=root)
            bison_output = f"{p_bison.stdout}\n{p_bison.stderr}"
            bison_accept = p_bison.returncode == 0
            bison_seq = parse_bison_reduction_sequence(bison_output)
            bison_reductions = len(bison_seq)

            case_out_dir = out_root / case.name
            case_out_dir.mkdir(parents=True, exist_ok=True)
            (case_out_dir / "bison_debug.log").write_text(bison_output, encoding="utf-8")
            (case_out_dir / "our_lalr_reductions.txt").write_text(
                "\n".join(str(x) for x in our_seq) + ("\n" if our_seq else ""),
                encoding="utf-8",
            )
            (case_out_dir / "bison_reductions.txt").write_text(
                "\n".join(str(x) for x in bison_seq) + ("\n" if bison_seq else ""),
                encoding="utf-8",
            )

            if our_lalr_accept != bison_accept:
                print(
                    f"FAIL: {case.name} 接受性不一致: our_lalr={our_lalr_accept}, bison={bison_accept}\n"
                    f"{bison_output}"
                )
                return 1
            if our_lr1_accept != case.expect_accept:
                print(f"FAIL: {case.name} LR1 接受性与预期不一致: {our_lr1_accept}")
                return 1
            if our_lalr_reductions != bison_reductions:
                if case.expect_accept:
                    print(
                        f"FAIL: {case.name} 规约次数不一致: our_lalr={our_lalr_reductions}, "
                        f"bison={bison_reductions}\n{bison_output}"
                    )
                    return 1
            sequence_equal = our_seq == bison_seq
            sequence_prefix_ok = bison_seq[: len(our_seq)] == our_seq
            if case.expect_accept:
                if not sequence_equal:
                    mismatch_pos = -1
                    limit = min(len(our_seq), len(bison_seq))
                    for i in range(limit):
                        if our_seq[i] != bison_seq[i]:
                            mismatch_pos = i
                            break
                    if mismatch_pos < 0 and len(our_seq) != len(bison_seq):
                        mismatch_pos = limit
                    print(
                        f"FAIL: {case.name} 规约序列不一致: first_mismatch_index={mismatch_pos}, "
                        f"our_len={len(our_seq)}, bison_len={len(bison_seq)}\n"
                        f"详见: {case_out_dir / 'our_lalr_reductions.txt'} 与 {case_out_dir / 'bison_reductions.txt'}"
                    )
                    return 1
            else:
                # reject 用例中，bison 可能在报错前多做默认规约；要求我方序列是其前缀。
                if not sequence_prefix_ok:
                    mismatch_pos = -1
                    limit = min(len(our_seq), len(bison_seq))
                    for i in range(limit):
                        if our_seq[i] != bison_seq[i]:
                            mismatch_pos = i
                            break
                    if mismatch_pos < 0 and len(our_seq) > len(bison_seq):
                        mismatch_pos = limit
                    print(
                        f"FAIL: {case.name} reject 前缀规约序列不一致: first_mismatch_index={mismatch_pos}, "
                        f"our_len={len(our_seq)}, bison_len={len(bison_seq)}\n"
                        f"详见: {case_out_dir / 'our_lalr_reductions.txt'} 与 {case_out_dir / 'bison_reductions.txt'}"
                    )
                    return 1

            messages.append(
                f"{case.name}: accept={bison_accept}, reductions(lalr/bison)="
                f"{our_lalr_reductions}/{bison_reductions}"
            )
            report_rows.append(
                {
                    "case": case.name,
                    "expect_accept": "true" if case.expect_accept else "false",
                    "our_lr1_accept": "true" if our_lr1_accept else "false",
                    "our_lalr_accept": "true" if our_lalr_accept else "false",
                    "bison_accept": "true" if bison_accept else "false",
                    "our_lalr_reductions": str(our_lalr_reductions),
                    "bison_reductions": str(bison_reductions),
                    "reduction_sequence_equal": "true" if sequence_equal else "false",
                    "reduction_sequence_prefix_ok": "true" if sequence_prefix_ok else "false",
                    "bison_debug_log": str((case_out_dir / "bison_debug.log").relative_to(root)),
                }
            )

        report_tsv = out_root / "compare_report.tsv"
        with report_tsv.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    "case",
                    "expect_accept",
                    "our_lr1_accept",
                    "our_lalr_accept",
                    "bison_accept",
                    "our_lalr_reductions",
                    "bison_reductions",
                    "reduction_sequence_equal",
                    "reduction_sequence_prefix_ok",
                    "bison_debug_log",
                ],
                delimiter="\t",
            )
            writer.writeheader()
            writer.writerows(report_rows)

        report_md = out_root / "compare_report.md"
        md_lines = [
            "# 测试2 严格对拍报告",
            "",
            "| case | expect_accept | our_lr1_accept | our_lalr_accept | bison_accept | our_lalr_reductions | bison_reductions | reduction_sequence_equal | reduction_sequence_prefix_ok |",
            "|---|---|---|---|---|---:|---:|---|",
        ]
        for row in report_rows:
            md_lines.append(
                f"| {row['case']} | {row['expect_accept']} | {row['our_lr1_accept']} | "
                f"{row['our_lalr_accept']} | {row['bison_accept']} | {row['our_lalr_reductions']} | "
                f"{row['bison_reductions']} | {row['reduction_sequence_equal']} | "
                f"{row['reduction_sequence_prefix_ok']} |"
            )
        md_lines.extend(
            [
                "",
                f"- TSV 明细: `{report_tsv.relative_to(root)}`",
                "- 每个 case 的 `bison_debug.log`、`our_lalr_reductions.txt`、`bison_reductions.txt` 在 `tests/out/test2/<case>/`",
            ]
        )
        report_md.write_text("\n".join(md_lines) + "\n", encoding="utf-8")

    print("PASS: 测试2通过（Bison 严格对拍一致）")
    for m in messages:
        print(f"  - {m}")
    print(f"  - report: {out_root / 'compare_report.tsv'}")
    print(f"  - report: {out_root / 'compare_report.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
