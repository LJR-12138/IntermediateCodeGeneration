package com.compiler.backend.ast;

import java.util.List;

/**
 * Pretty-prints the pruned AST with indentation.
 * Also provides a Graphviz DOT output method for report screenshots.
 */
public class AstPrinter {

    private static final String INDENT = "  ";

    /**
     * Prints the AST tree to stdout with indentation.
     */
    public static void printTree(AstNode node) {
        printTree(node, 0);
    }

    private static void printTree(AstNode node, int depth) {
        if (node == null) return;
        String prefix = INDENT.repeat(depth) + "├─ ";
        System.out.println(prefix + nodeToString(node));
        for (AstNode child : getChildren(node)) {
            printTree(child, depth + 1);
        }
    }

    // ─── Graphviz DOT output ────────────────────────────────────────

    private static int dotId;

    /**
     * Returns a Graphviz DOT-format string of the AST for rendering.
     */
    public static String toDot(AstNode root) {
        dotId = 0;
        StringBuilder sb = new StringBuilder();
        sb.append("digraph AST {\n");
        sb.append("  node [shape=box, style=filled, fillcolor=\"#E8F0FE\", fontname=\"monospace\"];\n");
        sb.append("  edge [fontname=\"monospace\", fontsize=9];\n");
        dumpDotNode(root, -1, "", sb);
        sb.append("}\n");
        return sb.toString();
    }

    private static int dumpDotNode(AstNode node, int parentId, String edgeLabel, StringBuilder sb) {
        if (node == null) return -1;
        int myId = dotId++;
        String label = nodeToString(node).replace("\"", "\\\"");
        sb.append("  n").append(myId).append(" [label=\"").append(label).append("\"];\n");
        if (parentId >= 0) {
            sb.append("  n").append(parentId).append(" -> n").append(myId);
            if (!edgeLabel.isEmpty()) {
                sb.append(" [label=\"").append(edgeLabel).append("\"]");
            }
            sb.append(";\n");
        }
        for (AstNode child : getChildren(node)) {
            if (child != null) {
                dumpDotNode(child, myId, "", sb);
            }
        }
        return myId;
    }

    // ─── Helpers ────────────────────────────────────────────────────

    private static String nodeToString(AstNode node) {
        return switch (node) {
            case TranslationUnitNode n -> "TranslationUnit [" + n.declarations.size() + " decls]";
            case FunctionDefNode n   -> "FunctionDef: " + n.returnType + " " + n.name + "()";
            case VarDeclNode n       -> "VarDecl: " + n.typeName
                                        + ("*".repeat(n.pointerDepth)) + " " + n.varName
                                        + (n.initExpr != null ? " = ..." : "");
            case BinaryExprNode n    -> "BinaryExpr: " + n.operator;
            case UnaryExprNode n     -> "UnaryExpr: " + n.operator;
            case ArraySubscriptNode n -> "ArraySubscript";
            case FunctionCallNode n   -> "FunctionCall: " + n.functionName + "(" + n.arguments.size() + " args)";
            case IfStmtNode n        -> "IfStmt" + (n.elseBranch != null ? " (if-else)" : "");
            case WhileStmtNode n     -> "WhileStmt";
            case ReturnStmtNode n    -> "ReturnStmt" + (n.value == null ? " (void)" : "");
            case BlockNode n         -> "Block [" + n.statements.size() + " stmts]";
            case IdentifierNode n    -> "Identifier: " + n.name;
            case IntLiteralNode n     -> "IntLiteral: " + n.value;
            case StringLiteralNode n  -> "StringLiteral: \"" + n.value + "\"";
            default                   -> "?";
        };
    }

    private static List<AstNode> getChildren(AstNode node) {
        return switch (node) {
            case TranslationUnitNode n -> n.declarations;
            case FunctionDefNode n -> {
                var list = new java.util.ArrayList<AstNode>();
                list.addAll(n.parameters);
                list.add(n.body);
                yield list;
            }
            case VarDeclNode n -> {
                var list = new java.util.ArrayList<AstNode>();
                if (n.initExpr != null) list.add(n.initExpr);
                yield list;
            }
            case BinaryExprNode n -> List.of(n.left, n.right);
            case UnaryExprNode n  -> List.of(n.operand);
            case ArraySubscriptNode n -> List.of(n.array, n.index);
            case FunctionCallNode n   -> n.arguments;
            case IfStmtNode n -> {
                var list = new java.util.ArrayList<AstNode>();
                list.add(n.condition);
                list.add(n.thenBranch);
                if (n.elseBranch != null) list.add(n.elseBranch);
                yield list;
            }
            case WhileStmtNode n  -> List.of(n.condition, n.body);
            case ReturnStmtNode n -> n.value != null ? List.of(n.value) : List.of();
            case BlockNode n         -> n.statements;
            case IntLiteralNode n    -> List.of();
            case StringLiteralNode n -> List.of();
            default                  -> List.of();
        };
    }
}
