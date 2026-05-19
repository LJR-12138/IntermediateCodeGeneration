# 编译原理后端生成器（Java + Soot）开发指南

## 1. 项目背景与当前处境
你好，Claude。你现在是我的高级 Java 后端研发工程师。
我们正在开发一个 C 语言子集（C99）到 Jimple 中间代码的编译器。
**我们的特殊架构：**
- **前端（已完成）：** 队友使用 C++ 编写了 Lex 和 Yacc（LALR）解析器。为了解决跨语言对接问题，前端**不会**在内存中传递 AST 对象，而是将解析过程输出为纯文本日志文件。
- **后端（我们的任务）：** 我们需要使用 **Java + Soot** 框架，读取前端吐出的“文本规约序列”，在 Java 内存中模拟“语义栈（Semantic Stack）”，最终生成并导出 Jimple 中间代码。

## 2. 核心输入文件（测试数据）
在本项目的工作目录中，前端队友已经为我们提供了标准的测试用例和解析输出（位于 `tests/golden/test1/decl_int/raw/` 等目录下）。你需要重点读取并分析以下文件：

1. **规约序列文件 (`parse_reductions_lalr.txt`)**：
   - 记录了自底向上的规约过程（例如 `3. #77 declaration -> declaration_specifiers ';'`）。
   - 这是我们触发语义动作（Soot API 调用）的核心驱动力。
2. **解析追踪文件 (`parse_trace_lalr.tsv`)**：
   - 记录了状态机的移进（Shift）和规约（Reduce）全过程，包含具体的 Lookahead Token（如 `INT`, `ID`）。
   - 当遇到变量名或常量值时，我们需要从这里（或配套的 `.tokens` 文件中）提取具体的字面量。
3. **文法定义 (`c99.y`)**：
   - 供我们参考完整的产生式规则，以便编写对应的 Java 处理逻辑。

## 3. 技术栈与依赖
- **语言：** Java 21
- **构建工具：** Maven 
- **核心框架：** Soot (用于生成 Jimple，需要引入相关依赖)
- **JSON/文件解析：** 如果需要处理复杂表格，可以使用 OpenCSV 或普通 Java IO 文本读取。

## 4. 核心架构设计思路：基于文本驱动的 SDT
请你采用**语法制导翻译（Syntax-Directed Translation, SDT）**的设计模式。
我们不建传统的树，而是建一个**栈**。

工作流如下：
1. 逐行读取 `parse_reductions_lalr.txt`。
2. 维护一个 `Stack<Object> semanticStack`。
3. 遇到 `Expr -> Expr + Expr` 等表达式规约时：从栈中 pop 出右侧操作数和左侧操作数，调用 Soot 创建一个加法运算的局部变量，然后 push 回栈中。
4. 遇到 `Decl -> Type ID` 等声明规约时：利用提取到的 ID 名字和 Type，调用 Soot 声明 Local 变量，并存入我们的 `HashMap` 符号表中。
5. 遍历结束后，调用 Soot 的 Printer 输出 `.jimple` 文件。

---

## 5. 你的开发任务（请一步步执行并与我确认）

请按照以下阶段推进代码编写，每完成一步请向我汇报，确认无误后再进行下一步：

### **Step 1: 工程初始化与依赖配置**
- 帮我生成一个标准的 Maven `pom.xml` 或 `build.gradle`。
- 引入 Soot 框架的依赖包。
- 创建项目的基本包结构（如 `com.compiler.backend`）。

### **Step 2: 文本解析引擎开发 (Text Parser)**
- 编写 `TraceParser.java` 和 `ReductionParser.java`。
- 读取 `parse_trace_lalr.tsv`，提取出所有的有效 Token（特别是标识符 ID 的名字和常量的数值）。
- 读取 `parse_reductions_lalr.txt`，将其解析为易于处理的 Java 对象列表（如 `class ReductionRule { int id; String lhs; List<String> rhs; }`）。

### **Step 3: 语义引擎与符号表构建 (Semantic Engine)**
- 编写 `SymbolTable.java`，支持嵌套作用域（目前可以先做全局和单层函数作用域）。
- 编写 `TranslationEngine.java`，这是核心类，包含我们的 `semanticStack`。
- 使用 `switch` 或策略模式，针对 `c99.y` 中的核心规则（声明、赋值、算术运算）编写初步的弹栈、压栈动作日志打印（暂不接入 Soot，先验证栈逻辑是否正确）。

### **Step 4: 接入 Soot 框架生成 Jimple**
- 将 Step 3 中的伪动作替换为真实的 Soot API 调用。
- 初始化 Soot 环境 (`G.reset()`, 设置 Scene 等)。
- 创建 Soot 的 `SootClass` 和 `SootMethod` (如 main 方法)。
- 利用 `Jimple.v().new...` 生成 `Local` 变量和三地址指令 (`AssignStmt`, `AddExpr` 等)。

### **Step 5: 联调与测试**
- 编写 `Main.java` 作为总入口。
- 将读取路径指向 `tests/golden/test1/decl_int/raw/`。
- 运行程序，验证是否能在控制台或文件中成功打印出等效的 Jimple 代码。

**收到指令后，请回复“确认理解”。然后主动开始帮我执行 Step 1，并提供相关的构建配置文件代码。**