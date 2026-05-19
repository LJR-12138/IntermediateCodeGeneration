# IntermediateCodeGeneration — JAR 使用文档

C99 语言子集编译器后端，将 C 源码经 LALR 解析器输出的文本规约序列转换为 **Soot Jimple 中间代码**。

## 目录

- [1. 快速开始](#1-快速开始)
- [2. 构建 JAR](#2-构建-jar)
- [3. 运行方式](#3-运行方式)
- [4. 命令行参数](#4-命令行参数)
- [5. 输入文件规范](#5-输入文件规范)
- [6. 输出说明](#6-输出说明)
- [7. 测试用例](#7-测试用例)
- [8. 项目架构](#8-项目架构)
- [9. 工作流程](#9-工作流程)
- [10. 扩展开发](#10-扩展开发)
- [11. 环境要求与依赖](#11-环境要求与依赖)

---

## 1. 快速开始

```bash
# 1. 构建 Fat JAR
mvn clean package

# 2. 运行全部 11 个测试用例
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar --all

# 3. 运行单个测试用例
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar \
    c99-yacc-lr-lalr-practice/tests/golden/test1/decl_multi/raw/
```

构建产物位于 `target/` 目录：

| 文件 | 大小 | 用途 |
|------|------|------|
| `IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar` | ~20 MB | **Fat JAR（推荐使用）** |
| `IntermediateCodeGeneration-1.0-SNAPSHOT.jar` | ~32 KB | 仅含本项目字节码 |

> **必须使用 Fat JAR**（带 `-jar-with-dependencies` 后缀，约 20MB）。该包已内嵌 Soot、OpenCSV、SLF4J 等全部依赖，无需额外配置 classpath。

---

## 2. 构建 JAR

### 前置条件

- JDK 21+
- Maven 3.8+

### 构建命令

```bash
mvn clean package
```

Maven 将执行编译、测试、打包。`maven-assembly-plugin` 会自动生成包含全部依赖的 Fat JAR。

### Maven 坐标

```xml
<groupId>com.compiler</groupId>
<artifactId>IntermediateCodeGeneration</artifactId>
<version>1.0-SNAPSHOT</version>
<packaging>jar</packaging>
```

主类：`com.compiler.backend.Main`

---

## 3. 运行方式

### 3.1 批量模式（运行全部测试）

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar
```

或显式指定：

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar --all
```

**行为**：依次运行内置的 11 个测试用例，打印每个用例的解析统计、符号表、Jimple 输出，最后输出汇总结果。

### 3.2 单测试模式

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar <testDir>
```

**参数 `<testDir>`**：包含以下两个文件的 `raw/` 目录路径：

| 文件 | 说明 |
|------|------|
| `parse_trace_lalr.tsv` | LALR 解析追踪（TSV） |
| `parse_reductions_lalr.txt` | 归约规则列表 |

程序会自动根据测试目录名在 `contracts/yacc/tokens/` 下查找对应的 Token 文件 `c99_<testName>.tokens`。

**示例**：

```bash
# 相对路径
java -jar target/...jar c99-yacc-lr-lalr-practice/tests/golden/test1/decl_int/raw/

# 绝对路径
java -jar target/...jar /home/user/project/tests/golden/test1/decl_multi/raw/
```

---

## 4. 命令行参数

| 参数 | 说明 |
|------|------|
| _(无参数)_ | 运行全部 11 个内置测试用例 |
| `--all` | 同上，显式运行全部测试 |
| `<testDir>` | 运行单个测试，指定 `raw/` 目录路径 |

---

## 5. 输入文件规范

### 5.1 解析追踪文件 (`parse_trace_lalr.tsv`)

**格式**：Tab 分隔的 TSV 文件，含表头行。

| 列 | 字段 | 类型 | 说明 |
|----|------|------|------|
| 0 | step | int | 步骤编号（1-based） |
| 1 | state | int | LALR 状态机当前状态 |
| 2 | lookahead_id | int | 前瞻符号 ID |
| 3 | lookahead | string | 前瞻符号文本 |
| 4 | action | string | `sN`(移进到状态N)、`rN`(按规则N归约)、`acc`(接受) |
| 5 | production_id | int | 产生式编号（移进/接受时为 -1） |
| 6 | state_stack | string | 状态栈（空格分隔） |
| 7 | symbol_stack | string | 符号栈（空格分隔） |
| 8 | input_index | int | 当前输入 Token 位置索引（0-based） |

**示例**：

```
step	state	lookahead_id	lookahead	action	production_id	state_stack	symbol_stack	input_index
1	0	44	INT	s15	-1	0		0
2	15	157	';'	r99	99	0 15	INT	1
3	30	157	';'	r81	81	0 30	type_specifier	1
```

### 5.2 归约规则文件 (`parse_reductions_lalr.txt`)

**格式**：每行一条产生式规则。

```
<序号>. #<规则ID> <左部> -> <右部空格分隔的符号列表>
```

**示例**：

```
1. #99  type_specifier -> INT
2. #81  declaration_specifiers -> type_specifier
3. #77  declaration -> declaration_specifiers ';'
4. #233 external_declaration -> declaration
5. #230 translation_unit -> external_declaration
```

| 字段 | 说明 |
|------|------|
| 序号 | 规约先后顺序（1-based） |
| 规则ID | 产生式编号，与 trace 中的 `production_id` 对应 |
| 左部 | 非终结符名称 |
| 右部 | 符号列表（空格分隔），空右部表示为 ε |

### 5.3 Token 文件 (`c99_<name>.tokens`)

**格式**：每行一个 Token，空格分隔字段。`#` 开头为注释行。

```
# 注释
<TOKEN_TYPE> <lexeme> [line] [column]
```

**示例** (`c99_decl_multi.tokens`)：

```
# 多变量声明: int a, b;
INT int 1 1
IDENTIFIER a 1 5
',' , 1 6
IDENTIFIER b 1 8
';' ; 1 9
```

| 字段 | 说明 |
|------|------|
| TOKEN_TYPE | Token 类型（大写标识符或 `'x'` 形式的标点） |
| lexeme | 源代码中的实际文本 |
| line | （可选）行号 |
| column | （可选）列号 |

### 5.4 测试目录结构约定

```
tests/golden/test1/
└── <test_name>/
    ├── summary.txt                    # 解析摘要
    ├── raw/
    │   ├── parse_trace_lalr.tsv       # LALR 解析追踪（必需）
    │   ├── parse_reductions_lalr.txt  # 归约规则（必需）
    │   └── parse_error_lalr.txt       # 解析错误信息
    └── output.jimple                  # 生成的 Jimple 输出（运行后产生）

contracts/yacc/tokens/
└── c99_<test_name>.tokens             # Token 文件（按命名约定自动匹配）
```

---

## 6. 输出说明

### 6.1 控制台输出

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
  ──────────────
  [Result] PASSED

═══════════════════════════════════════════════════
  Total: 11  |  Passed: 11  |  Failed: 0
═══════════════════════════════════════════════════
```

### 6.2 Jimple 输出文件 (`output.jimple`)

运行后在测试目录下生成，包含完整的 Soot Jimple 三地址中间代码：

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

### 6.3 退出码

- 正常结束（无论测试是否通过）：退出码 0
- 异常退出（未捕获异常）：退出码非 0

---

## 7. 测试用例

内置 11 个测试用例，涵盖合法与非法 C 输入：

| 测试名 | C 源码 | 类型 | 说明 |
|--------|--------|------|------|
| `decl_int` | `int;` | valid | 空类型声明（无变量名） |
| `decl_multi` | `int a, b;` | valid | 多变量声明，含符号表条目 |
| `decl_pointer` | `int *p;` | valid | 指针声明 |
| `enum_decl` | `enum { RED, BLUE };` | valid | 枚举声明 |
| `struct_decl` | `struct { int x; };` | valid | 结构体声明 |
| `func_param_return` | `int foo(int x) { return x; }` | valid | 函数定义 + 参数 + 返回 |
| `func_return_const` | `int foo() { return 42; }` | valid | 函数定义 + 常量返回 |
| `if_else_return` | `int foo() { if(...) return 1; else return 0; }` | valid | 条件分支 |
| `invalid_if` | 非法 if 语法 | invalid | 前端解析错误，跳过 |
| `invalid_missing_semi` | 缺少分号 | invalid | 前端部分解析，部分生成 |
| `invalid_unclosed_block` | 未闭合代码块 | invalid | 前端部分解析，部分生成 |

---

## 8. 项目架构

```
com.compiler.backend
├── Main.java                    # CLI 入口，测试编排
├── model/
│   ├── TraceEntry.java          # 解析追踪条目（step, state, action, ...）
│   ├── ReductionRule.java       # 归约规则（id, lhs, rhs）
│   └── TokenRecord.java         # 词法记录（type, lexeme, line, col）
├── parser/
│   ├── TraceParser.java         # TSV 追踪文件解析器（OpenCSV）
│   ├── ReductionParser.java     # 归约规则解析器（正则匹配）
│   └── TokenParser.java         # Token 文件解析器（按行拆分）
└── semantic/
    ├── SemanticValue.java       # 语义值类型体系（Type/Id/Const/DeclList/...）
    ├── SymbolTable.java         # 嵌套作用域符号表（栈式 Map）
    ├── TranslationEngine.java   # 核心 SDT 语义翻译引擎
    └── JimpleGenerator.java     # Soot Jimple 代码生成器
```

---

## 9. 工作流程

```
┌─────────────────────────────────────────────────────────┐
│                    Main.main(args)                       │
│                                                         │
│  1. TraceParser    → parse_trace_lalr.tsv   → traces     │
│  2. ReductionParser → parse_reductions_lalr.txt → rules  │
│  3. TokenParser    → c99_*.tokens           → tokens     │
│                                                         │
│  4. TranslationEngine.process(traces)                    │
│     ├── SHIFT  → tokenToSemanticValue() → push 到语义栈   │
│     └── REDUCE → pop(n) → computeSemantic() → push 结果  │
│                         ├── 查 ruleMap 获取产生式          │
│                         ├── LHS+RHS 模式匹配              │
│                         └── 声明规则 → 写入符号表 + Jimple  │
│                                                         │
│  5. JimpleGenerator.emitJimple() → 生成 Jimple 文本       │
│  6. 写入 output.jimple 文件 + 打印到控制台                  │
└─────────────────────────────────────────────────────────┘
```

### 语义栈推演示例

输入 `int a, b;` 的完整语义栈变化过程：

```
Step  Action   Semantic Stack (bottom → top)           说明
────  ──────── ──────────────────────────────────────  ──────────────────
 1    SHIFT    [Type(int)]                              INT Token 压栈
 2    REDUCE   [Type(int)]                              #99 type_specifier → INT (pass-through)
 3    REDUCE   [Type(int)]                              #81 decl_specs → type_specifier
 4    SHIFT    [Type(int), Id(a)]                       IDENTIFIER 'a' 压栈
 5-7  REDUCE   [Type(int), Id(a)]                       链式 pass-through (direct_declarator → declarator → init_declarator)
 8    REDUCE   [Type(int), DeclList[a]]                 #87 init_declarator_list
 9    SHIFT    [Type(int), DeclList[a], Punct(,)]       ',' 压栈
10    SHIFT    [Type(int), DeclList[a], Punct(,), Id(b)] IDENTIFIER 'b' 压栈
11-13 REDUCE   [Type(int), DeclList[a], Punct(,), Id(b)] 链式 pass-through
14    REDUCE   [Type(int), DeclList[a, b]]              #88 合并声明列表
15    SHIFT    [Type(int), DeclList[a, b], Punct(;)]    ';' 压栈
16    REDUCE   [Empty]                                  #78 declaration → 创建符号表 + Jimple locals
```

---

## 10. 扩展开发

### 10.1 添加新的产生式语义规则

在 [TranslationEngine.java](src/main/java/com/compiler/backend/semantic/TranslationEngine.java#L114) 的 `computeSemantic()` 方法中添加新的 LHS+RHS 匹配分支：

```java
// 示例：additive_expression → additive_expression '+' multiplicative_expression
if (lhs.equals("additive_expression") && rhs.size() == 3
        && rhs.get(1).equals("'+'")) {
    SemanticValue left = vals.get(0);
    SemanticValue right = vals.get(2);
    // 生成 Jimple 加法表达式 ...
}
```

### 10.2 添加新的 C 类型支持

在 [JimpleGenerator.java](src/main/java/com/compiler/backend/semantic/JimpleGenerator.java#L152) 的 `mapType()` 方法中添加类型映射：

```java
case "unsigned" -> IntType.v();
case "long long" -> LongType.v();
```

### 10.3 扩展符号表作用域

在进入复合语句/函数体时调用 `symbolTable.enterScope()`，离开时调用 `exitScope()`。

### 10.4 已知限制

- 指针语义仅标记类型，尚未生成引用类型
- 结构体/枚举成员尚未纳入符号表
- 表达式求值未完全接入 Soot 运算 API
- 函数定义体未完整支持

---

## 11. 环境要求与依赖

| 组件 | 版本 | 说明 |
|------|------|------|
| JDK | 21+ | 编译与运行时环境 |
| Maven | 3.8+ | 项目构建 |
| Soot | 4.5.0 | Jimple IR 生成框架 |
| OpenCSV | 5.9 | TSV 追踪文件解析 |
| SLF4J NOP | 2.0.9 | 日志静默绑定 |

### 运行目录要求

JAR 必须从**项目根目录**运行，因为它依赖相对路径约定来查找 Token 文件（`contracts/yacc/tokens/`）和内置测试数据（`c99-yacc-lr-lalr-practice/tests/golden/test1/`）。
