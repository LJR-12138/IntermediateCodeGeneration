# IntermediateCodeGeneration JAR 包使用文档

## 目录

- [1. 概述](#1-概述)
- [2. 环境要求](#2-环境要求)
- [3. 构建 JAR](#3-构建-jar)
- [4. 运行](#4-运行)
- [5. 命令行参数](#5-命令行参数)
- [6. 输入文件规范](#6-输入文件规范)
- [7. 输出说明](#7-输出说明)
- [8. 内置测试用例](#8-内置测试用例)
- [9. 工作流程](#9-工作流程)
- [10. 常见问题](#10-常见问题)

---

## 1. 概述

`IntermediateCodeGeneration` 是一个 C99 编译器后端，将 C 语言源码经 LALR 解析器输出的文本规约序列转换为 **Soot Jimple 三地址中间代码**。

```
C 源码 → (前端 Lex/Yacc) → 文本规约序列 → (本 JAR) → Jimple IR
```

前端与后端通过纯文本文件解耦，前后端可独立开发、调试和替换。

---

## 2. 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| JDK | 21 或更高 | 编译与运行时 |
| Maven | 3.8+ | 构建工具（仅构建时需要） |
| 操作系统 | 任意 | Windows / Linux / macOS 均可 |

运行时仅需 JRE 21+，无需 Maven。

---

## 3. 构建 JAR

```bash
mvn clean package
```

构建产物位于 `target/` 目录：

| 文件 | 大小 | 说明 |
|------|------|------|
| `IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar` | ~20 MB | **Fat JAR（请使用此文件）** |
| `IntermediateCodeGeneration-1.0-SNAPSHOT.jar` | ~32 KB | 仅含本项目代码，不含依赖 |

> **必须使用 Fat JAR**（文件名包含 `-jar-with-dependencies`）。该包已内嵌 Soot 4.5.0、OpenCSV 5.9、SLF4J 等全部依赖，开箱即用。

### JAR 基本信息

| 属性 | 值 |
|------|-----|
| GroupId | `com.compiler` |
| ArtifactId | `IntermediateCodeGeneration` |
| Version | `1.0-SNAPSHOT` |
| 主类 | `com.compiler.backend.Main` |
| 打包方式 | `jar` |

---

## 4. 运行

### 4.1 运行全部测试

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar
```

或显式指定：

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar --all
```

依次运行 11 个内置测试用例，打印每个用例的解析统计、符号表、生成的 Jimple 代码，最后输出汇总结果。

### 4.2 运行单个测试

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar <testDir>
```

**`<testDir>`** 是包含以下文件的 `raw/` 目录路径：

- `parse_trace_lalr.tsv` — LALR 解析追踪文件
- `parse_reductions_lalr.txt` — 归约规则文件

程序将自动根据测试目录名在 `contracts/yacc/tokens/` 下查找对应的 Token 文件。

**示例**：

```bash
# 相对路径
java -jar target/...jar c99-yacc-lr-lalr-practice/tests/golden/test1/decl_int/raw/

# 绝对路径
java -jar target/...jar /absolute/path/to/tests/golden/test1/decl_multi/raw/
```

### 4.3 运行目录要求

JAR 需从**项目根目录**运行，因为程序内部通过相对路径定位内置测试数据和 Token 文件：

```
项目根目录/
├── target/
│   └── ...jar              ← 在此目录下执行 java -jar
├── c99-yacc-lr-lalr-practice/
│   ├── tests/golden/test1/  ← 内置测试数据
│   └── contracts/yacc/tokens/ ← Token 文件
└── src/
```

---

## 5. 命令行参数

| 参数 | 行为 |
|------|------|
| _(无参数)_ | 运行全部 11 个内置测试用例 |
| `--all` | 同上，显式指定批量模式 |
| `<testDir>` | 运行指定目录的单个测试用例 |

---

## 6. 输入文件规范

### 6.1 解析追踪文件 (`parse_trace_lalr.tsv`)

**格式**：Tab 分隔的 TSV 文件，首行为列名。

| 列 | 字段 | 类型 | 说明 |
|----|------|------|------|
| 0 | step | int | 步骤编号（1-based） |
| 1 | state | int | LALR 状态机当前状态号 |
| 2 | lookahead_id | int | 前瞻符号的内部 ID |
| 3 | lookahead | string | 前瞻符号文本表示 |
| 4 | action | string | 动作：`sN`=移进到状态N、`rN`=按规则N归约、`acc`=接受 |
| 5 | production_id | int | 产生式编号（移进/接受时为 -1） |
| 6 | state_stack | string | 状态栈（空格分隔） |
| 7 | symbol_stack | string | 符号栈（空格分隔） |
| 8 | input_index | int | 当前消耗到的 Token 位置（0-based） |

**完整示例**：

```
step	state	lookahead_id	lookahead	action	production_id	state_stack	symbol_stack	input_index
1	0	44	INT	s15	-1	0		0
2	15	157	';'	r99	99	0 15	INT	1
3	30	157	';'	r81	81	0 30	type_specifier	1
4	29	157	';'	s49	-1	0 29	declaration_specifiers	1
5	49	131	$	r77	77	0 29 49	declaration_specifiers ';'	2
6	33	131	$	r233	233	0 33	declaration	2
7	26	131	$	r230	230	0 26	external_declaration	2
8	27	131	$	acc	-1	0 27	translation_unit	2
```

### 6.2 归约规则文件 (`parse_reductions_lalr.txt`)

**格式**：每行一条产生式规则。

```
<序号>. #<规则ID> <左部非终结符> -> <右部符号列表>
```

右部为空时表示 ε 产生式。

**完整示例**：

```
1. #99 type_specifier -> INT
2. #81 declaration_specifiers -> type_specifier
3. #77 declaration -> declaration_specifiers ';'
4. #233 external_declaration -> declaration
5. #230 translation_unit -> external_declaration
```

| 字段 | 说明 |
|------|------|
| 序号 | 规约的先后顺序（1-based） |
| 规则ID | 产生式编号，与 trace 文件中 `production_id` 列对应 |
| LHS | 产生式左部（非终结符名称） |
| RHS | 产生式右部（空格分隔的符号列表） |

### 6.3 Token 文件 (`c99_<name>.tokens`)

**格式**：每行一个词法单元，空格分隔。`#` 开头的行为注释（自动跳过）。

```
<TOKEN_TYPE> <lexeme> [行号] [列号]
```

**完整示例** (`c99_decl_multi.tokens`)：

```
# 多变量声明样例：int a, b;
INT int 1 1
IDENTIFIER a 1 5
',' , 1 6
IDENTIFIER b 1 8
';' ; 1 9
```

| 字段 | 必填 | 说明 |
|------|------|------|
| TOKEN_TYPE | 是 | Token 类型标识。大写（如 `INT`、`IDENTIFIER`）表示词法类别，`'x'` 形式表示标点符号 |
| lexeme | 是 | 源代码中对应的原始文本 |
| 行号 | 否 | Token 在源码中的行位置 |
| 列号 | 否 | Token 在源码中的列位置 |

Token 在文件中的顺序与 `trace` 的 `input_index` 字段一一对应（注释行不计入）。

### 6.4 目录结构约定

```
tests/golden/test1/
└── <test_name>/
    ├── summary.txt                    # 解析摘要信息
    ├── raw/
    │   ├── parse_trace_lalr.tsv       # 【必需】LALR 解析追踪
    │   ├── parse_reductions_lalr.txt  # 【必需】归约规则列表
    │   └── parse_error_lalr.txt       # 解析错误信息
    └── output.jimple                  # 【输出】运行后生成的 Jimple 文件

contracts/yacc/tokens/
└── c99_<test_name>.tokens             # 【必需】Token 文件（按命名约定自动匹配）
```

---

## 7. 输出说明

### 7.1 控制台输出

运行时会依次打印每个阶段的信息：

```
╔══════════════════════════════════════════════════╗
║  C99 Compiler Backend — Java + Soot (Step 5)    ║
╚══════════════════════════════════════════════════╝

──────────────────────────────────────────
  Test: decl_multi
──────────────────────────────────────────
  [Parse] Trace: 18 entries
  [Parse] Reductions: 11 rules
  [Parse] Tokens: 5 tokens
  [SymbolTable] 2 symbols: [int a (VARIABLE), int b (VARIABLE)]
  [Output] Jimple written to: .../decl_multi/output.jimple
  ─── Jimple ───
  public class Test extends java.lang.Object
  {
      ...
      int a;
      int b;
      ...
  }
  ──────────────
  [Result] PASSED

═══════════════════════════════════════════════════
  Total: 11  |  Passed: 11  |  Failed: 0
═══════════════════════════════════════════════════
```

### 7.2 Jimple 输出文件

在测试目录的父目录下生成 `output.jimple` 文件，内容为 Soot Jimple 标准格式的三地址中间代码：

```jimple
public class Test extends java.lang.Object
{
    public void <init>()
    {
        Test l0;
        l0 := @this: Test;
        specialinvoke l0.<java.lang.Object: void <init>()>();
        return;
    }

    public static void main(java.lang.String[])
    {
        java.lang.String[] l0;
        int a;
        int b;

        l0 := @parameter0: java.lang.String[];
        return;
    }
}
```

### 7.3 退出码

| 情况 | 退出码 |
|------|--------|
| 正常完成（无论测试通过与否） | 0 |
| 未捕获异常（如文件不存在） | 非 0，异常信息打印到 stderr |

---

## 8. 内置测试用例

| 测试名 | C 源码 | 类型 | 说明 |
|--------|--------|------|------|
| `decl_int` | `int;` | valid | 空类型声明（无变量名） |
| `decl_multi` | `int a, b;` | valid | 多变量声明，产生符号表条目和 Jimple 局部变量 |
| `decl_pointer` | `int *p;` | valid | 指针声明 |
| `enum_decl` | `enum { RED, BLUE };` | valid | 枚举类型声明 |
| `struct_decl` | `struct { int x; };` | valid | 结构体类型声明 |
| `func_param_return` | `int foo(int x) { return x; }` | valid | 函数定义 + 参数 + 返回语句 |
| `func_return_const` | `int foo() { return 42; }` | valid | 函数定义 + 常量返回 |
| `if_else_return` | `int foo() { if(...) return 1; else return 0; }` | valid | if-else 条件分支 |
| `invalid_if` | 非法 if 语法 | invalid | 前端解析错误，后端跳过 |
| `invalid_missing_semi` | 缺少分号 | invalid | 部分解析，部分生成 |
| `invalid_unclosed_block` | 未闭合代码块 | invalid | 部分解析，部分生成 |

---

## 9. 工作流程

JAR 内部的处理管线分为 5 个阶段：

```
┌──────────────────────────────────────────────────────────────┐
│                      Main.main(args)                         │
│                                                              │
│  阶段1: TraceParser.parse(parse_trace_lalr.tsv)              │
│         → List<TraceEntry>  (8 列 TSV → 结构化对象)           │
│                                                              │
│  阶段2: ReductionParser.parse(parse_reductions_lalr.txt)      │
│         → List<ReductionRule> (正则匹配 → 产生式规则)          │
│                                                              │
│  阶段3: TokenParser.parse(c99_*.tokens)                      │
│         → List<TokenRecord> (按行解析 → 词法单元序列)          │
│                                                              │
│  阶段4: TranslationEngine.process(traces)                     │
│         ├── SHIFT  → 根据 Token 类型创建 SemanticValue 并压栈  │
│         │           INT/FLOAT/... → TypeValue                │
│         │           IDENTIFIER    → IdentifierValue          │
│         │           CONSTANT      → ConstantValue             │
│         └── REDUCE → 弹出 RHS 数量的栈元素                     │
│                      → computeSemantic(lhs, rhs, vals)       │
│                      → 模式匹配产生式，执行语义动作              │
│                      → 声明规则写入 SymbolTable + Jimple       │
│                                                              │
│  阶段5: JimpleGenerator.emitJimple()                         │
│         → Soot Printer 输出 Jimple 文本                       │
│         → 写入 output.jimple                                 │
└──────────────────────────────────────────────────────────────┘
```

### 语义栈推演示例（输入 `int a, b;`）

```
Step  Action   栈 (底→顶)                           说明
────  ──────── ─────────────────────────────────── ─────────────────
  1   SHIFT    [Type(int)]                         INT Token 压栈
  2   REDUCE   [Type(int)]                         #99: type_specifier → INT
  3   REDUCE   [Type(int)]                         #81: decl_specs → type_specifier
  4   SHIFT    [Type(int), Id(a)]                  IDENTIFIER 'a' 压栈
  5-7 REDUCE   [Type(int), Id(a)]                  链式 pass-through
  8   REDUCE   [Type(int), DeclList[a]]            #87: init_declarator_list
  9   SHIFT    [Type(int), DeclList[a], Punct(,)]  ',' 压栈
 10   SHIFT    [..., Id(b)]                        IDENTIFIER 'b' 压栈
 11-13 REDUCE  [Type(int), DeclList[a], Punct(,), Id(b)]
 14   REDUCE   [Type(int), DeclList[a, b]]         #88: 合并声明列表
 15   SHIFT    [Type(int), DeclList[a, b], Punct(;)]  ';' 压栈
 16   REDUCE   [Empty]                             #78: declaration
                                                   → 创建 SymbolTable 条目
                                                   → 生成 Jimple 局部变量
```

---

## 10. 常见问题

### Q: 运行时报 `ClassNotFoundException`？

使用 Fat JAR（文件名含 `-jar-with-dependencies`），而非仅含本项目代码的瘦 JAR。

### Q: 运行单个测试时报文件找不到？

确保传入的是 `raw/` 目录路径，且该目录下存在 `parse_trace_lalr.tsv` 和 `parse_reductions_lalr.txt`。同时确认 `contracts/yacc/tokens/` 下有对应的 `c99_<testName>.tokens` 文件。

### Q: 生成的 `output.jimple` 为空或变量缺失？

检查 `parse_reductions_lalr.txt` 是否为空。如果前端解析出错导致归约规则为空，后端会跳过代码生成。查看 `summary.txt` 中的 `lalr_parse_accepted` 字段确认前端解析是否成功。

### Q: 能否处理任意 C99 代码？

当前仅支持 C99 的一个子集，包括变量声明、基本类型、指针声明、简单函数定义和 if-else 控制流。结构体成员、枚举值、复杂表达式等支持有限。

### Q: 如何添加新的测试用例？

1. 在 `tests/golden/test1/` 下新建 `<name>/raw/` 目录
2. 放入前端生成的 `parse_trace_lalr.tsv` 和 `parse_reductions_lalr.txt`
3. 在 `contracts/yacc/tokens/` 下放入 `c99_<name>.tokens`
4. 运行 `java -jar ...jar tests/golden/test1/<name>/raw/`

### Q: 需要联网吗？

不需要。JAR 完全离线运行，所有依赖已内嵌在 Fat JAR 中。
