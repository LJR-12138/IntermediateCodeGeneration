package com.compiler.backend.ast;

public class UnaryExprNode extends AstNode {
    public final String operator;   // e.g. "*", "&", "-", "!"
    public final AstNode operand;

    public UnaryExprNode(String operator, AstNode operand) {
        super("UnaryExpr");
        this.operator = operator;
        this.operand = operand;
    }
}
