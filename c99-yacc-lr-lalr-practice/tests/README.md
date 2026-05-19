# YACC 测试目录（测试 2/3/4）

本目录用于补齐你提出的验证：

- 测试1：基线回归（golden 快照：accept/trace/reduce/error）。
- 测试2：与 `bison` 基线对拍（接受性 + 规约次数）。
- 测试2f：完整 LALR 状态机对拍（状态核心项同构 + 转移 + Action/Goto + 冲突总数）。
- 测试3：Action/Goto 表独立结构校验（LR1 + LALR）。
- 测试4：解析 trace 语义不变式校验（LR1 + LALR）。

## 目录结构

- `tests/run_yacc_tests.py`：统一入口。
- `tests/scripts/test1_golden_regression.py`：测试1。
- `tests/scripts/test2_bison_compare.py`：测试2。
- `tests/scripts/test2_full_lalr_automaton_compare.py`：测试2f。
- `tests/scripts/test3_table_consistency.py`：测试3。
- `tests/scripts/test4_trace_invariants.py`：测试4。
- `tests/scripts/_common.py`：公共方法与默认测试用例。

## 运行方式

在项目根目录执行：

```bash
python3 tests/run_yacc_tests.py
```

仅跑部分测试：

```bash
python3 tests/run_yacc_tests.py --tests 3,4
python3 tests/run_yacc_tests.py --tests 2f --strict-bison
python3 tests/run_yacc_tests.py --tests 2,2f,3,4 --strict-bison
python3 tests/run_yacc_tests.py --tests 1 --update-golden
```

默认测试样例已扩展为多组合法/非法 token 流（声明、指针、函数参数、if-else、struct/enum、缺分号、缺右花括号等）。

## 测试1说明（golden）

- 首次或预期变更后，执行 `--update-golden` 生成/更新快照。
- 日常回归执行 `--tests 1`，若输出与快照不一致则失败。

## 依赖说明

- 测试3/4 只依赖你当前 `./build/src/yacc_parse_tool`。
- 测试2 额外依赖 `bison` + `gcc/cc`。
- 若环境缺少 `bison`，测试2默认输出 `SKIP` 且不失败；可用 `--strict-bison` 改为缺依赖即失败。

```bash
python3 tests/run_yacc_tests.py --tests 2 --strict-bison
```
