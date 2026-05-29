package com.compiler.backend.ast;

public class WhileStmtNode extends AstNode {
    public final AstNode condition;
    public final AstNode body;       // typically a BlockNode

    public WhileStmtNode(AstNode condition, AstNode body) {
        super("WhileStmt");
        this.condition = condition;
        this.body = body;
    }
}
