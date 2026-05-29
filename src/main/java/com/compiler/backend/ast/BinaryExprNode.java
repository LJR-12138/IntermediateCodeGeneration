package com.compiler.backend.ast;

public class BinaryExprNode extends AstNode {
    public final AstNode left;
    public final String operator;   // e.g. "+", "-", "*", "<", "==", "="
    public final AstNode right;

    public BinaryExprNode(AstNode left, String operator, AstNode right) {
        super("BinaryExpr");
        this.left = left;
        this.operator = operator;
        this.right = right;
    }
}
