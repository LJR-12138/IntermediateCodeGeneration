# Intermediate Code Generation Backend

将 yacc 产生的 CST JSON 转换为 Soot Jimple 中间代码（三地址码），支持**通过配置文件切换目标语言**，无需修改 Java 源码。

## 流水线总览

```
C 源码 → SeuLex (词法) → yacc (语法) → CST JSON → AstPruner → AST → JimpleGenerator → output.jimple
              ↑                  ↑                   ↑
           c99.l              c99.y            c99.sem.json
```

- **LEX/YACC 是通用工具** — 换一套 `.l` / `.y` 规则文件即可解析任意语言
- **后端也是通用的** — 通过 `.sem.json` 语义配置文件，将语言相关的 CST 节点名映射到语言无关的语义角色，实现零代码切换

## 项目结构

```
src/main/java/com/compiler/backend/
├── Main.java                         # 入口：解析CLI参数，串联流水线
├── ast/
│   ├── AstNode.java                  # AST节点接口
│   ├── TranslationUnitNode.java      # 根节点
│   ├── FunctionDefNode.java          # 函数定义
│   ├── VarDeclNode.java              # 变量声明（含数组初始化）
│   ├── BlockNode.java                # 语句块
│   ├── IfStmtNode.java               # if-else
│   ├── WhileStmtNode.java            # while循环
│   ├── ReturnStmtNode.java           # return
│   ├── BinaryExprNode.java           # 二元表达式
│   ├── UnaryExprNode.java            # 一元表达式
│   ├── FunctionCallNode.java         # 函数调用
│   ├── ArraySubscriptNode.java       # 数组下标
│   ├── ArrayInitNode.java            # 数组初始化列表
│   ├── IdentifierNode.java           # 标识符引用
│   ├── IntLiteralNode.java           # 整数字面量
│   ├── StringLiteralNode.java        # 字符串字面量
│   ├── JsonNode.java                 # JSON解析中间表示
│   ├── AstJsonParser.java            # CST JSON解析器
│   ├── AstPruner.java                # CST→AST剪枝器（核心）
│   └── AstPrinter.java               # AST调试打印
├── config/
│   └── LanguageConfig.java           # 语义配置（从.sem.json加载或使用C99默认）
├── generator/
│   └── JimpleGenerator.java          # Jimple代码生成器
└── semantic/
    ├── SymbolTable.java              # 符号表（作用域栈）
    ├── TypeMapper.java               # 类型映射（委托给LanguageConfig）
    └── SemanticException.java        # 语义异常

src/test/
├── java/com/compiler/backend/ast/
│   ├── AstJsonParserTest.java
│   └── AstPrunerTest.java
└── resources/
    ├── c99.sem.json                  # C99语义配置（示例）
    └── sample_ast_lalr.json          # 测试用CST JSON
```

## 核心设计：语义配置文件

### 为什么要 `.sem.json`？

LEX 和 YACC 是真正的生成器——换一套 `.l` / `.y` 规则文件就能解析任意语言。但后端此前在 `AstPruner` 中硬编码了 C99 的文法产生式名称（如 `"translation_unit"`、`"IF"`），导致换了语言后端就失效。

**`.sem.json` 将语言相关的 CST 名称映射到语言无关的语义角色**。换语言时，只需提供新的 `.sem.json`，无需改动 Java 代码。

### 配置文件结构

```json
{
  "language": "c99",
  "productions": {
    "translationUnit": "translation_unit",
    "functionDefinition": "function_definition",
    "declaration": "declaration",
    ...
  },
  "tokens": {
    "keywords": { "if": "IF", "else": "ELSE", "while": "WHILE", ... },
    "typeKeywords": ["INT", "CHAR", "VOID", ...],
    "leafTypes": {
      "identifier": "IDENTIFIER",
      "integerConstant": "CONSTANT",
      "stringLiteral": "STRING_LITERAL"
    },
    "binaryOperatorTokens": ["EQ_OP", "NE_OP", ...],
    "unaryOperatorTokens": ["INC_OP", "DEC_OP"],
    "postfixOperatorTokens": ["INC_OP", "DEC_OP"]
  },
  "typeSystem": {
    "int":  { "sootType": "IntType",    "aliases": ["signed", "unsigned", ...] },
    "char": { "sootType": "ByteType" },
    ...
  },
  "builtinFunctions": ["printf", "scanf"],
  "outputClassName": "GeneratedApp"
}
```

- **`productions`** — 语义角色 → CST 产生式名称
- **`tokens.keywords`** — 语义关键字 → CST token 名称
- **`tokens.typeKeywords`** — 类型关键字 token 列表
- **`tokens.leafTypes`** — 叶子节点类型映射
- **`typeSystem`** — 类型名 → Soot 类型（含类型别名）
- **`builtinFunctions`** — 不需要在源码中定义的库函数
- **`outputClassName`** — 生成的 Jimple 类名

标点符号（`'+'`、`'+'`、`';'`、`'{'` 等）在所有 yacc 前端中通用，不纳入配置。

## 快速开始

### 环境要求

- JDK 21+
- Maven 3.8+

### 编译

```bash
mvn clean compile
```

### 运行测试

```bash
mvn test
```

### 使用 C99 默认配置运行（无需配置文件）

```bash
mvn exec:java -Dexec.mainClass="com.compiler.backend.Main" \
  -Dexec.args="path/to/ast_lalr.json output.jimple"
```

### 使用自定义语义配置运行

```bash
mvn exec:java -Dexec.mainClass="com.compiler.backend.Main" \
  -Dexec.args="path/to/ast_lalr.json output.jimple --sem-config path/to/lang.sem.json"
```

### 打包可执行 JAR

```bash
mvn package -DskipTests
java -jar target/IntermediateCodeGeneration-1.0-SNAPSHOT-jar-with-dependencies.jar \
  ast.json output.jimple --sem-config c99.sem.json
```

## 如何支持一门新语言

假设你要支持一个叫 `calc` 的简单计算器语言：

1. **编写 `calc.l`** — 词法规则（给 SeuLex）
2. **编写 `calc.y`** — 语法规则（给 yacc）
3. **编写 `calc.sem.json`** — 语义映射（给本后端）

`calc.sem.json` 的核心是将 `calc.y` 中的产生式名称填入：

```json
{
  "language": "calc",
  "productions": {
    "translationUnit": "program",
    "functionDefinition": "func_def",
    ...
  },
  "tokens": {
    "keywords": { "if": "IF", "while": "WHILE", "return": "RETURN" },
    "typeKeywords": ["INT_TYPE", "FLOAT_TYPE"],
    "leafTypes": {
      "identifier": "ID",
      "integerConstant": "NUMBER",
      "stringLiteral": "STRING"
    },
    ...
  },
  "typeSystem": {
    "int":   { "sootType": "IntType", "aliases": ["integer"] },
    "float": { "sootType": "FloatType" }
  },
  "builtinFunctions": ["print", "input"],
  "outputClassName": "CalcApp"
}
```

4. **运行流水线**

```bash
# 前端：SeuLex + yacc 解析 calc 源码 → AST JSON
bash run_frontend.sh calc_source.calc

# 后端：用 calc.sem.json 生成 Jimple
java -jar backend.jar ast.json output.jimple --sem-config calc.sem.json
```

**无需修改一行 Java 代码。**

## 技术栈

| 组件 | 技术 |
|------|------|
| 中间表示 (IR) | Soot Jimple (三地址码) |
| 类型系统 | Soot 类型 (IntType, FloatType, ArrayType, ...) |
| JSON 解析 | Gson |
| 构建工具 | Maven |
| 测试框架 | JUnit 5 |
| Java 版本 | 21 |

## 如何贡献

项目地址：`https://github.com/LJR-12138/IntermediateCodeGeneration.git`

### 添加对一门新语言的支持

如果你为某门语言编写了 `.l`、`.y`、`.sem.json` 三件套，欢迎：
1. 将 `xxx.sem.json` 放入 `src/test/resources/`
2. 提交一个测试用例验证流水线
3. 确保 `mvn test` 全部通过

### 扩展更多 Jimple 特性

当前支持的语句类型：变量声明、赋值、if-else、while、return、函数调用、数组访问、数组初始化、二元/一元运算、逻辑短路（&& / ||）。

如需支持 for 循环、switch、struct、指针运算等特性，在 `JimpleGenerator` 中添加即可。
