# IntermediateCodeGeneration — 详细技术文档

## 目录

- [1. 项目概述](#1-项目概述)
- [2. 架构设计](#2-架构设计)
- [3. 输入数据格式](#3-输入数据格式)
- [4. 核心模块详解](#4-核心模块详解)
- [5. 数据流与处理流程](#5-数据流与处理流程)
- [6. 技术栈与依赖](#6-技术栈与依赖)
- [7. 构建与运行](#7-构建与运行)
- [8. 测试用例说明](#8-测试用例说明)
- [9. 扩展指南](#9-扩展指南)

---

## 1. 项目概述

本工程是 C99 语言子集编译器的**后端部分**，将 C 语言源码转化为 Soot Jimple 中间表示（Intermediate Representation, IR）。

### 整体架构

```
┌─────────────────────┐      ┌──────────────────────────────────────┐
│  前端 (C++ Lex/Yacc) │      │          后端 (Java + Soot)           │
│                     │      │                                      │
│  c99.l → c99.y      │ 文本 │  parse_trace_lalr.tsv               │
│       ↓             │─────→│  parse_reductions_lalr.txt           │
│  LALR 解析器         │ 输出 │  c99_*.tokens                       │
│       ↓             │      │       ↓                              │
│  文本规约序列         │      │  TraceParser / ReductionParser       │
│                     │      │       ↓                              │
│                     │      │  TranslationEngine (语义栈 SDT)       │
│                     │      │       ↓                              │
│                     │      │  JimpleGenerator (Soot API)           │
│                     │      │       ↓                              │
│                     │      │  output.jimple                       │
└─────────────────────┘      └──────────────────────────────────────┘
```

### 设计动机

前端和后端使用不同语言（C++ 与 Java），不通过内存传递 AST 对象。前端将整个解析过程输出为纯文本文件，后端通过读取这些文本重建语义信息并生成目标代码。这种"文本驱动的语法制导翻译（Text-Driven SDT）"架构的优势是：

- 前后端完全解耦，可独立开发、调试和替换
- 文本格式便于人工审查和自动化测试
- 避免了跨语言 FFI 的复杂性

---

## 2. 架构设计

### 2.1 语义栈（Semantic Stack）

本后端不构建传统的 AST 树，而是维护一个 `Stack<SemanticValue>` 语义栈。核心机制如下：

```
规约规则: A → X₁ X₂ ... Xₙ

动作: pop(Xₙ), ..., pop(X₂), pop(X₁) → compute(A) → push(A)
```

每一步规约都从语义栈中弹出 RHS 对应数量的语义值，根据产生式规则计算新的语义值，再将其压回栈顶。

### 2.2 语义值体系

`SemanticValue` 类体系表示栈上不同种类的语义信息：

| 类型 | 说明 | 示例 |
|------|------|------|
| `TypeValue` | 类型关键字 | `int`, `float`, `char` |
| `IdentifierValue` | 变量/函数标识符 | `a`, `foo`, `x` |
| `ConstantValue` | 字面常量 | `42`, `"hello"` |
| `DeclListValue` | 声明列表 | `DeclList[a, b]` |
| `PunctuationValue` | 标点符号（无语义） | `;`, `,`, `*` |
| `EmptyValue` | 空值占位 | — |

### 2.3 产生式模式匹配

`TranslationEngine.computeSemantic()` 通过 LHS 名称 + RHS 模式组合匹配产生式，分发到对应的语义动作。当前已实现的核心规则：

- **类型推导链**: `type_specifier`, `declaration_specifiers`
- **声明器链**: `direct_declarator`, `declarator`, `init_declarator`, `init_declarator_list`
- **声明生成**: `declaration → declaration_specifiers init_declarator_list ';'`
- **翻译单元**: `translation_unit`, `external_declaration`
- **表达式传递链**: 所有单元素非终结符自动 pass-through

### 2.4 符号表

`SymbolTable` 支持嵌套作用域（当前为全局 + 单层函数），通过栈式 `List<Map<String, Symbol>>` 实现：

- `enterScope()` / `exitScope()` — 进入/退出作用域
- `insert(name, type, kind)` — 注册符号（VARIABLE / FUNCTION / PARAMETER）
- `lookup(name)` — 由内向外查找符号
- `contains(name)` — 检查符号是否存在

---

## 3. 输入数据格式

### 3.1 规约序列文件 (`parse_reductions_lalr.txt`)

```
seq. #rule_id LHS -> RHS_symbols...
```

示例：

```
1. #99 type_specifier -> INT
2. #81 declaration_specifiers -> type_specifier
3. #143 direct_declarator -> IDENTIFIER
```

| 字段 | 说明 |
|------|------|
| `seq` | 规约顺序号 (1-based) |
| `rule_id` | 产生式规则编号（由 Yacc 分配） |
| `LHS` | 产生式左部（非终结符） |
| `RHS` | 产生式右部（符号序列，空格分隔） |

### 3.2 解析追踪文件 (`parse_trace_lalr.tsv`)

TSV 格式文件，每行代表一步解析动作：

| 列名 | 说明 |
|------|------|
| `step` | 步骤编号 |
| `state` | LALR 状态机当前状态 |
| `action` | 动作类型（`sN` = shift 到状态 N，`rN` = reduce 规则 N，`acc` = 接受） |
| `production_id` | 规约时的产生式 ID |
| `input_index` | 当前输入的 Token 序号 (0-based) |

Shift 动作通过 `input_index` 关联到 `.tokens` 文件中对应位置的 Token，从而获取实际字面量。

### 3.3 Token 文件 (`.tokens`)

```
# 注释行
TOKEN_TYPE lexeme line column
```

示例 (`c99_decl_multi.tokens`)：

```
INT int 1 1
IDENTIFIER a 1 5
',' , 1 6
IDENTIFIER b 1 8
';' ; 1 9
```

Token 按出现顺序排列，`input_index` 即为其在文件中的行序（注释行不计入）。

---

## 4. 核心模块详解

### 4.1 `parser/` — 文本解析引擎

#### `TraceParser`
- 使用 OpenCSV 读取 Tab 分隔的 TSV 文件
- 解析 `state_stack` 字段为 `List<Integer>`
- 解析 `symbol_stack` 字段为 `List<String>`
- 返回 `List<TraceEntry>`

#### `ReductionParser`
- 使用正则 `^(\d+)\.\s+#(\d+)\s+(\S+)\s+->\s+(.*)$` 逐行匹配
- RHS 按空格拆分为符号列表
- 返回 `List<ReductionRule>`

#### `TokenParser`
- 跳过 `#` 开头的注释行和空行
- 按空格拆分获取 TYPE / LEXEME / LINE / COL
- 返回按索引组织的 `List<TokenRecord>`

### 4.2 `semantic/` — 语义引擎与代码生成

#### `TranslationEngine`
- **shift 处理**: 根据 Token 类型创建对应的 `SemanticValue` 并压入语义栈
  - `INT/FLOAT/CHAR/...` → `TypeValue`
  - `IDENTIFIER` → `IdentifierValue`
  - `CONSTANT/STRING_LITERAL` → `ConstantValue`
  - 其他 → `PunctuationValue`
- **reduce 处理**: 通过 `ruleMap` 查找产生式 → pop RHS → `computeSemantic()` → push LHS
- **computeSemantic()**: 按 LHS 名称 + RHS 模式匹配分发语义动作
  - 声明规则：创建符号表条目 + 调用 `JimpleGenerator.declareLocal()`
  - 链式规则：pass-through
  - 未知规则：默认 pass-through（安全降级）

#### `SymbolTable`
- 栈式作用域结构
- 符号记录 `name / type / kind`
- 支持 insert / lookup / contains / enterScope / exitScope

#### `JimpleGenerator`
- **初始化**: `G.reset()` → Scene 配置 → 创建 `SootClass("Test")` → 创建 `static void main(String[])` → 创建 `JimpleBody`
- **局部变量**: `Jimple.v().newLocal(name, jimpleType)` → `body.getLocals().add()`
- **表达式**: `Jimple.v().newAddExpr/SubExpr/MulExpr/DivExpr/NegExpr()`
- **赋值**: `Jimple.v().newAssignStmt(lhs, rhs)`
- **Jimple 输出**: `Printer.v().printTo(body, pw)` 生成标准 Jimple 文本

### 4.3 `model/` — 数据模型

- `TraceEntry`: step, state, lookahead, action, productionId, stateStack, symbolStack, inputIndex
- `ReductionRule`: id, sequence, lhs, rhs
- `TokenRecord`: index, type, lexeme, line, col

---

## 5. 数据流与处理流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Main.main(args)                              │
│                                                                     │
│  1. 读取命令行参数，确定测试目录                                        │
│  2. TraceParser.parse(parse_trace_lalr.tsv)    → List<TraceEntry>    │
│  3. ReductionParser.parse(parse_reductions_lalr.txt) → List<Rule>    │
│  4. TokenParser.parse(c99_*.tokens)            → List<TokenRecord>   │
│                                                                     │
│  5. TranslationEngine(tokens, rules)                                 │
│     ├── process(trace)                                              │
│     │   ├── for each TraceEntry:                                    │
│     │   │   ├── SHIFT → push SemanticValue(token.lexeme)            │
│     │   │   └── REDUCE → pop(n) → compute → push(result)            │
│     │   └── end                                                     │
│     ├── getSymbolTable()               → 输出符号表                   │
│     └── getJimpleOutput()              → 输出 Jimple 代码             │
│         ├── JimpleGenerator.initialize()                            │
│         ├── JimpleGenerator.declareLocal(name, type)  [每个变量]      │
│         └── JimpleGenerator.emitJimple() → String                    │
│                                                                     │
│  6. 写入 output.jimple 文件 + 打印到控制台                              │
└─────────────────────────────────────────────────────────────────────┘
```

### 语义栈推演示例 (`int a, b;`)

```
Step  Action   Semantic Stack (bottom → top)       说明
────  ──────── ──────────────────────────────────  ──────────────────
 1    SHIFT    [Type(int)]                          INT Token
 2    REDUCE   [Type(int)]                          #99 type_specifier → INT
 3    REDUCE   [Type(int)]                          #81 decl_specs → type_specifier
 4    SHIFT    [Type(int), Id(a)]                   IDENTIFIER 'a'
 5    REDUCE   [Type(int), Id(a)]                   #143 direct_declarator → IDENTIFIER
 6    REDUCE   [Type(int), Id(a)]                   #142 declarator → direct_declarator
 7    REDUCE   [Type(int), Id(a)]                   #89 init_declarator → declarator
 8    REDUCE   [Type(int), DeclList[a]]             #87 init_declarator_list → init_decl
 9    SHIFT    [Type(int), DeclList[a], Punct(,)]   ','
10    SHIFT    [Type(int), DeclList[a], Punct(,), Id(b)]  IDENTIFIER 'b'
11-13 REDUCE   [Type(int), DeclList[a], Punct(,), Id(b)]  链式 pass-through
14    REDUCE   [Type(int), DeclList[a, b]]          #88 → 合并声明列表
15    SHIFT    [Type(int), DeclList[a, b], Punct(;)]  ';'
16    REDUCE   [Empty]                              #78 → 声明完成，生成 Jimple Local
17    REDUCE   [Empty]                              #233 external_declaration
18    REDUCE   [Empty]                              #230 translation_unit → 接受
```

---

## 6. 技术栈与依赖

| 组件 | 版本 | 用途 |
|------|------|------|
| Java | 21 | 编译与运行时 |
| Maven | 3.9+ | 项目构建与依赖管理 |
| Soot | 4.5.0 | Jimple IR 生成框架（`G.reset()`, `Scene`, `SootClass`, `SootMethod`, `JimpleBody`, `Local`, `Printer`） |
| OpenCSV | 5.9 | TSV 文件解析（Tab 分隔符、引号转义） |
| SLF4J | 2.0.9 (nop) | Soot 框架的日志依赖 |
| maven-assembly-plugin | 3.7.1 | Fat JAR 打包（`jar-with-dependencies`） |

### Maven 坐标

```xml
<groupId>com.compiler</groupId>
<artifactId>IntermediateCodeGeneration</artifactId>
<version>1.0-SNAPSHOT</version>
```

---

## 7. 构建与运行

### 7.1 编译打包

```bash
mvn clean package
```

构建产物：

| 文件 | 大小 | 说明 |
|------|------|------|
| `target/IntermediateCodeGeneration-1.0-SNAPSHOT.jar` | ~32 KB | 仅含项目字节码 |
| `target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar` | ~20 MB | **Fat JAR**，含所有依赖 |

### 7.2 运行

**必须使用带 `-jar-with-dependencies.jar` 后缀的 Fat JAR。**

#### 单个测试用例

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar \
    c99-yacc-lr-lalr-practice/tests/golden/test1/decl_multi/raw/
```

- 参数为包含 `parse_trace_lalr.tsv` 和 `parse_reductions_lalr.txt` 的目录路径
- 程序自动在 `contracts/yacc/tokens/` 目录下查找同名 `.tokens` 文件
- 运行成功后在输入目录生成 `output.jimple`

#### 批量测试（全部 11 个用例）

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar --all
```

#### 预期输出

控制台输出包含：
- 各步骤的解析统计（trace entries / reductions / tokens 数量）
- 符号表最终内容
- 完整 Jimple 代码
- 测试通过/失败汇总

---

## 8. 测试用例说明

测试数据位于 `c99-yacc-lr-lalr-practice/tests/golden/test1/`，Token 文件位于 `c99-yacc-lr-lalr-practice/contracts/yacc/tokens/`。

| 目录 | C 源码 | 类型 | 符号表预期 |
|------|--------|------|------------|
| `decl_int/` | `int;` | 空声明 | 0 符号 |
| `decl_multi/` | `int a, b;` | 多变量声明 | `int a`, `int b` |
| `decl_pointer/` | `int *p;` | 指针声明 | `int p` |
| `enum_decl/` | `enum { RED, BLUE };` | 枚举声明 | 0 符号（待扩展） |
| `struct_decl/` | `struct { int x; };` | 结构体声明 | 0 符号（待扩展） |
| `func_param_return/` | `int foo(int x) { return x; }` | 函数+参数+返回 | 0 符号（待扩展） |
| `func_return_const/` | `int foo() { return 42; }` | 函数+常量返回 | 0 符号（待扩展） |
| `if_else_return/` | `int foo() { if(...) return 1; else return 0; }` | 条件分支 | 0 符号（待扩展） |
| `invalid_if/` | 非法 if 语法 | 前端解析错误 | 跳过 |
| `invalid_missing_semi/` | 缺少分号 | 前端部分解析 | 部分生成 |
| `invalid_unclosed_block/` | 未闭合块 | 前端部分解析 | 部分生成 |

---

## 9. 扩展指南

### 9.1 添加新的产生式处理

在 `TranslationEngine.computeSemantic()` 中添加新的匹配分支：

```java
// -- additive_expression -> additive_expression '+' multiplicative_expression --
if (lhs.equals("additive_expression") && rhs.size() == 3
        && rhs.get(1).equals("'+'")) {
    SemanticValue left = vals.get(0);
    SemanticValue right = vals.get(2);
    Value result = jimpleGen.generateAddExpr(
            jimpleGen.getLocals().get(left.getStringValue()),
            jimpleGen.getLocals().get(right.getStringValue()));
    Local tmp = jimpleGen.generateTempLocal("int");
    jimpleGen.generateAssignment(tmp.getName(), result);
    return SemanticValue.identifierValue(tmp.getName());
}
```

### 9.2 扩展符号表作用域

在进入函数体/复合语句时调用 `symbolTable.enterScope()`，离开时调用 `exitScope()`。

### 9.3 支持新的 C 类型

在 `JimpleGenerator.mapType()` 的 switch 中添加新类型映射。Jimple 标准类型包括 `IntType`, `FloatType`, `DoubleType`, `LongType`, `ShortType`, `ByteType`, `BooleanType`, `RefType`。

### 9.4 添加函数定义支持

当前 `function_definition` 规则尚未处理。需要：
1. 解析函数名和参数列表创建 `SootMethod`
2. 处理 `compound_statement` 内部的 block_items
3. 生成方法体中的 Jimple 语句

### 9.5 已知限制

- 指针语义仅在符号表中标记类型，尚未生成 `RefType` 引用
- 结构体和枚举成员的符号尚未纳入符号表
- 表达式求值尚未接入 Soot 运算 API
- 函数定义不支持（待 Step 4 后续迭代）
