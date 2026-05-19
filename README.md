# IntermediateCodeGeneration

C99 语言子集 → Soot Jimple 中间代码编译器后端。

## 项目简介

本项目的后端使用 **Java + Soot** 读取前端（C++ Lex/Yacc）输出的文本规约序列，通过语法制导翻译（SDT）语义栈在 JVM 内存中重建类型与符号信息，最终生成并导出 Jimple 三地址中间代码（`.jimple`）。

## 核心目录结构

```
src/main/java/com/compiler/backend/
├── parser/          # 文本解析引擎
│   ├── TraceParser.java       # 解析 parse_trace_lalr.tsv（移进/规约追踪）
│   ├── ReductionParser.java   # 解析 parse_reductions_lalr.txt（规约序列）
│   └── TokenParser.java       # 解析 .tokens 文件（词法单元 + 字面量）
├── semantic/        # 语义引擎与代码生成
│   ├── TranslationEngine.java # 核心 SDT 引擎（语义栈 + 符号表 + 产生式分发）
│   ├── SymbolTable.java       # 嵌套作用域符号表
│   ├── SemanticValue.java     # 语义值类型体系
│   └── JimpleGenerator.java   # Soot API 封装（Jimple 局部变量与代码生成）
└── model/           # 数据模型
    ├── TraceEntry.java        # 追踪条目
    ├── ReductionRule.java     # 规约规则
    └── TokenRecord.java       # 词法记录

Main.java                 # CLI 入口
c99-yacc-lr-lalr-practice/ # 前端 C++ YACC 项目及测试数据（引用）
```

## 技术栈

| 组件 | 说明 |
|------|------|
| Java 21 | 编译与运行环境 |
| Maven | 构建工具（`pom.xml`） |
| Soot 4.5.0 | Jimple 中间代码生成框架 |
| OpenCSV 5.9 | TSV 追踪文件解析 |
| SLF4J 2.x | 日志门面 |
| maven-assembly-plugin | Fat JAR 打包 |

## 构建

```bash
mvn clean package
```

构建产物位于 `target/`：

| 文件 | 大小 | 用途 |
|------|------|------|
| `IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar` | ~20 MB | **Fat JAR（请使用这个）** |
| `IntermediateCodeGeneration-1.0-SNAPSHOT.jar` | ~32 KB | 仅含项目代码，不含依赖 |

## 运行说明

> **重要：请使用 Fat JAR（带 `-jar-with-dependencies.jar` 后缀的那个，约 20MB）。**
>
> 该包已将 Soot、OpenCSV、SLF4J 等所有依赖全部打入，无需额外安装任何 JAR，开箱即用。

### 单个测试

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar \
    c99-yacc-lr-lalr-practice/tests/golden/test1/decl_multi/raw/
```

成功后会在输入目录下生成 `output.jimple` 文件。路径可使用相对路径或绝对路径。

### 批量测试

```bash
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar --all
```

### 预期输出

控制台会打印每一步的解析统计、符号表内容和生成的 Jimple 代码。测试目录下会出现 `output.jimple`，例如：

```jimple
public class Test extends java.lang.Object
{
    public void <init>() { ... }

    public static void main(java.lang.String[])
    {
        int a, b;
        ...
    }
}
```

## 测试用例

| 用例 | 输入 | 预期 |
|------|------|------|
| `decl_int` | `int;` | 空 body |
| `decl_multi` | `int a, b;` | `int a, b;` |
| `decl_pointer` | `int *p;` | `int p;` |
| `func_param_return` | 函数定义 | clean body |
| `if_else_return` | if/else/return | clean body |
| `invalid_*` | 非法输入 | 跳过或部分生成 |
