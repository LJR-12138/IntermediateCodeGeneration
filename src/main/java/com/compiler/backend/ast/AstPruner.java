package com.compiler.backend.ast;

import com.compiler.backend.config.LanguageConfig;

import java.util.*;

/**
 * Converts a flat CST (HashMap of JsonNode from ast_lalr.json) into a clean
 * OOP AST by recursively walking the tree, skipping single-child wrapper
 * nodes, and pattern-matching on grammar constructs.
 *
 * Language-specific knowledge (production type names, token names, operator
 * lists, type keywords) is sourced from a {@link LanguageConfig} rather than
 * hardcoded strings.  Swap the .sem.json config file alongside .l and .y to
 * support a different source language.
 *
 * Core pruning rule:
 *   If a node has exactly 1 child and no lexeme, skip it and recurse into the child.
 *   This rule is applied selectively: structural nodes (declaration, function_def,
 *   compound_statement, etc.) are dispatched by raw type first; expression-like
 *   nodes use skipWrappers for collapsing deep chains like
 *   expression → assignment_expression → ... → additive_expression.
 */
public class AstPruner {

    // ── Language-specific sets (built from config in constructor) ──────
    private final Set<String> binaryOps;
    private final Set<String> unaryOps;
    private final Set<String> postfixOps;
    private final Set<String> typeKeywords;
    private final Set<String> structuralTypes;

    private final HashMap<Integer, JsonNode> nodeMap;
    private final LanguageConfig config;

    // ── Universal punctuation sets (same for all yacc-based frontends) ─

    private static final Set<String> PUNCT_BINARY_OPS = Set.of(
        "'+'", "'-'", "'*'", "'/'", "'%'",
        "'<'", "'>'", "'<='", "'>='", "'=='", "'!='",
        "'&&'", "'||'", "'&'", "'|'", "'^'",
        "'<<'", "'>>'", "'='"
    );

    private static final Set<String> PUNCT_UNARY_OPS = Set.of(
        "'*'", "'&'", "'-'", "'!'", "'~'", "'++'", "'--'"
    );

    // ── Constructors ─────────────────────────────────────────────────

    public AstPruner(HashMap<Integer, JsonNode> nodeMap, LanguageConfig config) {
        this.nodeMap = nodeMap;
        this.config = config;

        // Merge universal punctuation operators with language-specific named tokens.
        // The named tokens (e.g. EQ_OP, INC_OP) come from config; punctuation tokens
        // ('+', '-', etc.) are universal across yacc-based frontends.
        this.binaryOps = new HashSet<>(PUNCT_BINARY_OPS);
        this.unaryOps = new HashSet<>(PUNCT_UNARY_OPS);
        // Named operator tokens are checked via config methods at runtime in
        // isBinaryOp / isUnaryOp / isPostfixOp — no need to pre-populate.
        this.postfixOps = new HashSet<>();
        this.typeKeywords = config.getTypeKeywords();
        this.structuralTypes = config.getStructuralTypes();
    }

    /** Backward-compatible constructor using C99 defaults. */
    public AstPruner(HashMap<Integer, JsonNode> nodeMap) {
        this(nodeMap, LanguageConfig.c99Default());
    }

    // ── Public entry ─────────────────────────────────────────────────

    public TranslationUnitNode prune(int rootId) {
        JsonNode root = nodeMap.get(rootId);
        List<AstNode> decls = pruneAll(root);
        List<AstNode> flat = new ArrayList<>();
        for (AstNode d : decls) {
            if (d instanceof TranslationUnitNode tu) {
                flat.addAll(tu.declarations);
            } else if (d != null) {
                flat.add(d);
            }
        }
        return new TranslationUnitNode(flat);
    }

    // ── Core dispatch ────────────────────────────────────────────────

    private List<AstNode> pruneAll(JsonNode node) {
        if (node == null) return List.of();

        // -- Leaves ----------------------------------------------------
        if (node.isLeaf()) {
            AstNode leaf = leafNode(node);
            return leaf != null ? List.of(leaf) : List.of();
        }

        String type = node.type;

        // -- Structural dispatch (by raw type) -------------------------
        if (config.prod("translationUnit").equals(type))     return pruneTranslationUnit(node);
        if (config.prod("functionDefinition").equals(type))  return List.of(pruneFunctionDef(node));
        if (config.prod("declaration").equals(type))         return pruneDeclaration(node);
        if (config.prod("compoundStatement").equals(type))   return List.of(pruneBlock(node));
        if (config.prod("selectionStatement").equals(type))  return List.of(pruneIfStmt(node));
        if (config.prod("iterationStatement").equals(type))  return List.of(pruneIterationStmt(node));
        if (config.prod("jumpStatement").equals(type))       return List.of(pruneReturnStmt(node));

        // -- Optional unwrap for expression chains ---------------------
        if (node.children.size() == 1 && !structuralTypes.contains(type)) {
            return pruneAll(nodeMap.get(node.children.get(0)));
        }

        // -- Expression detection (on the potentially-unwrapped node) --
        List<JsonNode> children = getChildren(node);

        if (children.size() == 3) {
            JsonNode mid = children.get(1);
            JsonNode opNode = mid;
            while (!opNode.isLeaf() && opNode.children.size() == 1
                    && !structuralTypes.contains(opNode.type)) {
                opNode = nodeMap.get(opNode.children.get(0));
            }
            if (opNode.isLeaf() && isBinaryOp(opNode)) {
                AstNode left = pruneOne(children.get(0));
                AstNode right = pruneOne(children.get(2));
                if (left != null && right != null) {
                    return List.of(new BinaryExprNode(left, opNode.lexeme, right));
                }
            }
        }

        if (children.size() == 4) {
            JsonNode second = children.get(1);
            JsonNode fourth = children.get(3);
            if (second.isLeaf() && "'['".equals(second.type)
                    && fourth.isLeaf() && "']'".equals(fourth.type)) {
                AstNode array = pruneOne(children.get(0));
                AstNode index = pruneOne(children.get(2));
                if (array != null && index != null) {
                    return List.of(new ArraySubscriptNode(array, index));
                }
            }
            if (second.isLeaf() && "'('".equals(second.type)
                    && fourth.isLeaf() && "')'".equals(fourth.type)) {
                AstNode funcName = pruneOne(children.get(0));
                List<AstNode> args = extractArguments(children.get(2));
                if (funcName instanceof IdentifierNode id) {
                    return List.of(new FunctionCallNode(id.name, args));
                }
            }
        }

        if (children.size() == 2) {
            JsonNode first = children.get(0);
            JsonNode second = children.get(1);
            // Prefix: ++i, --i, -x, !x, *p, &x
            if (first.isLeaf() && isUnaryOp(first)) {
                AstNode operand = pruneOne(children.get(1));
                if (operand != null) {
                    return List.of(new UnaryExprNode(first.lexeme, operand));
                }
            }
            // Postfix: i++, i--  →  lower to  i = i + 1 / i = i - 1
            if (second.isLeaf() && isPostfixOp(second)) {
                AstNode operand = pruneOne(children.get(0));
                if (operand != null) {
                    String binOp = config.isPostfixOperatorToken("INC_OP") && "INC_OP".equals(second.type) ? "+" : "-";
                    return List.of(new BinaryExprNode(operand, "=",
                        new BinaryExprNode(operand, binOp, new IntLiteralNode(1))));
                }
            }
        }

        // -- Generic fallback: recurse into children -------------------
        List<AstNode> results = new ArrayList<>();
        for (JsonNode child : children) {
            results.addAll(pruneAll(child));
        }
        return results;
    }

    private AstNode pruneOne(JsonNode node) {
        List<AstNode> list = pruneAll(node);
        return list.isEmpty() ? null : list.get(0);
    }

    // ── Operator classification helpers ──────────────────────────────

    private boolean isBinaryOp(JsonNode node) {
        return binaryOps.contains(node.type)
            || config.isBinaryOperatorToken(node.type);
    }

    private boolean isUnaryOp(JsonNode node) {
        return unaryOps.contains(node.type)
            || config.isUnaryOperatorToken(node.type);
    }

    private boolean isPostfixOp(JsonNode node) {
        return postfixOps.contains(node.type)
            || config.isPostfixOperatorToken(node.type);
    }

    // ── Skip wrappers (helper, used only for expression contexts) ────

    /**
     * Skips through a chain of 1-child non-structural nodes.
     * Used for collapsing expression chains like
     * expression → assignment_expression → ... → additive_expression.
     */
    private JsonNode skipWrappers(JsonNode node) {
        JsonNode cur = node;
        while (cur != null && !cur.isLeaf()
                && cur.children.size() == 1
                && !structuralTypes.contains(cur.type)) {
            cur = nodeMap.get(cur.children.get(0));
        }
        return cur;
    }

    // ── Translation unit ─────────────────────────────────────────────

    private List<AstNode> pruneTranslationUnit(JsonNode node) {
        List<AstNode> decls = new ArrayList<>();
        for (JsonNode child : getChildren(node)) {
            List<AstNode> results = pruneAll(child);
            for (AstNode r : results) {
                if (r instanceof TranslationUnitNode tu) {
                    decls.addAll(tu.declarations);
                } else if (r != null) {
                    decls.add(r);
                }
            }
        }
        return List.of(new TranslationUnitNode(decls));
    }

    // ── Function definition ──────────────────────────────────────────

    private FunctionDefNode pruneFunctionDef(JsonNode node) {
        List<JsonNode> children = getChildren(node);
        String returnType = extractTypeName(children.get(0));
        FunctionInfo funcInfo = extractFunctionInfo(children.get(1));
        BlockNode body = pruneBlock(children.get(children.size() - 1));
        return new FunctionDefNode(returnType, funcInfo.name, funcInfo.parameters, body);
    }

    private FunctionInfo extractFunctionInfo(JsonNode declaratorNode) {
        // Walk through declarator -> direct_declarator (possibly via pointer)
        JsonNode dd = declaratorNode;
        while (dd != null && config.prod("declarator").equals(dd.type)) {
            List<JsonNode> children = getChildren(dd);
            if (children.size() == 1) {
                dd = children.get(0);
            } else if (children.size() == 2 && config.prod("pointer").equals(children.get(0).type)) {
                dd = children.get(1);
            } else {
                break;
            }
        }

        if (dd == null || !config.prod("directDeclarator").equals(dd.type)) {
            return new FunctionInfo("unknown", List.of());
        }

        List<JsonNode> ddChildren = getChildren(dd);

        // Function declarator: direct_declarator '(' parameter_type_list ')'
        if (ddChildren.size() == 4) {
            String name = extractIdentifierName(ddChildren.get(0));
            List<VarDeclNode> params = extractParameters(ddChildren.get(2));
            return new FunctionInfo(name, params);
        }

        // Simple declarator (fallback)
        String name = extractIdentifierName(dd);
        return new FunctionInfo(name, List.of());
    }

    private List<VarDeclNode> extractParameters(JsonNode paramTypeListNode) {
        if (paramTypeListNode == null) return List.of();

        // parameter_type_list -> parameter_list (or VOID)
        JsonNode paramList = paramTypeListNode;
        if (config.prod("parameterTypeList").equals(paramList.type)) {
            if (paramList.children.size() == 1) {
                paramList = nodeMap.get(paramList.children.get(0));
            }
        }

        if (paramList == null || !config.prod("parameterList").equals(paramList.type)) {
            return List.of();
        }

        List<VarDeclNode> params = new ArrayList<>();
        collectParams(paramList, params);
        return params;
    }

    private List<AstNode> extractArguments(JsonNode argList) {
        if (argList == null) return List.of();
        if (!config.prod("argumentExpressionList").equals(argList.type)) return List.of();

        List<JsonNode> children = getChildren(argList);
        List<AstNode> args = new ArrayList<>();

        if (children.size() == 1) {
            AstNode arg = pruneOne(children.get(0));
            if (arg != null) args.add(arg);
        } else if (children.size() == 3) {
            args.addAll(extractArguments(children.get(0)));
            AstNode arg = pruneOne(children.get(2));
            if (arg != null) args.add(arg);
        }
        return args;
    }

    private void collectParams(JsonNode paramListNode, List<VarDeclNode> out) {
        List<JsonNode> children = getChildren(paramListNode);

        if (children.size() == 1 && config.prod("parameterDeclaration").equals(children.get(0).type)) {
            VarDeclNode vd = extractParamDecl(children.get(0));
            if (vd != null) out.add(vd);
        } else if (children.size() == 3) {
            collectParams(children.get(0), out);
            if (config.prod("parameterDeclaration").equals(children.get(2).type)) {
                VarDeclNode vd = extractParamDecl(children.get(2));
                if (vd != null) out.add(vd);
            }
        }
    }

    private VarDeclNode extractParamDecl(JsonNode paramDeclNode) {
        List<JsonNode> children = getChildren(paramDeclNode);
        if (children.size() >= 2) {
            String typeName = extractTypeName(children.get(0));
            VarInfo info = extractDeclaratorInfo(children.get(1));
            return new VarDeclNode(typeName, info.name, info.pointerDepth, null);
        }
        return null;
    }

    // ── Declaration ──────────────────────────────────────────────────

    private List<AstNode> pruneDeclaration(JsonNode node) {
        List<JsonNode> children = getChildren(node);
        String typeName = extractTypeName(children.get(0));

        JsonNode initDeclList = null;
        for (JsonNode child : children) {
            if (config.prod("initDeclaratorList").equals(child.type)) {
                initDeclList = child;
                break;
            }
        }
        if (initDeclList == null) return List.of();

        return collectVarDecls(initDeclList, typeName);
    }

    // ── Init declarator list → VarDeclNodes ───────────────────────────

    private List<AstNode> collectVarDecls(JsonNode node, String typeName) {
        if (node == null) return List.of();

        List<JsonNode> children = getChildren(node);

        // Case: init_declarator_list → init_declarator  (1 child)
        if (children.size() == 1 && config.prod("initDeclarator").equals(children.get(0).type)) {
            VarDeclNode vd = pruneVarDecl(children.get(0), typeName);
            return vd != null ? List.of(vd) : List.of();
        }

        // Case: init_declarator_list → init_declarator_list ',' init_declarator  (3 children)
        if (children.size() == 3) {
            List<AstNode> result = new ArrayList<>(collectVarDecls(children.get(0), typeName));
            if (config.prod("initDeclarator").equals(children.get(2).type)) {
                VarDeclNode vd = pruneVarDecl(children.get(2), typeName);
                if (vd != null) result.add(vd);
            }
            return result;
        }

        // Fallback: recurse into children looking for init_declarator nodes
        List<AstNode> result = new ArrayList<>();
        String initDeclName = config.prod("initDeclarator");
        String initDeclListName = config.prod("initDeclaratorList");
        for (JsonNode child : children) {
            if (initDeclName.equals(child.type)) {
                VarDeclNode vd = pruneVarDecl(child, typeName);
                if (vd != null) result.add(vd);
            } else if (initDeclListName.equals(child.type)) {
                result.addAll(collectVarDecls(child, typeName));
            }
        }
        return result;
    }

    // ── Single init_declarator → VarDeclNode ─────────────────────────

    private VarDeclNode pruneVarDecl(JsonNode node, String typeName) {
        if (node == null) return null;
        List<JsonNode> children = getChildren(node);

        if (children.isEmpty()) return null;

        JsonNode declaratorNode = children.get(0);
        AstNode initExpr = null;
        if (children.size() >= 3) {
            initExpr = parseInitializer(children.get(2));
        }

        VarInfo info = extractDeclaratorInfo(declaratorNode);
        return new VarDeclNode(typeName, info.name, info.pointerDepth, initExpr);
    }

    // ── Block / compound_statement ───────────────────────────────────

    private BlockNode pruneBlock(JsonNode node) {
        if (node == null) return new BlockNode(List.of());
        List<AstNode> stmts = new ArrayList<>();
        for (JsonNode child : getChildren(node)) {
            if (child.isLeaf()) {
                String t = child.type;
                if ("'{'".equals(t) || "'}'".equals(t) || "';'".equals(t)) continue;
            }
            List<AstNode> results = pruneAll(child);
            for (AstNode r : results) {
                if (r != null) stmts.add(r);
            }
        }
        return new BlockNode(stmts);
    }

    // ── If statement ─────────────────────────────────────────────────

    private IfStmtNode pruneIfStmt(JsonNode node) {
        List<JsonNode> children = getChildren(node);
        AstNode condition = null;
        List<AstNode> thenStmts = new ArrayList<>();
        List<AstNode> elseStmts = new ArrayList<>();
        boolean inElse = false;

        String ifToken = config.kw("if");
        String elseToken = config.kw("else");

        for (JsonNode child : children) {
            if (child.isLeaf()) {
                String t = child.type;
                if (ifToken.equals(t) || "'('".equals(t) || "')'".equals(t)) continue;
                if (elseToken.equals(t)) { inElse = true; continue; }
            }
            List<AstNode> results = pruneAll(child);
            if (condition == null && results.size() == 1) {
                condition = results.get(0);
            } else if (!inElse) {
                for (AstNode r : results) if (r != null) thenStmts.add(r);
            } else {
                for (AstNode r : results) if (r != null) elseStmts.add(r);
            }
        }

        AstNode thenBranch = thenStmts.size() == 1 ? thenStmts.get(0)
            : new BlockNode(thenStmts);
        AstNode elseBranch = null;
        if (!elseStmts.isEmpty()) {
            elseBranch = elseStmts.size() == 1 ? elseStmts.get(0)
                : new BlockNode(elseStmts);
        }
        return new IfStmtNode(condition, thenBranch, elseBranch);
    }

    // ── Iteration statement (FOR / WHILE dispatch) ────────────────────

    private AstNode pruneIterationStmt(JsonNode node) {
        String forToken = config.kw("for");
        for (JsonNode child : getChildren(node)) {
            if (child.isLeaf() && forToken.equals(child.type)) {
                return pruneForStmt(node);
            }
        }
        return pruneWhileStmt(node);
    }

    // ── For statement → lowered to { init; while(cond) { body; step; } } ─

    private AstNode pruneForStmt(JsonNode node) {
        List<JsonNode> children = getChildren(node);
        String forToken = config.kw("for");

        // Collect non-punctuation children (skip FOR, '(', ')')
        List<JsonNode> structChildren = new ArrayList<>();
        for (JsonNode child : children) {
            if (child.isLeaf()) {
                String t = child.type;
                if (forToken.equals(t) || "'('".equals(t) || "')'".equals(t)) continue;
            }
            structChildren.add(child);
        }

        // structChildren: init, cond, [step], body  (3 or 4 items)
        if (structChildren.size() < 3) {
            return new BlockNode(List.of());
        }

        AstNode init = pruneOne(structChildren.get(0));
        AstNode cond = pruneOne(structChildren.get(1));

        AstNode body;
        AstNode step = null;

        if (structChildren.size() >= 4) {
            step = pruneOne(structChildren.get(2));
            body = pruneOne(structChildren.get(3));
        } else {
            body = pruneOne(structChildren.get(2));
        }

        // Empty condition → always true
        if (cond == null) {
            cond = new IntLiteralNode(1);
        }

        // Build while body: { original_body; step; }
        List<AstNode> whileBodyStmts = new ArrayList<>();
        if (body != null) whileBodyStmts.add(body);
        if (step != null) whileBodyStmts.add(step);
        BlockNode whileBody = new BlockNode(whileBodyStmts);

        WhileStmtNode whileStmt = new WhileStmtNode(cond, whileBody);

        // Build outer block: { init; while_stmt; }
        List<AstNode> outerStmts = new ArrayList<>();
        if (init != null) outerStmts.add(init);
        outerStmts.add(whileStmt);

        return new BlockNode(outerStmts);
    }

    // ── While statement ──────────────────────────────────────────────

    private WhileStmtNode pruneWhileStmt(JsonNode node) {
        List<JsonNode> children = getChildren(node);
        AstNode condition = null;
        List<AstNode> bodyStmts = new ArrayList<>();
        String whileToken = config.kw("while");

        for (JsonNode child : children) {
            if (child.isLeaf()) {
                String t = child.type;
                if (whileToken.equals(t) || "'('".equals(t) || "')'".equals(t)) continue;
            }
            List<AstNode> results = pruneAll(child);
            if (condition == null && results.size() == 1) {
                condition = results.get(0);
            } else {
                for (AstNode r : results) if (r != null) bodyStmts.add(r);
            }
        }

        AstNode body = bodyStmts.size() == 1 ? bodyStmts.get(0)
            : new BlockNode(bodyStmts);
        return new WhileStmtNode(condition, body);
    }

    // ── Return statement ─────────────────────────────────────────────

    private ReturnStmtNode pruneReturnStmt(JsonNode node) {
        String returnToken = config.kw("return");
        for (JsonNode child : getChildren(node)) {
            if (child.isLeaf()) {
                String t = child.type;
                if (returnToken.equals(t) || "';'".equals(t)) continue;
            }
            return new ReturnStmtNode(pruneOne(child));
        }
        return new ReturnStmtNode(null);
    }

    // ── Leaf node factory ────────────────────────────────────────────

    private AstNode leafNode(JsonNode node) {
        String type = node.type;
        String lexeme = node.lexeme;

        if (config.identifierLeafType().equals(type)) {
            return new IdentifierNode(lexeme);
        }
        String intConstType = config.integerConstantLeafType();
        if (intConstType.equals(type) || "INTEGER_CONSTANT".equals(type)) {
            try {
                return new IntLiteralNode(Integer.parseInt(lexeme));
            } catch (NumberFormatException e) {
                return new IdentifierNode(lexeme);
            }
        }
        if (config.stringLiteralLeafType().equals(type)) {
            String content = lexeme;
            // Strip C string delimiters.  The frontend may emit either
            //   \"...\"  (backslash-escaped quotes, the common case) or
            //   "..."   (plain ASCII quotes).
            // Both represent the C source string literal delimiters.
            if (content.length() >= 4
                    && content.charAt(0) == '\\'
                    && content.charAt(1) == '"'
                    && content.charAt(content.length() - 2) == '\\'
                    && content.charAt(content.length() - 1) == '"') {
                content = content.substring(2, content.length() - 2);
            } else if (content.length() >= 2
                    && content.startsWith("\"") && content.endsWith("\"")) {
                content = content.substring(1, content.length() - 1);
            }
            // Process C escape sequences into real Java characters.
            // The frontend may double-escape backslashes when writing JSON
            // (e.g. C "\n" → JSON "\\n" which Gson returns as two chars
            //  '\' + 'n', but sometimes → JSON "\\\\n" → "\" + "\" + "n").
            // ORDER MATTERS: collapse double-backslashes FIRST so that
            // subsequent passes see the intended single C escape.
            // Then convert C escapes to their real character values.
            // "\s" is a frontend quirk where literal space becomes backslash-s.
            content = content.replace("\\\\", "\\")
                             .replace("\\n", "\n")
                             .replace("\\t", "\t")
                             .replace("\\r", "\r")
                             .replace("\\\"", "\"")
                             .replace("\\s", " ")
                             .replace("\\'", "'");
            return new StringLiteralNode(content);
        }
        return null;
    }

    // ── Helpers ──────────────────────────────────────────────────────

    private String extractTypeName(JsonNode declSpecNode) {
        JsonNode cur = declSpecNode;
        while (cur != null && !cur.isLeaf()) {
            if (cur.children.size() == 1) {
                cur = nodeMap.get(cur.children.get(0));
            } else {
                for (int cid : cur.children) {
                    JsonNode child = nodeMap.get(cid);
                    if (child != null && child.isLeaf() && typeKeywords.contains(child.type)) {
                        return child.lexeme;
                    }
                }
                for (int cid : cur.children) {
                    JsonNode child = nodeMap.get(cid);
                    if (child != null && !child.isLeaf()) {
                        cur = child;
                        break;
                    }
                }
            }
        }
        return cur != null ? cur.lexeme : "int";
    }

    private VarInfo extractDeclaratorInfo(JsonNode declaratorNode) {
        if (declaratorNode == null) return new VarInfo("unknown", 0);

        List<JsonNode> children = getChildren(declaratorNode);

        // declarator → pointer direct_declarator
        if (children.size() == 2 && config.prod("pointer").equals(children.get(0).type)) {
            int ptrDepth = countPointerStars(children.get(0));
            String name = extractIdentifierName(children.get(1));
            if (hasArrayBrackets(children.get(1))) {
                ptrDepth += 1;
            }
            return new VarInfo(name, ptrDepth);
        }

        // declarator → direct_declarator (possibly with array brackets)
        JsonNode dd = findDirectDeclarator(declaratorNode);
        int ptrDepth = hasArrayBrackets(dd) ? 1 : 0;
        String name = extractIdentifierName(declaratorNode);
        return new VarInfo(name, ptrDepth);
    }

    private JsonNode findDirectDeclarator(JsonNode node) {
        JsonNode cur = node;
        String ddType = config.prod("directDeclarator");
        while (cur != null && !ddType.equals(cur.type)) {
            List<JsonNode> children = getChildren(cur);
            if (children.size() == 1) {
                cur = children.get(0);
            } else {
                break;
            }
        }
        return cur;
    }

    private boolean hasArrayBrackets(JsonNode node) {
        if (node == null) return false;
        for (JsonNode child : getChildren(node)) {
            if (child.isLeaf() && "'['".equals(child.type)) {
                return true;
            }
        }
        return false;
    }

    private int countPointerStars(JsonNode pointerNode) {
        int count = 0;
        JsonNode cur = pointerNode;
        String ptrType = config.prod("pointer");
        while (cur != null && ptrType.equals(cur.type)) {
            for (JsonNode child : getChildren(cur)) {
                if (child.isLeaf() && "'*'".equals(child.type)) count++;
            }
            // Find nested pointer
            JsonNode next = null;
            for (JsonNode child : getChildren(cur)) {
                if (ptrType.equals(child.type)) { next = child; break; }
            }
            cur = next;
        }
        return count;
    }

    private String extractIdentifierName(JsonNode directDeclNode) {
        if (directDeclNode == null) return "unknown";
        String idType = config.identifierLeafType();
        // Walk down 1-child chains until we hit IDENTIFIER
        JsonNode cur = directDeclNode;
        while (cur != null) {
            if (cur.isLeaf() && idType.equals(cur.type)) return cur.lexeme;
            if (cur.children.size() == 1) {
                cur = nodeMap.get(cur.children.get(0));
            } else {
                for (int cid : cur.children) {
                    JsonNode child = nodeMap.get(cid);
                    if (child != null && child.isLeaf() && idType.equals(child.type)) {
                        return child.lexeme;
                    }
                }
                // Recurse into first non-leaf child
                JsonNode next = null;
                for (int cid : cur.children) {
                    JsonNode child = nodeMap.get(cid);
                    if (child != null && !child.isLeaf()) {
                        next = child;
                        break;
                    }
                }
                cur = next;
            }
        }
        return "unknown";
    }

    private List<JsonNode> getChildren(JsonNode node) {
        return AstJsonParser.getChildren(nodeMap, node);
    }

    // ── Initializer parsing (array / scalar) ──────────────────────────

    /**
     * Parses an initializer node, detecting brace-enclosed initializer_list
     * (array init) vs. a single expression (scalar init).
     */
    private AstNode parseInitializer(JsonNode initNode) {
        if (initNode == null) return null;

        List<JsonNode> children = getChildren(initNode);

        // production: initializer → '{' initializer_list '}'
        if (children.size() == 3
                && children.get(0).isLeaf() && "'{'".equals(children.get(0).type)
                && children.get(2).isLeaf() && "'}'".equals(children.get(2).type)
                && config.prod("initializerList").equals(children.get(1).type)) {
            return parseInitializerList(children.get(1));
        }

        // Single expression initializer (e.g. int x = 5)
        return pruneOne(initNode);
    }

    private ArrayInitNode parseInitializerList(JsonNode initList) {
        List<AstNode> values = new ArrayList<>();
        collectInitValues(initList, values);
        return new ArrayInitNode(values);
    }

    /**
     * Recursively collects values from an initializer_list.
     * Handles the two grammar forms:
     *   initializer_list → initializer
     *   initializer_list → initializer_list ',' initializer
     */
    private void collectInitValues(JsonNode initListNode, List<AstNode> out) {
        List<JsonNode> children = getChildren(initListNode);
        String initName = config.prod("initializer");

        if (children.isEmpty()) return;

        if (children.size() == 1 && initName.equals(children.get(0).type)) {
            AstNode val = parseInitializer(children.get(0));
            if (val != null) out.add(val);
        } else if (children.size() == 3) {
            collectInitValues(children.get(0), out);
            if (initName.equals(children.get(2).type)) {
                AstNode val = parseInitializer(children.get(2));
                if (val != null) out.add(val);
            }
        }
    }

    // ── Internal types ───────────────────────────────────────────────

    private static class VarInfo {
        final String name;
        final int pointerDepth;
        VarInfo(String name, int pointerDepth) {
            this.name = name;
            this.pointerDepth = pointerDepth;
        }
    }

    private static class FunctionInfo {
        final String name;
        final List<VarDeclNode> parameters;
        FunctionInfo(String name, List<VarDeclNode> parameters) {
            this.name = name;
            this.parameters = parameters;
        }
    }
}
