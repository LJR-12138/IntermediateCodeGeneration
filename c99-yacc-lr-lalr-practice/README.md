# 编译原理课程实践（YACC 主线）项目说明

## 1. 项目是什么

本项目实现了一个面向 `c99.y` 的 YACC 实践工具链，目标是把语法文件转为可执行的 LR/LALR 分析器能力，并提供结构化导出、可视化和测试回归。

当前主程序为 C++ 实现，核心入口是：

- `./build/src/yacc_parse_tool`

它可以完成：

- 读取并解析 `c99.y`
- 文法预处理（含增广文法）
- First 集计算
- LR(1) 项目与规范族构造
- Action/Goto 表构造与冲突处理
- LR(1) -> LALR(1) 合并
- 基于 token 流的查表解析（LR1/LALR 双轨）
- 按步骤导出产物供后续阶段与可视化使用

---

## 2. 当前完成度

YACC 主线步骤完成情况：

- Step1 输入范围定义：已完成
- Step2 内部数据结构设计：已完成
- Step3 `.y` 文件解析器：已完成
- Step4 文法预处理与增广：已完成
- Step5 First 集：已完成
- Step6 I0/Closure/Goto(I0, X)：已完成
- Step7 LR(1) 规范族与状态图：已完成
- Step8 Action/Goto 与冲突处理：已完成
- Step9 运行时解析器（LR1/LALR）：已完成
- Step10 LALR 状态合并与对比：已完成
- Step11 与 Lex（`c99.l`）深度联调、语义动作规范：未完成（下一阶段）
- Step12 为语义分析/语法树构造预留接口：未完成

---

## 3. 环境需求

基础环境：

- Linux/macOS（当前仓库在 Linux 环境验证）
- `cmake >= 3.16`
- `g++`（支持 C++17）
- `python3 >= 3.8`
- `node >= 18` + `npm`（可视化前端）

可选依赖（用于严格对拍）：

- `bison`
- `gcc` 或 `cc`

---

## 4. 如何运行

### 4.1 构建 C++ 主程序

```bash
cmake -S . -B build
cmake --build build -j
```

### 4.2 运行到 Step10（不喂 token，仅构造与导出）

```bash
./build/src/yacc_parse_tool c99.y --export
```

默认会导出到：

- `artifacts/yacc/step10/c99/`

### 4.3 运行 Step9（喂 token，执行解析）

```bash
./build/src/yacc_parse_tool c99.y \
  --parse-tokens contracts/yacc/tokens/c99_decl_int.tokens \
  --export
```

默认会导出到：

- `artifacts/yacc/step9/c99/`

也可指定目录：

```bash
./build/src/yacc_parse_tool c99.y \
  --parse-tokens contracts/yacc/tokens/c99_func_param_return.tokens \
  --export --export-dir artifacts/yacc/step9/custom_case
```

### 4.4 生成可视化数据并启动前端

先把 `artifacts` 转换为前端 JSON 协议：

```bash
python3 scripts/yacc_visualizer_prepare.py --case c99
```

再启动前端：

```bash
cd visualizer
npm install
npm run dev
```

浏览器访问：

- `http://localhost:5174/?case=c99`

### 4.5 运行测试

> 测试运行时长较久，需要有一定耐心（）

统一入口：

```bash
python3 tests/run_yacc_tests.py --tests 1,2,2f,3,4 --strict-bison
```

常用子集：

```bash
python3 tests/run_yacc_tests.py --tests 1
python3 tests/run_yacc_tests.py --tests 2 --strict-bison
python3 tests/run_yacc_tests.py --tests 2f --strict-bison
python3 tests/run_yacc_tests.py --tests 3,4
python3 tests/run_yacc_tests.py --tests 1 --update-golden
```

---

## 5. 产物与数据流（从输入到可视化）

主流程：

1. 输入文法：`c99.y`
2. 主程序执行：`yacc_parse_tool`
3. 分步导出：`artifacts/yacc/stepX/c99/{summary.txt,raw/,analysis/}`
4. 转换脚本：`scripts/yacc_visualizer_prepare.py`
5. 前端读取：`visualizer/public/data/v1/c99/stepX/data.json`

说明：

- 每个 `stepX` 导出目录都包含 `raw/` 与 `analysis/`，并保留该步骤所需上下文。
- Step9 额外包含解析 trace、规约序列、错误信息，供后续中间代码阶段消费。
- 测试输出在 `tests/out/`，golden 基线在 `tests/golden/test1/`。

---

## 6. 目录详解（分模块文件树）

### 6.1 根目录关键文件

```text
.
├── c99.y                  # YACC 文法主输入文件
├── c99.l                  # Lex 词法输入文件（联调用）
├── CMakeLists.txt         # 顶层 CMake 配置，纳入 src 子工程
├── README.md              # 项目统一说明文档（当前文件）
└── YACC全流程实施文档.md   # YACC 步骤实施记录与过程说明
```

### 6.2 `src/`（C++ 主实现）

```text
src/
├── CMakeLists.txt          # src 子工程构建脚本（库+可执行程序）
├── tools/
│   └── yacc_parse_main.cpp # 命令行入口，串联 Step3~Step10
└── yacc/
    ├── model/              # 文法模型（符号、产生式、索引）
    ├── parser/             # .y 文件解析模块
    ├── preprocess/         # 文法预处理与增广文法
    ├── first/              # First 集与可空性计算
    ├── lr1/                # LR(1) 项目、闭包、规范族
    ├── lalr/               # LR(1)->LALR(1) 状态合并
    ├── table/              # Action/Goto 表构造与校验
    ├── runtime/            # 运行时移进-归约解析器
    └── report/             # Step3~Step10 导出器
```

### 6.3 `contracts/`（协议与样例）

```text
contracts/
├── yacc/
│   └── tokens/             # token 流样例（合法/非法/复杂）
└── yacc-artifact-schema/
    └── v1/                 # 导出产物协议 schema（v1）
```

### 6.4 `scripts/`（辅助脚本）

```text
scripts/
├── yacc_visualizer_prepare.py # artifacts -> visualizer JSON 转换
└── yacc_step9_validate.py     # Step9 自动验收与不变量检查
```

### 6.5 `tests/`（验证体系）

```text
tests/
├── run_yacc_tests.py                        # 测试总入口（1/2/2f/3/4）
├── scripts/
│   ├── _common.py                           # 公共工具与默认样例配置
│   ├── test1_golden_regression.py           # 测试1：golden 快照回归
│   ├── test2_bison_compare.py               # 测试2：行为对拍（bison）
│   ├── test2_full_lalr_automaton_compare.py # 测试2f：完整状态机对拍
│   ├── test3_table_consistency.py           # 测试3：分析表结构一致性
│   └── test4_trace_invariants.py            # 测试4：解析 trace 不变量
├── golden/
│   └── test1/                               # 测试1 基线快照数据
└── out/                                     # 测试运行输出与报告
```

### 6.6 `artifacts/`（运行导出产物）（运行后生成，不上传git）

```text
artifacts/
└── yacc/
    ├── step3/               # .y 解析结果导出
    ├── step4/               # 预处理与增广文法导出
    ├── step5/               # First 集导出
    ├── step6/               # I0/Closure/Goto(I0,X) 导出
    ├── step7/               # LR(1) 规范族与状态转移导出
    ├── step8/               # Action/Goto 与冲突处理导出
    ├── step9/               # token 驱动解析过程导出
    └── step10/              # LALR 合并与 LR1/LALR 对比导出
```

- 每个 `stepX` 下统一结构：`c99/{summary.txt, raw/, analysis/}`

### 6.7 `visualizer/`（Vue 可视化子系统）

```text
visualizer/
├── package.json             # 前端依赖与脚本定义
├── public/
│   └── data/
│       └── v1/              # 前端加载的数据目录
└── src/
    ├── views/               # 各步骤页面（Step1~Step10）
    ├── components/          # 图表面板与通用组件
    ├── stores/              # Pinia 状态管理
    └── utils/               # 数据读取与转换工具
```

### 6.8 `build/`（本地构建输出）

```text
build/
└── ...                      # CMake 生成物（目标文件/可执行文件）
```

---

## 7. 完整验证建议（提交前）

建议按顺序执行：

1. `cmake -S . -B build && cmake --build build -j`
2. `./build/src/yacc_parse_tool c99.y --export`
3. `./build/src/yacc_parse_tool c99.y --parse-tokens contracts/yacc/tokens/c99_func_param_return.tokens --export`
4. `python3 tests/run_yacc_tests.py --tests 1,2,2f,3,4 --strict-bison`
5. `python3 scripts/yacc_visualizer_prepare.py --case c99`

若第 4 步全通过，可认为“YACC 主线实现 + 对拍 + 回归 +可视化数据链路”均可用。

---

## 8. 与后续阶段的衔接

给中间代码/语义阶段的主要输入：

- Step9 解析 trace（移进/规约序列）
- Step9 规约产生式序列（可驱动语义动作触发）
- 错误位置与期望终结符集合（语法错误恢复入口）

下一阶段重点：

- 与 `c99.l` 的 token 编码与属性值协议统一
- 将规约点与语义动作/AST 构建器绑定（Step11）
- 增加端到端样例（源代码 -> token -> 语法 -> 语义）
